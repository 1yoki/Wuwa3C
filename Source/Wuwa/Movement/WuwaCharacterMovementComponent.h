#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Movement/WuwaMovementTypes.h"
#include "WuwaCharacterMovementComponent.generated.h"

class UWuwaMovementProfile;
class UWuwaStateTagComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWuwaLandingEventSignature, const FWuwaLandingEvent &, LandingEvent);

UCLASS()
class WUWA_API UWuwaCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()
public:
	UWuwaCharacterMovementComponent();

	// 广播已经发生的着陆事件。
	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Movement")
	FWuwaLandingEventSignature OnLandedEvent;

	// 设置统一状态标签组件。
	void SetStateTagComponent(UWuwaStateTagComponent *InStateTagComponent);

	// 复制 Profile 中的参数，不在Tick中每帧修改。
	bool ApplyMovementProfile(const UWuwaMovementProfile *Profile);

	// 接收持续移动输入。
	void SetLocomotionIntent(const FVector2D &MoveIntent);

	// 请求普通跳跃、土狼跳跃或写入跳跃输入缓存。
	bool RequestJump();

	// 请求空中二段跳
	bool RequestAirSprint(const FVector &DesiredWorldDirection);

	// 返回只读空中动作状态
	const FWuwaAirActionState &GetAirActionState() const
	{
		return AirActionState;
	}

	// 返回最近一次落地事件的只读副本
	const FWuwaLandingEvent &GetLastLandingEvent() const
	{
		return LastLandingEvent;
	}

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

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void ProcessLanded(const FHitResult &Hit, float RemainingTime, int32 Iterations) override;

private:
	bool CanMaintainSprintRun() const;
	void SetSprinting(bool bNewSprinting);

	// 存储运行时跳跃数据。
	bool StartJump(float InitialZVelocity, EWuwaJumpType JumpType);
	bool IsWithCoyoteTime(double CurrentTime) const;
	void ExpireJumpBuffer(double CurrentTime);

	UPROPERTY(Transient)
	FWuwaAirActionState AirActionState;
	UPROPERTY(Transient)
	FWuwaLandingEvent LastLandingEvent;
	UPROPERTY(Transient)
	EWuwaJumpType LastJumpType = EWuwaJumpType::None;

	// 每次成功起跳递增
	int32 JumpSequence = 0;
	// 落地缓存将在移动更新结束后消费
	bool bPendingBufferedJump = false;

	double GetMovementTime() const;

	void RefreshLocomotionState();

	float ConfiguredWalkSpeed = 250.f;
	float ConfiguredRunSpeed = 500.f;
	float ConfiguredSprintSpeed = 700.f;
	float ConfiguredAnalogRunThreshold = 0.5f;

	// 保存二段跳的运行时向上速度。
	float ConfiguredDoubleJumpZVelocity = 650.f;
	// 保存二段跳的运行时水平速度。
	float ConfiguredDoubleJumpForwardSpeed = 400.f;
	// 保存每次滞空允许的最大跳跃次数。
	int32 ConfiguredMaxJumpCount = 2;
	// 保存土狼时间的运行时配置。
	float ConfiguredCoyoteTime = 0.15f;
	// 保存跳跃输入缓存的有效时间。
	float ConfiguredJumpBufferTime = 0.10f;
	// 未加载Profile时的默认值

	float MoveInputMagnitude = 0.f;
	bool bIsSprinting = false;

	FGameplayTagContainer RuntimeSprintBlockedTags;

	UPROPERTY(Transient)
	TObjectPtr<UWuwaStateTagComponent> StateTagComponent;
};
