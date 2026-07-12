#include "Movement/WuwaCharacterMovementComponent.h"

#include "Core/WuwaStateTagComponent.h"
#include "Core/WuwaGameplayTags.h"
#include "Movement/WuwaMovementProfile.h"
#include "GameFramework/Character.h"
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

    RefreshLocomotionState();

    UE_LOG(LogWuwa, Display, TEXT("已应用 MovementProfile。Profile=%s"), *Profile->GetPathName());

    return true;
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
    Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

    // 离地时立即清除冲刺状态
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

    if (CharacterOwner && !Velocity.IsNearlyZero())
    {
        // 转为角色局部速度，供动画计算方向
        const FVector LocalVelocity = CharacterOwner->GetActorTransform().InverseTransformVectorNoScale(Velocity);

        Snapshot.Direction = FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X));
    }

    return Snapshot;
}
