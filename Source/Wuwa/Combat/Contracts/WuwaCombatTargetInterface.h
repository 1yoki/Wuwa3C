// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Combat/Contracts/WuwaCombatFacts.h"
#include "UObject/Interface.h"
#include "WuwaCombatTargetInterface.generated.h"

/** 中性 Combat Target Contract 的反射类型 */
UINTERFACE(BlueprintType)
class WUWA_API UWuwaCombatTargetInterface : public UInterface
{
	GENERATED_BODY()
};

/** 不暴露 GAS 或 Health 实现的可受击资格契约 */
class WUWA_API IWuwaCombatTargetInterface
{
	GENERATED_BODY()

public:
	/**
	 * 评估当前目标是否接受中性 Combat Hit
	 *
	 * @param Query	服务端权威命中查询
	 * @return 目标资格和中性拒绝原因
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wuwa|Combat|Target")
	FWuwaCombatTargetResponse EvaluateCombatTarget(const FWuwaCombatTargetQuery& Query) const;
};
