#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Actions/WuwaActionDefinition.h"
#include "Actions/WuwaActionRouterComponent.h"
#include "Core/WuwaGameplayTags.h"
#include "Core/WuwaStateTagComponent.h"
#include "Input/WuwaInputBufferComponent.h"
#include "Test/WuwaActionTestHarness.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    struct FWuwaRouterFixture
    {
        UWuwaInputBufferComponent *Buffer =
            NewObject<UWuwaInputBufferComponent>();

        UWuwaStateTagComponent *StateTags =
            NewObject<UWuwaStateTagComponent>();

        UWuwaActionRouterComponent *Router =
            NewObject<UWuwaActionRouterComponent>();

        UWuwaActionTestHarness *Harness =
            NewObject<UWuwaActionTestHarness>();

        bool bInitialized = false;
        bool bExecutorRegistered = false;

        FWuwaRouterFixture()
        {
            bInitialized = Router->Initialize(Buffer, StateTags);
            bExecutorRegistered = Router->RegisterExecutor(Harness);
        }
    };

    UWuwaActionDefinition *MakeDefinition(
        const FGameplayTag &ActionTag,
        const int32 Priority,
        const FGameplayTag &GrantedTag = FGameplayTag())
    {
        UWuwaActionDefinition *Definition =
            NewObject<UWuwaActionDefinition>();

        Definition->ActionTag = ActionTag;
        Definition->Priority = Priority;
        Definition->BufferTime = 0.5f;
        Definition->MovementPolicy = EWuwaActionMovementPolicy::None;

        if (GrantedTag.IsValid())
        {
            Definition->GrantedTags.AddTag(GrantedTag);
        }

        return Definition;
    }

    FWuwaActionRequest MakeRequest(
        UWuwaActionDefinition *Definition,
        const uint32 Sequence)
    {
        FWuwaActionRequest Request;

        Request.ActionTag = Definition
                                ? Definition->ActionTag
                                : FGameplayTag();
        Request.Definition = Definition;
        Request.SourceInputSequence = Sequence;

        Request.Context.FacingDirection = FVector::ForwardVector;
        Request.Context.MovementMode = MOVE_Walking;
        Request.Context.SourceInputSequence = Sequence;

        return Request;
    }

    FWuwaInputCommand MakeCommand(
        const FGameplayTag &InputTag,
        const uint32 Sequence)
    {
        FWuwaInputCommand Command;

        Command.InputTag = InputTag;
        Command.PressedAt = 0.0;
        Command.ValidDuration = 1.f;
        Command.Direction = FVector2D(0.f, 1.f);
        Command.Sequence = Sequence;

        return Command;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWuwaActionRouterAdmissionLifecycleTest,
    "Wuwa.Action.Router.AdmissionLifecycle",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FWuwaActionRouterAdmissionLifecycleTest::RunTest(
    const FString &Parameters)
{
    FWuwaRouterFixture Fixture;

    TestTrue(TEXT("Router 应成功初始化"), Fixture.bInitialized);
    TestTrue(TEXT("Executor 应成功注册"), Fixture.bExecutorRegistered);

    const FWuwaActionResult InvalidResult =
        Fixture.Router->Request(FWuwaActionRequest());

    TestTrue(
        TEXT("无效请求应返回 InvalidDefinition"),
        InvalidResult.RejectionReason ==
            EWuwaActionRejectionReason::InvalidDefinition);

    UWuwaActionDefinition *RequiredDefinition =
        MakeDefinition(
            WuwaGameplayTags::Action_Movement_Dash_Forward,
            5);

    RequiredDefinition->RequiredTags.AddTag(
        WuwaGameplayTags::State_Locomotion_Sprinting);
    Fixture.Harness->SupportedDefinitions.Add(RequiredDefinition);

    const FWuwaActionResult RequiredResult =
        Fixture.Router->Request(MakeRequest(RequiredDefinition, 1));

    TestTrue(
        TEXT("缺少必需标签应被拒绝"),
        RequiredResult.RejectionReason ==
            EWuwaActionRejectionReason::MissingRequiredTag);

    UWuwaActionDefinition *BlockedDefinition =
        MakeDefinition(
            WuwaGameplayTags::Action_Movement_Dash_Forward,
            5);

    BlockedDefinition->BlockedTags.AddTag(
        WuwaGameplayTags::Block_Input_Move);
    Fixture.Harness->SupportedDefinitions.Add(BlockedDefinition);

    FWuwaStateTagHandle BlockHandle =
        Fixture.StateTags->AcquireTag(WuwaGameplayTags::Block_Input_Move);

    const FWuwaActionResult BlockedResult =
        Fixture.Router->Request(MakeRequest(BlockedDefinition, 2));

    TestTrue(
        TEXT("命中阻止标签应被拒绝"),
        BlockedResult.RejectionReason ==
            EWuwaActionRejectionReason::BlockedByTag);

    Fixture.StateTags->ReleaseTag(BlockHandle);

    UWuwaInputBufferComponent *NoExecutorBuffer =
        NewObject<UWuwaInputBufferComponent>();
    UWuwaStateTagComponent *NoExecutorTags =
        NewObject<UWuwaStateTagComponent>();
    UWuwaActionRouterComponent *NoExecutorRouter =
        NewObject<UWuwaActionRouterComponent>();

    NoExecutorRouter->Initialize(NoExecutorBuffer, NoExecutorTags);

    const FWuwaActionResult NoExecutorResult =
        NoExecutorRouter->Request(MakeRequest(BlockedDefinition, 3));

    TestTrue(
        TEXT("没有 Executor 时应明确拒绝"),
        NoExecutorResult.RejectionReason ==
            EWuwaActionRejectionReason::NoExecutor);

    UWuwaActionDefinition *CurrentDefinition =
        MakeDefinition(
            WuwaGameplayTags::Action_Movement_Dash_Forward,
            10,
            WuwaGameplayTags::State_Action_Dash);

    UWuwaActionDefinition *LowDefinition =
        MakeDefinition(
            WuwaGameplayTags::Action_Movement_Backstep,
            5);

    UWuwaActionDefinition *HighDefinition =
        MakeDefinition(
            WuwaGameplayTags::Action_Movement_Backstep,
            30,
            WuwaGameplayTags::Block_Input_Move);

    Fixture.Harness->SupportedDefinitions.Add(CurrentDefinition);
    Fixture.Harness->SupportedDefinitions.Add(LowDefinition);
    Fixture.Harness->SupportedDefinitions.Add(HighDefinition);

    const FWuwaActionResult StartedResult =
        Fixture.Router->Request(MakeRequest(CurrentDefinition, 10));

    TestTrue(TEXT("合法动作应成功开始"), StartedResult.HasStarted());
    TestTrue(TEXT("Router 应保存当前动作"), Fixture.Router->HasActiveAction());
    TestEqual(
        TEXT("开始后应取得 Granted Tag"),
        Fixture.StateTags->GetTagSourceCount(
            WuwaGameplayTags::State_Action_Dash),
        1);

    const FWuwaActionResult PriorityResult =
        Fixture.Router->Request(MakeRequest(LowDefinition, 11));

    TestTrue(
        TEXT("低优先级动作不能替换当前动作"),
        PriorityResult.RejectionReason ==
            EWuwaActionRejectionReason::Priority);

    const FWuwaActionResult CancelRuleResult =
        Fixture.Router->Request(MakeRequest(HighDefinition, 12));

    TestTrue(
        TEXT("缺少双向取消许可时应拒绝"),
        CancelRuleResult.RejectionReason ==
            EWuwaActionRejectionReason::CancellationRule);

    // 双方都允许后才能执行动作替换。
    HighDefinition->CanCancelActions.AddTag(CurrentDefinition->ActionTag);
    CurrentDefinition->CanBeCancelledBy.AddTag(HighDefinition->ActionTag);

    const FWuwaActionResult InterruptedResult =
        Fixture.Router->Request(MakeRequest(HighDefinition, 13));

    TestTrue(TEXT("高优先级双向许可动作应开始"), InterruptedResult.HasStarted());
    TestTrue(
        TEXT("旧动作应以 Interrupted 结束"),
        Fixture.Harness->LastEndReason == EWuwaActionEndReason::Interrupted);
    TestEqual(
        TEXT("旧动作标签应被释放"),
        Fixture.StateTags->GetTagSourceCount(
            WuwaGameplayTags::State_Action_Dash),
        0);
    TestEqual(
        TEXT("新动作标签应被取得"),
        Fixture.StateTags->GetTagSourceCount(
            WuwaGameplayTags::Block_Input_Move),
        1);

    TestTrue(TEXT("CancelCurrent 应成功"), Fixture.Router->CancelCurrent());
    TestTrue(
        TEXT("取消应记录 Cancelled"),
        Fixture.Router->GetLastEndReason() == EWuwaActionEndReason::Cancelled);
    TestFalse(TEXT("取消后不应存在当前动作"), Fixture.Router->HasActiveAction());
    TestEqual(
        TEXT("取消后新动作标签应释放"),
        Fixture.StateTags->GetTagSourceCount(
            WuwaGameplayTags::Block_Input_Move),
        0);

    Fixture.Harness->bStartSucceeds = false;

    const FWuwaActionResult FailedResult =
        Fixture.Router->Request(MakeRequest(CurrentDefinition, 14));

    TestTrue(
        TEXT("Executor 启动失败应被拒绝"),
        FailedResult.RejectionReason ==
            EWuwaActionRejectionReason::ExecutorFailed);
    TestTrue(
        TEXT("启动失败应记录 Failed"),
        Fixture.Router->GetLastEndReason() == EWuwaActionEndReason::Failed);
    TestEqual(
        TEXT("启动失败不能残留 Granted Tag"),
        Fixture.StateTags->GetTagSourceCount(
            WuwaGameplayTags::State_Action_Dash),
        0);

    Fixture.Harness->bStartSucceeds = true;

    Fixture.Router->Request(MakeRequest(CurrentDefinition, 15));
    TestTrue(
        TEXT("Completed 应成功结束动作"),
        Fixture.Router->FinishCurrent(EWuwaActionEndReason::Completed));
    TestTrue(
        TEXT("应记录 Completed"),
        Fixture.Router->GetLastEndReason() == EWuwaActionEndReason::Completed);

    Fixture.Router->Request(MakeRequest(CurrentDefinition, 16));
    TestTrue(
        TEXT("OwnerDestroyed 路径应成功清理"),
        Fixture.Router->FinishCurrent(EWuwaActionEndReason::OwnerDestroyed));
    TestTrue(
        TEXT("应记录 OwnerDestroyed"),
        Fixture.Router->GetLastEndReason() == EWuwaActionEndReason::OwnerDestroyed);
    TestEqual(
        TEXT("OwnerDestroyed 后不能残留标签"),
        Fixture.StateTags->GetTagSourceCount(
            WuwaGameplayTags::State_Action_Dash),
        0);

    const int32 EndCountBeforeDuplicate = Fixture.Harness->EndCount;

    TestFalse(
        TEXT("重复结束不能再次清理"),
        Fixture.Router->FinishCurrent(EWuwaActionEndReason::Completed));
    TestEqual(
        TEXT("重复结束不能再次通知 Executor"),
        Fixture.Harness->EndCount,
        EndCountBeforeDuplicate);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWuwaActionRouterBufferTest,
    "Wuwa.Action.Router.Buffer",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FWuwaActionRouterBufferTest::RunTest(const FString &Parameters)
{
    FWuwaRouterFixture Fixture;

    TestTrue(TEXT("Router 应成功初始化"), Fixture.bInitialized);
    TestTrue(TEXT("Executor 应成功注册"), Fixture.bExecutorRegistered);
    TestTrue(
        TEXT("Action Source 应成功设置"),
        Fixture.Router->SetActionSource(Fixture.Harness));

    UWuwaActionDefinition *CurrentDefinition =
        MakeDefinition(
            WuwaGameplayTags::Action_Movement_Dash_Forward,
            20,
            WuwaGameplayTags::State_Action_Dash);

    UWuwaActionDefinition *BufferedDefinition =
        MakeDefinition(
            WuwaGameplayTags::Action_Movement_Backstep,
            10);

    Fixture.Harness->SupportedDefinitions.Add(CurrentDefinition);
    Fixture.Harness->SupportedDefinitions.Add(BufferedDefinition);
    Fixture.Harness->SourceInputTag = WuwaGameplayTags::Input_Sprint;
    Fixture.Harness->SourceDefinition = BufferedDefinition;

    TestTrue(
        TEXT("占用动作应成功开始"),
        Fixture.Router->Request(MakeRequest(CurrentDefinition, 1)).HasStarted());

    const FWuwaInputCommand BufferedCommand =
        MakeCommand(WuwaGameplayTags::Input_Sprint, 2);

    TestTrue(TEXT("等待命令应成功入队"), Fixture.Buffer->Push(BufferedCommand));
    Fixture.Router->TryConsumeBuffer();

    const FWuwaActionResult BufferedResult = Fixture.Router->GetLastResult();

    TestTrue(
        TEXT("低优先级请求应保留为 Buffered"),
        BufferedResult.Status == EWuwaActionRequestStatus::Buffered);
    TestTrue(
        TEXT("Buffered 应保留原始失败原因"),
        BufferedResult.RejectionReason == EWuwaActionRejectionReason::Priority);
    TestEqual(
        TEXT("Buffered 命令不能提前消费"),
        Fixture.Buffer->GetBufferedCommandCount(0.0),
        1);

    TestTrue(TEXT("取消当前动作应触发下一命令"), Fixture.Router->CancelCurrent());
    TestTrue(
        TEXT("清理后 Buffered 动作应开始"),
        Fixture.Router->GetCurrentActionTag() == BufferedDefinition->ActionTag);
    TestEqual(
        TEXT("动作开始后应消费原队首"),
        Fixture.Buffer->GetBufferedCommandCount(0.0),
        0);

    Fixture.Router->FinishCurrent(EWuwaActionEndReason::Completed);

    const FWuwaInputCommand UnhandledCommand =
        MakeCommand(WuwaGameplayTags::Input_Attack, 3);

    Fixture.Buffer->Push(UnhandledCommand);
    Fixture.Router->TryConsumeBuffer();

    TestEqual(
        TEXT("未接入命令应被消费"),
        Fixture.Buffer->GetBufferedCommandCount(0.0),
        0);
    TestTrue(
        TEXT("未接入命令不能启动动作"),
        Fixture.Router->GetLastResult().RejectionReason ==
            EWuwaActionRejectionReason::InvalidDefinition);

    Fixture.Harness->bCanStart = false;
    Fixture.Harness->CanStartFailureReason =
        EWuwaActionRejectionReason::Cooldown;

    const FWuwaInputCommand CooldownCommand =
        MakeCommand(WuwaGameplayTags::Input_Sprint, 4);

    Fixture.Buffer->Push(CooldownCommand);
    Fixture.Router->TryConsumeBuffer();

    TestTrue(
        TEXT("冷却中的请求应保留"),
        Fixture.Router->GetLastResult().Status ==
            EWuwaActionRequestStatus::Buffered);
    TestTrue(
        TEXT("Buffered 应记录 Cooldown"),
        Fixture.Router->GetLastResult().RejectionReason ==
            EWuwaActionRejectionReason::Cooldown);

    Fixture.Harness->bCanStart = true;
    Fixture.Router->TryConsumeBuffer();

    TestTrue(TEXT("冷却解除后动作应开始"), Fixture.Router->HasActiveAction());
    TestEqual(
        TEXT("冷却解除后命令应消费"),
        Fixture.Buffer->GetBufferedCommandCount(0.0),
        0);

    Fixture.Router->FinishCurrent(EWuwaActionEndReason::Completed);

    return true;
}

#endif
