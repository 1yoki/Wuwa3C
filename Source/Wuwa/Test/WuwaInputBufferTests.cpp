#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/WuwaGameplayTags.h"
#include "Input/WuwaInputBufferComponent.h"

namespace
{
    FWuwaInputCommand MakeInputCommand(
        const FGameplayTag &InputTag,
        const uint32 Sequence,
        const double PressedAt,
        const float ValidDuration = 1.f)
    {
        FWuwaInputCommand Command;

        Command.InputTag = InputTag;
        Command.Sequence = Sequence;
        Command.PressedAt = PressedAt;
        Command.ValidDuration = ValidDuration;

        return Command;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWuwaInputBufferTest,
    "Wuwa.Input.Buffer.FifoLifetime",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FWuwaInputBufferTest::RunTest(
    const FString &Parameters)
{
    UWuwaInputBufferComponent *Buffer =
        NewObject<UWuwaInputBufferComponent>();

    if (!TestNotNull(
            TEXT("Input Buffer 应成功创建"),
            Buffer))
    {
        return false;
    }

    const FWuwaInputCommand First =
        MakeInputCommand(
            WuwaGameplayTags::Input_Sprint,
            1,
            0.0);

    const FWuwaInputCommand Second =
        MakeInputCommand(
            WuwaGameplayTags::Input_Attack,
            2,
            0.1);

    TestTrue(
        TEXT("第一条命令应成功入队"),
        Buffer->Push(First));

    TestTrue(
        TEXT("第二条命令应成功入队"),
        Buffer->Push(Second));

    FWuwaInputCommand PeekedCommand;

    TestTrue(
        TEXT("Peek 应返回有效队首"),
        Buffer->Peek(0.2, PeekedCommand));

    TestEqual(
        TEXT("FIFO 队首应为第一条命令"),
        PeekedCommand.Sequence,
        static_cast<uint32>(1));

    FWuwaInputCommand ConsumedCommand;

    TestTrue(
        TEXT("应成功消费第一条命令"),
        Buffer->Consume(
            0.2,
            1,
            ConsumedCommand));

    TestEqual(
        TEXT("消费结果应保持原 Sequence"),
        ConsumedCommand.Sequence,
        static_cast<uint32>(1));

    TestTrue(
        TEXT("应成功消费第二条命令"),
        Buffer->Consume(
            0.2,
            2,
            ConsumedCommand));

    TestFalse(
        TEXT("已消费命令不能再次消费"),
        Buffer->Consume(
            0.2,
            2,
            ConsumedCommand));

    AddExpectedError(
        TEXT("拒绝入队重复命令"),
        EAutomationExpectedErrorFlags::Contains,
        1);

    TestFalse(
        TEXT("重复 Sequence 不能再次入队"),
        Buffer->Push(Second));

    const FWuwaInputCommand ExpiringCommand =
        MakeInputCommand(
            WuwaGameplayTags::Input_Dodge,
            3,
            2.0,
            0.25f);

    TestTrue(
        TEXT("过期测试命令应成功入队"),
        Buffer->Push(ExpiringCommand));

    TestEqual(
        TEXT("到达边界时间时命令应过期"),
        Buffer->Expire(2.25),
        1);

    const FWuwaInputCommand AttackCommand =
        MakeInputCommand(
            WuwaGameplayTags::Input_Attack,
            4,
            3.0);

    const FWuwaInputCommand SprintCommand =
        MakeInputCommand(
            WuwaGameplayTags::Input_Sprint,
            5,
            3.1);

    Buffer->Push(AttackCommand);
    Buffer->Push(SprintCommand);

    TestEqual(
        TEXT("ClearByTag 应只删除精确标签"),
        Buffer->ClearByTag(
            WuwaGameplayTags::Input_Sprint),
        1);

    TestTrue(
        TEXT("未匹配命令应继续保留"),
        Buffer->Peek(3.2, PeekedCommand));

    TestTrue(
        TEXT("保留命令应为 Attack"),
        PeekedCommand.InputTag ==
            WuwaGameplayTags::Input_Attack);

    TArray<FWuwaInputCommand> Snapshot =
        Buffer->GetDebugSnapshot(3.2);

    Snapshot.Reset();

    TestEqual(
        TEXT("修改 Debug 副本不能改变真实队列"),
        Buffer->GetBufferedCommandCount(3.2),
        1);

    UWuwaInputBufferComponent *CapacityBuffer =
        NewObject<UWuwaInputBufferComponent>();

    for (uint32 Sequence = 1;
         Sequence <= 16;
         ++Sequence)
    {
        const FWuwaInputCommand Command =
            MakeInputCommand(
                WuwaGameplayTags::Input_Attack,
                Sequence,
                Sequence * 0.01,
                10.f);

        TestTrue(
            TEXT("容量范围内命令应成功入队"),
            CapacityBuffer->Push(Command));
    }

    AddExpectedError(
        TEXT("队列已满"),
        EAutomationExpectedErrorFlags::Contains,
        1);

    const FWuwaInputCommand OverflowCommand =
        MakeInputCommand(
            WuwaGameplayTags::Input_Attack,
            17,
            0.17,
            10.f);

    TestFalse(
        TEXT("超过容量的命令应被拒绝"),
        CapacityBuffer->Push(OverflowCommand));

    return true;
}

#endif