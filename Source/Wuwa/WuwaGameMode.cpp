// Copyright Epic Games, Inc. All Rights Reserved.

#include "WuwaGameMode.h"

#include "AbilitySystem/Runtime/WuwaAbilitySystemComponent.h"
#include "AbilitySystem/Runtime/WuwaPawnAbilityInitComponent.h"
#include "Combat/Health/WuwaHealthComponent.h"
#include "Core/WuwaGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"
#include "WuwaCharacter.h"
#include "WuwaPlayerState.h"

AWuwaGameMode::AWuwaGameMode()
{
	PlayerStateClass = AWuwaPlayerState::StaticClass();
}

void AWuwaGameMode::HandlePawnDeath(const FWuwaDeathFact& Fact)
{
	AWuwaCharacter* DeadCharacter = Cast<AWuwaCharacter>(Fact.DeadActor.Get());
	AController* Controller = IsValid(DeadCharacter) ? DeadCharacter->GetController() : nullptr;
	if (!HasAuthority() || !Fact.bAuthority || Fact.DeathSequence <= 0 || !IsValid(DeadCharacter) ||
	    !IsValid(Controller) || Controller->GetPawn() != DeadCharacter || PendingRespawnTimers.Contains(Controller))
	{
		UE_LOG(LogGameMode,
		       Warning,
		       TEXT("拒绝无效或重复死亡重生请求。Authority=%s, FactAuthority=%s, Sequence=%d, Controller=%s, Pawn=%s, "
		            "CurrentPawn=%s, Pending=%s"),
		       HasAuthority() ? TEXT("true") : TEXT("false"),
		       Fact.bAuthority ? TEXT("true") : TEXT("false"),
		       Fact.DeathSequence,
		       *GetNameSafe(Controller),
		       *GetNameSafe(DeadCharacter),
		       *GetNameSafe(IsValid(Controller) ? Controller->GetPawn() : nullptr),
		       PendingRespawnTimers.Contains(Controller) ? TEXT("true") : TEXT("false"));
		return;
	}

	FTimerDelegate RespawnDelegate = FTimerDelegate::CreateUObject(this,
	                                                               &ThisClass::ExecuteRespawn,
	                                                               TWeakObjectPtr<AController>(Controller),
	                                                               TWeakObjectPtr<APawn>(DeadCharacter));
	FTimerHandle& TimerHandle = PendingRespawnTimers.Add(Controller);
	GetWorldTimerManager().SetTimer(TimerHandle, RespawnDelegate, FMath::Max(0.f, RespawnDelaySeconds), false);
}

void AWuwaGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (TPair<TWeakObjectPtr<AController>, FTimerHandle>& Entry : PendingRespawnTimers)
	{
		GetWorldTimerManager().ClearTimer(Entry.Value);
	}
	PendingRespawnTimers.Reset();
	Super::EndPlay(EndPlayReason);
}

void AWuwaGameMode::ExecuteRespawn(const TWeakObjectPtr<AController> ControllerKey,
                                   const TWeakObjectPtr<APawn> ExpectedDeadPawnKey)
{
	PendingRespawnTimers.Remove(ControllerKey);
	AController* Controller = ControllerKey.Get();
	AWuwaCharacter* DeadCharacter = Cast<AWuwaCharacter>(ExpectedDeadPawnKey.Get());
	if (!HasAuthority() || !IsValid(Controller) || !IsValid(DeadCharacter) || Controller->GetPawn() != DeadCharacter)
	{
		return;
	}

	AWuwaPlayerState* PlayerState = Controller->GetPlayerState<AWuwaPlayerState>();
	UWuwaAbilitySystemComponent* AbilitySystemComponent =
	    IsValid(PlayerState) ? PlayerState->GetWuwaAbilitySystemComponent() : nullptr;
	UWuwaHealthComponent* HealthComponent = DeadCharacter->GetHealthComponent();
	UWuwaPawnAbilityInitComponent* InitComponent = DeadCharacter->GetPawnAbilityInitComponent();
	if (!IsValid(PlayerState) || !IsValid(AbilitySystemComponent) || !IsValid(HealthComponent) ||
	    !IsValid(InitComponent) || !HealthComponent->BeginRespawn())
	{
		UE_LOG(LogGameMode,
		       Error,
		       TEXT("重生失败：旧 Pawn 上下文无效。Controller=%s, Pawn=%s"),
		       *GetNameSafe(Controller),
		       *GetNameSafe(DeadCharacter));
		return;
	}

	InitComponent->ShutdownAbilitySystem(EWuwaPawnAbilityShutdownReason::Respawn);
	RemoveDeadEffects(AbilitySystemComponent);
	Controller->UnPossess();
	DeadCharacter->Destroy();
	RestartPlayer(Controller);
}

int32 AWuwaGameMode::RemoveDeadEffects(UAbilitySystemComponent* AbilitySystemComponent)
{
	if (!IsValid(AbilitySystemComponent))
	{
		return 0;
	}

	FGameplayTagContainer DeadTags;
	DeadTags.AddTag(WuwaGameplayTags::State_Combat_Dead);
	return AbilitySystemComponent->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(DeadTags));
}
