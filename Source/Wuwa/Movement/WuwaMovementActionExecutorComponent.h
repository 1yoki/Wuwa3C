#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Components/ActorComponent.h"
#include "Actions/WuwaActionExecutor.h"
#include "Movement/WuwaMovementTypes.h"
#include "WuwaMovementActionExecutorComponent.generated.h"

class AWuwaCharacter;
class UWuwaActionDefinition;
class UWuwaActionRouterComponent;
class UWuwaCharacterMovementComponent;

class UAnimInstance;
class UAnimMontage;

UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaMovementActionExecutorComponent : public UActorComponent, public IWuwaActionExecutor
{
    GENERATED_BODY()

public:
    UWuwaMovementActionExecutorComponent();

    // Character作为Composition Root 显示注入依赖
    bool Initialize(AWuwaCharacter *InCharacter, UWuwaCharacterMovementComponent *InMovementComponent, UWuwaActionRouterComponent *InRouterComponent);

    bool IsInitialized() const;

    virtual bool SupportsAction(const UWuwaActionDefinition &Definition) const override;

    virtual bool CanStartAction(const FWuwaActionRequest &Request, EWuwaActionRejectionReason &OutReason) const override;

    virtual bool StartAction(const FWuwaActionRequest &Request) override;

    virtual void EndAction(const FGameplayTag &ActionTag, EWuwaActionEndReason EndReason) override;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<AWuwaCharacter> CharacterOwner;

    UPROPERTY(Transient)
    TObjectPtr<UWuwaCharacterMovementComponent> MovementComponent;

    UPROPERTY(Transient)
    TObjectPtr<UWuwaActionRouterComponent> ActionRouter;

    /**
     * Executor 启动 Montage 后，结束或失败时必须知道自己创建了哪些资源
     */

    // 判断标签是否属于本 Executor 的动作范围
    bool IsSupportedActionTag(const FGameplayTag &ActionTag) const;

    bool IsAirDoubleJumpActionTag(const FGameplayTag &ActionTag) const;

    bool IsGroundRootMotionActionTag(const FGameplayTag &ActionTag) const;

    // 验证地面 RMS 动作的接地事实和方向快照。
    bool CanStartAirDoubleJumpAction(const FWuwaActionRequest &Request, EWuwaActionRejectionReason &OutReason) const;

    bool CanStartGroundRootMotionAction(const FWuwaActionRequest &Request, EWuwaActionRejectionReason &OutReason) const;

    // 从当前Character Mesh 获取 AnimInstance
    UAnimInstance *ResolveAnimInstance() const;

    // 清空记录
    void ClearActiveRuntime();

    // 按固定顺序释放 Executor 持有的全部动作资源；不负责释放 Router 的 Granted Tags
    void ReleaseActiveExecutionResources();

    bool HasActiveExecution() const;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Action|Animation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "s"))
    float InterruptedMontageBlendOutTime = 0.1f;

    UPROPERTY(Transient)
    TObjectPtr<UAnimMontage> ActiveMontage;

    UPROPERTY(Transient)
    TObjectPtr<UAnimInstance> ActiveAnimInstance;

    UPROPERTY(Transient)
    FGameplayTag ActiveActionTag;

    // 只移除本 Executor 注册的 MovementMode 事件订阅
    FDelegateHandle MovementModeChangedDelegateHandle;

    // ApplyRootMotionSource 返回的本地唯一 ID, 0 对应 ERootMotionSourceID::Invalid。
    uint16 ActiveRootMotionSourceId = 0;

    // Ground Action 因离地或 Dash Handoff 退出时保留当前速度；释放 RMS 后必须立即清除。
    bool bPreserveGroundActionVelocityOnRelease = false;

private:
    // 把正常或中断的 Montage End 转换为 Router End Reason
    void HandleMontageEnded(UAnimMontage *Montage, bool bInterrupted);

    // Dash Montage 的专用中段事件；Notify 只报告时间，Gameplay 条件仍由 Executor 校验。
    UFUNCTION()
    void HandleMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload &BranchingPointPayload);

    // 真实落地发生时中断尚未结束的二段跳 Action
    UFUNCTION()
    void HandleLandedEvent(const FWuwaLandingEvent &LandingEvent);

    // 地面动作执行期间消费已经发生的离地事实
    void HandleMovementModeChangedEvent(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode, EMovementMode NewMovementMode, uint8 NewCustomMode);

    // 为当前 Montage 实例绑定结束委托和专用 Notify 监听。
    void BindActiveMontageDelegates();

    // 停止 Montage 前解除全部委托，防止递归 Finish 或旧 Notify 污染新动作。
    void UnbindActiveMontageDelegates();

    // 动作执行期间保存并覆盖角色旋转策略，阻止移动方向或 Controller 改写角色朝向
    void AcquireFacingRotationOverride(const FGameplayTag &ActionTag);

    // 恢复动作开始前保存的真实旋转策略
    void ReleaseFacingRotationOverride();

    // 根据地面动作 Context 和 Definition 创建唯一 RMS
    bool ApplyGroundRootMotionSource(const FWuwaActionRequest &Request);

    // 只移除本 Executor 当前持有的RMS ID
    void ReleaseActiveRootMotionSource();

    bool HasActiveRootMotionSource() const;

    bool bHasFacingRotationOverride = false;

    bool bSavedOrientRotationToMovement = false;

    bool bSavedUseControllerDesiredRotation = false;

    bool bSavedUseControllerRotationYaw = false;

    // 记录当前由朝向覆盖保护的动作，仅用于资源归属和日志验证
    FGameplayTag FacingOverrideActionTag;

    // 仅用于验证动作期间角色 Yaw 是否保持稳定
    float FacingOverrideStartYaw = 0.f;
};
