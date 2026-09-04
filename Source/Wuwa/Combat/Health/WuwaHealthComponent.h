// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Combat/Contracts/WuwaCombatFacts.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "WuwaHealthComponent.generated.h"

class UGameplayEffect;
class UWuwaAbilitySystemComponent;

/** 绑定 ASC 生命属性并发布 Pawn 级死亡事实 */
UCLASS(ClassGroup = "Wuwa")
class WUWA_API UWuwaHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 创建不参与 Tick 和复制的生命组件 */
	UWuwaHealthComponent();

	/**
	 * 绑定当前 ASC 与可追踪死亡效果
	 *
	 * @param AbilitySystemComponent	当前 PlayerState 持有的 Wuwa ASC
	 * @param DeadEffectClass		授予 State.Combat.Dead 的无限效果
	 * @return 是否完成幂等绑定
	 */
	bool Initialize(UWuwaAbilitySystemComponent* AbilitySystemComponent, TSubclassOf<UGameplayEffect> DeadEffectClass);

	/** @return 无；对称解绑属性与标签委托 */
	void Shutdown();

	/** @return 是否成功进入 Respawning 状态 */
	bool BeginRespawn();

	/** @return 是否已绑定有效 ASC */
	bool IsInitialized() const;

	/** @return 当前是否已经启动死亡 */
	bool IsDeadOrDying() const;

	/** @return 当前 Pawn 死亡状态 */
	EWuwaDeathState GetDeathState() const;

	/** @return 当前 HealthComponent 只读快照 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Health")
	FWuwaHealthRuntimeSnapshot GetRuntimeSnapshot() const;

	/** @return 真实生命变化事实委托 */
	FWuwaHealthChangedFactDelegate& OnHealthChanged();

	/** @return 类型化死亡事实委托 */
	FWuwaDeathFactDelegate& OnDeathFact();

protected:
	//~ Begin UActorComponent Interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

private:
	/** 当前绑定的 ASC */
	TWeakObjectPtr<UWuwaAbilitySystemComponent> AbilitySystemComponent;

	/** 当前验证通过的死亡效果类 */
	TSubclassOf<UGameplayEffect> DeadEffectClass;

	/** Authority 当前死亡效果 Handle */
	FActiveGameplayEffectHandle DeadEffectHandle;

	/** Health 属性回调 Handle */
	FDelegateHandle HealthChangedDelegateHandle;

	/** Dead Tag 回调 Handle */
	FDelegateHandle DeadTagChangedDelegateHandle;

	/** 当前 Pawn 死亡状态 */
	EWuwaDeathState DeathState = EWuwaDeathState::NotDead;

	/** 当前 Pawn 内死亡事实序号 */
	int32 DeathSequence = 0;

	/** 最近一次属性回调前的生命值 */
	float LastPreviousHealth = 0.f;

	/** 最近一次记录的生命值 */
	float LastCurrentHealth = 0.f;

	/** 类型化死亡事实委托 */
	FWuwaDeathFactDelegate DeathFactDelegate;

	/** 真实生命变化事实委托 */
	FWuwaHealthChangedFactDelegate HealthChangedFactDelegate;

	/**
	 * 接收 Health 属性变化
	 *
	 * @param ChangeData	GAS 属性变化数据
	 */
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);

	/**
	 * 接收 Dead Tag 数量变化
	 *
	 * @param Tag		死亡状态标签
	 * @param NewCount	变化后的标签数量
	 */
	void HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** 收敛非 Authority 当前 Avatar 的复制存活状态 */
	void ReconcileReplicatedAliveState();

	/** @return Authority 是否成功启动一次死亡 */
	bool TryStartAuthorityDeath();

	/** @return Authority 是否成功应用并保存死亡效果 */
	bool ApplyDeadEffect();

	/** 发布一次当前 Pawn 的类型化死亡事实 */
	void PublishDeathFact();

	/**
	 * 发布当前 ASC 的真实生命变化事实
	 *
	 * @param bInitialSync	是否为绑定完成后的初值同步
	 */
	void PublishHealthChangedFact(bool bInitialSync);

	/** 清理绑定字段但保留最近死亡快照 */
	void ResetBindingState();
};
