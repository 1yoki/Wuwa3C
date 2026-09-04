#pragma once

#include "CoreMinimal.h"
#include "Actions/Data/WuwaActionDefinition.h"
#include "Movement/Actions/WuwaMovementActionTypes.h"
#include "WuwaMovementActionDefinition.generated.h"

/** 使用 CharacterMovement 原语执行的 Action Definition */
UCLASS(BlueprintType)
class WUWA_API UWuwaMovementActionDefinition : public UWuwaActionDefinition
{
	GENERATED_BODY()

public:
	/** Action Prepare 时必须满足的移动环境 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Movement")
	EWuwaMovementActionCondition MovementCondition = EWuwaMovementActionCondition::Grounded;

	/** Capsule 的唯一主位移驱动 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Movement")
	EWuwaMovementActionDriver MovementDriver = EWuwaMovementActionDriver::RootMotionSource;

	/** Action 活动期间的朝向策略 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Movement")
	EWuwaActionFacingPolicy FacingPolicy = EWuwaActionFacingPolicy::UseLocomotion;

	/** RMS Driver 的参数 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Action|Movement",
	          meta = (EditCondition = "MovementDriver == EWuwaMovementActionDriver::RootMotionSource"))
	FWuwaRootMotionSourceConfig RootMotionSourceConfig;

	/** AirJump Driver 的变体 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Action|Movement",
	          meta = (EditCondition = "MovementDriver == EWuwaMovementActionDriver::AirJump"))
	EWuwaAirJumpVariant AirJumpVariant = EWuwaAirJumpVariant::Directional;

	/** 独立 Animation Capability 使用的表现配置 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Animation")
	FWuwaActionAnimationSpec AnimationSpec;

	/** Animation/Gameplay Event 到 Movement 策略的映射 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Movement|Events")
	TArray<FWuwaMovementActionEventBinding> EventBindings;

	/** Action 正常出口后的可选 Locomotion 行为 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Movement|Exit")
	EWuwaMovementActionExitPolicy ExitPolicy = EWuwaMovementActionExitPolicy::None;

	/** Grounded Action 离地中断时是否保留当前水平速度 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Movement|Exit")
	bool bPreserveVelocityWhenLeavingGround = false;

	//~ Begin UWuwaActionDefinition Interface
	virtual void GatherRequiredCapabilityTags(FGameplayTagContainer& OutCapabilityTags) const override;
	virtual const FWuwaActionAnimationSpec* GetAnimationSpec() const override;
	virtual float GetDesiredAnimationDuration() const override;
	virtual bool IsRuntimeValid() const override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	//~ End UWuwaActionDefinition Interface
};
