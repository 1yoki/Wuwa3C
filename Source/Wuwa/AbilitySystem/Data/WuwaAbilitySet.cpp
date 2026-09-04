// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Data/WuwaAbilitySet.h"

#include "AbilitySystem/Abilities/WuwaGameplayAbility.h"
#include "AbilitySystem/Runtime/WuwaAbilitySystemComponent.h"
#include "AbilitySystem/WuwaAbilitySystemLog.h"
#include "GameplayEffect.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

void FWuwaAbilitySetGrantedHandles::AddAbilitySpecHandle(const FGameplayAbilitySpecHandle Handle)
{
	if (Handle.IsValid())
	{
		AbilitySpecHandles.AddUnique(Handle);
	}
}

void FWuwaAbilitySetGrantedHandles::AddGameplayEffectHandle(const FActiveGameplayEffectHandle Handle)
{
	if (Handle.IsValid())
	{
		GameplayEffectHandles.AddUnique(Handle);
	}
}

void FWuwaAbilitySetGrantedHandles::AddInstantGameplayEffectApplication()
{
	++AppliedInstantGameplayEffectCount;
}

void FWuwaAbilitySetGrantedHandles::TakeFromAbilitySystem(UWuwaAbilitySystemComponent* AbilitySystemComponent)
{
	if (!IsValid(AbilitySystemComponent))
	{
		UE_LOG(LogWuwaAbility,
		       Warning,
		       TEXT("AbilitySet 撤销失败：ASC 无效。AbilityCount=%d, ActiveEffectCount=%d, InstantEffectCount=%d"),
		       AbilitySpecHandles.Num(),
		       GameplayEffectHandles.Num(),
		       AppliedInstantGameplayEffectCount);
		Reset();
		return;
	}

	if (!AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		UE_LOG(LogWuwaAbility,
		       Warning,
		       TEXT("AbilitySet 撤销被拒绝：调用方没有 Authority。ASC=%s"),
		       *GetNameSafe(AbilitySystemComponent));
		return;
	}

	for (const FGameplayAbilitySpecHandle Handle : AbilitySpecHandles)
	{
		if (Handle.IsValid())
		{
			AbilitySystemComponent->ClearAbility(Handle);
		}
	}

	for (const FActiveGameplayEffectHandle Handle : GameplayEffectHandles)
	{
		if (Handle.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(Handle);
		}
	}

	Reset();
}

void FWuwaAbilitySetGrantedHandles::Reset()
{
	AbilitySpecHandles.Reset();
	GameplayEffectHandles.Reset();
	AppliedInstantGameplayEffectCount = 0;
}

bool FWuwaAbilitySetGrantedHandles::IsEmpty() const
{
	return AbilitySpecHandles.IsEmpty() && GameplayEffectHandles.IsEmpty() && AppliedInstantGameplayEffectCount == 0;
}

int32 FWuwaAbilitySetGrantedHandles::GetAbilityCount() const
{
	return AbilitySpecHandles.Num();
}

int32 FWuwaAbilitySetGrantedHandles::GetEffectCount() const
{
	return GameplayEffectHandles.Num() + AppliedInstantGameplayEffectCount;
}

int32 FWuwaAbilitySetGrantedHandles::GetActiveEffectCount() const
{
	return GameplayEffectHandles.Num();
}

int32 FWuwaAbilitySetGrantedHandles::GetInstantEffectCount() const
{
	return AppliedInstantGameplayEffectCount;
}

bool UWuwaAbilitySet::GiveToAbilitySystem(UWuwaAbilitySystemComponent* AbilitySystemComponent,
                                          FWuwaAbilitySetGrantedHandles* OutGrantedHandles,
                                          UObject* SourceObject) const
{
	FString FailureReason;
	if (!ValidateEntries(GrantedAbilities, GrantedEffects, FailureReason))
	{
		UE_LOG(LogWuwaAbility,
		       Error,
		       TEXT("AbilitySet 授予失败：配置无效。Asset=%s, Reason=%s"),
		       *GetNameSafe(this),
		       *FailureReason);
		return false;
	}

	if (!IsValid(AbilitySystemComponent) || OutGrantedHandles == nullptr || !OutGrantedHandles->IsEmpty())
	{
		UE_LOG(LogWuwaAbility,
		       Error,
		       TEXT("AbilitySet 授予失败：ASC、输出 Handle 或初始状态无效。Asset=%s, ASC=%s"),
		       *GetNameSafe(this),
		       *GetNameSafe(AbilitySystemComponent));
		return false;
	}

	if (!AbilitySystemComponent->IsOwnerActorAuthoritative() || !IsValid(AbilitySystemComponent->GetOwnerActor()) ||
	    !IsValid(AbilitySystemComponent->GetAvatarActor()))
	{
		UE_LOG(LogWuwaAbility,
		       Error,
		       TEXT("AbilitySet 授予失败：需要 Authority 和完整 ActorInfo。Asset=%s, ASC=%s"),
		       *GetNameSafe(this),
		       *GetNameSafe(AbilitySystemComponent));
		return false;
	}

	for (const FWuwaAbilitySet_GameplayAbility& Entry : GrantedAbilities)
	{
		FGameplayAbilitySpec AbilitySpec(Entry.Ability, Entry.AbilityLevel, INDEX_NONE, SourceObject);
		if (Entry.InputTag.IsValid())
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(Entry.InputTag);
		}
		const FGameplayAbilitySpecHandle Handle = AbilitySystemComponent->GiveAbility(AbilitySpec);
		if (!Handle.IsValid())
		{
			UE_LOG(LogWuwaAbility,
			       Error,
			       TEXT("AbilitySet 授予失败：GiveAbility 未返回有效 Handle。Asset=%s, Ability=%s"),
			       *GetNameSafe(this),
			       *GetNameSafe(Entry.Ability.Get()));
			OutGrantedHandles->TakeFromAbilitySystem(AbilitySystemComponent);
			return false;
		}
		OutGrantedHandles->AddAbilitySpecHandle(Handle);
	}

	for (const FWuwaAbilitySet_GameplayEffect& Entry : GrantedEffects)
	{
		const UGameplayEffect* EffectCDO = Entry.GameplayEffect->GetDefaultObject<UGameplayEffect>();
		if (IsValid(EffectCDO) && EffectCDO->DurationPolicy == EGameplayEffectDurationType::Instant)
		{
			continue;
		}

		FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
		EffectContext.AddSourceObject(SourceObject);
		const FGameplayEffectSpecHandle EffectSpec =
		    AbilitySystemComponent->MakeOutgoingSpec(Entry.GameplayEffect, Entry.EffectLevel, EffectContext);
		const FActiveGameplayEffectHandle Handle =
		    EffectSpec.IsValid() ? AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get())
		                         : FActiveGameplayEffectHandle();
		if (!Handle.WasSuccessfullyApplied() || !Handle.IsValid())
		{
			UE_LOG(LogWuwaAbility,
			       Error,
			       TEXT("AbilitySet 授予失败：持续效果未返回有效 Active Handle。Asset=%s, Effect=%s"),
			       *GetNameSafe(this),
			       *GetNameSafe(Entry.GameplayEffect.Get()));
			OutGrantedHandles->TakeFromAbilitySystem(AbilitySystemComponent);
			return false;
		}
		OutGrantedHandles->AddGameplayEffectHandle(Handle);
	}

	for (const FWuwaAbilitySet_GameplayEffect& Entry : GrantedEffects)
	{
		const UGameplayEffect* EffectCDO = Entry.GameplayEffect->GetDefaultObject<UGameplayEffect>();
		if (!IsValid(EffectCDO) || EffectCDO->DurationPolicy != EGameplayEffectDurationType::Instant)
		{
			continue;
		}

		FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
		EffectContext.AddSourceObject(SourceObject);
		const FGameplayEffectSpecHandle EffectSpec =
		    AbilitySystemComponent->MakeOutgoingSpec(Entry.GameplayEffect, Entry.EffectLevel, EffectContext);
		const FActiveGameplayEffectHandle Handle =
		    EffectSpec.IsValid() ? AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get())
		                         : FActiveGameplayEffectHandle();
		if (!Handle.WasSuccessfullyApplied())
		{
			UE_LOG(LogWuwaAbility,
			       Error,
			       TEXT("AbilitySet 授予失败：瞬时效果应用失败。Asset=%s, Effect=%s"),
			       *GetNameSafe(this),
			       *GetNameSafe(Entry.GameplayEffect.Get()));
			OutGrantedHandles->TakeFromAbilitySystem(AbilitySystemComponent);
			return false;
		}
		if (Handle.IsValid())
		{
			OutGrantedHandles->AddGameplayEffectHandle(Handle);
		}
		else
		{
			OutGrantedHandles->AddInstantGameplayEffectApplication();
		}
	}

	return true;
}

bool UWuwaAbilitySet::ValidateEntries(const TArray<FWuwaAbilitySet_GameplayAbility>& AbilityEntries,
                                      const TArray<FWuwaAbilitySet_GameplayEffect>& EffectEntries,
                                      FString& OutFailureReason)
{
	OutFailureReason.Reset();
	TSet<UClass*> SeenAbilityClasses;
	FGameplayTagContainer SeenInputTags;
	const FGameplayTag InputRootTag = FGameplayTag::RequestGameplayTag(TEXT("Input"), false);

	for (const FWuwaAbilitySet_GameplayAbility& Entry : AbilityEntries)
	{
		UClass* AbilityClass = Entry.Ability.Get();
		if (!IsValid(AbilityClass) || !AbilityClass->IsChildOf(UWuwaGameplayAbility::StaticClass()))
		{
			OutFailureReason = TEXT("Ability Class 为空或不是 UWuwaGameplayAbility 子类");
			return false;
		}
		if (Entry.AbilityLevel <= 0)
		{
			OutFailureReason = TEXT("AbilityLevel 必须大于零");
			return false;
		}
		if (Entry.InputTag.IsValid() && (!InputRootTag.IsValid() || !Entry.InputTag.MatchesTag(InputRootTag)))
		{
			OutFailureReason = TEXT("有效 InputTag 必须位于 Input 根下");
			return false;
		}
		if (Entry.InputTag.IsValid() && SeenInputTags.HasTagExact(Entry.InputTag))
		{
			OutFailureReason = TEXT("同一 AbilitySet 中 InputTag 不得重复");
			return false;
		}
		if (SeenAbilityClasses.Contains(AbilityClass))
		{
			OutFailureReason = TEXT("同一 AbilitySet 中 Ability Class 不得重复");
			return false;
		}

		if (Entry.InputTag.IsValid())
		{
			SeenInputTags.AddTag(Entry.InputTag);
		}
		SeenAbilityClasses.Add(AbilityClass);
	}

	int32 InstantEffectCount = 0;
	for (const FWuwaAbilitySet_GameplayEffect& Entry : EffectEntries)
	{
		if (!IsValid(Entry.GameplayEffect.Get()))
		{
			OutFailureReason = TEXT("GameplayEffect Class 不能为空");
			return false;
		}
		if (!FMath::IsFinite(Entry.EffectLevel) || Entry.EffectLevel <= 0.f)
		{
			OutFailureReason = TEXT("EffectLevel 必须是大于零的有限值");
			return false;
		}

		const UGameplayEffect* EffectCDO = Entry.GameplayEffect->GetDefaultObject<UGameplayEffect>();
		if (!IsValid(EffectCDO))
		{
			OutFailureReason = TEXT("GameplayEffect CDO 无效");
			return false;
		}

		if (EffectCDO->DurationPolicy == EGameplayEffectDurationType::Instant && ++InstantEffectCount > 1)
		{
			OutFailureReason = TEXT("第一阶段每个 AbilitySet 最多允许一个瞬时 GameplayEffect");
			return false;
		}
	}

	return true;
}

#if WITH_EDITOR

EDataValidationResult UWuwaAbilitySet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	FString FailureReason;
	if (!ValidateEntries(GrantedAbilities, GrantedEffects, FailureReason))
	{
		Context.AddError(FText::FromString(FailureReason));
		return EDataValidationResult::Invalid;
	}

	return Result == EDataValidationResult::Invalid ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}

#endif
