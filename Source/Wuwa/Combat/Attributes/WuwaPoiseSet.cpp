// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Attributes/WuwaPoiseSet.h"

#include "AbilitySystem/WuwaAbilitySystemLog.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

namespace
{
/**
 * 将韧性属性候选值约束为有限数值
 *
 * @param AttributeName	属性诊断名
 * @param CandidateValue	候选值
 * @param FallbackValue	非法候选值的回退值
 * @return 有限候选值或安全回退值
 */
float SanitizePoiseValue(const TCHAR* AttributeName, const float CandidateValue, const float FallbackValue)
{
	if (FMath::IsFinite(CandidateValue))
	{
		return CandidateValue;
	}

	UE_LOG(LogWuwaAbility,
	       Warning,
	       TEXT("韧性属性拒绝非有限数值。Attribute=%s, Candidate=%f, Fallback=%f"),
	       AttributeName,
	       CandidateValue,
	       FallbackValue);
	return FMath::IsFinite(FallbackValue) ? FallbackValue : 0.f;
}
}

void UWuwaPoiseSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaPoiseSet, Poise, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaPoiseSet, MaxPoise, COND_None, REPNOTIFY_Always);
}

void UWuwaPoiseSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxPoiseAttribute())
	{
		NewValue = FMath::Max(0.f, SanitizePoiseValue(TEXT("MaxPoise"), NewValue, GetMaxPoise()));
		const float ClampedPoise = FMath::Min(GetPoise(), NewValue);
		if (!FMath::IsNearlyEqual(ClampedPoise, GetPoise()))
		{
			if (UAbilitySystemComponent* AbilitySystemComponent = GetOwningAbilitySystemComponent())
			{
				AbilitySystemComponent->ApplyModToAttributeUnsafe(
				    GetPoiseAttribute(), EGameplayModOp::Additive, ClampedPoise - GetPoise());
			}
		}
	}
	else if (Attribute == GetPoiseAttribute())
	{
		NewValue = FMath::Clamp(SanitizePoiseValue(TEXT("Poise"), NewValue, GetPoise()), 0.f, GetMaxPoise());
	}
	else if (Attribute == GetIncomingPoiseDamageAttribute())
	{
		NewValue = FMath::Max(0.f, SanitizePoiseValue(TEXT("IncomingPoiseDamage"), NewValue, 0.f));
	}
}

void UWuwaPoiseSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	if (Attribute == GetMaxPoiseAttribute())
	{
		NewValue = FMath::Max(0.f, SanitizePoiseValue(TEXT("MaxPoise.Base"), NewValue, GetMaxPoise()));
	}
	else if (Attribute == GetPoiseAttribute())
	{
		NewValue = FMath::Clamp(SanitizePoiseValue(TEXT("Poise.Base"), NewValue, GetPoise()), 0.f, GetMaxPoise());
	}
	else if (Attribute == GetIncomingPoiseDamageAttribute())
	{
		NewValue = FMath::Max(0.f, SanitizePoiseValue(TEXT("IncomingPoiseDamage.Base"), NewValue, 0.f));
	}
}

void UWuwaPoiseSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayAttribute& Attribute = Data.EvaluatedData.Attribute;
	if (Attribute == GetMaxPoiseAttribute())
	{
		SetMaxPoise(FMath::Max(0.f, SanitizePoiseValue(TEXT("MaxPoise.Execute"), GetMaxPoise(), 0.f)));
		SetPoise(FMath::Clamp(SanitizePoiseValue(TEXT("Poise.AfterMax"), GetPoise(), 0.f), 0.f, GetMaxPoise()));
	}
	else if (Attribute == GetPoiseAttribute())
	{
		SetPoise(FMath::Clamp(SanitizePoiseValue(TEXT("Poise.Execute"), GetPoise(), 0.f), 0.f, GetMaxPoise()));
	}
	else if (Attribute == GetIncomingPoiseDamageAttribute())
	{
		const float PendingDamage =
		    FMath::Max(0.f, SanitizePoiseValue(TEXT("IncomingPoiseDamage.Execute"), GetIncomingPoiseDamage(), 0.f));
		SetIncomingPoiseDamage(0.f);
		if (PendingDamage > 0.f)
		{
			const float NewPoise = FMath::Clamp(GetPoise() - PendingDamage, 0.f, GetMaxPoise());
			SetPoise(NewPoise);
		}
	}
}

void UWuwaPoiseSet::OnRep_Poise(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaPoiseSet, Poise, OldValue);
}

void UWuwaPoiseSet::OnRep_MaxPoise(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaPoiseSet, MaxPoise, OldValue);
}
