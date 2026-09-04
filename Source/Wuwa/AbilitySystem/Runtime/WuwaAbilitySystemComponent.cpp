// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Runtime/WuwaAbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Abilities/WuwaGameplayAbility.h"
#include "Combat/Attributes/WuwaCombatSet.h"
#include "Combat/Attributes/WuwaHealthSet.h"
#include "Combat/Attributes/WuwaResourceSet.h"
#include "Core/WuwaGameplayTags.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"

namespace
{
/**
 * 解析活动 AbilitySpec 当前的激活预测键
 *
 * @param Spec	目标 AbilitySpec
 * @return 当前实例或 Spec 保存的激活预测键
 */
FPredictionKey ResolveActivationPredictionKey(const FGameplayAbilitySpec& Spec)
{
	const UGameplayAbility* PrimaryInstance = Spec.GetPrimaryInstance();
	return IsValid(PrimaryInstance) ? PrimaryInstance->GetCurrentActivationInfo().GetActivationPredictionKey()
	                                : Spec.ActivationInfo.GetActivationPredictionKey();
}
}

UWuwaAbilitySystemComponent::UWuwaAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UWuwaAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputPressed(Spec);
	if (!Spec.IsActive() || !Spec.Handle.IsValid())
	{
		return;
	}

	InvokeReplicatedEvent(
	    EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle, ResolveActivationPredictionKey(Spec));
}

void UWuwaAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputReleased(Spec);
	if (!Spec.IsActive() || !Spec.Handle.IsValid())
	{
		return;
	}

	InvokeReplicatedEvent(
	    EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, ResolveActivationPredictionKey(Spec));
}

bool UWuwaAbilitySystemComponent::HasBlockingActiveAbility(const EWuwaAbilityInterruptSource Source) const
{
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!Spec.IsActive())
		{
			continue;
		}

		const UWuwaGameplayAbility* Ability = Cast<UWuwaGameplayAbility>(Spec.GetPrimaryInstance());

		if (!IsValid(Ability))
		{
			continue;
		}

		if (Ability->BlocksInterruptSource(Source))
		{
			return true;
		}
	}

	return false;
}

bool UWuwaAbilitySystemComponent::CanInterruptActiveAbility(const EWuwaAbilityInterruptSource Source,
                                                            const FGameplayTag& IncomingActionTag) const
{
	bool bFoundBlockingAbility = false;

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!Spec.IsActive())
		{
			continue;
		}

		const UWuwaGameplayAbility* Ability = Cast<UWuwaGameplayAbility>(Spec.GetPrimaryInstance());

		if (!IsValid(Ability) || !Ability->BlocksInterruptSource(Source))
		{
			continue;
		}

		bFoundBlockingAbility = true;

		// 只要存在一个 Blocking Ability 当前不能取消，
		// 整次输入就不能执行。
		if (!Ability->CanInterruptFrom(Source, IncomingActionTag))
		{
			return false;
		}
	}

	return bFoundBlockingAbility;
}

FWuwaAbilitySystemRuntimeSnapshot UWuwaAbilitySystemComponent::GetRuntimeSnapshot() const
{
	FWuwaAbilitySystemRuntimeSnapshot Snapshot;
	const bool bHasValidActorInfo = AbilityActorInfo.IsValid();
	const AActor* SnapshotOwnerActor = bHasValidActorInfo ? GetOwnerActor() : nullptr;
	const AActor* SnapshotAvatarActor = bHasValidActorInfo ? GetAvatarActor() : nullptr;
	Snapshot.bInitialized = IsValid(SnapshotOwnerActor) && IsValid(SnapshotAvatarActor);
	Snapshot.bAuthority = IsValid(SnapshotOwnerActor) && SnapshotOwnerActor->HasAuthority();
	Snapshot.OwnerActorName = GetNameSafe(SnapshotOwnerActor);
	Snapshot.AvatarActorName = GetNameSafe(SnapshotAvatarActor);
	Snapshot.ReplicationModeName =
	    StaticEnum<EGameplayEffectReplicationMode>()->GetNameStringByValue(static_cast<int64>(ReplicationMode));
	Snapshot.GrantedAbilityCount = GetActivatableAbilities().Num();
	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (!AbilitySpec.IsActive() || !IsValid(AbilitySpec.Ability))
		{
			continue;
		}

		++Snapshot.ActiveAbilityCount;
		Snapshot.ActiveAbilityTags.AppendTags(AbilitySpec.Ability->GetAssetTags());
	}
	Snapshot.ActiveGameplayEffectCount = GetActiveEffects(FGameplayEffectQuery()).Num();

	if (HasAttributeSetForAttribute(UWuwaHealthSet::GetHealthAttribute()))
	{
		Snapshot.Health = GetNumericAttribute(UWuwaHealthSet::GetHealthAttribute());
		Snapshot.MaxHealth = GetNumericAttribute(UWuwaHealthSet::GetMaxHealthAttribute());
		Snapshot.IncomingDamage = GetNumericAttribute(UWuwaHealthSet::GetIncomingDamageAttribute());
	}
	if (HasAttributeSetForAttribute(UWuwaResourceSet::GetStaminaAttribute()))
	{
		Snapshot.Stamina = GetNumericAttribute(UWuwaResourceSet::GetStaminaAttribute());
		Snapshot.MaxStamina = GetNumericAttribute(UWuwaResourceSet::GetMaxStaminaAttribute());
	}
	if (HasAttributeSetForAttribute(UWuwaCombatSet::GetAttackPowerAttribute()))
	{
		Snapshot.AttackPower = GetNumericAttribute(UWuwaCombatSet::GetAttackPowerAttribute());
		Snapshot.Defense = GetNumericAttribute(UWuwaCombatSet::GetDefenseAttribute());
	}
	Snapshot.DeadTagCount = GetGameplayTagCount(WuwaGameplayTags::State_Combat_Dead);
	return Snapshot;
}

bool UWuwaAbilitySystemComponent::TryInterruptActiveAbility(const EWuwaAbilityInterruptSource Source,
                                                            const FGameplayTag& IncomingActionTag)
{
	TArray<UWuwaGameplayAbility*> AbilitiesToInterrupt;

	// 只检查，不执行 Cancel
	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!Spec.IsActive())
		{
			continue;
		}

		UWuwaGameplayAbility* Ability = Cast<UWuwaGameplayAbility>(Spec.GetPrimaryInstance());

		if (!IsValid(Ability) || !Ability->BlocksInterruptSource(Source))
		{
			continue;
		}

		// 发现一个当前不能打断的 Ability，本次请求失败。
		if (!Ability->CanInterruptFrom(Source, IncomingActionTag))
		{
			return false;
		}

		AbilitiesToInterrupt.Add(Ability);
	}

	if (AbilitiesToInterrupt.IsEmpty())
	{
		return false;
	}

	// 前面全部验证通过之后再统一取消
	for (UWuwaGameplayAbility* Ability : AbilitiesToInterrupt)
	{
		if (!IsValid(Ability))
		{
			continue;
		}

		if (!Ability->TryInterruptFrom(Source, IncomingActionTag))
		{
			return false;
		}
	}

	return true;
}
