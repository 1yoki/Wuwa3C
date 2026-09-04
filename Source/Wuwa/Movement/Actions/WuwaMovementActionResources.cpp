#include "Movement/Actions/WuwaMovementActionCapabilityComponent.h"

#include "Core/WuwaGameplayTags.h"
#include "GameFramework/RootMotionSource.h"
#include "Messaging/WuwaCharacterMessageDispatcherComponent.h"
#include "Movement/Actions/WuwaMovementActionDefinition.h"
#include "Movement/Actions/WuwaMovementActionPolicies.h"
#include "Movement/Network/WuwaCharacterNetworkMoveTypes.h"
#include "Movement/WuwaCharacterMovementComponent.h"
#include "Misc/Crc.h"
#include "WuwaCharacter.h"
#include "Wuwa.h"

namespace
{
constexpr uint16 InvalidMovementActionResourceRootMotionSourceId = static_cast<uint16>(ERootMotionSourceID::Invalid);
constexpr uint16 MovementActionRootMotionPriority = 1000;
constexpr float ActionExitMoveThreshold = 0.1f;
}

bool UWuwaMovementActionCapabilityComponent::CommitMovementDriver(const FWuwaActionCommitMessage& Message)
{
	if (!IsValid(ActiveDefinition) || !IsValid(MovementComponent))
	{
		return false;
	}

	if (ActiveDefinition->MovementDriver == EWuwaMovementActionDriver::RootMotionSource)
	{
		return ApplyRootMotionSource(Message, *ActiveDefinition);
	}

	if (ActiveDefinition->AirJumpVariant == EWuwaAirJumpVariant::Directional)
	{
		return MovementComponent->RequestAirJump(Message.Request.Context.WorldDirection, EWuwaJumpType::AirSprint);
	}

	return MovementComponent->RequestAirJump(Message.Request.Context.WorldDirection, EWuwaJumpType::AirBackflip);
}

bool UWuwaMovementActionCapabilityComponent::ApplyRootMotionSource(const FWuwaActionCommitMessage& Message,
                                                                   const UWuwaMovementActionDefinition& Definition)
{
	if (!Message.Request.NetworkGeneration.IsValid())
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Legacy RMS 缺少跨端 Generation。Owner=%s, Action=%s"),
		       *GetNameSafe(CharacterOwner),
		       *Definition.ActionTag.ToString());
		return false;
	}

	return ApplyRootMotionSourceWithName(
	    BuildNetworkRootMotionSourceName(Message.Request.NetworkGeneration, Definition.ActionTag),
	    Message.Request.Context.WorldDirection,
	    Definition.RootMotionSourceConfig);
}

FName UWuwaMovementActionCapabilityComponent::BuildNetworkRootMotionSourceName(
    const FWuwaNetworkActionGeneration& Generation, const FGameplayTag& ActionTag)
{
	const uint32 ActionTagHash = FCrc::StrCrc32(*ActionTag.ToString());
	return FName(*FString::Printf(TEXT("WuwaActionNet_%d_%08X"), Generation.Value, ActionTagHash));
}

bool UWuwaMovementActionCapabilityComponent::GetLastResolvedRootMotionReleaseVelocity(
    const FWuwaNetworkActionGeneration& ActionGeneration, FVector& OutVelocity) const
{
	OutVelocity = FVector::ZeroVector;
	if (!ActionGeneration.IsValid() || LastResolvedRootMotionReleaseGeneration != ActionGeneration ||
	    LastResolvedRootMotionReleaseVelocity.ContainsNaN())
	{
		return false;
	}

	OutVelocity = LastResolvedRootMotionReleaseVelocity;
	return true;
}

bool UWuwaMovementActionCapabilityComponent::ApplyNetworkRootMotionSourceSpike(
    const FWuwaNetworkActionGeneration& Generation,
    const FGameplayTag& ActionTag,
    const FVector& WorldDirection,
    const FWuwaRootMotionSourceConfig& Config)
{
	if (!Generation.IsValid() || !ActionTag.IsValid())
	{
		UE_LOG(LogWuwa, Warning, TEXT("RMS 路线验证拒绝无效网络身份。Owner=%s"), *GetNameSafe(CharacterOwner));
		return false;
	}

	return ApplyRootMotionSourceWithName(
	    BuildNetworkRootMotionSourceName(Generation, ActionTag), WorldDirection, Config);
}

void UWuwaMovementActionCapabilityComponent::ReleaseNetworkRootMotionSourceSpike(
    const FWuwaNetworkActionGeneration& Generation, const FGameplayTag& ActionTag)
{
	if (!Generation.IsValid() || !ActionTag.IsValid() || !IsValid(MovementComponent))
	{
		return;
	}

	const FName InstanceName = BuildNetworkRootMotionSourceName(Generation, ActionTag);
	MovementComponent->RemoveRootMotionSource(InstanceName);
	if (ActiveRootMotionSourceInstanceName == InstanceName)
	{
		ActiveRootMotionSourceInstanceName = NAME_None;
		ActiveRootMotionSourceId = InvalidMovementActionResourceRootMotionSourceId;
	}
}

bool UWuwaMovementActionCapabilityComponent::ApplyRootMotionSourceWithName(const FName& InstanceName,
                                                                           const FVector& WorldDirection,
                                                                           const FWuwaRootMotionSourceConfig& Config)
{
	const bool bValidInstanceName = !InstanceName.IsNone();
	const bool bValidConfig = Config.IsRuntimeValid();
	const bool bValidCharacter = IsValid(CharacterOwner);
	const bool bValidMovement = IsValid(MovementComponent);
	if (!bValidInstanceName || !bValidConfig || !bValidCharacter || !bValidMovement)
	{
		UE_LOG(LogWuwa,
		       Warning,
		       TEXT("RMS 注入前置条件失败。Owner=%s, Name=%d, Config=%d, Character=%d, Movement=%d"),
		       *GetNameSafe(CharacterOwner),
		       bValidInstanceName ? 1 : 0,
		       bValidConfig ? 1 : 0,
		       bValidCharacter ? 1 : 0,
		       bValidMovement ? 1 : 0);
		return false;
	}

	TSharedPtr<FRootMotionSource> ExistingNamedSource = MovementComponent->GetRootMotionSource(InstanceName);
	if (!ExistingNamedSource.IsValid())
	{
		for (const TSharedPtr<FRootMotionSource>& PendingSource :
		     MovementComponent->CurrentRootMotion.PendingAddRootMotionSources)
		{
			if (PendingSource.IsValid() && PendingSource->InstanceName == InstanceName)
			{
				ExistingNamedSource = PendingSource;
				break;
			}
		}
	}
	if (ExistingNamedSource.IsValid())
	{
		if (!ActiveRootMotionSourceInstanceName.IsNone() && ActiveRootMotionSourceInstanceName != InstanceName)
		{
			UE_LOG(LogWuwa,
			       Error,
			       TEXT("RMS 稳定身份冲突。Owner=%s, ActiveName=%s, RequestedName=%s"),
			       *GetNameSafe(CharacterOwner),
			       *ActiveRootMotionSourceInstanceName.ToString(),
			       *InstanceName.ToString());
			return false;
		}

		ActiveRootMotionSourceInstanceName = InstanceName;
		return true;
	}

	if (HasActiveRootMotionSource())
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("RMS 注入检测到不同稳定身份的活动资源。Owner=%s, ActiveName=%s, RequestedName=%s"),
		       *GetNameSafe(CharacterOwner),
		       *ActiveRootMotionSourceInstanceName.ToString(),
		       *InstanceName.ToString());
		return false;
	}

	const FVector StartLocation = CharacterOwner->GetActorLocation();
	FVector TargetLocation = FVector::ZeroVector;
	if (!FWuwaMovementActionPolicies::ResolveRootMotionTarget(
	        StartLocation, WorldDirection, Config.Distance, TargetLocation))
	{
		UE_LOG(LogWuwa,
		       Warning,
		       TEXT("RMS 目标解析失败。Owner=%s, Name=%s"),
		       *GetNameSafe(CharacterOwner),
		       *InstanceName.ToString());
		return false;
	}

	TSharedPtr<FRootMotionSource_MoveToDynamicForce> RootMotionSource =
	    MakeShared<FRootMotionSource_MoveToDynamicForce>();
	RootMotionSource->InstanceName = InstanceName;
	RootMotionSource->Priority = MovementActionRootMotionPriority;
	RootMotionSource->AccumulateMode = ERootMotionAccumulateMode::Override;
	RootMotionSource->Duration = Config.Duration;
	RootMotionSource->StartLocation = StartLocation;
	RootMotionSource->InitialTargetLocation = TargetLocation;
	RootMotionSource->TargetLocation = TargetLocation;
	RootMotionSource->bRestrictSpeedToExpected = true;
	RootMotionSource->TimeMappingCurve = Config.TimeMappingCurve;
	RootMotionSource->Settings.SetFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate);
	RootMotionSource->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	RootMotionSource->FinishVelocityParams.SetVelocity = FVector::ZeroVector;

	const uint16 NewId = MovementComponent->ApplyRootMotionSource(RootMotionSource);
	if (NewId == InvalidMovementActionResourceRootMotionSourceId)
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("CharacterMovement 拒绝 RMS。Owner=%s, Name=%s"),
		       *GetNameSafe(CharacterOwner),
		       *InstanceName.ToString());
		return false;
	}

	ActiveRootMotionSourceId = NewId;
	ActiveRootMotionSourceInstanceName = InstanceName;
	return true;
}

bool UWuwaMovementActionCapabilityComponent::HasActiveRootMotionSource() const
{
	return ResolveActiveRootMotionSource().IsValid();
}

TSharedPtr<FRootMotionSource> UWuwaMovementActionCapabilityComponent::ResolveActiveRootMotionSource() const
{
	if (!IsValid(MovementComponent) || ActiveRootMotionSourceInstanceName.IsNone())
	{
		return nullptr;
	}
	TSharedPtr<FRootMotionSource> RootMotionSource =
	    MovementComponent->GetRootMotionSource(ActiveRootMotionSourceInstanceName);
	if (RootMotionSource.IsValid())
	{
		return RootMotionSource;
	}

	for (const TSharedPtr<FRootMotionSource>& PendingSource :
	     MovementComponent->CurrentRootMotion.PendingAddRootMotionSources)
	{
		if (PendingSource.IsValid() && PendingSource->InstanceName == ActiveRootMotionSourceInstanceName)
		{
			return PendingSource;
		}
	}
	return nullptr;
}

bool UWuwaMovementActionCapabilityComponent::ResolveRootMotionReleaseVelocity(
    const TSharedPtr<FRootMotionSource>& RootMotionSource, FVector& OutVelocity) const
{
	OutVelocity = FVector::ZeroVector;
	const bool bValidIdentity = IsValid(MovementComponent) && RootMotionSource.IsValid() &&
	                            !ActiveRootMotionSourceInstanceName.IsNone() &&
	                            RootMotionSource->InstanceName == ActiveRootMotionSourceInstanceName;
	if (!bValidIdentity)
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("RMS 退出速度解析缺少稳定资源。Owner=%s, ExpectedName=%s, ActualName=%s"),
		       *GetNameSafe(CharacterOwner),
		       *ActiveRootMotionSourceInstanceName.ToString(),
		       RootMotionSource.IsValid() ? *RootMotionSource->InstanceName.ToString() : TEXT("None"));
		return false;
	}

	const float CurrentTime = RootMotionSource->GetTime();
	const float PreviousTime = RootMotionSource->PreviousTime;
	const float SourceDeltaTime = CurrentTime - PreviousTime;
	const FVector RootMotionVelocity = RootMotionSource->RootMotionParams.GetRootMotionTransform().GetTranslation();
	const bool bValidSourceStep = RootMotionSource->RootMotionParams.bHasRootMotion && FMath::IsFinite(CurrentTime) &&
	                              FMath::IsFinite(PreviousTime) && FMath::IsFinite(SourceDeltaTime) &&
	                              SourceDeltaTime > UE_SMALL_NUMBER && !RootMotionVelocity.ContainsNaN();
	if (!bValidSourceStep)
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("RMS 退出速度解析缺少有效速度步长。Owner=%s, Name=%s, CurrentTime=%.6f, PreviousTime=%.6f, "
		            "SourceDeltaTime=%.6f, RootMotionVelocity=%s, HasRootMotion=%d"),
		       *GetNameSafe(CharacterOwner),
		       *RootMotionSource->InstanceName.ToString(),
		       CurrentTime,
		       PreviousTime,
		       SourceDeltaTime,
		       *RootMotionVelocity.ToCompactString(),
		       RootMotionSource->RootMotionParams.bHasRootMotion ? 1 : 0);
		return false;
	}

	FVector ReleaseVelocity = RootMotionVelocity;
	ReleaseVelocity.Z = MovementComponent->Velocity.Z;
	if (ReleaseVelocity.ContainsNaN())
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("RMS 退出速度解析得到无效结果。Owner=%s, Name=%s, SourceDeltaTime=%.6f, RootMotionVelocity=%s, "
		            "Velocity=%s"),
		       *GetNameSafe(CharacterOwner),
		       *RootMotionSource->InstanceName.ToString(),
		       SourceDeltaTime,
		       *RootMotionVelocity.ToCompactString(),
		       *ReleaseVelocity.ToCompactString());
		return false;
	}

	OutVelocity = ReleaseVelocity;
	return true;
}

void UWuwaMovementActionCapabilityComponent::ReleaseActiveRootMotionSource()
{
	const bool bShouldPreserveVelocity = PhysicalRuntime.bPreserveVelocityOnRelease;
	PhysicalRuntime.bPreserveVelocityOnRelease = false;
	const FName InstanceName = ActiveRootMotionSourceInstanceName;
	const TSharedPtr<FRootMotionSource> RootMotionSource = ResolveActiveRootMotionSource();
	FVector ReleaseVelocity = FVector::ZeroVector;
	const bool bHasReleaseVelocity =
	    bShouldPreserveVelocity && ResolveRootMotionReleaseVelocity(RootMotionSource, ReleaseVelocity);
	ActiveRootMotionSourceInstanceName = NAME_None;
	ActiveRootMotionSourceId = InvalidMovementActionResourceRootMotionSourceId;

	if (!IsValid(MovementComponent) || InstanceName.IsNone())
	{
		return;
	}

	if (bHasReleaseVelocity)
	{
		RootMotionSource->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
		RootMotionSource->FinishVelocityParams.SetVelocity = ReleaseVelocity;
		LastResolvedRootMotionReleaseGeneration = PhysicalRuntime.Generation;
		LastResolvedRootMotionReleaseVelocity = ReleaseVelocity;
	}

	MovementComponent->RemoveRootMotionSource(InstanceName);
}

bool UWuwaMovementActionCapabilityComponent::AcquireFacingRotationOverride(const EWuwaActionFacingPolicy FacingPolicy,
                                                                           const FVector& WorldDirection)
{
	if (!FWuwaMovementActionPolicies::RequiresFacingOverride(FacingPolicy))
	{
		if (IsValid(MovementComponent))
		{
			PhysicalRuntime.bSavedOrientRotationToMovement = MovementComponent->bOrientRotationToMovement;
			PhysicalRuntime.bSavedUseControllerDesiredRotation = MovementComponent->bUseControllerDesiredRotation;
		}
		if (IsValid(CharacterOwner))
		{
			PhysicalRuntime.bSavedUseControllerRotationYaw = CharacterOwner->bUseControllerRotationYaw;
		}
		PhysicalRuntime.bFacingOverrideActive = false;
		return true;
	}

	if (PhysicalRuntime.bFacingOverrideActive || !IsValid(MovementComponent) || !IsValid(CharacterOwner))
	{
		return false;
	}

	PhysicalRuntime.bSavedOrientRotationToMovement = MovementComponent->bOrientRotationToMovement;
	PhysicalRuntime.bSavedUseControllerDesiredRotation = MovementComponent->bUseControllerDesiredRotation;
	PhysicalRuntime.bSavedUseControllerRotationYaw = CharacterOwner->bUseControllerRotationYaw;

	MovementComponent->bOrientRotationToMovement = false;
	MovementComponent->bUseControllerDesiredRotation = false;
	CharacterOwner->bUseControllerRotationYaw = false;
	PhysicalRuntime.bFacingOverrideActive = true;

	return true;
}

void UWuwaMovementActionCapabilityComponent::ReleaseFacingRotationOverride()
{
	if (!PhysicalRuntime.bFacingOverrideActive)
	{
		return;
	}

	if (IsValid(MovementComponent))
	{
		MovementComponent->bOrientRotationToMovement = PhysicalRuntime.bSavedOrientRotationToMovement;
		MovementComponent->bUseControllerDesiredRotation = PhysicalRuntime.bSavedUseControllerDesiredRotation;
	}

	if (IsValid(CharacterOwner))
	{
		CharacterOwner->bUseControllerRotationYaw = PhysicalRuntime.bSavedUseControllerRotationYaw;
	}

	PhysicalRuntime.bFacingOverrideActive = false;
}

void UWuwaMovementActionCapabilityComponent::ReleaseActiveResources()
{
	PhysicalRuntime.bPreserveVelocityOnRelease = false;
	PhysicalRuntime.ExitPolicy = EWuwaMovementActionExitPolicy::None;
	PhysicalRuntime.ExitMoveIntent = FVector2D::ZeroVector;
	ReleaseActiveRootMotionSource();
	ReleaseFacingRotationOverride();
	ResetPhysicalRuntime();
	ResetActiveRuntime();
}

bool UWuwaMovementActionCapabilityComponent::ShouldDeferPhysicalFinalize(const FWuwaActionStopMessage& Message) const
{
	if (!PhysicalRuntime.bMovementActionActive || !PhysicalRuntime.Generation.IsValid() ||
	    ActiveRequest.NetworkGeneration != PhysicalRuntime.Generation ||
	    PhysicalRuntime.ExitMoveIntent.IsNearlyZero(ActionExitMoveThreshold) ||
	    (Message.EndReason != EWuwaActionEndReason::Completed && Message.EndReason != EWuwaActionEndReason::Cancelled))
	{
		return false;
	}

	if (IsValid(CharacterOwner) && CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy)
	{
		return true;
	}

	return IsValid(CharacterOwner) && CharacterOwner->HasAuthority() &&
	       (!CharacterOwner->IsLocallyControlled() || PreparedNetworkExitGeneration.IsValid());
}

bool UWuwaMovementActionCapabilityComponent::FinalizePhysicalMovementAction(
    const FWuwaNetworkActionGeneration& ExitGeneration, const bool bIsReplay)
{
	(void)bIsReplay;
	if (!PhysicalRuntime.bMovementActionActive)
	{
		return !ExitGeneration.IsValid() || PhysicalRuntime.LastAppliedExitGeneration == ExitGeneration;
	}

	const EWuwaMovementActionExitPolicy ExitPolicy = PhysicalRuntime.ExitPolicy;
	const FVector2D ExitMoveIntent = PhysicalRuntime.ExitMoveIntent;

	ReleaseActiveRootMotionSource();
	ReleaseFacingRotationOverride();

	if (ExitPolicy == EWuwaMovementActionExitPolicy::TryEnterSprintRun && IsValid(MovementComponent) &&
	    MovementComponent->IsMovingOnGround() && !ExitMoveIntent.IsNearlyZero(ActionExitMoveThreshold))
	{
		MovementComponent->EnterSprintRun(ExitMoveIntent);
	}

	const FWuwaNetworkActionGeneration LastAppliedExitGeneration =
	    ExitGeneration.IsValid() ? ExitGeneration : PhysicalRuntime.LastAppliedExitGeneration;
	ResetPhysicalRuntime();
	PhysicalRuntime.LastAppliedExitGeneration = LastAppliedExitGeneration;

	return true;
}

void UWuwaMovementActionCapabilityComponent::ResetPhysicalRuntime()
{
	const FWuwaNetworkActionGeneration LastAppliedExitGeneration = PhysicalRuntime.LastAppliedExitGeneration;
	PhysicalRuntime.Reset();
	PhysicalRuntime.LastAppliedExitGeneration = LastAppliedExitGeneration;
	PreparedNetworkExitGeneration = FWuwaNetworkActionGeneration();
	PhysicalActionDefinition = nullptr;
	ActiveRootMotionSourceInstanceName = NAME_None;
	ActiveRootMotionSourceId = InvalidMovementActionResourceRootMotionSourceId;
}

void UWuwaMovementActionCapabilityComponent::ResetActiveRuntime()
{
	ActiveHandle = FWuwaActionHandle();
	ActiveDefinition = nullptr;
	ActiveRequest = FWuwaActionRequest();
	ResetNetworkActionExitState();
}

bool UWuwaMovementActionCapabilityComponent::PrepareCompletedExit(const FVector2D& MoveIntent)
{
	const bool bFiniteIntent = FMath::IsFinite(MoveIntent.X) && FMath::IsFinite(MoveIntent.Y);
	if (!PhysicalRuntime.bMovementActionActive || !IsValid(PhysicalActionDefinition) || !IsValid(MovementComponent) ||
	    !bFiniteIntent || MoveIntent.IsNearlyZero(ActionExitMoveThreshold) ||
	    (PhysicalActionDefinition->ExitPolicy == EWuwaMovementActionExitPolicy::TryEnterSprintRun &&
	     !MovementComponent->IsMovingOnGround()))
	{
		return false;
	}

	PhysicalRuntime.bPreserveVelocityOnRelease = true;
	PhysicalRuntime.ExitPolicy = PhysicalActionDefinition->ExitPolicy;
	PhysicalRuntime.ExitMoveIntent = MoveIntent.GetClampedToMaxSize(1.f);
	return true;
}

void UWuwaMovementActionCapabilityComponent::ResetNetworkActionExitState()
{
	bMoveCancelWindowOpen = false;
	bCompletedExitReady = false;
	PreparedNetworkExitGeneration = FWuwaNetworkActionGeneration();
}

bool UWuwaMovementActionCapabilityComponent::ResolveEventResponse(const FGameplayTag& EventTag,
                                                                  EWuwaMovementActionEventResponse& OutResponse) const
{
	if (!IsValid(ActiveDefinition))
	{
		return false;
	}

	return FWuwaMovementActionPolicies::ResolveEventResponse(*ActiveDefinition, EventTag, OutResponse);
}

void UWuwaMovementActionCapabilityComponent::HandleLandedEvent(const FWuwaLandingEvent& LandingEvent)
{
	(void)LandingEvent;

	if (ActiveHandle.IsValid() && Dispatcher.IsValid())
	{
		Dispatcher->PublishActionEvent(ActiveHandle,
		                               WuwaGameplayTags::Action_Event_Movement_Landed,
		                               FVector2D::ZeroVector,
		                               MOVE_Falling,
		                               MovementComponent->MovementMode,
		                               0,
		                               MovementComponent->CustomMovementMode,
		                               this);
	}
}

void UWuwaMovementActionCapabilityComponent::HandleMovementModeChangedEvent(const EMovementMode PreviousMovementMode,
                                                                            const uint8 PreviousCustomMode,
                                                                            const EMovementMode NewMovementMode,
                                                                            const uint8 NewCustomMode)
{
	const bool bWasGrounded = PreviousMovementMode == MOVE_Walking || PreviousMovementMode == MOVE_NavWalking;
	if (!ActiveHandle.IsValid() || !Dispatcher.IsValid() || !bWasGrounded || NewMovementMode != MOVE_Falling)
	{
		return;
	}

	Dispatcher->PublishActionEvent(ActiveHandle,
	                               WuwaGameplayTags::Action_Event_Movement_LeftGround,
	                               FVector2D::ZeroVector,
	                               PreviousMovementMode,
	                               NewMovementMode,
	                               PreviousCustomMode,
	                               NewCustomMode,
	                               this);
}

void UWuwaMovementActionCapabilityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseActiveResources();

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

	MovementModeChangedDelegateHandle.Reset();
	Dispatcher = nullptr;
	MovementComponent = nullptr;
	CharacterOwner = nullptr;

	Super::EndPlay(EndPlayReason);
}
