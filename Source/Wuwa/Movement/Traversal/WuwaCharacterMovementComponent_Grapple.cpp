#include "Movement/WuwaCharacterMovementComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "Movement/Network/WuwaSavedMoveCharacter.h"
#include "Traversal/Query/WuwaGrappleTrajectoryEvaluator.h"
#include "Wuwa.h"

namespace
{
bool IsFiniteGrappleMovementVector(const FVector& Value)
{
	return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
}
}

bool UWuwaCharacterMovementComponent::StartGrappleMovement(const FWuwaGrappleMovementRequest& Request,
                                                           FWuwaGrappleMovementHandle& OutHandle)
{
	OutHandle = FWuwaGrappleMovementHandle();
	const bool bAllowedMovementMode =
	    MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking || MovementMode == MOVE_Falling;
	if (GrappleRuntime.Handle.IsValid() || !HasValidData() || !IsValid(UpdatedComponent) || !Request.IsRuntimeValid() ||
	    !bAllowedMovementMode)
	{
		return false;
	}

	const FVector CommitStartLocation = UpdatedComponent->GetComponentLocation();
	if (!IsFiniteGrappleMovementVector(CommitStartLocation))
	{
		return false;
	}

	const FVector PreviousVelocity = Velocity;

	GrappleRuntime = FWuwaGrappleMovementRuntime();
	GrappleReplayRuntimeCache = FWuwaGrappleMovementRuntime();
	GrappleRuntime.Handle.Value = NextGrappleMovementHandle++;
	if (NextGrappleMovementHandle <= 0)
	{
		NextGrappleMovementHandle = 1;
	}
	GrappleRuntime.Request = Request;
	GrappleRuntime.CommitStartLocation = CommitStartLocation;
	GrappleRuntime.Phase = EWuwaGrappleRuntimePhase::Windup;
	Velocity = Request.Context.EntryVelocity;

	GrappleMovementSnapshot = FWuwaGrappleMovementSnapshot();
	GrappleMovementSnapshot.Handle = GrappleRuntime.Handle;
	GrappleMovementSnapshot.NetworkGeneration = Request.NetworkGeneration;
	GrappleMovementSnapshot.bActive = true;
	GrappleMovementSnapshot.Phase = EWuwaGrappleRuntimePhase::Windup;
	GrappleMovementSnapshot.CommitStartLocation = CommitStartLocation;
	GrappleMovementSnapshot.LateralInputSource = EWuwaGrappleLateralInputSource::AccelerationProjection;

	SetMovementMode(MOVE_Custom, static_cast<uint8>(EWuwaCustomMovementMode::Grapple));
	if (MovementMode != MOVE_Custom || CustomMovementMode != static_cast<uint8>(EWuwaCustomMovementMode::Grapple))
	{
		GrappleRuntime = FWuwaGrappleMovementRuntime();
		GrappleMovementSnapshot = FWuwaGrappleMovementSnapshot();
		Velocity = PreviousVelocity;
		return false;
	}

	OutHandle = GrappleRuntime.Handle;
	return true;
}

bool UWuwaCharacterMovementComponent::StopGrappleMovement(const FWuwaGrappleMovementHandle& Handle)
{
	return StopGrappleMovement(Handle, EWuwaGrappleMovementEndReason::Stopped);
}

bool UWuwaCharacterMovementComponent::StopGrappleMovement(const FWuwaGrappleMovementHandle& Handle,
                                                          const EWuwaGrappleMovementEndReason EndReason)
{
	if (!Handle.IsValid() || Handle != GrappleRuntime.Handle || EndReason == EWuwaGrappleMovementEndReason::None ||
	    EndReason == EWuwaGrappleMovementEndReason::Released || EndReason == EWuwaGrappleMovementEndReason::Blocked ||
	    EndReason == EWuwaGrappleMovementEndReason::TimedOut || EndReason == EWuwaGrappleMovementEndReason::Landed)
	{
		return false;
	}

	FinishGrappleMovement(EndReason);
	return true;
}

void UWuwaCharacterMovementComponent::CaptureGrappleNetworkReplayState(FWuwaGrappleSavedMoveState& OutState) const
{
	OutState.Reset();
	if (!IsGrappleMovementActive())
	{
		return;
	}

	OutState.NetworkGeneration = GrappleRuntime.Request.NetworkGeneration;
	OutState.CommitStartLocation = GrappleRuntime.CommitStartLocation;
	OutState.PreviousAppliedOffset = GrappleRuntime.PreviousAppliedOffset;
	OutState.LastBaseTangentVelocity = GrappleRuntime.LastBaseTangentVelocity;
	OutState.BaseOffset = GrappleMovementSnapshot.BaseOffset;
	OutState.ElapsedTime = GrappleRuntime.ElapsedTime;
	OutState.NormalizedPullTime = GrappleMovementSnapshot.NormalizedPullTime;
	OutState.AccumulatedLateralOffset = GrappleRuntime.AccumulatedLateralOffset;
	OutState.LateralVelocity = GrappleRuntime.LateralVelocity;
	OutState.Phase = GrappleRuntime.Phase;
	OutState.bActive = true;
}

void UWuwaCharacterMovementComponent::RestoreGrappleNetworkReplayState(const FWuwaGrappleSavedMoveState& State)
{
	if (!State.bActive)
	{
		if (GrappleRuntime.Handle.IsValid())
		{
			GrappleReplayRuntimeCache = GrappleRuntime;
		}
		GrappleRuntime = FWuwaGrappleMovementRuntime();
		GrappleMovementSnapshot = FWuwaGrappleMovementSnapshot();
		return;
	}

	const FWuwaGrappleMovementRuntime* SourceRuntime = nullptr;
	if (GrappleRuntime.Handle.IsValid() && GrappleRuntime.Request.NetworkGeneration == State.NetworkGeneration)
	{
		SourceRuntime = &GrappleRuntime;
	}
	else if (GrappleReplayRuntimeCache.Handle.IsValid() &&
	         GrappleReplayRuntimeCache.Request.NetworkGeneration == State.NetworkGeneration)
	{
		SourceRuntime = &GrappleReplayRuntimeCache;
	}

	const bool bValidState = SourceRuntime != nullptr && SourceRuntime->Request.IsRuntimeValid() &&
	                         State.NetworkGeneration.IsValid() && State.Phase != EWuwaGrappleRuntimePhase::None &&
	                         FMath::IsFinite(State.ElapsedTime) && State.ElapsedTime >= 0.f &&
	                         FMath::IsFinite(State.NormalizedPullTime) &&
	                         FMath::IsFinite(State.AccumulatedLateralOffset) && FMath::IsFinite(State.LateralVelocity);
	if (!bValidState)
	{
		UE_LOG(LogWuwa, Error, TEXT("Grapple SavedMove 包含无效重演状态。Owner=%s"), *GetNameSafe(CharacterOwner));
		GrappleRuntime = FWuwaGrappleMovementRuntime();
		GrappleReplayRuntimeCache = FWuwaGrappleMovementRuntime();
		GrappleMovementSnapshot = FWuwaGrappleMovementSnapshot();
		return;
	}

	GrappleRuntime = *SourceRuntime;
	GrappleRuntime.CommitStartLocation = State.CommitStartLocation;
	GrappleRuntime.PreviousAppliedOffset = State.PreviousAppliedOffset;
	GrappleRuntime.LastBaseTangentVelocity = State.LastBaseTangentVelocity;
	GrappleRuntime.ElapsedTime = State.ElapsedTime;
	GrappleRuntime.AccumulatedLateralOffset = State.AccumulatedLateralOffset;
	GrappleRuntime.LateralVelocity = State.LateralVelocity;
	GrappleRuntime.Phase = State.Phase;
	GrappleReplayRuntimeCache = FWuwaGrappleMovementRuntime();

	GrappleMovementSnapshot = FWuwaGrappleMovementSnapshot();
	GrappleMovementSnapshot.Handle = GrappleRuntime.Handle;
	GrappleMovementSnapshot.NetworkGeneration = State.NetworkGeneration;
	GrappleMovementSnapshot.bActive = true;
	GrappleMovementSnapshot.Phase = State.Phase;
	GrappleMovementSnapshot.CommitStartLocation = State.CommitStartLocation;
	GrappleMovementSnapshot.ElapsedTime = State.ElapsedTime;
	GrappleMovementSnapshot.NormalizedPullTime = State.NormalizedPullTime;
	GrappleMovementSnapshot.BaseOffset = State.BaseOffset;
	GrappleMovementSnapshot.AccumulatedLateralOffset = State.AccumulatedLateralOffset;
	GrappleMovementSnapshot.LateralVelocity = State.LateralVelocity;
	GrappleMovementSnapshot.LateralInputSource = EWuwaGrappleLateralInputSource::AccelerationProjection;
	GrappleMovementSnapshot.LastBaseTangentVelocity = State.LastBaseTangentVelocity;
	NextGrappleMovementHandle = FMath::Max(NextGrappleMovementHandle, GrappleRuntime.Handle.Value + 1);

	if (!IsInGrappleMovementMode())
	{
		SetMovementMode(MOVE_Custom, static_cast<uint8>(EWuwaCustomMovementMode::Grapple));
	}
}

FWuwaGrappleMovementSnapshot UWuwaCharacterMovementComponent::GetGrappleMovementSnapshot() const
{
	return GrappleMovementSnapshot;
}

bool UWuwaCharacterMovementComponent::IsGrappleMovementActive() const
{
	return HasGrappleRuntime() && IsInGrappleMovementMode();
}

bool UWuwaCharacterMovementComponent::IsInGrappleMovementMode() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(EWuwaCustomMovementMode::Grapple);
}

bool UWuwaCharacterMovementComponent::HasGrappleRuntime() const
{
	return GrappleRuntime.Handle.IsValid();
}

FVector UWuwaCharacterMovementComponent::CalculateGrappleExitVelocity(const FWuwaGrappleMovementSpec& Spec,
                                                                      const FWuwaGrappleActionContext& Context,
                                                                      const FVector& BaseTangentVelocity)
{
	if (!Spec.IsRuntimeValid() || !Context.IsRuntimeValid() || !IsFiniteGrappleMovementVector(BaseTangentVelocity))
	{
		return FVector::ZeroVector;
	}

	const float ForwardTangent = FVector::DotProduct(BaseTangentVelocity, Context.TravelDirection);
	const float UpTangent = FVector::DotProduct(BaseTangentVelocity, FVector::UpVector);
	FVector CombinedVelocity = Context.TravelDirection * ForwardTangent * Spec.ExitForwardSpeedScale +
	                           FVector::UpVector * UpTangent * Spec.ExitUpSpeedScale +
	                           Context.EntryVelocity * Spec.EntryVelocityRetention;

	const float ForwardComponent = FVector::DotProduct(CombinedVelocity, Context.TravelDirection);
	if (ForwardComponent < Spec.MinimumExitForwardSpeed)
	{
		CombinedVelocity += Context.TravelDirection * (Spec.MinimumExitForwardSpeed - ForwardComponent);
	}

	const float UpComponent = FVector::DotProduct(CombinedVelocity, FVector::UpVector);
	if (UpComponent < Spec.MinimumExitUpSpeed)
	{
		CombinedVelocity += FVector::UpVector * (Spec.MinimumExitUpSpeed - UpComponent);
	}

	return IsFiniteGrappleMovementVector(CombinedVelocity) ? CombinedVelocity.GetClampedToMaxSize(Spec.MaximumExitSpeed)
	                                                       : FVector::ZeroVector;
}

void UWuwaCharacterMovementComponent::PhysCustom(const float DeltaTime, const int32 Iterations)
{
	if (CustomMovementMode == static_cast<uint8>(EWuwaCustomMovementMode::Grapple))
	{
		PhysGrapple(DeltaTime, Iterations);
		return;
	}

	Super::PhysCustom(DeltaTime, Iterations);
}

void UWuwaCharacterMovementComponent::PhysGrapple(const float DeltaTime, const int32 Iterations)
{
	(void)Iterations;
	if (!IsInGrappleMovementMode() || !HasGrappleRuntime() || !HasValidData() || !IsValid(UpdatedComponent) ||
	    !GrappleRuntime.Request.IsRuntimeValid() || !FMath::IsFinite(DeltaTime))
	{
		FinishGrappleMovement(EWuwaGrappleMovementEndReason::InvalidRuntime);
		return;
	}

	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	const FWuwaGrappleMovementSpec& Spec = GrappleRuntime.Request.Spec;
	const FWuwaGrappleActionContext& Context = GrappleRuntime.Request.Context;
	FRotator GrappleRotation = UpdatedComponent->GetComponentRotation();
	GrappleRotation.Yaw = Context.TravelDirection.Rotation().Yaw;
	const FQuat GrappleRotationQuat = GrappleRotation.Quaternion();
	GrappleRuntime.ElapsedTime += DeltaTime;

	if (GrappleRuntime.ElapsedTime < Spec.WindupDuration)
	{
		Velocity = Context.EntryVelocity +
		           FVector::UpVector * GetGravityZ() * Spec.GravityScaleDuringWindup * GrappleRuntime.ElapsedTime;
		FHitResult RotationHit;
		SafeMoveUpdatedComponent(FVector::ZeroVector, GrappleRotationQuat, true, RotationHit);
		RefreshGrappleMovementSnapshot(0.f);
		return;
	}

	if (GrappleRuntime.Phase == EWuwaGrappleRuntimePhase::Windup)
	{
		EnterGrapplePhase(EWuwaGrappleRuntimePhase::Pulling);
	}

	const float NormalizedPullTime =
	    FMath::Clamp((GrappleRuntime.ElapsedTime - Spec.WindupDuration) / Spec.PullDuration, 0.f, 1.f);
	const FWuwaGrappleTrajectorySample Trajectory =
	    FWuwaGrappleTrajectoryEvaluator::Evaluate(Spec, Context, NormalizedPullTime);
	if (!Trajectory.IsFinite())
	{
		FinishGrappleMovement(EWuwaGrappleMovementEndReason::InvalidRuntime);
		return;
	}

	constexpr float LateralInputDeadZone = 0.1f;
	const float MaximumAcceleration = FMath::Max(GetMaxAcceleration(), UE_KINDA_SMALL_NUMBER);
	const float ProjectedLateralInput =
	    FMath::Clamp(FVector::DotProduct(Acceleration, Context.LateralAxis) / MaximumAcceleration, -1.f, 1.f);
	const float LateralInput = FMath::Abs(ProjectedLateralInput) > LateralInputDeadZone ? ProjectedLateralInput : 0.f;
	if (!FMath::IsNearlyZero(LateralInput))
	{
		GrappleRuntime.LateralVelocity += LateralInput * Spec.LateralSteeringAcceleration * DeltaTime;
		GrappleRuntime.AccumulatedLateralOffset =
		    FMath::Clamp(GrappleRuntime.AccumulatedLateralOffset + GrappleRuntime.LateralVelocity * DeltaTime,
		                 -Spec.MaximumLateralOffset,
		                 Spec.MaximumLateralOffset);
		if (FMath::IsNearlyEqual(FMath::Abs(GrappleRuntime.AccumulatedLateralOffset), Spec.MaximumLateralOffset))
		{
			GrappleRuntime.LateralVelocity = 0.f;
		}
	}
	else
	{
		GrappleRuntime.AccumulatedLateralOffset =
		    FMath::FInterpConstantTo(GrappleRuntime.AccumulatedLateralOffset, 0.f, DeltaTime, Spec.LateralReturnSpeed);
		GrappleRuntime.LateralVelocity = 0.f;
	}

	const FVector AppliedOffset = Trajectory.BaseOffset + Context.LateralAxis * GrappleRuntime.AccumulatedLateralOffset;
	const FVector Delta = AppliedOffset - GrappleRuntime.PreviousAppliedOffset;
	Velocity = Delta / DeltaTime;

	FHitResult Hit;
	SafeMoveUpdatedComponent(Delta, GrappleRotationQuat, true, Hit);
	if (Hit.IsValidBlockingHit())
	{
		RefreshGrappleMovementSnapshot(NormalizedPullTime);
		FinishGrappleMovement(EWuwaGrappleMovementEndReason::Blocked);
		return;
	}

	GrappleRuntime.PreviousAppliedOffset = AppliedOffset;
	GrappleRuntime.LastBaseTangentVelocity = Trajectory.TangentVelocity;
	RefreshGrappleMovementSnapshot(NormalizedPullTime);

	if (NormalizedPullTime >= 1.f)
	{
		const FVector ExitVelocity = CalculateGrappleExitVelocity(Spec, Context, Trajectory.TangentVelocity);
		FinishGrappleMovement(EWuwaGrappleMovementEndReason::Released, ExitVelocity);
		return;
	}

	if (GrappleRuntime.ElapsedTime >= Spec.MaximumDuration)
	{
		FinishGrappleMovement(EWuwaGrappleMovementEndReason::TimedOut);
	}
}

void UWuwaCharacterMovementComponent::FinishGrappleMovement(const EWuwaGrappleMovementEndReason EndReason,
                                                            const FVector& ExitVelocity)
{
	if (!GrappleRuntime.Handle.IsValid() || EndReason == EWuwaGrappleMovementEndReason::None)
	{
		return;
	}

	const FWuwaGrappleMovementHandle EndedHandle = GrappleRuntime.Handle;
	GrappleRuntime.Phase = EWuwaGrappleRuntimePhase::Releasing;
	GrappleMovementSnapshot.Handle = EndedHandle;
	GrappleMovementSnapshot.bActive = false;
	GrappleMovementSnapshot.Phase = EWuwaGrappleRuntimePhase::Releasing;
	GrappleMovementSnapshot.EndReason = EndReason;
	GrappleMovementSnapshot.ExitVelocity =
	    EndReason == EWuwaGrappleMovementEndReason::Released && IsFiniteGrappleMovementVector(ExitVelocity)
	        ? ExitVelocity
	        : FVector::ZeroVector;

	GrappleRuntime = FWuwaGrappleMovementRuntime();
	GrappleReplayRuntimeCache = FWuwaGrappleMovementRuntime();

	if (EndReason == EWuwaGrappleMovementEndReason::Released)
	{
		Velocity = GrappleMovementSnapshot.ExitVelocity;
	}
	else if (EndReason != EWuwaGrappleMovementEndReason::Landed && EndReason != EWuwaGrappleMovementEndReason::TimedOut)
	{
		Velocity = FVector::ZeroVector;
	}

	if (MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(EWuwaCustomMovementMode::Grapple) &&
	    EndReason != EWuwaGrappleMovementEndReason::Landed)
	{
		SetMovementMode(MOVE_Falling);
	}

	FWuwaGrappleMovementFact Fact;
	Fact.Handle = EndedHandle;
	Fact.FactType = EWuwaGrappleMovementFactType::Ended;
	Fact.Phase = EWuwaGrappleRuntimePhase::Releasing;
	Fact.EndReason = EndReason;
	Fact.ExitVelocity = GrappleMovementSnapshot.ExitVelocity;
	OnGrappleMovementFact.Broadcast(Fact);
}

void UWuwaCharacterMovementComponent::EnterGrapplePhase(const EWuwaGrappleRuntimePhase NewPhase)
{
	if (!GrappleRuntime.Handle.IsValid() || GrappleRuntime.Phase == NewPhase)
	{
		return;
	}

	GrappleRuntime.Phase = NewPhase;
	GrappleMovementSnapshot.Phase = NewPhase;

	FWuwaGrappleMovementFact Fact;
	Fact.Handle = GrappleRuntime.Handle;
	Fact.FactType = EWuwaGrappleMovementFactType::PhaseChanged;
	Fact.Phase = NewPhase;
	OnGrappleMovementFact.Broadcast(Fact);
}

void UWuwaCharacterMovementComponent::RefreshGrappleMovementSnapshot(const float NormalizedPullTime)
{
	if (!GrappleRuntime.Handle.IsValid())
	{
		return;
	}

	GrappleMovementSnapshot.Handle = GrappleRuntime.Handle;
	GrappleMovementSnapshot.NetworkGeneration = GrappleRuntime.Request.NetworkGeneration;
	GrappleMovementSnapshot.bActive = true;
	GrappleMovementSnapshot.Phase = GrappleRuntime.Phase;
	GrappleMovementSnapshot.EndReason = EWuwaGrappleMovementEndReason::None;
	GrappleMovementSnapshot.CommitStartLocation = GrappleRuntime.CommitStartLocation;
	GrappleMovementSnapshot.ElapsedTime = GrappleRuntime.ElapsedTime;
	GrappleMovementSnapshot.NormalizedPullTime = NormalizedPullTime;
	GrappleMovementSnapshot.AccumulatedLateralOffset = GrappleRuntime.AccumulatedLateralOffset;
	GrappleMovementSnapshot.LateralVelocity = GrappleRuntime.LateralVelocity;
	GrappleMovementSnapshot.LateralInputSource = EWuwaGrappleLateralInputSource::AccelerationProjection;
	GrappleMovementSnapshot.LastBaseTangentVelocity = GrappleRuntime.LastBaseTangentVelocity;

	const FWuwaGrappleTrajectorySample Trajectory = FWuwaGrappleTrajectoryEvaluator::Evaluate(
	    GrappleRuntime.Request.Spec, GrappleRuntime.Request.Context, NormalizedPullTime);
	GrappleMovementSnapshot.BaseOffset = Trajectory.BaseOffset;
}
