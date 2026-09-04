// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Contracts/WuwaAbilityTypes.h"
#include "WuwaGameplayAbility.generated.h"

class AWuwaCharacter;
class AWuwaPlayerState;
class UWuwaAbilitySystemComponent;
class UAnimMontage;
class UAbilityTask_WaitGameplayEvent;

/** Wuwa Gameplay Ability 的统一基类 */
UCLASS(Abstract, Blueprintable)
class WUWA_API UWuwaGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	/** 创建默认使用输入触发策略的 Ability */
	UWuwaGameplayAbility();

	/** @return 当前输入激活策略 */
	EWuwaAbilityActivationPolicy GetActivationPolicy() const
	{
		return ActivationPolicy;
	}

	/** @return 激活前必须全部存在的 Legacy StateTag */
	const FGameplayTagContainer& GetWuwaRequiredStateTags() const
	{
		return WuwaRequiredStateTags;
	}

	/** @return 存在任意一项就阻止激活的 Legacy StateTag */
	const FGameplayTagContainer& GetWuwaBlockedStateTags() const
	{
		return WuwaBlockedStateTags;
	}

	/** @return ActorInfo 中的 Wuwa Character */
	AWuwaCharacter* GetWuwaCharacterFromActorInfo() const;

	/** @return ActorInfo 中的 Wuwa PlayerState */
	AWuwaPlayerState* GetWuwaPlayerStateFromActorInfo() const;

	/** @return ActorInfo 中的 Wuwa ASC */
	UWuwaAbilitySystemComponent* GetWuwaAbilitySystemComponentFromActorInfo() const;

	/** 当前 Ability 是否阻止指定输入域 */
	bool BlocksInterruptSource(EWuwaAbilityInterruptSource Source) const;

	/** 当前状态下是否允许该输入打断 */
	bool CanInterruptFrom(EWuwaAbilityInterruptSource Source,
	                      const FGameplayTag& IncomingActionTag = FGameplayTag()) const;

	/** 尝试因为玩家输入取消当前 Ability */
	bool TryInterruptFrom(EWuwaAbilityInterruptSource Source, const FGameplayTag& IncomingActionTag = FGameplayTag());

	/**
	 * Montage Ability 激活后调用一次。
	 * 注册当前 Montage，并开始监听 InterruptWindow。
	 */
	bool BeginInterruptRuntime(UAnimMontage* Montage);

	//~ Begin UGameplayAbility Interface
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle,
	                                const FGameplayAbilityActorInfo* ActorInfo,
	                                const FGameplayTagContainer* SourceTags = nullptr,
	                                const FGameplayTagContainer* TargetTags = nullptr,
	                                FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	//~ End UGameplayAbility Interface

protected:
	/** 输入激活策略 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|AbilitySystem")
	EWuwaAbilityActivationPolicy ActivationPolicy = EWuwaAbilityActivationPolicy::OnInputTriggered;

	/** 激活前必须全部存在的 Legacy StateTag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|State")
	FGameplayTagContainer WuwaRequiredStateTags;

	/** 存在任意一项就阻止激活的 Legacy StateTag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|State")
	FGameplayTagContainer WuwaBlockedStateTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Interrupt")
	FWuwaAbilityInterruptPolicy InterruptPolicy;

	void EndAbility(FGameplayAbilitySpecHandle Handle,
	                const FGameplayAbilityActorInfo* ActorInfo,
	                FGameplayAbilityActivationInfo ActivationInfo,
	                bool bReplicateEndAbility,
	                bool bWasCancelled);

	void ResetInterruptWindowForTransition();

private:
	/** 当前动画是否已经进入玩家输入取消阶段 */
	bool bInterruptWindowOpen = false;

	/** 当前处于打开状态的 InterruptWindow Id */
	int32 ActiveInterruptWindowId = INDEX_NONE;

	/** 当前打断窗口对应的 Montage */
	TWeakObjectPtr<UAnimMontage> InterruptMontage;

	/** 当前 Ability 是否贡献了一份 Block.Input.Move */
	bool bOwnsInterruptMoveBlock = false;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> InterruptWindowBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> InterruptWindowEndTask;

	UFUNCTION()
	void HandleInterruptWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void HandleInterruptWindowEnd(FGameplayEventData Payload);

	void CleanupInterruptRuntime();
};
