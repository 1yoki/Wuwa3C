// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Combat/Contracts/WuwaCombatFacts.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "WuwaPoiseComponent.generated.h"

class UWuwaAbilitySystemComponent;
struct FOnAttributeChangeData;

/** 绑定 ASC 韧性属性并在 Authority 发布破韧事实与事件 */
UCLASS(ClassGroup = "Wuwa")
class WUWA_API UWuwaPoiseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 创建不参与 Tick 和复制的韧性组件 */
	UWuwaPoiseComponent();

	/**
	 * 绑定当前 Avatar 的 ASC 与 Poise 属性
	 *
	 * @param AbilitySystemComponent	当前 PlayerState 持有的 Wuwa ASC
	 * @return 是否完成幂等绑定
	 */
	bool Initialize(UWuwaAbilitySystemComponent* AbilitySystemComponent);

	/** @return 无；对称解绑属性委托并取消待发布事实 */
	void Shutdown();

	/** @return 是否已绑定有效 ASC */
	bool IsInitialized() const;

	/** @return 当前 PoiseComponent 只读快照 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Poise")
	FWuwaPoiseRuntimeSnapshot GetRuntimeSnapshot() const;

	/** @return 类型化破韧事实委托 */
	FWuwaPoiseBreakFactDelegate& OnPoiseBreak();

protected:
	//~ Begin UActorComponent Interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

private:
	/** 当前绑定的 ASC */
	TWeakObjectPtr<UWuwaAbilitySystemComponent> AbilitySystemComponent;

	/** Poise 属性回调 Handle */
	FDelegateHandle PoiseChangedDelegateHandle;

	/** 下一帧破韧复核定时器 */
	FTimerHandle PendingBreakTimerHandle;

	/** 最近一次属性回调前的韧性值 */
	float LastPreviousPoise = 0.f;

	/** 最近一次记录的韧性值 */
	float LastCurrentPoise = 0.f;

	/** 最近一次 Poise 下降量 */
	float LastPoiseDamage = 0.f;

	/** 最近发布的破韧事实序号 */
	int32 BreakSequence = 0;

	/** 当前待复核的破韧事实序号 */
	int32 PendingBreakSequence = 0;

	/** 是否正在等待下一帧复核破韧 */
	bool bPendingBreak = false;

	/** 类型化破韧事实委托 */
	FWuwaPoiseBreakFactDelegate PoiseBreakFactDelegate;

	/**
	 * 接收 Poise 属性变化
	 *
	 * @param ChangeData	GAS 属性变化数据
	 */
	void HandlePoiseChanged(const FOnAttributeChangeData& ChangeData);

	/** 请求 Authority 在下一帧复核一次破韧边沿 */
	void ScheduleAuthorityBreakReview();

	/**
	 * 复核待发布破韧事实
	 *
	 * @param ExpectedSequence	调度时冻结的事实序号
	 */
	void ReviewPendingBreak(int32 ExpectedSequence);

	/** 发布一次破韧事实与标准 Gameplay Event */
	void PublishPoiseBreak();

	/** 取消当前待复核破韧 */
	void CancelPendingBreak();

	/** 清理绑定字段但保留最近破韧快照 */
	void ResetBindingState();
};
