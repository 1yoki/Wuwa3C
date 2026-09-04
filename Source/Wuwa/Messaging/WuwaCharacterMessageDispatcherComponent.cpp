#include "Messaging/WuwaCharacterMessageDispatcherComponent.h"

#include "AbilitySystem/Input/WuwaAbilityInputRouterComponent.h"
#include "Actions/Data/WuwaActionDefinition.h"
#include "Actions/Network/WuwaActionNetworkComponent.h"
#include "Actions/Runtime/WuwaActionCoordinatorComponent.h"
#include "Core/WuwaGameplayTags.h"
#include "Engine/World.h"
#include "Messaging/WuwaCharacterMessageHandler.h"
#include "Templates/UnrealTemplate.h"
#include "Wuwa.h"

UWuwaCharacterMessageDispatcherComponent::UWuwaCharacterMessageDispatcherComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

bool UWuwaCharacterMessageDispatcherComponent::Initialize(UWuwaActionCoordinatorComponent* InActionCoordinator)
{
	if (!IsValid(InActionCoordinator))
	{
		ActionCoordinator = nullptr;
		UE_LOG(LogWuwa, Error, TEXT("Character Message Dispatcher 初始化失败。Owner=%s"), *GetNameSafe(GetOwner()));
		return false;
	}

	ActionCoordinator = InActionCoordinator;
	return true;
}

bool UWuwaCharacterMessageDispatcherComponent::InitializeAbilityInputRouter(
    UWuwaAbilityInputRouterComponent* InAbilityInputRouter)
{
	if (!IsValid(InAbilityInputRouter))
	{
		AbilityInputRouter = nullptr;
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Character Message Dispatcher 缺少 Ability 输入路由。Owner=%s"),
		       *GetNameSafe(GetOwner()));
		return false;
	}

	AbilityInputRouter = InAbilityInputRouter;
	return true;
}

bool UWuwaCharacterMessageDispatcherComponent::InitializeActionNetwork(
    UWuwaActionNetworkComponent* InActionNetworkComponent)
{
	if (!IsValid(InActionNetworkComponent))
	{
		ActionNetworkComponent = nullptr;
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Character Message Dispatcher 缺少 Action Network。Owner=%s"),
		       *GetNameSafe(GetOwner()));
		return false;
	}

	ActionNetworkComponent = InActionNetworkComponent;
	return true;
}

bool UWuwaCharacterMessageDispatcherComponent::RegisterIntentProvider(UObject* ProviderObject)
{
	const IWuwaActionIntentProvider* Provider =
	    IsValid(ProviderObject) ? Cast<IWuwaActionIntentProvider>(ProviderObject) : nullptr;
	if (Provider == nullptr || IntentProviders.Contains(ProviderObject))
	{
		UE_LOG(LogWuwa,
		       Warning,
		       TEXT("拒绝注册无效或重复 Action Intent Provider。Provider=%s"),
		       *GetNameSafe(ProviderObject));
		return false;
	}

	FGameplayTagContainer NewTags;
	Provider->GatherHandledInputTags(NewTags);
	if (NewTags.IsEmpty())
	{
		UE_LOG(
		    LogWuwa, Warning, TEXT("Action Intent Provider 未认领输入标签。Provider=%s"), *GetNameSafe(ProviderObject));
		return false;
	}

	for (UObject* RegisteredObject : IntentProviders)
	{
		const IWuwaActionIntentProvider* Registered = Cast<IWuwaActionIntentProvider>(RegisteredObject);
		if (Registered == nullptr)
		{
			continue;
		}

		FGameplayTagContainer RegisteredTags;
		Registered->GatherHandledInputTags(RegisteredTags);
		if (RegisteredTags.HasAnyExact(NewTags))
		{
			UE_LOG(LogWuwa,
			       Error,
			       TEXT("Action 输入标签只能有一个 Intent Provider。Provider=%s"),
			       *GetNameSafe(ProviderObject));
			return false;
		}
	}

	IntentProviders.Add(ProviderObject);
	return true;
}

bool UWuwaCharacterMessageDispatcherComponent::UnregisterIntentProvider(UObject* ProviderObject)
{
	return IsValid(ProviderObject) && IntentProviders.Remove(ProviderObject) > 0;
}

bool UWuwaCharacterMessageDispatcherComponent::RegisterImmediateHandler(UObject* HandlerObject)
{
	IWuwaCharacterMessageHandler* Handler =
	    IsValid(HandlerObject) ? Cast<IWuwaCharacterMessageHandler>(HandlerObject) : nullptr;
	if (Handler == nullptr)
	{
		return false;
	}

	if (ImmediateHandlers.Contains(HandlerObject))
	{
		return true;
	}

	FGameplayTagContainer NewTags;
	Handler->GatherHandledInputTags(NewTags);
	if (NewTags.IsEmpty())
	{
		return false;
	}

	for (UObject* RegisteredObject : ImmediateHandlers)
	{
		const IWuwaCharacterMessageHandler* Registered = Cast<IWuwaCharacterMessageHandler>(RegisteredObject);
		if (Registered == nullptr)
		{
			continue;
		}

		FGameplayTagContainer RegisteredTags;
		Registered->GatherHandledInputTags(RegisteredTags);
		if (RegisteredTags.HasAny(NewTags))
		{
			UE_LOG(LogWuwa, Error, TEXT("即时输入标签只能有一个处理者。Handler=%s"), *GetNameSafe(HandlerObject));
			return false;
		}
	}

	ImmediateHandlers.Add(HandlerObject);
	return true;
}

bool UWuwaCharacterMessageDispatcherComponent::UnregisterImmediateHandler(UObject* HandlerObject)
{
	return IsValid(HandlerObject) && ImmediateHandlers.Remove(HandlerObject) > 0;
}

void UWuwaCharacterMessageDispatcherComponent::BeginInputFrame()
{
	DrainPendingFacts();
}

void UWuwaCharacterMessageDispatcherComponent::ProcessInputFrame(
    const FWuwaInputFrame& InputFrame, const FWuwaActionResolutionSnapshot& ResolutionSnapshot)
{
	if (!InputFrame.IsValid() || !ResolutionSnapshot.IsValid() || !IsValid(ActionCoordinator))
	{
		return;
	}

	DrainPendingFacts();

	CurrentMoveIntent =
	    InputFrame.MoveIntent.ContainsNaN() ? FVector2D::ZeroVector : InputFrame.MoveIntent.GetClampedToMaxSize(1.f);
	DispatchContinuousMoveIntent();

	TArray<FWuwaResolvedActionIntent> ResolvedIntents;
	TArray<FWuwaInputCommand> AbilityInputCommands;
	TSet<int32> ConsumedCommandSequences;
	FGameplayTagContainer ConsumedInputTags;

	for (const FWuwaInputCommand& Command : InputFrame.Commands)
	{
		UObject* ProviderObject = FindIntentProvider(Command.InputTag);
		const IWuwaActionIntentProvider* Provider = Cast<IWuwaActionIntentProvider>(ProviderObject);
		if (Provider == nullptr)
		{
			continue;
		}

		FWuwaActionIntentResolution Resolution = Provider->ResolveIntent(Command, ResolutionSnapshot);
		if (Resolution.Status == EWuwaActionResolutionStatus::Resolved && !Resolution.Intent.IsValid())
		{
			Resolution.Status = EWuwaActionResolutionStatus::Invalid;
		}

		if (Resolution.Status == EWuwaActionResolutionStatus::Resolved)
		{
			ConsumedInputTags.AppendTags(Resolution.Intent.ConsumedInputTags);
			ResolvedIntents.Add(Resolution.Intent);
			ConsumedCommandSequences.Add(Command.Header.Sequence);
			QueueIntentResolutionResult(Command, ProviderObject, Resolution);
		}
		else if (Resolution.Status == EWuwaActionResolutionStatus::Rejected)
		{
			ConsumedCommandSequences.Add(Command.Header.Sequence);
			QueueIntentResolutionResult(Command, ProviderObject, Resolution);
		}
		else if (Resolution.Status == EWuwaActionResolutionStatus::Ambiguous)
		{
			ConsumedCommandSequences.Add(Command.Header.Sequence);
			QueueIntentResolutionResult(Command, ProviderObject, Resolution);
			UE_LOG(LogWuwa,
			       Error,
			       TEXT("Action Intent Provider 解析歧义，命令被拒绝。Input=%s, Provider=%s, Sequence=%d"),
			       *Command.InputTag.ToString(),
			       *GetNameSafe(ProviderObject),
			       Command.Header.Sequence);
		}
		else if (Resolution.Status == EWuwaActionResolutionStatus::Invalid)
		{
			ConsumedCommandSequences.Add(Command.Header.Sequence);
			QueueIntentResolutionResult(Command, ProviderObject, Resolution);
			UE_LOG(LogWuwa,
			       Error,
			       TEXT("Action Intent Provider 返回无效结果，命令被拒绝。Input=%s, Provider=%s, Sequence=%d"),
			       *Command.InputTag.ToString(),
			       *GetNameSafe(ProviderObject),
			       Command.Header.Sequence);
		}
	}

	for (const FWuwaInputCommand& Command : InputFrame.Commands)
	{
		if (!ConsumedCommandSequences.Contains(Command.Header.Sequence) &&
		    !ConsumedInputTags.HasTagExact(Command.InputTag))
		{
			const FWuwaCommandDispatchResult DispatchResult = DispatchImmediateCommand(Command, InputFrame);
			if (DispatchResult.Status == EWuwaCommandDispatchStatus::Unhandled)
			{
				AbilityInputCommands.Add(Command);
			}
		}
	}

	for (const FWuwaResolvedActionIntent& Intent : ResolvedIntents)
	{
		if (UWuwaActionNetworkComponent* ActionNetwork = ActionNetworkComponent.Get())
		{
			ActionNetwork->RouteResolvedIntent(Intent);
		}
		else
		{
			UE_LOG(LogWuwa,
			       Error,
			       TEXT("Exclusive Action 缺少唯一网络路由，已拒绝。Owner=%s, Action=%s"),
			       *GetNameSafe(GetOwner()),
			       *GetNameSafe(Intent.Request.Definition.Get()));
		}
	}
	ActionCoordinator->PumpQueue();

	if (UWuwaAbilityInputRouterComponent* Router = AbilityInputRouter.Get())
	{
		for (const FWuwaInputCommand& Command : AbilityInputCommands)
		{
			if (Command.Trigger == EWuwaInputCommandTrigger::Pressed)
			{
				Router->AbilityInputTagPressed(Command.InputTag, Command.Header);
			}
			else
			{
				Router->AbilityInputTagReleased(Command.InputTag, Command.Header);
			}
		}
		Router->ProcessAbilityInput(InputFrame.DeltaTime, InputFrame.bGamePaused);
	}
}

bool UWuwaCharacterMessageDispatcherComponent::PublishActionEvent(const FWuwaActionHandle& Handle,
                                                                  const FGameplayTag& EventTag,
                                                                  const FVector2D& MoveIntent,
                                                                  const EMovementMode PreviousMovementMode,
                                                                  const EMovementMode MovementMode,
                                                                  const uint8 PreviousCustomMovementMode,
                                                                  const uint8 CustomMovementMode,
                                                                  UObject* SourceObject)
{
	if (!Handle.IsValid() || !EventTag.IsValid())
	{
		return false;
	}

	FWuwaActionEventMessage Message;
	Message.Header = MakeFactHeader(SourceObject);
	Message.Handle = Handle;
	Message.EventTag = EventTag;
	Message.MoveIntent = MoveIntent.ContainsNaN() ? FVector2D::ZeroVector : MoveIntent.GetClampedToMaxSize(1.f);
	Message.PreviousMovementMode = PreviousMovementMode;
	Message.MovementMode = MovementMode;
	Message.PreviousCustomMovementMode = PreviousCustomMovementMode;
	Message.CustomMovementMode = CustomMovementMode;

	PendingActionFacts.Add(Message);
	OnActionEventPublished.Broadcast(Message);
	return true;
}

void UWuwaCharacterMessageDispatcherComponent::TickComponent(const float DeltaTime,
                                                             const ELevelTick TickType,
                                                             FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	DrainPendingFacts();
	if (IsValid(ActionCoordinator))
	{
		ActionCoordinator->PumpQueue();
	}
	DrainIntentResolutionResults();
}

void UWuwaCharacterMessageDispatcherComponent::DrainPendingFacts()
{
	if (bIsDrainingFacts || !IsValid(ActionCoordinator) || PendingActionFacts.IsEmpty())
	{
		return;
	}

	TGuardValue<bool> DrainGuard(bIsDrainingFacts, true);
	PendingActionFacts.StableSort(
	    [](const FWuwaActionEventMessage& Left, const FWuwaActionEventMessage& Right)
	    {
		    return Left.Header.Sequence < Right.Header.Sequence;
	    });

	TArray<FWuwaActionEventMessage> FactsToDrain = MoveTemp(PendingActionFacts);
	PendingActionFacts.Reset();

	for (const FWuwaActionEventMessage& Fact : FactsToDrain)
	{
		ActionCoordinator->HandleActionEvent(Fact);
	}
}

void UWuwaCharacterMessageDispatcherComponent::DrainIntentResolutionResults()
{
	if (PendingIntentResolutionResults.IsEmpty())
	{
		return;
	}

	TArray<FWuwaActionIntentResolutionResult> ResultsToPublish = MoveTemp(PendingIntentResolutionResults);
	PendingIntentResolutionResults.Reset();

	for (const FWuwaActionIntentResolutionResult& Result : ResultsToPublish)
	{
		LastIntentResolutionResult = Result;
		OnIntentResolutionFinalized.Broadcast(Result);
	}
}

UObject* UWuwaCharacterMessageDispatcherComponent::FindIntentProvider(const FGameplayTag& InputTag) const
{
	if (!InputTag.IsValid())
	{
		return nullptr;
	}

	for (UObject* ProviderObject : IntentProviders)
	{
		const IWuwaActionIntentProvider* Provider = Cast<IWuwaActionIntentProvider>(ProviderObject);
		if (Provider == nullptr)
		{
			continue;
		}

		FGameplayTagContainer HandledTags;
		Provider->GatherHandledInputTags(HandledTags);
		if (HandledTags.HasTagExact(InputTag))
		{
			return ProviderObject;
		}
	}

	return nullptr;
}

void UWuwaCharacterMessageDispatcherComponent::QueueIntentResolutionResult(
    const FWuwaInputCommand& Command, UObject* ProviderObject, const FWuwaActionIntentResolution& Resolution)
{
	FWuwaActionIntentResolutionResult Result;
	Result.Header = Command.Header;
	Result.InputTag = Command.InputTag;
	Result.ProviderObject = ProviderObject;
	Result.Status = Resolution.Status;
	Result.DiagnosticPayload = Resolution.DiagnosticPayload;
	PendingIntentResolutionResults.Add(MoveTemp(Result));
}

void UWuwaCharacterMessageDispatcherComponent::DispatchContinuousMoveIntent()
{
	if (!IsValid(ActionCoordinator))
	{
		return;
	}

	const FWuwaActionRuntimeSnapshot Snapshot = ActionCoordinator->GetRuntimeSnapshot();
	if (!Snapshot.ActiveHandle.IsValid())
	{
		return;
	}

	FWuwaActionEventMessage Message;
	Message.Header = MakeFactHeader(this);
	Message.Handle = Snapshot.ActiveHandle;
	Message.EventTag = WuwaGameplayTags::Action_Event_Input_MoveChanged;
	Message.MoveIntent = CurrentMoveIntent;
	ActionCoordinator->HandleActionEvent(Message);
}

FWuwaCommandDispatchResult
UWuwaCharacterMessageDispatcherComponent::DispatchImmediateCommand(const FWuwaInputCommand& Command,
                                                                   const FWuwaInputFrame& InputFrame)
{
	FWuwaCommandDispatchResult Result;
	Result.MessageTag = Command.InputTag;

	for (UObject* HandlerObject : ImmediateHandlers)
	{
		IWuwaCharacterMessageHandler* Handler = Cast<IWuwaCharacterMessageHandler>(HandlerObject);
		if (Handler == nullptr)
		{
			continue;
		}

		FGameplayTagContainer HandledTags;
		Handler->GatherHandledInputTags(HandledTags);
		if (HandledTags.HasTagExact(Command.InputTag))
		{
			return Handler->HandleInputCommand(Command, InputFrame);
		}
	}

	return Result;
}

FWuwaMessageHeader UWuwaCharacterMessageDispatcherComponent::MakeFactHeader(UObject* SourceObject)
{
	FWuwaMessageHeader Header;
	Header.FrameNumber = GFrameCounter;
	Header.Sequence = NextFactSequence++;
	Header.CreatedAt = GetWorld() != nullptr ? static_cast<double>(GetWorld()->GetTimeSeconds()) : 0.0;
	Header.SourceObject = SourceObject;

	if (NextFactSequence <= 0)
	{
		NextFactSequence = 1;
	}

	return Header;
}

void UWuwaCharacterMessageDispatcherComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	PendingActionFacts.Reset();
	PendingIntentResolutionResults.Reset();
	IntentProviders.Reset();
	ImmediateHandlers.Reset();
	ActionCoordinator = nullptr;
	ActionNetworkComponent = nullptr;
	AbilityInputRouter = nullptr;

	Super::EndPlay(EndPlayReason);
}
