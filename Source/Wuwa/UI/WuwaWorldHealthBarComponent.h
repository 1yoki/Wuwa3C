// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/WidgetComponent.h"
#include "TimerManager.h"
#include "WuwaWorldHealthBarComponent.generated.h"

class UWuwaHealthComponent;
class UWuwaWorldHealthBarWidget;
struct FWuwaHealthChangedFact;

/** 世界血条组件对 Debug 与测试暴露的只读快照 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaWorldHealthBarRuntimeSnapshot
{
	GENERATED_BODY()

	/** 是否找到同 Owner 的生命事实源 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|UI|WorldHealthBar")
	bool bSourceFound = false;

	/** 是否已经创建目标 Widget */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|UI|WorldHealthBar")
	bool bWidgetCreated = false;

	/** 目标 Widget 是否已经向有效本地屏幕层提交表现 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|UI|WorldHealthBar")
	bool bScreenPresentationSubmitted = false;

	/** 当前组件是否可见 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|UI|WorldHealthBar")
	bool bVisible = false;

	/** 最近一次接收的真实生命值 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|UI|WorldHealthBar")
	float LastCurrentHealth = 0.f;

	/** 最近一次接收的真实最大生命值 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|UI|WorldHealthBar")
	float LastMaxHealth = 0.f;

	/** 唯一隐藏计时器是否活动 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|UI|WorldHealthBar")
	bool bHideTimerActive = false;

	/** 非初始生命变化后的可见时长 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|UI|WorldHealthBar")
	float VisibleDuration = 0.f;

	/** 是否因 Owner 为本地玩家而抑制显示 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|UI|WorldHealthBar")
	bool bHiddenForLocalOwner = false;
};

/** 订阅同 Owner 真实生命事实的世界血条表现组件 */
UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaWorldHealthBarComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	/** 创建无业务 Tick、默认隐藏的 Screen Space 血条组件 */
	UWuwaWorldHealthBarComponent();

	/** @return 当前世界血条只读快照 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|UI|WorldHealthBar")
	FWuwaWorldHealthBarRuntimeSnapshot GetRuntimeSnapshot() const;

	/** @return 非初始生命变化后的可见时长 */
	float GetVisibleDuration() const;

	/**
	 * 设置有限正数的可见时长
	 *
	 * @param InVisibleDuration	目标可见时长
	 */
	void SetVisibleDuration(float InVisibleDuration);

protected:
	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

private:
	/** 非初始生命变化后的可见时长 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|UI|WorldHealthBar",
	          meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float VisibleDuration = 2.f;

	/** 当前绑定的同 Owner 生命事实源 */
	TWeakObjectPtr<UWuwaHealthComponent> HealthSource;

	/** 生命变化事实委托 Handle */
	FDelegateHandle HealthChangedDelegateHandle;

	/** 当前 WidgetComponent 创建的血条控件 */
	TWeakObjectPtr<UWuwaWorldHealthBarWidget> HealthBarWidget;

	/** 当前组件唯一的隐藏计时器 */
	FTimerHandle HideTimerHandle;

	/** 最近一次接收的真实生命值 */
	float LastCurrentHealth = 0.f;

	/** 最近一次接收的真实最大生命值 */
	float LastMaxHealth = 0.f;

	/** 是否找到同 Owner 的生命事实源 */
	bool bSourceFound = false;

	/** 是否已经输出缺失事实源警告 */
	bool bMissingSourceWarningEmitted = false;

	/** 是否已经输出表现对象不可用警告 */
	bool bPresentationUnavailableWarningEmitted = false;

	/** 当前可见周期是否已经主动提交屏幕层更新 */
	bool bScreenPresentationUpdateIssued = false;

	/** 是否因 Owner 为本地玩家而抑制显示 */
	bool bHiddenForLocalOwner = false;

	/**
	 * 消费同 Owner 发布的真实生命变化事实
	 *
	 * @param Fact	真实生命变化事实
	 */
	void HandleHealthChanged(const FWuwaHealthChangedFact& Fact);

	/**
	 * 更新血条数值并执行显示门禁
	 *
	 * @param CurrentHealth	当前真实生命值
	 * @param MaxHealth		当前真实最大生命值
	 * @param bInitialSync	是否为初值同步
	 */
	void ApplyHealth(float CurrentHealth, float MaxHealth, bool bInitialSync);

	/** @return Owner 是否为本地玩家控制 Pawn */
	bool IsHiddenForLocalPlayerOwner() const;

	/** @return 当前 World 是否禁止创建 UI 表现 */
	bool IsDedicatedPresentationWorld() const;

	/** 隐藏血条并结束当前计时 */
	void HideHealthBar();

	/**
	 * 同步组件可见性与 Screen Space 屏幕层
	 *
	 * @param bShouldBeVisible	目标可见状态
	 */
	void SetPresentationVisibility(bool bShouldBeVisible);

	/** @return 当前 World 是否具备 Screen Space 屏幕层提交条件 */
	bool IsScreenPresentationContextReady() const;

	/** 清理当前组件唯一的隐藏计时器 */
	void ClearHideTimer();
};
