// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Poise/WuwaPoiseComponent.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/Runtime/WuwaAbilitySystemComponent.h"
#include "Combat/Attributes/WuwaHealthSet.h"
#include "Combat/Attributes/WuwaPoiseSet.h"
#include "Combat/WuwaCombatLog.h"
#include "Core/WuwaGameplayTags.h"
#include "Engine/World.h"
#include "TimerManager.h"

UWuwaPoiseComponent::UWuwaPoiseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

bool UWuwaPoiseComponent::Initialize(UWuwaAbilitySystemComponent* InAbilitySystemComponent)
{
	if (AbilitySystemComponent.Get() == InAbilitySystemComponent && PoiseChangedDelegateHandle.IsValid() &&
	    IsValid(InAbilitySystemComponent) && InAbilitySystemComponent->GetAvatarActor() == GetOwner())
	{
		return true;
	}

	Shutdown();
	if (!IsValid(InAbilitySystemComponent) ||
	    !InAbilitySystemComponent->HasAttributeSetForAttribute(UWuwaPoiseSet::GetPoiseAttribute()) ||
	    InAbilitySystemComponent->GetAvatarActor() != GetOwner())
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("PoiseComponent 初始化失败：ASC、PoiseSet 或 Avatar 归属无效。Owner=%s, ASC=%s, Avatar=%s"),
		       *GetNameSafe(GetOwner()),
		       *GetNameSafe(InAbilitySystemComponent),
		       *GetNameSafe(IsValid(InAbilitySystemComponent) ? InAbilitySystemComponent->GetAvatarActor() : nullptr));
		return false;
	}

	AbilitySystemComponent = InAbilitySystemComponent;
	BreakSequence = 0;
	PendingBreakSequence = 0;
	bPendingBreak = false;
	LastCurrentPoise = InAbilitySystemComponent->GetNumericAttribute(UWuwaPoiseSet::GetPoiseAttribute());
	LastPreviousPoise = LastCurrentPoise;
	LastPoiseDamage = 0.f;
	PoiseChangedDelegateHandle =
	    InAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UWuwaPoiseSet::GetPoiseAttribute())
	        .AddUObject(this, &ThisClass::HandlePoiseChanged);
	if (!PoiseChangedDelegateHandle.IsValid())
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("PoiseComponent 初始化失败：无法绑定 Poise 属性委托。Owner=%s"),
		       *GetNameSafe(GetOwner()));
		Shutdown();
		return false;
	}

	return true;
}

void UWuwaPoiseComponent::Shutdown()
{
	CancelPendingBreak();
	UWuwaAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (IsValid(ASC) && PoiseChangedDelegateHandle.IsValid())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UWuwaPoiseSet::GetPoiseAttribute())
		    .Remove(PoiseChangedDelegateHandle);
	}

	ResetBindingState();
}

bool UWuwaPoiseComponent::IsInitialized() const
{
	const UWuwaAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	return IsValid(ASC) && PoiseChangedDelegateHandle.IsValid() && ASC->GetAvatarActor() == GetOwner();
}

FWuwaPoiseRuntimeSnapshot UWuwaPoiseComponent::GetRuntimeSnapshot() const
{
	FWuwaPoiseRuntimeSnapshot Snapshot;
	const UWuwaAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	Snapshot.bInitialized = IsInitialized();
	Snapshot.bAuthority = GetOwner() != nullptr && GetOwner()->HasAuthority();
	Snapshot.Poise = IsValid(ASC) ? ASC->GetNumericAttribute(UWuwaPoiseSet::GetPoiseAttribute()) : LastCurrentPoise;
	Snapshot.MaxPoise = IsValid(ASC) ? ASC->GetNumericAttribute(UWuwaPoiseSet::GetMaxPoiseAttribute()) : 0.f;
	Snapshot.IncomingPoiseDamage =
	    IsValid(ASC) ? ASC->GetNumericAttribute(UWuwaPoiseSet::GetIncomingPoiseDamageAttribute()) : 0.f;
	Snapshot.PreviousPoise = LastPreviousPoise;
	Snapshot.LastPoiseDamage = LastPoiseDamage;
	Snapshot.bPendingBreak = bPendingBreak;
	Snapshot.BreakSequence = BreakSequence;
	return Snapshot;
}

FWuwaPoiseBreakFactDelegate& UWuwaPoiseComponent::OnPoiseBreak()
{
	return PoiseBreakFactDelegate;
}

void UWuwaPoiseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Shutdown();
	Super::EndPlay(EndPlayReason);
}

void UWuwaPoiseComponent::HandlePoiseChanged(const FOnAttributeChangeData& ChangeData)
{
	const float PreviousPoise = FMath::IsFinite(ChangeData.OldValue) ? ChangeData.OldValue : LastCurrentPoise;
	const float CurrentPoise = FMath::IsFinite(ChangeData.NewValue) ? ChangeData.NewValue : 0.f;
	if (bPendingBreak && PreviousPoise <= 0.f && CurrentPoise <= 0.f)
	{
		return;
	}

	LastPreviousPoise = PreviousPoise;
	LastCurrentPoise = CurrentPoise;
	LastPoiseDamage = FMath::Max(0.f, LastPreviousPoise - LastCurrentPoise);

	if (LastCurrentPoise > 0.f)
	{
		CancelPendingBreak();
		return;
	}

	if (LastPreviousPoise > 0.f && LastCurrentPoise <= 0.f && GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		ScheduleAuthorityBreakReview();
	}
}

void UWuwaPoiseComponent::ScheduleAuthorityBreakReview()
{
	CancelPendingBreak();
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("PoiseComponent 无法调度破韧复核：World 无效。Owner=%s"),
		       *GetNameSafe(GetOwner()));
		return;
	}

	PendingBreakSequence = BreakSequence >= MAX_int32 ? 1 : BreakSequence + 1;
	bPendingBreak = true;
	PendingBreakTimerHandle = World->GetTimerManager().SetTimerForNextTick(
	    FTimerDelegate::CreateUObject(this, &ThisClass::ReviewPendingBreak, PendingBreakSequence));
}

void UWuwaPoiseComponent::ReviewPendingBreak(const int32 ExpectedSequence)
{
	PendingBreakTimerHandle.Invalidate();
	UWuwaAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	const AActor* Owner = GetOwner();
	const bool bValidReview = bPendingBreak && PendingBreakSequence == ExpectedSequence && IsValid(ASC) &&
	                          IsValid(Owner) && Owner->HasAuthority() && ASC->GetAvatarActor() == Owner &&
	                          ASC->GetNumericAttribute(UWuwaHealthSet::GetHealthAttribute()) > 0.f &&
	                          ASC->GetNumericAttribute(UWuwaPoiseSet::GetPoiseAttribute()) <= 0.f &&
	                          !ASC->HasMatchingGameplayTag(WuwaGameplayTags::State_Combat_Dead) &&
	                          !ASC->HasMatchingGameplayTag(WuwaGameplayTags::State_Combat_Staggered);
	if (!bValidReview)
	{
		bPendingBreak = false;
		PendingBreakSequence = 0;
		return;
	}

	PublishPoiseBreak();
}

void UWuwaPoiseComponent::PublishPoiseBreak()
{
	UWuwaAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!IsValid(ASC) || !bPendingBreak)
	{
		return;
	}

	BreakSequence = PendingBreakSequence;
	bPendingBreak = false;
	PendingBreakSequence = 0;

	FWuwaPoiseBreakFact Fact;
	Fact.TargetActor = GetOwner();
	Fact.BreakSequence = BreakSequence;
	Fact.PreviousPoise = LastPreviousPoise;
	Fact.CurrentPoise = LastCurrentPoise;
	Fact.bAuthority = GetOwner() != nullptr && GetOwner()->HasAuthority();
	PoiseBreakFactDelegate.Broadcast(Fact);

	FGameplayEventData Payload;
	Payload.EventTag = WuwaGameplayTags::Event_Combat_Poise_Broken;
	Payload.Instigator = GetOwner();
	Payload.Target = GetOwner();
	Payload.EventMagnitude = static_cast<float>(BreakSequence);
	ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
}

void UWuwaPoiseComponent::CancelPendingBreak()
{
	if (UWorld* World = GetWorld(); IsValid(World) && PendingBreakTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(PendingBreakTimerHandle);
	}
	PendingBreakTimerHandle.Invalidate();
	bPendingBreak = false;
	PendingBreakSequence = 0;
}

void UWuwaPoiseComponent::ResetBindingState()
{
	AbilitySystemComponent.Reset();
	PoiseChangedDelegateHandle.Reset();
	PendingBreakTimerHandle.Invalidate();
	bPendingBreak = false;
	PendingBreakSequence = 0;
}
