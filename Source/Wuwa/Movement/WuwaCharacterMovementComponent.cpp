#include "Movement/WuwaCharacterMovementComponent.h"

#include "Core/WuwaStateTagComponent.h"
#include "Core/WuwaGameplayTags.h"
#include "Movement/WuwaMovementProfile.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Wuwa.h"

UWuwaCharacterMovementComponent::UWuwaCharacterMovementComponent()
{
    MaxWalkSpeed = ConfiguredRunSpeed;
    MaxAcceleration = 2200.f;
    BrakingDecelerationWalking = 1500.f;
    BrakingDecelerationFalling = 1500.f;
    GroundFriction = 8.f;
    BrakingFrictionFactor = 2.f;
    RotationRate = FRotator(0.f, 720.f, 0.f);
    AirControl = 0.4f;
}

void UWuwaCharacterMovementComponent::BeginPlay()
{
    Super::BeginPlay();

    const double CurrentTime = GetMovementTime();

    if (IsMovingOnGround())
    {
        // 初始化土狼时间使用的接地时间
        AirActionState.LastGroundedTime = CurrentTime;
    }

    if (CharacterOwner)
    {
        // 初始化本次空中的高度记录
        AirActionState.FallStartHeight = CharacterOwner->GetActorLocation().Z;
    }

    RefreshLocomotionState();
}

void UWuwaCharacterMovementComponent::SetStateTagComponent(UWuwaStateTagComponent *InStateTagComponent)
{
    if (StateTagComponent == InStateTagComponent)
    {
        return;
    }
    // 切换组件前释放旧标签引用
    if (bIsSprinting && StateTagComponent)
    {
        StateTagComponent->RemoveTag(WuwaGameplayTags::State_Locomotion_Sprinting);
    }

    StateTagComponent = InStateTagComponent;

    // 保持新组件中的标签与当前状态一致。
    if (bIsSprinting && StateTagComponent)
    {
        StateTagComponent->AddTag(
            WuwaGameplayTags::
                State_Locomotion_Sprinting);
    }

    RefreshLocomotionState();
}

double UWuwaCharacterMovementComponent::GetMovementTime() const
{
    return GetWorld() ? static_cast<double>(GetWorld()->GetTimeSeconds()) : 0.0;
}

// 判断角色离开地面后是否仍处于允许普通跳跃的时间窗口。
bool UWuwaCharacterMovementComponent::IsWithCoyoteTime(const double CurrentTime) const
{
    if (AirActionState.LastGroundedTime < 0.0)
    {
        return false;
    }

    const double TimeSinceGrounded = CurrentTime - AirActionState.LastGroundedTime;

    return TimeSinceGrounded >= 0.0 && TimeSinceGrounded <= ConfiguredCoyoteTime;
}

bool UWuwaCharacterMovementComponent::RequestJump()
{
    if (!CharacterOwner || !HasValidData())
    {
        return false;
    }

    const double CurrentTime = GetMovementTime();

    const bool bCanGroundJump = IsMovingOnGround() && CharacterOwner->CanJump();

    const bool bCanCoyoteJump = IsWithCoyoteTime(CurrentTime) && IsFalling() && AirActionState.JumpCount == 0;

    if ((bCanGroundJump || bCanCoyoteJump) && AirActionState.JumpCount < ConfiguredMaxJumpCount)
    {
        // 成功起跳后清楚缓存
        AirActionState.BufferedJumpExpireAt = -1.0;
        return StartJump(JumpZVelocity, bCanCoyoteJump ? EWuwaJumpType::Coyote : EWuwaJumpType::Ground);
    }

    if (IsFalling() && ConfiguredJumpBufferTime > 0.f)
    {
        // 保存落地前输入的跳跃请求。
        AirActionState.BufferedJumpExpireAt = CurrentTime + ConfiguredJumpBufferTime;

        UE_LOG(LogWuwa, Verbose, TEXT("已缓存跳跃请求。Owner=%s, ExpireAt=%.3f"), *GetNameSafe(CharacterOwner), AirActionState.BufferedJumpExpireAt);
    }

    return false;
}

// 由 Movement Component 设置跳跃速度、模式和跳跃计数
bool UWuwaCharacterMovementComponent::StartJump(const float InitialZVelocity, const EWuwaJumpType JumpType)
{
    if (!CharacterOwner || !HasValidData() || InitialZVelocity <= 0.f || MovementMode == MOVE_None)
    {
        return false;
    }

    Velocity.Z = InitialZVelocity;

    ++AirActionState.JumpCount;
    LastJumpType = JumpType;
    ++JumpSequence;

    SetMovementMode(MOVE_Falling);

    UE_LOG(LogWuwa, Verbose, TEXT("跳跃开始。Type=%d Count=%d"), static_cast<int32>(JumpType), AirActionState.JumpCount);

    return true;
}

bool UWuwaCharacterMovementComponent::RequestAirSprint(const FVector &DesiredWorldDirection)
{
    if (!CharacterOwner || !HasValidData() || !IsFalling() ||
        AirActionState.bAirSprintConsumed || AirActionState.JumpCount >= ConfiguredMaxJumpCount)
    {
        return false;
    }

    FVector JumpDirection = DesiredWorldDirection.GetSafeNormal2D();

    if (JumpDirection.IsNearlyZero())
    {
        // 没有输入时使用角色朝向
        JumpDirection = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
    }

    // 覆盖当前速度的水平分量
    Velocity.X = JumpDirection.X * ConfiguredDoubleJumpForwardSpeed;
    Velocity.Y = JumpDirection.Y * ConfiguredDoubleJumpForwardSpeed;
    Velocity.Z = ConfiguredDoubleJumpZVelocity;

    AirActionState.bAirSprintConsumed = true;
    AirActionState.BufferedJumpExpireAt = -1.0;
    ++AirActionState.JumpCount;

    LastJumpType = EWuwaJumpType::AirSprint;
    ++JumpSequence;

    UE_LOG(LogWuwa, Verbose, TEXT("空中冲刺起跳。 Count=%d"), AirActionState.JumpCount);

    return true;
}

bool UWuwaCharacterMovementComponent::ApplyMovementProfile(const UWuwaMovementProfile *Profile)
{
    if (!Profile)
    {
        UE_LOG(LogWuwa, Error, TEXT("MovementProfile 为空。Owner=%s"), *GetNameSafe(CharacterOwner));
        return false;
    }

    // 保存运行时副本，避免每帧访问 Data Asset。
    ConfiguredWalkSpeed = Profile->WalkSpeed;
    ConfiguredRunSpeed = Profile->RunSpeed;
    ConfiguredSprintSpeed = Profile->SprintSpeed;
    ConfiguredAnalogRunThreshold = Profile->AnalogRunThreshold;
    RuntimeSprintBlockedTags = Profile->SprintBlockedTags;

    MaxAcceleration = Profile->MaxAcceleration;
    BrakingDecelerationWalking = Profile->BrakingDecelerationWalking;
    BrakingDecelerationFalling = Profile->BrakingDecelerationFalling;
    GroundFriction = Profile->GroundFriction;
    BrakingFrictionFactor = Profile->BrakingFrictionFactor;
    RotationRate = Profile->RotationRate;
    AirControl = Profile->AirControl;

    // 角色初始化时应用Movement Profile中的跳跃运行时配置。
    JumpZVelocity = Profile->JumpZVelocity;
    ConfiguredDoubleJumpZVelocity = Profile->DoubleJumpZVelocity;
    ConfiguredDoubleJumpForwardSpeed = Profile->DoubleJumpForwardSpeed;
    ConfiguredMaxJumpCount = Profile->MaxJumpCount;
    ConfiguredCoyoteTime = Profile->CoyoteTime;
    ConfiguredJumpBufferTime = Profile->JumpBufferTime;
    GravityScale = Profile->GravityScale;

    RefreshLocomotionState();

    UE_LOG(LogWuwa, Display, TEXT("已应用 MovementProfile。Profile=%s"), *Profile->GetPathName());

    return true;
}

void UWuwaCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!HasValidData())
    {
        return;
    }

    const double CurrentTime = GetMovementTime();

    ExpireJumpBuffer(CurrentTime);

    if (IsFalling() && CharacterOwner)
    {
        // 持续记录本次滞空的最高位置
        AirActionState.FallStartHeight = FMath::Max(AirActionState.FallStartHeight, CharacterOwner->GetActorLocation().Z);
    }

    if (IsMovingOnGround())
    {
        AirActionState.LastGroundedTime = CurrentTime;

        if (bPendingBufferedJump)
        {
            // 落地后立即消费跳跃缓存
            bPendingBufferedJump = false;
            StartJump(JumpZVelocity, EWuwaJumpType::Ground);
        }
    }
}

// 实现缓存跳跃的过期逻辑，避免落地前的跳跃请求无限期保留。
void UWuwaCharacterMovementComponent::ExpireJumpBuffer(const double CurrentTime)
{
    if (AirActionState.BufferedJumpExpireAt > 0.0 && CurrentTime >= AirActionState.BufferedJumpExpireAt)
    {
        // 清除已经失效的跳跃请求。
        AirActionState.BufferedJumpExpireAt = -1.0;
    }
}

// 生成普通轻落地事件、重置所有空中次数，并准备消费跳跃缓存。
void UWuwaCharacterMovementComponent::ProcessLanded(const FHitResult &Hit, const float RemainingTime, const int32 Iterations)
{
    const double CurrentTime = GetMovementTime();
    const FVector ImpactVelocity = Velocity;

    const bool bHasBufferedJump = AirActionState.BufferedJumpExpireAt >= CurrentTime;

    const float LandingHeight = CharacterOwner ? CharacterOwner->GetActorLocation().Z : AirActionState.FallStartHeight;

    const float FallDistance = FMath::Max(0.f, AirActionState.FallStartHeight - LandingHeight);

    const int32 NextSequence = LastLandingEvent.Sequence + 1;

    LastLandingEvent = FWuwaLandingEvent();
    LastLandingEvent.ImpactVelocity = ImpactVelocity;
    LastLandingEvent.ImpactSpeed = FMath::Max(0.f, -ImpactVelocity.Z);
    LastLandingEvent.FallDistance = FallDistance;

    // 目前真实落地都为普通落地，后续可根据落地时的速度和行为类型区分。
    LastLandingEvent.LandingSource = EWuwaLandingSource::Normal;
    LastLandingEvent.LandingType = EWuwaLandingType::Light;
    LastLandingEvent.Sequence = NextSequence;

    // 落地时重置所有空中次数，避免滞空后继续使用二段跳、下落攻击等。
    AirActionState.ResetBudgetsOnLanding();
    AirActionState.LastGroundedTime = CurrentTime;
    AirActionState.BufferedJumpExpireAt = -1.0;
    AirActionState.FallStartHeight = LandingHeight;

    bPendingBufferedJump = bHasBufferedJump;

    Super::ProcessLanded(Hit, RemainingTime, Iterations);

    // 父类完成落地物理后再广播事实
    OnLandedEvent.Broadcast(LastLandingEvent);
}

void UWuwaCharacterMovementComponent::SetLocomotionIntent(const FVector2D &MoveIntent)
{
    // 对角输入最大按 1 处理。
    MoveInputMagnitude = FMath::Clamp(MoveIntent.Size(), 0.f, 1.f);

    RefreshLocomotionState();
}

void UWuwaCharacterMovementComponent::EnterSprintRun()
{
    if (CanMaintainSprintRun())
    {
        // 前冲完成后进入高速跑步。
        SetSprinting(true);
    }

    RefreshLocomotionState();
}

void UWuwaCharacterMovementComponent::ExitSprintRun()
{
    SetSprinting(false);
    RefreshLocomotionState();
}

bool UWuwaCharacterMovementComponent::CanMaintainSprintRun() const
{
    const bool bBlocked = StateTagComponent && StateTagComponent->HasAny(RuntimeSprintBlockedTags);

    return !bBlocked && MoveInputMagnitude > 0.1f && IsMovingOnGround();
}

void UWuwaCharacterMovementComponent::RefreshLocomotionState()
{
    if (bIsSprinting && !CanMaintainSprintRun())
    {
        // 停止移动、离地或受阻时退出高速跑步。
        SetSprinting(false);
    }

    if (bIsSprinting)
    {
        MaxWalkSpeed = ConfiguredSprintSpeed;
    }
    else if (MoveInputMagnitude >= ConfiguredAnalogRunThreshold)
    {
        MaxWalkSpeed = ConfiguredRunSpeed;
    }
    else
    {
        MaxWalkSpeed = ConfiguredWalkSpeed;
    }
}

void UWuwaCharacterMovementComponent::SetSprinting(const bool bNewSprinting)
{
    // 状态未变化时不重复增删标签。
    if (bIsSprinting == bNewSprinting)
    {
        return;
    }

    // 只在冲刺状态变化时增删标签
    bIsSprinting = bNewSprinting;

    if (!StateTagComponent)
    {
        return;
    }

    if (bIsSprinting)
    {
        StateTagComponent->AddTag(WuwaGameplayTags::State_Locomotion_Sprinting);
    }
    else
    {
        StateTagComponent->RemoveTag(WuwaGameplayTags::State_Locomotion_Sprinting);
    }
}

void UWuwaCharacterMovementComponent::OnMovementModeChanged(const EMovementMode PreviousMovementMode, const uint8 PreviousCustomMode)
{
    // 记录离地前的时间戳，用于土狼时间判断。
    const bool bWasGrounded = PreviousMovementMode == MOVE_Walking || PreviousMovementMode == MOVE_NavWalking;

    Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

    if (bWasGrounded && IsFalling())
    {
        AirActionState.LastGroundedTime = GetMovementTime();

        if (CharacterOwner)
        {
            // 记录本次滞空的起始高度
            AirActionState.FallStartHeight = CharacterOwner->GetActorLocation().Z;
        }
    }

    // 离地时立即清除状态
    RefreshLocomotionState();
}

void UWuwaCharacterMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 销毁前释放持有的冲刺标签
    if (bIsSprinting && StateTagComponent)
    {
        StateTagComponent->RemoveTag(WuwaGameplayTags::State_Locomotion_Sprinting);
    }
    bIsSprinting = false;

    Super::EndPlay(EndPlayReason);
}

// 让动画和 Debug 读取 Movement Component 的结果，而不是维护自己的计数。
FWuwaLocomotionSnapshot UWuwaCharacterMovementComponent::GetLocomotionSnapshot() const
{
    FWuwaLocomotionSnapshot Snapshot;

    Snapshot.Velocity = Velocity;
    Snapshot.Acceleration = GetCurrentAcceleration();
    Snapshot.HorizontalSpeed = Velocity.Size2D();
    Snapshot.InputMagnitude = MoveInputMagnitude;
    Snapshot.bIsMovingOnGround = IsMovingOnGround();
    Snapshot.bIsFalling = IsFalling();
    Snapshot.bIsSprinting = bIsSprinting;

    Snapshot.VerticalVelocity = Velocity.Z;
    Snapshot.JumpCount = AirActionState.JumpCount;
    Snapshot.LastJumpType = LastJumpType;
    Snapshot.JumpSequence = JumpSequence;

    Snapshot.LastLandingType = LastLandingEvent.LandingType;
    Snapshot.LastLandingVelocity = LastLandingEvent.ImpactSpeed;
    Snapshot.LastFallDistance = LastLandingEvent.FallDistance;
    Snapshot.LandingSequence = LastLandingEvent.Sequence;

    if (CharacterOwner && !Velocity.IsNearlyZero())
    {
        // 转为角色局部速度，供动画计算方向
        const FVector LocalVelocity = CharacterOwner->GetActorTransform().InverseTransformVectorNoScale(Velocity);

        Snapshot.Direction = FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X));
    }

    return Snapshot;
}
