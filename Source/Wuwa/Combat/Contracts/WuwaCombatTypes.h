// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Combat/Contracts/WuwaCombatWindowHandle.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "WuwaCombatTypes.generated.h"

/** Combat 碰撞通道的集中定义 */
namespace WuwaCombatCollision
{
inline constexpr ECollisionChannel MeleeTraceChannel = ECC_GameTraceChannel1;
}

/** Combat 运行边界的集中定义 */
namespace WuwaCombatLimits
{
inline constexpr int32 MaxMeleeTargets = 4;
}

/** 攻击期间的朝向策略 */
UENUM(BlueprintType)
enum class EWuwaAttackFacingPolicy : uint8
{
	/** 保持激活时的角色朝向，不执行目标吸附 */
	PreserveFacing
};

/** 攻击定义对 Montage Root Motion 的显式策略 */
UENUM(BlueprintType)
enum class EWuwaAttackRootMotionPolicy : uint8
{
	/** 尚未选择策略，禁止进入运行时 */
	Unspecified,

	/** 攻击 Montage 不得包含 Root Motion */
	InPlace,

	/** 攻击 Montage 必须由源序列提供 Root Motion */
	MontageDriven
};

/** 近战攻击命中窗口的 Ability 侧状态 */
UENUM(BlueprintType)
enum class EWuwaMeleeAttackWindowState : uint8
{
	/** 当前没有活动攻击 */
	Idle,

	/** 正在验证并提交攻击 */
	Activating,

	/** Montage 已启动，等待窗口开始事件 */
	WaitingForWindowBegin,

	/** 已收到合法窗口开始事件 */
	WindowOpen,

	/** 已收到合法窗口结束事件 */
	WindowClosed,

	/** 正在执行统一清理 */
	Ending
};

/** 连招输入被拒绝的原因 */
UENUM(BlueprintType)
enum class EWuwaMeleeComboInputRejectReason : uint8
{
	/** 没有拒绝 */
	None,

	/** 输入发生在连招窗口外 */
	OutsideWindow,

	/** 唯一缓冲槽已经占用 */
	BufferFull,

	/** 当前已经是最后一段 */
	LastStep
};

/** 近战攻击 Ability 的最近结束原因 */
UENUM(BlueprintType)
enum class EWuwaMeleeAttackEndReason : uint8
{
	/** 尚未结束或没有历史记录 */
	None,

	/** Montage 正常完成 */
	Completed,

	/** Ability 被主动取消 */
	Cancelled,

	/** Montage 被其他播放请求中断 */
	Interrupted,

	/** Cost 或 Cooldown 提交失败 */
	CommitFailed,

	/** Montage 未能开始播放 */
	MontageFailed,

	/** 命中窗口事件顺序或来源无效 */
	HitWindowEventRejected,

	/** 当前 Montage 收到非法攻击段事件 */
	InvalidStepEvent,

	/** Avatar 已切换或失效 */
	AvatarChanged,

	/** Ability 被从 ASC 移除 */
	AbilityRemoved
};

/** 近战剑刃轨迹的冻结配置 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaMeleeTraceSpec
{
	GENERATED_BODY()

	/** 球形轨迹半径 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Combat|Trace", meta = (ClampMin = "0.01"))
	float TraceRadius = 10.0f;

	/** 每帧沿剑刃插值的采样数量 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Trace",
	          meta = (ClampMin = "2", ClampMax = "16"))
	int32 BladeSampleCount = 5;
};

/** 武器组件对 Debug 和后续命中服务暴露的只读快照 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaWeaponRuntimeSnapshot
{
	GENERATED_BODY()

	/** 武器组件是否完成原子装配 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Weapon")
	bool bInitialized = false;

	/** 当前 WeaponDefinition 名称 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Weapon")
	FString WeaponDefinitionName;

	/** 当前角色挂载骨骼或插槽 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Weapon")
	FName CharacterAttachSocket;

	/** 剑刃根部 Socket 是否有效 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Weapon")
	bool bTraceBaseSocketValid = false;

	/** 剑刃尖端 Socket 是否有效 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Weapon")
	bool bTraceTipSocketValid = false;

	/** 剑刃根部世界坐标 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Weapon")
	FVector TraceBaseWorldPosition = FVector::ZeroVector;

	/** 剑刃尖端世界坐标 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Weapon")
	FVector TraceTipWorldPosition = FVector::ZeroVector;

	/** 当前武器是否为明确占位资产 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Weapon")
	bool bIsPlaceholder = false;
};

/** 近战攻击 Ability 对 Debug 暴露的只读生命周期快照 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaMeleeAttackRuntimeSnapshot
{
	GENERATED_BODY()

	/** Ability 当前是否活动 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	bool bActive = false;

	/** 当前轻攻击语义标签 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	FGameplayTag AttackTag;

	/** 当前或最近一次 AbilitySpec Handle 的安全字符串 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	FString AbilitySpecHandle;

	/** 当前或最近一次 PredictionKey 的安全摘要 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	FString PredictionKeySummary;

	/** 当前 AttackDefinition 名称 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	FString AttackDefinitionName;

	/** 当前 Montage 名称 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	FString MontageName;

	/** 当前窗口状态 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	EWuwaMeleeAttackWindowState WindowState = EWuwaMeleeAttackWindowState::Idle;

	/** 当前攻击段索引，未激活时为 INDEX_NONE */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	int32 CurrentStepIndex = INDEX_NONE;

	/** 当前激活实际进入的攻击段数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	int32 ExecutedStepCount = 0;

	/** 当前 Montage Section */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	FName CurrentSection;

	/** 已提交的下一 Montage Section */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	FName NextSection;

	/** 当前段连招输入窗口是否开放 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	bool bComboWindowOpen = false;

	/** 唯一输入缓冲槽是否占用 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	bool bBufferedNextStep = false;

	/** 下一 Section 是否已提交给 ASC */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	bool bTransitionCommitted = false;

	/** 当前输入缓冲数量，只能为零或一 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	int32 BufferedInputCount = 0;

	/** 当前或最近一次攻击接受的连招输入数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	int32 AcceptedComboInputCount = 0;

	/** 当前或最近一次攻击拒绝的连招输入数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	int32 RejectedComboInputCount = 0;

	/** 最近一次连招输入拒绝原因 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	EWuwaMeleeComboInputRejectReason LastComboInputRejectReason = EWuwaMeleeComboInputRejectReason::None;

	/** 三段命中窗口 Begin 计数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	TArray<int32> PerStepHitWindowBeginCounts;

	/** 三段命中窗口 End 计数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	TArray<int32> PerStepHitWindowEndCounts;

	/** 三段接受的命中事实计数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	TArray<int32> PerStepHitFactCounts;

	/** 三段成功应用伤害的计数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	TArray<int32> PerStepDamageApplicationCounts;

	/** 三段最近创建的窗口句柄 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	TArray<FWuwaCombatWindowHandle> PerStepCombatWindowHandles;

	/** 当前或最近一次攻击收到的合法 Begin 数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	int32 HitWindowBeginCount = 0;

	/** 当前或最近一次攻击收到的合法 End 数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	int32 HitWindowEndCount = 0;

	/** 最近结束原因 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	EWuwaMeleeAttackEndReason LastEndReason = EWuwaMeleeAttackEndReason::None;

	/** 当前定义是否明确标记为占位 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	bool bIsPlaceholder = false;

	/** Authority 当前或最近窗口的 Handle */
	UPROPERTY(VisibleAnywhere, Category = "Wuwa|Combat|Attack")
	uint32 CombatWindowHandle = 0;

	/** 当前或最近攻击接受的权威命中事实数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	int32 ReceivedHitFactCount = 0;

	/** 当前或最近攻击拒绝的旧命中事实数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	int32 RejectedHitFactCount = 0;

	/** 最近一次接受的目标名称 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	FString LastCombatHitTargetName;

	/** Authority 当前或最近窗口实际执行的 Sweep 查询次数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	int32 SweepCount = 0;

	/** Authority 当前或最近成功应用伤害的次数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	int32 DamageApplicationCount = 0;

	/** Authority 当前或最近拒绝应用伤害的次数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Attack")
	int32 RejectedDamageApplicationCount = 0;
};
