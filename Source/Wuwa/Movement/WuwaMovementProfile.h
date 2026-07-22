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

    // Dash 中段衔接后，水平速度向普通 RunSpeed 收敛的专用减速度。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Sprint", meta = (ClampMin = "0.01", Units = "cm/s^2"))
    float SprintRunDeceleration = 2000.f;

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
    float AirControl = 0.2f;

    // 普通跳跃的初始向上速度。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Jump", meta = (ClampMin = "0.0", Units = "cm/s"))
    float JumpZVelocity = 500.f;

    // 配置空中二段跳的初始向上速度。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Jump", meta = (ClampMin = "0.0", Units = "cm/s"))
    float DoubleJumpZVelocity = 650.f;

    // 配置空中二段跳的水平推进速度。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Jump", meta = (ClampMin = "0.0", Units = "cm/s"))
    float DoubleJumpForwardSpeed = 400.f;

    // 后空翻的初始向上速度。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Jump|Backflip", meta = (ClampMin = "0.01", Units = "cm/s"))
    float BackflipZVelocity = 650.f;

    // 后空翻的水平推进速度。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Jump|Backflip", meta = (ClampMin = "0.01", Units = "cm/s"))
    float BackflipBackwardSpeed = 400.f;

    // 限制每次滞空允许执行的跳跃次数。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Jump", meta = (ClampMin = "1", UIMax = "5"))
    int32 MaxJumpCount = 2;

    // 允许角色离开边缘后短时间内继续起跳。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Jump", meta = (ClampMin = "0.0", Units = "s"))
    float CoyoteTime = 0.15f;

    // 允许落地前输入的跳跃请求短暂保留。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Jump", meta = (ClampMin = "0.0", Units = "s"))
    float JumpBufferTime = 0.10f;

    // 配置普通跳跃和空中移动使用的重力倍率。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Air", meta = (ClampMin = "0.1", UIMax = "3.0"))
    float GravityScale = 1.f;

    // 向下冲击速度达到该值时记为重落地。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Landing", meta = (ClampMin = "0.01", Units = "cm/s"))
    float HeavyLandingVelocityThreshold = 900.f;

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
