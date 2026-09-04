// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Health/WuwaHealthComponent.h"

#include "AbilitySystem/Runtime/WuwaAbilitySystemComponent.h"
#include "Combat/Attributes/WuwaHealthSet.h"
#include "Combat/WuwaCombatLog.h"
#include "Core/WuwaGameplayTags.h"
#include "GameplayEffect.h"

UWuwaHealthComponent::UWuwaHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UWuwaHealthComponent::Initialize(UWuwaAbilitySystemComponent* InAbilitySystemComponent,
                                      const TSubclassOf<UGameplayEffect> InDeadEffectClass)
{
	if (AbilitySystemComponent.Get() == InAbilitySystemComponent && DeadEffectClass == InDeadEffectClass &&
	    HealthChangedDelegateHandle.IsValid() && DeadTagChangedDelegateHandle.IsValid())
	{
		return true;
	}

	Shutdown();
	const UGameplayEffect* DeadEffect =
	    IsValid(InDeadEffectClass) ? InDeadEffectClass->GetDefaultObject<UGameplayEffect>() : nullptr;
	if (!IsValid(InAbilitySystemComponent) ||
	    !InAbilitySystemComponent->HasAttributeSetForAttribute(UWuwaHealthSet::GetHealthAttribute()) ||
	    !IsValid(DeadEffect) || DeadEffect->DurationPolicy != EGameplayEffectDurationType::Infinite ||
	    !DeadEffect->GetGrantedTags().HasTagExact(WuwaGameplayTags::State_Combat_Dead))
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("HealthComponent 初始化失败：ASC、HealthSet 或 DeadEffect 无效。Owner=%s, ASC=%s, Effect=%s"),
		       *GetNameSafe(GetOwner()),
		       *GetNameSafe(InAbilitySystemComponent),
		       *GetNameSafe(DeadEffect));
		return false;
	}

	AbilitySystemComponent = InAbilitySystemComponent;
	DeadEffectClass = InDeadEffectClass;
	DeathState = EWuwaDeathState::NotDead;
	DeathSequence = 0;
	DeadEffectHandle.Invalidate();
	LastCurrentHealth = InAbilitySystemComponent->GetNumericAttribute(UWuwaHealthSet::GetHealthAttribute());
	LastPreviousHealth = LastCurrentHealth;
	HealthChangedDelegateHandle =
	    InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UWuwaHealthSet::GetHealthAttribute())
	        .AddUObject(this, &ThisClass::HandleHealthChanged);
	DeadTagChangedDelegateHandle =
	    InAbilitySystemComponent
	        ->RegisterGameplayTagEvent(WuwaGameplayTags::State_Combat_Dead, EGameplayTagEventType::NewOrRemoved)
	        .AddUObject(this, &ThisClass::HandleDeadTagChanged);

	if (!HealthChangedDelegateHandle.IsValid() || !DeadTagChangedDelegateHandle.IsValid())
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("HealthComponent 初始化失败：无法绑定属性或标签委托。Owner=%s"),
		       *GetNameSafe(GetOwner()));
		Shutdown();
		return false;
	}

	PublishHealthChangedFact(true);

	const int32 ExistingDeadTagCount = InAbilitySystemComponent->GetTagCount(WuwaGameplayTags::State_Combat_Dead);
	if (ExistingDeadTagCount > 0)
	{
		HandleDeadTagChanged(WuwaGameplayTags::State_Combat_Dead, ExistingDeadTagCount);
	}
	else if (GetOwner() != nullptr && GetOwner()->HasAuthority() && LastCurrentHealth <= 0.f)
	{
		TryStartAuthorityDeath();
	}

	return true;
}

void UWuwaHealthComponent::Shutdown()
{
	UWuwaAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (IsValid(ASC))
	{
		if (HealthChangedDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UWuwaHealthSet::GetHealthAttribute())
			    .Remove(HealthChangedDelegateHandle);
		}
		if (DeadTagChangedDelegateHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(WuwaGameplayTags::State_Combat_Dead, EGameplayTagEventType::NewOrRemoved)
			    .Remove(DeadTagChangedDelegateHandle);
		}
	}

	ResetBindingState();
}

bool UWuwaHealthComponent::BeginRespawn()
{
	if (DeathState != EWuwaDeathState::Dead)
	{
		UE_LOG(LogWuwaCombat,
		       Warning,
		       TEXT("HealthComponent 拒绝非法重生状态转换。Owner=%s, State=%d"),
		       *GetNameSafe(GetOwner()),
		       static_cast<int32>(DeathState));
		return false;
	}

	DeathState = EWuwaDeathState::Respawning;
	return true;
}

bool UWuwaHealthComponent::IsInitialized() const
{
	return AbilitySystemComponent.IsValid() && HealthChangedDelegateHandle.IsValid() &&
	       DeadTagChangedDelegateHandle.IsValid();
}

bool UWuwaHealthComponent::IsDeadOrDying() const
{
	return DeathState != EWuwaDeathState::NotDead;
}

EWuwaDeathState UWuwaHealthComponent::GetDeathState() const
{
	return DeathState;
}

FWuwaHealthRuntimeSnapshot UWuwaHealthComponent::GetRuntimeSnapshot() const
{
	FWuwaHealthRuntimeSnapshot Snapshot;
	const UWuwaAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	Snapshot.bInitialized = IsInitialized();
	Snapshot.bAuthority = GetOwner() != nullptr && GetOwner()->HasAuthority();
	Snapshot.DeathState = DeathState;
	Snapshot.Health = IsValid(ASC) ? ASC->GetNumericAttribute(UWuwaHealthSet::GetHealthAttribute()) : LastCurrentHealth;
	Snapshot.MaxHealth = IsValid(ASC) ? ASC->GetNumericAttribute(UWuwaHealthSet::GetMaxHealthAttribute()) : 0.f;
	Snapshot.PreviousHealth = LastPreviousHealth;
	Snapshot.LastDamage = FMath::Max(0.f, LastPreviousHealth - LastCurrentHealth);
	Snapshot.DeadTagCount = IsValid(ASC) ? ASC->GetTagCount(WuwaGameplayTags::State_Combat_Dead) : 0;
	Snapshot.DeathSequence = DeathSequence;
	Snapshot.bDeadEffectActive = DeadEffectHandle.IsValid();
	return Snapshot;
}

FWuwaHealthChangedFactDelegate& UWuwaHealthComponent::OnHealthChanged()
{
	return HealthChangedFactDelegate;
}

FWuwaDeathFactDelegate& UWuwaHealthComponent::OnDeathFact()
{
	return DeathFactDelegate;
}

void UWuwaHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Shutdown();
	Super::EndPlay(EndPlayReason);
}

void UWuwaHealthComponent::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	const bool bHasFiniteChange = FMath::IsFinite(ChangeData.OldValue) && FMath::IsFinite(ChangeData.NewValue) &&
	                              ChangeData.OldValue != ChangeData.NewValue;
	LastPreviousHealth = FMath::IsFinite(ChangeData.OldValue) ? ChangeData.OldValue : LastCurrentHealth;
	LastCurrentHealth = FMath::IsFinite(ChangeData.NewValue) ? ChangeData.NewValue : 0.f;
	if (bHasFiniteChange)
	{
		PublishHealthChangedFact(false);
	}
	if (LastCurrentHealth <= 0.f && DeathState == EWuwaDeathState::NotDead && GetOwner() != nullptr &&
	    GetOwner()->HasAuthority())
	{
		TryStartAuthorityDeath();
	}
	ReconcileReplicatedAliveState();
}

void UWuwaHealthComponent::HandleDeadTagChanged(const FGameplayTag Tag, const int32 NewCount)
{
	if (Tag != WuwaGameplayTags::State_Combat_Dead)
	{
		return;
	}
	if (NewCount <= 0)
	{
		ReconcileReplicatedAliveState();
		return;
	}
	if (DeathState != EWuwaDeathState::NotDead)
	{
		return;
	}

	DeathState = EWuwaDeathState::DeathStarted;
	PublishDeathFact();
	DeathState = EWuwaDeathState::Dead;
}

void UWuwaHealthComponent::ReconcileReplicatedAliveState()
{
	UWuwaAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	AActor* OwnerActor = GetOwner();
	if (!IsValid(ASC) || !IsValid(OwnerActor) || OwnerActor->HasAuthority() || ASC->GetAvatarActor() != OwnerActor ||
	    DeathState == EWuwaDeathState::NotDead)
	{
		return;
	}

	const float CurrentHealth = ASC->GetNumericAttribute(UWuwaHealthSet::GetHealthAttribute());
	if (!FMath::IsFinite(CurrentHealth) || CurrentHealth <= 0.f ||
	    ASC->GetTagCount(WuwaGameplayTags::State_Combat_Dead) > 0)
	{
		return;
	}

	DeathState = EWuwaDeathState::NotDead;
	DeathSequence = 0;
	DeadEffectHandle.Invalidate();
}

bool UWuwaHealthComponent::TryStartAuthorityDeath()
{
	if (DeathState != EWuwaDeathState::NotDead || GetOwner() == nullptr || !GetOwner()->HasAuthority() ||
	    !AbilitySystemComponent.IsValid())
	{
		return false;
	}

	DeathState = EWuwaDeathState::DeathStarted;
	if (!ApplyDeadEffect())
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("死亡状态效果应用失败，继续执行安全死亡清理。Owner=%s"),
		       *GetNameSafe(GetOwner()));
	}

	PublishDeathFact();
	DeathState = EWuwaDeathState::Dead;
	return true;
}

bool UWuwaHealthComponent::ApplyDeadEffect()
{
	UWuwaAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!IsValid(ASC) || !IsValid(DeadEffectClass))
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(GetOwner());
	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(DeadEffectClass, 1.f, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	DeadEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	return DeadEffectHandle.WasSuccessfullyApplied() && DeadEffectHandle.IsValid();
}

void UWuwaHealthComponent::PublishDeathFact()
{
	++DeathSequence;
	if (DeathSequence == 0)
	{
		++DeathSequence;
	}

	FWuwaDeathFact Fact;
	Fact.DeadActor = GetOwner();
	Fact.DeathSequence = DeathSequence;
	Fact.PreviousHealth = LastPreviousHealth;
	Fact.CurrentHealth = LastCurrentHealth;
	Fact.bAuthority = GetOwner() != nullptr && GetOwner()->HasAuthority();
	DeathFactDelegate.Broadcast(Fact);
}

void UWuwaHealthComponent::PublishHealthChangedFact(const bool bInitialSync)
{
	const UWuwaAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	const float MaxHealth = IsValid(ASC) ? ASC->GetNumericAttribute(UWuwaHealthSet::GetMaxHealthAttribute()) : 0.f;
	if (!FMath::IsFinite(LastPreviousHealth) || !FMath::IsFinite(LastCurrentHealth) || !FMath::IsFinite(MaxHealth))
	{
		UE_LOG(LogWuwaCombat,
		       Warning,
		       TEXT("HealthComponent 拒绝发布非法生命变化事实。Owner=%s, Previous=%f, Current=%f, Max=%f, Initial=%s"),
		       *GetNameSafe(GetOwner()),
		       LastPreviousHealth,
		       LastCurrentHealth,
		       MaxHealth,
		       bInitialSync ? TEXT("true") : TEXT("false"));
		return;
	}

	FWuwaHealthChangedFact Fact;
	Fact.AffectedActor = GetOwner();
	Fact.PreviousHealth = LastPreviousHealth;
	Fact.CurrentHealth = LastCurrentHealth;
	Fact.MaxHealth = MaxHealth;
	Fact.bInitialSync = bInitialSync;
	HealthChangedFactDelegate.Broadcast(Fact);
}

void UWuwaHealthComponent::ResetBindingState()
{
	AbilitySystemComponent.Reset();
	DeadEffectClass = nullptr;
	HealthChangedDelegateHandle.Reset();
	DeadTagChangedDelegateHandle.Reset();
	DeadEffectHandle.Invalidate();
}
