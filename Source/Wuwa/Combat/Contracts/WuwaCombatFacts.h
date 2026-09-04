// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Combat/Contracts/WuwaCombatWindowHandle.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "WuwaCombatFacts.generated.h"

class AActor;

/** 命中窗口的结束原因 */
UENUM(BlueprintType)
enum class EWuwaCombatWindowEndReason : uint8
{
	/** 尚未结束 */
	None,

	/** 动画通知正常关闭窗口 */
	NotifyEnded,

	/** Ability 统一清理关闭窗口 */
	AbilityCleanup,

	/** Pawn 战斗初始化正在关闭 */
	InitializationShutdown,

	/** ASC Avatar 已替换 */
	AvatarChanged,

	/** Actor 正在结束 Play */
	EndPlay,

	/** Pawn 已进入死亡流程 */
	Death,

	/** 运行时验证失败而中止 */
	ExplicitAbort
};

/** 命中窗口最近一次失败原因 */
UENUM(BlueprintType)
enum class EWuwaCombatWindowFailureReason : uint8
{
	/** 没有失败 */
	None,

	/** CombatExecution 尚未初始化 */
	NotInitialized,

	/** 非 Authority 尝试创建窗口 */
	NotAuthority,

	/** 当前已有活动窗口 */
	WindowAlreadyActive,

	/** 冻结请求字段无效 */
	InvalidRequest,

	/** 武器组件或冻结 Socket 无效 */
	InvalidWeapon,

	/** 角色骨骼网格无效 */
	InvalidCharacterMesh,

	/** 初始剑刃世界端点无效 */
	InvalidInitialBladePose,

	/** 当前世界不可用 */
	WorldUnavailable,

	/** 结束请求携带了旧窗口句柄 */
	StaleWindowHandle
};

/** Combat Target Contract 的拒绝原因 */
UENUM(BlueprintType)
enum class EWuwaCombatTargetRejectionReason : uint8
{
	/** 目标接受当前命中查询 */
	None,

	/** 查询缺少有效来源、窗口或攻击语义 */
	InvalidQuery,

	/** 目标当前关闭了中性可受击资格 */
	CombatTargetDisabled
};

/** 近战命中窗口在开始边界冻结的请求 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaMeleeWindowRequest
{
	GENERATED_BODY()

	/** 发起攻击的 Actor */
	TWeakObjectPtr<AActor> SourceActor;

	/** 攻击行为的 Instigator */
	TWeakObjectPtr<AActor> InstigatorActor;

	/** 当前攻击的中性语义标签 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	FGameplayTag AttackTag;

	/** 当前攻击段索引 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	uint8 StepIndex = 0;

	/** 球形轨迹半径 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	float TraceRadius = 10.0f;

	/** 沿剑刃插值的采样数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	int32 BladeSampleCount = 5;

	/** 当前窗口允许发布的不同目标上限 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	int32 MaxTargets = 4;

	/** 冻结的剑刃根部 Socket */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	FName TraceBaseSocket;

	/** 冻结的剑刃尖端 Socket */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	FName TraceTipSocket;

	/** 是否允许命中多个不同目标 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	bool bAllowMultipleTargets = true;
};

/** CombatExecution 向目标资格契约提交的中性查询 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCombatTargetQuery
{
	GENERATED_BODY()

	/** 当前命中窗口 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Target")
	FWuwaCombatWindowHandle WindowHandle;

	/** 发起攻击的 Actor */
	TWeakObjectPtr<AActor> SourceActor;

	/** 攻击行为的 Instigator */
	TWeakObjectPtr<AActor> InstigatorActor;

	/** 当前攻击的中性语义标签 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Target")
	FGameplayTag AttackTag;
};

/** Combat Target Contract 返回的资格响应 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCombatTargetResponse
{
	GENERATED_BODY()

	/** 是否接受当前 Combat Hit */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Target")
	bool bCanReceiveCombatHit = false;

	/** 拒绝时的中性原因 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Target")
	EWuwaCombatTargetRejectionReason RejectionReason = EWuwaCombatTargetRejectionReason::InvalidQuery;
};

/** 服务端权威命中事实 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCombatHitFact
{
	GENERATED_BODY()

	/** 发布事实的命中窗口 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	FWuwaCombatWindowHandle WindowHandle;

	/** 发布事实的攻击段索引 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	uint8 StepIndex = 0;

	/** 发起攻击的 Actor */
	TWeakObjectPtr<AActor> SourceActor;

	/** 被命中的目标 Actor */
	TWeakObjectPtr<AActor> TargetActor;

	/** 权威碰撞命中点 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	FVector ImpactPoint = FVector::ZeroVector;

	/** 权威碰撞法线 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	FVector ImpactNormal = FVector::UpVector;

	/** 命中的骨骼名称 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	FName BoneName;
};

/** Pawn 级死亡状态 */
UENUM(BlueprintType)
enum class EWuwaDeathState : uint8
{
	/** 尚未死亡 */
	NotDead,

	/** 正在启动死亡流程 */
	DeathStarted,

	/** 已完成死亡事实发布 */
	Dead,

	/** 正在清理旧 Avatar 并等待重生 */
	Respawning
};

/** HealthComponent 发布的真实生命变化事实 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaHealthChangedFact
{
	GENERATED_BODY()

	/** 生命变化所属 Actor */
	TWeakObjectPtr<AActor> AffectedActor;

	/** 变化前生命值 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Health")
	float PreviousHealth = 0.f;

	/** 当前生命值 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Health")
	float CurrentHealth = 0.f;

	/** 当前最大生命值 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Health")
	float MaxHealth = 0.f;

	/** 是否为 ASC 绑定完成后的初值同步 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Health")
	bool bInitialSync = false;
};

/** HealthComponent 发布的 GAS 中性死亡事实 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaDeathFact
{
	GENERATED_BODY()

	/** 死亡事实所属 Actor */
	TWeakObjectPtr<AActor> DeadActor;

	/** 当前 Pawn 内单调增长的死亡事实序号 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Health")
	int32 DeathSequence = 0;

	/** 触发当前属性回调前的生命值 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Health")
	float PreviousHealth = 0.f;

	/** 触发死亡时的生命值 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Health")
	float CurrentHealth = 0.f;

	/** 事实是否由 Authority 发布 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Health")
	bool bAuthority = false;
};

/** PoiseComponent 发布的权威破韧事实 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaPoiseBreakFact
{
	GENERATED_BODY()

	/** 破韧事实所属 Actor */
	TWeakObjectPtr<AActor> TargetActor;

	/** 当前 Pawn 内单调增长的破韧事实序号 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Poise")
	int32 BreakSequence = 0;

	/** 触发当前属性回调前的韧性值 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Poise")
	float PreviousPoise = 0.f;

	/** 触发破韧时的韧性值 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Poise")
	float CurrentPoise = 0.f;

	/** 事实是否由 Authority 发布 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Poise")
	bool bAuthority = false;
};

/** PoiseComponent 对 Debug 和测试暴露的只读快照 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaPoiseRuntimeSnapshot
{
	GENERATED_BODY()

	/** 是否已绑定当前 ASC */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Poise")
	bool bInitialized = false;

	/** Owner 是否为 Authority */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Poise")
	bool bAuthority = false;

	/** 当前韧性值 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Poise")
	float Poise = 0.f;

	/** 当前最大韧性值 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Poise")
	float MaxPoise = 0.f;

	/** 当前待结算韧性伤害 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Poise")
	float IncomingPoiseDamage = 0.f;

	/** 最近一次 Poise 属性变化前的韧性值 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Poise")
	float PreviousPoise = 0.f;

	/** 最近一次 Poise 下降量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Poise")
	float LastPoiseDamage = 0.f;

	/** Authority 是否正在等待下一帧复核破韧 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Poise")
	bool bPendingBreak = false;

	/** 最近发布的破韧事实序号 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Poise")
	int32 BreakSequence = 0;
};

/** HealthComponent 对 Debug 和测试暴露的只读快照 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaHealthRuntimeSnapshot
{
	GENERATED_BODY()

	/** 是否已绑定当前 ASC */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Health")
	bool bInitialized = false;

	/** Owner 是否为 Authority */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Health")
	bool bAuthority = false;

	/** 当前 Pawn 死亡状态 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Health")
	EWuwaDeathState DeathState = EWuwaDeathState::NotDead;

	/** 当前生命值 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Health")
	float Health = 0.f;

	/** 当前最大生命值 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Health")
	float MaxHealth = 0.f;

	/** 最近一次 Health 属性变化前的生命值 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Health")
	float PreviousHealth = 0.f;

	/** 最近一次 Health 下降量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Health")
	float LastDamage = 0.f;

	/** 当前死亡标签数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Health")
	int32 DeadTagCount = 0;

	/** 最近发布的死亡事实序号 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Health")
	int32 DeathSequence = 0;

	/** Authority 是否持有可追踪死亡效果 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Health")
	bool bDeadEffectActive = false;
};

/** CombatExecution 对 Debug 和测试暴露的只读快照 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCombatExecutionRuntimeSnapshot
{
	GENERATED_BODY()

	/** 是否完成依赖装配 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	bool bInitialized = false;

	/** Owner 当前是否为 Authority */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	bool bAuthority = false;

	/** 当前是否存在活动窗口 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	bool bWindowActive = false;

	/** 当前活动窗口句柄 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	FWuwaCombatWindowHandle ActiveWindowHandle;

	/** 最近结束原因 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	EWuwaCombatWindowEndReason LastEndReason = EWuwaCombatWindowEndReason::None;

	/** 最近失败原因 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	EWuwaCombatWindowFailureReason LastFailureReason = EWuwaCombatWindowFailureReason::None;

	/** 最近冻结的攻击语义 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	FGameplayTag AttackTag;

	/** 最近冻结的攻击段索引 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	uint8 StepIndex = 0;

	/** 最近冻结的轨迹半径 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	float TraceRadius = 0.0f;

	/** 最近冻结的剑刃采样数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	int32 BladeSampleCount = 0;

	/** 最近冻结的不同目标上限 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	int32 MaxTargets = 0;

	/** 最近窗口实际执行的 Sweep 查询数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	int32 SweepCount = 0;

	/** 最近窗口发布的不同目标数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	int32 HitCount = 0;

	/** 上一次剑刃根部位置 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	FVector PreviousBase = FVector::ZeroVector;

	/** 上一次剑刃尖端位置 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	FVector PreviousTip = FVector::ZeroVector;

	/** 当前剑刃根部位置 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	FVector CurrentBase = FVector::ZeroVector;

	/** 当前剑刃尖端位置 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	FVector CurrentTip = FVector::ZeroVector;

	/** 最近发布的权威命中事实 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	FWuwaCombatHitFact LastHitFact;

	/** 是否已建立上一帧剑刃姿态 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	bool bHasPreviousBladePose = false;

	/** 是否临时提升了服务器骨骼 Tick 选项 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	bool bMeshTickOptionElevated = false;

	/** 提升前的 Mesh Tick 选项数值 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	uint8 OriginalMeshTickOption = 0;

	/** 当前 Mesh Tick 选项数值 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Execution")
	uint8 CurrentMeshTickOption = 0;
};

/** CombatExecution 发布权威命中事实的中性委托 */
DECLARE_MULTICAST_DELEGATE_OneParam(FWuwaCombatHitFactDelegate, const FWuwaCombatHitFact&);

/** HealthComponent 发布真实生命变化事实的中性委托 */
DECLARE_MULTICAST_DELEGATE_OneParam(FWuwaHealthChangedFactDelegate, const FWuwaHealthChangedFact&);

/** HealthComponent 发布类型化死亡事实的中性委托 */
DECLARE_MULTICAST_DELEGATE_OneParam(FWuwaDeathFactDelegate, const FWuwaDeathFact&);

/** PoiseComponent 发布破韧事实的原生委托 */
DECLARE_MULTICAST_DELEGATE_OneParam(FWuwaPoiseBreakFactDelegate, const FWuwaPoiseBreakFact&);
