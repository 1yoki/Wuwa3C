#include "Movement/WuwaCharacterMovementComponent.h"

#include "Core/WuwaStateTagComponent.h"
#include "Core/WuwaGameplayTags.h"
#include "Movement/WuwaMovementProfile.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Wuwa.h"

namespace
{
    // 避免浮点抖动让 Sprint Run 在 RunSpeed 附近多停留一帧。
    constexpr float SprintRunExitSpeedTolerance = 5.f;
}

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

bool UWuwaCharacterMovementComponent::CanPerformAirDoubleJump() const
{
    return IsValid(CharacterOwner) && HasValidData() && IsFalling() && !AirActionState.bAirSprintConsumed && AirActionState.JumpCount < ConfiguredMaxJumpCount;
}

bool UWuwaCharacterMovementComponent::CommitAirDoubleJump(const FVector &WorldDirection, const float HorizontalSpeed, const float VerticalSpeed, const EWuwaJumpType JumpType)
{
    // 在修改任何运行态前完成全部准入校验，
    if (!CanPerformAirDoubleJump() || WorldDirection.ContainsNaN() ||
        HorizontalSpeed < 0.f || VerticalSpeed <= 0.f ||
        !FMath::IsFinite(HorizontalSpeed) || !FMath::IsFinite(VerticalSpeed))
    {
        return false;
    }

    // 限定空中二段跳类型
    if (JumpType != EWuwaJumpType::AirSprint && JumpType != EWuwaJumpType::AirBackflip)
    {
        return false;
    }

    const FVector HorizontalDirection = WorldDirection.GetSafeNormal2D();

    if (HorizontalDirection.IsNearlyZero())
    {
        return false;
    }

    // 使用局部副本计算完整结果
    FVector NewVelocity = Velocity;

    NewVelocity.X = HorizontalDirection.X * HorizontalSpeed;
    NewVelocity.Y = HorizontalDirection.Y * HorizontalSpeed;
    NewVelocity.Z = VerticalSpeed;

    // 执行原子提交，速度、预算和表现事件必须保持一致
    Velocity = NewVelocity;

    AirActionState.bAirSprintConsumed = true;
    AirActionState.BufferedJumpExpireAt = -1.0;
    ++AirActionState.JumpCount;

    // 防止落地缓存标记在空中二段跳后继续触发
    bPendingBufferedJump = false;

    LastJumpType = JumpType;
    ++JumpSequence;

    UE_LOG(LogWuwa, Verbose, TEXT("空中二段跳物理提交。Type=%d Count=%d"), static_cast<int32>(JumpType), AirActionState.JumpCount);

    return true;
}

bool UWuwaCharacterMovementComponent::RequestDirectionalDoubleJump(const FVector &ForwardDirectionSnapshot)
{
    // Directional 的方向已经由 Action Source 冻结为角色朝向
    return CommitAirDoubleJump(ForwardDirectionSnapshot, ConfiguredDoubleJumpForwardSpeed, ConfiguredDoubleJumpZVelocity, EWuwaJumpType::AirSprint);
}

bool UWuwaCharacterMovementComponent::RequestBackflipDoubleJump(const FVector &BackwardDirectionSnapshot)
{
    // Backflip 的方向已经由 Action Source 冻结为角色背向
    return CommitAirDoubleJump(BackwardDirectionSnapshot, ConfiguredBackflipBackwardSpeed, ConfiguredBackflipZVelocity, EWuwaJumpType::AirBackflip);
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
    ConfiguredSprintRunDeceleration = Profile->SprintRunDeceleration;
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
    ConfiguredBackflipZVelocity = Profile->BackflipZVelocity;
    ConfiguredBackflipBackwardSpeed = Profile->BackflipBackwardSpeed;
    ConfiguredMaxJumpCount = Profile->MaxJumpCount;
    ConfiguredCoyoteTime = Profile->CoyoteTime;
    ConfiguredJumpBufferTime = Profile->JumpBufferTime;
    ConfiguredHeavyLandingVelocityThreshold = Profile->HeavyLandingVelocityThreshold;
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

    // 物理更新完成后再衰减 Dash 出口速度，避免与本帧 CharacterMovement 积分互相覆盖。
    UpdateSprintRun(DeltaTime);
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

    // 真实落地来源均为 Normal；Light/Heavy 只由接触前的向下速度分类
    LastLandingEvent.LandingSource = EWuwaLandingSource::Normal;
    LastLandingEvent.LandingType = WuwaMovementRules::ClassifyLanding(LastLandingEvent.ImpactSpeed, ConfiguredHeavyLandingVelocityThreshold);
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

bool UWuwaCharacterMovementComponent::EnterSprintRun(const FVector2D &MoveIntent)
{
    if (MoveIntent.ContainsNaN())
    {
        return false;
    }

    // Block.Input.Move 刚由 Router 释放，本次调用先同步真实 WASD，避免等待下一帧输入门面。
    MoveInputMagnitude = FMath::Clamp(MoveIntent.Size(), 0.f, 1.f);

    if (!CanMaintainSprintRun())
    {
        RefreshLocomotionState();
        return false;
    }

    // Sprinting 标签只表示 Dash 出口减速阶段，不代表持续按键可永久保持高速。
    SetSprinting(true);

    RefreshLocomotionState();

    return bIsSprinting;
}

void UWuwaCharacterMovementComponent::ExitSprintRun()
{
    SetSprinting(false);
    RefreshLocomotionState();
}

bool UWuwaCharacterMovementComponent::CanMaintainSprintRun() const
{
    const bool bBlocked = StateTagComponent && StateTagComponent->HasAny(RuntimeSprintBlockedTags);
    const float HorizontalSpeed = Velocity.Size2D();

    return !bBlocked && MoveInputMagnitude > 0.1f && IsMovingOnGround() &&
           FMath::IsFinite(HorizontalSpeed) && HorizontalSpeed > ConfiguredRunSpeed + SprintRunExitSpeedTolerance;
}

void UWuwaCharacterMovementComponent::UpdateSprintRun(const float DeltaTime)
{
    if (!bIsSprinting)
    {
        return;
    }

    if (!CanMaintainSprintRun())
    {
        ExitSprintRun();
        return;
    }

    if (DeltaTime <= 0.f)
    {
        return;
    }

    const FVector HorizontalVelocity(Velocity.X, Velocity.Y, 0.f);
    const float CurrentHorizontalSpeed = HorizontalVelocity.Size();
    float NewHorizontalSpeed = FMath::Max(
        ConfiguredRunSpeed,
        CurrentHorizontalSpeed - ConfiguredSprintRunDeceleration * DeltaTime);

    if (NewHorizontalSpeed <= ConfiguredRunSpeed + SprintRunExitSpeedTolerance)
    {
        // 最后一帧直接钳到 RunSpeed，避免普通移动长期保留极小的超速尾差。
        NewHorizontalSpeed = ConfiguredRunSpeed;
    }

    // 只缩放本帧 CharacterMovement 已计算出的水平向量，保留 WASD 转向结果和垂直速度。
    const FVector NewHorizontalVelocity = HorizontalVelocity.GetSafeNormal() * NewHorizontalSpeed;
    Velocity.X = NewHorizontalVelocity.X;
    Velocity.Y = NewHorizontalVelocity.Y;

    if (NewHorizontalSpeed <= ConfiguredRunSpeed)
    {
        ExitSprintRun();
        return;
    }

    // 下一帧物理计算前让速度上限跟随衰减结果，避免 UE 内置超速制动再次抢先压到 RunSpeed。
    MaxWalkSpeed = NewHorizontalSpeed;
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
        // Sprint Run 的实际减速只由 UpdateSprintRun 负责；
        // 上限跟随当前出口速度，使 CharacterMovement 不会把继承速度判定为普通移动超速。
        MaxWalkSpeed = FMath::Max(ConfiguredRunSpeed, Velocity.Size2D());
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
    // 在 Super 前保存旧模式事实，供土狼时间与事件消费者使用
    const bool bWasGrounded = PreviousMovementMode == MOVE_Walking || PreviousMovementMode == MOVE_NavWalking;

    Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

    const EMovementMode NewMovementMode = MovementMode;
    const uint8 NewCustomMode = CustomMovementMode;

    if (bWasGrounded && NewMovementMode == MOVE_Falling)
    {
        AirActionState.LastGroundedTime = GetMovementTime();

        if (CharacterOwner)
        {
            // 记录本次滞空的起始高度
            AirActionState.FallStartHeight = CharacterOwner->GetActorLocation().Z;
        }
    }

    // 先同步 Movement Component 自己拥有的移动状态，再通知外部消费者
    RefreshLocomotionState();

    OnWuwaMovementModeChanged.Broadcast(PreviousMovementMode, PreviousCustomMode, NewMovementMode, NewCustomMode);
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
