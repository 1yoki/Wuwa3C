#include "Actions/Data/WuwaActionRuleSet.h"

#include "Actions/Data/WuwaActionDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
constexpr float DirectionThresholdSquared = 0.01f;

bool DirectionConditionsOverlap(const EWuwaActionDirectionCondition Left, const EWuwaActionDirectionCondition Right)
{
	return Left == Right || Left == EWuwaActionDirectionCondition::Any || Right == EWuwaActionDirectionCondition::Any;
}
}

bool FWuwaActionResolutionRule::Matches(const FWuwaInputCommand& Command,
                                        const FWuwaActionResolutionSnapshot& Snapshot) const
{
	if (Command.InputTag != InputTag || Command.Trigger != Trigger)
	{
		return false;
	}

	bool bMovementMatches = false;
	switch (MovementCondition)
	{
		case EWuwaMovementActionCondition::Grounded:
			bMovementMatches = Snapshot.bIsGrounded;
			break;

		case EWuwaMovementActionCondition::Falling:
			bMovementMatches = Snapshot.MovementMode == MOVE_Falling;
			break;

		case EWuwaMovementActionCondition::Airborne:
			bMovementMatches = Snapshot.bIsAirborne;
			break;

		default:
			break;
	}

	if (!bMovementMatches)
	{
		return false;
	}

	const bool bHasDirection = Command.Direction.SizeSquared() >= DirectionThresholdSquared;
	return DirectionCondition == EWuwaActionDirectionCondition::Any ||
	       (DirectionCondition == EWuwaActionDirectionCondition::HasDirection && bHasDirection) ||
	       (DirectionCondition == EWuwaActionDirectionCondition::NoDirection && !bHasDirection);
}

bool FWuwaActionResolutionRule::IsRuntimeValid() const
{
	return InputTag.IsValid() && RulePriority >= 0 && Definition != nullptr && Definition->IsRuntimeValid();
}

void UWuwaActionRuleSet::GatherHandledInputTags(FGameplayTagContainer& OutInputTags) const
{
	OutInputTags.Reset();

	for (const FWuwaActionResolutionRule& Rule : Rules)
	{
		if (Rule.InputTag.IsValid())
		{
			OutInputTags.AddTag(Rule.InputTag);
		}
	}
}

bool UWuwaActionRuleSet::IsRuntimeValid() const
{
	if (Rules.IsEmpty())
	{
		return false;
	}

	for (int32 RuleIndex = 0; RuleIndex < Rules.Num(); ++RuleIndex)
	{
		const FWuwaActionResolutionRule& Rule = Rules[RuleIndex];
		if (!Rule.IsRuntimeValid())
		{
			return false;
		}

		for (int32 OtherIndex = RuleIndex + 1; OtherIndex < Rules.Num(); ++OtherIndex)
		{
			const FWuwaActionResolutionRule& Other = Rules[OtherIndex];
			const bool bSameMatchKey = Rule.InputTag == Other.InputTag && Rule.Trigger == Other.Trigger &&
			                           Rule.MovementCondition == Other.MovementCondition &&
			                           DirectionConditionsOverlap(Rule.DirectionCondition, Other.DirectionCondition) &&
			                           Rule.RulePriority == Other.RulePriority;
			if (bSameMatchKey)
			{
				return false;
			}
		}
	}

	return true;
}

#if WITH_EDITOR

EDataValidationResult UWuwaActionRuleSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!IsRuntimeValid())
	{
		Context.AddError(FText::FromString(TEXT("Action RuleSet 存在无效或歧义规则")));
		return EDataValidationResult::Invalid;
	}

	return Result == EDataValidationResult::Invalid ? Result : EDataValidationResult::Valid;
}

#endif
