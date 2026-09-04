#include "Traversal/Query/WuwaGrappleQuery.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Traversal/Data/WuwaGrappleActionDefinition.h"
#include "Traversal/Query/WuwaGrappleTrajectoryEvaluator.h"

namespace
{
bool IsFiniteGrappleQueryVector(const FVector& Value)
{
	return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
}
}

bool FWuwaGrappleQuery::Query(const FWuwaActionResolutionSnapshot& Snapshot,
                              const UWuwaGrappleActionDefinition* Definition,
                              const int64 QueryFrameNumber,
                              FWuwaGrappleActionContext& OutContext,
                              FWuwaGrappleQueryResult& OutResult,
                              TArray<FVector>& OutPredictedPoints)
{
	OutContext = FWuwaGrappleActionContext();
	OutResult = FWuwaGrappleQueryResult();
	OutPredictedPoints.Reset();

	if (!IsValid(Definition) || !Definition->IsRuntimeValid())
	{
		OutResult.FailureReason = EWuwaGrappleQueryFailureReason::InvalidDefinition;
		return false;
	}

	AActor* SourceActor = Cast<AActor>(Snapshot.SourceObject.Get());
	UWorld* World = IsValid(SourceActor) ? SourceActor->GetWorld() : nullptr;
	if (!IsValid(SourceActor) || !IsValid(World) || World->bIsTearingDown)
	{
		OutResult.FailureReason = EWuwaGrappleQueryFailureReason::InvalidWorld;
		return false;
	}

	if (Snapshot.ViewRotation.ContainsNaN())
	{
		OutResult.FailureReason = EWuwaGrappleQueryFailureReason::InvalidView;
		return false;
	}

	FVector TravelDirection = Snapshot.ViewRotation.Vector();
	TravelDirection.Z = 0.f;
	if (!IsFiniteGrappleQueryVector(TravelDirection) || TravelDirection.IsNearlyZero())
	{
		OutResult.FailureReason = EWuwaGrappleQueryFailureReason::InvalidView;
		return false;
	}
	TravelDirection.Normalize();

	if (!Snapshot.IsValid())
	{
		OutResult.FailureReason = EWuwaGrappleQueryFailureReason::InvalidWorld;
		return false;
	}

	const FVector QueryStartLocation = SourceActor->GetActorLocation();
	const FVector EntryVelocity = SourceActor->GetVelocity();
	if (!IsFiniteGrappleQueryVector(QueryStartLocation) || !IsFiniteGrappleQueryVector(EntryVelocity) ||
	    QueryFrameNumber <= 0)
	{
		OutResult.FailureReason = EWuwaGrappleQueryFailureReason::InvalidWorld;
		return false;
	}

	OutContext.QueryStartLocation = QueryStartLocation;
	OutContext.TravelDirection = TravelDirection;
	OutContext.ViewRotation = Snapshot.ViewRotation.GetNormalized();
	OutContext.LateralAxis = FVector::CrossProduct(FVector::UpVector, TravelDirection).GetSafeNormal();
	OutContext.VisualAnchorLocation = QueryStartLocation +
	                                  TravelDirection * Definition->QuerySpec.VisualAnchorForwardDistance +
	                                  FVector::UpVector * Definition->QuerySpec.VisualAnchorHeight;
	OutContext.ForwardTravelDistance = Definition->MovementSpec.ForwardTravelDistance;
	OutContext.VerticalTravelHeight = Definition->MovementSpec.VerticalTravelHeight;
	OutContext.TrajectoryScale = 1.f;
	OutContext.EntryVelocity = EntryVelocity;
	OutContext.bStartedGrounded = Snapshot.bIsGrounded;
	OutContext.QueryFrameNumber = QueryFrameNumber;

	OutResult.QueryStartLocation = QueryStartLocation;
	OutResult.RequestedVisualAnchorLocation = OutContext.VisualAnchorLocation;
	OutResult.ResolvedVisualAnchorLocation = OutContext.VisualAnchorLocation;
	OutResult.RequestedTrajectoryScale = 1.f;

	float ResolvedScale = 0.f;
	if (!OutContext.IsRuntimeValid() ||
	    !ResolveSafeTrajectory(
	        World, SourceActor, *Definition, OutContext, 1.f, ResolvedScale, OutResult, OutPredictedPoints))
	{
		OutResult.FailureReason = EWuwaGrappleQueryFailureReason::InvalidDefinition;
		return false;
	}

	OutContext.TrajectoryScale = ResolvedScale;

	if (ResolvedScale < Definition->QuerySpec.MinimumTrajectoryScale)
	{
		OutResult.FailureReason = EWuwaGrappleQueryFailureReason::TrajectoryBlocked;
		return false;
	}

	const float ResolvedForwardDistance = Definition->MovementSpec.ForwardTravelDistance * ResolvedScale;
	if (ResolvedForwardDistance < Definition->QuerySpec.MinimumForwardTravelDistance)
	{
		OutResult.FailureReason = EWuwaGrappleQueryFailureReason::TooShort;
		return false;
	}

	if (!HasReleaseClearance(World, SourceActor, *Definition, OutResult.ResolvedReleaseLocation))
	{
		OutResult.FailureReason = EWuwaGrappleQueryFailureReason::NoReleaseClearance;
		return false;
	}

	OutResult.bSucceeded = true;
	OutResult.FailureReason = EWuwaGrappleQueryFailureReason::None;
	return true;
}

bool FWuwaGrappleQuery::RevalidateFrozenContext(const UWuwaGrappleActionDefinition* Definition,
                                                AActor* Owner,
                                                const FVector& CommitStartLocation,
                                                const FWuwaGrappleActionContext& FrozenContext,
                                                FWuwaGrappleActionContext& OutAdjustedContext,
                                                FWuwaGrappleQueryResult& OutResult)
{
	OutAdjustedContext = FrozenContext;
	OutResult = FWuwaGrappleQueryResult();
	TArray<FVector> PredictedPoints;

	UWorld* World = IsValid(Owner) ? Owner->GetWorld() : nullptr;
	if (!IsValid(Definition) || !Definition->IsRuntimeValid() || !FrozenContext.IsRuntimeValid() ||
	    !IsFiniteGrappleQueryVector(CommitStartLocation) || !IsValid(Owner) || !IsValid(World))
	{
		OutResult.FailureReason = EWuwaGrappleQueryFailureReason::InvalidDefinition;
		return false;
	}

	OutResult.QueryStartLocation = CommitStartLocation;
	OutResult.RequestedVisualAnchorLocation = FrozenContext.VisualAnchorLocation;
	OutResult.ResolvedVisualAnchorLocation = FrozenContext.VisualAnchorLocation;
	OutResult.RequestedTrajectoryScale = FrozenContext.TrajectoryScale;

	if (FVector::Dist(CommitStartLocation, FrozenContext.QueryStartLocation) >
	    Definition->QuerySpec.MaximumStartDriftBeforeReject)
	{
		OutResult.FailureReason = EWuwaGrappleQueryFailureReason::StartDriftTooLarge;
		return false;
	}

	FWuwaGrappleActionContext CommitContext = FrozenContext;
	CommitContext.QueryStartLocation = CommitStartLocation;
	float ResolvedScale = 0.f;
	if (!ResolveSafeTrajectory(World,
	                           Owner,
	                           *Definition,
	                           CommitContext,
	                           FrozenContext.TrajectoryScale,
	                           ResolvedScale,
	                           OutResult,
	                           PredictedPoints))
	{
		OutResult.FailureReason = EWuwaGrappleQueryFailureReason::InvalidDefinition;
		return false;
	}

	OutAdjustedContext.TrajectoryScale = ResolvedScale;
	if (ResolvedScale < Definition->QuerySpec.MinimumTrajectoryScale)
	{
		OutResult.FailureReason = EWuwaGrappleQueryFailureReason::TrajectoryBlocked;
		return false;
	}

	if (Definition->MovementSpec.ForwardTravelDistance * ResolvedScale <
	    Definition->QuerySpec.MinimumForwardTravelDistance)
	{
		OutResult.FailureReason = EWuwaGrappleQueryFailureReason::TooShort;
		return false;
	}

	if (!HasReleaseClearance(World, Owner, *Definition, OutResult.ResolvedReleaseLocation))
	{
		OutResult.FailureReason = EWuwaGrappleQueryFailureReason::NoReleaseClearance;
		return false;
	}

	OutResult.bSucceeded = true;
	OutResult.FailureReason = EWuwaGrappleQueryFailureReason::None;
	return true;
}

bool FWuwaGrappleQuery::BuildTrajectoryPoints(const UWuwaGrappleActionDefinition& Definition,
                                              const FWuwaGrappleActionContext& BaseContext,
                                              const float TrajectoryScale,
                                              TArray<FVector>& OutPoints)
{
	OutPoints.Reset();
	if (!FMath::IsFinite(TrajectoryScale) || TrajectoryScale < 0.f || TrajectoryScale > 1.f)
	{
		return false;
	}

	FWuwaGrappleActionContext ScaledContext = BaseContext;
	ScaledContext.TrajectoryScale = FMath::Max(TrajectoryScale, UE_KINDA_SMALL_NUMBER);
	OutPoints.Reserve(Definition.QuerySpec.TrajectorySampleCount + 1);

	for (int32 SampleIndex = 0; SampleIndex <= Definition.QuerySpec.TrajectorySampleCount; ++SampleIndex)
	{
		const float Time =
		    static_cast<float>(SampleIndex) / static_cast<float>(Definition.QuerySpec.TrajectorySampleCount);
		const FWuwaGrappleTrajectorySample Sample =
		    FWuwaGrappleTrajectoryEvaluator::Evaluate(Definition.MovementSpec, ScaledContext, Time);
		const FVector Point = BaseContext.QueryStartLocation + Sample.BaseOffset;
		if (!Sample.IsFinite() || !IsFiniteGrappleQueryVector(Point))
		{
			OutPoints.Reset();
			return false;
		}
		OutPoints.Add(Point);
	}

	return OutPoints.Num() == Definition.QuerySpec.TrajectorySampleCount + 1;
}

bool FWuwaGrappleQuery::IsTrajectoryClear(UWorld* World,
                                          AActor* Owner,
                                          const UWuwaGrappleActionDefinition& Definition,
                                          const TArray<FVector>& Points,
                                          FHitResult& OutBlockingHit,
                                          int32& OutBlockingSegmentIndex)
{
	OutBlockingHit = FHitResult();
	OutBlockingSegmentIndex = INDEX_NONE;
	if (!IsValid(World) || Points.Num() < 2)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WuwaGrappleTrajectory), false, Owner);
	const float SweepRadius = Definition.QuerySpec.TrajectorySweepRadius + Definition.QuerySpec.TrajectorySafetyMargin;
	const FCollisionShape SweepShape = FCollisionShape::MakeSphere(SweepRadius);

	for (int32 SegmentIndex = 0; SegmentIndex + 1 < Points.Num(); ++SegmentIndex)
	{
		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit,
		                                Points[SegmentIndex],
		                                Points[SegmentIndex + 1],
		                                FQuat::Identity,
		                                Definition.QuerySpec.TraceChannel.GetValue(),
		                                SweepShape,
		                                QueryParams))
		{
			OutBlockingHit = Hit;
			OutBlockingSegmentIndex = SegmentIndex;
			return false;
		}
	}

	return true;
}

bool FWuwaGrappleQuery::HasReleaseClearance(UWorld* World,
                                            AActor* Owner,
                                            const UWuwaGrappleActionDefinition& Definition,
                                            const FVector& ReleaseLocation)
{
	if (!IsValid(World))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WuwaGrappleReleaseClearance), false, Owner);
	const float Margin = Definition.QuerySpec.TrajectorySafetyMargin;
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(
	    Definition.QuerySpec.ReleaseClearanceRadius + Margin, Definition.QuerySpec.ReleaseClearanceHalfHeight + Margin);
	return !World->OverlapBlockingTestByChannel(
	    ReleaseLocation, FQuat::Identity, Definition.QuerySpec.TraceChannel.GetValue(), Capsule, QueryParams);
}

bool FWuwaGrappleQuery::ResolveSafeTrajectory(UWorld* World,
                                              AActor* Owner,
                                              const UWuwaGrappleActionDefinition& Definition,
                                              const FWuwaGrappleActionContext& BaseContext,
                                              const float RequestedScale,
                                              float& OutResolvedScale,
                                              FWuwaGrappleQueryResult& InOutResult,
                                              TArray<FVector>& OutPoints)
{
	OutResolvedScale = 0.f;
	if (!FMath::IsFinite(RequestedScale) || RequestedScale <= 0.f || RequestedScale > 1.f ||
	    !BuildTrajectoryPoints(Definition, BaseContext, RequestedScale, OutPoints))
	{
		return false;
	}

	InOutResult.RequestedTrajectoryScale = RequestedScale;
	InOutResult.RequestedReleaseLocation = OutPoints.Last();
	FHitResult RequestedBlockingHit;
	int32 RequestedBlockingSegmentIndex = INDEX_NONE;
	OutResolvedScale = RequestedScale;

	if (!IsTrajectoryClear(World, Owner, Definition, OutPoints, RequestedBlockingHit, RequestedBlockingSegmentIndex))
	{
		InOutResult.FirstBlockingHit = RequestedBlockingHit;
		InOutResult.BlockingSegmentIndex = RequestedBlockingSegmentIndex;

		constexpr int32 BinarySearchIterationCount = 12;
		float SafeScale = 0.f;
		float BlockedScale = RequestedScale;
		TArray<FVector> CandidatePoints;
		for (int32 Iteration = 0; Iteration < BinarySearchIterationCount; ++Iteration)
		{
			const float CandidateScale = (SafeScale + BlockedScale) * 0.5f;
			FHitResult CandidateHit;
			int32 CandidateSegment = INDEX_NONE;
			if (BuildTrajectoryPoints(Definition, BaseContext, CandidateScale, CandidatePoints) &&
			    IsTrajectoryClear(World, Owner, Definition, CandidatePoints, CandidateHit, CandidateSegment))
			{
				SafeScale = CandidateScale;
			}
			else
			{
				BlockedScale = CandidateScale;
			}
		}
		OutResolvedScale = SafeScale;
	}

	if (!BuildTrajectoryPoints(Definition, BaseContext, OutResolvedScale, OutPoints))
	{
		return false;
	}

	InOutResult.ResolvedTrajectoryScale = OutResolvedScale;
	InOutResult.ResolvedReleaseLocation = OutPoints.Last();
	return true;
}
