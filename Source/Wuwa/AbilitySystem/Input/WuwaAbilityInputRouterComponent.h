// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystem/Contracts/WuwaAbilityTypes.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "Messaging/WuwaMessageTypes.h"
#include "WuwaAbilityInputRouterComponent.generated.h"

class UWuwaAbilitySystemComponent;

/** 把有序 InputTag 边沿转换为 GAS AbilitySpec 输入的角色组件 */
UCLASS(ClassGroup = "Wuwa")
class WUWA_API UWuwaAbilityInputRouterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 创建不参与 Tick 和复制的输入路由组件 */
	UWuwaAbilityInputRouterComponent();

	/**
	 * 绑定当前 Pawn 使用的 ASC
	 *
	 * @param InAbilitySystemComponent	当前 PlayerState 持有的 Wuwa ASC
	 * @return 是否完成初始化
	 */
	bool Initialize(UWuwaAbilitySystemComponent* InAbilitySystemComponent);

	/** 释放当前 ASC 引用并清理输入 */
	void Shutdown();

	/**
	 * 接收一条 InputTag 按下边沿
	 *
	 * @param InputTag	输入语义
	 * @param Header	有序消息头
	 */
	void AbilityInputTagPressed(FGameplayTag InputTag, const FWuwaMessageHeader& Header);

	/**
	 * 接收一条 InputTag 释放边沿
	 *
	 * @param InputTag	输入语义
	 * @param Header	有序消息头
	 */
	void AbilityInputTagReleased(FGameplayTag InputTag, const FWuwaMessageHeader& Header);

	/**
	 * 按稳定顺序向 ASC 提交本帧输入
	 *
	 * @param DeltaTime	当前输入帧步长
	 * @param bGamePaused	游戏是否暂停
	 */
	void ProcessAbilityInput(float DeltaTime, bool bGamePaused);

	/** 清空 Pressed、Released 和 Held 集合 */
	void ClearAbilityInput();

	/** @return 当前输入路由只读快照 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|AbilitySystem|Input")
	FWuwaAbilityInputRouterRuntimeSnapshot GetRuntimeSnapshot() const;

	/** @return 是否绑定有效 ASC */
	bool IsInitialized() const;

protected:
	//~ Begin UActorComponent Interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

private:
	/** 当前 PlayerState 持有的 ASC */
	TWeakObjectPtr<UWuwaAbilitySystemComponent> AbilitySystemComponent;

	/** 本帧按下的 AbilitySpec */
	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;

	/** 本帧释放的 AbilitySpec */
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;

	/** 当前仍处于按住状态的 AbilitySpec */
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;

	/** 最近接受的输入序号 */
	int32 LastAcceptedSequence = 0;

	/** 最近接受的输入标签 */
	FGameplayTag LastAcceptedInputTag;

	/** 最近尝试激活的 AbilitySpec Handle */
	FGameplayAbilitySpecHandle LastActivationSpecHandle;

	/** 最近转发活动输入的 AbilitySpec Handle */
	FGameplayAbilitySpecHandle LastForwardedInputSpecHandle;

	/** 最近一次激活尝试是否成功 */
	bool bLastActivationSucceeded = false;

	/** 最近一次激活失败原因 */
	EWuwaAbilityActivationDebugFailure LastActivationFailure = EWuwaAbilityActivationDebugFailure::None;

	/** 累计激活尝试次数 */
	int32 ActivationAttemptCount = 0;

	/** 累计激活成功次数 */
	int32 ActivationSuccessCount = 0;

	/** 累计转发活动 Ability 输入的次数 */
	int32 ActiveInputForwardCount = 0;

	/**
	 * 收集动态标签精确匹配的 AbilitySpec Handle
	 *
	 * @param InputTag	目标输入标签
	 * @param OutHandles	接收匹配 Handle
	 */
	void GatherMatchingAbilitySpecHandles(const FGameplayTag& InputTag,
	                                      TArray<FGameplayAbilitySpecHandle>& OutHandles) const;

	/**
	 * 验证输入并推进全局单调序号
	 *
	 * @param InputTag	输入语义
	 * @param Header	有序消息头
	 * @return 是否接受本次输入
	 */
	bool AcceptInput(const FGameplayTag& InputTag, const FWuwaMessageHeader& Header);

	/**
	 * 记录一次不影响 Gameplay 的激活诊断结果
	 *
	 * @param Handle		AbilitySpec Handle
	 * @param bSucceeded	激活是否成功
	 * @param Failure	失败原因
	 */
	void RecordActivationResult(FGameplayAbilitySpecHandle Handle,
	                            bool bSucceeded,
	                            EWuwaAbilityActivationDebugFailure Failure);

	/**
	 * 记录一次活动 Ability 输入转发
	 *
	 * @param Handle	AbilitySpec Handle
	 * @return 无
	 */
	void RecordActiveInputForwarded(FGameplayAbilitySpecHandle Handle);
};
