#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "WuwaMessageTypes.generated.h"

/** 消息在单帧内允许执行的确定性阶段 */
UENUM()
enum class EWuwaMessagePhase : uint8
{
	/** 清理先前引擎回调产生的事实 */
	FactDrain,

	/** 更新连续输入和只读状态快照 */
	Snapshot,

	/** 无副作用地解析离散意图 */
	Resolve,

	/** 执行非独占领域命令 */
	Immediate,

	/** 提交独占 Action 命令 */
	Exclusive,

	/** 把未被前序阶段消费的输入提交给 GAS */
	AbilityInput,

	/** 收口本帧事实并释放运行资源 */
	Finalize
};

/** 所有角色级消息共享的来源与顺序信息 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaMessageHeader
{
	GENERATED_BODY()

	/** 产生消息的游戏帧编号 */
	UPROPERTY(BlueprintReadOnly, Category = "Message")
	int64 FrameNumber = 0;

	/** 当前发送者范围内的单调递增序号 */
	UPROPERTY(BlueprintReadOnly, Category = "Message")
	int32 Sequence = 0;

	/** 消息产生时使用的 World 时间 */
	UPROPERTY(BlueprintReadOnly, Category = "Message")
	double CreatedAt = 0.0;

	/** 来源对象使用弱引用，消息不能延长其生命周期 */
	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> SourceObject;

	/** @return 消息头是否包含有效顺序和时间 */
	bool IsValid() const
	{
		return FrameNumber > 0 && Sequence > 0 && CreatedAt >= 0.0;
	}
};

/** 角色命令的高层处理状态 */
UENUM(BlueprintType)
enum class EWuwaCommandDispatchStatus : uint8
{
	/** 命令已由唯一处理者接受 */
	Handled,

	/** 当前没有注册处理者 */
	Unhandled,

	/** 命令或上下文无效 */
	Rejected
};

/** 角色消息入口返回的可诊断结果 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCommandDispatchResult
{
	GENERATED_BODY()

	/** 命令最终处理状态 */
	UPROPERTY(BlueprintReadOnly, Category = "Message")
	EWuwaCommandDispatchStatus Status = EWuwaCommandDispatchStatus::Unhandled;

	/** 被处理或拒绝的语义标签 */
	UPROPERTY(BlueprintReadOnly, Category = "Message")
	FGameplayTag MessageTag;

	/** @return 命令是否已由处理者接受 */
	bool WasHandled() const
	{
		return Status == EWuwaCommandDispatchStatus::Handled;
	}
};
