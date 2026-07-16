#include "Input/WuwaInputBufferComponent.h"

#include "Wuwa.h"

UWuwaInputBufferComponent::UWuwaInputBufferComponent()
{
    // 命令只在请求和查询时处理，不需要每帧 Tick。
    PrimaryComponentTick.bCanEverTick = false;
}

bool UWuwaInputBufferComponent::IsExpired(const FWuwaInputCommand &Command, double CurrentTime) const
{
    return Command.GetExpireAt() <= CurrentTime;
}

bool UWuwaInputBufferComponent::Push(const FWuwaInputCommand &Command)
{
    if (!Command.IsValid())
    {
        UE_LOG(LogWuwa, Warning, TEXT("拒绝入队无效命令。Owner=%s"), *GetNameSafe(GetOwner()));
        return false;
    }

    // 新命令的按下时间也是本次队列清理时间。
    Expire(Command.PressedAt);

    if (Command.Sequence <= LastAcceptedSequence)
    {
        UE_LOG(
            LogWuwa,
            Warning,
            TEXT("拒绝入队重复命令。Owner=%s, Sequence=%u, LastSequence=%u"),
            *GetNameSafe(GetOwner()),
            Command.Sequence,
            LastAcceptedSequence);

        return false;
    }

    const int32 SafeMaxCount = FMath::Max(1, MaxBufferedCommands);

    if (BufferedCommands.Num() >= SafeMaxCount)
    {
        UE_LOG(
            LogWuwa,
            Warning,
            TEXT("拒绝入队命令，队列已满。Sequence=%u"),
            Command.Sequence);

        return false;
    }
    // 只在队尾追加新命令，保证先进先出。
    BufferedCommands.Add(Command);
    LastAcceptedSequence = Command.Sequence;

    return true;
}

bool UWuwaInputBufferComponent::Peek(const double CurrentTime, FWuwaInputCommand &OutCommand)
{
    // 失败时不向调用者保留任何旧命令。
    OutCommand = FWuwaInputCommand();

    Expire(CurrentTime);

    if (BufferedCommands.Num() == 0)
    {
        return false;
    }

    OutCommand = BufferedCommands[0];
    return true;
}

bool UWuwaInputBufferComponent::Consume(const double CurrentTime, uint32 Sequence, FWuwaInputCommand &OutCommand)
{
    // 失败时不向调用者保留任何旧命令。
    OutCommand = FWuwaInputCommand();

    Expire(CurrentTime);

    if (BufferedCommands.Num() == 0)
    {
        return false;
    }

    const FWuwaInputCommand &FrontCommand = BufferedCommands[0];

    if (FrontCommand.Sequence != Sequence)
    {
        UE_LOG(
            LogWuwa,
            Warning,
            TEXT("Input Command 消费失败，Sequence 与队首不匹配。Owner=%s, Expected=%u, Actual=%u"),
            *GetNameSafe(GetOwner()),
            FrontCommand.Sequence,
            Sequence);
        return false;
    }

    OutCommand = FrontCommand;

    // 只能移除队首，禁止跳过前面的命令。
    BufferedCommands.RemoveAt(0, 1, EAllowShrinking::No);

    return true;
}

int32 UWuwaInputBufferComponent::Expire(const double CurrentTime)
{
    const int32 PreviousCount = BufferedCommands.Num();

    BufferedCommands.RemoveAll([this, CurrentTime](const FWuwaInputCommand &Command)
                               { return IsExpired(Command, CurrentTime); });

    return PreviousCount - BufferedCommands.Num();
}

int32 UWuwaInputBufferComponent::ClearByTag(const FGameplayTag &InputTag)
{
    if (!InputTag.IsValid())
    {
        UE_LOG(LogWuwa, Warning, TEXT("拒绝清除无效标签。Owner=%s"), *GetNameSafe(GetOwner()));

        return 0;
    }

    const int32 PreviousCount = BufferedCommands.Num();

    // ClearByTag 只执行精确标签匹配
    BufferedCommands.RemoveAll([InputTag](const FWuwaInputCommand &Command)
                               { return Command.InputTag == InputTag; });

    return PreviousCount - BufferedCommands.Num();
}

TArray<FWuwaInputCommand> UWuwaInputBufferComponent::GetDebugSnapshot(const double CurrentTime)
{
    // Debug 只能获得清理后的队列副本
    Expire(CurrentTime);

    return BufferedCommands;
}