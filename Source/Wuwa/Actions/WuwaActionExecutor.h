#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Actions/WuwaActionTypes.h"
#include "WuwaActionExecutor.generated.h"

class UWuwaActionDefinition;

// 提供接口，实现速度、Root Motion、Montage 等具体执行
UINTERFACE(MinimalAPI)
class UWuwaActionExecutor : public UInterface
{
    GENERATED_BODY()
};

class WUWA_API IWuwaActionExecutor
{
    GENERATED_BODY()

public:
    // 判断该 Executor 是否支持指定动作
    virtual bool SupportsAction(const UWuwaActionDefinition &Definition) const = 0;

    // 检查 Executor 自己拥有的运行条件
    virtual bool CanStartAction(const FWuwaActionRequest &Request, EWuwaActionRejectionReason &OutReason) const = 0;

    // 执行具体 Gameplay 行为
    virtual bool StartAction(const FWuwaActionRequest &Request) = 0;

    // 清理由该 Executor 创建的运行资源
    virtual void EndAction(const FGameplayTag &ActionTag, EWuwaActionEndReason EndReason) = 0;
};