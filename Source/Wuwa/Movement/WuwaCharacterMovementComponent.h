#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Movement/WuwaMovementTypes.h"
#include "WuwaCharacterMovementComponent.generated.h"

class UWuwaMovementProfile;
class UWuwaStateTagComponent;

// 广播已经发生的着陆事件。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWuwaLandingEventSignature, const FWuwaLandingEvent &, LandingEvent);

// 广播 Movement Component 已经完成的真实移动模式变化
DECLARE_MULTICAST_DELEGATE_FourParams(FWuwaMovementModeChangedSignature, EMovementMode, uint8, EMovementMode, uint8);

UCLASS()
class WUWA_API UWuwaCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()
public:
	UWuwaCharacterMovementComponent();

	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Movement")
	FWuwaLandingEventSignature OnLandedEvent;

	// 参数依次为 PreviousMovementMode、PreviousCustomMode、NewMovementMode、NewCustomMode
	// 这是 C++ 运行时事实事件，不允许订阅者反向决定 MovementMode
	FWuwaMovementModeChangedSignature OnWuwaMovementModeChanged;

	// 设置统一状态标签组件。
	void SetStateTagComponent(UWuwaStateTagComponent *InStateTagComponent);

	// 复制 Profile 中的参数，不在Tick中每帧修改。
	bool ApplyMovementProfile(const UWuwaMovementProfile *Profile);

	// 接收持续移动输入。
	void SetLocomotionIntent(const FVector2D &MoveIntent);

	// 请求普通跳跃、土狼跳跃或写入跳跃输入缓存。
	bool RequestJump();

	// 检查当前是否仍有一次空中 Sprint 二段跳预算，只供 Executor 做启动前检查，不消费预算
	bool CanPerformAirDoubleJump() const;

	// 沿触发瞬间的角色正前方执行定向二段跳
	bool RequestDirectionalDoubleJump(const FVector &ForwardDirectionSnapshot);

	// 沿触发瞬间的角色后方执行后空翻二段跳
	bool RequestBackflipDoubleJump(const FVector &BackwardDirectionSnapshot);

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

	// Dash Handoff 完成后，用该时刻的真实输入尝试进入短暂 Sprint Run。
	bool EnterSprintRun(const FVector2D &MoveIntent);

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

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	virtual void ProcessLanded(const FHitResult &Hit, float RemainingTime, int32 Iterations) override;

private:
	bool CanMaintainSprintRun() const;
	void UpdateSprintRun(float DeltaTime);
	void SetSprinting(bool bNewSprinting);

	// 存储运行时跳跃数据。
	bool StartJump(float InitialZVelocity, EWuwaJumpType JumpType);
	// 所有空中二段跳最终进入这里，保证速度和预算原子提交
	bool CommitAirDoubleJump(const FVector &WorldDirection, float HorizontalSpeed, float VerticalSpeed, EWuwaJumpType JumpType);
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
	float ConfiguredSprintRunDeceleration = 2000.f;
	float ConfiguredAnalogRunThreshold = 0.5f;

	// 保存二段跳的运行时向上速度。
	float ConfiguredDoubleJumpZVelocity = 650.f;
	// 保存二段跳的运行时水平速度。
	float ConfiguredDoubleJumpForwardSpeed = 400.f;
	// 保存后空翻运行时向上速度。
	float ConfiguredBackflipZVelocity = 650.f;
	// 保存后空翻运行时向后推进速度。
	float ConfiguredBackflipBackwardSpeed = 400.f;
	// 保存每次滞空允许的最大跳跃次数。
	int32 ConfiguredMaxJumpCount = 2;
	// 保存土狼时间的运行时配置。
	float ConfiguredCoyoteTime = 0.15f;
	// 保存跳跃输入缓存的有效时间。
	float ConfiguredJumpBufferTime = 0.10f;
	// 保存重落地向下冲击速度阈值，不在 Tick 中读取 Data Asset。
	float ConfiguredHeavyLandingVelocityThreshold = 900.f;

	float MoveInputMagnitude = 0.f;
	bool bIsSprinting = false;

	FGameplayTagContainer RuntimeSprintBlockedTags;

	UPROPERTY(Transient)
	TObjectPtr<UWuwaStateTagComponent> StateTagComponent;
};
