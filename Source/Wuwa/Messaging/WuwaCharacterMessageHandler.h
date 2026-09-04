#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Input/WuwaInputTypes.h"
#include "Messaging/WuwaMessageTypes.h"
#include "WuwaCharacterMessageHandler.generated.h"

/** 标记可接收角色即时命令的 UObject */
UINTERFACE(MinimalAPI)
class UWuwaCharacterMessageHandler : public UInterface
{
	GENERATED_BODY()
};

/** 即时领域命令的窄处理端口 */
class WUWA_API IWuwaCharacterMessageHandler
{
	GENERATED_BODY()

public:
	/**
     * 返回该处理者独占认领的输入标签
     * @param OutInputTags	接收输入标签集合
     */
	virtual void GatherHandledInputTags(FGameplayTagContainer& OutInputTags) const = 0;

	/**
     * 处理一条不可变即时命令
     * @param Command	输入边沿命令
     * @param InputFrame	命令所属的完整输入帧
     * @return 结构化处理结果
     */
	virtual FWuwaCommandDispatchResult HandleInputCommand(const FWuwaInputCommand& Command,
	                                                      const FWuwaInputFrame& InputFrame) = 0;
};
