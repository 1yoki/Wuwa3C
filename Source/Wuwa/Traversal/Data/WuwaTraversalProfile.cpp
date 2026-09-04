#include "Traversal/Data/WuwaTraversalProfile.h"

#include "Actions/Data/WuwaActionRuleSet.h"
#include "Traversal/Data/WuwaGrappleActionDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UWuwaTraversalProfile::IsRuntimeValid() const
{
	if (!IsValid(TraversalRuleSet) || !TraversalRuleSet->IsRuntimeValid())
	{
		return false;
	}

	for (const FWuwaActionResolutionRule& Rule : TraversalRuleSet->Rules)
	{
		if (!IsValid(Cast<UWuwaGrappleActionDefinition>(Rule.Definition)))
		{
			return false;
		}
	}

	return true;
}

#if WITH_EDITOR

EDataValidationResult UWuwaTraversalProfile::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!IsRuntimeValid())
	{
		Context.AddError(FText::FromString(TEXT("Traversal Profile 必须使用只包含 Grapple Definition 的有效 RuleSet")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}

#endif
