#include "Camera/WuwaCameraProfile.h"

#include "Core/WuwaGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
bool IsValidCameraCollisionChannel(const ECollisionChannel Channel)
{
	return Channel >= ECC_WorldStatic && Channel < ECC_MAX;
}
}

UWuwaCameraProfile::UWuwaCameraProfile()
{
	FWuwaCameraModeConfig Exploration;
	Exploration.ModeTag = WuwaGameplayTags::Camera_Exploration;
	Exploration.Priority = 0;
	Exploration.FieldOfView = 90.f;
	Exploration.TargetArmLength = 400.f;
	Exploration.TargetOffset = FVector(0.f, 0.f, 60.f);
	Exploration.SocketOffset = FVector(0.f, 45.f, 0.f);
	Exploration.BlendInTime = 0.25f;
	Exploration.BlendOutTime = 0.30f;

	// Exploration 不消费 Target Context，所有目标构图参数必须保持关闭。
	Exploration.TargetPivotWeight = 0.f;
	Exploration.MaxTargetPivotOffset = 0.f;
	Exploration.TargetDistanceArmScale = 0.f;
	Exploration.MinTargetArmLength = 100.f;
	Exploration.MaxTargetArmLength = 800.f;

	Exploration.LockOnYawDeadZoneHalfAngle = 0.0f;
	Exploration.LockOnPitchDeadZoneHalfAngle = 0.0f;
	Exploration.LockOnMaxYawCorrectionSpeed = 0.0f;
	Exploration.LockOnMaxPitchCorrectionSpeed = 0.0f;

	FWuwaCameraModeConfig LockOn;
	LockOn.ModeTag = WuwaGameplayTags::Camera_LockOn;
	LockOn.Priority = 100;
	LockOn.FieldOfView = 88.f;
	LockOn.TargetArmLength = 430.f;
	LockOn.TargetOffset = FVector(0.f, 0.f, 70.f);
	LockOn.SocketOffset = FVector(0.f, 35.f, 0.f);
	LockOn.BlendInTime = 0.25f;
	LockOn.BlendOutTime = 0.30f;

	// LockOn 每帧只读 Targeting 发布的 Hard Target Point。
	LockOn.TargetPivotWeight = 0.25f;
	LockOn.MaxTargetPivotOffset = 220.f;
	LockOn.TargetDistanceArmScale = 0.12f;
	LockOn.MinTargetArmLength = 380.f;
	LockOn.MaxTargetArmLength = 600.f;

	// 安全区域角度和恢复速度
	LockOn.LockOnYawDeadZoneHalfAngle = 15.0f;
	LockOn.LockOnPitchDeadZoneHalfAngle = 8.0f;
	LockOn.LockOnMaxYawCorrectionSpeed = 150.0f;
	LockOn.LockOnMaxPitchCorrectionSpeed = 100.0f;

	ModeConfigs.Reserve(2);
	ModeConfigs.Add(Exploration);
	ModeConfigs.Add(LockOn);
}

const FWuwaCameraModeConfig* UWuwaCameraProfile::FindModeConfig(const FGameplayTag& ModeTag) const
{
	if (!ModeTag.IsValid())
	{
		return nullptr;
	}

	return ModeConfigs.FindByPredicate(
	    [&ModeTag](const FWuwaCameraModeConfig& Config)
	    {
		    return Config.ModeTag == ModeTag;
	    });
}

bool UWuwaCameraProfile::IsRuntimeValid() const
{
	// 目前只有两个模式
	if (ModeConfigs.Num() != 2)
	{
		return false;
	}

	TSet<FGameplayTag> SeenModeTags;

	for (const FWuwaCameraModeConfig& Config : ModeConfigs)
	{
		if (!Config.IsRuntimeValid())
		{
			return false;
		}

		const bool bSupportedMode =
		    Config.ModeTag == WuwaGameplayTags::Camera_Exploration || Config.ModeTag == WuwaGameplayTags::Camera_LockOn;

		// 不允许重复的 ModeTag
		if (!bSupportedMode || SeenModeTags.Contains(Config.ModeTag))
		{
			return false;
		}

		SeenModeTags.Add(Config.ModeTag);
	}
	const FWuwaCameraModeConfig* Exploration = FindModeConfig(WuwaGameplayTags::Camera_Exploration);
	const FWuwaCameraModeConfig* LockOn = FindModeConfig(WuwaGameplayTags::Camera_LockOn);

	if (!Exploration || !LockOn)
	{
		return false;
	}

	// LockOn 必须覆盖无请求时的 Exploration Fallback。
	if (LockOn->Priority <= Exploration->Priority)
	{
		return false;
	}

	const bool bExplorationTargetCompositionDisabled = FMath::IsNearlyZero(Exploration->TargetPivotWeight) &&
	                                                   FMath::IsNearlyZero(Exploration->MaxTargetPivotOffset) &&
	                                                   FMath::IsNearlyZero(Exploration->TargetDistanceArmScale) &&
	                                                   FMath::IsNearlyZero(Exploration->LockOnYawDeadZoneHalfAngle) &&
	                                                   FMath::IsNearlyZero(Exploration->LockOnPitchDeadZoneHalfAngle) &&
	                                                   FMath::IsNearlyZero(Exploration->LockOnMaxYawCorrectionSpeed) &&
	                                                   FMath::IsNearlyZero(Exploration->LockOnMaxPitchCorrectionSpeed);

	const bool bLockOnTargetCompositionEnabled =
	    LockOn->TargetPivotWeight > 0.0f && LockOn->MaxTargetPivotOffset > 0.0f &&
	    LockOn->TargetDistanceArmScale > 0.0f && LockOn->LockOnYawDeadZoneHalfAngle > 0.0f &&
	    LockOn->LockOnPitchDeadZoneHalfAngle > 0.0f && LockOn->LockOnMaxYawCorrectionSpeed > 0.0f &&
	    LockOn->LockOnMaxPitchCorrectionSpeed > 0.0f;

	const ECollisionChannel CollisionChannel = ProbeChannel.GetValue();

	const bool bValidCollision = FMath::IsFinite(ProbeSize) && ProbeSize > 0.f &&
	                             IsValidCameraCollisionChannel(CollisionChannel) &&
	                             FMath::IsFinite(CollisionRecoveryInterpSpeed) && CollisionRecoveryInterpSpeed > 0.f;

	return bExplorationTargetCompositionDisabled && bLockOnTargetCompositionEnabled && bValidCollision;
}

#if WITH_EDITOR

EDataValidationResult UWuwaCameraProfile::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (Result != EDataValidationResult::Invalid)
	{
		Result = EDataValidationResult::Valid;
	}

	// 统一记录具体资产错误，但不在校验期间修正或重排配置。
	auto Check = [&Context, &Result](const bool bCondition, const FString& Message)
	{
		if (!bCondition)
		{
			Context.AddError(FText::FromString(Message));
			Result = EDataValidationResult::Invalid;
		}
	};

	Check(ModeConfigs.Num() == 2, TEXT("ModeConfigs 必须恰好包含 Exploration 和 LockOn 两项"));

	TSet<FGameplayTag> SeenModeTags;

	for (int32 Index = 0; Index < ModeConfigs.Num(); ++Index)
	{
		const FWuwaCameraModeConfig& Config = ModeConfigs[Index];

		const FString Prefix = FString::Printf(TEXT("ModeConfigs[%d]"), Index);

		Check(Config.ModeTag.IsValid(), Prefix + TEXT(".ModeTag 必须有效"));

		const bool bSupportedMode =
		    Config.ModeTag == WuwaGameplayTags::Camera_Exploration || Config.ModeTag == WuwaGameplayTags::Camera_LockOn;

		Check(bSupportedMode, Prefix + TEXT(".ModeTag 只能是 Camera.Exploration 或 Camera.LockOn"));

		if (Config.ModeTag.IsValid())
		{
			Check(!SeenModeTags.Contains(Config.ModeTag), Prefix + TEXT(".ModeTag 与另一项重复"));

			SeenModeTags.Add(Config.ModeTag);
		}

		Check(Config.Priority >= 0, Prefix + TEXT(".Priority 不能为负数"));

		Check(FMath::IsFinite(Config.FieldOfView) && Config.FieldOfView >= 5.f && Config.FieldOfView <= 170.f,
		      Prefix + TEXT(".FieldOfView 必须是 [5, 170] 内的有限值"));

		Check(FMath::IsFinite(Config.TargetArmLength) && Config.TargetArmLength > 0.f,
		      Prefix + TEXT(".TargetArmLength 必须是有限正数"));

		const bool bFiniteTargetOffset = FMath::IsFinite(Config.TargetOffset.X) &&
		                                 FMath::IsFinite(Config.TargetOffset.Y) &&
		                                 FMath::IsFinite(Config.TargetOffset.Z);

		Check(bFiniteTargetOffset, Prefix + TEXT(".TargetOffset 必须全部为有限值"));

		const bool bFiniteSocketOffset = FMath::IsFinite(Config.SocketOffset.X) &&
		                                 FMath::IsFinite(Config.SocketOffset.Y) &&
		                                 FMath::IsFinite(Config.SocketOffset.Z);

		Check(bFiniteSocketOffset, Prefix + TEXT(".SocketOffset 必须全部为有限值"));

		Check(FMath::IsFinite(Config.BlendInTime) && Config.BlendInTime >= 0.f,
		      Prefix + TEXT(".BlendInTime 必须是有限非负数"));

		Check(FMath::IsFinite(Config.BlendOutTime) && Config.BlendOutTime >= 0.f,
		      Prefix + TEXT(".BlendOutTime 必须是有限非负数"));

		Check(FMath::IsFinite(Config.TargetPivotWeight) && Config.TargetPivotWeight >= 0.f &&
		          Config.TargetPivotWeight <= 1.f,
		      Prefix + TEXT(".TargetPivotWeight 必须在 [0, 1] 范围内"));

		Check(FMath::IsFinite(Config.MaxTargetPivotOffset) && Config.MaxTargetPivotOffset >= 0.f,
		      Prefix + TEXT(".MaxTargetPivotOffset 必须是有限非负数"));

		Check(FMath::IsFinite(Config.TargetDistanceArmScale) && Config.TargetDistanceArmScale >= 0.f,
		      Prefix + TEXT(".TargetDistanceArmScale 必须是有限非负数"));

		Check(FMath::IsFinite(Config.LockOnYawDeadZoneHalfAngle) && Config.LockOnYawDeadZoneHalfAngle >= 0.0f &&
		          Config.LockOnYawDeadZoneHalfAngle < 180.0f,
		      Prefix + TEXT(".LockOnYawDeadZoneHalfAngle 必须位于 [0, 180) 度"));

		Check(FMath::IsFinite(Config.LockOnPitchDeadZoneHalfAngle) && Config.LockOnPitchDeadZoneHalfAngle >= 0.0f &&
		          Config.LockOnPitchDeadZoneHalfAngle < 90.0f,
		      Prefix + TEXT(".LockOnPitchDeadZoneHalfAngle 必须位于 [0, 90) 度"));

		Check(FMath::IsFinite(Config.LockOnMaxYawCorrectionSpeed) && Config.LockOnMaxYawCorrectionSpeed >= 0.0f,
		      Prefix + TEXT(".LockOnMaxYawCorrectionSpeed 必须是有限非负数"));

		Check(FMath::IsFinite(Config.LockOnMaxPitchCorrectionSpeed) && Config.LockOnMaxPitchCorrectionSpeed >= 0.0f,
		      Prefix + TEXT(".LockOnMaxPitchCorrectionSpeed 必须是有限非负数"));

		Check(FMath::IsFinite(Config.MinTargetArmLength) && Config.MinTargetArmLength > 0.f,
		      Prefix + TEXT(".MinTargetArmLength 必须是有限正数"));

		Check(FMath::IsFinite(Config.MaxTargetArmLength) && Config.MaxTargetArmLength >= Config.MinTargetArmLength,
		      Prefix + TEXT(".MaxTargetArmLength 必须是有限值且不能小于最小臂长"));

		Check(Config.TargetArmLength >= Config.MinTargetArmLength &&
		          Config.TargetArmLength <= Config.MaxTargetArmLength,
		      Prefix + TEXT(".TargetArmLength 必须位于 Min/Max TargetArmLength 范围内"));
	}

	const FWuwaCameraModeConfig* Exploration = FindModeConfig(WuwaGameplayTags::Camera_Exploration);
	const FWuwaCameraModeConfig* LockOn = FindModeConfig(WuwaGameplayTags::Camera_LockOn);

	Check(Exploration != nullptr, TEXT("ModeConfigs 缺少 Camera.Exploration"));

	Check(LockOn != nullptr, TEXT("ModeConfigs 缺少 Camera.LockOn"));

	if (Exploration != nullptr)
	{
		Check(FMath::IsNearlyZero(Exploration->TargetPivotWeight) &&
		          FMath::IsNearlyZero(Exploration->MaxTargetPivotOffset) &&
		          FMath::IsNearlyZero(Exploration->TargetDistanceArmScale) &&
		          FMath::IsNearlyZero(Exploration->LockOnYawDeadZoneHalfAngle) &&
		          FMath::IsNearlyZero(Exploration->LockOnPitchDeadZoneHalfAngle) &&
		          FMath::IsNearlyZero(Exploration->LockOnMaxYawCorrectionSpeed) &&
		          FMath::IsNearlyZero(Exploration->LockOnMaxPitchCorrectionSpeed),
		      TEXT("Exploration 不得启用 Target Context 或 LockOn 构图参数"));
	}

	if (LockOn != nullptr)
	{
		Check(LockOn->TargetPivotWeight > 0.f, TEXT("LockOn.TargetPivotWeight 必须大于 0"));

		Check(LockOn->MaxTargetPivotOffset > 0.f, TEXT("LockOn.MaxTargetPivotOffset 必须大于 0"));

		Check(LockOn->TargetDistanceArmScale > 0.f, TEXT("LockOn.TargetDistanceArmScale 必须大于 0"));
		Check(LockOn->LockOnYawDeadZoneHalfAngle > 0.0f, TEXT("LockOn.LockOnYawDeadZoneHalfAngle 必须大于 0"));

		Check(LockOn->LockOnPitchDeadZoneHalfAngle > 0.0f, TEXT("LockOn.LockOnPitchDeadZoneHalfAngle 必须大于 0"));

		Check(LockOn->LockOnMaxYawCorrectionSpeed > 0.0f, TEXT("LockOn.LockOnMaxYawCorrectionSpeed 必须大于 0"));

		Check(LockOn->LockOnMaxPitchCorrectionSpeed > 0.0f, TEXT("LockOn.LockOnMaxPitchCorrectionSpeed 必须大于 0"));
	}

	if (Exploration != nullptr && LockOn != nullptr)
	{
		Check(LockOn->Priority > Exploration->Priority, TEXT("LockOn.Priority 必须高于 Exploration.Priority"));
	}

	Check(FMath::IsFinite(ProbeSize) && ProbeSize > 0.f, TEXT("ProbeSize 必须是有限正数"));

	Check(IsValidCameraCollisionChannel(ProbeChannel.GetValue()), TEXT("ProbeChannel 必须是有效碰撞通道"));

	Check(FMath::IsFinite(CollisionRecoveryInterpSpeed) && CollisionRecoveryInterpSpeed > 0.f,
	      TEXT("CollisionRecoveryInterpSpeed 必须是有限正数"));

	return Result;
}

#endif
