// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "WuwaResourceSet.generated.h"

/** 玩家战斗资源属性集 */
UCLASS()
class WUWA_API UWuwaResourceSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	ATTRIBUTE_ACCESSORS_BASIC(UWuwaResourceSet, Stamina)
	ATTRIBUTE_ACCESSORS_BASIC(UWuwaResourceSet, MaxStamina)

	//~ Begin UObject Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UObject Interface

	//~ Begin UAttributeSet Interface
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	//~ End UAttributeSet Interface

private:
	/** 当前耐力值 */
	UPROPERTY(BlueprintReadOnly,
	          ReplicatedUsing = OnRep_Stamina,
	          Category = "Wuwa|Combat|Resource",
	          meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Stamina;

	/** 最大耐力值 */
	UPROPERTY(BlueprintReadOnly,
	          ReplicatedUsing = OnRep_MaxStamina,
	          Category = "Wuwa|Combat|Resource",
	          meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxStamina;

	/**
	 * 处理耐力值复制通知
	 *
	 * @param OldValue	复制前的耐力值
	 */
	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldValue);

	/**
	 * 处理最大耐力值复制通知
	 *
	 * @param OldValue	复制前的最大耐力值
	 */
	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);
};
