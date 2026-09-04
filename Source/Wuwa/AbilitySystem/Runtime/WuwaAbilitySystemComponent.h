// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystem/Contracts/WuwaAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "WuwaAbilitySystemComponent.generated.h"

/** Wuwa 玩家能力系统组件 */
UCLASS()
class WUWA_API UWuwaAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	/**
	 * 创建 Wuwa 能力系统组件
	 *
	 * @param ObjectInitializer UObject 初始化器
	 */
	explicit UWuwaAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * 将活动 Ability 按下输入送入 GAS 通用复制事件
	 *
	 * @param Spec	目标 AbilitySpec
	 * @return 无
	 */
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;

	/**
	 * 将活动 Ability 释放输入送入 GAS 通用复制事件
	 *
	 * @param Spec	目标 AbilitySpec
	 * @return 无
	 */
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;

	// 当前是否存在会阻止指定输入域的活动 Wuwa Ability
	bool HasBlockingActiveAbility(EWuwaAbilityInterruptSource Source) const;

	// 当前所有阻止该输入的活动 Ability 是否都允许被打断
	bool CanInterruptActiveAbility(EWuwaAbilityInterruptSource Source,
	                               const FGameplayTag& IncomingActionTag = FGameplayTag()) const;

	// 尝试使用指定输入来源打断当前 Blocking Ability
	bool TryInterruptActiveAbility(EWuwaAbilityInterruptSource Source,
	                               const FGameplayTag& IncomingActionTag = FGameplayTag());

	/** @return 当前 ASC、Ability、Effect 与战斗属性的只读快照 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|AbilitySystem|Debug")
	FWuwaAbilitySystemRuntimeSnapshot GetRuntimeSnapshot() const;
};
