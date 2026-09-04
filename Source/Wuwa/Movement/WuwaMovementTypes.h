#pragma once

#include "CoreMinimal.h"
#include "WuwaMovementTypes.generated.h"

// 描述本次跳跃来源
UENUM(BlueprintType)
enum class EWuwaJumpType : uint8
{
	None UMETA(DisplayName = "None"),
	Ground UMETA(DisplayName = "地面跳跃"),
	Coyote UMETA(DisplayName = "土狼跳跃"),
	AirSprint UMETA(DisplayName = "空中定向二段跳"),
	AirBackflip UMETA(DisplayName = "空中后空翻")
};

/** 普通跳跃输入边沿类型 */
UENUM(BlueprintType)
enum class EWuwaJumpRequestKind : uint8
{
	/** 没有跳跃输入边沿 */
	None,

	/** 按下普通跳跃 */
	Pressed,

	/** 释放普通跳跃 */
	Released
};

// 保存本次普通落地的物理冲击强度分类。
UENUM(BlueprintType)
enum class EWuwaLandingType : uint8
{
	None UMETA(DisplayName = "尚未落地"),
	Light UMETA(DisplayName = "轻落地"),
	Heavy UMETA(DisplayName = "重落地")
};

namespace WuwaMovementRules
{
FORCEINLINE EWuwaLandingType ClassifyLanding(const float ImpactSpeed, const float HeavyLandingVelocityThreshold)
{
	return ImpactSpeed >= HeavyLandingVelocityThreshold ? EWuwaLandingType::Heavy : EWuwaLandingType::Light;
}
}

// 描述本次落地由什么行为触发。
UENUM(BlueprintType)
enum class EWuwaLandingSource : uint8
{
	Normal UMETA(DisplayName = "普通空中"),
	PlungeAttack UMETA(DisplayName = "下落攻击")
};

/** 可由客户端预测移动重演的地面速度档位 */
UENUM(BlueprintType)
enum class EWuwaLocomotionSpeedMode : uint8
{
	/** 低于模拟输入阈值的行走档位 */
	Walk,

	/** 达到模拟输入阈值的常规跑步档位 */
	Run,

	/** Dash 出口向 RunSpeed 收敛的冲刺档位 */
	SprintRun
};

/** Simulated Proxy 消费的低频权威移动表现事实 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaReplicatedLocomotionPresentationState
{
	GENERATED_BODY()

	/** 当前权威速度档位 */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Network")
	EWuwaLocomotionSpeedMode SpeedMode = EWuwaLocomotionSpeedMode::Walk;

	/** 当前滞空期间已提交的跳跃次数 */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Network")
	int32 JumpCount = 0;

	/** 最近一次成功跳跃类型 */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Network")
	EWuwaJumpType LastJumpType = EWuwaJumpType::None;

	/** 最近一次成功跳跃序号 */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Network")
	int32 JumpSequence = 0;

	/** 最近一次真实落地类型 */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Network")
	EWuwaLandingType LastLandingType = EWuwaLandingType::None;

	/** 最近一次落地的向下冲击速度 */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Network")
	float LastLandingVelocity = 0.f;

	/** 最近一次落地前的最大下落距离 */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Network")
	float LastFallDistance = 0.f;

	/** 最近一次真实落地序号 */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Network")
	int32 LandingSequence = 0;
};

USTRUCT(BlueprintType)
struct WUWA_API FWuwaAirActionState
{
	GENERATED_BODY()

	// 记录当前滞空期间已经执行的跳跃次数。
	UPROPERTY(BlueprintReadOnly, Category = "Air Action")
	int32 JumpCount = 0;

	// 保证空中 Sprint 每次滞空最多生效一次。
	UPROPERTY(BlueprintReadOnly, Category = "Air Action")
	bool bAirSprintConsumed = false;

	// 保存最后一次接地时间，用于判断土狼时间。
	double LastGroundedTime = -1.0;

	/** 最近成功普通跳跃所属的请求 Generation */
	UPROPERTY(BlueprintReadOnly, Category = "Air Action")
	int32 AirCycleGeneration = 0;

	/** 最近收到的普通跳跃输入 Generation */
	UPROPERTY(BlueprintReadOnly, Category = "Air Action")
	int32 JumpRequestGeneration = 0;

	/** 当前跳跃缓存所属的请求 Generation */
	UPROPERTY(BlueprintReadOnly, Category = "Air Action")
	int32 BufferedJumpGeneration = 0;

	/** 当前跳跃缓存的剩余秒数 */
	UPROPERTY(BlueprintReadOnly, Category = "Air Action")
	float BufferedJumpRemainingTime = 0.f;

	/** 最近收到的普通跳跃输入边沿 */
	UPROPERTY(BlueprintReadOnly, Category = "Air Action")
	EWuwaJumpRequestKind JumpRequestKind = EWuwaJumpRequestKind::None;

	// 保存本次下落开始高度，用于计算下落距离。
	float FallStartHeight = 0.f;

	// 只重置空中次数，不清除尚未消费的跳跃缓存。
	void ResetBudgetsOnLanding()
	{
		JumpCount = 0;
		bAirSprintConsumed = false;
	}
};

USTRUCT(BlueprintType)
struct WUWA_API FWuwaLandingEvent
{
	GENERATED_BODY()

	// 保存角色接触地面前的完整速度
	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	FVector ImpactVelocity = FVector::ZeroVector;

	// 保存向下冲击速度的绝对值，用于反馈强度和调试。
	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	float ImpactSpeed = 0.f;

	// 保存本次滞空期间的最大下落距离。
	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	float FallDistance = 0.f;

	// 保存触发本次落地的 Gameplay 行为。
	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	EWuwaLandingSource LandingSource = EWuwaLandingSource::Normal;

	// 保存本次落地需要播放的表现类型。
	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	EWuwaLandingType LandingType = EWuwaLandingType::None;

	// 每次真实落地递增，用于识别新事件。
	UPROPERTY(BlueprintReadOnly, Category = "Landing")
	int32 Sequence = 0;
};

USTRUCT(BlueprintType)
struct WUWA_API FWuwaLocomotionSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	FVector Acceleration = FVector::ZeroVector;

	// 角色在平面上的速度大小
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float HorizontalSpeed = 0.f;

	// 角色在平面上的移动方向，单位为角度，范围 [-180, 180]。
	// 正值表示向右
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float Direction = 0.f;

	// 角色在平面上的输入量大小，范围 [0, 1]。
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float InputMagnitude = 0.f;

	/** 当前由可重演加速度解析出的速度档位 */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Network")
	EWuwaLocomotionSpeedMode SpeedMode = EWuwaLocomotionSpeedMode::Walk;

	/** 当前可重演加速度对应的归一化输入量 */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Network")
	float ReplayInputMagnitude = 0.f;

	/** 当前角色是否拥有可供表现读取的本地输入 */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Network")
	bool bHasLocalInput = false;

	/** 当前 UE 移动模式 */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Network")
	TEnumAsByte<EMovementMode> MovementMode = MOVE_None;

	/** 当前 UE 自定义移动模式 */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Network")
	uint8 CustomMovementMode = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsMovingOnGround = false;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsFalling = false;

	/** 物理下落或空中自定义移动期间均为真 */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Air")
	bool bIsAirborne = false;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsSprinting = false;

	// 提供跳跃和下落动画需要的垂直速度。
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Air")
	float VerticalVelocity = 0.f;

	// 提供当前滞空期间的跳跃次数。
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Air")
	int32 JumpCount = 0;

	// 提供最近一次成功跳跃的类型。
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Air")
	EWuwaJumpType LastJumpType = EWuwaJumpType::None;

	// 每次成功起跳递增，用于识别空中的再次起跳。
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Air")
	int32 JumpSequence = 0;

	// 提供最近一次真实落地的类型。
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Landing")
	EWuwaLandingType LastLandingType = EWuwaLandingType::None;

	// 保存最近一次落地的向下冲击速度。
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Landing")
	float LastLandingVelocity = 0.f;

	// 保存最近一次落地前的最大下落距离。
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Landing")
	float LastFallDistance = 0.f;

	// 每次真实落地递增，用于避免重复播放落地表现。
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Landing")
	int32 LandingSequence = 0;
};

/** 网络移动基线只读快照 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaNetworkMovementBaselineSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Network Movement")
	TEnumAsByte<ENetRole> LocalRole = ROLE_None;

	UPROPERTY(BlueprintReadOnly, Category = "Network Movement")
	TEnumAsByte<ENetRole> RemoteRole = ROLE_None;

	UPROPERTY(BlueprintReadOnly, Category = "Network Movement")
	uint8 NetMode = static_cast<uint8>(NM_Standalone);

	UPROPERTY(BlueprintReadOnly, Category = "Network Movement")
	bool bLocallyControlled = false;

	UPROPERTY(BlueprintReadOnly, Category = "Network Movement")
	TEnumAsByte<EMovementMode> MovementMode = MOVE_None;

	UPROPERTY(BlueprintReadOnly, Category = "Network Movement")
	uint8 CustomMovementMode = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Network Movement")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Network Movement")
	FVector Acceleration = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Network Movement")
	float MaxWalkSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Network Movement")
	float ResolvedMaxSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Network Movement")
	float LocalInputMagnitude = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Network Movement")
	int32 CorrectionCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Network Movement")
	float LastCorrectionDistance = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Network Movement")
	float MaxCorrectionDistance = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Network Movement")
	float AverageCorrectionDistance = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Network Movement")
	float CorrectionsPerSecond = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Network Movement")
	float LastAckedMoveTimestamp = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Network Movement")
	int32 SavedMoveCount = 0;
};
