#pragma once

#include "CoreMinimal.h"
#include "Actions/Contracts/WuwaActionIntentProvider.h"
#include "Components/ActorComponent.h"
#include "StructUtils/InstancedStruct.h"
#include "WuwaRuleDrivenIntentProviderComponent.generated.h"

class UWuwaActionDefinition;
class UWuwaActionRuleSet;

/** 用规则集合解析输入、可选补充领域载荷的 Intent Provider 基类 */
UCLASS(Abstract, ClassGroup = (Wuwa))
class WUWA_API UWuwaRuleDrivenIntentProviderComponent : public UActorComponent, public IWuwaActionIntentProvider
{
	GENERATED_BODY()

public:
	UWuwaRuleDrivenIntentProviderComponent();

	/**
     * 注入当前角色使用的规则集合
     * @param InRuleSet	输入解析规则集合
     * @return 是否完成初始化
     */
	bool Initialize(UWuwaActionRuleSet* InRuleSet);

	/** @return 是否持有有效 RuleSet */
	bool IsInitialized() const;

	//~ Begin IWuwaActionIntentProvider Interface
	virtual void GatherHandledInputTags(FGameplayTagContainer& OutInputTags) const override;
	virtual FWuwaActionIntentResolution ResolveIntent(const FWuwaInputCommand& Command,
	                                                  const FWuwaActionResolutionSnapshot& Snapshot) const override;
	//~ End IWuwaActionIntentProvider Interface

protected:
	//~ Begin UActorComponent Interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	/**
     * 为已选中的 Definition 补充或改写冻结执行上下文
     * @param Definition	规则命中的 Definition
     * @param Command	输入边沿命令
     * @param Snapshot	当前只读角色快照
     * @param InOutContext	Resolver 已填充的执行上下文，默认实现保持不变
     * @param OutDiagnostic	领域条件拒绝时接收诊断
     * @return 领域条件是否允许生成 Intent；返回 false 时 Provider 输出 Rejected
     */
	virtual bool BuildDomainPayload(const UWuwaActionDefinition& Definition,
	                                const FWuwaInputCommand& Command,
	                                const FWuwaActionResolutionSnapshot& Snapshot,
	                                FWuwaActionContext& InOutContext,
	                                FInstancedStruct& OutDiagnostic) const;

private:
	/** 当前角色使用的纯数据解析规则 */
	UPROPERTY(Transient)
	TObjectPtr<UWuwaActionRuleSet> RuleSet;
};
