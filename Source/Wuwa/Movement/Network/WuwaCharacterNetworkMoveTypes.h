#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "GameplayTagContainer.h"
#include "Movement/WuwaMovementTypes.h"
#include "WuwaCharacterNetworkMoveTypes.generated.h"

/** 自定义移动协议承载的一次性命令类型 */
UENUM(BlueprintType)
enum class EWuwaNetworkMovementCommandKind : uint8
{
	/** 仅包含连续移动元数据 */
	None,

	/** 普通跳跃输入边沿 */
	Jump,

	/** Dash、Backstep 或二段跳动作 */
	LegacyAction,

	/** 请求取消正在运行的 Legacy Action */
	LegacyActionExit,

	/** Grapple 查询种子 */
	Grapple
};

/** Legacy Movement Action 允许通过网络请求的有限退出类型 */
UENUM(BlueprintType)
enum class EWuwaLegacyActionExitKind : uint8
{
	/** 没有退出请求 */
	None,

	/** 在移动取消窗口内取消 */
	MoveCancel,

	/** 在完成出口就绪后正常结束 */
	CompletedExit
};

/** 服务端拒绝移动命令的有限原因 */
UENUM(BlueprintType)
enum class EWuwaNetworkActionRejectReason : uint8
{
	/** 没有拒绝 */
	None,

	/** 字段缺失、非有限或组合非法 */
	InvalidPayload,

	/** Generation 早于服务端已经处理的请求 */
	StaleGeneration,

	/** Generation 已经被服务端处理 */
	DuplicateGeneration,

	/** 当前阶段不支持该命令类型 */
	UnsupportedCommandKind,

	/** 当前 Move 的一次性命令槽已经占用 */
	NetworkCommandSlotOccupied,

	/** 服务端没有安装唯一命令处理器 */
	ProcessorUnavailable,

	/** 命令处理器拒绝请求 */
	ProcessorRejected
};

/** 移动命令附加标志 */
UENUM()
enum class EWuwaNetworkMovementCommandFlags : uint8
{
	None = 0,
	HasDesiredFacing = 1 << 0,
	AllowFacingSnap = 1 << 1,
	SprintRun = 1 << 2
};
ENUM_CLASS_FLAGS(EWuwaNetworkMovementCommandFlags);

/** 跨端对应同一次动作请求的稳定序号 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaNetworkActionGeneration
{
	GENERATED_BODY()

	/** 零表示没有有效请求 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	int32 Value = 0;

	/** @return Generation 是否可以参与跨端匹配 */
	bool IsValid() const
	{
		return Value > 0;
	}

	bool operator==(const FWuwaNetworkActionGeneration& Other) const
	{
		return Value == Other.Value;
	}

	bool operator!=(const FWuwaNetworkActionGeneration& Other) const
	{
		return !(*this == Other);
	}
};

FORCEINLINE uint32 GetTypeHash(const FWuwaNetworkActionGeneration& Generation)
{
	return GetTypeHash(Generation.Value);
}

/** SavedMove 与 Packed Move 之间唯一的一次性移动命令 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaPendingNetworkMovementCommand
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	FWuwaNetworkActionGeneration Generation;

	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	EWuwaNetworkMovementCommandKind Kind = EWuwaNetworkMovementCommandKind::None;

	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	FGameplayTag ActionTag;

	/** 取消命令指向的 Legacy Action 启动序号 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	FWuwaNetworkActionGeneration TargetActionGeneration;

	/** 触发取消的二维移动输入 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	FVector2D ExitMoveIntent = FVector2D::ZeroVector;

	/** 当前请求的有限退出类型 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	EWuwaLegacyActionExitKind ExitKind = EWuwaLegacyActionExitKind::None;

	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	EWuwaJumpRequestKind JumpType = EWuwaJumpRequestKind::None;

	/** AirJump 必须匹配的普通跳跃滞空 Generation */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	int32 AirCycleGeneration = 0;

	/** 量化前的平面单位方向 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	FVector WorldDirection = FVector::ZeroVector;

	/** Grapple 输入边沿的二维移动输入 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	FVector2D GrappleInputDirection = FVector2D::ZeroVector;

	/** Grapple Query 使用的受限观察 Yaw */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	float GrappleViewYaw = 0.f;

	/** Grapple Query 使用的受限观察 Pitch */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	float GrappleViewPitch = 0.f;

	/** 客户端预测轨迹尺度，只用于诊断与容差判断 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	float ClientPredictedGrappleScale = 0.f;

	/** 客户端执行预测 Query 的帧号，只用于诊断 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	int32 ClientGrappleQueryFrame = 0;

	/** 量化前的世界 Yaw */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	float DesiredFacingYaw = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	uint8 Flags = 0;

	/** @return 是否包含需要写入 MoveData 的字段 */
	bool HasData() const
	{
		return Kind != EWuwaNetworkMovementCommandKind::None || Flags != 0;
	}

	/** @return 是否包含指定标志 */
	bool HasFlag(const EWuwaNetworkMovementCommandFlags Flag) const
	{
		return (Flags & static_cast<uint8>(Flag)) != 0;
	}

	/** @return 是否必须独占 SavedMove */
	bool IsOneShot() const
	{
		return Kind != EWuwaNetworkMovementCommandKind::None ||
		       (HasFlag(EWuwaNetworkMovementCommandFlags::HasDesiredFacing) &&
		        HasFlag(EWuwaNetworkMovementCommandFlags::AllowFacingSnap));
	}

	/** @return 是否为 Legacy Action 移动取消命令 */
	bool IsLegacyActionExit() const
	{
		return Kind == EWuwaNetworkMovementCommandKind::LegacyActionExit;
	}

	/** @return 字段组合是否可以进入网络协议 */
	bool IsPayloadValid() const
	{
		constexpr uint8 AllowedFlags = static_cast<uint8>(EWuwaNetworkMovementCommandFlags::HasDesiredFacing) |
		                               static_cast<uint8>(EWuwaNetworkMovementCommandFlags::AllowFacingSnap) |
		                               static_cast<uint8>(EWuwaNetworkMovementCommandFlags::SprintRun);
		const bool bFiniteDirection =
		    FMath::IsFinite(WorldDirection.X) && FMath::IsFinite(WorldDirection.Y) && FMath::IsFinite(WorldDirection.Z);
		const bool bFiniteFacing = FMath::IsFinite(DesiredFacingYaw);
		const bool bFiniteExitMoveIntent = FMath::IsFinite(ExitMoveIntent.X) && FMath::IsFinite(ExitMoveIntent.Y);
		const bool bFiniteGrappleInput =
		    FMath::IsFinite(GrappleInputDirection.X) && FMath::IsFinite(GrappleInputDirection.Y);
		const bool bFiniteGrappleSeed = FMath::IsFinite(GrappleViewYaw) && FMath::IsFinite(GrappleViewPitch) &&
		                                FMath::IsFinite(ClientPredictedGrappleScale);
		if ((Flags & ~AllowedFlags) != 0 || !bFiniteDirection || !bFiniteFacing || !bFiniteExitMoveIntent ||
		    !bFiniteGrappleInput || !bFiniteGrappleSeed || ExitMoveIntent.GetAbsMax() > 1.001f ||
		    GrappleInputDirection.Size() > 1.001f ||
		    (HasFlag(EWuwaNetworkMovementCommandFlags::AllowFacingSnap) &&
		     !HasFlag(EWuwaNetworkMovementCommandFlags::HasDesiredFacing)))
		{
			return false;
		}

		if (Kind == EWuwaNetworkMovementCommandKind::None)
		{
			return !Generation.IsValid() && !TargetActionGeneration.IsValid() && !ActionTag.IsValid() &&
			       ExitKind == EWuwaLegacyActionExitKind::None && ExitMoveIntent.IsNearlyZero() &&
			       JumpType == EWuwaJumpRequestKind::None && AirCycleGeneration == 0 && WorldDirection.IsNearlyZero() &&
			       GrappleInputDirection.IsNearlyZero() && FMath::IsNearlyZero(GrappleViewYaw) &&
			       FMath::IsNearlyZero(GrappleViewPitch) && FMath::IsNearlyZero(ClientPredictedGrappleScale) &&
			       ClientGrappleQueryFrame == 0;
		}

		if (!Generation.IsValid())
		{
			return false;
		}

		if (Kind == EWuwaNetworkMovementCommandKind::Jump)
		{
			return !TargetActionGeneration.IsValid() && !ActionTag.IsValid() &&
			       ExitKind == EWuwaLegacyActionExitKind::None && ExitMoveIntent.IsNearlyZero() &&
			       JumpType != EWuwaJumpRequestKind::None && AirCycleGeneration == 0 && WorldDirection.IsNearlyZero() &&
			       GrappleInputDirection.IsNearlyZero() && FMath::IsNearlyZero(GrappleViewYaw) &&
			       FMath::IsNearlyZero(GrappleViewPitch) && FMath::IsNearlyZero(ClientPredictedGrappleScale) &&
			       ClientGrappleQueryFrame == 0;
		}

		if (Kind == EWuwaNetworkMovementCommandKind::LegacyActionExit)
		{
			const bool bHasOnlyContinuousFlags = !HasFlag(EWuwaNetworkMovementCommandFlags::HasDesiredFacing) &&
			                                     !HasFlag(EWuwaNetworkMovementCommandFlags::AllowFacingSnap);
			return TargetActionGeneration.IsValid() && TargetActionGeneration != Generation && ActionTag.IsValid() &&
			       ExitKind != EWuwaLegacyActionExitKind::None &&
			       static_cast<uint8>(ExitKind) <= static_cast<uint8>(EWuwaLegacyActionExitKind::CompletedExit) &&
			       !ExitMoveIntent.IsNearlyZero(0.1f) && bHasOnlyContinuousFlags &&
			       JumpType == EWuwaJumpRequestKind::None && AirCycleGeneration == 0 && WorldDirection.IsNearlyZero() &&
			       GrappleInputDirection.IsNearlyZero() && FMath::IsNearlyZero(GrappleViewYaw) &&
			       FMath::IsNearlyZero(GrappleViewPitch) && FMath::IsNearlyZero(ClientPredictedGrappleScale) &&
			       ClientGrappleQueryFrame == 0;
		}

		if (Kind == EWuwaNetworkMovementCommandKind::LegacyAction)
		{
			return !TargetActionGeneration.IsValid() && ExitKind == EWuwaLegacyActionExitKind::None &&
			       ExitMoveIntent.IsNearlyZero() && ActionTag.IsValid() && JumpType == EWuwaJumpRequestKind::None &&
			       AirCycleGeneration >= 0 && !WorldDirection.GetSafeNormal2D().IsNearlyZero() &&
			       GrappleInputDirection.IsNearlyZero() && FMath::IsNearlyZero(GrappleViewYaw) &&
			       FMath::IsNearlyZero(GrappleViewPitch) && FMath::IsNearlyZero(ClientPredictedGrappleScale) &&
			       ClientGrappleQueryFrame == 0;
		}

		if (Kind == EWuwaNetworkMovementCommandKind::Grapple)
		{
			const bool bFacingMatchesView =
			    HasFlag(EWuwaNetworkMovementCommandFlags::HasDesiredFacing) &&
			    HasFlag(EWuwaNetworkMovementCommandFlags::AllowFacingSnap) &&
			    !HasFlag(EWuwaNetworkMovementCommandFlags::SprintRun) &&
			    FMath::Abs(FMath::FindDeltaAngleDegrees(DesiredFacingYaw, GrappleViewYaw)) <= 2.f;
			return !TargetActionGeneration.IsValid() && ExitKind == EWuwaLegacyActionExitKind::None &&
			       ExitMoveIntent.IsNearlyZero() && ActionTag.IsValid() && JumpType == EWuwaJumpRequestKind::None &&
			       AirCycleGeneration == 0 && WorldDirection.IsNearlyZero() && GrappleViewYaw >= -180.f &&
			       GrappleViewYaw <= 180.f && GrappleViewPitch >= -90.f && GrappleViewPitch <= 90.f &&
			       ClientPredictedGrappleScale > 0.f && ClientPredictedGrappleScale <= 1.f &&
			       ClientGrappleQueryFrame > 0 && bFacingMatchesView;
		}

		return false;
	}

	/** 清空全部协议字段 */
	void Reset()
	{
		*this = FWuwaPendingNetworkMovementCommand();
	}
};

/** 服务端随移动响应返回的有限动作结果 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaNetworkActionResponse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	bool bHasResponse = false;

	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	FWuwaNetworkActionGeneration ProcessedGeneration;

	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	bool bAccepted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	EWuwaNetworkActionRejectReason RejectReason = EWuwaNetworkActionRejectReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	FWuwaNetworkActionGeneration AuthorityGeneration;

	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	uint8 AuthorityMovementMode = MOVE_None;

	/** 当前响应是否携带 Grapple 权威 Query 摘要 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	bool bHasGrappleAuthoritySummary = false;

	/** 服务端 Query 最终接受的轨迹尺度 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	float AuthorityGrappleTrajectoryScale = 0.f;

	/** 服务端 Query 最终接受的水平行进 Yaw */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	float AuthorityGrappleTravelYaw = 0.f;

	/** 服务端 Query 使用的量化 Capsule 起点 */
	UPROPERTY(BlueprintReadOnly, Category = "Network Action")
	FVector_NetQuantize10 AuthorityGrappleStartLocation = FVector_NetQuantize10(ForceInitToZero);

	/** @return Grapple 权威摘要是否完整且有限 */
	bool IsGrappleAuthoritySummaryValid() const
	{
		return bHasGrappleAuthoritySummary && bAccepted && FMath::IsFinite(AuthorityGrappleTrajectoryScale) &&
		       AuthorityGrappleTrajectoryScale > 0.f && AuthorityGrappleTrajectoryScale <= 1.f &&
		       FMath::IsFinite(AuthorityGrappleTravelYaw) && FMath::IsFinite(AuthorityGrappleStartLocation.X) &&
		       FMath::IsFinite(AuthorityGrappleStartLocation.Y) && FMath::IsFinite(AuthorityGrappleStartLocation.Z);
	}

	/** @return 接受指定 Generation 的响应 */
	static FWuwaNetworkActionResponse Accepted(const FWuwaNetworkActionGeneration& Generation)
	{
		FWuwaNetworkActionResponse Response;
		Response.bHasResponse = true;
		Response.ProcessedGeneration = Generation;
		Response.bAccepted = true;
		Response.AuthorityGeneration = Generation;
		return Response;
	}

	/** @return 拒绝指定 Generation 的响应 */
	static FWuwaNetworkActionResponse Rejected(const FWuwaNetworkActionGeneration& Generation,
	                                           const EWuwaNetworkActionRejectReason Reason)
	{
		FWuwaNetworkActionResponse Response;
		Response.bHasResponse = true;
		Response.ProcessedGeneration = Generation;
		Response.RejectReason = Reason;
		return Response;
	}

	/**
     * 构造携带 Grapple 权威 Query 摘要的接受响应
     * @param Generation			已处理的 Grapple 序号
     * @param TrajectoryScale	服务端最终轨迹尺度
     * @param TravelYaw			服务端最终水平行进 Yaw
     * @param StartLocation	服务端 Query 起点
     * @return 完整 Grapple 接受响应
     */
	static FWuwaNetworkActionResponse AcceptedGrapple(const FWuwaNetworkActionGeneration& Generation,
	                                                  float TrajectoryScale,
	                                                  float TravelYaw,
	                                                  const FVector& StartLocation)
	{
		FWuwaNetworkActionResponse Response = Accepted(Generation);
		Response.bHasGrappleAuthoritySummary = true;
		Response.AuthorityGrappleTrajectoryScale = TrajectoryScale;
		Response.AuthorityGrappleTravelYaw = FRotator::NormalizeAxis(TravelYaw);
		Response.AuthorityGrappleStartLocation = StartLocation;
		return Response;
	}
};
