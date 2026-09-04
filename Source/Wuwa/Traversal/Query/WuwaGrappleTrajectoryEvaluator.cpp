#include "Traversal/Query/WuwaGrappleTrajectoryEvaluator.h"

#include "Curves/CurveFloat.h"
#include "Traversal/Contracts/WuwaTraversalTypes.h"

namespace
{
bool IsFiniteGrappleTrajectoryVector(const FVector& Value)
{
	return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
}

float EvaluateCurveDerivative(const UCurveFloat& Curve, const float Time)
{
	constexpr float DerivativeStep = 0.001f;
	const float StartTime = FMath::Max(0.f, Time - DerivativeStep);
	const float EndTime = FMath::Min(1.f, Time + DerivativeStep);
	const float TimeRange = EndTime - StartTime;
	if (TimeRange <= UE_SMALL_NUMBER)
	{
		return 0.f;
	}

	return (Curve.GetFloatValue(EndTime) - Curve.GetFloatValue(StartTime)) / TimeRange;
}
}

bool FWuwaGrappleTrajectorySample::IsFinite() const
{
	return IsFiniteGrappleTrajectoryVector(BaseOffset) && IsFiniteGrappleTrajectoryVector(TangentVelocity);
}

FWuwaGrappleTrajectorySample FWuwaGrappleTrajectoryEvaluator::Evaluate(const FWuwaGrappleMovementSpec& Spec,
                                                                       const FWuwaGrappleActionContext& Context,
                                                                       const float NormalizedPullTime)
{
	FWuwaGrappleTrajectorySample Sample;
	if (!Spec.IsRuntimeValid() || !Context.IsRuntimeValid() || !FMath::IsFinite(NormalizedPullTime))
	{
		return Sample;
	}

	const float Time = FMath::Clamp(NormalizedPullTime, 0.f, 1.f);
	const float ForwardValue = Spec.ForwardDistanceCurve->GetFloatValue(Time);
	const float VerticalValue = Spec.VerticalDistanceCurve->GetFloatValue(Time);
	const float ForwardDerivative = EvaluateCurveDerivative(*Spec.ForwardDistanceCurve, Time);
	const float VerticalDerivative = EvaluateCurveDerivative(*Spec.VerticalDistanceCurve, Time);
	const float Scale = Context.TrajectoryScale;

	Sample.BaseOffset = Context.TravelDirection * Context.ForwardTravelDistance * Scale * ForwardValue +
	                    FVector::UpVector * Context.VerticalTravelHeight * Scale * VerticalValue;

	Sample.TangentVelocity = (Context.TravelDirection * Context.ForwardTravelDistance * Scale * ForwardDerivative +
	                          FVector::UpVector * Context.VerticalTravelHeight * Scale * VerticalDerivative) /
	                         Spec.PullDuration;

	if (!Sample.IsFinite())
	{
		return FWuwaGrappleTrajectorySample();
	}

	return Sample;
}
