#include "Camera/WuwaCameraModeComponent.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

#include "Camera/WuwaCameraFramingRules.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "Camera/WuwaCameraProfile.h"
#include "Core/WuwaGameplayTags.h"
#include "Targeting/WuwaTargetingComponent.h"
#include "Wuwa.h"

namespace
{
    bool IsUsableHardTargetContext(const FWuwaTargetContext &Context)
    {
        const FVector &TargetPoint = Context.TargetPoint;

        const bool bFiniteTargetPoint = FMath::IsFinite(TargetPoint.X) && FMath::IsFinite(TargetPoint.Y) && FMath::IsFinite(TargetPoint.Z);

        return Context.Mode == EWuwaTargetingMode::Hard &&
               Context.TargetActor.IsValid() &&
               bFiniteTargetPoint;
    }
}

UWuwaCameraModeComponent::UWuwaCameraModeComponent()
{
    // Tick 负责异常来源剪枝、事实复核和计算并应用 Desired Rig State。
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

bool UWuwaCameraModeComponent::Initialize(const UWuwaCameraProfile *InProfile, UWuwaTargetingComponent *InTargetingComponent, USpringArmComponent *InCameraBoom, UCameraComponent *InFollowCamera)
{
    ResetRuntimeState();

    if (!::IsValid(GetOwner()) ||
        !::IsValid(InTargetingComponent) ||
        InTargetingComponent->GetOwner() != GetOwner() ||
        !InTargetingComponent->IsInitialized() ||
        !::IsValid(InCameraBoom) ||
        InCameraBoom->GetOwner() != GetOwner() ||
        !::IsValid(InFollowCamera) ||
        InFollowCamera->GetOwner() != GetOwner() ||
        InFollowCamera->GetAttachParent() != InCameraBoom)
    {
        return false;
    }

    /*
     * Mode Component 将成为 FOV/Arm/Offset 的唯一插值位置。
     * SpringArm 原生位置和旋转 Lag 必须关闭，避免重复延迟。
     */
    if (InCameraBoom->bEnableCameraLag || InCameraBoom->bEnableCameraRotationLag)
    {
        UE_LOG(
            LogWuwa,
            Error,
            TEXT("Camera Rig 初始化失败：SpringArm Camera Lag 或 Rotation Lag 未关闭。Owner=%s"),
            *GetNameSafe(GetOwner()));

        return false;
    }

    FWuwaCameraRigState CapturedRigState;
    CapturedRigState.FieldOfView = InFollowCamera->FieldOfView;
    CapturedRigState.TargetArmLength = InCameraBoom->TargetArmLength;
    CapturedRigState.TargetOffset = InCameraBoom->TargetOffset;
    CapturedRigState.SocketOffset = InCameraBoom->SocketOffset;

    if (!CapturedRigState.IsFinite() ||
        CapturedRigState.FieldOfView < 5.f ||
        CapturedRigState.FieldOfView > 170.f ||
        CapturedRigState.TargetArmLength <= 0.f)
    {
        return false;
    }

    if (!ModeStack.Initialize(InProfile))
    {
        return false;
    }

    TargetingComponent = InTargetingComponent;
    CameraBoom = InCameraBoom;
    FollowCamera = InFollowCamera;
    InitialRigState = CapturedRigState;
    CurrentRigState = CapturedRigState;

    // 固定默认 Exploration Pitch功能： 只为本地视口捕获进入 PIE 时的默认 Exploration Pitch
    if (APlayerController *ViewController = GetLocalViewController())
    {
        const FRotator InitialControlRotation = ViewController->GetControlRotation();

        if (InitialControlRotation.ContainsNaN())
        {
            ResetRuntimeState();

            return false;
        }

        const float CapturedDefaultPitch = FMath::UnwindDegrees(InitialControlRotation.Pitch);
        if (!FMath::IsFinite(CapturedDefaultPitch))
        {
            ResetRuntimeState();

            return false;
        }

        DefaultExplorationControlPitch = CapturedDefaultPitch;
        bHasDefaultExplorationControlPitch = true;
    }

    /*
     * Mode Component 在 TG_PrePhysics 写入 Desired Rig；
     * SpringArm 的 TG_PostPhysics Tick 随后执行唯一原生 Probe。
     */
    InCameraBoom->AddTickPrerequisiteComponent(this);

    /*
     * 先绑定语义变化事件，再主动读取当前事实。
     * 这样 Camera 初始化晚于 Hard Lock 时也不会漏掉已有状态。
     */
    InTargetingComponent->OnTargetContextChanged.AddUniqueDynamic(this, &UWuwaCameraModeComponent::HandleTargetContextChanged);

    bInitialized = true;

    const FWuwaTargetContext InitialTargetContext = InTargetingComponent->GetTargetContext();
    if (!ReconcileTargetContext(InitialTargetContext))
    {
        /*
         * 初始 Hard Context 无法转换为 Camera 请求时，
         * 必须解除委托并清空 Camera 自己的运行态。
         * Camera 不能反向修改 Targeting 来修复初始化失败。
         */
        ResetRuntimeState();
        return false;
    }

    const FWuwaCameraModeConfig *InitialModeConfig = ModeStack.GetActiveModeConfig();

    if (InitialModeConfig == nullptr || !InitialModeConfig->IsRuntimeValid())
    {
        ResetRuntimeState();
        return false;
    }

    AppliedModeTag = ModeStack.GetActiveModeTag();

    if (AppliedModeTag == WuwaGameplayTags::Camera_LockOn && IsUsableHardTargetContext(InitialTargetContext))
    {
        CompositionTargetActor = InitialTargetContext.TargetActor;
    }
    else
    {
        CompositionTargetActor.Reset();
    }

    /*
     * 初次接管也从 Character 当前真实 Rig 开始，
     * 不从 Data Asset 参数直接跳变。
     */
    BeginRigTransition(InitialModeConfig->BlendInTime);

    SetComponentTickEnabled(true);
    return true;
}

bool UWuwaCameraModeComponent::IsInitialized() const
{
    const UWuwaTargetingComponent *Targeting =
        TargetingComponent.Get();

    return bInitialized &&
           ModeStack.IsInitialized() &&
           ::IsValid(Targeting) &&
           Targeting->IsInitialized() &&
           CameraBoom.IsValid() &&
           FollowCamera.IsValid();
}

FGameplayTag UWuwaCameraModeComponent::GetActiveModeTag() const
{
    return IsInitialized() ? ModeStack.GetActiveModeTag() : FGameplayTag();
}

bool UWuwaCameraModeComponent::HasViewRotationAuthority() const
{
    return IsInitialized() && ModeStack.GetActiveModeTag() == WuwaGameplayTags::Camera_LockOn;
}

int32 UWuwaCameraModeComponent::GetStoredRequestCount() const
{
    return ModeStack.GetStoredRequestCount();
}

const FWuwaCameraModeConfig *UWuwaCameraModeComponent::GetActiveModeConfig() const
{
    return IsInitialized() ? ModeStack.GetActiveModeConfig() : nullptr;
}

void UWuwaCameraModeComponent::HandleTargetContextChanged(const FWuwaTargetContext &TargetContext)
{
    if (!bInitialized)
    {
        return;
    }

    if (!ReconcileTargetContext(TargetContext))
    {
        /*
         * 请求提交异常时安全回退 Exploration。
         * Camera 不能通过清除 Hard Lock 来修复自己的内部失败。
         */
        ModeStack.ClearRequests();
        LockOnRequestHandle.Reset();

        UE_LOG(
            LogWuwa,
            Error,
            TEXT("Camera Mode 无法同步 Target Context，已回退 Exploration。Owner=%s"),
            *GetNameSafe(GetOwner()));
    }
}

bool UWuwaCameraModeComponent::ReconcileTargetContext(const FWuwaTargetContext &TargetContext)
{
    UWuwaTargetingComponent *Targeting = TargetingComponent.Get();

    if (!::IsValid(Targeting) || !ModeStack.IsInitialized())
    {
        return false;
    }

    if (IsUsableHardTargetContext(TargetContext))
    {
        const int32 ExistingSourceRequests = ModeStack.GetRequestCountForSource(Targeting);

        /*
         * Hard Target 切换仍然是同一个 LockOn 模式。
         * 已有内部请求时只更新 Targeting 事实，不重复 Acquire。
         */
        if (LockOnRequestHandle.IsValid() && ExistingSourceRequests == 1)
        {
            return true;
        }

        // 异常重复或外部统一清理后，从确定的干净状态重新取得。
        if (ExistingSourceRequests > 0)
        {
            ModeStack.ReleaseBySource(Targeting);
        }

        LockOnRequestHandle.Reset();
        LockOnRequestHandle = ModeStack.AcquireMode(WuwaGameplayTags::Camera_LockOn, Targeting);

        return LockOnRequestHandle.IsValid();
    }

    /*
     * Soft/None、目标销毁、资格失效、超距和遮挡超时都经过这里。
     * 正常路径精确释放；来源清理只作为异常残留的收尾。
     */
    if (LockOnRequestHandle.IsValid())
    {
        if (!ModeStack.ReleaseMode(LockOnRequestHandle))
        {
            LockOnRequestHandle.Reset();
        }
    }

    if (ModeStack.GetRequestCountForSource(Targeting) > 0)
    {
        ModeStack.ReleaseBySource(Targeting);
    }

    LockOnRequestHandle.Reset();

    return ModeStack.GetRequestCountForSource(Targeting) == 0;
}

void UWuwaCameraModeComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bInitialized)
    {
        return;
    }

    UWuwaTargetingComponent *Targeting = TargetingComponent.Get();

    if (!::IsValid(Targeting) || !Targeting->IsInitialized())
    {
        /*
         * 来源异常结束时不能等待 Targeting 再广播。
         * Camera 只清理自己的请求和订阅，不写回 Targeting。
         */
        ResetRuntimeState();
        return;
    }

    const int32 RemovedRequestCount = ModeStack.PruneInvalidSources();

    if (RemovedRequestCount > 0)
    {
        LockOnRequestHandle.Reset();
    }

    /*
     * 事件负责正常边沿，Tick 只复核当前只读事实。
     * 这也覆盖 Targeting 重新初始化时没有最终 None 广播的异常路径。
     */
    const FWuwaTargetContext TargetContext = Targeting->GetTargetContext();
    if (!ReconcileTargetContext(TargetContext))
    {
        UE_LOG(
            LogWuwa,
            Error,
            TEXT("Camera Mode Tick 无法同步 Target Context，停止当前 Camera Mode 生命周期。Owner=%s"),
            *GetNameSafe(GetOwner()));

        ResetRuntimeState();
        return;
    }

    if (!UpdateAndApplyRig(DeltaTime, TargetContext))
    {
        /*
         * Rig 配置或依赖异常时只终止 Camera 自己的生命周期，
         * 不反向清除 Targeting、Movement 或任何 Gameplay 状态。
         */
        UE_LOG(
            LogWuwa,
            Error,
            TEXT("Camera Rig 更新失败，已恢复初始化参数。Owner=%s"),
            *GetNameSafe(GetOwner()));

        ResetRuntimeState();
    }
    
    if (!UpdateExplorationRecenter(DeltaTime))
    {
        /*
         * 一次性回正失败释放本次回正状态。
         */
        UE_LOG(
            LogWuwa,
            Warning,
            TEXT("Exploration 视角回正失败，已取消。Owner=%s"),
            *GetNameSafe(GetOwner()));

        ResetExplorationRecenter();
    }
}

bool UWuwaCameraModeComponent::RequestExplorationRecenter()
{
    // 回正是 Exploration 的一次性视角命令，
    
    if (!IsInitialized() || ModeStack.GetActiveModeTag() != WuwaGameplayTags::Camera_Exploration)
    {
        return false;
    }
    APlayerController* ViewController = GetLocalViewController();

    const AActor* OwnerActor = GetOwner();

    const FWuwaCameraModeConfig* ExplorationConfig = ModeStack.FindModeConfig(WuwaGameplayTags::Camera_Exploration);

    if (ViewController == nullptr || !::IsValid(OwnerActor) || ExplorationConfig == nullptr ||
        !ExplorationConfig->IsRuntimeValid() || !bHasDefaultExplorationControlPitch || !FMath::IsFinite(DefaultExplorationControlPitch))
    {
        return false;
    }

    FRotator StartRotation = ViewController->GetControlRotation();

    const FRotator OwnerRotation = OwnerActor->GetActorRotation();

    if (StartRotation.ContainsNaN() || OwnerRotation.ContainsNaN())
    {
        return false;
    }

    /*
     * 默认 Exploration 视角：
     *
     * Pitch：Camera 初始化时捕获的默认 Exploration Pitch。
     * Yaw：角色当前正前方，使 SpringArm 位于角色正后方。
     * Roll：始终为 0。
     */
    FRotator TargetRotation(DefaultExplorationControlPitch, OwnerRotation.Yaw, 0.0f);

    StartRotation.Roll = 0.0f;
    StartRotation.Normalize();

    TargetRotation.Roll = 0.0f;
    TargetRotation.Normalize();

    if (StartRotation.ContainsNaN() || TargetRotation.ContainsNaN())
    {
        return false;
    }

    /*
     * 重复按键时从当前真实画面重新开始，
     */
    ExplorationRecenterStartRotation = StartRotation;

    ExplorationRecenterTargetRotation = TargetRotation;

    ExplorationRecenterElapsed = 0.0f;

    /*
     * 复用 Exploration BlendInTime , ExplorationRecenterTime。
     */
    ExplorationRecenterDuration =FMath::Max(0.0f, ExplorationConfig->BlendInTime);

    /*
     * 已经处于默认视角，或者配置为 0 秒时，直接提交结果，不额外占有视角控制权。
     */
    if (StartRotation.Equals(TargetRotation,0.1f) ||ExplorationRecenterDuration <= KINDA_SMALL_NUMBER)
    {
        ViewController->SetControlRotation(TargetRotation);

        ResetExplorationRecenter();
        return true;
    }

    bExplorationRecenterActive = true;
    return true;
}

bool UWuwaCameraModeComponent::UpdateExplorationRecenter(const float DeltaTime)
{
    if (!bExplorationRecenterActive)
    {
        return true;
    }

    if (!FMath::IsFinite(DeltaTime) || DeltaTime < 0.0f)
    {
        return false;
    }

    /*
     * 如果期间成功进入 LockOn，立即释放 Exploration 回正权威。后续 ControlRotation 由 LockOn 管理。
     */
    if (ModeStack.GetActiveModeTag() != WuwaGameplayTags::Camera_Exploration)
    {
        ResetExplorationRecenter();
        return true;
    }

    APlayerController* ViewController = GetLocalViewController();

    if (ViewController == nullptr || ExplorationRecenterStartRotation.ContainsNaN() ||
        ExplorationRecenterTargetRotation.ContainsNaN() || !FMath::IsFinite(ExplorationRecenterDuration) ||
        ExplorationRecenterDuration <= KINDA_SMALL_NUMBER)
    {
        return false;
    }

    ExplorationRecenterElapsed =FMath::Min(ExplorationRecenterElapsed + DeltaTime, ExplorationRecenterDuration);

    const float RawAlpha =FMath::Clamp(ExplorationRecenterElapsed / ExplorationRecenterDuration, 0.0f,1.0f);

    // SmoothStep：起点和终点速度都逐渐趋近于 0。
    const float SmoothAlpha = RawAlpha * RawAlpha * (3.0f - 2.0f * RawAlpha);

    /*
     * 使用最短角度差插值
     */
    const float PitchDelta = FMath::FindDeltaAngleDegrees(ExplorationRecenterStartRotation.Pitch, ExplorationRecenterTargetRotation.Pitch);

    const float YawDelta =FMath::FindDeltaAngleDegrees(ExplorationRecenterStartRotation.Yaw, ExplorationRecenterTargetRotation.Yaw);

    FRotator AppliedRotation(FMath::UnwindDegrees(ExplorationRecenterStartRotation.Pitch +
            PitchDelta * SmoothAlpha), FMath::UnwindDegrees(ExplorationRecenterStartRotation.Yaw + YawDelta * SmoothAlpha),0.0f);

    AppliedRotation.Normalize();

    if (AppliedRotation.ContainsNaN())
    {
        return false;
    }

    ViewController->SetControlRotation(AppliedRotation);

    if (RawAlpha >= 1.0f)
    {
        // 最后一帧提交精确目标值
        ViewController->SetControlRotation(ExplorationRecenterTargetRotation);

        ResetExplorationRecenter();
    }

    return true;
}

bool UWuwaCameraModeComponent::CancelExplorationRecenter()
{
    if (!bExplorationRecenterActive)
    {
        return false;
    }
    
    ResetExplorationRecenter();
    return true;
}

void UWuwaCameraModeComponent::ResetExplorationRecenter()
{
    ExplorationRecenterStartRotation = FRotator::ZeroRotator;

    ExplorationRecenterTargetRotation = FRotator::ZeroRotator;

    ExplorationRecenterElapsed = 0.0f;
    ExplorationRecenterDuration = 0.0f;
    bExplorationRecenterActive = false;
}

FWuwaCameraRigState UWuwaCameraModeComponent::BuildBaseRigState(const FWuwaCameraModeConfig &ModeConfig) const
{
    FWuwaCameraRigState Result;
    Result.FieldOfView = ModeConfig.FieldOfView;
    Result.TargetArmLength = ModeConfig.TargetArmLength;
    Result.TargetOffset = ModeConfig.TargetOffset;
    Result.SocketOffset = ModeConfig.SocketOffset;
    return Result;
}

bool UWuwaCameraModeComponent::ComposeLockOnRigState(const FWuwaCameraModeConfig &ModeConfig, const FWuwaTargetContext &TargetContext, FWuwaCameraRigState &InOutRigState) const
{
    const AActor *OwnerActor = GetOwner();
    const USpringArmComponent *CameraBoomComponent = CameraBoom.Get();

    if (!::IsValid(OwnerActor) ||
        !::IsValid(CameraBoomComponent) ||
        !IsUsableHardTargetContext(TargetContext))
    {
        return false;
    }

    /*
     * SpringArm TargetOffset 是世界空间 Pivot 偏移。
     * Camera 只将 Pivot 有限度地拉向 TargetPoint，不计算 LookAt Rotation。
     */
    const FVector BasePivotWorld = CameraBoomComponent->GetComponentLocation() + ModeConfig.TargetOffset;

    FVector WeightedPivotOffset = (TargetContext.TargetPoint - BasePivotWorld) * ModeConfig.TargetPivotWeight;

    WeightedPivotOffset = WeightedPivotOffset.GetClampedToMaxSize(ModeConfig.MaxTargetPivotOffset);

    InOutRigState.TargetOffset = ModeConfig.TargetOffset + WeightedPivotOffset;

    // SpringArm TargetArmLength 是从 Pivot 到 Camera 的距离。
    const float TargetDistance = FVector::Distance(OwnerActor->GetActorLocation(), TargetContext.TargetPoint);

    const float DesiredArmLength = ModeConfig.TargetArmLength + TargetDistance * ModeConfig.TargetDistanceArmScale;

    InOutRigState.TargetArmLength = FMath::Clamp(DesiredArmLength, ModeConfig.MinTargetArmLength, ModeConfig.MaxTargetArmLength);

    return InOutRigState.IsFinite();
}

APlayerController *UWuwaCameraModeComponent::GetLocalViewController() const
{
    const APawn *PawnOwner = Cast<APawn>(GetOwner());

    APlayerController *Controller = PawnOwner ? Cast<APlayerController>(PawnOwner->GetController()) : nullptr;

    return ::IsValid(Controller) && Controller->IsLocalController() ? Controller : nullptr;
}

/**
 * 固定视角的旋转计算
bool UWuwaCameraModeComponent::CalculateDesiredLockOnControlRotation(const FWuwaTargetContext &TargetContext, FRotator &OutControlRotation) const
{
    const USpringArmComponent *CameraBoomComponent = CameraBoom.Get();

    if (!::IsValid(CameraBoomComponent) || !CurrentRigState.IsFinite() || !IsUsableHardTargetContext(TargetContext))
    {
        return false;
    }

    const FVector PivotWorld = CameraBoomComponent->GetComponentLocation() + CurrentRigState.TargetOffset;

    const FVector PivotToTarget = TargetContext.TargetPoint - PivotWorld;

    if (PivotToTarget.ContainsNaN() || PivotToTarget.IsNearlyZero())
    {
        return false;
    }

    OutControlRotation = PivotToTarget.Rotation();
    OutControlRotation.Roll = 0.f;
    OutControlRotation.Normalize();

    return !OutControlRotation.ContainsNaN();
}
*/

/**
 * 固定yaw范围和pitch范围视角的旋转计算
bool UWuwaCameraModeComponent::CalculateDesiredLockOnControlRotation(const FWuwaTargetContext &TargetContext, FRotator &OutControlRotation) const
{
    const UCameraComponent *FollowCameraComponent = FollowCamera.Get();

    if (!::IsValid(FollowCameraComponent) || !IsUsableHardTargetContext(TargetContext))
    {
        return false;
    }

    // 目标方向必须从实际 FollowCamera 世界位置计算。
    // 这样 SpringArm 碰撞缩短后的真实镜头位置也会被正确消费，
    // 同时不会把玩家与目标的贴身距离直接放大为镜头角速度。
    const FVector CameraToTarget = TargetContext.TargetPoint - FollowCameraComponent->GetComponentLocation();

    if (CameraToTarget.ContainsNaN() || CameraToTarget.IsNearlyZero())
    {
        return false;
    }

    OutControlRotation = CameraToTarget.Rotation();
    OutControlRotation.Roll = 0.0f;
    OutControlRotation.Normalize();

    return !OutControlRotation.ContainsNaN();
}
*/

bool UWuwaCameraModeComponent::CalculateDesiredLockOnControlRotation(const FWuwaTargetContext &TargetContext, FRotator &OutControlRotation) const
{
    const AActor *OwnerActor = GetOwner();

    if (!::IsValid(OwnerActor) ||
        !IsUsableHardTargetContext(TargetContext) ||
        !bHasDefaultExplorationControlPitch ||
        !FMath::IsFinite(DefaultExplorationControlPitch))
    {
        return false;
    }

    float DesiredYaw = 0.0f;

    /*
     * 相机轨道只跟随 Player->Target 的水平战斗轴。
     * 不允许从实际 CameraLocation 反推 DesiredYaw：SpringArm 又会依据该 Yaw
     * 重算 CameraLocation，二者互相驱动会在近距离产生“镜头越过敌人再看回玩家”
     * 的第二个稳定解。
     */
    if (!WuwaCameraFramingRules::CalculateHorizontalCombatAxisYaw(
            OwnerActor->GetActorLocation(),
            TargetContext.TargetPoint,
            DesiredYaw))
    {
        return false;
    }

    const UCameraComponent* Camera = FollowCamera.Get();

    if (!IsValid(Camera))
    {
        return false;
    }

    const float BasePitch = DefaultExplorationControlPitch;

    const FVector CameraToTarget = TargetContext.TargetPoint - Camera->GetComponentLocation();

    if (CameraToTarget.IsNearlyZero())
    {
        return false;
    }

    const float LookAtPitch = CameraToTarget.Rotation().Pitch;

    // 只消费一部分目标俯仰差，保留现在的镜头构图风格。
    const float PitchDelta = FMath::FindDeltaAngleDegrees(BasePitch, LookAtPitch);

    constexpr float TargetPitchWeight = 0.95f;
    constexpr float MaxDownCorrection = 50.0f;
    constexpr float MaxUpCorrection = 10.0f;

    const float PitchCorrection = FMath::Clamp(PitchDelta * TargetPitchWeight, -MaxDownCorrection, MaxUpCorrection);

    const float DesiredPitch = BasePitch + PitchCorrection - 3;

    OutControlRotation = FRotator(DesiredPitch, DesiredYaw, 0.0f);


    // Pitch 使用进入 PIE 时捕获的默认 Exploration 基准，不再随 SpringArm 当前高度或玩家绕敌方位变化。
    //OutControlRotation = FRotator(DefaultExplorationControlPitch - 8.0f, DesiredYaw, 0.0f);

    OutControlRotation.Normalize();


    return !OutControlRotation.ContainsNaN();
}

/**
 * 应用固定的 LockOn 视角旋转
bool UWuwaCameraModeComponent::ApplyLockOnControlRotation(const FWuwaTargetContext &TargetContext, const float BlendAlpha) const
{
    APlayerController *ViewController = GetLocalViewController();

    // 非本地角色没有本地视口，不取得其 Controller Rotation 权限。
    if (ViewController == nullptr)
    {
        return true;
    }

    FRotator DesiredRotation;

    if (!CalculateDesiredLockOnControlRotation(TargetContext, DesiredRotation))
    {
        return false;
    }

    const float ClampedAlpha = FMath::Clamp(BlendAlpha, 0.f, 1.f);

    FRotator AppliedRotation = DesiredRotation;

    if (ClampedAlpha < 1.f)
    {
        const FRotator StartRotation = bHasTransitionStartControlRotation ? TransitionStartControlRotation : ViewController->GetControlRotation();

        if (StartRotation.ContainsNaN())
        {
            return false;
        }

        const FQuat BlendedRotation = FQuat::Slerp(StartRotation.Quaternion(), DesiredRotation.Quaternion(), ClampedAlpha).GetNormalized();

        if (BlendedRotation.ContainsNaN())
        {
            return false;
        }

        AppliedRotation = BlendedRotation.Rotator();
        AppliedRotation.Roll = 0.f;
        AppliedRotation.Normalize();
    }

    ViewController->SetControlRotation(AppliedRotation);
    return true;
}
*/

bool UWuwaCameraModeComponent::ApplyLockOnControlRotation(const FWuwaCameraModeConfig &ModeConfig, const FWuwaTargetContext &TargetContext, const float DeltaTime, const float AuthorityAlpha) const
{
    APlayerController *ViewController = GetLocalViewController();

    // 非本地角色没有本地视口，不取得其 Controller Rotation 权限。
    if (ViewController == nullptr)
    {
        return true;
    }

    FRotator DesiredRotation;

    if (!CalculateDesiredLockOnControlRotation(TargetContext, DesiredRotation))
    {
        return false;
    }

    const FRotator CurrentRotation = ViewController->GetControlRotation();

    if (CurrentRotation.ContainsNaN())
    {
        return false;
    }

    const AActor *OwnerActor = GetOwner();
    const UCameraComponent *FollowCameraComponent = FollowCamera.Get();

    if (!::IsValid(OwnerActor) || !::IsValid(FollowCameraComponent))
    {
        return false;
    }

    float CameraCombatAxisProjection = 0.0f;

    if (!WuwaCameraFramingRules::CalculateCameraCombatAxisProjection(
            OwnerActor->GetActorLocation(),
            TargetContext.TargetPoint,
            FollowCameraComponent->GetComponentLocation(),
            CameraCombatAxisProjection))
    {
        return false;
    }

    /*
     * Camera 位于玩家朝向目标的一侧时，说明镜头已经越过玩家平面。
     * 此时不能继续享受普通 LockOn 死区，也不能被模式 Blend 削弱修正权重；
     * 必须立即按 Player->Target 战斗轴恢复。修正仍受最大角速度限制，避免跳变。
     */
    const bool bCameraOnTargetSideOfPlayer = CameraCombatAxisProjection > KINDA_SMALL_NUMBER;

    /*
     * Camera 只消费 Profile 参数和 Target Context。
     * Yaw/Pitch 共用已通过自动化的纯规则，不在组件中复制算法。
     */
    FWuwaCameraAngleCorrectionInput YawInput;
    YawInput.CurrentAngleDegrees = CurrentRotation.Yaw;
    YawInput.DesiredAngleDegrees = DesiredRotation.Yaw;
    YawInput.DeadZoneHalfAngleDegrees = bCameraOnTargetSideOfPlayer
                                              ? 0.0f
                                              : ModeConfig.LockOnYawDeadZoneHalfAngle;
    YawInput.MaxCorrectionSpeedDegrees = ModeConfig.LockOnMaxYawCorrectionSpeed;
    YawInput.DeltaTime = DeltaTime;
    YawInput.AuthorityAlpha = bCameraOnTargetSideOfPlayer ? 1.0f : AuthorityAlpha;

    float CorrectedYaw = 0.0f;

    if (!WuwaCameraFramingRules::CalculateBoundedAngleCorrection(YawInput, CorrectedYaw))
    {
        return false;
    }

    FWuwaCameraAngleCorrectionInput PitchInput = YawInput;
    PitchInput.CurrentAngleDegrees = CurrentRotation.Pitch;
    PitchInput.DesiredAngleDegrees = DesiredRotation.Pitch;

    // 固定视角的 Pitch 规则暂时不启用，避免在低角度时出现过度抖动。
    // PitchInput.DeadZoneHalfAngleDegrees = ModeConfig.LockOnPitchDeadZoneHalfAngle;
    // PitchInput.MaxCorrectionSpeedDegrees = ModeConfig.LockOnMaxPitchCorrectionSpeed;

    const bool bAcquiringLockOnPitchComposition = bRigTransitionActive;

    PitchInput.DeadZoneHalfAngleDegrees = bAcquiringLockOnPitchComposition ? 0.0f : ModeConfig.LockOnPitchDeadZoneHalfAngle;
    PitchInput.MaxCorrectionSpeedDegrees = ModeConfig.LockOnMaxPitchCorrectionSpeed;

    float CorrectedPitch = 0.0f;

    if (!WuwaCameraFramingRules::CalculateBoundedAngleCorrection(PitchInput, CorrectedPitch))
    {
        return false;
    }

    FRotator AppliedRotation(CorrectedPitch, CorrectedYaw, 0.0f);

    AppliedRotation.Normalize();

    if (AppliedRotation.ContainsNaN())
    {
        return false;
    }

    ViewController->SetControlRotation(AppliedRotation);
    return true;
}

void UWuwaCameraModeComponent::BeginRigTransition(const float BlendDuration)
{
    TransitionStartRigState = CurrentRigState;
    TransitionElapsed = 0.f;
    TransitionDuration = FMath::Max(0.f, BlendDuration);
    bRigTransitionActive = TransitionDuration > KINDA_SMALL_NUMBER;
    
    /**
     * 固定旋转视角的旧逻辑
    bHasTransitionStartControlRotation = false;

    if (const APlayerController *ViewController = GetLocalViewController())
    {
        const FRotator CurrentControlRotation = ViewController->GetControlRotation();

        if (!CurrentControlRotation.ContainsNaN())
        {
            TransitionStartControlRotation = CurrentControlRotation;

            bHasTransitionStartControlRotation = true;
        }
    }
    */
}

bool UWuwaCameraModeComponent::UpdateAndApplyRig(const float DeltaTime, const FWuwaTargetContext &TargetContext)
{
    if (!FMath::IsFinite(DeltaTime) || DeltaTime < 0.f)
    {
        return false;
    }

    const FGameplayTag ActiveModeTag = ModeStack.GetActiveModeTag();

    const FWuwaCameraModeConfig *ActiveConfig = ModeStack.GetActiveModeConfig();

    if (!ActiveModeTag.IsValid() || ActiveConfig == nullptr || !ActiveConfig->IsRuntimeValid())
    {
        return false;
    }

    const bool bUsesLockOnComposition = ActiveModeTag == WuwaGameplayTags::Camera_LockOn;

    if (bUsesLockOnComposition && !IsUsableHardTargetContext(TargetContext))
    {
        return false;
    }

    AActor *DesiredCompositionTarget = bUsesLockOnComposition ? TargetContext.TargetActor.Get() : nullptr;

    const bool bModeChanged = AppliedModeTag != ActiveModeTag;

    if (bModeChanged)
    {
        float BlendDuration = ActiveConfig->BlendInTime;

        if (ActiveModeTag == WuwaGameplayTags::Camera_Exploration)
        {
            if (const FWuwaCameraModeConfig *PreviousConfig = ModeStack.FindModeConfig(AppliedModeTag))
            {
                BlendDuration = PreviousConfig->BlendOutTime;
            }
        }

        AppliedModeTag = ActiveModeTag;
        BeginRigTransition(BlendDuration);
    }
    else if (bUsesLockOnComposition && CompositionTargetActor.Get() != DesiredCompositionTarget)
    {
        /*
         * 目标切换不创建新模式请求，只从当前已应用 Rig
         * 向新目标构图连续过渡。
         */
        BeginRigTransition(ActiveConfig->BlendInTime);
    }

    CompositionTargetActor = DesiredCompositionTarget;

    FWuwaCameraRigState GoalRig = BuildBaseRigState(*ActiveConfig);

    if (bUsesLockOnComposition && !ComposeLockOnRigState(*ActiveConfig, TargetContext, GoalRig))
    {
        return false;
    }

    if (!GoalRig.IsFinite())
    {
        return false;
    }

    float ViewRotationBlendAlpha = 1.f;

    if (bRigTransitionActive)
    {
        TransitionElapsed += DeltaTime;

        const float RawAlpha = FMath::Clamp(TransitionElapsed / TransitionDuration, 0.f, 1.f);

        const float SmoothAlpha = RawAlpha * RawAlpha * (3.f - 2.f * RawAlpha);

        ViewRotationBlendAlpha = SmoothAlpha;

        CurrentRigState = InterpolateRigState(TransitionStartRigState, GoalRig, SmoothAlpha);

        if (RawAlpha >= 1.f)
        {
            CurrentRigState = GoalRig;
            bRigTransitionActive = false;
        }
    }
    else
    {
        // 同一目标移动只更新动态 Goal，不反复重启 Blend。
        CurrentRigState = GoalRig;
    }

    if (!ApplyRigState(CurrentRigState))
    {
        return false;
    }

    if (bUsesLockOnComposition && !ApplyLockOnControlRotation(*ActiveConfig, TargetContext, DeltaTime, ViewRotationBlendAlpha))
    {
        return false;
    }

    return true;
}

bool UWuwaCameraModeComponent::ApplyRigState(const FWuwaCameraRigState &RigState) const
{
    USpringArmComponent *CameraBoomComponent = CameraBoom.Get();
    UCameraComponent *FollowCameraComponent = FollowCamera.Get();

    if (!::IsValid(CameraBoomComponent) ||
        !::IsValid(FollowCameraComponent) ||
        !RigState.IsFinite() ||
        RigState.FieldOfView < 5.f ||
        RigState.FieldOfView > 170.f ||
        RigState.TargetArmLength <= 0.f)
    {
        return false;
    }

    CameraBoomComponent->TargetArmLength = RigState.TargetArmLength;
    CameraBoomComponent->TargetOffset = RigState.TargetOffset;
    CameraBoomComponent->SocketOffset = RigState.SocketOffset;
    FollowCameraComponent->SetFieldOfView(RigState.FieldOfView);

    return true;
}

FWuwaCameraRigState UWuwaCameraModeComponent::InterpolateRigState(
    const FWuwaCameraRigState &From,
    const FWuwaCameraRigState &To,
    const float Alpha)
{
    const float ClampedAlpha = FMath::Clamp(Alpha, 0.f, 1.f);

    FWuwaCameraRigState Result;
    Result.FieldOfView = FMath::Lerp(From.FieldOfView, To.FieldOfView, ClampedAlpha);
    Result.TargetArmLength = FMath::Lerp(From.TargetArmLength, To.TargetArmLength, ClampedAlpha);
    Result.TargetOffset = FMath::Lerp(From.TargetOffset, To.TargetOffset, ClampedAlpha);
    Result.SocketOffset = FMath::Lerp(From.SocketOffset, To.SocketOffset, ClampedAlpha);

    return Result;
}

void UWuwaCameraModeComponent::ResetRuntimeState()
{
    ResetExplorationRecenter();
    
    /*
     * 重新初始化或异常结束时恢复接管前的 Rig，
     * 防止上一次模式参数残留到下一次 Camera 生命周期。
     */
    if (CameraBoom.IsValid() && FollowCamera.IsValid())
    {
        ApplyRigState(InitialRigState);
    }

    // 移除Tick前置依赖
    if (USpringArmComponent *CameraBoomComponent = CameraBoom.Get())
    {
        CameraBoomComponent->RemoveTickPrerequisiteComponent(this);
    }

    UWuwaTargetingComponent *Targeting = TargetingComponent.Get();

    if (::IsValid(Targeting))
    {
        Targeting->OnTargetContextChanged.RemoveDynamic(this, &UWuwaCameraModeComponent::HandleTargetContextChanged);
    }

    /*
     * 正常结束优先精确释放 Handle；来源销毁、重复请求或 EndPlay
     * 再由 ReleaseBySource 与 Reset 保证不留下 Camera 请求。
     */
    if (LockOnRequestHandle.IsValid())
    {
        ModeStack.ReleaseMode(LockOnRequestHandle);
    }

    if (Targeting != nullptr)
    {
        ModeStack.ReleaseBySource(Targeting);
    }

    LockOnRequestHandle.Reset();
    ModeStack.Reset();
    TargetingComponent.Reset();

    CameraBoom.Reset();
    FollowCamera.Reset();

    DefaultExplorationControlPitch = 0.0f;
    bHasDefaultExplorationControlPitch = false;

    InitialRigState = FWuwaCameraRigState();
    CurrentRigState = FWuwaCameraRigState();

    TransitionStartRigState = FWuwaCameraRigState();

    /*
     * 不恢复锁定前的 ControlRotation。
     * 解除后 Exploration 从最后画面朝向恢复手动 Look，避免回跳。
     */
    // TransitionStartControlRotation = FRotator::ZeroRotator;
    // bHasTransitionStartControlRotation = false;

    AppliedModeTag = FGameplayTag();
    CompositionTargetActor.Reset();
    TransitionElapsed = 0.f;
    TransitionDuration = 0.f;
    bRigTransitionActive = false;

    bInitialized = false;

    SetComponentTickEnabled(false);
}

void UWuwaCameraModeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ResetRuntimeState();

    Super::EndPlay(EndPlayReason);
}
