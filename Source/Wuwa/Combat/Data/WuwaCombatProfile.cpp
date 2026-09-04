// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Data/WuwaCombatProfile.h"

#include "AbilitySystem/Data/WuwaAbilitySet.h"
#include "Combat/Data/WuwaMeleeAttackDefinition.h"
#include "Combat/Data/WuwaWeaponDefinition.h"
#include "Core/WuwaGameplayTags.h"
#include "GameplayEffect.h"
#include "Misc/DataValidation.h"

bool UWuwaCombatProfile::IsRuntimeValid() const
{
	FString AbilitySetFailureReason;
	const UGameplayEffect* DeadEffect =
	    IsValid(DeadEffectClass) ? DeadEffectClass->GetDefaultObject<UGameplayEffect>() : nullptr;
	return IsValid(DefaultAbilitySet) &&
	       DefaultAbilitySet->ValidateEntries(DefaultAbilitySet->GetGrantedAbilities(),
	                                          DefaultAbilitySet->GetGrantedEffects(),
	                                          AbilitySetFailureReason) &&
	       IsValid(DefaultWeaponDefinition) && DefaultWeaponDefinition->IsRuntimeValid() &&
	       IsValid(DefaultAttackDefinition) && DefaultAttackDefinition->IsRuntimeValid() && IsValid(DeadEffect) &&
	       DeadEffect->DurationPolicy == EGameplayEffectDurationType::Infinite &&
	       DeadEffect->GetGrantedTags().HasTagExact(WuwaGameplayTags::State_Combat_Dead);
}

#if WITH_EDITOR

EDataValidationResult UWuwaCombatProfile::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!IsRuntimeValid())
	{
		Context.AddError(FText::FromString(
		    TEXT("CombatProfile 的 AbilitySet、WeaponDefinition、AttackDefinition 或 DeadEffect 无效")));
		Result = EDataValidationResult::Invalid;
	}

	return Result == EDataValidationResult::Invalid ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}

#endif
