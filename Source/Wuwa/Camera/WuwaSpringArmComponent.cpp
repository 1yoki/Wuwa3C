#include "Camera/WuwaSpringArmComponent.h"

namespace
{
    constexpr float RecoveryDistanceTolerance = 0.5f;

    bool IsValidCollisionChannel(const ECollisionChannel Channel)
    {
        return Channel >= ECC_WorldStatic && Channel < ECC_MAX;
    }
}

bool UWuwaSpringArmComponent::ConfigureCollision(const float InProbeSize, ECollisionChannel InProbeChannel, const float InRecoveryInterpSpeed)
{
    if (!FMath::IsFinite(InProbeSize) || InProbeSize <= 0.f ||
        !IsValidCollisionChannel(InProbeChannel) || !FMath::IsFinite(InRecoveryInterpSpeed) ||
        InRecoveryInterpSpeed <= 0.f)
    {
        return false;
    }

    ProbeSize = InProbeSize;
    ProbeChannel = InProbeChannel;
    CollisionRecoveryInterpSpeed = InRecoveryInterpSpeed;
    bDoCollisionTest = true;

    ResetCollisionRecovery();
    return true;
}

void UWuwaSpringArmComponent::OnRegister()
{
    Super::OnRegister();
    ResetCollisionRecovery();
}

void UWuwaSpringArmComponent::OnUnregister()
{
    /*
     * 组件注销、Owner EndPlay 和重新注册都不能携带旧墙体距离。
     * 这里只清理 Camera 自己的表现状态。
     */
    ResetCollisionRecovery();
    Super::OnUnregister();
}

FVector UWuwaSpringArmComponent::BlendLocations(const FVector &DesiredArmLocation, const FVector &TraceHitLocation, const bool bHitSomething, const float DeltaTime)
{
    const FVector NativeLocation = Super::BlendLocations(DesiredArmLocation, TraceHitLocation, bHitSomething, DeltaTime);

    const FVector ArmOrigin = GetComponentLocation() + TargetOffset;

    if (DesiredArmLocation.ContainsNaN() || NativeLocation.ContainsNaN() ||
        ArmOrigin.ContainsNaN() || !FMath::IsFinite(DeltaTime) || DeltaTime < 0.f ||
        !FMath::IsFinite(CollisionRecoveryInterpSpeed) || CollisionRecoveryInterpSpeed <= 0.f)
    {
        ResetCollisionRecovery();
        return NativeLocation;
    }

    const FVector DesiredVector = DesiredArmLocation - ArmOrigin;
    const float DesiredDistance = DesiredVector.Size();

    if (!FMath::IsFinite(DesiredDistance) || DesiredDistance <= KINDA_SMALL_NUMBER)
    {
        ResetCollisionRecovery();
        return NativeLocation;
    }

    const FVector DesiredDirection = DesiredVector / DesiredDistance;

    float NativeDistance = FVector::Distance(ArmOrigin, NativeLocation);

    if (!FMath::IsFinite(NativeDistance))
    {
        ResetCollisionRecovery();
        return NativeLocation;
    }

    NativeDistance = FMath::Clamp(NativeDistance, 0.f, DesiredDistance);

    /*
     * 没有碰撞历史时保持原生行为：
     * 无命中直接使用 Desired，首次命中立即缩短到命中距离。
     */
    if (!bHasResolvedCollisionDistance)
    {
        if (!bHitSomething)
        {
            return DesiredArmLocation;
        }

        CurrentResolvedDistance = NativeDistance;
        bHasResolvedCollisionDistance = true;

        return ArmOrigin + DesiredDirection * CurrentResolvedDistance;
    }

    /*
     * 无命中且模式本身把 Desired Arm 缩短到当前距离以内时，
     * 立即服从 Mode Rig；碰撞恢复不得平滑 Camera Mode 参数。
     */
    if (!bHitSomething && DesiredDistance <= CurrentResolvedDistance + RecoveryDistanceTolerance)
    {
        ResetCollisionRecovery();
        return DesiredArmLocation;
    }

    if (NativeDistance < CurrentResolvedDistance - RecoveryDistanceTolerance)
    {
        // 新障碍更近：立即缩短，优先避免穿墙。
        CurrentResolvedDistance = NativeDistance;
    }
    else if (NativeDistance > CurrentResolvedDistance + RecoveryDistanceTolerance)
    {
        // 障碍后退或消失：只平滑增加有效距离。
        CurrentResolvedDistance = FMath::FInterpTo(CurrentResolvedDistance, NativeDistance, DeltaTime, CollisionRecoveryInterpSpeed);
    }
    else
    {
        // 误差范围内不更新距离，避免抖动。
        CurrentResolvedDistance = NativeDistance;
    }

    CurrentResolvedDistance = FMath::Clamp(CurrentResolvedDistance, 0.f, DesiredDistance);

    if (!bHitSomething && FMath::IsNearlyEqual(CurrentResolvedDistance, DesiredDistance, RecoveryDistanceTolerance))
    {
        ResetCollisionRecovery();
        return DesiredArmLocation;
    }

    /*
     * 使用当前帧 Arm Origin 与 Desired Direction 重建位置。
     * 角色移动或旋转时不会产生旧世界位置拖尾。
     */
    return ArmOrigin + DesiredDirection * CurrentResolvedDistance;
}

void UWuwaSpringArmComponent::ResetCollisionRecovery()
{
    CurrentResolvedDistance = 0.f;
    bHasResolvedCollisionDistance = false;
}