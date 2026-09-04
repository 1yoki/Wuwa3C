// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystem/Contracts/WuwaPawnAbilityInitTypes.h"
#include "AbilitySystem/Data/WuwaAbilitySet.h"
#include "Components/ActorComponent.h"
#include "WuwaPawnAbilityInitComponent.generated.h"

class AWuwaCharacter;
class AWuwaPlayerState;
class UWuwaAbilityInputRouterComponent;
class UWuwaAbilitySystemComponent;
class UWuwaAbilitySet;
class UWuwaCombatProfile;
struct FWuwaDeathFact;

/** 建立 PlayerState ASC 与 Character Avatar 绑定的幂等初始化组件 */
UCLASS(ClassGroup = "Wuwa")
class WUWA_API UWuwaPawnAbilityInitComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 创建不参与 Tick 和复制的初始化组件 */
	UWuwaPawnAbilityInitComponent();

	/** @return 是否已建立并验证当前 PlayerState 与 Character 的 ActorInfo */
	bool TryInitializeAbilitySystem();

	/**
	 * 对称释放当前 Pawn 的能力系统绑定
	 *
	 * @param Reason	关闭原因
	 */
	void ShutdownAbilitySystem(EWuwaPawnAbilityShutdownReason Reason);

	/**
	 * 响应当前 Pawn 的类型化死亡事实
	 *
	 * @param Fact	当前 Pawn 的死亡事实
	 * @return 无
	 */
	void HandlePawnDeath(const FWuwaDeathFact& Fact);

	/** @return 当前 Pawn 是否已完成 GAS 基础上下文初始化 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|AbilitySystem")
	bool IsGameplayReady() const;

	/** @return 当前初始化状态 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|AbilitySystem")
	EWuwaPawnAbilityInitState GetInitState() const;

	/** @return 最近一次初始化失败原因 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|AbilitySystem")
	EWuwaPawnAbilityInitFailureReason GetLastFailureReason() const;

	/** @return 当前绑定的 Wuwa 能力系统组件 */
	UWuwaAbilitySystemComponent* GetWuwaAbilitySystemComponent() const;

	/** @return 当前 Pawn 能力系统初始化只读快照 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|AbilitySystem")
	FWuwaPawnAbilityInitRuntimeSnapshot GetRuntimeSnapshot() const;

	/** @return 当前已验证的 CombatProfile */
	const UWuwaCombatProfile* GetCombatProfile() const;

protected:
	//~ Begin UActorComponent Interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

private:
	/** 当前初始化状态 */
	UPROPERTY(Transient)
	EWuwaPawnAbilityInitState InitState = EWuwaPawnAbilityInitState::Uninitialized;

	/** 最近一次初始化失败原因 */
	UPROPERTY(Transient)
	EWuwaPawnAbilityInitFailureReason LastFailureReason = EWuwaPawnAbilityInitFailureReason::None;

	/** 当前绑定的 Character */
	TWeakObjectPtr<AWuwaCharacter> BoundCharacter;

	/** 当前绑定的 PlayerState */
	TWeakObjectPtr<AWuwaPlayerState> BoundPlayerState;

	/** 当前绑定的能力系统组件 */
	TWeakObjectPtr<UWuwaAbilitySystemComponent> BoundAbilitySystemComponent;

	/** 当前绑定的 Ability 输入路由 */
	TWeakObjectPtr<UWuwaAbilityInputRouterComponent> BoundAbilityInputRouter;

	/** 当前 Pawn 战斗系统的唯一装配入口 */
	UPROPERTY(EditDefaultsOnly, Category = "Wuwa|AbilitySystem")
	TSoftObjectPtr<UWuwaCombatProfile> DefaultCombatProfile;

	/** 当前已验证的 CombatProfile */
	UPROPERTY(Transient)
	TObjectPtr<UWuwaCombatProfile> BoundCombatProfile;

	/** Authority 当前 Pawn 保存的 AbilitySet Handle */
	FWuwaAbilitySetGrantedHandles GrantedAbilitySetHandles;

	/** Authority 是否已完成当前 Pawn 的默认 AbilitySet 授予 */
	bool bAuthorityAbilitySetGranted = false;

	/** 成功绑定新上下文的次数 */
	int32 InitializationGeneration = 0;

	/** 实际释放已绑定上下文的次数 */
	int32 ShutdownGeneration = 0;

	/** Authority 成功授予默认 Pawn AbilitySet 的次数 */
	int32 AbilitySetGrantGeneration = 0;

	/**
	 * 记录初始化失败并更新状态
	 *
	 * @param State	失败后的初始化状态
	 * @param Reason	失败原因
	 * @return 固定返回 false
	 */
	bool FailInitialization(EWuwaPawnAbilityInitState State, EWuwaPawnAbilityInitFailureReason Reason);

	/**
	 * 补齐 Router 与 Authority AbilitySet 依赖
	 *
	 * @param Character	当前 Character
	 * @param AbilitySystemComponent	当前 ASC
	 * @return 是否可以进入 GameplayReady
	 */
	bool CompleteGameplayInitialization(AWuwaCharacter* Character, UWuwaAbilitySystemComponent* AbilitySystemComponent);

	/** @return 当前缓存是否与 ASC ActorInfo 完全一致 */
	bool DoesActorInfoMatch() const;
};
