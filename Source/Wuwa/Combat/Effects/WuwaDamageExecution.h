// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayEffectExecutionCalculation.h"
#include "WuwaDamageExecution.generated.h"

/** 把基础伤害、攻击力和防御力结算到目标 IncomingDamage */
UCLASS()
class WUWA_API UWuwaDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	/** 注册伤害公式需要捕获的来源与目标属性 */
	UWuwaDamageExecution();

	//~ Begin UGameplayEffectExecutionCalculation Interface
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	                                    FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
	//~ End UGameplayEffectExecutionCalculation Interface
};
