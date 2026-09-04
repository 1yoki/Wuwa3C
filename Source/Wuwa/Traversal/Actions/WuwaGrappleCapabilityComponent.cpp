#include "Traversal/Actions/WuwaGrappleCapabilityComponent.h"

#include "Core/WuwaGameplayTags.h"
#include "Messaging/WuwaCharacterMessageDispatcherComponent.h"
#include "Movement/WuwaCharacterMovementComponent.h"
#include "Traversal/Data/WuwaGrappleActionDefinition.h"
#include "Traversal/Query/WuwaGrappleQuery.h"
#include "WuwaCharacter.h"
#include "Wuwa.h"

UWuwaGrappleCapabilityComponent::UWuwaGrappleCapabilityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UWuwaGrappleCapabilityComponent::Initialize(AWuwaCharacter* InCharacter,
                                                 UWuwaCharacterMovementComponent* InMovementComponent,
                                                 UWuwaCharacterMessageDispatcherComponent* InDispatcher)
{
	if (IsValid(MovementComponent))
	{
		MovementComponent->OnGrappleMovementFact.RemoveAll(this);
	}

	CharacterOwner = nullptr;
	MovementComponent = nullptr;
	Dispatcher = nullptr;
	ResetActiveRuntime();
	FinalizingActionHandle = FWuwaActionHandle();
	RuntimeSnapshot = FWuwaGrappleRuntimeSnapshot();

	if (!IsValid(InCharacter) || !IsValid(InMovementComponent) || !IsValid(InDispatcher) || InCharacter != GetOwner() ||
	    InMovementComponent != InCharacter->GetWuwaMovementComponent() || InDispatcher->GetOwner() != InCharacter)
	{
		UE_LOG(LogWuwa,
		       Warning,
		       TEXT("Grapple Capability 初始化失败，依赖或 Owner 不一致。Owner=%s"),
		       *GetNameSafe(GetOwner()));
		return false;
	}

	CharacterOwner = InCharacter;
	MovementComponent = InMovementComponent;
	Dispatcher = InDispatcher;
	MovementComponent->OnGrappleMovementFact.AddUObject(this,
	                                                    &UWuwaGrappleCapabilityComponent::HandleGrappleMovementFact);
	return true;
}

bool UWuwaGrappleCapabilityComponent::IsInitialized() const
{
	return IsValid(CharacterOwner) && IsValid(MovementComponent) && Dispatcher.IsValid();
}

bool UWuwaGrappleCapabilityComponent::GetActivePresentationData(const FWuwaActionHandle& Handle,
                                                                const UWuwaGrappleActionDefinition*& OutDefinition,
                                                                FWuwaGrappleActionContext& OutContext) const
{
	OutDefinition = nullptr;
	OutContext = FWuwaGrappleActionContext();
	const FWuwaGrappleActionContext* Context = ActiveRequest.Context.DomainPayload.GetPtr<FWuwaGrappleActionContext>();
	if (Handle != ActiveActionHandle || !IsValid(ActiveDefinition) || Context == nullptr || !Context->IsRuntimeValid())
	{
		return false;
	}

	OutDefinition = ActiveDefinition;
	OutContext = *Context;
	return true;
}

bool UWuwaGrappleCapabilityComponent::GetActiveNetworkContext(const FWuwaNetworkActionGeneration& Generation,
                                                              FWuwaGrappleActionContext& OutContext) const
{
	OutContext = FWuwaGrappleActionContext();
	const FWuwaGrappleActionContext* Context = ActiveRequest.Context.DomainPayload.GetPtr<FWuwaGrappleActionContext>();
	if (!Generation.IsValid() || ActiveRequest.NetworkGeneration != Generation || Context == nullptr ||
	    !Context->IsRuntimeValid())
	{
		return false;
	}

	OutContext = *Context;
	return true;
}

bool UWuwaGrappleCapabilityComponent::StopActiveGrapple(const FWuwaNetworkActionGeneration& Generation,
                                                        const EWuwaGrappleMovementEndReason EndReason)
{
	if (!Generation.IsValid() || ActiveRequest.NetworkGeneration != Generation || !ActiveMovementHandle.IsValid() ||
	    !IsValid(MovementComponent) ||
	    (EndReason != EWuwaGrappleMovementEndReason::NetworkRejected &&
	     EndReason != EWuwaGrappleMovementEndReason::NetworkCorrection &&
	     EndReason != EWuwaGrappleMovementEndReason::Death &&
	     EndReason != EWuwaGrappleMovementEndReason::AvatarChanged &&
	     EndReason != EWuwaGrappleMovementEndReason::EndPlay))
	{
		return false;
	}

	return MovementComponent->StopGrappleMovement(ActiveMovementHandle, EndReason);
}

FGameplayTag UWuwaGrappleCapabilityComponent::GetActionCapabilityTag() const
{
	return WuwaGameplayTags::Action_Capability_Traversal_Grapple;
}

int32 UWuwaGrappleCapabilityComponent::GetActionCapabilityCommitOrder() const
{
	return 300;
}

FWuwaActionCapabilityResult
UWuwaGrappleCapabilityComponent::PrepareAction(const FWuwaActionPrepareMessage& Message) const
{
	return ValidatePrepare(Message);
}

FWuwaActionCapabilityResult UWuwaGrappleCapabilityComponent::CommitAction(const FWuwaActionCommitMessage& Message)
{
	if (ActiveActionHandle.IsValid())
	{
		return FWuwaActionCapabilityResult::Failure(EWuwaActionRejectionReason::CapabilityCommitFailed);
	}

	FWuwaActionPrepareMessage PrepareMessage;
	PrepareMessage.Handle = Message.Handle;
	PrepareMessage.Request = Message.Request;
	const FWuwaActionCapabilityResult PrepareResult = ValidatePrepare(PrepareMessage);
	if (!PrepareResult.bSucceeded)
	{
		return FWuwaActionCapabilityResult::Failure(EWuwaActionRejectionReason::CapabilityCommitFailed);
	}

	UWuwaGrappleActionDefinition* Definition = Cast<UWuwaGrappleActionDefinition>(Message.Request.Definition);
	const FWuwaGrappleActionContext* FrozenContext =
	    Message.Request.Context.DomainPayload.GetPtr<FWuwaGrappleActionContext>();
	FWuwaGrappleActionContext AdjustedContext;
	FWuwaGrappleQueryResult RevalidationResult;
	const bool bRevalidated = IsValid(Definition) && FrozenContext != nullptr &&
	                          FWuwaGrappleQuery::RevalidateFrozenContext(Definition,
	                                                                     CharacterOwner,
	                                                                     CharacterOwner->GetActorLocation(),
	                                                                     *FrozenContext,
	                                                                     AdjustedContext,
	                                                                     RevalidationResult);
	if (!bRevalidated)
	{
		UE_LOG(LogWuwa,
		       Warning,
		       TEXT("Grapple Commit 复核失败。ActionHandle=%lld Failure=%s"),
		       Message.Handle.Value,
		       *StaticEnum<EWuwaGrappleQueryFailureReason>()->GetNameStringByValue(
		           static_cast<int64>(RevalidationResult.FailureReason)));
		return FWuwaActionCapabilityResult::Failure(EWuwaActionRejectionReason::CapabilityCommitFailed);
	}

	FWuwaGrappleMovementRequest MovementRequest;
	MovementRequest.Spec = Definition->MovementSpec;
	MovementRequest.Context = AdjustedContext;
	MovementRequest.NetworkGeneration = Message.Request.NetworkGeneration;
	FWuwaGrappleMovementHandle MovementHandle;
	if (!MovementComponent->StartGrappleMovement(MovementRequest, MovementHandle))
	{
		return FWuwaActionCapabilityResult::Failure(EWuwaActionRejectionReason::CapabilityCommitFailed);
	}

	ActiveActionHandle = Message.Handle;
	ActiveMovementHandle = MovementHandle;
	ActiveDefinition = Definition;
	ActiveRequest = Message.Request;
	ActiveRequest.Context.DomainPayload.InitializeAs<FWuwaGrappleActionContext>(AdjustedContext);
	FinalizingActionHandle = FWuwaActionHandle();
	RuntimeSnapshot.ActionHandle = ActiveActionHandle;
	RuntimeSnapshot.NetworkGeneration = Message.Request.NetworkGeneration;
	RuntimeSnapshot.MovementHandle = ActiveMovementHandle;
	RuntimeSnapshot.Phase = EWuwaGrappleRuntimePhase::Windup;
	RuntimeSnapshot.LastMovementEndReason = EWuwaGrappleMovementEndReason::None;
	RuntimeSnapshot.ExitVelocity = FVector::ZeroVector;
	MovementComponent->ExitSprintRun();
	return FWuwaActionCapabilityResult::Success();
}

void UWuwaGrappleCapabilityComponent::RollbackAction(const FWuwaActionHandle& Handle)
{
	if (Handle != ActiveActionHandle)
	{
		return;
	}

	if (ActiveMovementHandle.IsValid())
	{
		MovementComponent->StopGrappleMovement(ActiveMovementHandle, EWuwaGrappleMovementEndReason::Stopped);
	}
	ResetActiveRuntime();
	RuntimeSnapshot = FWuwaGrappleRuntimeSnapshot();
}

void UWuwaGrappleCapabilityComponent::StopAction(const FWuwaActionStopMessage& Message)
{
	if (Message.Handle != ActiveActionHandle)
	{
		return;
	}

	FinalizingActionHandle = ActiveActionHandle;
	RuntimeSnapshot.Phase = EWuwaGrappleRuntimePhase::Releasing;
	if (ActiveMovementHandle.IsValid())
	{
		MovementComponent->StopGrappleMovement(ActiveMovementHandle, EWuwaGrappleMovementEndReason::Stopped);
	}
	ResetActiveRuntime();
}

EWuwaActionEndReason UWuwaGrappleCapabilityComponent::HandleActionEvent(const FWuwaActionEventMessage& Message)
{
	(void)Message;
	return EWuwaActionEndReason::None;
}

void UWuwaGrappleCapabilityComponent::HandleActionFinalized(const FWuwaActionFinalizedMessage& Message)
{
	if (Message.Handle != FinalizingActionHandle && Message.Handle != RuntimeSnapshot.ActionHandle)
	{
		return;
	}

	FinalizingActionHandle = FWuwaActionHandle();
	RuntimeSnapshot = FWuwaGrappleRuntimeSnapshot();
}

FWuwaActionCapabilityResult
UWuwaGrappleCapabilityComponent::ValidatePrepare(const FWuwaActionPrepareMessage& Message) const
{
	const UWuwaGrappleActionDefinition* Definition = Cast<UWuwaGrappleActionDefinition>(Message.Request.Definition);
	if (!IsInitialized() || !Message.Handle.IsValid() || !Message.Request.IsValid() || !IsValid(Definition) ||
	    !Definition->IsRuntimeValid() || Message.Request.Context.SourceObject.Get() != CharacterOwner ||
	    !Definition->IsContextValid(Message.Request.Context))
	{
		return FWuwaActionCapabilityResult::Failure(EWuwaActionRejectionReason::InvalidDefinition);
	}

	const bool bReplacingCurrent = ActiveActionHandle.IsValid() && Message.ReplacingHandle == ActiveActionHandle;
	if ((ActiveActionHandle.IsValid() || MovementComponent->IsGrappleMovementActive()) && !bReplacingCurrent)
	{
		return FWuwaActionCapabilityResult::Failure(EWuwaActionRejectionReason::InvalidContext);
	}

	const EMovementMode SnapshotMode = Message.Request.Context.MovementMode.GetValue();
	const bool bValidSnapshotMode =
	    SnapshotMode == MOVE_Walking || SnapshotMode == MOVE_NavWalking || SnapshotMode == MOVE_Falling;
	const bool bValidCurrentMode = bReplacingCurrent || MovementComponent->MovementMode == MOVE_Walking ||
	                               MovementComponent->MovementMode == MOVE_NavWalking ||
	                               MovementComponent->MovementMode == MOVE_Falling;
	if (!bValidSnapshotMode || !bValidCurrentMode)
	{
		return FWuwaActionCapabilityResult::Failure(EWuwaActionRejectionReason::InvalidContext);
	}

	const FWuwaGrappleActionContext* FrozenContext =
	    Message.Request.Context.DomainPayload.GetPtr<FWuwaGrappleActionContext>();
	FWuwaGrappleActionContext AdjustedContext;
	FWuwaGrappleQueryResult RevalidationResult;
	if (FrozenContext == nullptr || !FWuwaGrappleQuery::RevalidateFrozenContext(Definition,
	                                                                            CharacterOwner,
	                                                                            CharacterOwner->GetActorLocation(),
	                                                                            *FrozenContext,
	                                                                            AdjustedContext,
	                                                                            RevalidationResult))
	{
		return FWuwaActionCapabilityResult::Failure(EWuwaActionRejectionReason::InvalidContext);
	}

	return FWuwaActionCapabilityResult::Success();
}

void UWuwaGrappleCapabilityComponent::ResetActiveRuntime()
{
	ActiveActionHandle = FWuwaActionHandle();
	ActiveMovementHandle = FWuwaGrappleMovementHandle();
	ActiveDefinition = nullptr;
	ActiveRequest = FWuwaActionRequest();
}

void UWuwaGrappleCapabilityComponent::HandleGrappleMovementFact(const FWuwaGrappleMovementFact& Fact)
{
	if (!ActiveActionHandle.IsValid() || Fact.Handle != ActiveMovementHandle || !Dispatcher.IsValid())
	{
		return;
	}

	if (Fact.FactType == EWuwaGrappleMovementFactType::PhaseChanged)
	{
		RuntimeSnapshot.Phase = Fact.Phase;
		if (Fact.Phase == EWuwaGrappleRuntimePhase::Pulling)
		{
			Dispatcher->PublishActionEvent(ActiveActionHandle,
			                               WuwaGameplayTags::Action_Event_Traversal_Grapple_PullStarted,
			                               Dispatcher->GetCurrentMoveIntent(),
			                               MOVE_Custom,
			                               MOVE_Custom,
			                               static_cast<uint8>(EWuwaCustomMovementMode::Grapple),
			                               static_cast<uint8>(EWuwaCustomMovementMode::Grapple),
			                               this);
		}
		return;
	}

	RuntimeSnapshot.Phase = EWuwaGrappleRuntimePhase::Releasing;
	RuntimeSnapshot.LastMovementEndReason = Fact.EndReason;
	RuntimeSnapshot.ExitVelocity = Fact.ExitVelocity;

	FGameplayTag EventTag;
	switch (Fact.EndReason)
	{
		case EWuwaGrappleMovementEndReason::Released:
			EventTag = WuwaGameplayTags::Action_Event_Traversal_Grapple_Released;
			break;

		case EWuwaGrappleMovementEndReason::Blocked:
			EventTag = WuwaGameplayTags::Action_Event_Traversal_Grapple_Blocked;
			break;

		case EWuwaGrappleMovementEndReason::TimedOut:
		case EWuwaGrappleMovementEndReason::InvalidRuntime:
			EventTag = WuwaGameplayTags::Action_Event_Traversal_Grapple_TimedOut;
			break;

		case EWuwaGrappleMovementEndReason::Landed:
			EventTag = WuwaGameplayTags::Action_Event_Traversal_Grapple_Landed;
			break;

		case EWuwaGrappleMovementEndReason::Stopped:
		case EWuwaGrappleMovementEndReason::NetworkRejected:
		case EWuwaGrappleMovementEndReason::NetworkCorrection:
		case EWuwaGrappleMovementEndReason::Death:
		case EWuwaGrappleMovementEndReason::AvatarChanged:
		case EWuwaGrappleMovementEndReason::EndPlay:
		case EWuwaGrappleMovementEndReason::None:
		default:
			return;
	}

	Dispatcher->PublishActionEvent(ActiveActionHandle,
	                               EventTag,
	                               Dispatcher->GetCurrentMoveIntent(),
	                               MOVE_Custom,
	                               MovementComponent->MovementMode,
	                               static_cast<uint8>(EWuwaCustomMovementMode::Grapple),
	                               MovementComponent->CustomMovementMode,
	                               this);
}

void UWuwaGrappleCapabilityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ActiveMovementHandle.IsValid() && IsValid(MovementComponent))
	{
		MovementComponent->StopGrappleMovement(ActiveMovementHandle, EWuwaGrappleMovementEndReason::EndPlay);
	}

	if (IsValid(MovementComponent))
	{
		MovementComponent->OnGrappleMovementFact.RemoveAll(this);
	}

	ResetActiveRuntime();
	FinalizingActionHandle = FWuwaActionHandle();
	RuntimeSnapshot = FWuwaGrappleRuntimeSnapshot();
	Dispatcher = nullptr;
	MovementComponent = nullptr;
	CharacterOwner = nullptr;
	Super::EndPlay(EndPlayReason);
}
