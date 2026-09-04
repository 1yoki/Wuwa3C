#include "Actions/Network/WuwaActionNetworkComponent.h"

#include "Actions/Data/WuwaActionDefinition.h"
#include "Actions/Data/WuwaActionRuleSet.h"
#include "Actions/Runtime/WuwaActionCoordinatorComponent.h"
#include "Animation/Actions/WuwaActionAnimationCapabilityComponent.h"
#include "Core/WuwaGameplayTags.h"
#include "GameFramework/GameStateBase.h"
#include "Movement/Actions/WuwaMovementActionCapabilityComponent.h"
#include "Movement/Actions/WuwaMovementActionDefinition.h"
#include "Movement/WuwaCharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Traversal/Actions/WuwaGrappleCapabilityComponent.h"
#include "Traversal/Data/WuwaGrappleActionDefinition.h"
#include "Traversal/Data/WuwaTraversalProfile.h"
#include "Traversal/Query/WuwaGrappleQuery.h"
#include "Wuwa.h"
#include "WuwaCharacter.h"

namespace
{
FWuwaActionResult MakeRejectedActionResult(const FWuwaResolvedActionIntent& Intent,
                                           const EWuwaActionRejectionReason Reason)
{
	FWuwaActionResult Result;
	Result.ActionTag = IsValid(Intent.Request.Definition) ? Intent.Request.Definition->ActionTag : FGameplayTag();
	Result.SourceSequence = Intent.Request.Header.Sequence;
	Result.NetworkGeneration = Intent.Request.NetworkGeneration;
	Result.RejectionReason = Reason;
	return Result;
}
}

UWuwaActionNetworkComponent::UWuwaActionNetworkComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

bool UWuwaActionNetworkComponent::Initialize(AWuwaCharacter* InCharacter,
                                             UWuwaActionCoordinatorComponent* InCoordinator,
                                             UWuwaCharacterMovementComponent* InMovement,
                                             UWuwaMovementActionCapabilityComponent* InMovementCapability,
                                             UWuwaActionAnimationCapabilityComponent* InAnimationCapability,
                                             const UWuwaActionRuleSet* InActionRuleSet,
                                             const UWuwaTraversalProfile* InTraversalProfile)
{
	ShutdownBindings();
	DefinitionRegistry.Reset();
	GrappleCapability = nullptr;
	GrappleNetworkSnapshot = FWuwaGrappleNetworkRuntimeSnapshot();

	CharacterOwner = InCharacter;
	Coordinator = InCoordinator;
	Movement = InMovement;
	MovementCapability = InMovementCapability;
	AnimationCapability = InAnimationCapability;

	const bool bValidDependencies =
	    IsValid(CharacterOwner) && IsValid(Coordinator) && IsValid(Movement) && IsValid(MovementCapability) &&
	    IsValid(AnimationCapability) && IsValid(InActionRuleSet) && CharacterOwner == GetOwner() &&
	    Coordinator->GetOwner() == CharacterOwner && Movement->GetOwner() == CharacterOwner &&
	    MovementCapability->GetOwner() == CharacterOwner && AnimationCapability->GetOwner() == CharacterOwner;
	if (!bValidDependencies || !AddRuleSetToRegistry(InActionRuleSet))
	{
		UE_LOG(LogWuwa, Error, TEXT("Action Network 初始化失败。Owner=%s"), *GetNameSafe(GetOwner()));
		ShutdownBindings();
		DefinitionRegistry.Reset();
		CharacterOwner = nullptr;
		Coordinator = nullptr;
		Movement = nullptr;
		MovementCapability = nullptr;
		AnimationCapability = nullptr;
		return false;
	}

	if (IsValid(InTraversalProfile) && IsValid(InTraversalProfile->TraversalRuleSet) &&
	    !AddRuleSetToRegistry(InTraversalProfile->TraversalRuleSet))
	{
		UE_LOG(LogWuwa, Error, TEXT("Action Network Traversal Registry 冲突。Owner=%s"), *GetNameSafe(GetOwner()));
		ShutdownBindings();
		DefinitionRegistry.Reset();
		CharacterOwner = nullptr;
		Coordinator = nullptr;
		Movement = nullptr;
		MovementCapability = nullptr;
		AnimationCapability = nullptr;
		return false;
	}

	ActionStartedDelegateHandle =
	    Coordinator->OnActionStarted.AddUObject(this, &UWuwaActionNetworkComponent::HandleAuthorityActionStarted);
	ActionFinalizedDelegateHandle =
	    Coordinator->OnActionFinalized.AddUObject(this, &UWuwaActionNetworkComponent::HandleAuthorityActionFinalized);

	bool bBindingsValid = true;
	if (CharacterOwner->HasAuthority())
	{
		bBindingsValid = Movement->BindNetworkCommandProcessor(
		    this,
		    FWuwaNetworkCommandProcessor::CreateUObject(this,
		                                                &UWuwaActionNetworkComponent::ProcessServerMovementCommand));
	}
	else if (CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy)
	{
		bBindingsValid =
		    Movement->BindNetworkMoveResponseConsumer(this,
		                                              FWuwaNetworkMoveResponseConsumer::CreateUObject(
		                                                  this, &UWuwaActionNetworkComponent::HandleMoveResponse)) &&
		    Movement->BindNetworkMoveReplayConsumer(this,
		                                            FWuwaNetworkMoveReplayConsumer::CreateUObject(
		                                                this, &UWuwaActionNetworkComponent::HandlePredictedMoveReplay));
	}

	if (!bBindingsValid)
	{
		UE_LOG(LogWuwa, Error, TEXT("Action Network 唯一 CMC 委托绑定失败。Owner=%s"), *GetNameSafe(GetOwner()));
		ShutdownBindings();
		DefinitionRegistry.Reset();
		CharacterOwner = nullptr;
		Coordinator = nullptr;
		Movement = nullptr;
		MovementCapability = nullptr;
		AnimationCapability = nullptr;
		return false;
	}

	return true;
}

bool UWuwaActionNetworkComponent::BindGrappleCapability(UWuwaGrappleCapabilityComponent* InGrappleCapability)
{
	if (!IsInitialized() || !IsValid(InGrappleCapability) || InGrappleCapability->GetOwner() != CharacterOwner ||
	    !InGrappleCapability->IsInitialized())
	{
		UE_LOG(LogWuwa, Error, TEXT("Action Network Grapple 依赖绑定失败。Owner=%s"), *GetNameSafe(CharacterOwner));
		GrappleCapability = nullptr;
		return false;
	}

	GrappleCapability = InGrappleCapability;
	return true;
}

FWuwaActionResult UWuwaActionNetworkComponent::RouteResolvedIntent(const FWuwaResolvedActionIntent& Intent)
{
	if (!IsInitialized() || !Intent.IsValid() || !IsValid(Intent.Request.Definition))
	{
		return MakeRejectedActionResult(Intent, EWuwaActionRejectionReason::InvalidDefinition);
	}

	const TObjectPtr<UWuwaActionDefinition>* RegisteredDefinition =
	    DefinitionRegistry.Find(Intent.Request.Definition->ActionTag);
	if (RegisteredDefinition == nullptr || *RegisteredDefinition != Intent.Request.Definition)
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Action Network 拒绝未注册或冲突 Definition。Owner=%s, Action=%s"),
		       *GetNameSafe(CharacterOwner),
		       *Intent.Request.Definition->ActionTag.ToString());
		return MakeRejectedActionResult(Intent, EWuwaActionRejectionReason::InvalidDefinition);
	}

	const UWuwaMovementActionDefinition* MovementDefinition =
	    Cast<UWuwaMovementActionDefinition>(Intent.Request.Definition);
	const UWuwaGrappleActionDefinition* GrappleDefinition =
	    Cast<UWuwaGrappleActionDefinition>(Intent.Request.Definition);
	const bool bStandalone = CharacterOwner->GetNetMode() == NM_Standalone;
	if (!IsValid(MovementDefinition) && !IsValid(GrappleDefinition))
	{
		if (bStandalone && CharacterOwner->HasAuthority())
		{
			return Coordinator->EnqueueIntent(Intent);
		}

		return MakeRejectedActionResult(Intent, EWuwaActionRejectionReason::UnsupportedNetworkCommand);
	}

	if (IsValid(GrappleDefinition) && !IsValid(GrappleCapability))
	{
		UE_LOG(LogWuwa, Error, TEXT("Action Network Grapple Runtime 尚未绑定。Owner=%s"), *GetNameSafe(CharacterOwner));
		return MakeRejectedActionResult(Intent, EWuwaActionRejectionReason::InvalidContext);
	}

	if (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		return MakeRejectedActionResult(Intent, EWuwaActionRejectionReason::InvalidContext);
	}

	FWuwaResolvedActionIntent RoutedIntent = Intent;
	RoutedIntent.Request.NetworkGeneration = Movement->AllocateNetworkActionGeneration();
	if (!RoutedIntent.Request.NetworkGeneration.IsValid())
	{
		return MakeRejectedActionResult(RoutedIntent, EWuwaActionRejectionReason::InvalidContext);
	}

	if (CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy &&
	    (LastLegacyRouteFrame == static_cast<uint64>(GFrameCounter) || !Movement->CanQueueNetworkMovementCommand()))
	{
		return MakeRejectedActionResult(RoutedIntent, EWuwaActionRejectionReason::NetworkCommandSlotOccupied);
	}

	FWuwaPendingNetworkMovementCommand Command;
	const bool bBuiltCommand = IsValid(GrappleDefinition) ? BuildGrappleMovementCommand(RoutedIntent, Command)
	                                                      : BuildLegacyMovementCommand(RoutedIntent, Command);
	const bool bAppliedFacing =
	    bBuiltCommand && (IsValid(GrappleDefinition) ? ApplyLocalGrappleFacing(Command)
	                                                 : ApplyLocalActionFacing(Command, MovementDefinition));
	if (!bBuiltCommand || !bAppliedFacing)
	{
		return MakeRejectedActionResult(RoutedIntent, EWuwaActionRejectionReason::InvalidContext);
	}

	if (CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy)
	{
		LastLegacyRouteFrame = static_cast<uint64>(GFrameCounter);
		PendingCommandsByGeneration.Add(Command.Generation.Value, Command);
		FWuwaActionResult Result = Coordinator->EnqueueIntent(RoutedIntent);
		if (Result.Status == EWuwaActionRequestStatus::Rejected)
		{
			RemovePendingGeneration(Command.Generation);
			if (Command.Kind == EWuwaNetworkMovementCommandKind::Grapple)
			{
				GrappleNetworkSnapshot.PredictedGeneration = FWuwaNetworkActionGeneration();
			}
			return Result;
		}

		Coordinator->PumpQueue();
		const FWuwaActionRuntimeSnapshot Snapshot = Coordinator->GetRuntimeSnapshot();
		if (Snapshot.LastResult.NetworkGeneration == Command.Generation)
		{
			Result = Snapshot.LastResult;
		}
		return Result;
	}

	FWuwaActionResult Result = Coordinator->EnqueueIntent(RoutedIntent);
	if (Result.Status != EWuwaActionRequestStatus::Rejected)
	{
		Coordinator->PumpQueue();
		const FWuwaActionRuntimeSnapshot Snapshot = Coordinator->GetRuntimeSnapshot();
		if (Snapshot.LastResult.NetworkGeneration == RoutedIntent.Request.NetworkGeneration)
		{
			Result = Snapshot.LastResult;
		}
	}
	return Result;
}

FWuwaNetworkActionResponse
UWuwaActionNetworkComponent::ProcessServerMovementCommand(const FWuwaPendingNetworkMovementCommand& Command)
{
	++AuthorityProcessorCount;
	if (!IsInitialized() || !CharacterOwner->HasAuthority() || !Command.IsPayloadValid())
	{
		return FWuwaNetworkActionResponse::Rejected(Command.Generation, EWuwaNetworkActionRejectReason::InvalidPayload);
	}

	if (Command.Generation.Value < LastAuthorityGeneration.Value)
	{
		return FWuwaNetworkActionResponse::Rejected(Command.Generation,
		                                            EWuwaNetworkActionRejectReason::StaleGeneration);
	}
	if (Command.Generation == LastAuthorityGeneration)
	{
		if (Command.IsLegacyActionExit() && Command.TargetActionGeneration == LastFinalizedAuthorityActionGeneration &&
		    LegacyActionPresentation.NetworkGeneration == Command.TargetActionGeneration &&
		    LegacyActionPresentation.ActionTag == Command.ActionTag)
		{
			return FWuwaNetworkActionResponse::Accepted(Command.Generation);
		}
		return FWuwaNetworkActionResponse::Rejected(Command.Generation,
		                                            EWuwaNetworkActionRejectReason::DuplicateGeneration);
	}

	LastAuthorityGeneration = Command.Generation;
	if (Command.Kind == EWuwaNetworkMovementCommandKind::LegacyActionExit)
	{
		return ProcessServerLegacyActionExit(Command);
	}
	if (Command.Kind == EWuwaNetworkMovementCommandKind::Grapple)
	{
		return ProcessServerGrappleCommand(Command);
	}
	if (Command.Kind != EWuwaNetworkMovementCommandKind::LegacyAction)
	{
		return FWuwaNetworkActionResponse::Rejected(Command.Generation,
		                                            EWuwaNetworkActionRejectReason::UnsupportedCommandKind);
	}

	UWuwaMovementActionDefinition* Definition =
	    Cast<UWuwaMovementActionDefinition>(DefinitionRegistry.FindRef(Command.ActionTag));
	if (!IsValid(Definition) || !Definition->IsRuntimeValid())
	{
		UE_LOG(LogWuwa,
		       Warning,
		       TEXT("服务端拒绝未知 Legacy Action。Owner=%s, Action=%s, Generation=%d"),
		       *GetNameSafe(CharacterOwner),
		       *Command.ActionTag.ToString(),
		       Command.Generation.Value);
		return FWuwaNetworkActionResponse::Rejected(Command.Generation,
		                                            EWuwaNetworkActionRejectReason::ProcessorRejected);
	}

	if (Definition->MovementDriver == EWuwaMovementActionDriver::AirJump)
	{
		const int32 CurrentAirCycle = Movement->GetAirCycleGeneration();
		if (Command.AirCycleGeneration <= 0 || Command.AirCycleGeneration != CurrentAirCycle ||
		    Command.AirCycleGeneration <= LastAuthorityAirCycleGeneration || !Movement->CanPerformAirDoubleJump())
		{
			return FWuwaNetworkActionResponse::Rejected(Command.Generation,
			                                            EWuwaNetworkActionRejectReason::ProcessorRejected);
		}
	}
	else if (Command.AirCycleGeneration != 0)
	{
		return FWuwaNetworkActionResponse::Rejected(Command.Generation, EWuwaNetworkActionRejectReason::InvalidPayload);
	}

	FWuwaResolvedActionIntent AuthorityIntent;
	if (!RebuildAuthorityIntent(Command, Definition, AuthorityIntent))
	{
		return FWuwaNetworkActionResponse::Rejected(Command.Generation,
		                                            EWuwaNetworkActionRejectReason::ProcessorRejected);
	}

	const FWuwaActionResult ActionResult = Coordinator->StartAuthoritativeIntent(AuthorityIntent);
	if (!ActionResult.HasStarted())
	{
		UE_LOG(LogWuwa,
		       Warning,
		       TEXT("服务端 Legacy Action 裁决失败。Owner=%s, Action=%s, Generation=%d, Reason=%d"),
		       *GetNameSafe(CharacterOwner),
		       *Command.ActionTag.ToString(),
		       Command.Generation.Value,
		       static_cast<int32>(ActionResult.RejectionReason));
		return FWuwaNetworkActionResponse::Rejected(Command.Generation,
		                                            EWuwaNetworkActionRejectReason::ProcessorRejected);
	}

	if (Definition->MovementDriver == EWuwaMovementActionDriver::AirJump)
	{
		LastAuthorityAirJumpGeneration = Command.Generation;
		LastAuthorityAirCycleGeneration = Command.AirCycleGeneration;
	}
	return FWuwaNetworkActionResponse::Accepted(Command.Generation);
}

void UWuwaActionNetworkComponent::HandleMoveResponse(const FWuwaNetworkActionResponse& Response)
{
	if (!Response.bHasResponse || !Response.ProcessedGeneration.IsValid() ||
	    !PendingCommandsByGeneration.Contains(Response.ProcessedGeneration.Value))
	{
		return;
	}

	const FWuwaPendingNetworkMovementCommand PendingCommand =
	    PendingCommandsByGeneration.FindRef(Response.ProcessedGeneration.Value);
	LastResponse = Response;
	if (PendingCommand.IsLegacyActionExit())
	{
		if (Response.bAccepted)
		{
			++AcceptedActionExitResponseCount;
		}
		else
		{
			++RejectedActionExitResponseCount;
			UE_LOG(LogWuwa,
			       Warning,
			       TEXT("Owning Client 的 Legacy Action 退出被服务端拒绝。Owner=%s, ExitGeneration=%d, "
			            "TargetGeneration=%d, ExitKind=%d, Reason=%d"),
			       *GetNameSafe(CharacterOwner),
			       Response.ProcessedGeneration.Value,
			       PendingCommand.TargetActionGeneration.Value,
			       static_cast<int32>(PendingCommand.ExitKind),
			       static_cast<int32>(Response.RejectReason));
		}
		RemovePendingGeneration(Response.ProcessedGeneration);
		return;
	}

	if (Response.bAccepted)
	{
		++AcceptedResponseCount;
		if (PendingCommand.Kind == EWuwaNetworkMovementCommandKind::Grapple &&
		    !ReconcileAcceptedGrapple(PendingCommand, Response))
		{
			CancelPredictedGeneration(Response.ProcessedGeneration, EWuwaGrappleMovementEndReason::NetworkCorrection);
			UE_LOG(LogWuwa,
			       Warning,
			       TEXT("Owning Client 的 Grapple 预测超出权威容差，已交给 CMC 校正。Owner=%s, Generation=%d, "
			            "Distance=%.3f"),
			       *GetNameSafe(CharacterOwner),
			       Response.ProcessedGeneration.Value,
			       GrappleNetworkSnapshot.LastCorrectionDistance);
			return;
		}
		if (PendingCommand.Kind == EWuwaNetworkMovementCommandKind::Grapple &&
		    GrappleNetworkSnapshot.PredictedGeneration == Response.ProcessedGeneration)
		{
			GrappleNetworkSnapshot.PredictedGeneration = FWuwaNetworkActionGeneration();
		}
		RemovePendingGeneration(Response.ProcessedGeneration);
		return;
	}

	++RejectedResponseCount;
	CancelPredictedGeneration(Response.ProcessedGeneration, EWuwaGrappleMovementEndReason::NetworkRejected);
	UE_LOG(LogWuwa,
	       Warning,
	       TEXT("Owning Client 已回滚服务端拒绝的网络 Action。Owner=%s, Generation=%d, Reason=%d"),
	       *GetNameSafe(CharacterOwner),
	       Response.ProcessedGeneration.Value,
	       static_cast<int32>(Response.RejectReason));
}

void UWuwaActionNetworkComponent::HandleAuthorityActionStarted(const FWuwaActionResult& Result)
{
	if (!Result.HasStarted() || !Result.NetworkGeneration.IsValid() ||
	    FindRegisteredDefinition(Result.ActionTag) == nullptr)
	{
		return;
	}

	ActiveLocalGeneration = Result.NetworkGeneration;
	ActiveLocalHandle = Result.ActionHandle;
	++LocalStartCount;

	if (CharacterOwner->HasAuthority())
	{
		if (Cast<UWuwaMovementActionDefinition>(FindRegisteredDefinition(Result.ActionTag)) != nullptr)
		{
			LegacyActionPresentation.NetworkGeneration = Result.NetworkGeneration;
			LegacyActionPresentation.ActionTag = Result.ActionTag;
			LegacyActionPresentation.ServerStartTime = GetSynchronizedServerTime();
			LegacyActionPresentation.bActive = true;
			LegacyActionPresentation.EndReason = EWuwaActionEndReason::None;
			CharacterOwner->ForceNetUpdate();
		}
		return;
	}

	if (CharacterOwner->GetLocalRole() != ROLE_AutonomousProxy)
	{
		return;
	}

	const FWuwaPendingNetworkMovementCommand* Command =
	    PendingCommandsByGeneration.Find(Result.NetworkGeneration.Value);
	if (Command == nullptr || !Movement->QueueNetworkMovementCommand(*Command))
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("预测 Legacy Action 启动后无法写入 Packed Move。Owner=%s, Generation=%d"),
		       *GetNameSafe(CharacterOwner),
		       Result.NetworkGeneration.Value);
		CancelPredictedGeneration(Result.NetworkGeneration, EWuwaGrappleMovementEndReason::NetworkRejected);
	}
}

void UWuwaActionNetworkComponent::HandleAuthorityActionFinalized(const FWuwaActionFinalizedMessage& Message)
{
	if (!Message.NetworkGeneration.IsValid())
	{
		return;
	}

	const bool bMatchesActiveLocalGeneration = Message.NetworkGeneration == ActiveLocalGeneration;
	EWuwaLegacyActionExitKind ExitKind = EWuwaLegacyActionExitKind::None;
	if (bMatchesActiveLocalGeneration && CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy &&
	    !bSuppressPredictedCancelEmission && ResolveLegacyActionExitKind(Message, ExitKind))
	{
		if (!QueuePredictedLegacyActionExit(Message))
		{
			UE_LOG(LogWuwa,
			       Error,
			       TEXT("预测 Legacy Action 退出无法写入 Packed Move。Owner=%s, TargetGeneration=%d, ExitKind=%d"),
			       *GetNameSafe(CharacterOwner),
			       Message.NetworkGeneration.Value,
			       static_cast<int32>(ExitKind));
		}
	}

	if (CharacterOwner->HasAuthority())
	{
		LastFinalizedAuthorityActionGeneration = Message.NetworkGeneration;
	}

	if (bMatchesActiveLocalGeneration)
	{
		ActiveLocalGeneration = FWuwaNetworkActionGeneration();
		ActiveLocalHandle = FWuwaActionHandle();
		if (GrappleNetworkSnapshot.PredictedGeneration == Message.NetworkGeneration &&
		    !PendingCommandsByGeneration.Contains(Message.NetworkGeneration.Value))
		{
			GrappleNetworkSnapshot.PredictedGeneration = FWuwaNetworkActionGeneration();
		}
	}

	if (!CharacterOwner->HasAuthority() ||
	    Cast<UWuwaMovementActionDefinition>(FindRegisteredDefinition(Message.ActionTag)) == nullptr ||
	    Message.NetworkGeneration != LegacyActionPresentation.NetworkGeneration)
	{
		return;
	}

	LegacyActionPresentation.bActive = false;
	LegacyActionPresentation.EndReason = Message.EndReason;
	CharacterOwner->ForceNetUpdate();
}

void UWuwaActionNetworkComponent::HandleRep_LegacyActionPresentation()
{
	if (!IsInitialized() || CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		return;
	}

	if (LegacyActionPresentation.bActive)
	{
		if (!LegacyActionPresentation.IsActivePayloadValid())
		{
			UE_LOG(LogWuwa,
			       Error,
			       TEXT("Simulated Proxy 拒绝无效 Legacy Action 表现。Owner=%s, Action=%s, Generation=%d"),
			       *GetNameSafe(CharacterOwner),
			       *LegacyActionPresentation.ActionTag.ToString(),
			       LegacyActionPresentation.NetworkGeneration.Value);
			return;
		}

		PlayReplicatedActionPresentation(LegacyActionPresentation.NetworkGeneration,
		                                 LegacyActionPresentation.ActionTag,
		                                 LegacyActionPresentation.ServerStartTime);
		return;
	}

	StopReplicatedActionPresentation(LegacyActionPresentation.NetworkGeneration);
}

bool UWuwaActionNetworkComponent::PlayReplicatedActionPresentation(const FWuwaNetworkActionGeneration& Generation,
                                                                   const FGameplayTag& ActionTag,
                                                                   const float ServerStartTime)
{
	if (!IsInitialized() || CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy || !Generation.IsValid() ||
	    !ActionTag.IsValid() || !FMath::IsFinite(ServerStartTime) || ServerStartTime < 0.f)
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("远端 Action Montage 前置条件失败。Owner=%s, Generation=%d, Action=%s, ServerStartTime=%.3f"),
		       *GetNameSafe(CharacterOwner),
		       Generation.Value,
		       *ActionTag.ToString(),
		       ServerStartTime);
		return false;
	}

	UWuwaActionDefinition* Definition = DefinitionRegistry.FindRef(ActionTag);
	if (!IsValid(Definition) ||
	    !AnimationCapability->PlayReplicatedPresentation(Generation, Definition, ServerStartTime))
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("远端 Action Montage 播放失败。Owner=%s, Generation=%d, Action=%s, ServerStartTime=%.3f"),
		       *GetNameSafe(CharacterOwner),
		       Generation.Value,
		       *ActionTag.ToString(),
		       ServerStartTime);
		return false;
	}

	return true;
}

void UWuwaActionNetworkComponent::StopReplicatedActionPresentation(const FWuwaNetworkActionGeneration& Generation)
{
	if (Generation.IsValid() && IsValid(AnimationCapability))
	{
		AnimationCapability->StopReplicatedPresentation(Generation);
	}
}

bool UWuwaActionNetworkComponent::CancelPredictedGeneration(const FWuwaNetworkActionGeneration& Generation,
                                                            const EWuwaGrappleMovementEndReason GrappleEndReason)
{
	if (!Generation.IsValid() || (GrappleEndReason != EWuwaGrappleMovementEndReason::NetworkRejected &&
	                              GrappleEndReason != EWuwaGrappleMovementEndReason::NetworkCorrection))
	{
		return false;
	}

	RemovePendingGeneration(Generation);
	if (GrappleNetworkSnapshot.PredictedGeneration == Generation)
	{
		GrappleNetworkSnapshot.PredictedGeneration = FWuwaNetworkActionGeneration();
	}
	if (Generation != ActiveLocalGeneration || !IsValid(Coordinator))
	{
		return false;
	}

	if (IsValid(GrappleCapability))
	{
		GrappleCapability->StopActiveGrapple(Generation, GrappleEndReason);
	}

	TGuardValue<bool> SuppressCancelEmission(bSuppressPredictedCancelEmission, true);
	const bool bCancelled = Coordinator->FinishCurrent(EWuwaActionEndReason::Failed);
	if (!bCancelled)
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("预测网络 Action 回滚失败。Owner=%s, Generation=%d"),
		       *GetNameSafe(CharacterOwner),
		       Generation.Value);
	}
	return bCancelled;
}

void UWuwaActionNetworkComponent::InvalidateAvatarGenerations(const EWuwaGrappleMovementEndReason GrappleEndReason,
                                                              const EWuwaActionEndReason ActionEndReason)
{
	++AvatarInvalidationCount;
	LastAvatarInvalidationGrappleEndReason = GrappleEndReason;
	LastAvatarInvalidationActionEndReason = ActionEndReason;

	if (ActiveLocalGeneration.IsValid() && IsValid(GrappleCapability))
	{
		GrappleCapability->StopActiveGrapple(ActiveLocalGeneration, GrappleEndReason);
	}
	if (IsValid(Coordinator) && ActionEndReason != EWuwaActionEndReason::None)
	{
		TGuardValue<bool> SuppressCancelEmission(bSuppressPredictedCancelEmission, true);
		Coordinator->AbortAllActions(ActionEndReason);
	}

	if (IsValid(AnimationCapability) && LegacyActionPresentation.NetworkGeneration.IsValid())
	{
		StopReplicatedActionPresentation(LegacyActionPresentation.NetworkGeneration);
	}

	PendingCommandsByGeneration.Reset();
	ActiveLocalGeneration = FWuwaNetworkActionGeneration();
	ActiveLocalHandle = FWuwaActionHandle();
	LastResponse = FWuwaNetworkActionResponse();
	GrappleNetworkSnapshot = FWuwaGrappleNetworkRuntimeSnapshot();
	LegacyActionPresentation.Reset();
	if (IsValid(CharacterOwner) && CharacterOwner->HasAuthority())
	{
		CharacterOwner->ForceNetUpdate();
	}
}

FWuwaActionNetworkRuntimeSnapshot UWuwaActionNetworkComponent::GetRuntimeSnapshot() const
{
	FWuwaActionNetworkRuntimeSnapshot Snapshot;
	Snapshot.bInitialized = IsInitialized();
	Snapshot.RegistryCount = DefinitionRegistry.Num();
	Snapshot.ActiveLocalGeneration = ActiveLocalGeneration;
	Snapshot.ActiveLocalHandle = ActiveLocalHandle;
	Snapshot.LastAuthorityGeneration = LastAuthorityGeneration;
	Snapshot.PresentationGeneration = LegacyActionPresentation.NetworkGeneration;
	Snapshot.LastResponse = LastResponse;
	Snapshot.LocalStartCount = LocalStartCount;
	Snapshot.AuthorityProcessorCount = AuthorityProcessorCount;
	Snapshot.AcceptedResponseCount = AcceptedResponseCount;
	Snapshot.RejectedResponseCount = RejectedResponseCount;
	for (const TPair<int32, FWuwaPendingNetworkMovementCommand>& Entry : PendingCommandsByGeneration)
	{
		Snapshot.PendingActionExitCommandCount += Entry.Value.IsLegacyActionExit() ? 1 : 0;
	}
	Snapshot.AcceptedActionExitResponseCount = AcceptedActionExitResponseCount;
	Snapshot.RejectedActionExitResponseCount = RejectedActionExitResponseCount;
	Snapshot.LastFinalizedAuthorityActionGeneration = LastFinalizedAuthorityActionGeneration;
	Snapshot.AvatarInvalidationCount = AvatarInvalidationCount;
	Snapshot.LastAvatarInvalidationActionEndReason = LastAvatarInvalidationActionEndReason;
	Snapshot.LastAvatarInvalidationGrappleEndReason = LastAvatarInvalidationGrappleEndReason;
	Snapshot.PresentationStartCount =
	    IsValid(AnimationCapability) ? AnimationCapability->GetReplicatedPresentationStartCount() : 0;
	Snapshot.bPresentationActive = IsValid(AnimationCapability) && AnimationCapability->HasReplicatedPresentation();
	Snapshot.Grapple = GrappleNetworkSnapshot;
	return Snapshot;
}

const UWuwaActionDefinition* UWuwaActionNetworkComponent::FindRegisteredDefinition(const FGameplayTag& ActionTag) const
{
	return ActionTag.IsValid() ? DefinitionRegistry.FindRef(ActionTag) : nullptr;
}

void UWuwaActionNetworkComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UWuwaActionNetworkComponent, LegacyActionPresentation, COND_SkipOwner);
}

void UWuwaActionNetworkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	InvalidateAvatarGenerations(EWuwaGrappleMovementEndReason::EndPlay, EWuwaActionEndReason::OwnerDestroyed);
	ShutdownBindings();
	DefinitionRegistry.Reset();
	CharacterOwner = nullptr;
	Coordinator = nullptr;
	Movement = nullptr;
	MovementCapability = nullptr;
	AnimationCapability = nullptr;
	GrappleCapability = nullptr;
	Super::EndPlay(EndPlayReason);
}

bool UWuwaActionNetworkComponent::IsInitialized() const
{
	return IsValid(CharacterOwner) && IsValid(Coordinator) && IsValid(Movement) && IsValid(MovementCapability) &&
	       IsValid(AnimationCapability) && !DefinitionRegistry.IsEmpty();
}

void UWuwaActionNetworkComponent::ShutdownBindings()
{
	if (IsValid(Movement))
	{
		Movement->UnbindNetworkCommandProcessor(this);
		Movement->UnbindNetworkMoveResponseConsumer(this);
		Movement->UnbindNetworkMoveReplayConsumer(this);
	}

	if (IsValid(Coordinator))
	{
		if (ActionStartedDelegateHandle.IsValid())
		{
			Coordinator->OnActionStarted.Remove(ActionStartedDelegateHandle);
		}
		if (ActionFinalizedDelegateHandle.IsValid())
		{
			Coordinator->OnActionFinalized.Remove(ActionFinalizedDelegateHandle);
		}
	}

	ActionStartedDelegateHandle.Reset();
	ActionFinalizedDelegateHandle.Reset();
}

bool UWuwaActionNetworkComponent::AddRuleSetToRegistry(const UWuwaActionRuleSet* RuleSet)
{
	if (!IsValid(RuleSet) || !RuleSet->IsRuntimeValid())
	{
		return false;
	}

	for (const FWuwaActionResolutionRule& Rule : RuleSet->Rules)
	{
		UWuwaActionDefinition* Definition = Rule.Definition;
		if (!IsValid(Definition) || !Definition->IsRuntimeValid() || !Definition->ActionTag.IsValid())
		{
			return false;
		}

		if (const TObjectPtr<UWuwaActionDefinition>* Existing = DefinitionRegistry.Find(Definition->ActionTag))
		{
			if (*Existing != Definition)
			{
				UE_LOG(LogWuwa,
				       Error,
				       TEXT("Action Registry 同标签映射到多个 Definition。Action=%s, Left=%s, Right=%s"),
				       *Definition->ActionTag.ToString(),
				       *GetNameSafe(*Existing),
				       *GetNameSafe(Definition));
				return false;
			}
			continue;
		}

		DefinitionRegistry.Add(Definition->ActionTag, Definition);
	}
	return !DefinitionRegistry.IsEmpty();
}

bool UWuwaActionNetworkComponent::BuildLegacyMovementCommand(const FWuwaResolvedActionIntent& Intent,
                                                             FWuwaPendingNetworkMovementCommand& OutCommand) const
{
	const UWuwaMovementActionDefinition* Definition = Cast<UWuwaMovementActionDefinition>(Intent.Request.Definition);
	const FVector WorldDirection = Intent.Request.Context.WorldDirection.GetSafeNormal2D();
	if (!IsValid(Definition) || !Intent.Request.NetworkGeneration.IsValid() || WorldDirection.IsNearlyZero())
	{
		return false;
	}

	OutCommand.Reset();
	OutCommand.Generation = Intent.Request.NetworkGeneration;
	OutCommand.Kind = EWuwaNetworkMovementCommandKind::LegacyAction;
	OutCommand.ActionTag = Definition->ActionTag;
	OutCommand.WorldDirection = WorldDirection;
	if (Definition->MovementDriver == EWuwaMovementActionDriver::AirJump)
	{
		OutCommand.AirCycleGeneration = Movement->GetAirCycleGeneration();
	}

	if (Definition->FacingPolicy == EWuwaActionFacingPolicy::FaceContextDirection)
	{
		OutCommand.DesiredFacingYaw = WorldDirection.Rotation().Yaw;
		OutCommand.Flags |= static_cast<uint8>(EWuwaNetworkMovementCommandFlags::HasDesiredFacing) |
		                    static_cast<uint8>(EWuwaNetworkMovementCommandFlags::AllowFacingSnap);
	}
	else if (Definition->FacingPolicy == EWuwaActionFacingPolicy::PreserveStartingFacing)
	{
		const FVector FacingDirection = Intent.Request.Context.FacingDirection.GetSafeNormal2D();
		if (FacingDirection.IsNearlyZero())
		{
			return false;
		}
		OutCommand.DesiredFacingYaw = FacingDirection.Rotation().Yaw;
		OutCommand.Flags |= static_cast<uint8>(EWuwaNetworkMovementCommandFlags::HasDesiredFacing);
	}

	return OutCommand.IsPayloadValid();
}

bool UWuwaActionNetworkComponent::BuildGrappleMovementCommand(const FWuwaResolvedActionIntent& Intent,
                                                              FWuwaPendingNetworkMovementCommand& OutCommand)
{
	const UWuwaGrappleActionDefinition* Definition = Cast<UWuwaGrappleActionDefinition>(Intent.Request.Definition);
	const FWuwaGrappleActionContext* Context = Intent.Request.Context.DomainPayload.GetPtr<FWuwaGrappleActionContext>();
	if (!IsValid(Definition) || !Intent.Request.NetworkGeneration.IsValid() || Context == nullptr ||
	    !Context->IsRuntimeValid() || Context->QueryFrameNumber <= 0 || Context->QueryFrameNumber > MAX_int32)
	{
		return false;
	}

	const FRotator NormalizedView = Context->ViewRotation.GetNormalized();
	OutCommand.Reset();
	OutCommand.Generation = Intent.Request.NetworkGeneration;
	OutCommand.Kind = EWuwaNetworkMovementCommandKind::Grapple;
	OutCommand.ActionTag = Definition->ActionTag;
	OutCommand.GrappleInputDirection = Intent.Request.Context.InputDirection.GetClampedToMaxSize(1.f);
	OutCommand.GrappleViewYaw = NormalizedView.Yaw;
	OutCommand.GrappleViewPitch = FMath::Clamp(NormalizedView.Pitch, -89.9f, 89.9f);
	OutCommand.ClientPredictedGrappleScale = Context->TrajectoryScale;
	OutCommand.ClientGrappleQueryFrame = static_cast<int32>(Context->QueryFrameNumber);
	OutCommand.DesiredFacingYaw = Context->TravelDirection.Rotation().Yaw;
	OutCommand.Flags = static_cast<uint8>(EWuwaNetworkMovementCommandFlags::HasDesiredFacing) |
	                   static_cast<uint8>(EWuwaNetworkMovementCommandFlags::AllowFacingSnap);
	if (!OutCommand.IsPayloadValid())
	{
		return false;
	}

	GrappleNetworkSnapshot.PredictedGeneration = OutCommand.Generation;
	GrappleNetworkSnapshot.PredictedTrajectoryScale = Context->TrajectoryScale;
	GrappleNetworkSnapshot.PredictedTravelYaw = Context->TravelDirection.Rotation().Yaw;
	GrappleNetworkSnapshot.PredictedStartLocation = Context->QueryStartLocation;
	GrappleNetworkSnapshot.LastCorrectionDistance = 0.f;
	GrappleNetworkSnapshot.LastQueryRejectReason = EWuwaGrappleQueryFailureReason::None;
	GrappleNetworkSnapshot.LateralInputSource = EWuwaGrappleLateralInputSource::AccelerationProjection;
	GrappleNetworkSnapshot.bCorrectionRequired = false;
	return true;
}

bool UWuwaActionNetworkComponent::ResolveLegacyActionExitKind(const FWuwaActionFinalizedMessage& Message,
                                                              EWuwaLegacyActionExitKind& OutExitKind) const
{
	OutExitKind = EWuwaLegacyActionExitKind::None;
	const UWuwaMovementActionDefinition* Definition =
	    Cast<UWuwaMovementActionDefinition>(FindRegisteredDefinition(Message.ActionTag));
	const bool bFiniteIntent =
	    FMath::IsFinite(Message.TriggerMoveIntent.X) && FMath::IsFinite(Message.TriggerMoveIntent.Y);
	if (!IsInitialized() || !Message.NetworkGeneration.IsValid() || !Message.ActionTag.IsValid() ||
	    !Message.TriggerEventTag.IsValid() || !bFiniteIntent || Message.TriggerMoveIntent.IsNearlyZero(0.1f) ||
	    !IsValid(Definition) || !Definition->IsRuntimeValid())
	{
		return false;
	}

	EWuwaMovementActionEventResponse RequiredResponse;
	if (Message.EndReason == EWuwaActionEndReason::Cancelled)
	{
		OutExitKind = EWuwaLegacyActionExitKind::MoveCancel;
		RequiredResponse = EWuwaMovementActionEventResponse::OpenMoveCancelWindow;
	}
	else if (Message.EndReason == EWuwaActionEndReason::Completed)
	{
		OutExitKind = EWuwaLegacyActionExitKind::CompletedExit;
		RequiredResponse = EWuwaMovementActionEventResponse::RequestCompletedExit;
	}
	else
	{
		OutExitKind = EWuwaLegacyActionExitKind::None;
		return false;
	}

	bool bHasRequiredResponse = false;
	bool bTriggerMatchesRequiredResponse = false;
	for (const FWuwaMovementActionEventBinding& Binding : Definition->EventBindings)
	{
		if (!Binding.EventTag.IsValid() || Binding.Response != RequiredResponse)
		{
			continue;
		}
		bHasRequiredResponse = true;
		bTriggerMatchesRequiredResponse |= Binding.EventTag == Message.TriggerEventTag;
	}

	const bool bInputFollowup = Message.TriggerEventTag == WuwaGameplayTags::Action_Event_Input_MoveChanged;
	if (!bHasRequiredResponse || (!bTriggerMatchesRequiredResponse && !bInputFollowup))
	{
		OutExitKind = EWuwaLegacyActionExitKind::None;
		return false;
	}
	return true;
}

bool UWuwaActionNetworkComponent::BuildLegacyActionExitCommand(const FWuwaActionFinalizedMessage& Message,
                                                               FWuwaPendingNetworkMovementCommand& OutCommand)
{
	EWuwaLegacyActionExitKind ExitKind = EWuwaLegacyActionExitKind::None;
	if (!ResolveLegacyActionExitKind(Message, ExitKind))
	{
		return false;
	}

	OutCommand.Reset();
	OutCommand.Generation = Movement->AllocateNetworkActionGeneration();
	OutCommand.TargetActionGeneration = Message.NetworkGeneration;
	OutCommand.Kind = EWuwaNetworkMovementCommandKind::LegacyActionExit;
	OutCommand.ActionTag = Message.ActionTag;
	OutCommand.ExitKind = ExitKind;
	OutCommand.ExitMoveIntent = Message.TriggerMoveIntent.GetClampedToMaxSize(1.f);
	return OutCommand.IsPayloadValid();
}

bool UWuwaActionNetworkComponent::QueuePredictedLegacyActionExit(const FWuwaActionFinalizedMessage& Message)
{
	if (!IsInitialized() || CharacterOwner->GetLocalRole() != ROLE_AutonomousProxy ||
	    !Movement->CanQueueNetworkMovementCommand())
	{
		return false;
	}

	FWuwaPendingNetworkMovementCommand Command;
	if (!BuildLegacyActionExitCommand(Message, Command))
	{
		return false;
	}

	PendingCommandsByGeneration.Add(Command.Generation.Value, Command);
	if (!Movement->QueueNetworkMovementCommand(Command))
	{
		RemovePendingGeneration(Command.Generation);
		return false;
	}
	return true;
}

FWuwaNetworkActionResponse
UWuwaActionNetworkComponent::ProcessServerLegacyActionExit(const FWuwaPendingNetworkMovementCommand& Command)
{
	const FWuwaActionRuntimeSnapshot CoordinatorSnapshot = Coordinator->GetRuntimeSnapshot();
	FWuwaMovementActionNetworkReplayState PhysicalState;
	MovementCapability->CaptureNetworkReplayState(PhysicalState);
	const bool bGameplayActiveMatches = CoordinatorSnapshot.ActiveNetworkGeneration == Command.TargetActionGeneration &&
	                                    CoordinatorSnapshot.ActiveActionTag == Command.ActionTag &&
	                                    CoordinatorSnapshot.ActiveHandle == ActiveLocalHandle &&
	                                    Command.TargetActionGeneration == ActiveLocalGeneration;
	const bool bGameplayAlreadyFinalizedWithPhysicalRuntime =
	    Command.TargetActionGeneration == LastFinalizedAuthorityActionGeneration &&
	    LegacyActionPresentation.NetworkGeneration == Command.TargetActionGeneration &&
	    LegacyActionPresentation.ActionTag == Command.ActionTag &&
	    !CoordinatorSnapshot.ActiveNetworkGeneration.IsValid() && !ActiveLocalGeneration.IsValid() &&
	    PhysicalState.bMovementActionActive && PhysicalState.Generation == Command.TargetActionGeneration &&
	    PhysicalState.ActionTag == Command.ActionTag;
	if ((!bGameplayActiveMatches && !bGameplayAlreadyFinalizedWithPhysicalRuntime) ||
	    Cast<UWuwaMovementActionDefinition>(FindRegisteredDefinition(Command.ActionTag)) == nullptr ||
	    !MovementCapability->HasConfiguredActionExit(Command.ExitKind))
	{
		return FWuwaNetworkActionResponse::Rejected(Command.Generation,
		                                            EWuwaNetworkActionRejectReason::ProcessorRejected);
	}

	EWuwaActionEndReason EndReason;
	const bool bPrepared = MovementCapability->PrepareNetworkActionExitTransition(Command, EndReason);
	const bool bGameplayFinished =
	    bGameplayAlreadyFinalizedWithPhysicalRuntime ||
	    (bGameplayActiveMatches && EndReason != EWuwaActionEndReason::None && Coordinator->FinishCurrent(EndReason));
	if (!bPrepared || EndReason == EWuwaActionEndReason::None || !bGameplayFinished)
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("服务端无法在当前 Move 内结束 Legacy Action。Owner=%s, ExitGeneration=%d, TargetGeneration=%d, "
		            "ExitKind=%d"),
		       *GetNameSafe(CharacterOwner),
		       Command.Generation.Value,
		       Command.TargetActionGeneration.Value,
		       static_cast<int32>(Command.ExitKind));
		return FWuwaNetworkActionResponse::Rejected(Command.Generation,
		                                            EWuwaNetworkActionRejectReason::ProcessorRejected);
	}

	return FWuwaNetworkActionResponse::Accepted(Command.Generation);
}

FWuwaNetworkActionResponse
UWuwaActionNetworkComponent::ProcessServerGrappleCommand(const FWuwaPendingNetworkMovementCommand& Command)
{
	UWuwaGrappleActionDefinition* Definition =
	    Cast<UWuwaGrappleActionDefinition>(DefinitionRegistry.FindRef(Command.ActionTag));
	if (!IsValid(GrappleCapability) || !IsValid(Definition) || !Definition->IsRuntimeValid())
	{
		UE_LOG(LogWuwa,
		       Warning,
		       TEXT("服务端拒绝未知或未装配的 Grapple。Owner=%s, Action=%s, Generation=%d"),
		       *GetNameSafe(CharacterOwner),
		       *Command.ActionTag.ToString(),
		       Command.Generation.Value);
		return FWuwaNetworkActionResponse::Rejected(Command.Generation,
		                                            EWuwaNetworkActionRejectReason::ProcessorRejected);
	}

	FWuwaResolvedActionIntent AuthorityIntent;
	FWuwaGrappleActionContext AuthorityContext;
	if (!RebuildAuthorityGrappleIntent(Command, Definition, AuthorityIntent, AuthorityContext) ||
	    !Movement->QueueActionFacing(AuthorityContext.TravelDirection.Rotation().Yaw, true))
	{
		return FWuwaNetworkActionResponse::Rejected(Command.Generation,
		                                            EWuwaNetworkActionRejectReason::ProcessorRejected);
	}

	const FWuwaActionResult ActionResult = Coordinator->StartAuthoritativeIntent(AuthorityIntent);
	if (!ActionResult.HasStarted())
	{
		UE_LOG(LogWuwa,
		       Warning,
		       TEXT("服务端 Grapple 裁决失败。Owner=%s, Action=%s, Generation=%d, Reason=%d"),
		       *GetNameSafe(CharacterOwner),
		       *Command.ActionTag.ToString(),
		       Command.Generation.Value,
		       static_cast<int32>(ActionResult.RejectionReason));
		return FWuwaNetworkActionResponse::Rejected(Command.Generation,
		                                            EWuwaNetworkActionRejectReason::ProcessorRejected);
	}

	FWuwaGrappleActionContext CommittedContext;
	if (!GrappleCapability->GetActiveNetworkContext(Command.Generation, CommittedContext))
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("服务端 Grapple 启动后无法读取 Commit Context。Owner=%s, Generation=%d"),
		       *GetNameSafe(CharacterOwner),
		       Command.Generation.Value);
		Coordinator->FinishCurrent(EWuwaActionEndReason::Failed);
		return FWuwaNetworkActionResponse::Rejected(Command.Generation,
		                                            EWuwaNetworkActionRejectReason::ProcessorRejected);
	}
	AuthorityContext = CommittedContext;

	GrappleNetworkSnapshot.AuthorityGeneration = Command.Generation;
	GrappleNetworkSnapshot.AuthorityTrajectoryScale = AuthorityContext.TrajectoryScale;
	GrappleNetworkSnapshot.AuthorityTravelYaw = AuthorityContext.TravelDirection.Rotation().Yaw;
	GrappleNetworkSnapshot.AuthorityStartLocation = AuthorityContext.QueryStartLocation;
	GrappleNetworkSnapshot.LastQueryRejectReason = EWuwaGrappleQueryFailureReason::None;
	GrappleNetworkSnapshot.LateralInputSource = EWuwaGrappleLateralInputSource::AccelerationProjection;
	return FWuwaNetworkActionResponse::AcceptedGrapple(Command.Generation,
	                                                   AuthorityContext.TrajectoryScale,
	                                                   AuthorityContext.TravelDirection.Rotation().Yaw,
	                                                   AuthorityContext.QueryStartLocation);
}

bool UWuwaActionNetworkComponent::ApplyLocalActionFacing(const FWuwaPendingNetworkMovementCommand& Command,
                                                         const UWuwaMovementActionDefinition* Definition) const
{
	if (!IsValid(Definition) || !IsValid(Movement))
	{
		return false;
	}

	if (Definition->FacingPolicy == EWuwaActionFacingPolicy::UseLocomotion)
	{
		return !Command.HasFlag(EWuwaNetworkMovementCommandFlags::HasDesiredFacing);
	}

	if (!Command.HasFlag(EWuwaNetworkMovementCommandFlags::HasDesiredFacing))
	{
		return false;
	}

	const bool bAllowSnap = Definition->FacingPolicy == EWuwaActionFacingPolicy::FaceContextDirection;
	if (bAllowSnap)
	{
		const float DirectionYaw = Command.WorldDirection.Rotation().Yaw;
		if (!Command.HasFlag(EWuwaNetworkMovementCommandFlags::AllowFacingSnap) ||
		    FMath::Abs(FMath::FindDeltaAngleDegrees(DirectionYaw, Command.DesiredFacingYaw)) > 2.f)
		{
			return false;
		}
	}

	return Movement->QueueActionFacing(Command.DesiredFacingYaw, bAllowSnap);
}

bool UWuwaActionNetworkComponent::ApplyLocalGrappleFacing(const FWuwaPendingNetworkMovementCommand& Command) const
{
	if (!IsValid(Movement) || Command.Kind != EWuwaNetworkMovementCommandKind::Grapple ||
	    !Command.HasFlag(EWuwaNetworkMovementCommandFlags::HasDesiredFacing) ||
	    !Command.HasFlag(EWuwaNetworkMovementCommandFlags::AllowFacingSnap) ||
	    FMath::Abs(FMath::FindDeltaAngleDegrees(Command.DesiredFacingYaw, Command.GrappleViewYaw)) > 2.f)
	{
		return false;
	}

	return Movement->QueueActionFacing(Command.DesiredFacingYaw, true);
}

bool UWuwaActionNetworkComponent::RebuildAuthorityIntent(const FWuwaPendingNetworkMovementCommand& Command,
                                                         UWuwaMovementActionDefinition* Definition,
                                                         FWuwaResolvedActionIntent& OutIntent) const
{
	const FVector WorldDirection = Command.WorldDirection.GetSafeNormal2D();
	const FVector FacingDirection = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
	if (!IsValid(Definition) || WorldDirection.IsNearlyZero() || FacingDirection.IsNearlyZero())
	{
		return false;
	}

	const bool bRequiresDesiredFacing = Definition->FacingPolicy != EWuwaActionFacingPolicy::UseLocomotion;
	if (bRequiresDesiredFacing && !Command.HasFlag(EWuwaNetworkMovementCommandFlags::HasDesiredFacing))
	{
		return false;
	}

	if (Definition->FacingPolicy == EWuwaActionFacingPolicy::FaceContextDirection)
	{
		const float DirectionYaw = WorldDirection.Rotation().Yaw;
		if (!Command.HasFlag(EWuwaNetworkMovementCommandFlags::AllowFacingSnap) ||
		    FMath::Abs(FMath::FindDeltaAngleDegrees(DirectionYaw, Command.DesiredFacingYaw)) > 2.f)
		{
			return false;
		}
	}

	FWuwaMessageHeader Header;
	Header.FrameNumber = static_cast<int64>(GFrameCounter);
	Header.Sequence = Command.Generation.Value;
	Header.CreatedAt = GetWorld() != nullptr ? static_cast<double>(GetWorld()->GetTimeSeconds()) : 0.0;
	Header.SourceObject = CharacterOwner;

	OutIntent = FWuwaResolvedActionIntent();
	OutIntent.Request.Header = Header;
	OutIntent.Request.Definition = Definition;
	OutIntent.Request.NetworkGeneration = Command.Generation;
	OutIntent.Request.Context.WorldDirection = WorldDirection;
	OutIntent.Request.Context.FacingDirection = FacingDirection;
	OutIntent.Request.Context.MovementMode = Movement->MovementMode;
	OutIntent.Request.Context.CustomMovementMode = Movement->CustomMovementMode;
	OutIntent.Request.Context.SourceObject = CharacterOwner;
	OutIntent.ExpireAt = Header.CreatedAt;
	return OutIntent.IsValid() && Definition->IsContextValid(OutIntent.Request.Context);
}

bool UWuwaActionNetworkComponent::RebuildAuthorityGrappleIntent(const FWuwaPendingNetworkMovementCommand& Command,
                                                                UWuwaGrappleActionDefinition* Definition,
                                                                FWuwaResolvedActionIntent& OutIntent,
                                                                FWuwaGrappleActionContext& OutContext)
{
	OutIntent = FWuwaResolvedActionIntent();
	OutContext = FWuwaGrappleActionContext();
	if (!IsValid(CharacterOwner) || !IsValid(Movement) || !IsValid(Definition))
	{
		return false;
	}

	FWuwaActionResolutionSnapshot Snapshot;
	Snapshot.FacingDirection = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
	Snapshot.ViewRotation = FRotator(Command.GrappleViewPitch, Command.GrappleViewYaw, 0.f);
	Snapshot.MovementMode = Movement->MovementMode;
	Snapshot.CustomMovementMode = Movement->CustomMovementMode;
	Snapshot.bIsGrounded = Movement->IsMovingOnGround();
	Snapshot.bIsAirborne = Movement->IsFalling();
	Snapshot.SourceObject = CharacterOwner;

	FWuwaGrappleQueryResult QueryResult;
	TArray<FVector> PredictedPoints;
	const int64 AuthorityQueryFrame = FMath::Max<int64>(1, static_cast<int64>(GFrameCounter));
	if (!Snapshot.IsValid() ||
	    !FWuwaGrappleQuery::Query(Snapshot, Definition, AuthorityQueryFrame, OutContext, QueryResult, PredictedPoints))
	{
		GrappleNetworkSnapshot.LastQueryRejectReason = QueryResult.FailureReason;
		UE_LOG(LogWuwa,
		       Warning,
		       TEXT("服务端 Grapple Query 拒绝。Owner=%s, Generation=%d, ClientFrame=%d, Failure=%s"),
		       *GetNameSafe(CharacterOwner),
		       Command.Generation.Value,
		       Command.ClientGrappleQueryFrame,
		       *StaticEnum<EWuwaGrappleQueryFailureReason>()->GetNameStringByValue(
		           static_cast<int64>(QueryResult.FailureReason)));
		return false;
	}

	FWuwaMessageHeader Header;
	Header.FrameNumber = AuthorityQueryFrame;
	Header.Sequence = Command.Generation.Value;
	Header.CreatedAt = GetWorld() != nullptr ? static_cast<double>(GetWorld()->GetTimeSeconds()) : 0.0;
	Header.SourceObject = CharacterOwner;

	OutIntent.Request.Header = Header;
	OutIntent.Request.Definition = Definition;
	OutIntent.Request.NetworkGeneration = Command.Generation;
	OutIntent.Request.Context.InputDirection = Command.GrappleInputDirection;
	OutIntent.Request.Context.WorldDirection = OutContext.TravelDirection;
	OutIntent.Request.Context.FacingDirection = Snapshot.FacingDirection;
	OutIntent.Request.Context.MovementMode = Snapshot.MovementMode;
	OutIntent.Request.Context.CustomMovementMode = Snapshot.CustomMovementMode;
	OutIntent.Request.Context.SourceObject = CharacterOwner;
	OutIntent.Request.Context.DomainPayload.InitializeAs<FWuwaGrappleActionContext>(OutContext);
	OutIntent.ExpireAt = Header.CreatedAt;
	return OutIntent.IsValid() && Definition->IsContextValid(OutIntent.Request.Context);
}

bool UWuwaActionNetworkComponent::ReconcileAcceptedGrapple(const FWuwaPendingNetworkMovementCommand& Command,
                                                           const FWuwaNetworkActionResponse& Response)
{
	FWuwaGrappleActionContext PredictedContext;
	const UWuwaGrappleActionDefinition* Definition =
	    Cast<UWuwaGrappleActionDefinition>(DefinitionRegistry.FindRef(Command.ActionTag));
	if (Command.Kind != EWuwaNetworkMovementCommandKind::Grapple || !Response.IsGrappleAuthoritySummaryValid() ||
	    !IsValid(GrappleCapability) || !IsValid(Definition))
	{
		GrappleNetworkSnapshot.bCorrectionRequired = true;
		return false;
	}

	const bool bHasActiveContext = GrappleCapability->GetActiveNetworkContext(Command.Generation, PredictedContext);
	const bool bHasFinalizedPredictionSummary = GrappleNetworkSnapshot.PredictedGeneration == Command.Generation &&
	                                            FMath::IsFinite(GrappleNetworkSnapshot.PredictedTrajectoryScale) &&
	                                            GrappleNetworkSnapshot.PredictedTrajectoryScale > 0.f &&
	                                            FMath::IsFinite(GrappleNetworkSnapshot.PredictedTravelYaw) &&
	                                            FMath::IsFinite(GrappleNetworkSnapshot.PredictedStartLocation.X) &&
	                                            FMath::IsFinite(GrappleNetworkSnapshot.PredictedStartLocation.Y) &&
	                                            FMath::IsFinite(GrappleNetworkSnapshot.PredictedStartLocation.Z);
	if (!bHasActiveContext && !bHasFinalizedPredictionSummary)
	{
		GrappleNetworkSnapshot.bCorrectionRequired = true;
		return false;
	}

	constexpr float TrajectoryScaleTolerance = 0.02f;
	constexpr float TravelYawTolerance = 2.f;
	const float PredictedScale =
	    bHasActiveContext ? PredictedContext.TrajectoryScale : GrappleNetworkSnapshot.PredictedTrajectoryScale;
	const float PredictedYaw =
	    bHasActiveContext ? PredictedContext.TravelDirection.Rotation().Yaw : GrappleNetworkSnapshot.PredictedTravelYaw;
	const FVector PredictedStart =
	    bHasActiveContext ? PredictedContext.QueryStartLocation : GrappleNetworkSnapshot.PredictedStartLocation;
	const float StartDistance =
	    FVector::Distance(PredictedStart, static_cast<FVector>(Response.AuthorityGrappleStartLocation));
	const float ScaleDifference = FMath::Abs(PredictedScale - Response.AuthorityGrappleTrajectoryScale);
	const float YawDifference =
	    FMath::Abs(FMath::FindDeltaAngleDegrees(PredictedYaw, Response.AuthorityGrappleTravelYaw));

	GrappleNetworkSnapshot.AuthorityGeneration = Response.AuthorityGeneration;
	GrappleNetworkSnapshot.AuthorityTrajectoryScale = Response.AuthorityGrappleTrajectoryScale;
	GrappleNetworkSnapshot.AuthorityTravelYaw = Response.AuthorityGrappleTravelYaw;
	GrappleNetworkSnapshot.AuthorityStartLocation = Response.AuthorityGrappleStartLocation;
	GrappleNetworkSnapshot.LastCorrectionDistance = StartDistance;
	GrappleNetworkSnapshot.bCorrectionRequired =
	    !FMath::IsFinite(StartDistance) || ScaleDifference > TrajectoryScaleTolerance ||
	    YawDifference > TravelYawTolerance || StartDistance > Definition->QuerySpec.MaximumStartDriftBeforeReject;
	return !GrappleNetworkSnapshot.bCorrectionRequired;
}

void UWuwaActionNetworkComponent::HandlePredictedMoveReplay(const FWuwaPendingNetworkMovementCommand& Command)
{
	if (Command.Kind == EWuwaNetworkMovementCommandKind::Grapple)
	{
		const FWuwaGrappleMovementSnapshot Snapshot = Movement->GetGrappleMovementSnapshot();
		if (Snapshot.bActive && Snapshot.NetworkGeneration == Command.Generation)
		{
			GrappleNetworkSnapshot.LateralInputSource = Snapshot.LateralInputSource;
			return;
		}

		GrappleNetworkSnapshot.bCorrectionRequired = true;
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Grapple SavedMove 未恢复匹配 Runtime。Owner=%s, Generation=%d"),
		       *GetNameSafe(CharacterOwner),
		       Command.Generation.Value);
		CancelPredictedGeneration(Command.Generation, EWuwaGrappleMovementEndReason::NetworkCorrection);
		return;
	}

	const UWuwaMovementActionDefinition* Definition =
	    Cast<UWuwaMovementActionDefinition>(DefinitionRegistry.FindRef(Command.ActionTag));
	if (!IsValid(Definition) || Definition->MovementDriver != EWuwaMovementActionDriver::AirJump)
	{
		return;
	}

	const EWuwaJumpType JumpType = Definition->AirJumpVariant == EWuwaAirJumpVariant::Directional
	                                   ? EWuwaJumpType::AirSprint
	                                   : EWuwaJumpType::AirBackflip;
	Movement->ReplayAirJump(Command.WorldDirection, JumpType, Command.AirCycleGeneration);
}

float UWuwaActionNetworkComponent::GetSynchronizedServerTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
	return GameState != nullptr ? GameState->GetServerWorldTimeSeconds()
	                            : (World != nullptr ? World->GetTimeSeconds() : 0.f);
}

void UWuwaActionNetworkComponent::RemovePendingGeneration(const FWuwaNetworkActionGeneration& Generation)
{
	if (Generation.IsValid())
	{
		PendingCommandsByGeneration.Remove(Generation.Value);
	}
}
