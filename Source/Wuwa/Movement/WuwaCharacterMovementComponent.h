#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Movement/WuwaMovementTypes.h"
#include "WuwaCharacterMovementComponent.generated.h"

class UWuwaMovementProfile;
class UWuwaStateTagComponent;

UCLASS()
class WUWA_API UWuwaCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()
public:
	UWuwaCharacterMovementComponent();

	// 设置统一状态标签组件。
	void SetStateTagComponent(UWuwaStateTagComponent *InStateTagComponent);

	// 复制 Profile 中的参数，不在Tick中每帧修改。
	bool ApplyMovementProfile(const UWuwaMovementProfile *Profile);

	// 接收持续移动输入。
	void SetLocomotionIntent(const FVector2D &MoveIntent);

	// 仅由 Day 5 前冲动作正常完成后调用。
	void EnterSprintRun();

	void ExitSprintRun();

	UFUNCTION(BlueprintPure, Category = "Wuwa|Movement")
	FWuwaLocomotionSnapshot GetLocomotionSnapshot() const;

	FORCEINLINE bool IsSprinting() const
	{
		return bIsSprinting;
	}

protected:
	virtual void BeginPlay() override;

	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool CanMaintainSprintRun() const;
	void RefreshLocomotionState();
	void SetSprinting(bool bNewSprinting);

	float ConfiguredWalkSpeed = 250.f;
	float ConfiguredRunSpeed = 500.f;
	float ConfiguredSprintSpeed = 700.f;
	float ConfiguredAnalogRunThreshold = 0.5f;

	float MoveInputMagnitude = 0.f;
	bool bIsSprinting = false;

	FGameplayTagContainer RuntimeSprintBlockedTags;

	UPROPERTY(Transient)
	TObjectPtr<UWuwaStateTagComponent> StateTagComponent;
};
