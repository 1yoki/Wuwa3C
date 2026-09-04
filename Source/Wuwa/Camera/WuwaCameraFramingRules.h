#pragma once

#include "CoreMinimal.h"

/*
 * 单轴镜头构图修正输入。
 * 这是不依赖 World、Camera 或 Targeting 的纯规则数据，不持有运行时状态。
 */
struct WUWA_API FWuwaCameraAngleCorrectionInput
{
	float CurrentAngleDegrees = 0.0f;
	float DesiredAngleDegrees = 0.0f;
	float DeadZoneHalfAngleDegrees = 0.0f;
	float MaxCorrectionSpeedDegrees = 0.0f;
	float DeltaTime = 0.0f;
	float AuthorityAlpha = 1.0f;
};

namespace WuwaCameraFramingRules
{
/*
     * 计算玩家指向目标的水平战斗轴 Yaw。
     *
     * LockOn 的相机轨道必须以该轴为权威，不能用 Camera->Target 方向反推轨道：
     * CameraLocation 本身由轨道 Yaw 决定，反向使用它会形成自反馈，并允许镜头在
     * 敌人一侧看回玩家的错误稳定解。
     */
WUWA_API bool
CalculateHorizontalCombatAxisYaw(const FVector& PlayerPoint, const FVector& TargetPoint, float& OutYawDegrees);

/*
     * 返回 Camera 相对玩家在战斗轴上的有符号投影。
     * 小于等于 0 表示 Camera 位于玩家背向目标的一侧；大于 0 表示 Camera 已越过
     * 玩家平面、进入目标侧，需要退出普通死区并立即恢复正确轨道半球。
     */
WUWA_API bool CalculateCameraCombatAxisProjection(const FVector& PlayerPoint,
                                                  const FVector& TargetPoint,
                                                  const FVector& CameraPoint,
                                                  float& OutProjectionDistance);

/*
     * 目标位于安全角内时保持当前角度。
     * 超出安全角时只修正超出部分，并限制本帧最大角度变化。
     * 输入非法时返回 false，并把输出归零，避免调用方复用旧结果。
     */
WUWA_API bool CalculateBoundedAngleCorrection(const FWuwaCameraAngleCorrectionInput& Input,
                                              float& OutCorrectedAngleDegrees);
}
