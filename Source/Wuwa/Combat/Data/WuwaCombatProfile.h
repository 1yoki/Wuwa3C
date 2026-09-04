// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WuwaCombatProfile.generated.h"

class UWuwaAbilitySet;
class UWuwaMeleeAttackDefinition;
class UWuwaWeaponDefinition;
class UGameplayEffect;

/** 当前 Pawn 战斗系统的唯一数据装配入口 */
UCLASS(BlueprintType, Const)
class WUWA_API UWuwaCombatProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** @return Authority 授予的默认 AbilitySet */
	UWuwaAbilitySet* GetDefaultAbilitySet() const
	{
		return DefaultAbilitySet;
	}

	/** @return 当前默认武器定义 */
	UWuwaWeaponDefinition* GetDefaultWeaponDefinition() const
	{
		return DefaultWeaponDefinition;
	}

	/** @return 当前默认轻攻击定义 */
	UWuwaMeleeAttackDefinition* GetDefaultAttackDefinition() const
	{
		return DefaultAttackDefinition;
	}

	/** @return 当前 Pawn 使用的可追踪死亡效果 */
	TSubclassOf<UGameplayEffect> GetDeadEffectClass() const
	{
		return DeadEffectClass;
	}

	/** @return 三个必需定义与死亡效果是否全部有效 */
	bool IsRuntimeValid() const;

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
	//~ End UObject Interface
#endif

private:
	/** Authority 授予的默认 AbilitySet */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaAbilitySet> DefaultAbilitySet;

	/** 当前默认武器定义 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaWeaponDefinition> DefaultWeaponDefinition;

	/** 当前默认轻攻击定义 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaMeleeAttackDefinition> DefaultAttackDefinition;

	/** 当前 Pawn 使用的可追踪无限死亡效果 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Combat", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> DeadEffectClass;
};
