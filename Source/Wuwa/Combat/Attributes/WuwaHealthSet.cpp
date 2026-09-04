// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Attributes/WuwaHealthSet.h"

#include "AbilitySystem/WuwaAbilitySystemLog.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

namespace
{
/**
 * 将生命属性候选值约束为有限数值
 *
 * @param AttributeName	属性诊断名
 * @param CandidateValue	候选值
 * @param FallbackValue	非法候选值的回退值
 * @return 有限候选值或安全回退值
 */
float SanitizeHealthValue(const TCHAR* AttributeName, const float CandidateValue, const float FallbackValue)
{
	if (FMath::IsFinite(CandidateValue))
	{
		return CandidateValue;
	}

	UE_LOG(LogWuwaAbility,
	       Warning,
	       TEXT("生命属性拒绝非有限数值。Attribute=%s, Candidate=%f, Fallback=%f"),
	       AttributeName,
	       CandidateValue,
	       FallbackValue);
	return FMath::IsFinite(FallbackValue) ? FallbackValue : 0.f;
}
}

void UWuwaHealthSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaHealthSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaHealthSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UWuwaHealthSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(0.f, SanitizeHealthValue(TEXT("MaxHealth"), NewValue, GetMaxHealth()));
		const float ClampedHealth = FMath::Min(GetHealth(), NewValue);
		if (!FMath::IsNearlyEqual(ClampedHealth, GetHealth()))
		{
			if (UAbilitySystemComponent* AbilitySystemComponent = GetOwningAbilitySystemComponent())
			{
				AbilitySystemComponent->ApplyModToAttributeUnsafe(
				    GetHealthAttribute(), EGameplayModOp::Additive, ClampedHealth - GetHealth());
			}
		}
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(SanitizeHealthValue(TEXT("Health"), NewValue, GetHealth()), 0.f, GetMaxHealth());
	}
	else if (Attribute == GetIncomingDamageAttribute())
	{
		NewValue = FMath::Max(0.f, SanitizeHealthValue(TEXT("IncomingDamage"), NewValue, 0.f));
	}
}

void UWuwaHealthSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(0.f, SanitizeHealthValue(TEXT("MaxHealth.Base"), NewValue, GetMaxHealth()));
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(SanitizeHealthValue(TEXT("Health.Base"), NewValue, GetHealth()), 0.f, GetMaxHealth());
	}
	else if (Attribute == GetIncomingDamageAttribute())
	{
		NewValue = FMath::Max(0.f, SanitizeHealthValue(TEXT("IncomingDamage.Base"), NewValue, 0.f));
	}
}

void UWuwaHealthSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayAttribute& Attribute = Data.EvaluatedData.Attribute;
	if (Attribute == GetMaxHealthAttribute())
	{
		SetMaxHealth(FMath::Max(0.f, SanitizeHealthValue(TEXT("MaxHealth.Execute"), GetMaxHealth(), 0.f)));
		SetHealth(FMath::Clamp(SanitizeHealthValue(TEXT("Health.AfterMax"), GetHealth(), 0.f), 0.f, GetMaxHealth()));
	}
	else if (Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(SanitizeHealthValue(TEXT("Health.Execute"), GetHealth(), 0.f), 0.f, GetMaxHealth()));
	}
	else if (Attribute == GetIncomingDamageAttribute())
	{
		const float PendingDamage =
		    FMath::Max(0.f, SanitizeHealthValue(TEXT("IncomingDamage.Execute"), GetIncomingDamage(), 0.f));
		SetIncomingDamage(0.f);
		if (PendingDamage > 0.f)
		{
			const float NewHealth = FMath::Clamp(GetHealth() - PendingDamage, 0.f, GetMaxHealth());
			SetHealth(NewHealth);
		}
	}
}

void UWuwaHealthSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaHealthSet, Health, OldValue);
}

void UWuwaHealthSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaHealthSet, MaxHealth, OldValue);
}
