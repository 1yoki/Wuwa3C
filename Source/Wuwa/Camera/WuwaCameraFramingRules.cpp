#include "Camera/WuwaCameraFramingRules.h"

namespace
{
bool IsFiniteCameraFramingVector(const FVector& Value)
{
	return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
}

bool BuildHorizontalCombatAxis(const FVector& PlayerPoint, const FVector& TargetPoint, FVector& OutCombatAxis)
{
	OutCombatAxis = FVector::ZeroVector;

	if (!IsFiniteCameraFramingVector(PlayerPoint) || !IsFiniteCameraFramingVector(TargetPoint))
	{
		return false;
	}

	FVector PlayerToTarget = TargetPoint - PlayerPoint;
	PlayerToTarget.Z = 0.0;

	if (PlayerToTarget.IsNearlyZero())
	{
		return false;
	}

	OutCombatAxis = PlayerToTarget.GetSafeNormal2D();
	return !OutCombatAxis.IsNearlyZero() && IsFiniteCameraFramingVector(OutCombatAxis);
}
}

bool WuwaCameraFramingRules::CalculateHorizontalCombatAxisYaw(const FVector& PlayerPoint,
                                                              const FVector& TargetPoint,
                                                              float& OutYawDegrees)
{
	OutYawDegrees = 0.0f;

	FVector CombatAxis;
	if (!BuildHorizontalCombatAxis(PlayerPoint, TargetPoint, CombatAxis))
	{
		return false;
	}

	OutYawDegrees = FMath::UnwindDegrees(CombatAxis.Rotation().Yaw);
	return FMath::IsFinite(OutYawDegrees);
}

bool WuwaCameraFramingRules::CalculateCameraCombatAxisProjection(const FVector& PlayerPoint,
                                                                 const FVector& TargetPoint,
                                                                 const FVector& CameraPoint,
                                                                 float& OutProjectionDistance)
{
	OutProjectionDistance = 0.0f;

	FVector CombatAxis;
	if (!BuildHorizontalCombatAxis(PlayerPoint, TargetPoint, CombatAxis) || !IsFiniteCameraFramingVector(CameraPoint))
	{
		return false;
	}

	FVector PlayerToCamera = CameraPoint - PlayerPoint;
	PlayerToCamera.Z = 0.0;

	OutProjectionDistance = FVector::DotProduct(PlayerToCamera, CombatAxis);
	return FMath::IsFinite(OutProjectionDistance);
}

bool WuwaCameraFramingRules::CalculateBoundedAngleCorrection(const FWuwaCameraAngleCorrectionInput& Input,
                                                             float& OutCorrectedAngleDegrees)
{
	OutCorrectedAngleDegrees = 0.0f;

	const bool bHasFiniteInput =
	    FMath::IsFinite(Input.CurrentAngleDegrees) && FMath::IsFinite(Input.DesiredAngleDegrees) &&
	    FMath::IsFinite(Input.DeadZoneHalfAngleDegrees) && FMath::IsFinite(Input.MaxCorrectionSpeedDegrees) &&
	    FMath::IsFinite(Input.DeltaTime) && FMath::IsFinite(Input.AuthorityAlpha);

	const bool bHasValidRange = Input.DeadZoneHalfAngleDegrees >= 0.0f && Input.DeadZoneHalfAngleDegrees < 180.0f &&
	                            Input.MaxCorrectionSpeedDegrees > 0.0f && Input.DeltaTime >= 0.0f &&
	                            Input.AuthorityAlpha >= 0.0f && Input.AuthorityAlpha <= 1.0f;

	if (!bHasFiniteInput || !bHasValidRange)
	{
		return false;
	}

	const float CurrentAngle = FMath::UnwindDegrees(Input.CurrentAngleDegrees);

	const float AngleError = FMath::FindDeltaAngleDegrees(CurrentAngle, Input.DesiredAngleDegrees);

	const float ExcessAngle = FMath::Max(0.0f, FMath::Abs(AngleError) - Input.DeadZoneHalfAngleDegrees);

	const float MaxCorrectionStep = Input.MaxCorrectionSpeedDegrees * Input.DeltaTime * Input.AuthorityAlpha;

	if (!FMath::IsFinite(MaxCorrectionStep))
	{
		return false;
	}

	const float CorrectionStep = FMath::Sign(AngleError) * FMath::Min(ExcessAngle, MaxCorrectionStep);

	OutCorrectedAngleDegrees = FMath::UnwindDegrees(CurrentAngle + CorrectionStep);

	return FMath::IsFinite(OutCorrectedAngleDegrees);
}
