// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Animation/WuwaAnimNotifyState_CombatHitWindow.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyLibrary.h"
#include "Combat/WuwaCombatLog.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/WuwaGameplayTags.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

namespace
{
void SendHitWindowEvent(USkeletalMeshComponent* MeshComp,
                        UAnimSequenceBase* Animation,
                        const FGameplayTag& EventTag,
                        const uint8 StepIndex)
{
	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(Owner) || !IsValid(Animation) || !EventTag.IsValid())
	{
		return;
	}

	AActor* EventRecipient =
	    IsValid(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner)) ? Owner : nullptr;
	const APawn* Pawn = Cast<APawn>(Owner);
	if (!IsValid(EventRecipient) && IsValid(Pawn))
	{
		EventRecipient = Pawn->GetPlayerState();
	}
	if (!IsValid(EventRecipient) || !IsValid(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(EventRecipient)))
	{
		UE_LOG(LogWuwaCombat,
		       Warning,
		       TEXT("HitWindow Event 投递失败：Owner 或 PlayerState 缺少 ASC。Owner=%s, Event=%s"),
		       *GetNameSafe(Owner),
		       *EventTag.ToString());
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = Owner;
	Payload.Target = Owner;
	Payload.OptionalObject = Animation;
	Payload.OptionalObject2 = MeshComp;
	Payload.EventMagnitude = static_cast<float>(StepIndex) + 1.0f;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(EventRecipient, EventTag, Payload);
}

/**
 * 获取当前 Montage 实例编号
 *
 * @param MeshComp	动画 Mesh
 * @param Animation	当前动画
 * @return 找到活动 Montage 实例时返回实例编号，否则返回 INDEX_NONE
 */
int32 GetMontageInstanceId(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	const UAnimMontage* Montage = Cast<UAnimMontage>(Animation);
	const UAnimInstance* AnimInstance = IsValid(MeshComp) ? MeshComp->GetAnimInstance() : nullptr;
	const FAnimMontageInstance* MontageInstance =
	    IsValid(AnimInstance) && IsValid(Montage) ? AnimInstance->GetActiveInstanceForMontage(Montage) : nullptr;
	return MontageInstance != nullptr ? MontageInstance->GetInstanceID() : INDEX_NONE;
}
}

UWuwaAnimNotifyState_CombatHitWindow::UWuwaAnimNotifyState_CombatHitWindow()
{
	bIsNativeBranchingPoint = true;
}

void UWuwaAnimNotifyState_CombatHitWindow::NotifyBegin(USkeletalMeshComponent* MeshComp,
                                                       UAnimSequenceBase* Animation,
                                                       float TotalDuration,
                                                       const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	for (auto Iterator = ActiveMeshStepMontageInstanceIds.CreateIterator(); Iterator; ++Iterator)
	{
		if (!Iterator.Key().IsValid())
		{
			Iterator.RemoveCurrent();
		}
	}

	const TWeakObjectPtr<USkeletalMeshComponent> MeshKey(MeshComp);
	const int32 MontageInstanceId = GetMontageInstanceId(MeshComp, Animation);
	TMap<uint8, int32>& ActiveStepInstances = ActiveMeshStepMontageInstanceIds.FindOrAdd(MeshKey);
	if (const int32* ActiveMontageInstanceId = ActiveStepInstances.Find(StepIndex))
	{
		if (*ActiveMontageInstanceId == MontageInstanceId)
		{
			return;
		}
	}
	ActiveStepInstances.Add(StepIndex, MontageInstanceId);
	SendHitWindowEvent(MeshComp, Animation, WuwaGameplayTags::Event_Combat_HitWindow_Begin, StepIndex);
}

void UWuwaAnimNotifyState_CombatHitWindow::NotifyEnd(USkeletalMeshComponent* MeshComp,
                                                     UAnimSequenceBase* Animation,
                                                     const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	const TWeakObjectPtr<USkeletalMeshComponent> MeshKey(MeshComp);
	TMap<uint8, int32>* ActiveStepInstances = ActiveMeshStepMontageInstanceIds.Find(MeshKey);
	const int32* ActiveMontageInstanceId =
	    ActiveStepInstances != nullptr ? ActiveStepInstances->Find(StepIndex) : nullptr;
	if (!UAnimNotifyLibrary::NotifyStateReachedEnd(EventReference) || ActiveMontageInstanceId == nullptr ||
	    *ActiveMontageInstanceId != GetMontageInstanceId(MeshComp, Animation))
	{
		return;
	}

	ActiveStepInstances->Remove(StepIndex);
	if (ActiveStepInstances->IsEmpty())
	{
		ActiveMeshStepMontageInstanceIds.Remove(MeshKey);
	}
	SendHitWindowEvent(MeshComp, Animation, WuwaGameplayTags::Event_Combat_HitWindow_End, StepIndex);
}

FString UWuwaAnimNotifyState_CombatHitWindow::GetNotifyName_Implementation() const
{
	return TEXT("Wuwa Combat Hit Window");
}
