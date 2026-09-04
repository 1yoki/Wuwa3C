#include "Actions/Resolution/WuwaActionResolver.h"

#include "Actions/Data/WuwaActionDefinition.h"

EWuwaActionResolutionStatus FWuwaActionResolver::Resolve(const FWuwaInputCommand& Command,
                                                         const FWuwaActionResolutionSnapshot& Snapshot,
                                                         const UWuwaActionRuleSet* RuleSet,
                                                         FWuwaResolvedActionIntent& OutIntent)
{
	OutIntent = FWuwaResolvedActionIntent();

	if (!Command.IsValid() || !Snapshot.IsValid() || RuleSet == nullptr)
	{
		return EWuwaActionResolutionStatus::Invalid;
	}

	const FWuwaActionResolutionRule* BestRule = nullptr;
	bool bBestRuleIsAmbiguous = false;

	for (const FWuwaActionResolutionRule& Rule : RuleSet->Rules)
	{
		if (!Rule.IsRuntimeValid() || !Rule.Matches(Command, Snapshot))
		{
			continue;
		}

		if (BestRule == nullptr || Rule.RulePriority > BestRule->RulePriority)
		{
			BestRule = &Rule;
			bBestRuleIsAmbiguous = false;
		}
		else if (Rule.RulePriority == BestRule->RulePriority)
		{
			bBestRuleIsAmbiguous = true;
		}
	}

	if (BestRule == nullptr)
	{
		return EWuwaActionResolutionStatus::NoMatch;
	}

	if (bBestRuleIsAmbiguous)
	{
		return EWuwaActionResolutionStatus::Ambiguous;
	}

	const UWuwaActionDefinition* Definition = BestRule->Definition;
	OutIntent.Request.Header = Command.Header;
	OutIntent.Request.Definition = BestRule->Definition;
	OutIntent.Request.Context.InputDirection = Command.Direction.GetClampedToMaxSize(1.f);
	OutIntent.Request.Context.WorldDirection = ResolveWorldDirection(Command, Snapshot, BestRule->WorldDirectionPolicy);
	OutIntent.Request.Context.FacingDirection = Snapshot.FacingDirection.GetSafeNormal2D();
	OutIntent.Request.Context.MovementMode = Snapshot.MovementMode;
	OutIntent.Request.Context.CustomMovementMode = Snapshot.CustomMovementMode;
	OutIntent.Request.Context.SourceObject = Snapshot.SourceObject;
	OutIntent.ExpireAt =
	    FMath::Min(Command.GetExpireAt(), Command.PressedAt + static_cast<double>(Definition->BufferTime));
	OutIntent.ConsumedInputTags = BestRule->AdditionalConsumedInputTags;
	OutIntent.ConsumedInputTags.AddTag(Command.InputTag);

	return OutIntent.IsValid() ? EWuwaActionResolutionStatus::Resolved : EWuwaActionResolutionStatus::Invalid;
}

FVector FWuwaActionResolver::ResolveWorldDirection(const FWuwaInputCommand& Command,
                                                   const FWuwaActionResolutionSnapshot& Snapshot,
                                                   const EWuwaActionWorldDirectionPolicy Policy)
{
	FVector WorldDirection = FVector::ZeroVector;

	switch (Policy)
	{
		case EWuwaActionWorldDirectionPolicy::InputRelativeToView:
		{
			const FRotator HorizontalView(0.f, Snapshot.ViewRotation.Yaw, 0.f);
			WorldDirection = HorizontalView.RotateVector(FVector(Command.Direction.Y, Command.Direction.X, 0.f));
			break;
		}

		case EWuwaActionWorldDirectionPolicy::Facing:
			WorldDirection = Snapshot.FacingDirection;
			break;

		case EWuwaActionWorldDirectionPolicy::OppositeFacing:
			WorldDirection = -Snapshot.FacingDirection;
			break;

		default:
			break;
	}

	WorldDirection.Z = 0.f;
	return WorldDirection.GetSafeNormal();
}
