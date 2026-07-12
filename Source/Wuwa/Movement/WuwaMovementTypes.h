#pragma once

#include "CoreMinimal.h"
#include "WuwaMovementTypes.generated.h"

USTRUCT(BlueprintType)
struct WUWA_API FWuwaLocomotionSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
    FVector Velocity = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
    FVector Acceleration = FVector::ZeroVector;

    // 角色在平面上的速度大小
    UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
    float HorizontalSpeed = 0.f;

    // 角色在平面上的移动方向，单位为角度，范围 [-180, 180]。
    // 正值表示向右
    UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
    float Direction = 0.f;

    // 角色在平面上的输入量大小，范围 [0, 1]。
    UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
    float InputMagnitude = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
    bool bIsMovingOnGround = false;

    UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
    bool bIsFalling = false;

    UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
    bool bIsSprinting = false;
};