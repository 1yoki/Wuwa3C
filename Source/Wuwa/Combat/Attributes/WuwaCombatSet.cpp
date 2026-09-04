// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Attributes/WuwaCombatSet.h"

#include "AbilitySystem/WuwaAbilitySystemLog.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

namespace
{
/**
 * 将战斗属性候选值约束为非负有限数值
 *
 * @param AttributeName	属性诊断名
 * @param CandidateValue	候选值
 * @param FallbackValue	非法候选值的回退值
 * @return 非负有限候选值或安全回退值
 */
float SanitizeCombatValue(const TCHAR* AttributeName, const float CandidateValue, const float FallbackValue)
{
	if (FMath::IsFinite(CandidateValue))
	{
		return FMath::Max(0.f, CandidateValue);
	}

	UE_LOG(LogWuwaAbility,
	       Warning,
	       TEXT("战斗属性拒绝非有限数值。Attribute=%s, Candidate=%f, Fallback=%f"),
	       AttributeName,
	       CandidateValue,
	       FallbackValue);
	return FMath::IsFinite(FallbackValue) ? FMath::Max(0.f, FallbackValue) : 0.f;
}
}

void UWuwaCombatSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaCombatSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaCombatSet, Defense, COND_None, REPNOTIFY_Always);
}

void UWuwaCombatSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetAttackPowerAttribute())
	{
		NewValue = SanitizeCombatValue(TEXT("AttackPower"), NewValue, GetAttackPower());
	}
	else if (Attribute == GetDefenseAttribute())
	{
		NewValue = SanitizeCombatValue(TEXT("Defense"), NewValue, GetDefense());
	}
}

void UWuwaCombatSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	if (Attribute == GetAttackPowerAttribute())
	{
		NewValue = SanitizeCombatValue(TEXT("AttackPower.Base"), NewValue, GetAttackPower());
	}
	else if (Attribute == GetDefenseAttribute())
	{
		NewValue = SanitizeCombatValue(TEXT("Defense.Base"), NewValue, GetDefense());
	}
}

void UWuwaCombatSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayAttribute& Attribute = Data.EvaluatedData.Attribute;
	if (Attribute == GetAttackPowerAttribute())
	{
		SetAttackPower(SanitizeCombatValue(TEXT("AttackPower.Execute"), GetAttackPower(), 0.f));
	}
	else if (Attribute == GetDefenseAttribute())
	{
		SetDefense(SanitizeCombatValue(TEXT("Defense.Execute"), GetDefense(), 0.f));
	}
}

void UWuwaCombatSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaCombatSet, AttackPower, OldValue);
}

void UWuwaCombatSet::OnRep_Defense(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaCombatSet, Defense, OldValue);
}
