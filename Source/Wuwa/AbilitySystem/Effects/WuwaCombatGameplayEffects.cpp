// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Effects/WuwaCombatGameplayEffects.h"

#include "Combat/Attributes/WuwaCombatSet.h"
#include "Combat/Attributes/WuwaHealthSet.h"
#include "Combat/Attributes/WuwaPoiseSet.h"
#include "Combat/Attributes/WuwaResourceSet.h"
#include "Combat/Effects/WuwaDamageExecution.h"
#include "Core/WuwaGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

namespace
{
/** 玩家初始耐力契约值 */
constexpr float InitialPlayerStamina = 10000.f;

/**
 * 添加常量 GameplayEffect Modifier
 *
 * @param GameplayEffect	目标效果
 * @param Attribute		目标属性
 * @param Operation		运算类型
 * @param Magnitude		常量幅值
 */
void AddConstantModifier(UGameplayEffect& GameplayEffect,
                         const FGameplayAttribute& Attribute,
                         const EGameplayModOp::Type Operation,
                         const float Magnitude)
{
	FGameplayModifierInfo& Modifier = GameplayEffect.Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = Attribute;
	Modifier.ModifierOp = Operation;
	Modifier.ModifierMagnitude = FScalableFloat(Magnitude);
}
}

UWuwaGameplayEffect_InitPlayer::UWuwaGameplayEffect_InitPlayer()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddConstantModifier(*this, UWuwaHealthSet::GetMaxHealthAttribute(), EGameplayModOp::Override, 100.f);
	AddConstantModifier(*this, UWuwaHealthSet::GetHealthAttribute(), EGameplayModOp::Override, 100.f);
	AddConstantModifier(*this, UWuwaPoiseSet::GetMaxPoiseAttribute(), EGameplayModOp::Override, 30.f);
	AddConstantModifier(*this, UWuwaPoiseSet::GetPoiseAttribute(), EGameplayModOp::Override, 30.f);
	AddConstantModifier(
	    *this, UWuwaResourceSet::GetMaxStaminaAttribute(), EGameplayModOp::Override, InitialPlayerStamina);
	AddConstantModifier(*this, UWuwaResourceSet::GetStaminaAttribute(), EGameplayModOp::Override, InitialPlayerStamina);
	AddConstantModifier(*this, UWuwaCombatSet::GetAttackPowerAttribute(), EGameplayModOp::Override, 20.f);
	AddConstantModifier(*this, UWuwaCombatSet::GetDefenseAttribute(), EGameplayModOp::Override, 5.f);
}

UWuwaGameplayEffect_Cost_Stamina_SwordLight::UWuwaGameplayEffect_Cost_Stamina_SwordLight()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	AddConstantModifier(*this, UWuwaResourceSet::GetStaminaAttribute(), EGameplayModOp::Additive, -20.f);
}

UWuwaGameplayEffect_Cooldown_SwordLight::UWuwaGameplayEffect_Cooldown_SwordLight()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(0.6f);

	FInheritedTagContainer CooldownTags;
	CooldownTags.AddTag(WuwaGameplayTags::Cooldown_Combat_Attack_Light);
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
	    CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTagsComponent);
	TargetTagsComponent->SetAndApplyTargetTagChanges(CooldownTags);
}

UWuwaGameplayEffect_Damage_Sword::UWuwaGameplayEffect_Damage_Sword()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayEffectExecutionDefinition& Execution = Executions.AddDefaulted_GetRef();
	Execution.CalculationClass = UWuwaDamageExecution::StaticClass();
}

UWuwaGameplayEffect_Dead::UWuwaGameplayEffect_Dead()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FInheritedTagContainer DeadTags;
	DeadTags.AddTag(WuwaGameplayTags::State_Combat_Dead);
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
	    CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTagsComponent);
	TargetTagsComponent->SetAndApplyTargetTagChanges(DeadTags);
}

UWuwaGameplayEffect_State_Staggered::UWuwaGameplayEffect_State_Staggered()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FInheritedTagContainer StaggerTags;
	StaggerTags.AddTag(WuwaGameplayTags::State_Combat_Staggered);
	StaggerTags.AddTag(WuwaGameplayTags::Block_Input_Move);
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
	    CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTagsComponent);
	TargetTagsComponent->SetAndApplyTargetTagChanges(StaggerTags);
}

UWuwaGameplayEffect_Reset_Poise::UWuwaGameplayEffect_Reset_Poise()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat SetByCallerMagnitude;
	SetByCallerMagnitude.DataTag = WuwaGameplayTags::Data_Poise_Reset;
	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UWuwaPoiseSet::GetPoiseAttribute();
	Modifier.ModifierOp = EGameplayModOp::Override;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerMagnitude);
}
