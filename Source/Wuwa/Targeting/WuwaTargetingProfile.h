#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "WuwaTargetingProfile.generated.h"

/** Targeting 查询、评分、保持和切换使用的只读配置。 */
UCLASS(BlueprintType)
class WUWA_API UWuwaTargetingProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UWuwaTargetingProfile();

	// Gather 只查询这些碰撞对象类型；默认包含 Pawn 与 WorldDynamic。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Gather")
	TArray<TEnumAsByte<EObjectTypeQuery>> CandidateObjectTypes;

	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Targeting|Gather",
	          meta = (ClampMin = "1.0", Units = "cm"))
	float SearchRadius = 2500.f;

	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Targeting|Filter",
	          meta = (ClampMin = "1.0", ClampMax = "180.0", Units = "deg"))
	float MaxAcquireAngleDegrees = 65.f;

	// Hard Lock 可以离开初始搜索范围，但超过该距离必须解除。
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Targeting|Filter",
	          meta = (ClampMin = "1.0", Units = "cm"))
	float HardLockReleaseDistance = 3200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Visibility")
	TEnumAsByte<ECollisionChannel> VisibilityTraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Targeting|Visibility",
	          meta = (ClampMin = "0.0", Units = "s"))
	float OcclusionGraceTime = 0.35f;

	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Targeting|Refresh",
	          meta = (ClampMin = "0.01", Units = "s"))
	float CandidateRefreshInterval = 0.10f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Score", meta = (ClampMin = "0.0"))
	float DistanceWeight = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Score", meta = (ClampMin = "0.0"))
	float ViewAlignmentWeight = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Score", meta = (ClampMin = "0.0"))
	float CurrentSoftTargetBonus = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Switch", meta = (ClampMin = "0.0"))
	float SwitchMinimumHorizontalDelta = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Switch", meta = (ClampMin = "0.0"))
	float SwitchVerticalPenaltyWeight = 0.50f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Switch", meta = (ClampMin = "0.0"))
	float SwitchDistancePenaltyWeight = 0.25f;

	// Component 初始化时调用；运行期间不应每帧重新读取并验证 Data Asset。
	bool IsRuntimeValid() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
