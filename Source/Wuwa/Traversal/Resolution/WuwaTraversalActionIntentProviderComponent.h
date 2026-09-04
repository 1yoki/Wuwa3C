#pragma once

#include "CoreMinimal.h"
#include "Actions/Resolution/WuwaRuleDrivenIntentProviderComponent.h"
#include "Traversal/Contracts/WuwaTraversalTypes.h"
#include "WuwaTraversalActionIntentProviderComponent.generated.h"

/** 为 Traversal RuleSet 命中的 Grapple Definition 补充世界查询领域载荷的 Provider */
UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaTraversalActionIntentProviderComponent : public UWuwaRuleDrivenIntentProviderComponent
{
	GENERATED_BODY()

public:
	UWuwaTraversalActionIntentProviderComponent();

	/** @return 最近一次 Query 生成的只读调试快照 */
	const FWuwaGrappleQueryDebugSnapshot& GetDebugSnapshot() const
	{
		return DebugSnapshot;
	}

protected:
	//~ Begin UWuwaRuleDrivenIntentProviderComponent Interface
	virtual bool BuildDomainPayload(const UWuwaActionDefinition& Definition,
	                                const FWuwaInputCommand& Command,
	                                const FWuwaActionResolutionSnapshot& Snapshot,
	                                FWuwaActionContext& InOutContext,
	                                FInstancedStruct& OutDiagnostic) const override;
	//~ End UWuwaRuleDrivenIntentProviderComponent Interface

private:
	/** 最近一次 Query 及其最终预测弧线，供 Debug 可视化只读消费 */
	UPROPERTY(Transient)
	mutable FWuwaGrappleQueryDebugSnapshot DebugSnapshot;
};
