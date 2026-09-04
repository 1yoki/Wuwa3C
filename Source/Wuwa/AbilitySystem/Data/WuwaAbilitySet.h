// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "WuwaAbilitySet.generated.h"

class UGameplayEffect;
class UWuwaAbilitySystemComponent;
class UWuwaGameplayAbility;

/** AbilitySet 中的一项 Gameplay Ability 配置 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaAbilitySet_GameplayAbility
{
	GENERATED_BODY()

	/** 授予的 Wuwa Ability 类 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (AssetBundles = "Client,Server"))
	TSubclassOf<UWuwaGameplayAbility> Ability;

	/** Ability Spec 等级 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = "1"))
	int32 AbilityLevel = 1;

	/** 写入 AbilitySpec 动态标签的输入语义 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (Categories = "Input"))
	FGameplayTag InputTag;
};

/** AbilitySet 中的一项 Gameplay Effect 配置 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaAbilitySet_GameplayEffect
{
	GENERATED_BODY()

	/** 应用到 ASC 自身的 Effect 类 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect", meta = (AssetBundles = "Client,Server"))
	TSubclassOf<UGameplayEffect> GameplayEffect;

	/** Effect Spec 等级 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "0.01"))
	float EffectLevel = 1.f;
};

/** AbilitySet 授予后用于精确撤销的 Handle 集合 */
USTRUCT()
struct WUWA_API FWuwaAbilitySetGrantedHandles
{
	GENERATED_BODY()

public:
	/** 保存一个有效 AbilitySpec Handle */
	void AddAbilitySpecHandle(FGameplayAbilitySpecHandle Handle);

	/** 保存一个有效 Active GameplayEffect Handle */
	void AddGameplayEffectHandle(FActiveGameplayEffectHandle Handle);

	/** 记录一次成功执行且没有 Active Handle 的瞬时效果 */
	void AddInstantGameplayEffectApplication();

	/**
	 * 从指定 ASC 精确撤销全部保存 Handle
	 *
	 * @param AbilitySystemComponent	授予时使用的 ASC
	 */
	void TakeFromAbilitySystem(UWuwaAbilitySystemComponent* AbilitySystemComponent);

	/** 清空全部保存 Handle */
	void Reset();

	/** @return 是否没有保存任何授予 Handle */
	bool IsEmpty() const;

	/** @return 保存的 AbilitySpec Handle 数量 */
	int32 GetAbilityCount() const;

	/** @return 保存的 GameplayEffect Handle 数量 */
	/** @return 持续效果 Active Handle 与成功瞬时效果的总数 */
	int32 GetEffectCount() const;

	/** @return 保存的持续效果 Active Handle 数量 */
	int32 GetActiveEffectCount() const;

	/** @return 成功执行的瞬时效果数量 */
	int32 GetInstantEffectCount() const;

private:
	/** 本 AbilitySet 授予的 AbilitySpec Handle */
	UPROPERTY(Transient)
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	/** 本 AbilitySet 应用的 GameplayEffect Handle */
	UPROPERTY(Transient)
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;

	/** 本 AbilitySet 成功执行的瞬时效果数量 */
	UPROPERTY(Transient)
	int32 AppliedInstantGameplayEffectCount = 0;
};

/** 可由 Authority 一次性授予并按 Handle 撤销的 Ability/Effect 集合 */
UCLASS(BlueprintType, Const)
class WUWA_API UWuwaAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * 在 Authority ASC 上原子授予本集合
	 *
	 * @param AbilitySystemComponent	目标 Wuwa ASC
	 * @param OutGrantedHandles		接收精确撤销 Handle 的空集合
	 * @param SourceObject			AbilitySpec 和 EffectContext 的来源
	 * @return 是否完整授予
	 */
	bool GiveToAbilitySystem(UWuwaAbilitySystemComponent* AbilitySystemComponent,
	                         FWuwaAbilitySetGrantedHandles* OutGrantedHandles,
	                         UObject* SourceObject = nullptr) const;

	/**
	 * 验证任意 AbilitySet 条目集合
	 *
	 * @param AbilityEntries	待验证 Ability 条目
	 * @param EffectEntries	待验证 Effect 条目
	 * @param OutFailureReason	接收首个失败原因
	 * @return 所有条目是否满足运行时不变量
	 */
	static bool ValidateEntries(const TArray<FWuwaAbilitySet_GameplayAbility>& AbilityEntries,
	                            const TArray<FWuwaAbilitySet_GameplayEffect>& EffectEntries,
	                            FString& OutFailureReason);

	/** @return 当前 Ability 条目 */
	const TArray<FWuwaAbilitySet_GameplayAbility>& GetGrantedAbilities() const
	{
		return GrantedAbilities;
	}

	/** @return 当前 Effect 条目 */
	const TArray<FWuwaAbilitySet_GameplayEffect>& GetGrantedEffects() const
	{
		return GrantedEffects;
	}

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
	//~ End UObject Interface
#endif

private:
	/** Authority 初始化时授予的 Ability */
	UPROPERTY(EditDefaultsOnly, Category = "AbilitySet", meta = (TitleProperty = "Ability"))
	TArray<FWuwaAbilitySet_GameplayAbility> GrantedAbilities;

	/** Authority 初始化时应用的 Effect */
	UPROPERTY(EditDefaultsOnly, Category = "AbilitySet", meta = (TitleProperty = "GameplayEffect"))
	TArray<FWuwaAbilitySet_GameplayEffect> GrantedEffects;
};
