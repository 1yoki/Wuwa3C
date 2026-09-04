#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WuwaTraversalProfile.generated.h"

class UWuwaActionRuleSet;

/** 角色无锚点 Traversal 的只读装配配置 */
UCLASS(BlueprintType)
class WUWA_API UWuwaTraversalProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Traversal Provider 使用的输入解析规则集合 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Traversal")
	TObjectPtr<UWuwaActionRuleSet> TraversalRuleSet = nullptr;

	/** @return Profile 是否可以安全注入 Traversal Provider */
	bool IsRuntimeValid() const;

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
	//~ End UObject Interface
#endif
};
