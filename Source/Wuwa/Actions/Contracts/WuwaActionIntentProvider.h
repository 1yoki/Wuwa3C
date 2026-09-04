#pragma once

#include "CoreMinimal.h"
#include "Actions/Contracts/WuwaActionMessages.h"
#include "Input/WuwaInputTypes.h"
#include "UObject/Interface.h"
#include "WuwaActionIntentProvider.generated.h"

/** Action Resolve 阶段的无副作用领域入口 */
UINTERFACE(MinimalAPI)
class UWuwaActionIntentProvider : public UInterface
{
	GENERATED_BODY()
};

/** 把输入边沿解析为唯一 Action Intent 的领域契约 */
class WUWA_API IWuwaActionIntentProvider
{
	GENERATED_BODY()

public:
	/**
     * 收集当前 Provider 唯一认领的输入标签
     * @param OutInputTags	接收 exact 输入标签
     */
	virtual void GatherHandledInputTags(FGameplayTagContainer& OutInputTags) const = 0;

	/**
     * 无副作用地解析一次输入边沿
     * @param Command	输入边沿命令
     * @param Snapshot	当前只读角色快照
     * @return 状态、可选 Intent 与可选诊断
     */
	virtual FWuwaActionIntentResolution ResolveIntent(const FWuwaInputCommand& Command,
	                                                  const FWuwaActionResolutionSnapshot& Snapshot) const = 0;
};
