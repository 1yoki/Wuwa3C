#pragma once

#include "CoreMinimal.h"
#include "Actions/Resolution/WuwaRuleDrivenIntentProviderComponent.h"
#include "WuwaActionRuleIntentProviderComponent.generated.h"

/** 承接现有 RuleSet Resolver 的 Action Intent Provider */
UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaActionRuleIntentProviderComponent : public UWuwaRuleDrivenIntentProviderComponent
{
	GENERATED_BODY()

public:
	UWuwaActionRuleIntentProviderComponent();
};
