// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "WuwaCombatSet.generated.h"

/** 玩家战斗数值属性集 */
UCLASS()
class WUWA_API UWuwaCombatSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	ATTRIBUTE_ACCESSORS_BASIC(UWuwaCombatSet, AttackPower)
	ATTRIBUTE_ACCESSORS_BASIC(UWuwaCombatSet, Defense)

	//~ Begin UObject Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UObject Interface

	//~ Begin UAttributeSet Interface
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	//~ End UAttributeSet Interface

private:
	/** 攻击力 */
	UPROPERTY(BlueprintReadOnly,
	          ReplicatedUsing = OnRep_AttackPower,
	          Category = "Wuwa|Combat|Attributes",
	          meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData AttackPower;

	/** 防御力 */
	UPROPERTY(BlueprintReadOnly,
	          ReplicatedUsing = OnRep_Defense,
	          Category = "Wuwa|Combat|Attributes",
	          meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Defense;

	/**
	 * 处理攻击力复制通知
	 *
	 * @param OldValue	复制前的攻击力
	 */
	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldValue);

	/**
	 * 处理防御力复制通知
	 *
	 * @param OldValue	复制前的防御力
	 */
	UFUNCTION()
	void OnRep_Defense(const FGameplayAttributeData& OldValue);
};
