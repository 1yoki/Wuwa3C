#pragma once

#include "CoreMinimal.h"
#include "Actions/Contracts/WuwaActionMessages.h"
#include "Actions/Data/WuwaActionRuleSet.h"

/** 无状态输入规则解析器 */
class WUWA_API FWuwaActionResolver
{
public:
	/**
     * 将输入边沿和只读快照解析为不可变 Action 意图
     * @param Command 输入边沿命令
     * @param Snapshot 当前只读角色快照
     * @param RuleSet 当前角色使用的解析规则
     * @param OutIntent 接收唯一解析结果
     * @return 解析状态
     */
	static EWuwaActionResolutionStatus Resolve(const FWuwaInputCommand& Command,
	                                           const FWuwaActionResolutionSnapshot& Snapshot,
	                                           const UWuwaActionRuleSet* RuleSet,
	                                           FWuwaResolvedActionIntent& OutIntent);

private:
	/**
     * 计算规则要求的水平世界方向
     * @param Command 输入命令
     * @param Snapshot 只读角色快照
     * @param Policy 世界方向策略
     * @return 归一化水平世界方向
     */
	static FVector ResolveWorldDirection(const FWuwaInputCommand& Command,
	                                     const FWuwaActionResolutionSnapshot& Snapshot,
	                                     EWuwaActionWorldDirectionPolicy Policy);
};
