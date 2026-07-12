#pragma once

#include "CoreMinimal.h"
#include "WuwaInputTypes.generated.h"

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