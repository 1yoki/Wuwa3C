#pragma once

#include "CoreMinimal.h"
#include "WuwaCameraFeedbackTypes.generated.h"

class UCurveFloat;

/** 镜头反馈请求的唯一身份 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCameraFeedbackRequestHandle
{
	GENERATED_BODY()

	/** 当前反馈栈内唯一编号 */
	UPROPERTY(BlueprintReadOnly, Category = "Camera Feedback")
	int64 Value = 0;

	/** @return Handle 是否引用有效反馈 */
	bool IsValid() const
	{
		return Value > 0;
	}

	/** @return 是否引用同一次反馈 */
	bool operator==(const FWuwaCameraFeedbackRequestHandle& Other) const
	{
		return Value == Other.Value;
	}

	/** @return 是否引用不同反馈 */
	bool operator!=(const FWuwaCameraFeedbackRequestHandle& Other) const
	{
		return !(*this == Other);
	}

	/** 清空当前引用 */
	void Reset()
	{
		Value = 0;
	}
};

/** 一次镜头反馈请求的显式生命周期 */
UENUM(BlueprintType)
enum class EWuwaCameraFeedbackLifecycle : uint8
{
	/** 正在淡入或保持活动 */
	Active,

	/** Released 后短暂保持 */
	Holding,

	/** 正在独立淡出 */
	BlendingOut,

	/** 已从反馈栈移除 */
	Removed
};

/** 镜头反馈各通道的只读配置 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCameraFeedbackSpec
{
	GENERATED_BODY()

	/** 视野角偏移 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Feedback", meta = (Units = "deg"))
	float FOVOffset = 12.f;

	/** 镜头臂长偏移 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Feedback", meta = (Units = "cm"))
	float ArmLengthOffset = 160.f;

	/** 枢轴位置偏移 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Feedback", meta = (Units = "cm"))
	FVector PivotOffset = FVector(0.f, 0.f, 20.f);

	/** 位置跟随速度倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Feedback", meta = (ClampMin = "0.01"))
	float LocationLagSpeedMultiplier = 0.65f;

	/** 淡入时长 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Feedback", meta = (ClampMin = "0.0", Units = "s"))
	float BlendInTime = 0.1f;

	/** 释放后的保持时长 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Feedback", meta = (ClampMin = "0.0", Units = "s"))
	float HoldAfterRelease = 0.12f;

	/** 淡出时长 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Feedback", meta = (ClampMin = "0.0", Units = "s"))
	float BlendOutTime = 0.35f;

	/** 淡入曲线 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Feedback")
	TObjectPtr<UCurveFloat> BlendInCurve = nullptr;

	/** 淡出曲线 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Feedback")
	TObjectPtr<UCurveFloat> BlendOutCurve = nullptr;

	/** 多来源混合时的优先级 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Feedback")
	int32 Priority = 0;

	/** @return 配置是否满足运行时数值约束 */
	bool IsRuntimeValid() const
	{
		return FMath::IsFinite(FOVOffset) && FMath::IsFinite(ArmLengthOffset) && !PivotOffset.ContainsNaN() &&
		       FMath::IsFinite(LocationLagSpeedMultiplier) && LocationLagSpeedMultiplier > 0.f &&
		       FMath::IsFinite(BlendInTime) && BlendInTime >= 0.f && FMath::IsFinite(HoldAfterRelease) &&
		       HoldAfterRelease >= 0.f && FMath::IsFinite(BlendOutTime) && BlendOutTime >= 0.f &&
		       BlendInCurve != nullptr && BlendOutCurve != nullptr;
	}
};

/** 反馈栈分别混合后的各镜头通道 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCameraFeedbackOutput
{
	GENERATED_BODY()

	/** 叠加到基础视野角的偏移 */
	UPROPERTY(BlueprintReadOnly, Category = "Camera Feedback")
	float FOVOffset = 0.f;

	/** 叠加到基础镜头臂长的偏移 */
	UPROPERTY(BlueprintReadOnly, Category = "Camera Feedback")
	float ArmLengthOffset = 0.f;

	/** 叠加到基础枢轴位置的偏移 */
	UPROPERTY(BlueprintReadOnly, Category = "Camera Feedback")
	FVector PivotOffset = FVector::ZeroVector;

	/** 原生位置跟随速度倍率 */
	UPROPERTY(BlueprintReadOnly, Category = "Camera Feedback")
	float LocationLagSpeedMultiplier = 1.f;

	/** 位置跟随滞后的独立混合权重 */
	UPROPERTY(BlueprintReadOnly, Category = "Camera Feedback")
	float LocationLagAlpha = 0.f;

	/** 是否至少存在一个有效反馈来源 */
	UPROPERTY(BlueprintReadOnly, Category = "Camera Feedback")
	bool bHasFeedback = false;

	/** @return 所有通道是否满足运行时数值约束 */
	bool IsRuntimeValid() const
	{
		return FMath::IsFinite(FOVOffset) && FMath::IsFinite(ArmLengthOffset) && !PivotOffset.ContainsNaN() &&
		       FMath::IsFinite(LocationLagSpeedMultiplier) && LocationLagSpeedMultiplier > 0.f &&
		       FMath::IsFinite(LocationLagAlpha) && LocationLagAlpha >= 0.f && LocationLagAlpha <= 1.f;
	}
};

FORCEINLINE uint32 GetTypeHash(const FWuwaCameraFeedbackRequestHandle& Handle)
{
	return GetTypeHash(Handle.Value);
}
