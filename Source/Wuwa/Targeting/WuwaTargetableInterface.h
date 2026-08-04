#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "WuwaTargetableInterface.generated.h"

class AActor;

/**
 * Targeting 领域依赖的最小目标契约。
 * 具体对象自行决定目标点以及对指定请求者是否可选，Targeting 不读取敌人、阵营或属性实现。
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UWuwaTargetableInterface : public UInterface
{
    GENERATED_BODY()
};

class WUWA_API IWuwaTargetableInterface
{
    GENERATED_BODY()

public:
    /** 返回当前目标点的实时世界坐标。 */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wuwa|Targeting")
    FVector GetTargetingPoint() const;

    virtual FVector GetTargetingPoint_Implementation() const
    {
        // 未实现契约的对象不能凭默认 Actor 位置被静默选中。
        return FVector::ZeroVector;
    }

    /**
     * 返回本对象当前是否允许被 Requester 选择。
     * 存活、阵营和临时不可锁定等对象自身规则统一由实现者回答。
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wuwa|Targeting")
    bool CanBeTargetedBy(AActor *Requester) const;

    virtual bool CanBeTargetedBy_Implementation(AActor *Requester) const
    {
        (void)Requester;
        // 安全默认值为不可选，要求蓝图或 C++ 实现者明确授权。
        return false;
    }
};
