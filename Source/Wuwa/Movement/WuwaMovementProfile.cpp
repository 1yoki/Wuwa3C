#include "Movement/WuwaMovementProfile.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult UWuwaMovementProfile::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    if (Result != EDataValidationResult::Invalid)
    {
        Result = EDataValidationResult::Valid;
    }

    // 统一记录错误并将资产标记为无效。
    auto Check = [&Context, &Result](const bool bCondition, const TCHAR *Message)
    {
        if (!bCondition)
        {
            Context.AddError(FText::FromString(Message));
            Result = EDataValidationResult::Invalid;
        }
    };

    Check(WalkSpeed > 0.f, TEXT("WalkSpeed 必须大于 0"));

    Check(RunSpeed >= WalkSpeed, TEXT("RunSpeed 不能小于 WalkSpeed"));

    Check(SprintSpeed >= RunSpeed, TEXT("SprintSpeed 不能小于 RunSpeed"));

    Check(SprintRunDeceleration > 0.f, TEXT("SprintRunDeceleration 必须大于 0"));

    Check(MaxAcceleration > 0.f, TEXT("MaxAcceleration 必须大于 0"));

    Check(BrakingDecelerationWalking > 0.f, TEXT("BrakingDecelerationWalking 必须大于 0"));

    Check(BrakingDecelerationFalling > 0.f, TEXT("BrakingDecelerationFalling 必须大于 0"));

    Check(GroundFriction >= 0.f, TEXT("GroundFriction 不能为负数"));

    Check(BrakingFrictionFactor >= 0.f, TEXT("BrakingFrictionFactor 不能为负数"));

    Check(RotationRate.Yaw >= 0.f, TEXT("RotationRate.Yaw 不能为负数"));

    Check(AirControl >= 0.f && AirControl <= 1.f, TEXT("AirControl 必须在 [0, 1] 范围内"));

    Check(AnalogRunThreshold >= 0.f && AnalogRunThreshold <= 1.f, TEXT("AnalogRunThreshold 必须在 [0, 1] 范围内"));

    // 普通跳跃必须提供有效的向上速度。
    Check(JumpZVelocity > 0.f, TEXT("JumpZVelocity 必须大于 0"));

    // 二段跳必须提供有效的向上速度。
    Check(DoubleJumpZVelocity > 0.f, TEXT("DoubleJumpZVelocity 必须大于 0"));

    // 水平推进速度允许为零，但不能为负数。
    Check(DoubleJumpForwardSpeed >= 0.f, TEXT("DoubleJumpForwardSpeed 不能为负数"));

    Check(BackflipZVelocity > 0.f, TEXT("BackflipZVelocity 必须大于 0"));

    Check(BackflipBackwardSpeed > 0.f, TEXT("BackflipBackwardSpeed 必须大于 0"));

    // 至少需要允许一次普通跳跃。
    Check(MaxJumpCount >= 1, TEXT("MaxJumpCount 必须大于等于 1"));

    // 零表示关闭土狼时间。
    Check(CoyoteTime >= 0.f, TEXT("CoyoteTime 不能为负数"));

    // 零表示关闭跳跃缓存。
    Check(JumpBufferTime >= 0.f, TEXT("JumpBufferTime 不能为负数"));

    // 重力倍率必须保持为正数。
    Check(GravityScale > 0.f, TEXT("GravityScale 必须大于 0"));

    Check(HeavyLandingVelocityThreshold > 0.f, TEXT("HeavyLandingVelocityThreshold 必须大于 0"));

    return Result;
}

#endif
