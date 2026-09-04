// Copyright Epic Games, Inc. All Rights Reserved.

#include "WuwaPlayerState.h"

#include "AbilitySystem/Runtime/WuwaAbilitySystemComponent.h"
#include "AbilitySystem/WuwaAbilitySystemLog.h"
#include "Combat/Attributes/WuwaCombatSet.h"
#include "Combat/Attributes/WuwaHealthSet.h"
#include "Combat/Attributes/WuwaPoiseSet.h"
#include "Combat/Attributes/WuwaResourceSet.h"

AWuwaPlayerState::AWuwaPlayerState()
{
	NetUpdateFrequency = 60.0f;
	MinNetUpdateFrequency = 30.0f;

	AbilitySystemComponent = CreateDefaultSubobject<UWuwaAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	HealthSet = CreateDefaultSubobject<UWuwaHealthSet>(TEXT("HealthSet"));
	PoiseSet = CreateDefaultSubobject<UWuwaPoiseSet>(TEXT("PoiseSet"));
	ResourceSet = CreateDefaultSubobject<UWuwaResourceSet>(TEXT("ResourceSet"));
	CombatSet = CreateDefaultSubobject<UWuwaCombatSet>(TEXT("CombatSet"));

	if (!AbilitySystemComponent || !HealthSet || !PoiseSet || !ResourceSet || !CombatSet)
	{
		UE_LOG(LogWuwaAbility,
		       Error,
		       TEXT("PlayerState GAS 基础对象创建失败。Owner=%s, ASC=%s, HealthSet=%s, PoiseSet=%s, ResourceSet=%s, "
		            "CombatSet=%s"),
		       *GetNameSafe(this),
		       *GetNameSafe(AbilitySystemComponent),
		       *GetNameSafe(HealthSet),
		       *GetNameSafe(PoiseSet),
		       *GetNameSafe(ResourceSet),
		       *GetNameSafe(CombatSet));
		return;
	}

	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

UAbilitySystemComponent* AWuwaPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UWuwaAbilitySystemComponent* AWuwaPlayerState::GetWuwaAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

const UWuwaHealthSet* AWuwaPlayerState::GetHealthSet() const
{
	return HealthSet;
}

const UWuwaPoiseSet* AWuwaPlayerState::GetPoiseSet() const
{
	return PoiseSet;
}

const UWuwaResourceSet* AWuwaPlayerState::GetResourceSet() const
{
	return ResourceSet;
}

const UWuwaCombatSet* AWuwaPlayerState::GetCombatSet() const
{
	return CombatSet;
}
