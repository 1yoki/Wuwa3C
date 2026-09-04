// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "WuwaPoiseSet.generated.h"

/** 玩家韧性属性集 */
UCLASS()
class WUWA_API UWuwaPoiseSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	ATTRIBUTE_ACCESSORS_BASIC(UWuwaPoiseSet, Poise)
	ATTRIBUTE_ACCESSORS_BASIC(UWuwaPoiseSet, MaxPoise)
	ATTRIBUTE_ACCESSORS_BASIC(UWuwaPoiseSet, IncomingPoiseDamage)

	//~ Begin UObject Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UObject Interface

	//~ Begin UAttributeSet Interface
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	//~ End UAttributeSet Interface

private:
	/** 当前韧性值 */
	UPROPERTY(BlueprintReadOnly,
	          ReplicatedUsing = OnRep_Poise,
	          Category = "Wuwa|Combat|Poise",
	          meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Poise;

	/** 最大韧性值 */
	UPROPERTY(BlueprintReadOnly,
	          ReplicatedUsing = OnRep_MaxPoise,
	          Category = "Wuwa|Combat|Poise",
	          meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxPoise;

	/** 等待韧性结算提交消费的元属性 */
	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Combat|Poise", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData IncomingPoiseDamage;

	/**
	 * 处理韧性值复制通知
	 *
	 * @param OldValue	复制前的韧性值
	 * @return 无
	 */
	UFUNCTION()
	void OnRep_Poise(const FGameplayAttributeData& OldValue);

	/**
	 * 处理最大韧性值复制通知
	 *
	 * @param OldValue	复制前的最大韧性值
	 * @return 无
	 */
	UFUNCTION()
	void OnRep_MaxPoise(const FGameplayAttributeData& OldValue);
};
