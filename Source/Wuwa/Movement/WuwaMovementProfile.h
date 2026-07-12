#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WuwaMovementProfile.generated.h"

UCLASS(BlueprintType)
class WUWA_API UWuwaMovementProfile : public UDataAsset
{
    GENERATED_BODY()

public:
    // 地面低速移动上限
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|speed", meta = (ClampMin = "0.0", Units = "cm/s"))
    float WalkSpeed = 250.f;

    // 地面常规移动上限
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|speed", meta = (ClampMin = "0.0", Units = "cm/s"))
    float RunSpeed = 500.f;

    // 有效冲刺移动上限
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|speed", meta = (ClampMin = "0.0", Units = "cm/s"))
    float SprintSpeed = 700.f;

    // 加速度
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Acceleration", meta = (ClampMin = "0.0", Units = "cm/s^2"))
    float MaxAcceleration = 2200.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Breaking", meta = (ClampMin = "0.0", Units = "cm/s^2"))
    float BrakingDecelerationWalking = 1500.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Breaking", meta = (ClampMin = "0.0", Units = "cm/s^2"))
    float BrakingDecelerationFalling = 1500.f;

    // 地面摩擦力
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Breaking", meta = (ClampMin = "0.0"))
    float GroundFriction = 8.f;

    // 摩擦力系数
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Breaking", meta = (ClampMin = "0.0"))
    float BrakingFrictionFactor = 2.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Rotation")
    FRotator RotationRate = FRotator(0.f, 720.f, 0.f);

    // 空中控制能力，0 表示完全无法控制，1 表示完全可控。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Air", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AirControl = 0.4f;

    // 输入量达到该值后由走路切换为跑步
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Input", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AnalogRunThreshold = 0.5f;

    // 拥有其中任意一个标签时，角色将无法冲刺。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Sprint")
    FGameplayTagContainer SprintBlockedTags;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};