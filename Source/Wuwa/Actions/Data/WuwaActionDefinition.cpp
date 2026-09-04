#include "Actions/Data/WuwaActionDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

void UWuwaActionDefinition::GatherRequiredCapabilityTags(FGameplayTagContainer& OutCapabilityTags) const
{
	OutCapabilityTags.Reset();
}

const FWuwaActionAnimationSpec* UWuwaActionDefinition::GetAnimationSpec() const
{
	return nullptr;
}

float UWuwaActionDefinition::GetDesiredAnimationDuration() const
{
	return 0.f;
}

bool UWuwaActionDefinition::IsContextValid(const FWuwaActionContext& Context) const
{
	const uint8 MovementModeValue = Context.MovementMode.GetValue();
	return !Context.InputDirection.ContainsNaN() && !Context.WorldDirection.ContainsNaN() &&
	       !Context.FacingDirection.ContainsNaN() && Context.InputDirection.SizeSquared() <= 1.0001f &&
	       !Context.FacingDirection.IsNearlyZero() && MovementModeValue < MOVE_MAX;
}

bool UWuwaActionDefinition::ResolveCompletionRule(const FGameplayTag& EventTag,
                                                  EWuwaActionEndReason& OutEndReason) const
{
	OutEndReason = EWuwaActionEndReason::None;

	if (!EventTag.IsValid())
	{
		return false;
	}

	for (const FWuwaActionCompletionRule& Rule : CompletionRules)
	{
		if (Rule.EventTag == EventTag)
		{
			OutEndReason = Rule.EndReason;
			return OutEndReason != EWuwaActionEndReason::None;
		}
	}

	return false;
}

bool UWuwaActionDefinition::IsRuntimeValid() const
{
	if (!ActionTag.IsValid() || Priority < 0 || !FMath::IsFinite(BufferTime) || BufferTime < 0.f ||
	    !FMath::IsFinite(CooldownDuration) || CooldownDuration < 0.f || RequiredTags.HasAny(BlockedTags))
	{
		return false;
	}

	FGameplayTagContainer SeenCompletionEvents;

	for (const FWuwaActionCompletionRule& Rule : CompletionRules)
	{
		if (!Rule.EventTag.IsValid() || Rule.EndReason == EWuwaActionEndReason::None ||
		    SeenCompletionEvents.HasTagExact(Rule.EventTag))
		{
			return false;
		}

		SeenCompletionEvents.AddTag(Rule.EventTag);
	}

	return true;
}

#if WITH_EDITOR

EDataValidationResult UWuwaActionDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (Result != EDataValidationResult::Invalid)
	{
		Result = EDataValidationResult::Valid;
	}

	if (!IsRuntimeValid())
	{
		Context.AddError(FText::FromString(TEXT("Action Definition 的通用生命周期配置无效")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}

#endif
