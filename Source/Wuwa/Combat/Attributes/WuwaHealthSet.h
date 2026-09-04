// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "WuwaHealthSet.generated.h"

/** 玩家生命属性集 */
UCLASS()
class WUWA_API UWuwaHealthSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	ATTRIBUTE_ACCESSORS_BASIC(UWuwaHealthSet, Health)
	ATTRIBUTE_ACCESSORS_BASIC(UWuwaHealthSet, MaxHealth)
	ATTRIBUTE_ACCESSORS_BASIC(UWuwaHealthSet, IncomingDamage)

	//~ Begin UObject Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UObject Interface

	//~ Begin UAttributeSet Interface
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	//~ End UAttributeSet Interface

private:
	/** 当前生命值 */
	UPROPERTY(BlueprintReadOnly,
	          ReplicatedUsing = OnRep_Health,
	          Category = "Wuwa|Combat|Health",
	          meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Health;

	/** 最大生命值 */
	UPROPERTY(BlueprintReadOnly,
	          ReplicatedUsing = OnRep_MaxHealth,
	          Category = "Wuwa|Combat|Health",
	          meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxHealth;

	/** 等待后续伤害结算提交消费的元属性 */
	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Combat|Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData IncomingDamage;

	/**
	 * 处理生命值复制通知
	 *
	 * @param OldValue	复制前的生命值
	 */
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	/**
	 * 处理最大生命值复制通知
	 *
	 * @param OldValue	复制前的最大生命值
	 */
	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
};
