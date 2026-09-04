#pragma once

#include "CoreMinimal.h"
#include "Actions/Contracts/WuwaActionMessages.h"
#include "WuwaActionQueue.generated.h"

/** 保存已解析请求的严格 FIFO，不在重试时重新读取世界状态 */
USTRUCT()
struct WUWA_API FWuwaActionQueue
{
	GENERATED_BODY()

	/**
     * 将唯一请求追加到队尾
     * @param Intent 已解析意图
     * @param MaxDepth 最大队列深度
     * @param OutReason 接收拒绝原因
     * @return 是否入队
     */
	bool Enqueue(const FWuwaResolvedActionIntent& Intent, int32 MaxDepth, EWuwaActionRejectionReason& OutReason);

	/**
     * 移除全部失效请求
     * @param CurrentTime 当前 World 时间
     * @param OutExpired 接收被移除的请求
     * @return 移除数量
     */
	int32 Expire(double CurrentTime, TArray<FWuwaResolvedActionIntent>& OutExpired);

	/** @return 队首请求，空队列返回空指针 */
	const FWuwaResolvedActionIntent* Peek() const;

	/**
     * 弹出队首请求
     * @param OutIntent 接收请求
     * @return 是否成功弹出
     */
	bool PopFront(FWuwaResolvedActionIntent& OutIntent);

	/**
     * 清空全部请求和来源序号记录
     * @param OutRemoved 接收被移除的请求
     */
	void Reset(TArray<FWuwaResolvedActionIntent>& OutRemoved);

	/** @return 当前请求数量 */
	int32 Num() const
	{
		return Items.Num();
	}

	/** @return 当前 Items 已保留容量 */
	int32 Capacity() const
	{
		return Items.Max();
	}

private:
	/** 按接受顺序保存的不可变请求 */
	UPROPERTY()
	TArray<FWuwaResolvedActionIntent> Items;

	/** 每个有效发送者独立维护最后接收序号，符合 MessageHeader 的发送者内序列契约 */
	TMap<TWeakObjectPtr<UObject>, int32> LastAcceptedSequenceBySource;

	/** 无 SourceObject 的测试或系统消息共享独立序列域 */
	int32 LastAnonymousAcceptedSequence = 0;
};
