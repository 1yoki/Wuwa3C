// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WuwaPawnAbilityInitTypes.generated.h"

/** Pawn 能力系统初始化状态 */
UENUM(BlueprintType)
enum class EWuwaPawnAbilityInitState : uint8
{
	/** 尚未建立有效能力系统上下文 */
	Uninitialized,

	/** Character 正在等待 PlayerState 或 ASC 到达 */
	WaitingForPlayerState,

	/** ASC ActorInfo 已建立并通过一致性检查 */
	ActorInfoReady,

	/** 当前 Pawn 的 GAS 基础上下文可供后续系统使用 */
	GameplayReady,

	/** 当前 Pawn 正在释放能力系统上下文 */
	ShuttingDown
};

/** Pawn 能力系统初始化失败原因 */
UENUM(BlueprintType)
enum class EWuwaPawnAbilityInitFailureReason : uint8
{
	/** 没有初始化失败 */
	None,

	/** 组件 Owner 不是有效 Wuwa Character */
	InvalidOwnerCharacter,

	/** Character 尚未获得 PlayerState */
	MissingPlayerState,

	/** Character 的 PlayerState 不是 Wuwa PlayerState */
	UnsupportedPlayerState,

	/** Wuwa PlayerState 缺少能力系统组件 */
	MissingAbilitySystemComponent,

	/** Character 缺少 Ability 输入路由组件 */
	MissingAbilityInputRouter,

	/** Character 缺少 Action/Ability Interop 组件 */
	MissingActionAbilityInteropComponent,

	/** Action/Ability Interop 无法绑定当前 ASC 与 StateTag */
	ActionAbilityInteropInitializationFailed,

	/** Ability 输入路由拒绝当前 ASC */
	AbilityInputRouterInitializationFailed,

	/** Authority 缺少默认 Pawn AbilitySet */
	MissingDefaultAbilitySet,

	/** 缺少默认 CombatProfile */
	MissingCombatProfile,

	/** 默认 CombatProfile 的必需定义无效 */
	InvalidCombatProfile,

	/** Character 缺少 WeaponComponent */
	MissingWeaponComponent,

	/** WeaponComponent 无法装配默认武器 */
	WeaponInitializationFailed,

	/** Character 缺少 CombatExecutionComponent */
	MissingCombatExecutionComponent,

	/** CombatExecutionComponent 无法装配当前 Mesh 和 Weapon */
	CombatExecutionInitializationFailed,

	/** Character 缺少 HealthComponent */
	MissingHealthComponent,

	/** HealthComponent 无法绑定当前 ASC 和 DeadEffect */
	HealthComponentInitializationFailed,

	/** Character 缺少 PoiseComponent */
	MissingPoiseComponent,

	/** PoiseComponent 无法绑定当前 ASC */
	PoiseComponentInitializationFailed,

	/** Authority 无法完整授予默认 Pawn AbilitySet */
	AbilitySetGrantFailed,

	/** PlayerState 当前 Pawn 不是发起初始化的 Character */
	PlayerStatePawnMismatch,

	/** InitAbilityActorInfo 后 Owner 或 Avatar 不一致 */
	ActorInfoVerificationFailed,

	/** 组件正在关闭，拒绝新的初始化 */
	ShuttingDown
};

/** Pawn 能力系统关闭原因 */
UENUM(BlueprintType)
enum class EWuwaPawnAbilityShutdownReason : uint8
{
	/** Pawn 被 Controller 解除占有 */
	Unpossessed,

	/** Pawn 正在结束 Play */
	EndPlay,

	/** PlayerState ASC 已切换到其他 Avatar */
	AvatarReplaced,

	/** 调用方明确请求重置当前上下文 */
	ExplicitReset,

	/** GameMode 正在清理旧 Avatar 以执行重生 */
	Respawn
};

/** Pawn 能力系统只读运行快照 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaPawnAbilityInitRuntimeSnapshot
{
	GENERATED_BODY()

	/** 当前初始化状态 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	EWuwaPawnAbilityInitState InitState = EWuwaPawnAbilityInitState::Uninitialized;

	/** 最近一次初始化失败原因 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	EWuwaPawnAbilityInitFailureReason LastFailureReason = EWuwaPawnAbilityInitFailureReason::None;

	/** 当前 Character 名称 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	FString CharacterName;

	/** 当前 PlayerState 名称 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	FString PlayerStateName;

	/** 当前能力系统组件名称 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	FString AbilitySystemComponentName;

	/** ASC OwnerActor 名称 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	FString OwnerActorName;

	/** ASC AvatarActor 名称 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	FString AvatarActorName;

	/** ASC ActorInfo 是否与当前缓存完全一致 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	bool bActorInfoMatches = false;

	/** Ability 输入路由是否已绑定当前 ASC */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	bool bAbilityInputReady = false;

	/** Action/Ability Interop 是否已绑定当前 ASC 与 StateTag */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	bool bActionAbilityInteropReady = false;

	/** Authority 是否已授予默认 Pawn AbilitySet */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	bool bAuthorityAbilitySetGranted = false;

	/** 默认 CombatProfile 是否已加载并通过验证 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	bool bCombatProfileReady = false;

	/** 默认 WeaponDefinition 是否已成功装配 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	bool bWeaponReady = false;

	/** 默认 AttackDefinition 是否可供攻击 Ability 使用 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	bool bAttackDefinitionReady = false;

	/** CombatExecution 是否已完成当前 Pawn 装配 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	bool bCombatExecutionReady = false;

	/** HealthComponent 是否已绑定当前 ASC */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	bool bHealthComponentReady = false;

	/** PoiseComponent 是否已绑定当前 ASC */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	bool bPoiseComponentReady = false;

	/** 当前 CombatProfile 名称 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	FString CombatProfileName;

	/** 当前保存的 AbilitySpec Handle 数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	int32 GrantedAbilityCount = 0;

	/** 当前保存的 GameplayEffect Handle 数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	int32 GrantedEffectCount = 0;

	/** 当前保存的持续 GameplayEffect Handle 数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	int32 GrantedActiveEffectCount = 0;

	/** 当前成功执行的瞬时 GameplayEffect 数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	int32 AppliedInstantEffectCount = 0;

	/** 成功绑定新上下文的次数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	int32 InitializationGeneration = 0;

	/** 实际释放已绑定上下文的次数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	int32 ShutdownGeneration = 0;

	/** Authority 成功授予默认 Pawn AbilitySet 的次数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	int32 AbilitySetGrantGeneration = 0;
};
