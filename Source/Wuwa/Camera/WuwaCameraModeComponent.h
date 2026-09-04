#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Camera/WuwaCameraModeStack.h"
#include "Camera/Feedback/WuwaCameraFeedbackStack.h"
#include "Targeting/WuwaTargetingTypes.h"

#include "WuwaCameraModeComponent.generated.h"

class AActor;

class UCameraComponent;
class USpringArmComponent;
class APlayerController;

class UWuwaCameraProfile;
class UWuwaTargetingComponent;

/*
 * Character 所有的 Camera Mode 策略入口。
 * 组件只消费 Targeting 事实，不搜索目标，也不修改锁定或角色朝向。
 */
UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaCameraModeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWuwaCameraModeComponent();

	/*
     * 由 Character Composition Root 显式装配。
     * Profile 被 Stack 快照；Targeting 只作为弱只读事实来源。
     */

	bool Initialize(const UWuwaCameraProfile* InProfile,
	                UWuwaTargetingComponent* InTargetingComponent,
	                USpringArmComponent* InCameraBoom,
	                UCameraComponent* InFollowCamera);

	UFUNCTION(BlueprintPure, Category = "Wuwa|Camera")
	bool IsInitialized() const;

	UFUNCTION(BlueprintPure, Category = "Wuwa|Camera")
	FGameplayTag GetActiveModeTag() const;

	/*
     * 返回 Camera 当前是否持有 ControlRotation 写权限。
     * 该事实直接来自胜出的 Camera Mode，不额外保存第二份状态。
     */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Camera")
	bool HasViewRotationAuthority() const;

	// 返回当前连续 Rig State 副本，外部不能修改 Camera 运行态。
	UFUNCTION(BlueprintPure, Category = "Wuwa|Camera")
	FWuwaCameraRigState GetCurrentRigState() const
	{
		return CurrentRigState;
	}

	UFUNCTION(BlueprintPure, Category = "Wuwa|Camera")
	int32 GetStoredRequestCount() const;

	// 只读返回 Stack 内的配置快照，不暴露可变 Stack。
	const FWuwaCameraModeConfig* GetActiveModeConfig() const;

	/**
     * 取得不改变 Camera Mode 的短时反馈
     * @param Spec	各镜头通道的反馈配置
     * @param SourceObject	反馈来源弱引用
     * @return 成功时返回唯一 Handle
     */
	FWuwaCameraFeedbackRequestHandle AcquireCameraFeedback(const FWuwaCameraFeedbackSpec& Spec, UObject* SourceObject);

	/**
     * 让指定反馈进入 Released Tail
     * @param Handle	目标反馈 Handle
     * @return 是否找到目标反馈
     */
	bool BeginReleaseCameraFeedback(const FWuwaCameraFeedbackRequestHandle& Handle);

	/**
     * 立即移除指定反馈
     * @param Handle	目标反馈 Handle
     * @return 是否移除目标反馈
     */
	bool ForceReleaseCameraFeedback(const FWuwaCameraFeedbackRequestHandle& Handle);

	/**
     * 立即移除来源持有的全部反馈
     * @param SourceObject	目标来源
     * @return 移除数量
     */
	int32 ForceReleaseCameraFeedbackBySource(const UObject* SourceObject);

	/** @return 当前反馈栈条目数量 */
	int32 GetCameraFeedbackCount() const
	{
		return FeedbackStack.GetEntryCount();
	}

	/** @return 最近一次分别混合后的反馈通道 */
	const FWuwaCameraFeedbackOutput& GetCameraFeedbackOutput() const
	{
		return FeedbackStack.GetOutput();
	}

	bool RequestExplorationRecenter();

	bool CancelExplorationRecenter();

protected:
	virtual void
	TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()

	void HandleTargetContextChanged(const FWuwaTargetContext& TargetContext);

	/**
     * 响应 Targeting 已完成的即时命令事实
     * @param Fact Targeting 命令结果
     */
	void HandleTargetingCommandFact(const FWuwaTargetingCommandFact& Fact);

	// 把只读 Context 对称转换为内部 LockOn Handle。
	bool ReconcileTargetContext(const FWuwaTargetContext& TargetContext);

	// 只生成模式基础参数；LockOn 目标构图随后单独叠加。
	FWuwaCameraRigState BuildBaseRigState(const FWuwaCameraModeConfig& ModeConfig) const;

	bool ComposeLockOnRigState(const FWuwaCameraModeConfig& ModeConfig,
	                           const FWuwaTargetContext& TargetContext,
	                           FWuwaCameraRigState& InOutRigState) const;

	APlayerController* GetLocalViewController() const;
	bool CalculateDesiredLockOnControlRotation(const FWuwaTargetContext& TargetContext,
	                                           FRotator& OutControlRotation) const;

	bool ApplyLockOnControlRotation(const FWuwaCameraModeConfig& ModeConfig,
	                                const FWuwaTargetContext& TargetContext,
	                                float DeltaTime,
	                                float AuthorityAlpha) const;

	// 模式改变时始终从当前已应用状态重新起步，避免快速切换回跳。
	void BeginRigTransition(float BlendDuration);

	bool UpdateAndApplyRig(float DeltaTime, const FWuwaTargetContext& TargetContext);

	bool ApplyRigState(const FWuwaCameraRigState& RigState) const;

	static FWuwaCameraRigState
	InterpolateRigState(const FWuwaCameraRigState& From, const FWuwaCameraRigState& To, float Alpha);

	/*
     * 重新初始化和 EndPlay 共用同一清理入口。
     * Targeting EndPlay 不保证广播最终 None，因此 Camera 必须独立解绑。
     */
	void ResetRuntimeState();

	FWuwaCameraModeStack ModeStack;

	/** 短时镜头反馈的唯一生命周期与混合状态 */
	FWuwaCameraFeedbackStack FeedbackStack;

	UPROPERTY(Transient)
	TWeakObjectPtr<UWuwaTargetingComponent> TargetingComponent;

	/*
     * Character 当前相机 Rig 的弱引用。
     * Mode Component 不延长组件生命周期，Owner EndPlay 时独立清理。
     */

	UPROPERTY(Transient)
	TWeakObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(Transient)
	TWeakObjectPtr<UCameraComponent> FollowCamera;

	// 初始化前的真实相机参数，用作连续 Blend 起点。
	UPROPERTY(Transient)
	FWuwaCameraRigState InitialRigState;

	// 固定默认 Exploration Pitch功能：本地 Camera 初始化时捕获的默认 Exploration Control Pitch。
	UPROPERTY(Transient)
	float DefaultExplorationControlPitch = 0.0f;
	bool bHasDefaultExplorationControlPitch = false;

	// 将由唯一插值位置持续更新该状态。
	UPROPERTY(Transient)
	FWuwaCameraRigState CurrentRigState;

	// 当前 Blend 的固定起点；模式中途变化时会重新捕获 CurrentRigState。
	UPROPERTY(Transient)
	FWuwaCameraRigState TransitionStartRigState;

	// 进入 LockOn 或切换目标时捕获当前真实画面旋转。
	// UPROPERTY(Transient)
	// FRotator TransitionStartControlRotation = FRotator::ZeroRotator;

	// bool bHasTransitionStartControlRotation = false;

	UPROPERTY(Transient)
	FGameplayTag AppliedModeTag;

	/*
     * 只用于判断 Hard Target Actor 是否发生切换。
     * 弱引用不延长目标生命周期；目标销毁仍由 Targeting 权威清理。
     */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CompositionTargetActor;

	float TransitionElapsed = 0.f;
	float TransitionDuration = 0.f;
	bool bRigTransitionActive = false;

	/** 反馈位置滞后的世界空间跟随点 */
	FVector FeedbackLaggedOwnerLocation = FVector::ZeroVector;

	/** 是否已经捕获反馈位置滞后起点 */
	bool bHasFeedbackLaggedOwnerLocation = false;

	// 只代表 Camera 自己从 Stack 取得的一次 LockOn 请求。
	UPROPERTY(Transient)
	FWuwaCameraModeRequestHandle LockOnRequestHandle;

	// 每帧更新一次 Exploration 回正。
	// 返回 false 表示运行时数据异常。
	bool UpdateExplorationRecenter(float DeltaTime);

	// 停止回正并清理一次性状态。
	void ResetExplorationRecenter();

	/*
     * 无候选Lock视角回正
     */
	UPROPERTY(Transient)
	FRotator ExplorationRecenterStartRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	FRotator ExplorationRecenterTargetRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	float ExplorationRecenterElapsed = 0.0f;

	UPROPERTY(Transient)
	float ExplorationRecenterDuration = 0.0f;

	UPROPERTY(Transient)
	bool bExplorationRecenterActive = false;

	bool bInitialized = false;
};
