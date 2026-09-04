#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/NetSerialization.h"
#include "Movement/Network/WuwaCharacterNetworkMoveTypes.h"
#include "WuwaTraversalTypes.generated.h"

class UCurveFloat;

/** 无锚点钩锁查询失败原因 */
UENUM(BlueprintType)
enum class EWuwaGrappleQueryFailureReason : uint8
{
	/** 查询成功 */
	None,

	/** 相机水平前向无效 */
	InvalidView,

	/** Definition 或配置无效 */
	InvalidDefinition,

	/** 安全缩短后的前向距离不足 */
	TooShort,

	/** 轨迹无法缩短到安全尺度 */
	TrajectoryBlocked,

	/** 释放点没有完整 Capsule 空间 */
	NoReleaseClearance,

	/** Prepare 时 Query 起点漂移过大 */
	StartDriftTooLarge,

	/** 查询世界或来源对象无效 */
	InvalidWorld
};

/** Grapple Gameplay 的只读运行阶段 */
UENUM(BlueprintType)
enum class EWuwaGrappleRuntimePhase : uint8
{
	/** 当前没有 Grapple 实例 */
	None,

	/** 零或极短的起手状态标记 */
	Windup,

	/** 正在沿共享基础轨迹牵引 */
	Pulling,

	/** Movement 已结束且 Action 正在收口 */
	Releasing
};

/** Grapple 横向输入的可重演来源 */
UENUM(BlueprintType)
enum class EWuwaGrappleLateralInputSource : uint8
{
	/** 当前没有 Grapple 横向输入 */
	None,

	/** 使用 CMC Acceleration 在权威横向轴上的投影 */
	AccelerationProjection
};

/** 无锚点钩锁只读世界查询配置 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaFreeGrappleQuerySpec
{
	GENERATED_BODY()

	/** 视觉锚点前向距离 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Query", meta = (ClampMin = "0.0", Units = "cm"))
	float VisualAnchorForwardDistance = 1200.f;

	/** 视觉锚点相对高度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Query", meta = (Units = "cm"))
	float VisualAnchorHeight = 520.f;

	/** 允许生成意图的最低前向位移 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Query", meta = (ClampMin = "0.0", Units = "cm"))
	float MinimumForwardTravelDistance = 300.f;

	/** 预测轨迹分段扫掠半径 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Query", meta = (ClampMin = "0.0", Units = "cm"))
	float TrajectorySweepRadius = 42.f;

	/** 预测轨迹采样数 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Query", meta = (ClampMin = "2", ClampMax = "64"))
	int32 TrajectorySampleCount = 12;

	/** 释放点空间检查半径 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Query", meta = (ClampMin = "0.0", Units = "cm"))
	float ReleaseClearanceRadius = 42.f;

	/** 释放点空间检查半高 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Query", meta = (ClampMin = "0.0", Units = "cm"))
	float ReleaseClearanceHalfHeight = 96.f;

	/** 轨迹与释放空间的额外安全边距 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Query", meta = (ClampMin = "0.0", Units = "cm"))
	float TrajectorySafetyMargin = 4.f;

	/** 轨迹阻挡缩短后的最低统一尺度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Query", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumTrajectoryScale = 0.35f;

	/** Prepare 允许的 Query 起点漂移 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Query", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumStartDriftBeforeReject = 80.f;

	/** 轨迹和释放空间使用的碰撞通道 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Query")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** @return 配置是否满足运行时数值约束 */
	bool IsRuntimeValid() const
	{
		const ECollisionChannel Channel = TraceChannel.GetValue();
		return FMath::IsFinite(VisualAnchorForwardDistance) && VisualAnchorForwardDistance > 0.f &&
		       FMath::IsFinite(VisualAnchorHeight) && FMath::IsFinite(MinimumForwardTravelDistance) &&
		       MinimumForwardTravelDistance > 0.f && FMath::IsFinite(TrajectorySweepRadius) &&
		       TrajectorySweepRadius > 0.f && TrajectorySampleCount >= 2 && TrajectorySampleCount <= 64 &&
		       FMath::IsFinite(ReleaseClearanceRadius) && ReleaseClearanceRadius > 0.f &&
		       FMath::IsFinite(ReleaseClearanceHalfHeight) && ReleaseClearanceHalfHeight >= ReleaseClearanceRadius &&
		       FMath::IsFinite(TrajectorySafetyMargin) && TrajectorySafetyMargin >= 0.f &&
		       FMath::IsFinite(MinimumTrajectoryScale) && MinimumTrajectoryScale > 0.f &&
		       MinimumTrajectoryScale <= 1.f && FMath::IsFinite(MaximumStartDriftBeforeReject) &&
		       MaximumStartDriftBeforeReject >= 0.f && Channel >= ECC_WorldStatic && Channel < ECC_MAX;
	}
};

/** 一次无锚点钩锁世界查询的可诊断结果 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaGrappleQueryResult
{
	GENERATED_BODY()

	/** Query 是否通过全部轨迹与释放空间检查 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Query")
	bool bSucceeded = false;

	/** 失败时的唯一领域原因 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Query")
	EWuwaGrappleQueryFailureReason FailureReason = EWuwaGrappleQueryFailureReason::InvalidDefinition;

	/** 输入边沿读取的角色位置 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Query")
	FVector QueryStartLocation = FVector::ZeroVector;

	/** 数据配置直接生成的视觉锚点 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Query")
	FVector RequestedVisualAnchorLocation = FVector::ZeroVector;

	/** Query 最终冻结的视觉锚点 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Query")
	FVector ResolvedVisualAnchorLocation = FVector::ZeroVector;

	/** 满尺度共享轨迹的预测释放点 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Query")
	FVector RequestedReleaseLocation = FVector::ZeroVector;

	/** 安全尺度共享轨迹的预测释放点 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Query")
	FVector ResolvedReleaseLocation = FVector::ZeroVector;

	/** 首次轨迹检查使用的统一尺度 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Query")
	float RequestedTrajectoryScale = 1.f;

	/** 二分检查得到的最大安全尺度 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Query")
	float ResolvedTrajectoryScale = 0.f;

	/** 满尺度轨迹遇到的首个阻挡 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Query")
	FHitResult FirstBlockingHit;

	/** 首个阻挡在预测采样点中的分段索引 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Query")
	int32 BlockingSegmentIndex = INDEX_NONE;
};

/** 输入边沿冻结的无锚点钩锁领域上下文 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaGrappleActionContext
{
	GENERATED_BODY()

	/** Query 执行时的角色位置 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Context")
	FVector QueryStartLocation = FVector::ZeroVector;

	/** 输入边沿相机水平前向 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Context")
	FVector TravelDirection = FVector::ForwardVector;

	/** 输入边沿完整观察旋转，只用于构造受限网络 Seed */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Context")
	FRotator ViewRotation = FRotator::ZeroRotator;

	/** 运行时有限横向修正使用的冻结轴 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Context")
	FVector LateralAxis = FVector::RightVector;

	/** 仅供表现消费者使用的独立视觉锚点 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Context")
	FVector VisualAnchorLocation = FVector::ZeroVector;

	/** Definition 冻结的满尺度前向距离 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Context")
	float ForwardTravelDistance = 0.f;

	/** Definition 冻结的满尺度垂直高度 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Context")
	float VerticalTravelHeight = 0.f;

	/** Query 冻结的统一轨迹尺度 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Context")
	float TrajectoryScale = 0.f;

	/** 输入边沿的完整三维进入速度 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Context")
	FVector EntryVelocity = FVector::ZeroVector;

	/** Query 时是否处于 Grounded 状态 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Context")
	bool bStartedGrounded = false;

	/** 产生本次 Query 的输入帧号 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Context")
	int64 QueryFrameNumber = 0;

	/** @return 冻结内容是否满足基础数值约束 */
	bool IsRuntimeValid() const
	{
		const auto IsFiniteVector = [](const FVector& Value)
		{
			return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
		};

		return IsFiniteVector(QueryStartLocation) && IsFiniteVector(TravelDirection) &&
		       TravelDirection.IsNormalized() && FMath::IsNearlyZero(TravelDirection.Z) &&
		       !ViewRotation.ContainsNaN() && IsFiniteVector(LateralAxis) && LateralAxis.IsNormalized() &&
		       FMath::IsNearlyZero(LateralAxis.Z) &&
		       FMath::IsNearlyZero(FVector::DotProduct(TravelDirection, LateralAxis), 0.001f) &&
		       IsFiniteVector(VisualAnchorLocation) && FMath::IsFinite(ForwardTravelDistance) &&
		       ForwardTravelDistance > 0.f && FMath::IsFinite(VerticalTravelHeight) && VerticalTravelHeight >= 0.f &&
		       FMath::IsFinite(TrajectoryScale) && TrajectoryScale > 0.f && TrajectoryScale <= 1.f &&
		       IsFiniteVector(EntryVelocity) && QueryFrameNumber > 0;
	}
};

/** Grapple Debug 只消费的查询快照 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaGrappleQueryDebugSnapshot
{
	GENERATED_BODY()

	/** 是否已经执行过至少一次 Query */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Debug")
	bool bHasQuery = false;

	/** 最近一次完整 Query 结果 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Debug")
	FWuwaGrappleQueryResult QueryResult;

	/** 最近一次最终尺度对应的世界空间预测点 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Debug")
	TArray<FVector> PredictedTrajectoryPoints;
};

/** 无锚点钩锁基础轨迹、横向修正与出口速度配置 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaGrappleMovementSpec
{
	GENERATED_BODY()

	/** 起手状态标记时长 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float WindupDuration = 0.05f;

	/** 基础牵引轨迹时长 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Timing", meta = (ClampMin = "0.01", Units = "s"))
	float PullDuration = 0.55f;

	/** 异常运行安全上限 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Timing", meta = (ClampMin = "0.01", Units = "s"))
	float MaximumDuration = 1.0f;

	/** 满尺度前向位移 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Trajectory", meta = (ClampMin = "0.0", Units = "cm"))
	float ForwardTravelDistance = 900.f;

	/** 满尺度垂直位移 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Trajectory", meta = (ClampMin = "0.0", Units = "cm"))
	float VerticalTravelHeight = 320.f;

	/** 归一化前向位移曲线 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Trajectory")
	TObjectPtr<UCurveFloat> ForwardDistanceCurve = nullptr;

	/** 归一化垂直位移曲线 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Trajectory")
	TObjectPtr<UCurveFloat> VerticalDistanceCurve = nullptr;

	/** 横向输入加速度 */
	UPROPERTY(EditAnywhere,
	          BlueprintReadOnly,
	          Category = "Grapple|Steering",
	          meta = (ClampMin = "0.0", Units = "cm/s^2"))
	float LateralSteeringAcceleration = 900.f;

	/** 横向偏移上限 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Steering", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumLateralOffset = 160.f;

	/** 无横向输入时的回正速度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Steering", meta = (ClampMin = "0.0", Units = "cm/s"))
	float LateralReturnSpeed = 360.f;

	/** 完整进入速度的保留比例 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Exit", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EntryVelocityRetention = 0.35f;

	/** 前向切线速度缩放 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Exit", meta = (ClampMin = "0.0"))
	float ExitForwardSpeedScale = 0.8f;

	/** 向上切线速度缩放 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Exit", meta = (ClampMin = "0.0"))
	float ExitUpSpeedScale = 0.75f;

	/** 正常释放时的最低前向速度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Exit", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MinimumExitForwardSpeed = 700.f;

	/** 正常释放时的最低向上速度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Exit", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MinimumExitUpSpeed = 420.f;

	/** 正常释放速度上限 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Exit", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaximumExitSpeed = 1500.f;

	/** 起手标记阶段使用的局部重力倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Physics", meta = (ClampMin = "0.0"))
	float GravityScaleDuringWindup = 0.f;

	/** @return 配置是否满足运行时数值约束 */
	bool IsRuntimeValid() const
	{
		return FMath::IsFinite(WindupDuration) && WindupDuration >= 0.f && FMath::IsFinite(PullDuration) &&
		       PullDuration > 0.f && FMath::IsFinite(MaximumDuration) &&
		       MaximumDuration > WindupDuration + PullDuration && FMath::IsFinite(ForwardTravelDistance) &&
		       ForwardTravelDistance > 0.f && FMath::IsFinite(VerticalTravelHeight) && VerticalTravelHeight >= 0.f &&
		       ForwardDistanceCurve != nullptr && VerticalDistanceCurve != nullptr &&
		       FMath::IsFinite(LateralSteeringAcceleration) && LateralSteeringAcceleration >= 0.f &&
		       FMath::IsFinite(MaximumLateralOffset) && MaximumLateralOffset >= 0.f &&
		       FMath::IsFinite(LateralReturnSpeed) && LateralReturnSpeed >= 0.f &&
		       FMath::IsFinite(EntryVelocityRetention) && EntryVelocityRetention >= 0.f &&
		       EntryVelocityRetention <= 1.f && FMath::IsFinite(ExitForwardSpeedScale) &&
		       ExitForwardSpeedScale >= 0.f && FMath::IsFinite(ExitUpSpeedScale) && ExitUpSpeedScale >= 0.f &&
		       FMath::IsFinite(MinimumExitForwardSpeed) && MinimumExitForwardSpeed >= 0.f &&
		       FMath::IsFinite(MinimumExitUpSpeed) && MinimumExitUpSpeed >= 0.f && FMath::IsFinite(MaximumExitSpeed) &&
		       MaximumExitSpeed > 0.f && MinimumExitForwardSpeed <= MaximumExitSpeed &&
		       MinimumExitUpSpeed <= MaximumExitSpeed && FMath::IsFinite(GravityScaleDuringWindup) &&
		       GravityScaleDuringWindup >= 0.f;
	}
};

/** Grapple Movement 原语的结束原因 */
UENUM(BlueprintType)
enum class EWuwaGrappleMovementEndReason : uint8
{
	/** Movement 尚未结束 */
	None,

	/** 共享轨迹到达 T=1 后正常释放 */
	Released,

	/** 运行时遇到任意 Blocking Hit */
	Blocked,

	/** 超过 MaximumDuration 异常上限 */
	TimedOut,

	/** Grapple 活动期间发生真实落地 */
	Landed,

	/** Action Stop 或 Rollback 主动停止 */
	Stopped,

	/** 运行数据或组件状态失效 */
	InvalidRuntime,

	/** Owning Client 的预测请求被服务端拒绝 */
	NetworkRejected,

	/** Owning Client 的预测上下文超出服务端容差 */
	NetworkCorrection,

	/** 角色死亡导致运行实例终止 */
	Death,

	/** 角色硬直导致运行实例终止 */
	Staggered,

	/** Avatar 身份切换导致运行实例终止 */
	AvatarChanged,

	/** 组件 EndPlay 导致运行实例终止 */
	EndPlay
};

/** 服务端复制给 Simulated Proxy 的 Grapple 只读表现状态 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaReplicatedGrapplePresentationState
{
	GENERATED_BODY()

	/** 跨端稳定动作序号 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Network")
	FWuwaNetworkActionGeneration NetworkGeneration;

	/** 服务端 Query 生成的视觉锚点 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Network")
	FVector_NetQuantize10 AuthorityVisualAnchor = FVector_NetQuantize10(ForceInitToZero);

	/** 服务端 Grapple 当前阶段 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Network")
	EWuwaGrappleRuntimePhase Phase = EWuwaGrappleRuntimePhase::None;

	/** 服务端同步世界时间中的 Montage 开始时刻 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Network")
	float ServerStartTime = 0.f;

	/** 服务端是否仍持有 Grapple Runtime */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Network")
	bool bActive = false;

	/** 非活动状态携带的最终 Movement 原因 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Network")
	EWuwaGrappleMovementEndReason EndReason = EWuwaGrappleMovementEndReason::None;

	/** @return 当前复制字段是否构成完整有限状态 */
	bool IsPayloadValid() const
	{
		const bool bFiniteAnchor = FMath::IsFinite(AuthorityVisualAnchor.X) &&
		                           FMath::IsFinite(AuthorityVisualAnchor.Y) && FMath::IsFinite(AuthorityVisualAnchor.Z);
		return NetworkGeneration.IsValid() && bFiniteAnchor && FMath::IsFinite(ServerStartTime) &&
		       ServerStartTime >= 0.f && Phase != EWuwaGrappleRuntimePhase::None &&
		       (bActive ? EndReason == EWuwaGrappleMovementEndReason::None
		                : EndReason != EWuwaGrappleMovementEndReason::None);
	}

	/** 清空跨 Avatar 表现状态 */
	void Reset()
	{
		*this = FWuwaReplicatedGrapplePresentationState();
	}
};

/** CharacterMovement 发布的 Grapple 事实类型 */
UENUM()
enum class EWuwaGrappleMovementFactType : uint8
{
	/** Grapple 进入新的运行阶段 */
	PhaseChanged,

	/** Grapple Movement 已结束并释放 CustomMode */
	Ended
};

/** 与 ActionHandle 分离的 Grapple Movement 实例身份 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaGrappleMovementHandle
{
	GENERATED_BODY()

	/** 当前 CharacterMovement 内唯一编号 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Movement")
	int64 Value = 0;

	/** @return Handle 是否引用有效 Movement 实例 */
	bool IsValid() const
	{
		return Value > 0;
	}

	/** @return 是否引用同一次 Movement */
	bool operator==(const FWuwaGrappleMovementHandle& Other) const
	{
		return Value == Other.Value;
	}

	/** @return 是否引用不同 Movement */
	bool operator!=(const FWuwaGrappleMovementHandle& Other) const
	{
		return !(*this == Other);
	}
};

/** Capability 提交给 CharacterMovement 的冻结 Grapple 请求 */
USTRUCT()
struct WUWA_API FWuwaGrappleMovementRequest
{
	GENERATED_BODY()

	/** 本次运行使用的值拷贝 Movement 配置 */
	UPROPERTY()
	FWuwaGrappleMovementSpec Spec;

	/** Query 与 Prepare 共同冻结的领域上下文 */
	UPROPERTY()
	FWuwaGrappleActionContext Context;

	/** 当前 Movement 所属跨端动作序号 */
	UPROPERTY()
	FWuwaNetworkActionGeneration NetworkGeneration;

	/** @return 请求是否可以进入 Movement 原语 */
	bool IsRuntimeValid() const
	{
		return Spec.IsRuntimeValid() && Context.IsRuntimeValid();
	}
};

/** CharacterMovement 对外提供的 Grapple 只读快照 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaGrappleMovementSnapshot
{
	GENERATED_BODY()

	/** 当前或最近一次 Movement Handle */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Movement")
	FWuwaGrappleMovementHandle Handle;

	/** 当前或最近一次 Movement 所属跨端动作序号 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Movement")
	FWuwaNetworkActionGeneration NetworkGeneration;

	/** 是否仍持有 Grapple CustomMode */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Movement")
	bool bActive = false;

	/** 当前运行阶段 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Movement")
	EWuwaGrappleRuntimePhase Phase = EWuwaGrappleRuntimePhase::None;

	/** 最近一次 Movement 结束原因 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Movement")
	EWuwaGrappleMovementEndReason EndReason = EWuwaGrappleMovementEndReason::None;

	/** 实际 Commit 起点 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Movement")
	FVector CommitStartLocation = FVector::ZeroVector;

	/** Movement 原语累计时间 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Movement")
	float ElapsedTime = 0.f;

	/** 当前牵引阶段归一化时间 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Movement")
	float NormalizedPullTime = 0.f;

	/** 当前共享轨迹基础位移 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Movement")
	FVector BaseOffset = FVector::ZeroVector;

	/** 当前冻结横向轴上的累计偏移 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Movement")
	float AccumulatedLateralOffset = 0.f;

	/** 当前冻结横向轴上的速度 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Movement")
	float LateralVelocity = 0.f;

	/** 当前横向输入读取来源 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Movement")
	EWuwaGrappleLateralInputSource LateralInputSource = EWuwaGrappleLateralInputSource::None;

	/** 最近一次共享轨迹基础切线 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Movement")
	FVector LastBaseTangentVelocity = FVector::ZeroVector;

	/** 正常 Released 时应用的最终出口速度 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Movement")
	FVector ExitVelocity = FVector::ZeroVector;
};

/** Grapple 预测、权威 Query 与校正的只读网络快照 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaGrappleNetworkRuntimeSnapshot
{
	GENERATED_BODY()

	/** Owning Client 当前预测的 Grapple 序号 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Network")
	FWuwaNetworkActionGeneration PredictedGeneration;

	/** 服务端最近接受的 Grapple 序号 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Network")
	FWuwaNetworkActionGeneration AuthorityGeneration;

	/** 客户端预测轨迹尺度 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Network")
	float PredictedTrajectoryScale = 0.f;

	/** 服务端权威轨迹尺度 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Network")
	float AuthorityTrajectoryScale = 0.f;

	/** 客户端预测水平行进 Yaw */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Network")
	float PredictedTravelYaw = 0.f;

	/** 客户端预测 Query 起点 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Network")
	FVector PredictedStartLocation = FVector::ZeroVector;

	/** 服务端权威水平行进 Yaw */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Network")
	float AuthorityTravelYaw = 0.f;

	/** 服务端权威 Query 起点摘要 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Network")
	FVector AuthorityStartLocation = FVector::ZeroVector;

	/** 最近一次客户端与服务端起点摘要距离 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Network")
	float LastCorrectionDistance = 0.f;

	/** 最近一次服务端 Query 失败原因 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Network")
	EWuwaGrappleQueryFailureReason LastQueryRejectReason = EWuwaGrappleQueryFailureReason::None;

	/** `PhysGrapple` 当前横向输入来源 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Network")
	EWuwaGrappleLateralInputSource LateralInputSource = EWuwaGrappleLateralInputSource::None;

	/** 客户端是否因上下文偏差终止了预测 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Network")
	bool bCorrectionRequired = false;
};

/** CharacterMovement 向 Grapple Capability 广播的只读事实 */
USTRUCT()
struct WUWA_API FWuwaGrappleMovementFact
{
	GENERATED_BODY()

	/** 事实所属 Movement 实例 */
	UPROPERTY()
	FWuwaGrappleMovementHandle Handle;

	/** 阶段变化或结束事实 */
	UPROPERTY()
	EWuwaGrappleMovementFactType FactType = EWuwaGrappleMovementFactType::PhaseChanged;

	/** 阶段变化后的状态 */
	UPROPERTY()
	EWuwaGrappleRuntimePhase Phase = EWuwaGrappleRuntimePhase::None;

	/** Ended 事实携带的原因 */
	UPROPERTY()
	EWuwaGrappleMovementEndReason EndReason = EWuwaGrappleMovementEndReason::None;

	/** Released 事实携带的出口速度 */
	UPROPERTY()
	FVector ExitVelocity = FVector::ZeroVector;
};

FORCEINLINE uint32 GetTypeHash(const FWuwaGrappleMovementHandle& Handle)
{
	return GetTypeHash(Handle.Value);
}
