// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Effects/WuwaDamageExecution.h"

#include "Combat/Attributes/WuwaCombatSet.h"
#include "Combat/Attributes/WuwaHealthSet.h"
#include "Combat/Attributes/WuwaPoiseSet.h"
#include "Combat/WuwaCombatLog.h"
#include "Core/WuwaGameplayTags.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"

#include <limits>

namespace
{
/** 伤害公式的属性捕获定义 */
struct FWuwaDamageCaptureDefinitions
{
	/** 来源攻击力捕获 */
	FGameplayEffectAttributeCaptureDefinition AttackPowerDef;

	/** 目标防御力捕获 */
	FGameplayEffectAttributeCaptureDefinition DefenseDef;

	/** 配置来源攻击力与目标防御力捕获 */
	FWuwaDamageCaptureDefinitions()
	{
		AttackPowerDef = FGameplayEffectAttributeCaptureDefinition(
		    UWuwaCombatSet::GetAttackPowerAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);
		DefenseDef = FGameplayEffectAttributeCaptureDefinition(
		    UWuwaCombatSet::GetDefenseAttribute(), EGameplayEffectAttributeCaptureSource::Target, true);
	}
};

/** @return 进程内唯一的伤害捕获定义 */
const FWuwaDamageCaptureDefinitions& GetDamageCaptureDefinitions()
{
	static const FWuwaDamageCaptureDefinitions Definitions;
	return Definitions;
}

/**
 * 把伤害输入约束为有限非负值
 *
 * @param Value		候选数值
 * @param ValueName	诊断字段名
 * @return 有限非负值或安全值零
 */
float SanitizeDamageInput(const float Value, const TCHAR* ValueName)
{
	if (FMath::IsFinite(Value))
	{
		return FMath::Max(0.f, Value);
	}

	UE_LOG(LogWuwaCombat, Warning, TEXT("伤害结算拒绝非有限输入。Field=%s, Value=%f, Fallback=0"), ValueName, Value);
	return 0.f;
}
}

UWuwaDamageExecution::UWuwaDamageExecution()
{
	const FWuwaDamageCaptureDefinitions& Definitions = GetDamageCaptureDefinitions();
	RelevantAttributesToCapture.Add(Definitions.AttackPowerDef);
	RelevantAttributesToCapture.Add(Definitions.DefenseDef);
}

void UWuwaDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
                                                  FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const FWuwaDamageCaptureDefinitions& Definitions = GetDamageCaptureDefinitions();
	FAggregatorEvaluateParameters EvaluationParameters;
	EvaluationParameters.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvaluationParameters.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	float CapturedAttackPower = 0.f;
	if (!ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
	        Definitions.AttackPowerDef, EvaluationParameters, CapturedAttackPower))
	{
		UE_LOG(LogWuwaCombat, Warning, TEXT("伤害结算无法捕获来源 AttackPower，使用安全值零"));
		CapturedAttackPower = 0.f;
	}

	float CapturedDefense = 0.f;
	if (!ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
	        Definitions.DefenseDef, EvaluationParameters, CapturedDefense))
	{
		UE_LOG(LogWuwaCombat, Warning, TEXT("伤害结算无法捕获目标 Defense，使用安全值零"));
		CapturedDefense = 0.f;
	}

	const float MissingMagnitude = std::numeric_limits<float>::quiet_NaN();
	const float RawBaseDamage =
	    Spec.GetSetByCallerMagnitude(WuwaGameplayTags::Data_Damage_Base, false, MissingMagnitude);
	if (!FMath::IsFinite(RawBaseDamage))
	{
		UE_LOG(LogWuwaCombat,
		       Warning,
		       TEXT("伤害结算缺少或收到非法 Data.Damage.Base，使用安全值零。Source=%s, Target=%s"),
		       *GetNameSafe(ExecutionParams.GetSourceAbilitySystemComponent()),
		       *GetNameSafe(ExecutionParams.GetTargetAbilitySystemComponent()));
	}

	const float BaseDamage =
	    FMath::IsFinite(RawBaseDamage) ? SanitizeDamageInput(RawBaseDamage, TEXT("BaseDamage")) : 0.f;
	const float AttackPower = SanitizeDamageInput(CapturedAttackPower, TEXT("AttackPower"));
	const float Defense = SanitizeDamageInput(CapturedDefense, TEXT("Defense"));
	const float RawFinalDamage = BaseDamage + AttackPower - Defense;
	const float FinalDamage = FMath::IsFinite(RawFinalDamage) ? FMath::Max(0.f, RawFinalDamage) : 0.f;

	if (!FMath::IsNearlyZero(FinalDamage))
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
		    UWuwaHealthSet::GetIncomingDamageAttribute(), EGameplayModOp::Additive, FinalDamage));
	}

	float PoiseDamage = 0.f;
	if (const float* RawPoiseDamage = Spec.SetByCallerTagMagnitudes.Find(WuwaGameplayTags::Data_Damage_Poise))
	{
		if (!FMath::IsFinite(*RawPoiseDamage))
		{
			UE_LOG(LogWuwaCombat,
			       Warning,
			       TEXT("伤害结算收到非法 Data.Damage.Poise，使用安全值零。Source=%s, Target=%s"),
			       *GetNameSafe(ExecutionParams.GetSourceAbilitySystemComponent()),
			       *GetNameSafe(ExecutionParams.GetTargetAbilitySystemComponent()));
		}
		else
		{
			PoiseDamage = FMath::Max(0.f, *RawPoiseDamage);
		}
	}

	if (!FMath::IsNearlyZero(PoiseDamage))
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
		    UWuwaPoiseSet::GetIncomingPoiseDamageAttribute(), EGameplayModOp::Additive, PoiseDamage));
	}
}
