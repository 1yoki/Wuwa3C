#pragma once

#include "CoreMinimal.h"
#include "Actions/Contracts/WuwaActionTypes.h"
#include "Movement/Actions/WuwaMovementActionTypes.h"

class UWuwaMovementActionDefinition;

/** Movement Action 使用的无状态验证与纯计算策略 */
class WUWA_API FWuwaMovementActionPolicies
{
public:
	/**
     * 判断冻结环境和当前物理事实是否同时满足条件
     * @param Condition Definition 要求的移动条件
     * @param SnapshotMovementMode 输入边沿冻结的 MovementMode
     * @param SnapshotCustomMovementMode	输入边沿冻结的 CustomMovementMode
     * @param bCurrentlyGrounded 当前是否接地
     * @param bCurrentlyFalling 当前是否 Falling
     * @param bCurrentlyGrappling		当前是否持有 Grapple Movement
     * @return 是否同时满足
     */
	static bool MatchesMovementCondition(EWuwaMovementActionCondition Condition,
	                                     EMovementMode SnapshotMovementMode,
	                                     uint8 SnapshotCustomMovementMode,
	                                     bool bCurrentlyGrounded,
	                                     bool bCurrentlyFalling,
	                                     bool bCurrentlyGrappling);

	/**
     * 计算水平 RMS 的目标位置
     * @param StartLocation 起点
     * @param WorldDirection 冻结世界方向
     * @param Distance 水平距离
     * @param OutTargetLocation 接收目标位置
     * @return 是否得到有限结果
     */
	static bool ResolveRootMotionTarget(const FVector& StartLocation,
	                                    const FVector& WorldDirection,
	                                    float Distance,
	                                    FVector& OutTargetLocation);

	/**
     * 从 Definition 的数据绑定中解析事件响应
     * @param Definition 当前 Movement Definition
     * @param EventTag 已发生事件
     * @param OutResponse 接收响应策略
     * @return 是否命中唯一绑定
     */
	static bool ResolveEventResponse(const UWuwaMovementActionDefinition& Definition,
	                                 const FGameplayTag& EventTag,
	                                 EWuwaMovementActionEventResponse& OutResponse);

	/**
     * 判断朝向策略是否需要取得角色旋转所有权
     * @param FacingPolicy Definition 朝向策略
     * @return 是否需要覆盖
     */
	static bool RequiresFacingOverride(EWuwaActionFacingPolicy FacingPolicy);
};
