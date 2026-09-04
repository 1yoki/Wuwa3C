#pragma once

#include "CoreMinimal.h"
#include "Actions/Contracts/WuwaActionTypes.h"
#include "Engine/DataAsset.h"
#include "Input/WuwaInputTypes.h"
#include "WuwaActionRuleSet.generated.h"

class UWuwaActionDefinition;

/** 规则对二维方向输入的要求 */
UENUM(BlueprintType)
enum class EWuwaActionDirectionCondition : uint8
{
	/** 不限制方向输入 */
	Any,

	/** 必须存在有效方向输入 */
	HasDirection,

	/** 必须没有有效方向输入 */
	NoDirection
};

/** 将输入边沿转换为世界方向的策略 */
UENUM(BlueprintType)
enum class EWuwaActionWorldDirectionPolicy : uint8
{
	/** 使用观察方向解释二维输入 */
	InputRelativeToView,

	/** 使用角色当前朝向 */
	Facing,

	/** 使用角色当前朝向的反方向 */
	OppositeFacing
};

/** 一条输入到 Action Definition 的纯数据映射 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaActionResolutionRule
{
	GENERATED_BODY()

	/** 需要匹配的输入语义 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule", meta = (Categories = "Input"))
	FGameplayTag InputTag;

	/** 需要匹配的输入边沿 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule")
	EWuwaInputCommandTrigger Trigger = EWuwaInputCommandTrigger::Pressed;

	/** 需要匹配的移动环境 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule")
	EWuwaMovementActionCondition MovementCondition = EWuwaMovementActionCondition::Grounded;

	/** 需要匹配的方向输入状态 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule")
	EWuwaActionDirectionCondition DirectionCondition = EWuwaActionDirectionCondition::Any;

	/** 冻结世界方向时使用的策略 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule")
	EWuwaActionWorldDirectionPolicy WorldDirectionPolicy = EWuwaActionWorldDirectionPolicy::InputRelativeToView;

	/** 同一输入同时命中多条规则时的解析优先级 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule", meta = (ClampMin = "0"))
	int32 RulePriority = 0;

	/** 命中后生成请求使用的 Definition */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule")
	TObjectPtr<UWuwaActionDefinition> Definition = nullptr;

	/** 除 InputTag 外需要阻止后续即时处理的输入语义 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule", meta = (Categories = "Input"))
	FGameplayTagContainer AdditionalConsumedInputTags;

	/**
     * 判断规则是否命中输入与快照
     * @param Command 输入边沿命令
     * @param Snapshot 当前只读角色快照
     * @return 是否命中
     */
	bool Matches(const FWuwaInputCommand& Command, const FWuwaActionResolutionSnapshot& Snapshot) const;

	/** @return 规则数据是否满足运行时不变量 */
	bool IsRuntimeValid() const;
};

/** 角色当前使用的输入解析规则集合，不承担可用性与权限判断 */
UCLASS(BlueprintType)
class WUWA_API UWuwaActionRuleSet : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 按语义条件匹配的全部解析规则 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action Resolution")
	TArray<FWuwaActionResolutionRule> Rules;

	/**
     * 收集规则声明的输入标签
     * @param OutInputTags 接收唯一输入标签
     */
	void GatherHandledInputTags(FGameplayTagContainer& OutInputTags) const;

	/** @return 规则集合是否满足运行时不变量 */
	bool IsRuntimeValid() const;

#if WITH_EDITOR
	/**
     * 执行 Editor Data Validation
     * @param Context 资产校验上下文
     * @return 资产校验结果
     */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
