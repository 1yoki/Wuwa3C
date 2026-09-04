// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Attributes/WuwaResourceSet.h"

#include "AbilitySystem/WuwaAbilitySystemLog.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

namespace
{
/**
 * 将资源属性候选值约束为有限数值
 *
 * @param AttributeName	属性诊断名
 * @param CandidateValue	候选值
 * @param FallbackValue	非法候选值的回退值
 * @return 有限候选值或安全回退值
 */
float SanitizeResourceValue(const TCHAR* AttributeName, const float CandidateValue, const float FallbackValue)
{
	if (FMath::IsFinite(CandidateValue))
	{
		return CandidateValue;
	}

	UE_LOG(LogWuwaAbility,
	       Warning,
	       TEXT("资源属性拒绝非有限数值。Attribute=%s, Candidate=%f, Fallback=%f"),
	       AttributeName,
	       CandidateValue,
	       FallbackValue);
	return FMath::IsFinite(FallbackValue) ? FallbackValue : 0.f;
}
}

void UWuwaResourceSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaResourceSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaResourceSet, MaxStamina, COND_None, REPNOTIFY_Always);
}

void UWuwaResourceSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(0.f, SanitizeResourceValue(TEXT("MaxStamina"), NewValue, GetMaxStamina()));
		const float ClampedStamina = FMath::Min(GetStamina(), NewValue);
		if (!FMath::IsNearlyEqual(ClampedStamina, GetStamina()))
		{
			if (UAbilitySystemComponent* AbilitySystemComponent = GetOwningAbilitySystemComponent())
			{
				AbilitySystemComponent->ApplyModToAttributeUnsafe(
				    GetStaminaAttribute(), EGameplayModOp::Additive, ClampedStamina - GetStamina());
			}
		}
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(SanitizeResourceValue(TEXT("Stamina"), NewValue, GetStamina()), 0.f, GetMaxStamina());
	}
}

void UWuwaResourceSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	if (Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(0.f, SanitizeResourceValue(TEXT("MaxStamina.Base"), NewValue, GetMaxStamina()));
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue =
		    FMath::Clamp(SanitizeResourceValue(TEXT("Stamina.Base"), NewValue, GetStamina()), 0.f, GetMaxStamina());
	}
}

void UWuwaResourceSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayAttribute& Attribute = Data.EvaluatedData.Attribute;
	if (Attribute == GetMaxStaminaAttribute())
	{
		SetMaxStamina(FMath::Max(0.f, SanitizeResourceValue(TEXT("MaxStamina.Execute"), GetMaxStamina(), 0.f)));
		SetStamina(
		    FMath::Clamp(SanitizeResourceValue(TEXT("Stamina.AfterMax"), GetStamina(), 0.f), 0.f, GetMaxStamina()));
	}
	else if (Attribute == GetStaminaAttribute())
	{
		SetStamina(
		    FMath::Clamp(SanitizeResourceValue(TEXT("Stamina.Execute"), GetStamina(), 0.f), 0.f, GetMaxStamina()));
	}
}

void UWuwaResourceSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaResourceSet, Stamina, OldValue);
}

void UWuwaResourceSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaResourceSet, MaxStamina, OldValue);
}
