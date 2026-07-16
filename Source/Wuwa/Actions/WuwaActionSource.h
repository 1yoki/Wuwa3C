#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Actions/WuwaActionTypes.h"
#include "Input/WuwaInputTypes.h"
#include "WuwaActionSource.generated.h"

UINTERFACE(MinimalAPI)
class UWuwaActionSource : public UInterface
{
    GENERATED_BODY()
};

class WUWA_API IWuwaActionSource
{
    GENERATED_BODY()

public:
    // 将不可变输入命令转换为动作请求。
    virtual bool BuildActionRequest(const FWuwaInputCommand &Command, FWuwaActionRequest &OutRequest) const = 0;
};