#include "Traversal/Resolution/WuwaTraversalActionIntentProviderComponent.h"

#include "Core/WuwaGameplayTags.h"
#include "Traversal/Data/WuwaGrappleActionDefinition.h"
#include "Traversal/Query/WuwaGrappleQuery.h"

UWuwaTraversalActionIntentProviderComponent::UWuwaTraversalActionIntentProviderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UWuwaTraversalActionIntentProviderComponent::BuildDomainPayload(const UWuwaActionDefinition& Definition,
                                                                     const FWuwaInputCommand& Command,
                                                                     const FWuwaActionResolutionSnapshot& Snapshot,
                                                                     FWuwaActionContext& InOutContext,
                                                                     FInstancedStruct& OutDiagnostic) const
{
	const UWuwaGrappleActionDefinition* GrappleDefinition = Cast<UWuwaGrappleActionDefinition>(&Definition);
	if (GrappleDefinition == nullptr)
	{
		FWuwaGrappleQueryResult Diagnostic;
		Diagnostic.FailureReason = EWuwaGrappleQueryFailureReason::InvalidDefinition;
		OutDiagnostic.InitializeAs<FWuwaGrappleQueryResult>(Diagnostic);
		return false;
	}

	FWuwaGrappleActionContext GrappleContext;
	FWuwaGrappleQueryResult QueryResult;
	TArray<FVector> PredictedPoints;
	const bool bQuerySucceeded = FWuwaGrappleQuery::Query(
	    Snapshot, GrappleDefinition, Command.Header.FrameNumber, GrappleContext, QueryResult, PredictedPoints);

	DebugSnapshot.bHasQuery = true;
	DebugSnapshot.QueryResult = QueryResult;
	DebugSnapshot.PredictedTrajectoryPoints = PredictedPoints;

	if (!bQuerySucceeded)
	{
		OutDiagnostic.InitializeAs<FWuwaGrappleQueryResult>(QueryResult);
		return false;
	}

	InOutContext.WorldDirection = GrappleContext.TravelDirection;
	InOutContext.DomainPayload.InitializeAs<FWuwaGrappleActionContext>(GrappleContext);
	return true;
}
