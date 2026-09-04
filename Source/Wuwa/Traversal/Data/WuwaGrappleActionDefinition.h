#pragma once

#include "CoreMinimal.h"
#include "Actions/Data/WuwaActionDefinition.h"
#include "Camera/Feedback/WuwaCameraFeedbackTypes.h"
#include "Traversal/Contracts/WuwaTraversalTypes.h"
#include "UObject/SoftObjectPtr.h"
#include "WuwaGrappleActionDefinition.generated.h"

class UNiagaraSystem;

/** 钩锁绳索表现的可选资源配置 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaGrapplePresentationSpec
{
	GENERATED_BODY()

	/** 绳索起点使用的角色 Mesh Socket */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Presentation")
	FName HandSocketName = NAME_None;

	/** 绳索使用的 Niagara System */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Presentation")
	TSoftObjectPtr<UNiagaraSystem> RopeSystem;

	/** Niagara 绳索起点向量参数 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Presentation")
	FName RopeStartParameterName = NAME_None;

	/** Niagara 绳索终点向量参数 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grapple|Presentation")
	FName RopeEndParameterName = NAME_None;

	/** 释放后的绳索淡出时长 */
	UPROPERTY(EditAnywhere,
	          BlueprintReadOnly,
	          Category = "Grapple|Presentation",
	          meta = (ClampMin = "0.0", Units = "s"))
	float RopeFadeOutDuration = 0.15f;

	/** @return 是否配置了任意绳索资源字段 */
	bool IsConfigured() const
	{
		return !HandSocketName.IsNone() || !RopeSystem.IsNull() || !RopeStartParameterName.IsNone() ||
		       !RopeEndParameterName.IsNone();
	}

	/** @return 未启用或完整配置时返回真 */
	bool IsRuntimeValid() const
	{
		if (!FMath::IsFinite(RopeFadeOutDuration) || RopeFadeOutDuration < 0.f)
		{
			return false;
		}

		return !IsConfigured() || (!HandSocketName.IsNone() && !RopeSystem.IsNull() &&
		                           !RopeStartParameterName.IsNone() && !RopeEndParameterName.IsNone());
	}
};

/** 聚合无锚点钩锁查询、移动和表现配置的 Action Definition */
UCLASS(BlueprintType)
class WUWA_API UWuwaGrappleActionDefinition : public UWuwaActionDefinition
{
	GENERATED_BODY()

public:
	/** 输入边沿执行的只读世界查询配置 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Query")
	FWuwaFreeGrappleQuerySpec QuerySpec;

	/** CharacterMovement 使用的冻结轨迹配置 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Movement")
	FWuwaGrappleMovementSpec MovementSpec;

	/** 通用 Animation Capability 使用的配置 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Animation")
	FWuwaActionAnimationSpec AnimationSpec;

	/** 可降级镜头消费者使用的配置 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Camera")
	FWuwaCameraFeedbackSpec CameraFeedbackSpec;

	/** 可降级绳索消费者使用的配置 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Presentation")
	FWuwaGrapplePresentationSpec PresentationSpec;

	//~ Begin UWuwaActionDefinition Interface
	virtual void GatherRequiredCapabilityTags(FGameplayTagContainer& OutCapabilityTags) const override;
	virtual const FWuwaActionAnimationSpec* GetAnimationSpec() const override;
	virtual float GetDesiredAnimationDuration() const override;
	virtual bool IsContextValid(const FWuwaActionContext& Context) const override;
	virtual bool IsRuntimeValid() const override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	//~ End UWuwaActionDefinition Interface
};
