#include "Test/WuwaActionTestHarness.h"

#include "Actions/WuwaActionDefinition.h"

bool UWuwaActionTestHarness::BuildActionRequest(
    const FWuwaInputCommand &Command,
    FWuwaActionRequest &OutRequest) const
{
    OutRequest = FWuwaActionRequest();

    if (!bBuildRequests ||
        !IsValid(SourceDefinition) ||
        Command.InputTag != SourceInputTag)
    {
        return false;
    }

    OutRequest.ActionTag = SourceDefinition->ActionTag;
    OutRequest.Definition = SourceDefinition;
    OutRequest.SourceInputSequence = Command.Sequence;

    // Context 使用命令产生时的方向快照。
    OutRequest.Context.InputDirection = Command.Direction;
    OutRequest.Context.FacingDirection = FVector::ForwardVector;
    OutRequest.Context.MovementMode = MOVE_Walking;
    OutRequest.Context.SourceInputSequence = Command.Sequence;

    return true;
}

bool UWuwaActionTestHarness::SupportsAction(
    const UWuwaActionDefinition &Definition) const
{
    for (const TObjectPtr<UWuwaActionDefinition> &SupportedDefinition : SupportedDefinitions)
    {
        if (SupportedDefinition.Get() == &Definition)
        {
            return true;
        }
    }

    return false;
}

bool UWuwaActionTestHarness::CanStartAction(
    const FWuwaActionRequest &Request,
    EWuwaActionRejectionReason &OutReason) const
{
    if (!bCanStart)
    {
        OutReason = CanStartFailureReason;
        return false;
    }

    OutReason = EWuwaActionRejectionReason::None;
    return true;
}

bool UWuwaActionTestHarness::StartAction(
    const FWuwaActionRequest &Request)
{
    ++StartCount;
    LastStartedAction = Request.ActionTag;

    return bStartSucceeds;
}

void UWuwaActionTestHarness::EndAction(
    const FGameplayTag &ActionTag,
    const EWuwaActionEndReason EndReason)
{
    ++EndCount;
    LastEndedAction = ActionTag;
    LastEndReason = EndReason;
}
