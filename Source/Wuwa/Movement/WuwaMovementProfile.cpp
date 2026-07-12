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

    Check(MaxAcceleration > 0.f, TEXT("MaxAcceleration 必须大于 0"));

    Check(BrakingDecelerationWalking > 0.f, TEXT("BrakingDecelerationWalking 必须大于 0"));

    Check(BrakingDecelerationFalling > 0.f, TEXT("BrakingDecelerationFalling 必须大于 0"));

    Check(GroundFriction >= 0.f, TEXT("GroundFriction 不能为负数"));

    Check(BrakingFrictionFactor >= 0.f, TEXT("BrakingFrictionFactor 不能为负数"));

    Check(RotationRate.Yaw >= 0.f, TEXT("RotationRate.Yaw 不能为负数"));

    Check(AirControl >= 0.f && AirControl <= 1.f, TEXT("AirControl 必须在 [0, 1] 范围内"));

    Check(AnalogRunThreshold >= 0.f && AnalogRunThreshold <= 1.f, TEXT("AnalogRunThreshold 必须在 [0, 1] 范围内"));

    return Result;
}

#endif