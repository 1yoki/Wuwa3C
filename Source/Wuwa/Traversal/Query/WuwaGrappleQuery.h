#pragma once

#include "CoreMinimal.h"
#include "Actions/Contracts/WuwaActionTypes.h"
#include "Traversal/Contracts/WuwaTraversalTypes.h"

class AActor;
class UWorld;
class UWuwaGrappleActionDefinition;

/** 无锚点钩锁的无状态世界查询求值器 */
class WUWA_API FWuwaGrappleQuery
{
public:
	/**
     * 执行一次无副作用世界查询并冻结领域上下文
     * @param Snapshot	输入边沿的角色快照
     * @param Definition	Free Grapple 数据定义
     * @param QueryFrameNumber	输入边沿帧号
     * @param OutContext	成功时接收冻结领域上下文
     * @param OutResult	接收完整诊断结果
     * @param OutPredictedPoints	接收最终尺度的世界空间预测点
     * @return 查询是否成功
     */
	static bool Query(const FWuwaActionResolutionSnapshot& Snapshot,
	                  const UWuwaGrappleActionDefinition* Definition,
	                  int64 QueryFrameNumber,
	                  FWuwaGrappleActionContext& OutContext,
	                  FWuwaGrappleQueryResult& OutResult,
	                  TArray<FVector>& OutPredictedPoints);

	/**
     * 用当前 Capsule 起点复核已经冻结的方向、锚点和轨迹尺度
     * @param Definition	Free Grapple 数据定义
     * @param Owner	执行复核查询的角色
     * @param CommitStartLocation	当前实际 Capsule 起点
     * @param FrozenContext	输入边沿冻结的领域上下文
     * @param OutAdjustedContext	接收只降低尺度的运行上下文
     * @param OutResult	接收完整复核诊断
     * @return 当前起点是否仍允许 Commit
     */
	static bool RevalidateFrozenContext(const UWuwaGrappleActionDefinition* Definition,
	                                    AActor* Owner,
	                                    const FVector& CommitStartLocation,
	                                    const FWuwaGrappleActionContext& FrozenContext,
	                                    FWuwaGrappleActionContext& OutAdjustedContext,
	                                    FWuwaGrappleQueryResult& OutResult);

private:
	/**
     * 生成指定尺度的完整预测点
     * @param Definition	轨迹与采样配置
     * @param BaseContext	冻结方向和距离
     * @param TrajectoryScale	待检查统一尺度
     * @param OutPoints	接收世界空间预测点
     * @return 所有样本是否有限
     */
	static bool BuildTrajectoryPoints(const UWuwaGrappleActionDefinition& Definition,
	                                  const FWuwaGrappleActionContext& BaseContext,
	                                  float TrajectoryScale,
	                                  TArray<FVector>& OutPoints);

	/**
     * 检查预测弧线全部分段
     * @param World	执行 Sweep 的世界
     * @param Owner	需要忽略的查询发起者
     * @param Definition	查询配置
     * @param Points	世界空间预测点
     * @param OutBlockingHit	接收首个阻挡
     * @param OutBlockingSegmentIndex	接收首个阻挡段索引
     * @return 轨迹是否没有阻挡
     */
	static bool IsTrajectoryClear(UWorld* World,
	                              AActor* Owner,
	                              const UWuwaGrappleActionDefinition& Definition,
	                              const TArray<FVector>& Points,
	                              FHitResult& OutBlockingHit,
	                              int32& OutBlockingSegmentIndex);

	/**
     * 检查释放点 Capsule 空间
     * @param World	执行 Overlap 检查的世界
     * @param Owner	需要忽略的查询发起者
     * @param Definition	查询配置
     * @param ReleaseLocation	预测释放位置
     * @return 释放点是否没有阻挡
     */
	static bool HasReleaseClearance(UWorld* World,
	                                AActor* Owner,
	                                const UWuwaGrappleActionDefinition& Definition,
	                                const FVector& ReleaseLocation);

	/**
     * 解析不高于请求值的最大安全统一尺度
     * @param World	执行世界查询的世界
     * @param Owner	需要忽略的查询发起者
     * @param Definition	查询配置
     * @param BaseContext	使用当前起点的轨迹上下文
     * @param RequestedScale	冻结的最大允许尺度
     * @param OutResolvedScale	接收最大安全尺度
     * @param InOutResult	写入释放点和首个阻挡诊断
     * @param OutPoints	接收最终尺度的预测弧线
     * @return 求值和碰撞查询是否完成
     */
	static bool ResolveSafeTrajectory(UWorld* World,
	                                  AActor* Owner,
	                                  const UWuwaGrappleActionDefinition& Definition,
	                                  const FWuwaGrappleActionContext& BaseContext,
	                                  float RequestedScale,
	                                  float& OutResolvedScale,
	                                  FWuwaGrappleQueryResult& InOutResult,
	                                  TArray<FVector>& OutPoints);
};
