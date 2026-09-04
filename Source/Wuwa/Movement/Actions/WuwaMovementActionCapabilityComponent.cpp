#include "Movement/Actions/WuwaMovementActionCapabilityComponent.h"

#include "Core/WuwaGameplayTags.h"
#include "GameFramework/RootMotionSource.h"
#include "Messaging/WuwaCharacterMessageDispatcherComponent.h"
#include "Movement/Actions/WuwaMovementActionDefinition.h"
#include "Movement/Actions/WuwaMovementActionPolicies.h"
#include "Movement/WuwaCharacterMovementComponent.h"
#include "WuwaCharacter.h"
#include "Wuwa.h"

namespace
{
constexpr uint16 InvalidRootMotionSourceId = static_cast<uint16>(ERootMotionSourceID::Invalid);
constexpr float MoveCancelThreshold = 0.1f;
}

UWuwaMovementActionCapabilityComponent::UWuwaMovementActionCapabilityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	ActiveRootMotionSourceId = InvalidRootMotionSourceId;
}

bool UWuwaMovementActionCapabilityComponent::Initialize(AWuwaCharacter* InCharacter,
                                                        UWuwaCharacterMovementComponent* InMovementComponent,
                                                        UWuwaCharacterMessageDispatcherComponent* InDispatcher)
{
	if (IsValid(MovementComponent))
	{
		MovementComponent->UnbindMovementActionNetworkProvider(this);
		MovementComponent->OnLandedEvent.RemoveDynamic(this,
		                                               &UWuwaMovementActionCapabilityComponent::HandleLandedEvent);
		if (MovementModeChangedDelegateHandle.IsValid())
		{
			MovementComponent->OnWuwaMovementModeChanged.Remove(MovementModeChangedDelegateHandle);
		}
	}

	CharacterOwner = nullptr;
	MovementComponent = nullptr;
	Dispatcher = nullptr;
	MovementModeChangedDelegateHandle.Reset();

	if (!IsValid(InCharacter) || !IsValid(InMovementComponent) || !IsValid(InDispatcher) || InCharacter != GetOwner() ||
	    InMovementComponent != InCharacter->GetWuwaMovementComponent())
	{
		UE_LOG(LogWuwa, Error, TEXT("Movement Action Capability 初始化失败。Owner=%s"), *GetNameSafe(GetOwner()));
		return false;
	}

	CharacterOwner = InCharacter;
	MovementComponent = InMovementComponent;
	Dispatcher = InDispatcher;

	if (!MovementComponent->BindMovementActionNetworkProvider(this))
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Movement Action Capability 无法绑定移动重演入口。Owner=%s"),
		       *GetNameSafe(GetOwner()));
		CharacterOwner = nullptr;
		MovementComponent = nullptr;
		Dispatcher = nullptr;
		return false;
	}

	MovementComponent->OnLandedEvent.AddDynamic(this, &UWuwaMovementActionCapabilityComponent::HandleLandedEvent);
	MovementModeChangedDelegateHandle = MovementComponent->OnWuwaMovementModeChanged.AddUObject(
	    this, &UWuwaMovementActionCapabilityComponent::HandleMovementModeChangedEvent);
	return true;
}

bool UWuwaMovementActionCapabilityComponent::IsInitialized() const
{
	return IsValid(CharacterOwner) && IsValid(MovementComponent) && Dispatcher.IsValid();
}

bool UWuwaMovementActionCapabilityComponent::PrepareNetworkActionExitTransition(
    const FWuwaPendingNetworkMovementCommand& Command, EWuwaActionEndReason& OutEndReason)
{
	OutEndReason = EWuwaActionEndReason::None;
	const bool bMatchesActiveAction = PhysicalRuntime.bMovementActionActive &&
	                                  PhysicalRuntime.Generation == Command.TargetActionGeneration &&
	                                  PhysicalRuntime.ActionTag == Command.ActionTag;
	if (!IsInitialized() || !CharacterOwner->HasAuthority() || !Command.IsLegacyActionExit() ||
	    !Command.IsPayloadValid() || !bMatchesActiveAction || !HasConfiguredActionExit(Command.ExitKind))
	{
		return false;
	}

	if (PreparedNetworkExitGeneration.IsValid())
	{
		if (PreparedNetworkExitGeneration != Command.Generation)
		{
			return false;
		}
		OutEndReason = Command.ExitKind == EWuwaLegacyActionExitKind::MoveCancel ? EWuwaActionEndReason::Cancelled
		                                                                         : EWuwaActionEndReason::Completed;
		return true;
	}

	if (Command.ExitKind == EWuwaLegacyActionExitKind::MoveCancel)
	{
		PhysicalRuntime.bPreserveVelocityOnRelease = false;
		PhysicalRuntime.ExitPolicy = EWuwaMovementActionExitPolicy::None;
		PhysicalRuntime.ExitMoveIntent = Command.ExitMoveIntent.GetClampedToMaxSize(1.f);
		OutEndReason = EWuwaActionEndReason::Cancelled;
	}
	else if (Command.ExitKind == EWuwaLegacyActionExitKind::CompletedExit)
	{
		if (!PrepareCompletedExit(Command.ExitMoveIntent))
		{
			return false;
		}
		OutEndReason = EWuwaActionEndReason::Completed;
	}
	else
	{
		return false;
	}

	PreparedNetworkExitGeneration = Command.Generation;
	return true;
}

bool UWuwaMovementActionCapabilityComponent::HasConfiguredActionExit(const EWuwaLegacyActionExitKind ExitKind) const
{
	if (!IsValid(PhysicalActionDefinition) || ExitKind == EWuwaLegacyActionExitKind::None)
	{
		return false;
	}

	EWuwaMovementActionEventResponse RequiredResponse;
	if (ExitKind == EWuwaLegacyActionExitKind::MoveCancel)
	{
		RequiredResponse = EWuwaMovementActionEventResponse::OpenMoveCancelWindow;
	}
	else if (ExitKind == EWuwaLegacyActionExitKind::CompletedExit)
	{
		RequiredResponse = EWuwaMovementActionEventResponse::RequestCompletedExit;
	}
	else
	{
		return false;
	}

	for (const FWuwaMovementActionEventBinding& Binding : PhysicalActionDefinition->EventBindings)
	{
		if (Binding.EventTag.IsValid() && Binding.Response == RequiredResponse)
		{
			return true;
		}
	}
	return false;
}

bool UWuwaMovementActionCapabilityComponent::ApplyNetworkActionExitTransition(
    const FWuwaPendingNetworkMovementCommand& Command, const bool bIsReplay)
{
	if (!Command.IsLegacyActionExit() || !Command.IsPayloadValid())
	{
		return false;
	}

	if (PhysicalRuntime.LastAppliedExitGeneration == Command.Generation)
	{
		return true;
	}

	if (!PhysicalRuntime.bMovementActionActive || PhysicalRuntime.Generation != Command.TargetActionGeneration ||
	    PhysicalRuntime.ActionTag != Command.ActionTag)
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("MovementAction 退出命令与物理状态不匹配。Owner=%s, ExitGeneration=%d, TargetGeneration=%d, "
		            "PhysicalGeneration=%d, Action=%s, Replay=%d"),
		       *GetNameSafe(CharacterOwner),
		       Command.Generation.Value,
		       Command.TargetActionGeneration.Value,
		       PhysicalRuntime.Generation.Value,
		       *Command.ActionTag.ToString(),
		       bIsReplay ? 1 : 0);
		return false;
	}

	PhysicalRuntime.ExitMoveIntent = Command.ExitMoveIntent.GetClampedToMaxSize(1.f);
	if (Command.ExitKind == EWuwaLegacyActionExitKind::MoveCancel)
	{
		PhysicalRuntime.bPreserveVelocityOnRelease = false;
		PhysicalRuntime.ExitPolicy = EWuwaMovementActionExitPolicy::None;
	}
	else if (Command.ExitKind == EWuwaLegacyActionExitKind::CompletedExit)
	{
		PhysicalRuntime.bPreserveVelocityOnRelease = true;
	}
	return FinalizePhysicalMovementAction(Command.Generation, bIsReplay);
}

bool UWuwaMovementActionCapabilityComponent::ValidateLocalPredictedRootMotionMoveStart(
    const FWuwaPendingNetworkMovementCommand& Command, const float MoveStartTime) const
{
	if (Command.Kind != EWuwaNetworkMovementCommandKind::LegacyAction)
	{
		return true;
	}

	const bool bMatchingPhysicalRuntime = Command.IsPayloadValid() && PhysicalRuntime.bMovementActionActive &&
	                                      PhysicalRuntime.Generation == Command.Generation &&
	                                      PhysicalRuntime.ActionTag == Command.ActionTag;
	if (!bMatchingPhysicalRuntime || !FMath::IsFinite(MoveStartTime) || MoveStartTime < 0.f)
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Owning Client RMS Move 起点校验前置状态无效。Owner=%s, Generation=%d, PhysicalGeneration=%d, "
		            "Action=%s, PhysicalAction=%s, MoveStartTime=%.6f"),
		       *GetNameSafe(CharacterOwner),
		       Command.Generation.Value,
		       PhysicalRuntime.Generation.Value,
		       *Command.ActionTag.ToString(),
		       *PhysicalRuntime.ActionTag.ToString(),
		       MoveStartTime);
		return false;
	}

	if (PhysicalRuntime.Driver != EWuwaMovementActionDriver::RootMotionSource)
	{
		return true;
	}

	const FName ExpectedInstanceName = BuildNetworkRootMotionSourceName(Command.Generation, Command.ActionTag);
	const TSharedPtr<FRootMotionSource> RootMotionSource = ResolveActiveRootMotionSource();
	if (!RootMotionSource.IsValid() || RootMotionSource->InstanceName != ExpectedInstanceName)
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Owning Client RMS Move 起点校验缺少稳定资源。Owner=%s, Generation=%d, Action=%s, ExpectedName=%s, "
		            "ActualName=%s"),
		       *GetNameSafe(CharacterOwner),
		       Command.Generation.Value,
		       *Command.ActionTag.ToString(),
		       *ExpectedInstanceName.ToString(),
		       RootMotionSource.IsValid() ? *RootMotionSource->InstanceName.ToString() : TEXT("None"));
		return false;
	}

	constexpr float RootMotionMoveStartTolerance = 0.001f;
	const float StartTime = RootMotionSource->GetStartTime();
	const float CurrentTime = RootMotionSource->GetTime();
	const bool bValidTimePhase = FMath::IsFinite(StartTime) && FMath::IsFinite(CurrentTime) &&
	                             FMath::IsNearlyZero(CurrentTime, RootMotionMoveStartTolerance) &&
	                             FMath::Abs(StartTime - MoveStartTime) <= RootMotionMoveStartTolerance;
	if (!bValidTimePhase)
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Owning Client RMS 未保持原生 Move 起点。Owner=%s, Generation=%d, Action=%s, Name=%s, "
		            "StartTime=%.6f, CurrentTime=%.6f, MoveStartTime=%.6f"),
		       *GetNameSafe(CharacterOwner),
		       Command.Generation.Value,
		       *Command.ActionTag.ToString(),
		       *ExpectedInstanceName.ToString(),
		       StartTime,
		       CurrentTime,
		       MoveStartTime);
		return false;
	}

	return true;
}

bool UWuwaMovementActionCapabilityComponent::AlignAuthorityRootMotionMoveStart(
    const FWuwaPendingNetworkMovementCommand& Command, const float MoveStartTime)
{
	if (Command.Kind != EWuwaNetworkMovementCommandKind::LegacyAction)
	{
		return true;
	}

	const bool bMatchingPhysicalRuntime = Command.IsPayloadValid() && PhysicalRuntime.bMovementActionActive &&
	                                      PhysicalRuntime.Generation == Command.Generation &&
	                                      PhysicalRuntime.ActionTag == Command.ActionTag;
	if (!bMatchingPhysicalRuntime || !FMath::IsFinite(MoveStartTime) || MoveStartTime < 0.f)
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Authority RMS Move 起点对齐前置状态无效。Owner=%s, Generation=%d, PhysicalGeneration=%d, "
		            "Action=%s, PhysicalAction=%s, MoveStartTime=%.6f"),
		       *GetNameSafe(CharacterOwner),
		       Command.Generation.Value,
		       PhysicalRuntime.Generation.Value,
		       *Command.ActionTag.ToString(),
		       *PhysicalRuntime.ActionTag.ToString(),
		       MoveStartTime);
		return false;
	}

	if (PhysicalRuntime.Driver != EWuwaMovementActionDriver::RootMotionSource)
	{
		return true;
	}

	const FName ExpectedInstanceName = BuildNetworkRootMotionSourceName(Command.Generation, Command.ActionTag);
	TSharedPtr<FRootMotionSource> RootMotionSource = ResolveActiveRootMotionSource();
	if (!RootMotionSource.IsValid() || RootMotionSource->InstanceName != ExpectedInstanceName)
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Authority RMS Move 起点对齐缺少稳定资源。Owner=%s, Generation=%d, Action=%s, ExpectedName=%s, "
		            "ActualName=%s"),
		       *GetNameSafe(CharacterOwner),
		       Command.Generation.Value,
		       *Command.ActionTag.ToString(),
		       *ExpectedInstanceName.ToString(),
		       RootMotionSource.IsValid() ? *RootMotionSource->InstanceName.ToString() : TEXT("None"));
		return false;
	}

	constexpr float RootMotionMoveStartTolerance = 0.001f;
	const float CurrentTime = RootMotionSource->GetTime();
	const float PreviousStartTime = RootMotionSource->GetStartTime();
	if (!FMath::IsFinite(CurrentTime) || !FMath::IsFinite(PreviousStartTime) ||
	    !FMath::IsNearlyZero(CurrentTime, RootMotionMoveStartTolerance))
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Authority RMS 已在 Move 起点对齐前推进。Owner=%s, Generation=%d, Action=%s, Name=%s, "
		            "StartTime=%.6f, CurrentTime=%.6f, MoveStartTime=%.6f"),
		       *GetNameSafe(CharacterOwner),
		       Command.Generation.Value,
		       *Command.ActionTag.ToString(),
		       *ExpectedInstanceName.ToString(),
		       PreviousStartTime,
		       CurrentTime,
		       MoveStartTime);
		return false;
	}

	if (FMath::Abs(PreviousStartTime - MoveStartTime) > RootMotionMoveStartTolerance)
	{
		RootMotionSource->StartTime = MoveStartTime;
	}
	return true;
}

void UWuwaMovementActionCapabilityComponent::CaptureNetworkReplayState(
    FWuwaMovementActionNetworkReplayState& OutState) const
{
	OutState = PhysicalRuntime;
	if (!OutState.bMovementActionActive)
	{
		OutState.Generation = FWuwaNetworkActionGeneration();
		OutState.ActionTag = FGameplayTag();
		OutState.ExitMoveIntent = FVector2D::ZeroVector;
		OutState.ExitPolicy = EWuwaMovementActionExitPolicy::None;
		OutState.bFacingOverrideActive = false;
		OutState.bPreserveVelocityOnRelease = false;
		if (IsValid(MovementComponent))
		{
			OutState.bSavedOrientRotationToMovement = MovementComponent->bOrientRotationToMovement;
			OutState.bSavedUseControllerDesiredRotation = MovementComponent->bUseControllerDesiredRotation;
		}
		if (IsValid(CharacterOwner))
		{
			OutState.bSavedUseControllerRotationYaw = CharacterOwner->bUseControllerRotationYaw;
		}
	}
}

void UWuwaMovementActionCapabilityComponent::RestoreNetworkReplayState(
    const FWuwaMovementActionNetworkReplayState& State)
{
	if (!State.IsPayloadValid() || !IsInitialized())
	{
		UE_LOG(
		    LogWuwa, Error, TEXT("MovementAction 拒绝无效 SavedMove 物理状态。Owner=%s"), *GetNameSafe(CharacterOwner));
		return;
	}

	PhysicalRuntime = State;
	PreparedNetworkExitGeneration = FWuwaNetworkActionGeneration();
	ActiveRootMotionSourceId = InvalidRootMotionSourceId;
	ActiveRootMotionSourceInstanceName =
	    State.bMovementActionActive && State.Driver == EWuwaMovementActionDriver::RootMotionSource
	        ? BuildNetworkRootMotionSourceName(State.Generation, State.ActionTag)
	        : NAME_None;

	MovementComponent->bOrientRotationToMovement =
	    State.bFacingOverrideActive ? false : State.bSavedOrientRotationToMovement;
	MovementComponent->bUseControllerDesiredRotation =
	    State.bFacingOverrideActive ? false : State.bSavedUseControllerDesiredRotation;
	CharacterOwner->bUseControllerRotationYaw =
	    State.bFacingOverrideActive ? false : State.bSavedUseControllerRotationYaw;
}

FGameplayTag UWuwaMovementActionCapabilityComponent::GetActionCapabilityTag() const
{
	return WuwaGameplayTags::Action_Capability_Movement;
}

int32 UWuwaMovementActionCapabilityComponent::GetActionCapabilityCommitOrder() const
{
	return 200;
}

FWuwaActionCapabilityResult
UWuwaMovementActionCapabilityComponent::PrepareAction(const FWuwaActionPrepareMessage& Message) const
{
	return ValidatePrepare(Message);
}

FWuwaActionCapabilityResult
UWuwaMovementActionCapabilityComponent::CommitAction(const FWuwaActionCommitMessage& Message)
{
	if (ActiveHandle.IsValid() || PhysicalRuntime.bMovementActionActive)
	{
		return FWuwaActionCapabilityResult::Failure(EWuwaActionRejectionReason::CapabilityCommitFailed);
	}

	FWuwaActionPrepareMessage PrepareMessage;
	PrepareMessage.Handle = Message.Handle;
	PrepareMessage.Request = Message.Request;
	const FWuwaActionCapabilityResult PrepareResult = ValidatePrepare(PrepareMessage);
	if (!PrepareResult.bSucceeded)
	{
		return PrepareResult;
	}

	UWuwaMovementActionDefinition* Definition = Cast<UWuwaMovementActionDefinition>(Message.Request.Definition);
	ActiveHandle = Message.Handle;
	ActiveDefinition = Definition;
	ActiveRequest = Message.Request;

	const FWuwaNetworkActionGeneration LastAppliedExitGeneration = PhysicalRuntime.LastAppliedExitGeneration;
	PhysicalRuntime.Reset();
	PhysicalRuntime.LastAppliedExitGeneration = LastAppliedExitGeneration;
	PhysicalRuntime.Generation = Message.Request.NetworkGeneration;
	PhysicalRuntime.ActionTag = Definition->ActionTag;
	PhysicalRuntime.Driver = Definition->MovementDriver;
	PhysicalRuntime.ExitPolicy = Definition->ExitPolicy;
	PhysicalRuntime.bMovementActionActive = true;
	PhysicalActionDefinition = Definition;

	if (!AcquireFacingRotationOverride(Definition->FacingPolicy, Message.Request.Context.WorldDirection) ||
	    !CommitMovementDriver(Message))
	{
		ReleaseActiveResources();
		return FWuwaActionCapabilityResult::Failure(EWuwaActionRejectionReason::CapabilityCommitFailed);
	}

	MovementComponent->ExitSprintRun();
	return FWuwaActionCapabilityResult::Success();
}

void UWuwaMovementActionCapabilityComponent::RollbackAction(const FWuwaActionHandle& Handle)
{
	if (Handle == ActiveHandle)
	{
		ReleaseActiveResources();
	}
}

void UWuwaMovementActionCapabilityComponent::StopAction(const FWuwaActionStopMessage& Message)
{
	if (Message.Handle != ActiveHandle)
	{
		return;
	}

	if (Message.EndReason == EWuwaActionEndReason::Cancelled)
	{
		PhysicalRuntime.bPreserveVelocityOnRelease = false;
		PhysicalRuntime.ExitPolicy = EWuwaMovementActionExitPolicy::None;
	}
	else if (Message.EndReason != EWuwaActionEndReason::Completed)
	{
		PhysicalRuntime.ExitPolicy = EWuwaMovementActionExitPolicy::None;
		PhysicalRuntime.ExitMoveIntent = FVector2D::ZeroVector;
	}

	if (ShouldDeferPhysicalFinalize(Message))
	{
		ResetActiveRuntime();
		return;
	}

	FinalizePhysicalMovementAction(FWuwaNetworkActionGeneration(), false);
	ResetActiveRuntime();
}

EWuwaActionEndReason UWuwaMovementActionCapabilityComponent::HandleActionEvent(const FWuwaActionEventMessage& Message)
{
	if (Message.Handle != ActiveHandle || !IsValid(ActiveDefinition))
	{
		return EWuwaActionEndReason::None;
	}

	const bool bWaitForNetworkExitCommand = CharacterOwner->HasAuthority() && !CharacterOwner->IsLocallyControlled() &&
	                                        ActiveRequest.NetworkGeneration.IsValid();

	if (Message.EventTag == WuwaGameplayTags::Action_Event_Movement_Landed &&
	    (ActiveDefinition->MovementCondition == EWuwaMovementActionCondition::Falling ||
	     ActiveDefinition->MovementCondition == EWuwaMovementActionCondition::Airborne))
	{
		return EWuwaActionEndReason::Interrupted;
	}

	if (Message.EventTag == WuwaGameplayTags::Action_Event_Movement_LeftGround &&
	    ActiveDefinition->MovementCondition == EWuwaMovementActionCondition::Grounded)
	{
		PhysicalRuntime.bPreserveVelocityOnRelease = ActiveDefinition->bPreserveVelocityWhenLeavingGround;
		return EWuwaActionEndReason::Interrupted;
	}

	if (Message.EventTag == WuwaGameplayTags::Action_Event_Input_MoveChanged)
	{
		if (bMoveCancelWindowOpen && !Message.MoveIntent.IsNearlyZero(MoveCancelThreshold))
		{
			bMoveCancelWindowOpen = false;
			PhysicalRuntime.bPreserveVelocityOnRelease = false;
			PhysicalRuntime.ExitPolicy = EWuwaMovementActionExitPolicy::None;
			PhysicalRuntime.ExitMoveIntent = Message.MoveIntent.GetClampedToMaxSize(1.f);
			return bWaitForNetworkExitCommand ? EWuwaActionEndReason::None : EWuwaActionEndReason::Cancelled;
		}
		if (bCompletedExitReady && PrepareCompletedExit(Message.MoveIntent))
		{
			bCompletedExitReady = false;
			return bWaitForNetworkExitCommand ? EWuwaActionEndReason::None : EWuwaActionEndReason::Completed;
		}
		return EWuwaActionEndReason::None;
	}

	EWuwaMovementActionEventResponse Response;
	if (!ResolveEventResponse(Message.EventTag, Response))
	{
		return EWuwaActionEndReason::None;
	}

	if (Response == EWuwaMovementActionEventResponse::OpenMoveCancelWindow)
	{
		bMoveCancelWindowOpen = true;
		if (!Message.MoveIntent.IsNearlyZero(MoveCancelThreshold))
		{
			bMoveCancelWindowOpen = false;
			PhysicalRuntime.bPreserveVelocityOnRelease = false;
			PhysicalRuntime.ExitPolicy = EWuwaMovementActionExitPolicy::None;
			PhysicalRuntime.ExitMoveIntent = Message.MoveIntent.GetClampedToMaxSize(1.f);
			return bWaitForNetworkExitCommand ? EWuwaActionEndReason::None : EWuwaActionEndReason::Cancelled;
		}
		return EWuwaActionEndReason::None;
	}

	if (Response == EWuwaMovementActionEventResponse::RequestCompletedExit)
	{
		bCompletedExitReady = true;
		if (PrepareCompletedExit(Message.MoveIntent))
		{
			bCompletedExitReady = false;
			return bWaitForNetworkExitCommand ? EWuwaActionEndReason::None : EWuwaActionEndReason::Completed;
		}
		return EWuwaActionEndReason::None;
	}

	return EWuwaActionEndReason::None;
}

void UWuwaMovementActionCapabilityComponent::HandleActionFinalized(const FWuwaActionFinalizedMessage& Message)
{
	(void)Message;
}

FWuwaActionCapabilityResult
UWuwaMovementActionCapabilityComponent::ValidatePrepare(const FWuwaActionPrepareMessage& Message) const
{
	const UWuwaMovementActionDefinition* Definition = Cast<UWuwaMovementActionDefinition>(Message.Request.Definition);
	if (!IsInitialized() || !Message.Handle.IsValid() || !Message.Request.IsValid() || !IsValid(Definition) ||
	    !Definition->IsRuntimeValid() || Message.Request.Context.SourceObject.Get() != CharacterOwner)
	{
		return FWuwaActionCapabilityResult::Failure(EWuwaActionRejectionReason::InvalidDefinition);
	}

	const FWuwaActionContext& Context = Message.Request.Context;
	if (Context.WorldDirection.IsNearlyZero() || Context.WorldDirection.ContainsNaN())
	{
		return FWuwaActionCapabilityResult::Failure(EWuwaActionRejectionReason::InvalidContext);
	}

	if (!FWuwaMovementActionPolicies::MatchesMovementCondition(Definition->MovementCondition,
	                                                           Context.MovementMode,
	                                                           Context.CustomMovementMode,
	                                                           MovementComponent->IsMovingOnGround(),
	                                                           MovementComponent->IsFalling(),
	                                                           MovementComponent->IsGrappleMovementActive()))
	{
		return FWuwaActionCapabilityResult::Failure(EWuwaActionRejectionReason::InvalidContext);
	}

	if (Definition->MovementDriver == EWuwaMovementActionDriver::AirJump &&
	    !MovementComponent->CanPerformAirDoubleJump())
	{
		return FWuwaActionCapabilityResult::Failure(EWuwaActionRejectionReason::InvalidContext);
	}

	if (Definition->MovementDriver == EWuwaMovementActionDriver::RootMotionSource &&
	    !Definition->RootMotionSourceConfig.IsRuntimeValid())
	{
		return FWuwaActionCapabilityResult::Failure(EWuwaActionRejectionReason::InvalidDefinition);
	}

	return FWuwaActionCapabilityResult::Success();
}
