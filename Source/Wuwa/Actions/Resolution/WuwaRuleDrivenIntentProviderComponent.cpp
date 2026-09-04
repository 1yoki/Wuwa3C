#include "Actions/Resolution/WuwaRuleDrivenIntentProviderComponent.h"

#include "Actions/Data/WuwaActionDefinition.h"
#include "Actions/Data/WuwaActionRuleSet.h"
#include "Actions/Resolution/WuwaActionRuleIntentProviderComponent.h"
#include "Actions/Resolution/WuwaActionResolver.h"
#include "Wuwa.h"

UWuwaRuleDrivenIntentProviderComponent::UWuwaRuleDrivenIntentProviderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UWuwaActionRuleIntentProviderComponent::UWuwaActionRuleIntentProviderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UWuwaRuleDrivenIntentProviderComponent::Initialize(UWuwaActionRuleSet* InRuleSet)
{
	if (!IsValid(InRuleSet))
	{
		RuleSet = nullptr;
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Rule Driven Intent Provider 初始化失败，RuleSet 无效。Owner=%s"),
		       *GetNameSafe(GetOwner()));
		return false;
	}

	RuleSet = InRuleSet;
	return true;
}

bool UWuwaRuleDrivenIntentProviderComponent::IsInitialized() const
{
	return IsValid(RuleSet);
}

void UWuwaRuleDrivenIntentProviderComponent::GatherHandledInputTags(FGameplayTagContainer& OutInputTags) const
{
	OutInputTags.Reset();

	if (IsValid(RuleSet))
	{
		RuleSet->GatherHandledInputTags(OutInputTags);
	}
}

FWuwaActionIntentResolution
UWuwaRuleDrivenIntentProviderComponent::ResolveIntent(const FWuwaInputCommand& Command,
                                                      const FWuwaActionResolutionSnapshot& Snapshot) const
{
	FWuwaActionIntentResolution Resolution;
	Resolution.Status = FWuwaActionResolver::Resolve(Command, Snapshot, RuleSet, Resolution.Intent);
	if (Resolution.Status != EWuwaActionResolutionStatus::Resolved)
	{
		return Resolution;
	}

	const UWuwaActionDefinition* Definition = Resolution.Intent.Request.Definition;
	FInstancedStruct Diagnostic;
	if (Definition == nullptr ||
	    !BuildDomainPayload(*Definition, Command, Snapshot, Resolution.Intent.Request.Context, Diagnostic))
	{
		Resolution.Intent = FWuwaResolvedActionIntent();
		Resolution.Status = EWuwaActionResolutionStatus::Rejected;
		Resolution.DiagnosticPayload = Diagnostic;
		return Resolution;
	}

	if (!Resolution.Intent.IsValid() || !Definition->IsContextValid(Resolution.Intent.Request.Context))
	{
		Resolution.Intent = FWuwaResolvedActionIntent();
		Resolution.Status = EWuwaActionResolutionStatus::Invalid;
		return Resolution;
	}

	return Resolution;
}

bool UWuwaRuleDrivenIntentProviderComponent::BuildDomainPayload(const UWuwaActionDefinition& Definition,
                                                                const FWuwaInputCommand& Command,
                                                                const FWuwaActionResolutionSnapshot& Snapshot,
                                                                FWuwaActionContext& InOutContext,
                                                                FInstancedStruct& OutDiagnostic) const
{
	(void)Definition;
	(void)Command;
	(void)Snapshot;
	(void)InOutContext;
	OutDiagnostic.Reset();
	return true;
}

void UWuwaRuleDrivenIntentProviderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RuleSet = nullptr;
	Super::EndPlay(EndPlayReason);
}
