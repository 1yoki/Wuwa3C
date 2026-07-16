#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "WuwaInputTypes.generated.h"

/*
 * 一次离散输入边沿的不可变快照。
 * Move/Look 等持续状态仍保留在 FWuwaInputIntent 中，不进入普通 FIFO。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaInputCommand
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Input Command")
    FGameplayTag InputTag;

    UPROPERTY(BlueprintReadOnly, Category = "Input Command")
    double PressedAt = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "Input Command", meta = (ClampMin = "0.01", Units = "s"))
    float ValidDuration = 0.15f;

    // 按键边沿发生时的 WASD 快照；入队后不再随持续输入变化。
    UPROPERTY(BlueprintReadOnly, Category = "Input Command")
    FVector2D Direction = FVector2D::ZeroVector;

    UPROPERTY()
    uint32 Sequence = 0;

    bool IsValid() const
    {
        return InputTag.IsValid() && PressedAt >= 0.0 && ValidDuration > 0.f && Sequence != 0;
    }

    double GetExpireAt() const
    {
        return PressedAt + static_cast<double>(ValidDuration);
    }

    float GetRemainingTime(const double CurrentTime) const
    {
        return static_cast<float>(FMath::Max(0.f, static_cast<float>(GetExpireAt() - CurrentTime)));
    }
};

USTRUCT(BlueprintType)
struct WUWA_API FWuwaInputIntent
{
    GENERATED_BODY()

    FVector2D MoveIntent = FVector2D::ZeroVector;
    FVector2D LookIntent = FVector2D::ZeroVector;

    bool bJumpPressed = false;
    bool bJumpReleased = false;
    bool bSprintPressed = false;

    bool bDodgePressed = false;
    bool bAttackPressed = false;
    bool bGrapplePressed = false;
    bool bLockTargetPressed = false;

    float SwitchTargetAxis = 0.0f;

    void ResetTransientInputs()
    {
        LookIntent = FVector2D::ZeroVector;

        bJumpPressed = false;
        bJumpReleased = false;
        bSprintPressed = false;
        bDodgePressed = false;
        bAttackPressed = false;
        bGrapplePressed = false;
        bLockTargetPressed = false;
        SwitchTargetAxis = 0.0f;
    }
};
