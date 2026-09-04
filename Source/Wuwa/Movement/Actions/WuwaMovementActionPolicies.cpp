#include "Movement/Actions/WuwaMovementActionPolicies.h"

#include "Movement/Actions/WuwaMovementActionDefinition.h"
#include "Movement/WuwaCharacterMovementComponent.h"

bool FWuwaMovementActionPolicies::MatchesMovementCondition(const EWuwaMovementActionCondition Condition,
                                                           const EMovementMode SnapshotMovementMode,
                                                           const uint8 SnapshotCustomMovementMode,
                                                           const bool bCurrentlyGrounded,
                                                           const bool bCurrentlyFalling,
                                                           const bool bCurrentlyGrappling)
{
	const bool bSnapshotWasGrounded = SnapshotMovementMode == MOVE_Walking || SnapshotMovementMode == MOVE_NavWalking;
	const bool bSnapshotWasGrappling =
	    SnapshotMovementMode == MOVE_Custom &&
	    SnapshotCustomMovementMode == static_cast<uint8>(EWuwaCustomMovementMode::Grapple);

	switch (Condition)
	{
		case EWuwaMovementActionCondition::Grounded:
			return bSnapshotWasGrounded && bCurrentlyGrounded;

		case EWuwaMovementActionCondition::Falling:
			return SnapshotMovementMode == MOVE_Falling && bCurrentlyFalling;

		case EWuwaMovementActionCondition::Airborne:
			return (SnapshotMovementMode == MOVE_Falling || bSnapshotWasGrappling) &&
			       (bCurrentlyFalling || bCurrentlyGrappling);

		default:
			return false;
	}
}

bool FWuwaMovementActionPolicies::ResolveRootMotionTarget(const FVector& StartLocation,
                                                          const FVector& WorldDirection,
                                                          const float Distance,
                                                          FVector& OutTargetLocation)
{
	OutTargetLocation = StartLocation;
	const FVector HorizontalDirection = WorldDirection.GetSafeNormal2D();

	if (StartLocation.ContainsNaN() || WorldDirection.ContainsNaN() || HorizontalDirection.IsNearlyZero() ||
	    !FMath::IsFinite(Distance) || Distance <= 0.f)
	{
		return false;
	}

	OutTargetLocation = StartLocation + HorizontalDirection * Distance;
	return !OutTargetLocation.ContainsNaN();
}

bool FWuwaMovementActionPolicies::ResolveEventResponse(const UWuwaMovementActionDefinition& Definition,
                                                       const FGameplayTag& EventTag,
                                                       EWuwaMovementActionEventResponse& OutResponse)
{
	for (const FWuwaMovementActionEventBinding& Binding : Definition.EventBindings)
	{
		if (Binding.EventTag == EventTag)
		{
			OutResponse = Binding.Response;
			return true;
		}
	}

	return false;
}

bool FWuwaMovementActionPolicies::RequiresFacingOverride(const EWuwaActionFacingPolicy FacingPolicy)
{
	return FacingPolicy != EWuwaActionFacingPolicy::UseLocomotion;
}
