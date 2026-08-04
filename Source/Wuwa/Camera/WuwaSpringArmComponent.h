#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpringArmComponent.h"

#include "WuwaSpringArmComponent.generated.h"

/*
 * 保留 USpringArmComponent 原生 Probe/Sweep，只扩展碰撞结果恢复。
 * 不拥有 Desired Rig，也不读取 Targeting、Movement 或 Camera Mode。
 */

UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaSpringArmComponent : public USpringArmComponent
{
    GENERATED_BODY()

public:
    /*
     * 由 Character Composition Root 从 Camera Profile 注入。
     * 配置变化会清除旧恢复距离，避免跨配置残留。
     */
    bool ConfigureCollision(const float InProbeSize, ECollisionChannel InProbeChannel, const float InRecoveryInterpSpeed);

    float GetCollisionRecoveryInterpSpeed() const
    {
        return CollisionRecoveryInterpSpeed;
    }

protected:
    virtual void OnRegister() override;
    virtual void OnUnregister() override;

    /*
     * 引擎已经完成唯一 Sweep。
     * 本函数只能混合原生命中结果，禁止再次查询 World。
     */

    virtual FVector BlendLocations(const FVector &DesiredArmLocation, const FVector &TraceHitLocation, bool bHitSomething, float DeltaTime) override;

private:
    void ResetCollisionRecovery();

    UPROPERTY(Transient)
    float CollisionRecoveryInterpSpeed = 8.f;

    // 相对当前 Arm Origin 的距离，不缓存旧世界坐标。
    UPROPERTY(Transient)
    float CurrentResolvedDistance = 0.f;

    UPROPERTY(Transient)
    bool bHasResolvedCollisionDistance = false;
};