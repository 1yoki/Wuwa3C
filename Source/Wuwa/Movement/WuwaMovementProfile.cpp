#include "Movement/WuwaMovementProfile.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult UWuwaMovementProfile::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (Result != EDataValidationResult::Invalid)
	{
		Result = EDataValidationResult::Valid;
	}

	// 统一记录错误并将资产标记为无效。
	auto Check = [&Context, &Result](const bool bCondition, const TCHAR* Message)
	{
		if (!bCondition)
		{
			Context.AddError(FText::FromString(Message));
			Result = EDataValidationResult::Invalid;
		}
	};

	Check(FMath::IsFinite(WalkSpeed) && WalkSpeed > 0.f, TEXT("WalkSpeed 必须为大于 0 的有限值"));

	Check(FMath::IsFinite(RunSpeed) && RunSpeed >= WalkSpeed, TEXT("RunSpeed 必须为不小于 WalkSpeed 的有限值"));

	Check(FMath::IsFinite(SprintSpeed) && SprintSpeed >= RunSpeed, TEXT("SprintSpeed 必须为不小于 RunSpeed 的有限值"));

	Check(FMath::IsFinite(SprintRunDeceleration) && SprintRunDeceleration > 0.f,
	      TEXT("SprintRunDeceleration 必须为大于 0 的有限值"));

	Check(FMath::IsFinite(MaxAcceleration) && MaxAcceleration > 0.f, TEXT("MaxAcceleration 必须为大于 0 的有限值"));

	Check(FMath::IsFinite(BrakingDecelerationWalking) && BrakingDecelerationWalking > 0.f,
	      TEXT("BrakingDecelerationWalking 必须为大于 0 的有限值"));

	Check(FMath::IsFinite(BrakingDecelerationFalling) && BrakingDecelerationFalling > 0.f,
	      TEXT("BrakingDecelerationFalling 必须为大于 0 的有限值"));

	Check(FMath::IsFinite(GroundFriction) && GroundFriction >= 0.f, TEXT("GroundFriction 必须为非负有限值"));

	Check(FMath::IsFinite(BrakingFrictionFactor) && BrakingFrictionFactor >= 0.f,
	      TEXT("BrakingFrictionFactor 必须为非负有限值"));

	Check(!RotationRate.ContainsNaN() && RotationRate.Yaw >= 0.f, TEXT("RotationRate 必须为有限旋转且 Yaw 非负"));

	Check(FMath::IsFinite(AirControl) && AirControl >= 0.f && AirControl <= 1.f,
	      TEXT("AirControl 必须为 [0, 1] 内的有限值"));

	Check(FMath::IsFinite(AnalogRunThreshold) && AnalogRunThreshold >= 0.f && AnalogRunThreshold <= 1.f,
	      TEXT("AnalogRunThreshold 必须为 [0, 1] 内的有限值"));

	// 普通跳跃必须提供有效的向上速度。
	Check(FMath::IsFinite(JumpZVelocity) && JumpZVelocity > 0.f, TEXT("JumpZVelocity 必须为大于 0 的有限值"));

	// 二段跳必须提供有效的向上速度。
	Check(FMath::IsFinite(DoubleJumpZVelocity) && DoubleJumpZVelocity > 0.f,
	      TEXT("DoubleJumpZVelocity 必须为大于 0 的有限值"));

	// 水平推进速度允许为零，但不能为负数。
	Check(FMath::IsFinite(DoubleJumpForwardSpeed) && DoubleJumpForwardSpeed >= 0.f,
	      TEXT("DoubleJumpForwardSpeed 必须为非负有限值"));

	Check(FMath::IsFinite(BackflipZVelocity) && BackflipZVelocity > 0.f,
	      TEXT("BackflipZVelocity 必须为大于 0 的有限值"));

	Check(FMath::IsFinite(BackflipBackwardSpeed) && BackflipBackwardSpeed > 0.f,
	      TEXT("BackflipBackwardSpeed 必须为大于 0 的有限值"));

	// 至少需要允许一次普通跳跃。
	Check(MaxJumpCount >= 1, TEXT("MaxJumpCount 必须大于等于 1"));

	// 零表示关闭土狼时间。
	Check(FMath::IsFinite(CoyoteTime) && CoyoteTime >= 0.f, TEXT("CoyoteTime 必须为非负有限值"));

	// 零表示关闭跳跃缓存。
	Check(FMath::IsFinite(JumpBufferTime) && JumpBufferTime >= 0.f, TEXT("JumpBufferTime 必须为非负有限值"));

	// 重力倍率必须保持为正数。
	Check(FMath::IsFinite(GravityScale) && GravityScale > 0.f, TEXT("GravityScale 必须为大于 0 的有限值"));

	Check(FMath::IsFinite(HeavyLandingVelocityThreshold) && HeavyLandingVelocityThreshold > 0.f,
	      TEXT("HeavyLandingVelocityThreshold 必须为大于 0 的有限值"));

	return Result;
}

#endif
