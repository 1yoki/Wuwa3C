#pragma once

#include "CoreMinimal.h"
#include "WuwaMovementTypes.generated.h"

// 描述本次跳跃来源
UENUM(BlueprintType)
enum class EWuwaJumpType : uint8
{
    None UMETA(DisplayName = "None"),
    Ground UMETA(DisplayName = "地面跳跃"),
    Coyote UMETA(DisplayName = "土狼跳跃"),
    AirSprint UMETA(DisplayName = "空中冲刺跳跃")
};

// 描述落地行为需要使用的表现类型。
UENUM(BlueprintType)
enum class EWuwaLandingType : uint8
{
    None UMETA(DisplayName = "尚未落地"),
    Light UMETA(DisplayName = "普通落地"),
    Heavy UMETA(DisplayName = "下落攻击落地")
};

// 描述本次落地由什么行为触发。
UENUM(BlueprintType)
enum class EWuwaLandingSource : uint8
{
    Normal UMETA(DisplayName = "普通空中"),
    PlungeAttack UMETA(DisplayName = "下落攻击")
};

USTRUCT(BlueprintType)
struct WUWA_API FWuwaAirActionState
{
    GENERATED_BODY()

    // 记录当前滞空期间已经执行的跳跃次数。
    UPROPERTY(BlueprintReadOnly, Category = "Air Action")
    int32 JumpCount = 0;

    // 为后续空中攻击次数限制预留。
    UPROPERTY(BlueprintReadOnly, Category = "Air Action")
    int32 AirAttackCount = 0;

    // 为后续空中闪避次数限制预留。
    UPROPERTY(BlueprintReadOnly, Category = "Air Action")
    int32 AirDodgeCount = 0;

    // 为后续钩锁次数限制预留。
    UPROPERTY(BlueprintReadOnly, Category = "Air Action")
    int32 GrappleCount = 0;

    // 保证空中 Sprint 每次滞空最多生效一次。
    UPROPERTY(BlueprintReadOnly, Category = "Air Action")
    bool bAirSprintConsumed = false;

    // 保存最后一次接地时间，用于判断土狼时间。
    double LastGroundedTime = -1.0;

    // 保存跳跃缓存的过期时间，负值表示没有缓存。
    double BufferedJumpExpireAt = -1.0;

    // 保存本次下落开始高度，用于计算下落距离。
    float FallStartHeight = 0.f;

    // 只重置空中次数，不清除尚未消费的跳跃缓存。
    void ResetBudgetsOnLanding()
    {
        JumpCount = 0;
        AirAttackCount = 0;
        AirDodgeCount = 0;
        GrappleCount = 0;
        bAirSprintConsumed = false;
    }
};

USTRUCT(BlueprintType)
struct WUWA_API FWuwaLandingEvent
{
    GENERATED_BODY()

    // 保存角色接触地面前的完整速度
    UPROPERTY(BlueprintReadOnly, Category = "Landing")
    FVector ImpactVelocity = FVector::ZeroVector;

    // 保存向下冲击速度的绝对值，用于反馈强度和调试。
    UPROPERTY(BlueprintReadOnly, Category = "Landing")
    float ImpactSpeed = 0.f;

    // 保存本次滞空期间的最大下落距离。
    UPROPERTY(BlueprintReadOnly, Category = "Landing")
    float FallDistance = 0.f;

    // 保存触发本次落地的 Gameplay 行为。
    UPROPERTY(BlueprintReadOnly, Category = "Landing")
    EWuwaLandingSource LandingSource = EWuwaLandingSource::Normal;

    // 保存本次落地需要播放的表现类型。
    UPROPERTY(BlueprintReadOnly, Category = "Landing")
    EWuwaLandingType LandingType = EWuwaLandingType::None;

    // 每次真实落地递增，用于识别新事件。
    UPROPERTY(BlueprintReadOnly, Category = "Landing")
    int32 Sequence = 0;
};

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

    // 提供跳跃和下落动画需要的垂直速度。
    UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Air")
    float VerticalVelocity = 0.f;

    // 提供当前滞空期间的跳跃次数。
    UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Air")
    int32 JumpCount = 0;

    // 提供最近一次成功跳跃的类型。
    UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Air")
    EWuwaJumpType LastJumpType = EWuwaJumpType::None;

    // 每次成功起跳递增，用于识别空中的再次起跳。
    UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Air")
    int32 JumpSequence = 0;

    // 提供最近一次真实落地的类型。
    UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Landing")
    EWuwaLandingType LastLandingType = EWuwaLandingType::None;

    // 保存最近一次落地的向下冲击速度。
    UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Landing")
    float LastLandingVelocity = 0.f;

    // 保存最近一次落地前的最大下落距离。
    UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Landing")
    float LastFallDistance = 0.f;

    // 每次真实落地递增，用于避免重复播放落地表现。
    UPROPERTY(BlueprintReadOnly, Category = "Locomotion|Landing")
    int32 LandingSequence = 0;
};