// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "WuwaDamageReceiverInterface.generated.h"

class UWuwaAbilitySystemComponent;
class UWuwaHealthComponent;
class UWuwaPoiseComponent;

/** 伤害应用链依赖的 GAS 接收者反射类型 */
UINTERFACE(MinimalAPI)
class UWuwaDamageReceiverInterface : public UInterface
{
	GENERATED_BODY()
};

/** 向伤害应用链暴露 ASC、生命与韧性门面的窄契约 */
class WUWA_API IWuwaDamageReceiverInterface
{
	GENERATED_BODY()

public:
	/** @return 当前受伤者的 Wuwa ASC */
	virtual UWuwaAbilitySystemComponent* GetDamageReceiverAbilitySystemComponent() const = 0;

	/** @return 当前受伤者的生命门面 */
	virtual UWuwaHealthComponent* GetDamageReceiverHealthComponent() const = 0;

	/** @return 当前受伤者的韧性门面 */
	virtual UWuwaPoiseComponent* GetDamageReceiverPoiseComponent() const = 0;
};
