// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "WuwaPlayerState.generated.h"

class UWuwaAbilitySystemComponent;
class UWuwaCombatSet;
class UWuwaHealthSet;
class UWuwaPoiseSet;
class UWuwaResourceSet;

/** 持有玩家长期能力系统状态的 PlayerState */
UCLASS()
class WUWA_API AWuwaPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	/** 创建玩家能力系统基础对象 */
	AWuwaPlayerState();

	/** @return 当前 PlayerState 持有的能力系统组件 */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** @return 当前 PlayerState 持有的 Wuwa 能力系统组件 */
	UWuwaAbilitySystemComponent* GetWuwaAbilitySystemComponent() const;

	/** @return 当前 PlayerState 持有的生命属性集合 */
	const UWuwaHealthSet* GetHealthSet() const;

	/** @return 当前 PlayerState 持有的韧性属性集合 */
	const UWuwaPoiseSet* GetPoiseSet() const;

	/** @return 当前 PlayerState 持有的资源属性集合 */
	const UWuwaResourceSet* GetResourceSet() const;

	/** @return 当前 PlayerState 持有的战斗属性集合 */
	const UWuwaCombatSet* GetCombatSet() const;

private:
	/** 玩家长期持有并参与复制的能力系统组件 */
	UPROPERTY(VisibleAnywhere, Category = "Wuwa|AbilitySystem")
	TObjectPtr<UWuwaAbilitySystemComponent> AbilitySystemComponent;

	/** 玩家生命属性集合 */
	UPROPERTY()
	TObjectPtr<UWuwaHealthSet> HealthSet;

	/** 玩家韧性属性集合 */
	UPROPERTY()
	TObjectPtr<UWuwaPoiseSet> PoiseSet;

	/** 玩家战斗资源属性集合 */
	UPROPERTY()
	TObjectPtr<UWuwaResourceSet> ResourceSet;

	/** 玩家战斗数值属性集合 */
	UPROPERTY()
	TObjectPtr<UWuwaCombatSet> CombatSet;
};
