// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Combat/Contracts/WuwaCombatTypes.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WuwaMeleeAttackDefinition.generated.h"

class UAnimMontage;

/** 单段近战攻击的数据契约 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaMeleeAttackStep
{
	GENERATED_BODY()

	/** Montage Section 名称 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	FName MontageSection;

	/** Health 基础伤害 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Combat|Attack", meta = (ClampMin = "0.0"))
	float BaseDamage = 20.0f;

	/** Poise 基础伤害 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Combat|Attack", meta = (ClampMin = "0.0"))
	float PoiseDamage = 0.0f;

	/** 当前攻击段包含的独立伤害窗口数量；每个窗口代表一次独立砍击 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Attack",
	          meta = (ClampMin = "1", ClampMax = "8"))
	int32 HitWindowCount = 1;

	/** 当前段的轨迹配置 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	FWuwaMeleeTraceSpec TraceSpec;

	/** 是否允许同一窗口命中多个不同目标 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	bool bAllowMultipleTargets = true;

	/** 单窗口最多命中的不同目标数量 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Attack",
	          meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaxTargets = 4;

	/** 挥剑表现标签 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Attack",
	          meta = (Categories = "GameplayCue.Combat"))
	FGameplayTag SwingCueTag;

	/** 命中表现标签 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Attack",
	          meta = (Categories = "GameplayCue.Combat"))
	FGameplayTag HitCueTag;
};

/** 单次近战攻击的动画、轨迹和表现数据定义 */
UCLASS(BlueprintType, Const)
class WUWA_API UWuwaMeleeAttackDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 严格三段攻击要求的段数 */
	static constexpr int32 RequiredStepCount = 3;

	/** @return 攻击语义标签 */
	const FGameplayTag& GetAttackTag() const
	{
		return AttackTag;
	}

	/** @return 攻击 Montage */
	UAnimMontage* GetMontage() const
	{
		return Montage;
	}

	/** @return Montage 播放倍率 */
	float GetPlayRate() const
	{
		return PlayRate;
	}

	/** @return 攻击朝向策略 */
	EWuwaAttackFacingPolicy GetFacingPolicy() const
	{
		return FacingPolicy;
	}

	/** @return 攻击 Montage 的显式 Root Motion 策略 */
	EWuwaAttackRootMotionPolicy GetRootMotionPolicy() const
	{
		return RootMotionPolicy;
	}

	/** @return 当前攻击是否要求由 Montage Root Motion 驱动 */
	bool UsesMontageRootMotion() const
	{
		return RootMotionPolicy == EWuwaAttackRootMotionPolicy::MontageDriven;
	}

	/** @return 严格三段攻击数据 */
	const TArray<FWuwaMeleeAttackStep>& GetSteps() const
	{
		return Steps;
	}

	/**
	 * 获取指定攻击段
	 *
	 * @param StepIndex	攻击段索引
	 * @return 索引有效时返回对应攻击段，否则返回 nullptr
	 */
	const FWuwaMeleeAttackStep* GetStep(int32 StepIndex) const
	{
		return Steps.IsValidIndex(StepIndex) ? &Steps[StepIndex] : nullptr;
	}

	/** @return 是否明确标记为占位攻击 */
	bool IsPlaceholder() const
	{
		return bIsPlaceholder;
	}

	/** @return 运行时关键字段、Section、Notify 和 Root Motion 是否有效 */
	bool IsRuntimeValid() const;

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
	//~ End UObject Interface
#endif

private:
	/** 攻击语义标签 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Attack",
	          meta = (AllowPrivateAccess = "true", Categories = "Ability.Combat.Attack"))
	FGameplayTag AttackTag;

	/** 攻击动画 Montage */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Attack",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> Montage;

	/** Montage 播放倍率 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Attack",
	          meta = (AllowPrivateAccess = "true", ClampMin = "0.01"))
	float PlayRate = 1.0f;

	/** 第一阶段保持角色现有朝向 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Attack",
	          meta = (AllowPrivateAccess = "true"))
	EWuwaAttackFacingPolicy FacingPolicy = EWuwaAttackFacingPolicy::PreserveFacing;

	/** 攻击 Montage 的显式 Root Motion 策略 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Attack",
	          meta = (AllowPrivateAccess = "true"))
	EWuwaAttackRootMotionPolicy RootMotionPolicy = EWuwaAttackRootMotionPolicy::Unspecified;

	/** 严格三段攻击数据 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          EditFixedSize,
	          Category = "Wuwa|Combat|Attack",
	          meta = (AllowPrivateAccess = "true"))
	TArray<FWuwaMeleeAttackStep> Steps;

	/** 是否为第一阶段占位资产 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Attack",
	          meta = (AllowPrivateAccess = "true"))
	bool bIsPlaceholder = false;
};
