#pragma once

#include "CoreMinimal.h"
#include "Actions/Contracts/WuwaActionMessages.h"
#include "UObject/Interface.h"
#include "WuwaActionCapability.generated.h"

/** 可由 Coordinator 按语义标签发现的 Action 执行能力 */
UINTERFACE(MinimalAPI)
class UWuwaActionCapability : public UInterface
{
	GENERATED_BODY()
};

/** Action 执行原语的事务边界 */
class WUWA_API IWuwaActionCapability
{
	GENERATED_BODY()

public:
	/** @return 唯一标识该执行能力的 GameplayTag */
	virtual FGameplayTag GetActionCapabilityTag() const = 0;

	/** @return Commit 的稳定顺序，数值较小者先执行 */
	virtual int32 GetActionCapabilityCommitOrder() const = 0;

	/**
     * 无副作用地验证本能力能否提交请求
     * @param Message Action 准备消息
     * @return 准备结果
     */
	virtual FWuwaActionCapabilityResult PrepareAction(const FWuwaActionPrepareMessage& Message) const = 0;

	/**
     * 为已经通过全部 Prepare 的 Action 创建运行资源
     * @param Message Action 提交消息
     * @return 提交结果
     */
	virtual FWuwaActionCapabilityResult CommitAction(const FWuwaActionCommitMessage& Message) = 0;

	/**
     * 回滚本次 Commit 创建的全部资源
     * @param Handle 需要回滚的 Action Handle
     */
	virtual void RollbackAction(const FWuwaActionHandle& Handle) = 0;

	/**
     * 按统一结束原因停止活动资源
     * @param Message Action 停止消息
     */
	virtual void StopAction(const FWuwaActionStopMessage& Message) = 0;

	/**
     * 接收当前 Action 的有序事实消息
     * @param Message Action 事件
     * @return 本能力请求的结束原因，None 表示继续
     */
	virtual EWuwaActionEndReason HandleActionEvent(const FWuwaActionEventMessage& Message) = 0;

	/**
     * 在全部能力和状态标签释放后接收最终事实
     * @param Message Action 已完成消息
     */
	virtual void HandleActionFinalized(const FWuwaActionFinalizedMessage& Message) = 0;
};
