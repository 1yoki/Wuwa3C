#pragma once

#include "CoreMinimal.h"

struct FWuwaGrappleActionContext;
struct FWuwaGrappleMovementSpec;

/** 共享轨迹在指定归一化时刻的基础样本 */
struct WUWA_API FWuwaGrappleTrajectorySample
{
	/** 相对 Query 起点的基础位移 */
	FVector BaseOffset = FVector::ZeroVector;

	/** 按 PullDuration 换算的基础切线速度 */
	FVector TangentVelocity = FVector::ZeroVector;

	/** @return 样本是否只包含有限数值 */
	bool IsFinite() const;
};

/** Query 与 CharacterMovement 共用的无状态轨迹求值器 */
struct WUWA_API FWuwaGrappleTrajectoryEvaluator
{
	/**
     * 计算冻结基础轨迹样本
     * @param Spec	Movement 数据配置
     * @param Context	冻结的钩锁领域上下文
     * @param NormalizedPullTime	归一化牵引时间
     * @return 基础位移和按秒计的切线速度
     */
	static FWuwaGrappleTrajectorySample
	Evaluate(const FWuwaGrappleMovementSpec& Spec, const FWuwaGrappleActionContext& Context, float NormalizedPullTime);
};
