#include "Actions/WuwaActionRouterComponent.h"

#include "Actions/WuwaActionSource.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "Actions/WuwaActionDefinition.h"
#include "Core/WuwaStateTagComponent.h"
#include "Input/WuwaInputBufferComponent.h"
#include "Actions/WuwaActionExecutor.h"
#include "Templates/UnrealTemplate.h"
#include "Wuwa.h"

void FWuwaActiveActionRuntime::ClearAfterCleanup()
{
    // 此处只清空记录，不负责释放外部资源
    Definition = nullptr;
    ExecutorObject = nullptr;
    ActionTag = FGameplayTag();
    Context = FWuwaActionContext();
    SourceInputSequence = 0;
    GrantedTagHandles.Reset();
}

UWuwaActionRouterComponent::UWuwaActionRouterComponent()
{
    // Router 由请求和结束事件驱动，不需要每帧 Tick
    PrimaryComponentTick.bCanEverTick = false;
}

bool UWuwaActionRouterComponent::IsInitialized() const
{
    // 完整类型可安全转换为 UObject 指针。
    return IsValid(InputBufferComponent.Get()) &&
           IsValid(StateTagComponent.Get());
}

bool UWuwaActionRouterComponent::Initialize(UWuwaInputBufferComponent *InInputBufferComponent, UWuwaStateTagComponent *InStateTagComponent)
{
    if (!IsValid(InInputBufferComponent) || !IsValid(InStateTagComponent))
    {
        StateTagComponent = nullptr;
        InputBufferComponent = nullptr;

        UE_LOG(
            LogWuwa,
            Error,
            TEXT("Action Router 初始化失败，缺少依赖组件。Owner=%s"),
            *GetNameSafe(GetOwner()));

        return false;
    }

    InputBufferComponent = InInputBufferComponent;
    StateTagComponent = InStateTagComponent;

    UE_LOG(
        LogWuwa,
        Verbose,
        TEXT("Action Router 初始化成功。Owner=%s"),
        *GetNameSafe(GetOwner()));

    return true;
}

bool UWuwaActionRouterComponent::SetActionSource(UObject *InActionSourceObject)
{
    if (!IsValid(InActionSourceObject) || !InActionSourceObject->GetClass()->ImplementsInterface(UWuwaActionSource::StaticClass()))
    {
        UE_LOG(
            LogWuwa,
            Warning,
            TEXT("拒绝设置无效 Action Source。Owner=%s"),
            *GetNameSafe(GetOwner()));

        return false;
    }

    ActionSourceObject = InActionSourceObject;

    UE_LOG(
        LogWuwa,
        Verbose,
        TEXT("Action Source 已设置。Owner=%s, Source=%s"),
        *GetNameSafe(GetOwner()),
        *GetNameSafe(InActionSourceObject));

    return true;
}

// 实现统一时间基准，避免不同组件使用不同时间源导致的逻辑不一致。
double UWuwaActionRouterComponent::GetActionTime() const
{
    const UWorld *World = GetWorld();

    return World ? static_cast<double>(World->GetTimeSeconds()) : 0.0;
}

// 判断失败请求是否仍可保留在 FIFO 中
bool UWuwaActionRouterComponent::CanRemainBuffered(
    const FWuwaInputCommand &Command,
    const FWuwaActionRequest &Request,
    const EWuwaActionRejectionReason RejectionReason,
    const double CurrentTime) const
{
    UWuwaActionDefinition *Definition = Request.Definition.Get();

    if (!IsValid(Definition) || Definition->BufferTime <= 0.f)
    {
        return false;
    }

    bool bTemporaryFailure = false;

    // 仅对部分拒绝原因允许暂时保留请求，避免因瞬时状态变化导致的误拒绝。
    switch (RejectionReason)
    {
    case EWuwaActionRejectionReason::MissingRequiredTag:
    case EWuwaActionRejectionReason::BlockedByTag:
    case EWuwaActionRejectionReason::Priority:
    case EWuwaActionRejectionReason::CancellationRule:
    case EWuwaActionRejectionReason::Cooldown:
        bTemporaryFailure = true;
        break;

    default:
        break;
    }

    if (!bTemporaryFailure)
    {
        return false;
    }

    const double DefinitionExpireAt = Command.PressedAt + static_cast<double>(Definition->BufferTime);

    // Input 与 Definition 任意一方到期都会失效。
    const double EffectiveExpireAt = FMath::Min(Command.GetExpireAt(), DefinitionExpireAt);

    return CurrentTime < EffectiveExpireAt;
}

// 实现FIFO消费逻辑，从 FIFO 队首构建并执行一个动作请求
void UWuwaActionRouterComponent::TryConsumeBuffer()
{
    if (!IsInitialized() || bIsConsumingBuffer || bIsProcessingRequest)
    {
        return;
    }

    UObject *SourceObject = ActionSourceObject.Get();

    IWuwaActionSource *ActionSource = IsValid(SourceObject) ? Cast<IWuwaActionSource>(SourceObject) : nullptr;

    if (!ActionSource)
    {
        // Source 尚未装配时不能擅自丢弃命令。
        return;
    }

    TGuardValue<bool> ConsumeGuard(bIsConsumingBuffer, true);

    while (true)
    {
        const double CurrentTime = GetActionTime();

        FWuwaInputCommand Command;

        if (!InputBufferComponent->Peek(CurrentTime, Command))
        {
            return;
        }

        FWuwaActionRequest ActionRequest;

        if (!ActionSource->BuildActionRequest(Command, ActionRequest))
        {
            FWuwaInputCommand DiscardedCommand;

            // 已装配 Source 但无法构建表示该命令当前未接入。
            if (!InputBufferComponent->Consume(CurrentTime, Command.Sequence, DiscardedCommand))
            {
                return;
            }

            LastResult = FWuwaActionResult();
            LastResult.Status = EWuwaActionRequestStatus::Rejected;
            LastResult.RejectionReason = EWuwaActionRejectionReason::InvalidDefinition;
            LastResult.SourceInputSequence = Command.Sequence;

            UE_LOG(
                LogWuwa,
                Verbose,
                TEXT("未接入的 Input Command 已丢弃。Input=%s, Sequence=%u"),
                *Command.InputTag.ToString(),
                Command.Sequence);

            continue;
        }

        FWuwaActionResult Result = Request(ActionRequest);

        if (Result.HasStarted())
        {
            FWuwaInputCommand ConsumedCommand;

            // 动作开始后只提交对应 Sequence 的队首命令。
            if (!InputBufferComponent->Consume(CurrentTime, Command.Sequence, ConsumedCommand))
            {
                FinishCurrentInternal(EWuwaActionEndReason::Failed);

                LastResult = FWuwaActionResult();
                LastResult.ActionTag = ActionRequest.ActionTag;
                LastResult.SourceInputSequence = Command.Sequence;
                LastResult.RejectionReason = EWuwaActionRejectionReason::InvalidContext;

                return;
            }

            // 同步完成的动作允许继续查看下一条命令。
            if (!HasActiveAction())
            {
                continue;
            }

            return;
        }

        if (CanRemainBuffered(Command, ActionRequest, Result.RejectionReason, CurrentTime))
        {
            Result.Status = EWuwaActionRequestStatus::Buffered;

            LastResult = Result;

            UE_LOG(
                LogWuwa,
                Verbose,
                TEXT("Action Request 保留在缓存。Action=%s, Reason=%s, Sequence=%u"),
                *ActionRequest.ActionTag.ToString(),
                *UEnum::GetValueAsString(Result.RejectionReason),
                Command.Sequence);

            if (Result.RejectionReason == EWuwaActionRejectionReason::Cooldown)
            {
                // Router 不启用 Tick，只为当前因冷却等待的队首命令设置一次性重试
                ScheduleCooldownBufferRetry(Command, ActionRequest, CurrentTime);
            }
            return;
        }

        FWuwaInputCommand DiscardedCommand;

        // 永久拒绝或缓存到期后消费该命令。
        if (!InputBufferComponent->Consume(CurrentTime, Command.Sequence, DiscardedCommand))
        {
            return;
        }
    }
}

void UWuwaActionRouterComponent::ScheduleCooldownBufferRetry(const FWuwaInputCommand &Command,
                                                             const FWuwaActionRequest &Request,
                                                             const double CurrentTime)
{
    const UWuwaActionDefinition *Definition = Request.Definition.Get();

    UWorld *World = GetWorld();

    if (!IsValid(Definition) || !IsValid(World) || Definition->BufferTime <= 0.f)
    {
        return;
    }

    const double CooldownExpireAt = GetActionCooldownExpireAt(Request.ActionTag);

    const double DefinitionExpireAt = Command.PressedAt + static_cast<double>(Definition->BufferTime);

    // 命令自身有效期和 Definition BufferTime 任一先到，都必须停止等待
    const double EffectiveCommandExpireAt = FMath::Min(Command.GetExpireAt(), DefinitionExpireAt);

    const double RetryAt = FMath::Min(CooldownExpireAt, EffectiveCommandExpireAt);

    constexpr double MinimumRetryDelay = 0.001;

    const float RetryDelay = static_cast<float>(FMath::Max(RetryAt - GetActionTime(), MinimumRetryDelay));

    World->GetTimerManager().SetTimer(
        CooldownBufferRetryTimerHandle,
        this,
        &UWuwaActionRouterComponent::HandleCooldownBufferRetry,
        RetryDelay,
        false);
}

void UWuwaActionRouterComponent::ClearCooldownBufferRetry()
{
    if (UWorld *World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CooldownBufferRetryTimerHandle);
    }

    CooldownBufferRetryTimerHandle.Invalidate();
}

void UWuwaActionRouterComponent::HandleCooldownBufferRetry()
{
    CooldownBufferRetryTimerHandle.Invalidate();

    // Timer 只重新驱动 Router；仍由 FIFO 和 CanStart 决定启动或过期
    TryConsumeBuffer();
}

bool UWuwaActionRouterComponent::RegisterExecutor(UObject *ExecutorObject)
{
    if (!IsValid(ExecutorObject) || !ExecutorObject->GetClass()->ImplementsInterface(UWuwaActionExecutor::StaticClass()))
    {
        UE_LOG(
            LogWuwa,
            Warning,
            TEXT("拒绝注册无效 Action Executor。Owner=%s"),
            *GetNameSafe(GetOwner()));

        return false;
    }

    if (RegisteredExecutors.Contains(ExecutorObject))
    {
        // 重复注册不改变原有注册顺序。
        return true;
    }

    RegisteredExecutors.Add(ExecutorObject);

    UE_LOG(
        LogWuwa,
        Verbose,
        TEXT("Action Executor 已注册。Owner=%s, Executor=%s"),
        *GetNameSafe(GetOwner()),
        *GetNameSafe(ExecutorObject));

    return true;
}

bool UWuwaActionRouterComponent::UnregisterExecutor(UObject *ExecutorObject)
{
    if (!IsValid(ExecutorObject))
    {
        return false;
    }

    // 活动执行者不能在动作清理前被移除。
    if (HasActiveAction() && ActiveAction.ExecutorObject == ExecutorObject)
    {
        UE_LOG(
            LogWuwa,
            Warning,
            TEXT("活动 Action Executor 不能直接注销。Executor=%s"),
            *GetNameSafe(ExecutorObject));

        return false;
    }

    return RegisteredExecutors.Remove(ExecutorObject) > 0;
}

UObject *UWuwaActionRouterComponent::ResolveExecutor(const UWuwaActionDefinition &Definition) const
{
    for (UObject *ExecutorObject : RegisteredExecutors)
    {
        if (!IsValid(ExecutorObject))
        {
            continue;
        }

        IWuwaActionExecutor *Executor = Cast<IWuwaActionExecutor>(ExecutorObject);

        if (Executor && Executor->SupportsAction(Definition))
        {
            // 多个 Executor 支持时按注册顺序选择。
            return ExecutorObject;
        }
    }

    return nullptr;
}

bool UWuwaActionRouterComponent::IsContextValid(const FWuwaActionRequest &Request) const
{
    const FWuwaActionContext &Context = Request.Context;

    // 请求与上下文必须指向同一条输入命令
    if (Request.SourceInputSequence != Context.SourceInputSequence)
    {
        return false;
    }

    // 非有限向量会污染后续移动计算
    if (Context.InputDirection.ContainsNaN() || Context.WorldDirection.ContainsNaN() || Context.FacingDirection.ContainsNaN())
    {
        return false;
    }

    // 输入方向必须保持在单位圆内
    constexpr float MaxInputMagnitudeSquared = 1.0001f;

    if (Context.InputDirection.SizeSquared() > MaxInputMagnitudeSquared)
    {
        return false;
    }

    // 朝向快照必须提供有效方向
    if (Context.FacingDirection.IsNearlyZero())
    {
        return false;
    }

    return true;
}

// 实现双向取消规则
bool UWuwaActionRouterComponent::CanInterruptCurrent(const FWuwaActionRequest &Request, EWuwaActionRejectionReason &OutReason) const
{
    // 没有当前动作时不需要执行打断仲裁
    if (HasActiveAction() == false)
    {
        return true;
    }

    UWuwaActionDefinition *IncomingDefinition = Request.Definition.Get();

    UWuwaActionDefinition *CurrentDefinition = ActiveAction.Definition.Get();

    // 当前运行态损坏时采用拒绝策略
    if (!IsValid(CurrentDefinition))
    {
        OutReason = EWuwaActionRejectionReason::InvalidContext;
        return false;
    }

    // 相同或更低优先级不能替换当前动作
    if (IncomingDefinition->Priority <= CurrentDefinition->Priority)
    {
        OutReason = EWuwaActionRejectionReason::Priority;
        return false;
    }

    // 新动作必须声明可以打断当前动作
    const bool bIncomingAllowsCancel = ActiveAction.ActionTag.MatchesAny(IncomingDefinition->CanCancelActions);

    // 当前动作必须声明允许被新动作打断
    const bool bCurrentAllowsIncoming = Request.ActionTag.MatchesAny(CurrentDefinition->CanBeCancelledBy);

    if (!bIncomingAllowsCancel || !bCurrentAllowsIncoming)
    {
        OutReason = EWuwaActionRejectionReason::CancellationRule;
        return false;
    }

    return true;
}

bool UWuwaActionRouterComponent::CanStart(const FWuwaActionRequest &Request, EWuwaActionRejectionReason &OutReason) const
{
    OutReason = EWuwaActionRejectionReason::None;

    // 未完成依赖注入时不能接受动作
    if (!IsInitialized())
    {
        OutReason = EWuwaActionRejectionReason::InvalidContext;
        return false;
    }

    UWuwaActionDefinition *Definition = Request.Definition.Get();

    // 请求标签必须与有效Definition完全一致
    if (!Request.IsValid() || !IsValid(Definition) || !Definition->IsRuntimeValid() || Request.ActionTag != Definition->ActionTag)
    {
        OutReason = EWuwaActionRejectionReason::InvalidDefinition;
        return false;
    }

    // 上下文快照必须有效
    if (!IsContextValid(Request))
    {
        OutReason = EWuwaActionRejectionReason::InvalidContext;
        return false;
    }

    const FGameplayTagContainer ActiveTags = StateTagComponent->GetActiveTags();

    // RequiredTags 必须全部满足
    if (!ActiveTags.HasAll(Definition->RequiredTags))
    {
        OutReason = EWuwaActionRejectionReason::MissingRequiredTag;
        return false;
    }

    // BlockedTags 命中任意一个就拒绝请求
    if (ActiveTags.HasAny(Definition->BlockedTags))
    {
        OutReason = EWuwaActionRejectionReason::BlockedByTag;
        return false;
    }

    UObject *ExecutorObject = ResolveExecutor(*Definition);

    if (!IsValid(ExecutorObject))
    {
        OutReason = EWuwaActionRejectionReason::NoExecutor;
        return false;
    }

    IWuwaActionExecutor *Executor = Cast<IWuwaActionExecutor>(ExecutorObject);

    if (!Executor)
    {
        OutReason = EWuwaActionRejectionReason::NoExecutor;
        return false;
    }

    // 当前动作冲突必须先返回 Priority/CancellationRule，
    // 不能被 Executor 的“已有运行资源”误报为 InvalidContext
    if (!CanInterruptCurrent(Request, OutReason))
    {
        return false;
    }

    EWuwaActionRejectionReason ExecutorReason = EWuwaActionRejectionReason::None;

    // Executor 检查真实 MovementMode、Context 和自身资源
    if (!Executor->CanStartAction(Request, ExecutorReason))
    {
        OutReason = ExecutorReason == EWuwaActionRejectionReason::None
                        ? EWuwaActionRejectionReason::InvalidContext
                        : ExecutorReason;

        return false;
    }

    // Context 有效且没有动作冲突后，再检查同 ActionTag 的通用冷却
    if (IsActionOnCooldown(Request.ActionTag, GetActionTime()))
    {
        OutReason =
            EWuwaActionRejectionReason::Cooldown;

        return false;
    }

    return true;
}

// 提交动作请求并返回结构化结果
FWuwaActionResult UWuwaActionRouterComponent::Request(const FWuwaActionRequest &Request)
{
    FWuwaActionResult Result;
    Result.ActionTag = Request.ActionTag;
    Result.SourceInputSequence = Request.SourceInputSequence;

    if (bIsProcessingRequest)
    {
        // 拒绝 Executor 回调造成的同步递归请求。
        Result.RejectionReason = EWuwaActionRejectionReason::InvalidContext;
        return Result;
    }

    TGuardValue<bool> RequestGuard(bIsProcessingRequest, true);

    EWuwaActionRejectionReason RejectionReason = EWuwaActionRejectionReason::None;

    if (!CanStart(Request, RejectionReason))
    {
        Result.Status = EWuwaActionRequestStatus::Rejected;
        Result.RejectionReason = RejectionReason;
        LastResult = Result;

        UE_LOG(
            LogWuwa,
            Verbose,
            TEXT("Action Request 被拒绝。Action=%s, Reason=%s, Sequence=%u"),
            *Request.ActionTag.ToString(),
            *UEnum::GetValueAsString(RejectionReason),
            Request.SourceInputSequence);

        return Result;
    }

    // 通过仲裁的新动作先中断当前动作。
    if (HasActiveAction() && !FinishCurrentInternal(EWuwaActionEndReason::Interrupted))
    {
        Result.Status = EWuwaActionRequestStatus::Rejected;
        Result.RejectionReason = EWuwaActionRejectionReason::InvalidContext;
        LastResult = Result;
        return Result;
    }

    if (!StartApprovedAction(Request, RejectionReason))
    {
        Result.Status = EWuwaActionRequestStatus::Rejected;
        Result.RejectionReason = RejectionReason;
        LastResult = Result;
        return Result;
    }

    Result.Status = EWuwaActionRequestStatus::Started;
    Result.RejectionReason = EWuwaActionRejectionReason::None;

    LastResult = Result;

    UE_LOG(
        LogWuwa,
        Log,
        TEXT("Action Request 已开始。Action=%s, Sequence=%u"),
        *Request.ActionTag.ToString(),
        Request.SourceInputSequence);

    return Result;
}

bool UWuwaActionRouterComponent::CancelCurrent()
{
    // 主动取消统一使用 Cancelled 原因。
    return FinishCurrent(EWuwaActionEndReason::Cancelled);
}

// 由 Executor 或 Gameplay 系统结束当前动作
bool UWuwaActionRouterComponent::FinishCurrent(const EWuwaActionEndReason EndReason)
{
    if (EndReason == EWuwaActionEndReason::None)
    {
        UE_LOG(
            LogWuwa,
            Warning,
            TEXT("拒绝使用 None 结束 Action。Owner=%s"),
            *GetNameSafe(GetOwner()));

        return false;
    }

    const bool bFinished = FinishCurrentInternal(EndReason);
    if (bFinished && EndReason != EWuwaActionEndReason::OwnerDestroyed)
    {
        // 当前动作完全清理后再处理下一条命令
        TryConsumeBuffer();
    }

    return bFinished;
}

// 实现原子化启动
bool UWuwaActionRouterComponent::StartApprovedAction(const FWuwaActionRequest &Request, EWuwaActionRejectionReason &OutReason)
{
    OutReason = EWuwaActionRejectionReason::None;

    UWuwaActionDefinition *Definition = Request.Definition.Get();

    UObject *ExecutorObject = IsValid(Definition) ? ResolveExecutor(*Definition) : nullptr;

    IWuwaActionExecutor *Executor = IsValid(ExecutorObject) ? Cast<IWuwaActionExecutor>(ExecutorObject) : nullptr;

    if (!Executor)
    {
        OutReason = EWuwaActionRejectionReason::NoExecutor;
        return false;
    }

    TArray<FWuwaStateTagHandle> AcquiredHandles;

    for (const FGameplayTag &GrantedTag : Definition->GrantedTags)
    {
        FWuwaStateTagHandle Handle = StateTagComponent->AcquireTag(GrantedTag);

        if (!Handle.IsValid())
        {
            // 任意标签失败都会回滚已取得句柄。
            ReleaseGrantedTagHandles(AcquiredHandles);

            OutReason = EWuwaActionRejectionReason::InvalidDefinition;
            return false;
        }

        AcquiredHandles.Add(MoveTemp(Handle));
    }

    // 调用 Executor 前先提交完整运行态。
    ActiveAction.Definition = Definition;
    ActiveAction.ExecutorObject = ExecutorObject;
    ActiveAction.ActionTag = Request.ActionTag;
    ActiveAction.Context = Request.Context;
    ActiveAction.SourceInputSequence = Request.SourceInputSequence;
    ActiveAction.GrantedTagHandles = MoveTemp(AcquiredHandles);

    LastEndReason = EWuwaActionEndReason::None;

    if (!Executor->StartAction(Request))
    {
        // Executor 可能已经创建部分资源，统一按 Failed 清理。
        FinishCurrentInternal(EWuwaActionEndReason::Failed);

        OutReason = EWuwaActionRejectionReason::ExecutorFailed;
        return false;
    }

    // 只有表现和 Gameplay 位移都成功启动后，才正式提交本次冷却
    CommitActionCooldown(*Definition);

    return true;
}

void UWuwaActionRouterComponent::ReleaseGrantedTagHandles(TArray<FWuwaStateTagHandle> &Handles)
{
    if (!IsValid(StateTagComponent))
    {
        // 组件失效后只能清空本地句柄记录。
        Handles.Reset();
        return;
    }

    // 按取得顺序的逆序释放动作标签。
    for (int32 Index = Handles.Num() - 1; Index >= 0; --Index)
    {
        FWuwaStateTagHandle &Handle = Handles[Index];

        if (Handle.IsValid())
        {
            StateTagComponent->ReleaseTag(Handle);
        }
    }

    Handles.Reset();
}

// 结束当前动作，但暂时不消费下一条输入
bool UWuwaActionRouterComponent::FinishCurrentInternal(const EWuwaActionEndReason EndReason)
{
    if (!HasActiveAction() || EndReason == EWuwaActionEndReason::None || bIsFinishingCurrent)
    {
        return false;
    }

    // 作用域结束时自动恢复清理锁，防止递归调用导致的重复清理。
    TGuardValue<bool> FinishGuard(bIsFinishingCurrent, true);

    UObject *ExecutorObject = ActiveAction.ExecutorObject.Get();

    const FGameplayTag FinishedActionTag = ActiveAction.ActionTag;

    IWuwaActionExecutor *Executor = IsValid(ExecutorObject) ? Cast<IWuwaActionExecutor>(ExecutorObject) : nullptr;

    // Executor 先清理自己创建的 Gameplay 资源。
    if (Executor)
    {
        Executor->EndAction(FinishedActionTag, EndReason);
    }

    // Router 只释放当前动作取得的标签。
    ReleaseGrantedTagHandles(ActiveAction.GrantedTagHandles);

    ActiveAction.ClearAfterCleanup();
    LastEndReason = EndReason;

    UE_LOG(
        LogWuwa,
        Log,
        TEXT("Action 已结束。Owner=%s, Action=%s, Reason=%s"),
        *GetNameSafe(GetOwner()),
        *FinishedActionTag.ToString(),
        *UEnum::GetValueAsString(EndReason));

    return true;
}

bool UWuwaActionRouterComponent::IsActionOnCooldown(const FGameplayTag &ActionTag, const double CurrentTime) const
{
    const double *ExpireAt = CooldownExpireAtByAction.Find(ActionTag);

    return ExpireAt != nullptr && CurrentTime < *ExpireAt;
}

double UWuwaActionRouterComponent::GetActionCooldownExpireAt(const FGameplayTag &ActionTag) const
{
    const double *ExpireAt = CooldownExpireAtByAction.Find(ActionTag);

    return ExpireAt != nullptr ? *ExpireAt : 0.0;
}

void UWuwaActionRouterComponent::CommitActionCooldown(const UWuwaActionDefinition &Definition)
{
    if (Definition.CooldownDuration <= 0.f)
    {
        return;
    }

    // 冷却从 Executor 成功启动的时刻开始，失败启动不能污染冷却运行态
    const double NewExpireAt = GetActionTime() + static_cast<double>(Definition.CooldownDuration);

    double &StoredExpireAt = CooldownExpireAtByAction.FindOrAdd(Definition.ActionTag);

    StoredExpireAt = FMath::Max(StoredExpireAt, NewExpireAt);
}

void UWuwaActionRouterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasActiveAction())
    {
        // Owner 销毁时强制结束当前动作，避免残留标签污染后续运行。
        FinishCurrentInternal(EWuwaActionEndReason::OwnerDestroyed);
    }

    // Timer 不能在 Router 销毁后再次尝试消费 FIFO
    ClearCooldownBufferRetry();
    CooldownExpireAtByAction.Reset();

    // 清理完成后再释放组件引用
    RegisteredExecutors.Reset();
    ActionSourceObject = nullptr;
    InputBufferComponent = nullptr;
    StateTagComponent = nullptr;

    Super::EndPlay(EndPlayReason);
}