#pragma once

#include "Animation/AnimMontage.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "WuwaActionPresentationTypes.generated.h"

/** Action Montage 的生命周期权威 */
UENUM(BlueprintType)
enum class EWuwaActionAnimationLifetimePolicy : uint8
{
	/** Montage 自然结束并发布动画完成事实 */
	MontageControlled,

	/** Action 结束时显式停止并淡出 Montage */
	ActionControlled
};

/** Action Montage 的通用只读表现配置 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaActionAnimationSpec
{
	GENERATED_BODY()

	/** Action 获准后播放的 Montage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Animation")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	/** 是否用 Action 提供的期望时长校正播放速率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Animation")
	bool bMatchActionDuration = false;

	/** 决定 Montage 自然结束还是由 Action 显式收口 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Animation")
	EWuwaActionAnimationLifetimePolicy LifetimePolicy = EWuwaActionAnimationLifetimePolicy::MontageControlled;

	/** 外部中断时使用的 Blend Out 时长 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Animation", meta = (ClampMin = "0.0", Units = "s"))
	float ActionEndBlendOutTime = 0.1f;

	/** @return 配置是否可以安全用于运行时 */
	bool IsRuntimeValid() const
	{
		if (Montage == nullptr || !FMath::IsFinite(ActionEndBlendOutTime) || ActionEndBlendOutTime < 0.f)
		{
			return false;
		}

		switch (LifetimePolicy)
		{
			case EWuwaActionAnimationLifetimePolicy::MontageControlled:
				return true;

			case EWuwaActionAnimationLifetimePolicy::ActionControlled:
				return !Montage->bEnableAutoBlendOut;

			default:
				return false;
		}
	}
};
