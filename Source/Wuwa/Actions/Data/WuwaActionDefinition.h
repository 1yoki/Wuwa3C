#pragma once

#include "CoreMinimal.h"
#include "Actions/Contracts/WuwaActionPresentationTypes.h"
#include "Actions/Contracts/WuwaActionTypes.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WuwaActionDefinition.generated.h"

/** 所有独占 Action 共享的生命周期与准入配置 */
UCLASS(Abstract, BlueprintType)
class WUWA_API UWuwaActionDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 唯一标识 Action 的语义标签 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action", meta = (Categories = "Action"))
	FGameplayTag ActionTag;

	/** 只参与当前 Action 替换仲裁的优先级 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action", meta = (ClampMin = "0"))
	int32 Priority = 0;

	/** 开始前必须全部存在的状态标签 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Tags")
	FGameplayTagContainer RequiredTags;

	/** 存在任意一项就拒绝开始的状态标签 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Tags")
	FGameplayTagContainer BlockedTags;

	/** Action 活动期间由 Coordinator 取得的状态标签 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Tags")
	FGameplayTagContainer GrantedTags;

	/** 暂时无法开始时允许保留在 FIFO 的时长 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Buffer", meta = (ClampMin = "0.0", Units = "s"))
	float BufferTime = 0.f;

	/** 成功启动后同 ActionTag 的冷却时长 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Cooldown", meta = (ClampMin = "0.0", Units = "s"))
	float CooldownDuration = 0.f;

	/** 本 Action 允许取消的当前 ActionTag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Cancel", meta = (Categories = "Action"))
	FGameplayTagContainer CanCancelActions;

	/** 允许取消本 Action 的新 ActionTag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Cancel", meta = (Categories = "Action"))
	FGameplayTagContainer CanBeCancelledBy;

	/** 事件到统一结束原因的通用映射 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Completion")
	TArray<FWuwaActionCompletionRule> CompletionRules;

	/**
     * 收集该 Definition 启动所需的 CapabilityTag
     * @param OutCapabilityTags	接收必需能力标签
     */
	virtual void GatherRequiredCapabilityTags(FGameplayTagContainer& OutCapabilityTags) const;

	/** @return 可选的 Montage 表现配置，空指针表示不需要 Animation Capability */
	virtual const FWuwaActionAnimationSpec* GetAnimationSpec() const;

	/** @return 大于零时要求 Animation Capability 匹配该时长 */
	virtual float GetDesiredAnimationDuration() const;

	/**
     * 验证请求携带的通用与领域上下文
     * @param Context	冻结的 Action 上下文
     * @return 上下文是否满足当前 Definition 的运行时不变量
     */
	virtual bool IsContextValid(const FWuwaActionContext& Context) const;

	/**
     * 查询事件是否要求结束 Action
     * @param EventTag	已经发生的 Action Event
     * @param OutEndReason	接收结束原因
     * @return 是否命中唯一完成规则
     */
	bool ResolveCompletionRule(const FGameplayTag& EventTag, EWuwaActionEndReason& OutEndReason) const;

	/** @return 通用配置是否满足运行时不变量 */
	virtual bool IsRuntimeValid() const;

#if WITH_EDITOR
	/**
     * 执行 Editor Data Validation
     * @param Context	资产校验上下文
     * @return 资产校验结果
     */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
