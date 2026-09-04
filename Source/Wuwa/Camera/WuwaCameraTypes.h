#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "WuwaCameraTypes.generated.h"

/*
 * 标识 Camera Stack 中的一次模式请求。
 * Handle 只标识释放权限；请求来源、优先级和生命周期仍由 Stack 持有。
 */

USTRUCT(BlueprintType)
struct WUWA_API FWuwaCameraModeRequestHandle
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Mode")
	FGuid Id;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Mode")
	FGameplayTag ModeTag;

	FWuwaCameraModeRequestHandle() = default;

	FWuwaCameraModeRequestHandle(const FGuid& InId, const FGameplayTag& InModeTag) : Id(InId), ModeTag(InModeTag) {}

	bool IsValid() const
	{
		return Id.IsValid() && ModeTag.IsValid();
	}

	void Reset()
	{
		Id.Invalidate();
		ModeTag = FGameplayTag();
	}
};

/*
 * Camera Mode Component 当前期望应用到相机 Rig 的参数。
 * 这不是最终 POV；SpringArm 碰撞与 APlayerCameraManager 仍在其后处理视图。
 */

USTRUCT(BlueprintType)
struct WUWA_API FWuwaCameraRigState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Rig")
	float FieldOfView = 90.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Rig")
	float TargetArmLength = 400.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Rig")
	FVector TargetOffset = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Rig")
	FVector SocketOffset = FVector::ZeroVector;

	bool IsFinite() const
	{
		const auto IsFiniteVector = [](const FVector& Value)
		{
			return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
		};

		return FMath::IsFinite(FieldOfView) && FMath::IsFinite(TargetArmLength) && IsFiniteVector(TargetOffset) &&
		       IsFiniteVector(SocketOffset);
	}
};

/*
 * 单个 Camera Mode 的资产配置。
 * Priority 和镜头参数只来自 Camera Profile，请求方不能在运行时覆盖。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCameraModeConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Mode", meta = (Categories = "Camera"))
	FGameplayTag ModeTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Mode", meta = (ClampMin = "0"))
	int32 Priority = 0;

	// CameraComponent 的 FieldOfView，单位度。
	UPROPERTY(EditAnywhere,
	          BlueprintReadOnly,
	          Category = "Camera Mode|Rig",
	          meta = (ClampMin = "5.0", ClampMax = "170.0", Units = "deg"))
	float FieldOfView = 90.f;

	// SpringArmComponent 的 TargetArmLength，碰撞后仍会缩短。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Mode|Rig", meta = (ClampMin = "1.0", Units = "cm"))
	float TargetArmLength = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Mode|Rig")
	FVector TargetOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Mode|Rig")
	FVector SocketOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Mode|Blend", meta = (ClampMin = "0.0", Units = "s"))
	float BlendInTime = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Mode|Blend", meta = (ClampMin = "0.0", Units = "s"))
	float BlendOutTime = 0.25f;

	// 仅 LockOn 使用；0 表示不向目标偏移 Pivot。
	UPROPERTY(EditAnywhere,
	          BlueprintReadOnly,
	          Category = "Camera Mode|LockOn",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TargetPivotWeight = 0.0f;

	// LockOn 模式下，Pivot 在目标方向上的最大偏移量；0 表示不偏移。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Mode|LockOn", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxTargetPivotOffset = 0.0f;

	// 玩家与目标距离乘以该比例后，加入基础臂长。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Mode|LockOn", meta = (ClampMin = "0.0"))
	float TargetDistanceArmScale = 0.0f;

	/*
     * LockOn 水平方向的安全区半角。
     * 目标位于当前朝向正负该角度内时，Camera 不主动修正 Yaw。
     */
	UPROPERTY(EditAnywhere,
	          BlueprintReadOnly,
	          Category = "Camera Mode|LockOn|Framing",
	          meta = (ClampMin = "0.0", ClampMax = "179.0", Units = "deg"))
	float LockOnYawDeadZoneHalfAngle = 0.0f;

	/*
     * LockOn 垂直方向的安全区半角。
     * 目标位于当前朝向正负该角度内时，Camera 不主动修正 Pitch。
     */
	UPROPERTY(EditAnywhere,
	          BlueprintReadOnly,
	          Category = "Camera Mode|LockOn|Framing",
	          meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
	float LockOnPitchDeadZoneHalfAngle = 0.0f;

	// LockOn 超出水平安全区后允许的最大 Yaw 修正速度。
	UPROPERTY(EditAnywhere,
	          BlueprintReadOnly,
	          Category = "Camera Mode|LockOn|Framing",
	          meta = (ClampMin = "0.0", Units = "deg/s"))
	float LockOnMaxYawCorrectionSpeed = 0.0f;

	// LockOn 超出垂直安全区后允许的最大 Pitch 修正速度。
	UPROPERTY(EditAnywhere,
	          BlueprintReadOnly,
	          Category = "Camera Mode|LockOn|Framing",
	          meta = (ClampMin = "0.0", Units = "deg/s"))
	float LockOnMaxPitchCorrectionSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Mode|LockOn", meta = (ClampMin = "1.0", Units = "cm"))
	float MinTargetArmLength = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Mode|LockOn", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxTargetArmLength = 800.f;

	bool IsRuntimeValid() const
	{
		const auto IsFiniteVector = [](const FVector& Value)
		{
			return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
		};

		return ModeTag.IsValid() && Priority >= 0 && FMath::IsFinite(FieldOfView) && FieldOfView >= 5.f &&
		       FieldOfView <= 170.f && FMath::IsFinite(TargetArmLength) && TargetArmLength > 0.f &&
		       IsFiniteVector(TargetOffset) && IsFiniteVector(SocketOffset) && FMath::IsFinite(BlendInTime) &&
		       BlendInTime >= 0.f && FMath::IsFinite(BlendOutTime) && BlendOutTime >= 0.f &&
		       FMath::IsFinite(TargetPivotWeight) && TargetPivotWeight >= 0.f && TargetPivotWeight <= 1.f &&
		       FMath::IsFinite(MaxTargetPivotOffset) && MaxTargetPivotOffset >= 0.f &&
		       FMath::IsFinite(TargetDistanceArmScale) && TargetDistanceArmScale >= 0.f &&
		       FMath::IsFinite(LockOnYawDeadZoneHalfAngle) && LockOnYawDeadZoneHalfAngle >= 0.0f &&
		       LockOnYawDeadZoneHalfAngle < 180.0f && FMath::IsFinite(LockOnPitchDeadZoneHalfAngle) &&
		       LockOnPitchDeadZoneHalfAngle >= 0.0f && LockOnPitchDeadZoneHalfAngle < 90.0f &&
		       FMath::IsFinite(LockOnMaxYawCorrectionSpeed) && LockOnMaxYawCorrectionSpeed >= 0.0f &&
		       FMath::IsFinite(LockOnMaxPitchCorrectionSpeed) && LockOnMaxPitchCorrectionSpeed >= 0.0f &&
		       FMath::IsFinite(MinTargetArmLength) && MinTargetArmLength > 0.f && FMath::IsFinite(MaxTargetArmLength) &&
		       MaxTargetArmLength >= MinTargetArmLength && TargetArmLength >= MinTargetArmLength &&
		       TargetArmLength <= MaxTargetArmLength;
	}
};
