#include "Movement/WuwaMovementActionExecutorComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/RootMotionSource.h"

#include "Actions/WuwaActionDefinition.h"
#include "Actions/WuwaActionRouterComponent.h"
#include "Core/WuwaGameplayTags.h"
#include "Movement/WuwaCharacterMovementComponent.h"
#include "WuwaCharacter.h"
#include "Wuwa.h"

namespace
{
    constexpr uint16 InvalidRootMotionSourceId = static_cast<uint16>(ERootMotionSourceID::Invalid);

    constexpr uint16 GroundActionRootMotionPriority = 1000;

    const FName DashSprintHandoffNotifyName(TEXT("DashSprintHandoff"));
}

UWuwaMovementActionExecutorComponent::UWuwaMovementActionExecutorComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UWuwaMovementActionExecutorComponent::IsInitialized() const
{
    return IsValid(CharacterOwner.Get()) && IsValid(MovementComponent.Get()) && IsValid(ActionRouter.Get());
}

bool UWuwaMovementActionExecutorComponent::Initialize(AWuwaCharacter *InCharacter, UWuwaCharacterMovementComponent *InMovementComponent, UWuwaActionRouterComponent *InActionRouter)
{

    if (IsValid(MovementComponent.Get()))
    {
        MovementComponent->OnLandedEvent.RemoveDynamic(this, &UWuwaMovementActionExecutorComponent::HandleLandedEvent);

        if (MovementModeChangedDelegateHandle.IsValid())
        {
            MovementComponent->OnWuwaMovementModeChanged.Remove(MovementModeChangedDelegateHandle);
        }
    }

    MovementModeChangedDelegateHandle.Reset();

    CharacterOwner = nullptr;
    MovementComponent = nullptr;
    ActionRouter = nullptr;

    if (!IsValid(InCharacter) || !IsValid(InMovementComponent) || !IsValid(InActionRouter) ||
        InCharacter != GetOwner() || InMovementComponent != InCharacter->GetWuwaMovementComponent() || InActionRouter != InCharacter->GetActionRouterComponent())
    {
        UE_LOG(LogWuwa, Error, TEXT("Movement Action Executor初始化失败，依赖不匹配。Owner=%s"),
               *GetNameSafe(GetOwner()));
        return false;
    }

    CharacterOwner = InCharacter;
    MovementComponent = InMovementComponent;
    ActionRouter = InActionRouter;

    // Executor 只订阅已经发生的落地事实,不参与 Movement Component 的落地判定
    MovementComponent->OnLandedEvent.AddDynamic(this, &UWuwaMovementActionExecutorComponent::HandleLandedEvent);

    // Executor 只观察真实 MovementMode 变化，不参与决定移动模式
    MovementModeChangedDelegateHandle = MovementComponent->OnWuwaMovementModeChanged.AddUObject(this, &UWuwaMovementActionExecutorComponent::HandleMovementModeChangedEvent);

    return true;
}

// 重新验证实际 MovementMode、共享预算、Definition 和方向关系
bool UWuwaMovementActionExecutorComponent::CanStartAction(const FWuwaActionRequest &Request, EWuwaActionRejectionReason &OutReason) const
{
    OutReason = EWuwaActionRejectionReason::None;

    UWuwaActionDefinition *Definition = Request.Definition.Get();

    // Definition、支持范围和 Montage 属于静态配置检查
    if (!IsValid(Definition) || !Definition->IsRuntimeValid() || !SupportsAction(*Definition) || !IsValid(Definition->Montage.Get()))
    {
        OutReason = EWuwaActionRejectionReason::InvalidDefinition;
        return false;
    }

    // 同一个 Executor 同时只持有一个动作实例
    if (!IsInitialized() || HasActiveExecution())
    {
        OutReason = EWuwaActionRejectionReason::InvalidContext;
        return false;
    }

    if (!IsValid(ResolveAnimInstance()))
    {
        OutReason = EWuwaActionRejectionReason::InvalidContext;
        return false;
    }

    if (IsAirDoubleJumpActionTag(Request.ActionTag))
    {
        if (Definition->MovementPolicy != EWuwaActionMovementPolicy::CharacterMovement)
        {
            OutReason = EWuwaActionRejectionReason::InvalidDefinition;
            return false;
        }
        return CanStartAirDoubleJumpAction(Request, OutReason);
    }

    if (IsGroundRootMotionActionTag(Request.ActionTag))
    {
        if (Definition->MovementPolicy != EWuwaActionMovementPolicy::RootMotionSource || !Definition->RootMotionSourceConfig.IsRuntimeValid())
        {
            OutReason = EWuwaActionRejectionReason::InvalidDefinition;
            return false;
        }
        return CanStartGroundRootMotionAction(Request, OutReason);
    }

    OutReason = EWuwaActionRejectionReason::InvalidDefinition;
    return false;
}

/**
 * CanStart
 * → Backflip：取得旋转覆盖
 * → Montage Play
 * → Movement Commit
 * → 绑定 Montage End 与 Handoff Notify
 */
bool UWuwaMovementActionExecutorComponent::StartAction(const FWuwaActionRequest &Request)
{
    EWuwaActionRejectionReason RejectionReason = EWuwaActionRejectionReason::None;

    // Router 准入和实际Start 之间仍重新检查一次
    // 防止外部同步回调改变移动事实
    if (!CanStartAction(Request, RejectionReason))
        return false;

    UWuwaActionDefinition *Definition = Request.Definition.Get();
    UAnimInstance *AnimInstance = ResolveAnimInstance();
    UAnimMontage *Montage = IsValid(Definition) ? Definition->Montage.Get() : nullptr;

    if (!IsValid(AnimInstance) || !IsValid(Montage) || !IsValid(Definition))
    {
        return false;
    }

    const bool bGroundRootMotionAction = IsGroundRootMotionActionTag(Request.ActionTag);

    const bool bBackflip = Request.ActionTag == WuwaGameplayTags::Action_Movement_DoubleJump_Backflip;

    // 后空翻固定需要保持起跳朝向；地面动作是否保持朝向由 Definition 配置决定
    const bool bShouldPreserveFacing = bBackflip || (bGroundRootMotionAction && Definition->RootMotionSourceConfig.bPreserveFacing);

    float MontagePlayRate = 1.f;

    if (bGroundRootMotionAction)
    {
        const float DesiredDuration = Definition->RootMotionSourceConfig.Duration;
        const float MontageLength = Montage->GetPlayLength();

        if (!FMath::IsFinite(DesiredDuration) || !FMath::IsFinite(MontageLength) || DesiredDuration <= 0.f || MontageLength <= 0.f)
        {
            return false;
        }

        // RMS Duration 是地面动作的时间权威, 调整 Montage 播放速率，使动画主体和 Capsule 位移同步结束
        MontagePlayRate = MontageLength / DesiredDuration;

        if (!FMath::IsFinite(MontagePlayRate) || MontagePlayRate <= 0.f)
        {
            return false;
        }
    }

    // 新动作即将取得自己的移动资源，必须立即结束可能仍存在的 Dash 出口减速状态。
    MovementComponent->ExitSprintRun();

    if (bShouldPreserveFacing)
    {
        // 在 Montage 和位移资源启动前锁住旋转策略，避免首帧后移速度触发角色转身
        AcquireFacingRotationOverride(Request.ActionTag);
    }

    // 先确认表现资源可以播放，再提交 Gameplay 位移。
    // Montage只负责表现与时间边界
    const float PlayedDuration = AnimInstance->Montage_Play(Montage, MontagePlayRate, EMontagePlayReturnType::MontageLength, 0.f, false);

    if (PlayedDuration <= 0.f)
    {
        UE_LOG(LogWuwa, Warning, TEXT("Montage播放失败。Owner=%s Montage=%s"),
               *GetNameSafe(GetOwner()), *GetNameSafe(Montage));

        // Montage 播放失败时必须撤销已经获取的资源，Router 随后会按 Failed 释放 Granted Tags。
        ReleaseActiveExecutionResources();
        return false;
    }

    // Montage 成功后才记录本次 Executor 资源
    ActiveActionTag = Request.ActionTag;
    ActiveMontage = Montage;
    ActiveAnimInstance = AnimInstance;

    bool bMovementCommitted = false;

    if (Request.ActionTag == WuwaGameplayTags::Action_Movement_DoubleJump_Directional)
    {
        bMovementCommitted = MovementComponent->RequestDirectionalDoubleJump(Request.Context.WorldDirection);
    }
    else if (Request.ActionTag == WuwaGameplayTags::Action_Movement_DoubleJump_Backflip)
    {
        bMovementCommitted = MovementComponent->RequestBackflipDoubleJump(Request.Context.WorldDirection);
    }
    else if (bGroundRootMotionAction)
    {
        // 地面动作的唯一 Capsule 位移由 RMS 提供
        bMovementCommitted = ApplyGroundRootMotionSource(Request);
    }

    if (!bMovementCommitted)
    {
        // Montage 已播放但 Gameplay 位移提交失败，统一回滚全部 Executor 资源
        ReleaseActiveExecutionResources();

        UE_LOG(LogWuwa, Warning, TEXT("Movement提交失败。Owner=%s ActionTag=%s"),
               *GetNameSafe(CharacterOwner.Get()), *Request.ActionTag.ToString());

        return false;
    }

    // Montage 与 Movement/RMS 均成功后才允许正常结束回调。
    BindActiveMontageDelegates();

    return true;
}

void UWuwaMovementActionExecutorComponent::ReleaseActiveExecutionResources()
{
    UAnimInstance *AnimInstance = ActiveAnimInstance.Get();
    UAnimMontage *Montage = ActiveMontage.Get();

    // 必须先解除结束委托，避免 Montage_Stop 同步触发回调并再次请求 Router Finish
    UnbindActiveMontageDelegates();

    // Montage 是表现资源；动作提前结束时不能继续播放
    if (IsValid(AnimInstance) && IsValid(Montage) && AnimInstance->Montage_IsPlaying(Montage))
    {
        AnimInstance->Montage_Stop(InterruptedMontageBlendOutTime, Montage);
    }

    // RMS 是 Capsule 位移资源，必须在清空其唯一 ID 前按 ID 移除
    ReleaseActiveRootMotionSource();

    // 位移结束后恢复动作开始前保存的角色旋转策略
    ReleaseFacingRotationOverride();

    // 所有外部资源释放完成后，最后清空 Executor 的本地所有权记录
    ClearActiveRuntime();
}

/**
 * EndAction
 * → 解绑 Montage Delegate
 * → 停止仍在播放的 Montage
 * → 恢复旋转覆盖
 * → 清空 Executor Runtime
 * → 返回 Router 释放标签
 */
void UWuwaMovementActionExecutorComponent::EndAction(const FGameplayTag &ActionTag, EWuwaActionEndReason EndReason)
{
    (void)EndReason;

    if (!HasActiveExecution())
    {
        return;
    }

    if (ActionTag != ActiveActionTag)
    {
        UE_LOG(LogWuwa, Warning, TEXT("Executor 收到不匹配的结束请求。Active=%s Requested=%s"),
               *ActiveActionTag.ToString(), *ActionTag.ToString());

        // 过期结束请求不能释放当前新动作持有的资源
        return;
    }

    // EndAction 只对称释放 Executor 资源；Dash 是否衔接只允许由中段 Notify 决定。
    ReleaseActiveExecutionResources();
}

/**
 * EndPlay
 * → Router OwnerDestroyed 清理
 * → 解除 Landing Delegate
 * → 本地兜底
 * → 清空依赖
 */
void UWuwaMovementActionExecutorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UWuwaActionRouterComponent *Router = ActionRouter.Get();

    if (IsValid(Router) && Router->HasActiveAction() && IsSupportedActionTag(Router->GetCurrentActionTag()))
    {
        // Owner 销毁必须先进入 Router 的统一结束路径,不能直接丢弃 Executor Runtime。
        Router->FinishCurrent(EWuwaActionEndReason::OwnerDestroyed);
    }

    if (IsValid(Router))
    {
        Router->UnregisterExecutor(this);
    }

    // Router 已失效时仍做最后的本地兜底清理。
    if (HasActiveExecution())
    {
        EndAction(ActiveActionTag, EWuwaActionEndReason::OwnerDestroyed);
    }

    if (IsValid(MovementComponent.Get()))
    {
        // 销毁前解除所有 Movement 事件订阅。
        MovementComponent->OnLandedEvent.RemoveDynamic(this, &UWuwaMovementActionExecutorComponent::HandleLandedEvent);

        if (MovementModeChangedDelegateHandle.IsValid())
        {
            MovementComponent->OnWuwaMovementModeChanged.Remove(MovementModeChangedDelegateHandle);
        }
    }

    MovementModeChangedDelegateHandle.Reset();

    CharacterOwner = nullptr;
    MovementComponent = nullptr;
    ActionRouter = nullptr;

    Super::EndPlay(EndPlayReason);
}

bool UWuwaMovementActionExecutorComponent::IsAirDoubleJumpActionTag(const FGameplayTag &ActionTag) const
{
    return ActionTag == WuwaGameplayTags::Action_Movement_DoubleJump_Directional ||
           ActionTag == WuwaGameplayTags::Action_Movement_DoubleJump_Backflip;
}

bool UWuwaMovementActionExecutorComponent::IsGroundRootMotionActionTag(const FGameplayTag &ActionTag) const
{
    return ActionTag == WuwaGameplayTags::Action_Movement_Dash_Forward ||
           ActionTag == WuwaGameplayTags::Action_Movement_Backstep;
}

bool UWuwaMovementActionExecutorComponent::IsSupportedActionTag(const FGameplayTag &ActionTag) const
{
    return IsAirDoubleJumpActionTag(ActionTag) ||
           IsGroundRootMotionActionTag(ActionTag);
}

bool UWuwaMovementActionExecutorComponent::SupportsAction(const UWuwaActionDefinition &Definition) const
{
    // 只认明确列出的动作标签，不能抢占其他 Executor 的动作
    return IsSupportedActionTag(Definition.ActionTag);
}

bool UWuwaMovementActionExecutorComponent::CanStartAirDoubleJumpAction(const FWuwaActionRequest &Request, EWuwaActionRejectionReason &OutReason) const
{
    const FWuwaActionContext &Context = Request.Context;

    // Source 快照、MovementMode 和当前物理状态必须同时成立，不能只信任可能经过 Buffer 等待的旧 Context。
    if (Context.SourceObject.Get() != CharacterOwner.Get() || Context.MovementMode != MOVE_Falling || !MovementComponent->IsFalling() || !MovementComponent->CanPerformAirDoubleJump())
    {
        OutReason = EWuwaActionRejectionReason::InvalidContext;
        return false;
    }

    const FVector FacingDirection = Context.FacingDirection.GetSafeNormal2D();
    const FVector WorldDirection = Context.WorldDirection.GetSafeNormal2D();

    if (FacingDirection.IsNearlyZero() || WorldDirection.IsNearlyZero() || Context.FacingDirection.ContainsNaN() || Context.WorldDirection.ContainsNaN() || Context.InputDirection.ContainsNaN())
    {
        OutReason = EWuwaActionRejectionReason::InvalidContext;
        return false;
    }

    const bool bDirectional = Request.ActionTag == WuwaGameplayTags::Action_Movement_DoubleJump_Directional;
    const bool bBackflip = Request.ActionTag == WuwaGameplayTags::Action_Movement_DoubleJump_Backflip;
    const double DirectionDot = FVector::DotProduct(FacingDirection, WorldDirection);

    const bool bDirectionalContextValid = bDirectional && !Context.InputDirection.IsNearlyZero() && DirectionDot >= 0.99f;
    const bool bBackflipContextValid = bBackflip && Context.InputDirection.IsNearlyZero() && DirectionDot <= -0.99f;

    if (!bDirectionalContextValid && !bBackflipContextValid)
    {
        OutReason = EWuwaActionRejectionReason::InvalidContext;
        return false;
    }

    return true;
}

bool UWuwaMovementActionExecutorComponent::CanStartGroundRootMotionAction(const FWuwaActionRequest &Request, EWuwaActionRejectionReason &OutReason) const
{
    const FWuwaActionContext &Context = Request.Context;

    const bool bContextWasGrounded = Context.MovementMode == MOVE_Walking || Context.MovementMode == MOVE_NavWalking;

    // 请求产生时和 Executor 真正准入时都必须接地，Buffer 等待期间如果已经离地，旧请求不能继续启动
    if (Context.SourceObject.Get() != CharacterOwner.Get() || !bContextWasGrounded || !MovementComponent->IsMovingOnGround())
    {
        OutReason = EWuwaActionRejectionReason::InvalidContext;
        return false;
    }

    const FVector WorldDirection = Context.WorldDirection.GetSafeNormal2D();
    const FVector FacingDirection = Context.FacingDirection.GetSafeNormal2D();

    if (WorldDirection.IsNearlyZero() || FacingDirection.IsNearlyZero() || Context.WorldDirection.ContainsNaN() || Context.FacingDirection.ContainsNaN() || Context.InputDirection.ContainsNaN())
    {
        OutReason = EWuwaActionRejectionReason::InvalidContext;
        return false;
    }

    const bool bGroundDash = Request.ActionTag == WuwaGameplayTags::Action_Movement_Dash_Forward;
    const bool bGroundBackstep = Request.ActionTag == WuwaGameplayTags::Action_Movement_Backstep;
    const double DirectionDot = FVector::DotProduct(FacingDirection, WorldDirection);

    const bool bGroundDashContextValid = bGroundDash && !Context.InputDirection.IsNearlyZero() && DirectionDot >= 0.99f;
    const bool bGroundBackstepContextValid = bGroundBackstep && Context.InputDirection.IsNearlyZero() && DirectionDot <= -0.99f;

    if (!bGroundDashContextValid && !bGroundBackstepContextValid)
    {
        OutReason = EWuwaActionRejectionReason::InvalidContext;
        return false;
    }

    return true;
}

UAnimInstance *UWuwaMovementActionExecutorComponent::ResolveAnimInstance() const
{
    if (!IsValid(CharacterOwner.Get()) || !IsValid(CharacterOwner->GetMesh()))
    {
        return nullptr;
    }

    return CharacterOwner->GetMesh()->GetAnimInstance();
}

bool UWuwaMovementActionExecutorComponent::ApplyGroundRootMotionSource(const FWuwaActionRequest &Request)
{
    if (!IsInitialized() || HasActiveRootMotionSource())
    {
        return false;
    }

    UWuwaActionDefinition *Definition = Request.Definition.Get();

    if (!IsValid(Definition) || Definition->MovementPolicy != EWuwaActionMovementPolicy::RootMotionSource || !Definition->RootMotionSourceConfig.IsRuntimeValid())
    {
        return false;
    }

    const bool bGroundDash = Request.ActionTag == WuwaGameplayTags::Action_Movement_Dash_Forward;
    const bool bGroundBackstep = Request.ActionTag == WuwaGameplayTags::Action_Movement_Backstep;
    if (!bGroundDash && !bGroundBackstep)
    {
        return false;
    }

    const FVector WorldDirection = Request.Context.WorldDirection.GetSafeNormal2D();
    if (WorldDirection.IsNearlyZero() || Request.Context.WorldDirection.IsNearlyZero())
    {
        return false;
    }

    const FWuwaRootMotionSourceConfig &Config = Definition->RootMotionSourceConfig;

    const FVector StartLocation = CharacterOwner->GetActorLocation();

    const FVector TargetLocation = StartLocation + WorldDirection * Config.Distance;

    if (StartLocation.ContainsNaN() || TargetLocation.ContainsNaN())
    {
        return false;
    }

    TSharedPtr<FRootMotionSource_MoveToDynamicForce> RootMotionSource = MakeShared<FRootMotionSource_MoveToDynamicForce>();

    // Sequence 保证每次输入动作都有独立名字；
    RootMotionSource->InstanceName = FName(*FString::Printf(TEXT("WuwaGroundAction_%u"), Request.SourceInputSequence));

    RootMotionSource->Priority = GroundActionRootMotionPriority;
    RootMotionSource->AccumulateMode = ERootMotionAccumulateMode::Override;
    RootMotionSource->Duration = Config.Duration;
    RootMotionSource->StartLocation = StartLocation;
    RootMotionSource->InitialTargetLocation = TargetLocation;
    RootMotionSource->TargetLocation = TargetLocation;
    RootMotionSource->bRestrictSpeedToExpected = true;
    RootMotionSource->TimeMappingCurve = Config.TimeMappingCurve.Get();

    // 地面动作只控制水平移动，不能用 RMS 覆盖垂直物理, 后续离开地面时仍由 CharacterMovement 处理 Falling。
    RootMotionSource->Settings.SetFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate);

    // 默认结束清除 RMS 速度；离地和 Dash Handoff 会在释放前临时改为保留出口速度。
    RootMotionSource->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
    RootMotionSource->FinishVelocityParams.SetVelocity = FVector::ZeroVector;

    const uint16 NewRootMotionSourceId = MovementComponent->ApplyRootMotionSource(RootMotionSource);

    if (NewRootMotionSourceId == InvalidRootMotionSourceId)
    {
        return false;
    }

    // 只有 Apply 成功后才取得 RMS 资源所有权
    ActiveRootMotionSourceId = NewRootMotionSourceId;

    return true;
}

bool UWuwaMovementActionExecutorComponent::HasActiveRootMotionSource() const
{
    return ActiveRootMotionSourceId != InvalidRootMotionSourceId;
}

void UWuwaMovementActionExecutorComponent::ReleaseActiveRootMotionSource()
{
    // 先消费一次性策略，保证任何提前返回都不会污染下一次动作
    const bool bShouldPreserveVelocity = bPreserveGroundActionVelocityOnRelease;

    bPreserveGroundActionVelocityOnRelease = false;

    if (!HasActiveRootMotionSource())
    {
        return;
    }

    const uint16 RootMotionSourceIdToRemove = ActiveRootMotionSourceId;

    // 先清空本地所有权，防止同步回调重复移除同一个 RMS
    ActiveRootMotionSourceId = InvalidRootMotionSourceId;

    UWuwaCharacterMovementComponent *Movement = MovementComponent.Get();

    if (!IsValid(Movement))
    {
        return;
    }

    if (bShouldPreserveVelocity)
    {
        TSharedPtr<FRootMotionSource> RootMotionSource = Movement->GetRootMotionSourceByID(
            RootMotionSourceIdToRemove);

        if (RootMotionSource.IsValid())
        {
            // 离地时只停止 RMS，不把当前水平惯性或主动跳跃的 Z 速度清零
            RootMotionSource->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity;
        }

    }

    // 无论是否保留速度，都必须按当前 Executor 持有的 ID 移除 RMS。
    Movement->RemoveRootMotionSourceByID(RootMotionSourceIdToRemove);
}

bool UWuwaMovementActionExecutorComponent::HasActiveExecution() const
{
    // 任一资源仍存在都表示上一次动作尚未彻底清理
    return ActiveActionTag.IsValid() || IsValid(ActiveMontage.Get()) || IsValid(ActiveAnimInstance.Get()) ||
           HasActiveRootMotionSource() || bHasFacingRotationOverride;
}

void UWuwaMovementActionExecutorComponent::ClearActiveRuntime()
{
    // 外部资源必须先完成清理，再清空本地记录
    ActiveActionTag = FGameplayTag();
    ActiveMontage = nullptr;
    ActiveAnimInstance = nullptr;
}

void UWuwaMovementActionExecutorComponent::HandleLandedEvent(const FWuwaLandingEvent &LandingEvent)
{
    (void)LandingEvent;

    if (!HasActiveExecution())
    {
        return;
    }

    UWuwaActionRouterComponent *Router = ActionRouter.Get();

    if (!IsValid(Router) || !Router->HasActiveAction() || Router->GetCurrentActionTag() != ActiveActionTag)
    {
        return;
    }

    // 提前落地表示空中动作被物理事实中断，不能继续等待 Montage 原定结束时间。
    Router->FinishCurrent(EWuwaActionEndReason::Interrupted);
}

void UWuwaMovementActionExecutorComponent::HandleMovementModeChangedEvent(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode, EMovementMode NewMovementMode, uint8 NewCustomMode)
{
    const bool bWasGrounded = PreviousMovementMode == MOVE_Walking || PreviousMovementMode == MOVE_NavWalking;

    // 目前 Walking 与 NavWalking 之间切换不能误判为离地。
    if (!bWasGrounded || NewMovementMode != MOVE_Falling)
    {
        return;
    }

    if (!HasActiveExecution() || !IsGroundRootMotionActionTag(ActiveActionTag))
    {
        return;
    }

    UWuwaActionRouterComponent *Router = ActionRouter.Get();

    if (!IsValid(Router) || !Router->HasActiveAction() || Router->GetCurrentActionTag() != ActiveActionTag)
    {
        return;
    }

    // 某些情况下防止RMS清理时吞掉速度
    bPreserveGroundActionVelocityOnRelease = true;

    const bool bFinished = Router->FinishCurrent(EWuwaActionEndReason::Interrupted);

    if (!bFinished)
    {
        // Router 没有接管清理时不能把策略泄漏给之后的结束请求
        bPreserveGroundActionVelocityOnRelease = false;
    }
}

// 二段跳后移时，角色朝向必须锁定在起跳时的朝向，直到落地或动作结束
void UWuwaMovementActionExecutorComponent::AcquireFacingRotationOverride(const FGameplayTag &ActionTag)
{
    if (bHasFacingRotationOverride || !ActionTag.IsValid() || !IsValid(MovementComponent.Get()) || !IsValid(CharacterOwner.Get()))
    {
        return;
    }

    // 保存动作开始前的真实策略，结束时不能恢复成硬编码默认值
    bSavedOrientRotationToMovement = MovementComponent->bOrientRotationToMovement;
    bSavedUseControllerDesiredRotation = MovementComponent->bUseControllerDesiredRotation;
    bSavedUseControllerRotationYaw = CharacterOwner->bUseControllerRotationYaw;

    FacingOverrideActionTag = ActionTag;
    FacingOverrideStartYaw = CharacterOwner->GetActorRotation().Yaw;

    // 后移速度不能驱动角色转身；鼠标仍可转动镜头,但 Controller Yaw 暂时不写回角色
    MovementComponent->bOrientRotationToMovement = false;
    MovementComponent->bUseControllerDesiredRotation = false;
    CharacterOwner->bUseControllerRotationYaw = false;

    bHasFacingRotationOverride = true;
}

void UWuwaMovementActionExecutorComponent::ReleaseFacingRotationOverride()
{
    if (!bHasFacingRotationOverride)
    {
        return;
    }

    UWuwaCharacterMovementComponent *Movement = MovementComponent.Get();
    AWuwaCharacter *Character = CharacterOwner.Get();

    const FGameplayTag ReleasedActionTag = FacingOverrideActionTag;
    const float StartYaw = FacingOverrideStartYaw;

    // 恢复动作开始前保存的旋转策略，不能写死为默认值。
    if (IsValid(Movement))
    {
        Movement->bOrientRotationToMovement = bSavedOrientRotationToMovement;
        Movement->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
    }

    if (IsValid(Character))
    {
        Character->bUseControllerRotationYaw = bSavedUseControllerRotationYaw;

        // 仅记录后空翻期间的朝向变化，不在结束时强制修正角色旋转。
        const float EndYaw = Character->GetActorRotation().Yaw;
        const float YawDelta = FMath::FindDeltaAngleDegrees(StartYaw, EndYaw);

        UE_LOG(
            LogWuwa,
            Verbose,
            TEXT("朝向旋转覆盖已恢复。Owner=%s StartYaw=%.2f EndYaw=%.2f Delta=%.2f"),
            *GetNameSafe(Character),
            StartYaw,
            EndYaw,
            YawDelta);
    }

    // 即使角色或移动组件已失效，也必须清除本地资源占用状态。
    bHasFacingRotationOverride = false;
    bSavedOrientRotationToMovement = false;
    bSavedUseControllerDesiredRotation = false;
    bSavedUseControllerRotationYaw = false;
    FacingOverrideActionTag = FGameplayTag();
    FacingOverrideStartYaw = 0.f;
}

/*
 * Montage End 和 Dash Handoff 都只能报告时间事实，Gameplay Tag 必须继续由 Router 统一释放：
 * 时间事件 → Executor 校验 → Router FinishCurrent → Executor EndAction → Router 释放 Granted Tags。
 */
void UWuwaMovementActionExecutorComponent::BindActiveMontageDelegates()
{
    if (!IsValid(ActiveAnimInstance.Get()) || !IsValid(ActiveMontage.Get()))
    {
        return;
    }

    FOnMontageEnded MontageEndDelegate;
    MontageEndDelegate.BindUObject(this, &UWuwaMovementActionExecutorComponent::HandleMontageEnded);

    // 委托绑定到本次明确的 Montage 实例,不能监听所有角色 Montage。
    ActiveAnimInstance->Montage_SetEndDelegate(MontageEndDelegate, ActiveMontage);

    // 只在本 Executor 持有活动 Montage 时监听 Montage Notify。
    ActiveAnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(
        this,
        &UWuwaMovementActionExecutorComponent::HandleMontageNotifyBegin);
}

void UWuwaMovementActionExecutorComponent::UnbindActiveMontageDelegates()
{
    if (!IsValid(ActiveAnimInstance.Get()))
    {
        return;
    }

    ActiveAnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(
        this,
        &UWuwaMovementActionExecutorComponent::HandleMontageNotifyBegin);

    if (!IsValid(ActiveMontage.Get()))
    {
        return;
    }

    FOnMontageEnded EmptyDelegate;

    // 停止 Montage 前先清空回调, 防止 EndAction → Montage_Stop → 再次 FinishCurrent。
    ActiveAnimInstance->Montage_SetEndDelegate(EmptyDelegate, ActiveMontage);
}

void UWuwaMovementActionExecutorComponent::HandleMontageNotifyBegin(
    const FName NotifyName,
    const FBranchingPointNotifyPayload &BranchingPointPayload)
{
    if (NotifyName != DashSprintHandoffNotifyName ||
        !HasActiveExecution() ||
        ActiveActionTag != WuwaGameplayTags::Action_Movement_Dash_Forward)
    {
        return;
    }

    AWuwaCharacter *Character = CharacterOwner.Get();
    UWuwaCharacterMovementComponent *Movement = MovementComponent.Get();
    UWuwaActionRouterComponent *Router = ActionRouter.Get();

    if (!IsValid(Character) || !IsValid(Character->GetMesh()) ||
        !IsValid(Movement) || !IsValid(Router) ||
        BranchingPointPayload.SkelMeshComponent != Character->GetMesh() ||
        BranchingPointPayload.SequenceAsset != ActiveMontage.Get())
    {
        return;
    }

    if (!Router->HasActiveAction() ||
        Router->GetCurrentActionTag() != WuwaGameplayTags::Action_Movement_Dash_Forward ||
        !Movement->IsMovingOnGround())
    {
        return;
    }

    const FVector2D MoveIntentSnapshot = Character->GetCurrentMoveIntent();

    if (MoveIntentSnapshot.ContainsNaN() || MoveIntentSnapshot.IsNearlyZero(0.1f))
    {
        // 到点时没有真实 WASD：不结束动作，Dash 继续播放剩余恢复段。
        return;
    }

    // Handoff 是 Dash 的设计内成功分支；清理 RMS 时保留此刻出口速度。
    bPreserveGroundActionVelocityOnRelease = true;

    if (!Router->FinishCurrent(EWuwaActionEndReason::Completed))
    {
        // Router 未接管清理时不能把一次性速度策略泄漏给后续动作。
        bPreserveGroundActionVelocityOnRelease = false;
        return;
    }

    // FinishCurrent 会同步尝试消费 Buffer；若已有新动作启动，它拥有更高优先级。
    if (Router->HasActiveAction())
    {
        return;
    }

    // Router 已释放 Block.Input.Move 后再次校验，避免同步回调改变接地或输入事实。
    if (!Movement->IsMovingOnGround() ||
        Character->GetCurrentMoveIntent().ContainsNaN() ||
        Character->GetCurrentMoveIntent().IsNearlyZero(0.1f))
    {
        return;
    }

    const bool bEnteredSprintRun = Movement->EnterSprintRun(Character->GetCurrentMoveIntent());

    UE_LOG(
        LogWuwa,
        Verbose,
        TEXT("Dash Handoff 已处理。Owner=%s EnteredSprintRun=%s HorizontalSpeed=%.2f"),
        *GetNameSafe(Character),
        bEnteredSprintRun ? TEXT("true") : TEXT("false"),
        Movement->Velocity.Size2D());
}

void UWuwaMovementActionExecutorComponent::HandleMontageEnded(UAnimMontage *Montage, bool bInterrupted)
{
    if (!HasActiveExecution() || Montage != ActiveMontage.Get())
    {
        return;
    }

    const FGameplayTag FinishedActionTag = ActiveActionTag;

    const EWuwaActionEndReason EndReason = bInterrupted ? EWuwaActionEndReason::Interrupted : EWuwaActionEndReason::Completed;

    UWuwaActionRouterComponent *Router = ActionRouter.Get();

    if (IsValid(Router) && Router->HasActiveAction() && Router->GetCurrentActionTag() == FinishedActionTag)
    {
        // Montage 正常结束或被中断时报告事实，由 Router 结束 Action 和释放标签
        Router->FinishCurrent(EndReason);
        return;
    }

    // Router 已先行失效时执行本地兜底，避免旋转覆盖残留。
    EndAction(FinishedActionTag, EndReason);
}
