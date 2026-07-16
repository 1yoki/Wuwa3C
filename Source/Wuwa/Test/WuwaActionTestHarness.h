#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Actions/WuwaActionExecutor.h"
#include "Actions/WuwaActionSource.h"
#include "WuwaActionTestHarness.generated.h"

class UWuwaActionDefinition;

// 只为自动化测试提供可控 Source 和 Executor。
UCLASS(Transient)
class UWuwaActionTestHarness
    : public UObject,
      public IWuwaActionSource,
      public IWuwaActionExecutor
{
    GENERATED_BODY()

public:
    virtual bool BuildActionRequest(
        const FWuwaInputCommand &Command,
        FWuwaActionRequest &OutRequest) const override;

    virtual bool SupportsAction(
        const UWuwaActionDefinition &Definition) const override;

    virtual bool CanStartAction(
        const FWuwaActionRequest &Request,
        EWuwaActionRejectionReason &OutReason) const override;

    virtual bool StartAction(
        const FWuwaActionRequest &Request) override;

    virtual void EndAction(
        const FGameplayTag &ActionTag,
        EWuwaActionEndReason EndReason) override;

    // Source 只为这个输入标签构建请求。
    UPROPERTY(Transient)
    FGameplayTag SourceInputTag;

    UPROPERTY(Transient)
    TObjectPtr<UWuwaActionDefinition> SourceDefinition;

    // Executor 只支持显式加入的 Definition。
    UPROPERTY(Transient)
    TArray<TObjectPtr<UWuwaActionDefinition>> SupportedDefinitions;

    bool bBuildRequests = true;
    bool bCanStart = true;
    bool bStartSucceeds = true;

    EWuwaActionRejectionReason CanStartFailureReason =
        EWuwaActionRejectionReason::Cooldown;

    int32 StartCount = 0;
    int32 EndCount = 0;

    FGameplayTag LastStartedAction;
    FGameplayTag LastEndedAction;

    EWuwaActionEndReason LastEndReason =
        EWuwaActionEndReason::None;
};
