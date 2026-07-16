#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Input/WuwaInputTypes.h"
#include "WuwaInputBufferComponent.generated.h"

UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaInputBufferComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWuwaInputBufferComponent();

    // 将有效命令追加到FIFO队列中，返回是否成功入队。
    bool Push(const FWuwaInputCommand &Command);

    // 查看队首有效命令，但不移除它
    bool Peek(double CurrentTime, FWuwaInputCommand &OutCommand);

    // 只消费与Sequence匹配的队首命令，返回是否成功消费。
    bool Consume(double CurrentTime, uint32 Sequence, FWuwaInputCommand &OutCommand);

    // 删除所有过期命令
    int32 Expire(double CurrentTime);

    // 清除指定输入标签的全部命令。
    int32 ClearByTag(const FGameplayTag &InputTag);

    // 返回队列副本，外部不能修改内部数组。
    TArray<FWuwaInputCommand> GetDebugSnapshot(double CurrentTime);

    int32 GetBufferedCommandCount(double CurrentTime)
    {
        // 先清除过期命令
        Expire(CurrentTime);
        return BufferedCommands.Num();
    }

private:
    bool IsExpired(const FWuwaInputCommand &Command, double CurrentTime) const;

    // Input Buffer 是命令队列的唯一拥有者，外部不应直接修改队列。
    UPROPERTY(Transient)
    TArray<FWuwaInputCommand> BufferedCommands;

    // 防止异常输入让队列无限增长。
    UPROPERTY(EditDefaultsOnly, Category = "Input Buffer", meta = (ClampMin = "1"))
    int32 MaxBufferedCommands = 16;

    // 已接受的Sequence 不允许再次入队，避免重复消费。
    uint32 LastAcceptedSequence = 0;
};