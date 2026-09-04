// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Abilities/WuwaGameplayAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Core/WuwaGameplayTags.h"
#include "AbilitySystem/Runtime/WuwaAbilitySystemComponent.h"
#include "AbilitySystem/WuwaAbilitySystemLog.h"
#include "Core/WuwaStateTagComponent.h"
#include "WuwaCharacter.h"
#include "WuwaPlayerState.h"

UWuwaGameplayAbility::UWuwaGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

AWuwaCharacter* UWuwaGameplayAbility::GetWuwaCharacterFromActorInfo() const
{
	return CurrentActorInfo != nullptr ? Cast<AWuwaCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
}

AWuwaPlayerState* UWuwaGameplayAbility::GetWuwaPlayerStateFromActorInfo() const
{
	return CurrentActorInfo != nullptr ? Cast<AWuwaPlayerState>(CurrentActorInfo->OwnerActor.Get()) : nullptr;
}

UWuwaAbilitySystemComponent* UWuwaGameplayAbility::GetWuwaAbilitySystemComponentFromActorInfo() const
{
	return CurrentActorInfo != nullptr
	           ? Cast<UWuwaAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent.Get())
	           : nullptr;
}

bool UWuwaGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                              const FGameplayAbilityActorInfo* ActorInfo,
                                              const FGameplayTagContainer* SourceTags,
                                              const FGameplayTagContainer* TargetTags,
                                              FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AWuwaCharacter* Character =
	    ActorInfo != nullptr ? Cast<AWuwaCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UWuwaStateTagComponent* StateTagComponent = IsValid(Character) ? Character->GetStateTagComponent() : nullptr;
	if (!IsValid(StateTagComponent))
	{
		UE_LOG(
		    LogWuwaAbility, Warning, TEXT("Ability 激活失败：缺少 StateTagComponent。Ability=%s"), *GetNameSafe(this));
		return false;
	}

	const FGameplayTagContainer ActiveStateTags = StateTagComponent->GetActiveTags();
	return ActiveStateTags.HasAll(WuwaRequiredStateTags) && !ActiveStateTags.HasAny(WuwaBlockedStateTags);
}

bool UWuwaGameplayAbility::BeginInterruptRuntime(UAnimMontage* Montage)
{
	if (!IsActive() || !IsValid(Montage))
	{
		return false;
	}

	CleanupInterruptRuntime();

	InterruptMontage = Montage;
	bInterruptWindowOpen = false;
	ActiveInterruptWindowId = INDEX_NONE;

	UWuwaAbilitySystemComponent* ASC = GetWuwaAbilitySystemComponentFromActorInfo();

	if (!IsValid(ASC))
	{
		return false;
	}

	// 当前 Ability 如果需要锁 WASD，则自己贡献一份 Block.Input.Move。
	if (InterruptPolicy.bBlockMoveWhileActive)
	{
		ASC->AddLooseGameplayTag(WuwaGameplayTags::Block_Input_Move, 1, EGameplayTagReplicationState::TagOnly);

		bOwnsInterruptMoveBlock = true;
	}

	InterruptWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
	    this, WuwaGameplayTags::Event_Ability_InterruptWindow_Begin, nullptr, false, true);

	InterruptWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
	    this, WuwaGameplayTags::Event_Ability_InterruptWindow_End, nullptr, false, true);

	if (!IsValid(InterruptWindowBeginTask) || !IsValid(InterruptWindowEndTask))
	{
		CleanupInterruptRuntime();
		return false;
	}

	InterruptWindowBeginTask->EventReceived.AddDynamic(this, &ThisClass::HandleInterruptWindowBegin);

	InterruptWindowEndTask->EventReceived.AddDynamic(this, &ThisClass::HandleInterruptWindowEnd);

	InterruptWindowBeginTask->ReadyForActivation();
	InterruptWindowEndTask->ReadyForActivation();

	return true;
}

void UWuwaGameplayAbility::HandleInterruptWindowBegin(FGameplayEventData Payload)
{
	AWuwaCharacter* Character = GetWuwaCharacterFromActorInfo();

	if (!IsActive() || !InterruptMontage.IsValid() || Payload.OptionalObject != InterruptMontage.Get() ||
	    !IsValid(Character) || Payload.OptionalObject2 != Character->GetMesh())
	{
		return;
	}

	const int32 WindowId = FMath::RoundToInt(Payload.EventMagnitude);

	if (WindowId < 0)
	{
		return;
	}

	ActiveInterruptWindowId = WindowId;
	bInterruptWindowOpen = true;
}

void UWuwaGameplayAbility::HandleInterruptWindowEnd(FGameplayEventData Payload)
{
	AWuwaCharacter* Character = GetWuwaCharacterFromActorInfo();

	if (!InterruptMontage.IsValid() || Payload.OptionalObject != InterruptMontage.Get() || !IsValid(Character) ||
	    Payload.OptionalObject2 != Character->GetMesh())
	{
		return;
	}

	const int32 WindowId = FMath::RoundToInt(Payload.EventMagnitude);

	if (!bInterruptWindowOpen || ActiveInterruptWindowId != WindowId)
	{
		return;
	}

	bInterruptWindowOpen = false;
	ActiveInterruptWindowId = INDEX_NONE;
}

bool UWuwaGameplayAbility::CanInterruptFrom(const EWuwaAbilityInterruptSource Source,
                                            const FGameplayTag& IncomingActionTag) const
{
	if (!IsActive() || !CanBeCanceled() || !bInterruptWindowOpen)
	{
		return false;
	}

	switch (Source)
	{
		case EWuwaAbilityInterruptSource::Move:

			return InterruptPolicy.bAllowMoveInterrupt;

		case EWuwaAbilityInterruptSource::Action:
		{
			if (!InterruptPolicy.bAllowActionInterrupt)
			{
				return false;
			}

			if (InterruptPolicy.AllowedInterruptActionTags.IsEmpty())
			{
				return true;
			}

			return IncomingActionTag.IsValid() &&
			       IncomingActionTag.MatchesAny(InterruptPolicy.AllowedInterruptActionTags);
		}

		default:
			return false;
	}
}

bool UWuwaGameplayAbility::TryInterruptFrom(const EWuwaAbilityInterruptSource Source,
                                            const FGameplayTag& IncomingActionTag)
{
	if (!CanInterruptFrom(Source, IncomingActionTag))
	{
		return false;
	}

	// 本地预测取消需要向服务器同步
	CancelAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true);

	return true;
}

void UWuwaGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
                                      const FGameplayAbilityActorInfo* ActorInfo,
                                      const FGameplayAbilityActivationInfo ActivationInfo,
                                      const bool bReplicateEndAbility,
                                      const bool bWasCancelled)
{
	CleanupInterruptRuntime();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UWuwaGameplayAbility::CleanupInterruptRuntime()
{
	if (IsValid(InterruptWindowBeginTask))
	{
		InterruptWindowBeginTask->EndTask();
	}

	if (IsValid(InterruptWindowEndTask))
	{
		InterruptWindowEndTask->EndTask();
	}

	InterruptWindowBeginTask = nullptr;
	InterruptWindowEndTask = nullptr;

	if (bOwnsInterruptMoveBlock)
	{
		if (UWuwaAbilitySystemComponent* ASC = GetWuwaAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(WuwaGameplayTags::Block_Input_Move, 1, EGameplayTagReplicationState::TagOnly);
		}
	}

	bOwnsInterruptMoveBlock = false;
	bInterruptWindowOpen = false;
	ActiveInterruptWindowId = INDEX_NONE;
	InterruptMontage.Reset();
}

bool UWuwaGameplayAbility::BlocksInterruptSource(const EWuwaAbilityInterruptSource Source) const
{
	switch (Source)
	{
		case EWuwaAbilityInterruptSource::Move:
			return InterruptPolicy.bBlockMoveWhileActive;

		case EWuwaAbilityInterruptSource::Action:
			return InterruptPolicy.bBlockActionWhileActive;

		case EWuwaAbilityInterruptSource::Ability:
			return false;

		default:
			return false;
	}
}

void UWuwaGameplayAbility::ResetInterruptWindowForTransition()
{
	bInterruptWindowOpen = false;
	ActiveInterruptWindowId = INDEX_NONE;
}
