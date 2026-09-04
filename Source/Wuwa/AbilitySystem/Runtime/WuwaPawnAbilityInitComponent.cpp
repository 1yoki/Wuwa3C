// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Runtime/WuwaPawnAbilityInitComponent.h"

#include "AbilitySystem/Input/WuwaAbilityInputRouterComponent.h"
#include "AbilitySystem/Interop/WuwaActionAbilityInteropComponent.h"
#include "AbilitySystem/Runtime/WuwaAbilitySystemComponent.h"
#include "AbilitySystem/WuwaAbilitySystemLog.h"
#include "Combat/Data/WuwaCombatProfile.h"
#include "Combat/Data/WuwaMeleeAttackDefinition.h"
#include "Combat/Health/WuwaHealthComponent.h"
#include "Combat/Poise/WuwaPoiseComponent.h"
#include "Actions/Runtime/WuwaActionCoordinatorComponent.h"
#include "Combat/Runtime/WuwaCombatExecutionComponent.h"
#include "Combat/Runtime/WuwaWeaponComponent.h"
#include "Core/WuwaGameplayTags.h"
#include "WuwaCharacter.h"
#include "WuwaPlayerState.h"

namespace
{
EWuwaCombatWindowEndReason ResolveCombatShutdownReason(const EWuwaPawnAbilityShutdownReason Reason)
{
	switch (Reason)
	{
		case EWuwaPawnAbilityShutdownReason::EndPlay:
			return EWuwaCombatWindowEndReason::EndPlay;

		case EWuwaPawnAbilityShutdownReason::AvatarReplaced:
			return EWuwaCombatWindowEndReason::AvatarChanged;

		case EWuwaPawnAbilityShutdownReason::Unpossessed:
		case EWuwaPawnAbilityShutdownReason::Respawn:
		case EWuwaPawnAbilityShutdownReason::ExplicitReset:
		default:
			return EWuwaCombatWindowEndReason::InitializationShutdown;
	}
}
}

UWuwaPawnAbilityInitComponent::UWuwaPawnAbilityInitComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
	DefaultCombatProfile = TSoftObjectPtr<UWuwaCombatProfile>(
	    FSoftObjectPath(TEXT("/Game/Wuwa3c/Data/Combat/DA_CombatProfile_Default.DA_CombatProfile_Default")));
}

bool UWuwaPawnAbilityInitComponent::TryInitializeAbilitySystem()
{
	if (InitState == EWuwaPawnAbilityInitState::ShuttingDown)
	{
		return FailInitialization(InitState, EWuwaPawnAbilityInitFailureReason::ShuttingDown);
	}

	AWuwaCharacter* Character = Cast<AWuwaCharacter>(GetOwner());
	if (!IsValid(Character))
	{
		return FailInitialization(EWuwaPawnAbilityInitState::Uninitialized,
		                          EWuwaPawnAbilityInitFailureReason::InvalidOwnerCharacter);
	}

	APlayerState* RawPlayerState = Character->GetPlayerState();
	if (!IsValid(RawPlayerState))
	{
		if (BoundCharacter.IsValid() || BoundPlayerState.IsValid() || BoundAbilitySystemComponent.IsValid())
		{
			ShutdownAbilitySystem(EWuwaPawnAbilityShutdownReason::AvatarReplaced);
		}

		return FailInitialization(EWuwaPawnAbilityInitState::WaitingForPlayerState,
		                          EWuwaPawnAbilityInitFailureReason::MissingPlayerState);
	}

	if (BoundPlayerState.IsValid() && BoundPlayerState.Get() != RawPlayerState)
	{
		ShutdownAbilitySystem(EWuwaPawnAbilityShutdownReason::AvatarReplaced);
	}

	AWuwaPlayerState* PlayerState = Cast<AWuwaPlayerState>(RawPlayerState);
	if (!IsValid(PlayerState))
	{
		return FailInitialization(EWuwaPawnAbilityInitState::WaitingForPlayerState,
		                          EWuwaPawnAbilityInitFailureReason::UnsupportedPlayerState);
	}

	if (PlayerState->GetPawn() != Character)
	{
		if (BoundCharacter.IsValid() || BoundPlayerState.IsValid() || BoundAbilitySystemComponent.IsValid())
		{
			ShutdownAbilitySystem(EWuwaPawnAbilityShutdownReason::AvatarReplaced);
		}

		return FailInitialization(EWuwaPawnAbilityInitState::WaitingForPlayerState,
		                          EWuwaPawnAbilityInitFailureReason::PlayerStatePawnMismatch);
	}

	UWuwaAbilitySystemComponent* AbilitySystemComponent = PlayerState->GetWuwaAbilitySystemComponent();
	if (!IsValid(AbilitySystemComponent))
	{
		return FailInitialization(EWuwaPawnAbilityInitState::WaitingForPlayerState,
		                          EWuwaPawnAbilityInitFailureReason::MissingAbilitySystemComponent);
	}

	const bool bSameBinding = BoundCharacter.Get() == Character && BoundPlayerState.Get() == PlayerState &&
	                          BoundAbilitySystemComponent.Get() == AbilitySystemComponent;
	if (bSameBinding && DoesActorInfoMatch())
	{
		if (!CompleteGameplayInitialization(Character, AbilitySystemComponent))
		{
			return false;
		}

		return true;
	}

	if (BoundCharacter.IsValid() || BoundPlayerState.IsValid() || BoundAbilitySystemComponent.IsValid())
	{
		ShutdownAbilitySystem(EWuwaPawnAbilityShutdownReason::AvatarReplaced);
	}

	BoundCharacter = Character;
	BoundPlayerState = PlayerState;
	BoundAbilitySystemComponent = AbilitySystemComponent;
	AbilitySystemComponent->InitAbilityActorInfo(PlayerState, Character);
	InitState = EWuwaPawnAbilityInitState::ActorInfoReady;

	if (!DoesActorInfoMatch())
	{
		ShutdownAbilitySystem(EWuwaPawnAbilityShutdownReason::ExplicitReset);
		return FailInitialization(EWuwaPawnAbilityInitState::Uninitialized,
		                          EWuwaPawnAbilityInitFailureReason::ActorInfoVerificationFailed);
	}

	++InitializationGeneration;
	if (!CompleteGameplayInitialization(Character, AbilitySystemComponent))
	{
		return false;
	}

	return true;
}

void UWuwaPawnAbilityInitComponent::ShutdownAbilitySystem(const EWuwaPawnAbilityShutdownReason Reason)
{
	const bool bHadBinding = BoundCharacter.IsValid() || BoundPlayerState.IsValid() ||
	                         BoundAbilitySystemComponent.IsValid() || BoundAbilityInputRouter.IsValid() ||
	                         !GrantedAbilitySetHandles.IsEmpty() || bAuthorityAbilitySetGranted ||
	                         IsValid(BoundCombatProfile);
	if (!bHadBinding)
	{
		InitState = EWuwaPawnAbilityInitState::Uninitialized;
		LastFailureReason = EWuwaPawnAbilityInitFailureReason::None;
		return;
	}

	InitState = EWuwaPawnAbilityInitState::ShuttingDown;
	AWuwaCharacter* Character = BoundCharacter.Get();
	UWuwaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get();
	UWuwaAbilityInputRouterComponent* AbilityInputRouter = BoundAbilityInputRouter.Get();
	UWuwaActionAbilityInteropComponent* ActionAbilityInterop =
	    IsValid(Character) ? Character->GetActionAbilityInteropComponent() : nullptr;

	const bool bOwnsCurrentAvatar =
	    IsValid(AbilitySystemComponent) && AbilitySystemComponent->GetAvatarActor() == Character;

	if (IsValid(AbilityInputRouter))
	{
		AbilityInputRouter->Shutdown();
	}

	if (IsValid(AbilitySystemComponent) && bOwnsCurrentAvatar)
	{
		FGameplayTagContainer CombatAbilityTags;
		CombatAbilityTags.AddTag(WuwaGameplayTags::Ability_Combat);
		AbilitySystemComponent->CancelAbilities(&CombatAbilityTags);
	}
	if (IsValid(ActionAbilityInterop))
	{
		ActionAbilityInterop->Shutdown();
	}

	if (IsValid(Character) && IsValid(Character->GetPoiseComponent()))
	{
		Character->GetPoiseComponent()->Shutdown();
	}

	if (IsValid(Character) && IsValid(Character->GetHealthComponent()))
	{
		Character->GetHealthComponent()->Shutdown();
	}

	if (IsValid(Character) && IsValid(Character->GetCombatExecutionComponent()))
	{
		Character->GetCombatExecutionComponent()->Shutdown(ResolveCombatShutdownReason(Reason));
	}

	if (IsValid(Character) && IsValid(Character->GetWeaponComponent()))
	{
		Character->GetWeaponComponent()->Shutdown();
	}

	if (IsValid(AbilitySystemComponent) && AbilitySystemComponent->IsOwnerActorAuthoritative() && IsValid(Character))
	{
		FGameplayTagContainer CooldownTags;
		CooldownTags.AddTag(WuwaGameplayTags::Cooldown_Combat_Attack_Light);
		FGameplayEffectQuery CooldownQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags);
		CooldownQuery.EffectSource = Character;
		AbilitySystemComponent->RemoveActiveEffects(CooldownQuery);
	}

	if (!GrantedAbilitySetHandles.IsEmpty())
	{
		GrantedAbilitySetHandles.TakeFromAbilitySystem(AbilitySystemComponent);
	}
	bAuthorityAbilitySetGranted = false;

	if (bOwnsCurrentAvatar)
	{
		AbilitySystemComponent->ClearActorInfo();
	}

	BoundCharacter.Reset();
	BoundPlayerState.Reset();
	BoundAbilitySystemComponent.Reset();
	BoundAbilityInputRouter.Reset();
	BoundCombatProfile = nullptr;
	++ShutdownGeneration;
	InitState = EWuwaPawnAbilityInitState::Uninitialized;
	LastFailureReason = EWuwaPawnAbilityInitFailureReason::None;
}

void UWuwaPawnAbilityInitComponent::HandlePawnDeath(const FWuwaDeathFact& Fact)
{
	AWuwaCharacter* Character = BoundCharacter.Get();
	UWuwaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get();
	if (!IsValid(Character) || Fact.DeadActor.Get() != Character || Fact.DeathSequence <= 0 ||
	    !IsValid(AbilitySystemComponent) || AbilitySystemComponent->GetAvatarActor() != Character)
	{
		UE_LOG(LogWuwaAbility,
		       Warning,
		       TEXT("Pawn GAS 忽略不属于当前 Avatar 的死亡事实。Character=%s, FactActor=%s, Sequence=%d"),
		       *GetNameSafe(Character),
		       *GetNameSafe(Fact.DeadActor.Get()),
		       Fact.DeathSequence);
		return;
	}

	if (UWuwaAbilityInputRouterComponent* AbilityInputRouter = BoundAbilityInputRouter.Get())
	{
		AbilityInputRouter->ClearAbilityInput();
	}

	FGameplayTagContainer CombatAbilityTags;
	CombatAbilityTags.AddTag(WuwaGameplayTags::Ability_Combat);
	AbilitySystemComponent->CancelAbilities(&CombatAbilityTags);
}

bool UWuwaPawnAbilityInitComponent::IsGameplayReady() const
{
	const AWuwaCharacter* Character = BoundCharacter.Get();
	const bool bAuthorityGrantReady = !IsValid(Character) || !Character->HasAuthority() || bAuthorityAbilitySetGranted;
	return InitState == EWuwaPawnAbilityInitState::GameplayReady && DoesActorInfoMatch() &&
	       BoundAbilityInputRouter.IsValid() && BoundAbilityInputRouter->IsInitialized() &&
	       IsValid(BoundCombatProfile) && BoundCombatProfile->IsRuntimeValid() && IsValid(Character) &&
	       IsValid(Character->GetActionAbilityInteropComponent()) &&
	       Character->GetActionAbilityInteropComponent()->IsInitialized() && IsValid(Character->GetWeaponComponent()) &&
	       Character->GetWeaponComponent()->IsInitialized() && IsValid(Character->GetCombatExecutionComponent()) &&
	       Character->GetCombatExecutionComponent()->IsInitialized() && IsValid(Character->GetHealthComponent()) &&
	       Character->GetHealthComponent()->IsInitialized() && IsValid(Character->GetPoiseComponent()) &&
	       Character->GetPoiseComponent()->IsInitialized() && bAuthorityGrantReady;
}

EWuwaPawnAbilityInitState UWuwaPawnAbilityInitComponent::GetInitState() const
{
	return InitState;
}

EWuwaPawnAbilityInitFailureReason UWuwaPawnAbilityInitComponent::GetLastFailureReason() const
{
	return LastFailureReason;
}

UWuwaAbilitySystemComponent* UWuwaPawnAbilityInitComponent::GetWuwaAbilitySystemComponent() const
{
	return BoundAbilitySystemComponent.Get();
}

FWuwaPawnAbilityInitRuntimeSnapshot UWuwaPawnAbilityInitComponent::GetRuntimeSnapshot() const
{
	const AWuwaCharacter* Character = BoundCharacter.Get();
	const AWuwaPlayerState* PlayerState = BoundPlayerState.Get();
	const UWuwaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get();

	FWuwaPawnAbilityInitRuntimeSnapshot Snapshot;
	Snapshot.InitState = InitState;
	Snapshot.LastFailureReason = LastFailureReason;
	Snapshot.CharacterName = GetNameSafe(Character);
	Snapshot.PlayerStateName = GetNameSafe(PlayerState);
	Snapshot.AbilitySystemComponentName = GetNameSafe(AbilitySystemComponent);
	Snapshot.OwnerActorName =
	    IsValid(AbilitySystemComponent) ? GetNameSafe(AbilitySystemComponent->GetOwnerActor()) : TEXT("None");
	Snapshot.AvatarActorName =
	    IsValid(AbilitySystemComponent) ? GetNameSafe(AbilitySystemComponent->GetAvatarActor()) : TEXT("None");
	Snapshot.bActorInfoMatches = DoesActorInfoMatch();
	Snapshot.bAbilityInputReady = BoundAbilityInputRouter.IsValid() && BoundAbilityInputRouter->IsInitialized();
	Snapshot.bActionAbilityInteropReady = IsValid(Character) &&
	                                      IsValid(Character->GetActionAbilityInteropComponent()) &&
	                                      Character->GetActionAbilityInteropComponent()->IsInitialized();
	Snapshot.bAuthorityAbilitySetGranted = bAuthorityAbilitySetGranted;
	Snapshot.bCombatProfileReady = IsValid(BoundCombatProfile) && BoundCombatProfile->IsRuntimeValid();
	Snapshot.bWeaponReady = IsValid(Character) && IsValid(Character->GetWeaponComponent()) &&
	                        Character->GetWeaponComponent()->IsInitialized();
	Snapshot.bAttackDefinitionReady = IsValid(BoundCombatProfile) &&
	                                  IsValid(BoundCombatProfile->GetDefaultAttackDefinition()) &&
	                                  BoundCombatProfile->GetDefaultAttackDefinition()->IsRuntimeValid();
	Snapshot.bCombatExecutionReady = IsValid(Character) && IsValid(Character->GetCombatExecutionComponent()) &&
	                                 Character->GetCombatExecutionComponent()->IsInitialized();
	Snapshot.bHealthComponentReady = IsValid(Character) && IsValid(Character->GetHealthComponent()) &&
	                                 Character->GetHealthComponent()->IsInitialized();
	Snapshot.bPoiseComponentReady = IsValid(Character) && IsValid(Character->GetPoiseComponent()) &&
	                                Character->GetPoiseComponent()->IsInitialized();
	Snapshot.CombatProfileName = GetNameSafe(BoundCombatProfile.Get());
	Snapshot.GrantedAbilityCount = GrantedAbilitySetHandles.GetAbilityCount();
	Snapshot.GrantedEffectCount = GrantedAbilitySetHandles.GetEffectCount();
	Snapshot.GrantedActiveEffectCount = GrantedAbilitySetHandles.GetActiveEffectCount();
	Snapshot.AppliedInstantEffectCount = GrantedAbilitySetHandles.GetInstantEffectCount();
	Snapshot.InitializationGeneration = InitializationGeneration;
	Snapshot.ShutdownGeneration = ShutdownGeneration;
	Snapshot.AbilitySetGrantGeneration = AbilitySetGrantGeneration;
	return Snapshot;
}

const UWuwaCombatProfile* UWuwaPawnAbilityInitComponent::GetCombatProfile() const
{
	return BoundCombatProfile.Get();
}

void UWuwaPawnAbilityInitComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ShutdownAbilitySystem(EWuwaPawnAbilityShutdownReason::EndPlay);
	Super::EndPlay(EndPlayReason);
}

bool UWuwaPawnAbilityInitComponent::FailInitialization(const EWuwaPawnAbilityInitState State,
                                                       const EWuwaPawnAbilityInitFailureReason Reason)
{
	InitState = State;
	LastFailureReason = Reason;

	if (Reason != EWuwaPawnAbilityInitFailureReason::MissingPlayerState &&
	    Reason != EWuwaPawnAbilityInitFailureReason::PlayerStatePawnMismatch)
	{
		UE_LOG(LogWuwaAbility,
		       Warning,
		       TEXT("Pawn GAS 初始化失败。Owner=%s, State=%d, FailureReason=%d"),
		       *GetNameSafe(GetOwner()),
		       static_cast<int32>(State),
		       static_cast<int32>(Reason));
	}
	return false;
}

bool UWuwaPawnAbilityInitComponent::CompleteGameplayInitialization(AWuwaCharacter* Character,
                                                                   UWuwaAbilitySystemComponent* AbilitySystemComponent)
{
	UWuwaAbilityInputRouterComponent* AbilityInputRouter =
	    IsValid(Character) ? Character->GetAbilityInputRouterComponent() : nullptr;
	if (!IsValid(AbilityInputRouter))
	{
		return FailInitialization(EWuwaPawnAbilityInitState::ActorInfoReady,
		                          EWuwaPawnAbilityInitFailureReason::MissingAbilityInputRouter);
	}

	if (!AbilityInputRouter->Initialize(AbilitySystemComponent))
	{
		return FailInitialization(EWuwaPawnAbilityInitState::ActorInfoReady,
		                          EWuwaPawnAbilityInitFailureReason::AbilityInputRouterInitializationFailed);
	}
	BoundAbilityInputRouter = AbilityInputRouter;

	UWuwaActionAbilityInteropComponent* ActionAbilityInterop =
	    IsValid(Character) ? Character->GetActionAbilityInteropComponent() : nullptr;
	if (!IsValid(ActionAbilityInterop))
	{
		AbilityInputRouter->Shutdown();
		BoundAbilityInputRouter.Reset();
		return FailInitialization(EWuwaPawnAbilityInitState::ActorInfoReady,
		                          EWuwaPawnAbilityInitFailureReason::MissingActionAbilityInteropComponent);
	}

	if (!ActionAbilityInterop->Initialize(AbilitySystemComponent,
	                                      Character->GetStateTagComponent(),
	                                      Character->GetActionNetworkComponent(),
	                                      AbilityInputRouter))
	{
		AbilityInputRouter->Shutdown();
		BoundAbilityInputRouter.Reset();
		return FailInitialization(EWuwaPawnAbilityInitState::ActorInfoReady,
		                          EWuwaPawnAbilityInitFailureReason::ActionAbilityInteropInitializationFailed);
	}

	UWuwaActionCoordinatorComponent* ActionCoordinator = Character->GetActionCoordinatorComponent();

	if (!IsValid(ActionCoordinator))
	{
		ActionAbilityInterop->Shutdown();

		AbilityInputRouter->Shutdown();
		BoundAbilityInputRouter.Reset();

		return FailInitialization(EWuwaPawnAbilityInitState::ActorInfoReady,
		                          EWuwaPawnAbilityInitFailureReason::ActionAbilityInteropInitializationFailed);
	}

	if (!ActionCoordinator->BindAbilityInterop(ActionAbilityInterop))
	{
		ActionAbilityInterop->Shutdown();

		AbilityInputRouter->Shutdown();
		BoundAbilityInputRouter.Reset();

		return FailInitialization(EWuwaPawnAbilityInitState::ActorInfoReady,
		                          EWuwaPawnAbilityInitFailureReason::ActionAbilityInteropInitializationFailed);
	}

	UWuwaCombatProfile* CombatProfile = BoundCombatProfile.Get();
	if (!IsValid(CombatProfile))
	{
		CombatProfile = DefaultCombatProfile.LoadSynchronous();
		if (!IsValid(CombatProfile))
		{
			return FailInitialization(EWuwaPawnAbilityInitState::ActorInfoReady,
			                          EWuwaPawnAbilityInitFailureReason::MissingCombatProfile);
		}

		if (!CombatProfile->IsRuntimeValid())
		{
			return FailInitialization(EWuwaPawnAbilityInitState::ActorInfoReady,
			                          EWuwaPawnAbilityInitFailureReason::InvalidCombatProfile);
		}
		BoundCombatProfile = CombatProfile;
	}

	UWuwaWeaponComponent* WeaponComponent = Character->GetWeaponComponent();
	if (!IsValid(WeaponComponent))
	{
		return FailInitialization(EWuwaPawnAbilityInitState::ActorInfoReady,
		                          EWuwaPawnAbilityInitFailureReason::MissingWeaponComponent);
	}

	if (!WeaponComponent->Initialize(Character->GetMesh(),
	                                 Character->GetWeaponMeshComponent(),
	                                 Character->GetScabbardMeshComponent(),
	                                 CombatProfile->GetDefaultWeaponDefinition()))
	{
		return FailInitialization(EWuwaPawnAbilityInitState::ActorInfoReady,
		                          EWuwaPawnAbilityInitFailureReason::WeaponInitializationFailed);
	}

	UWuwaCombatExecutionComponent* CombatExecutionComponent = Character->GetCombatExecutionComponent();
	if (!IsValid(CombatExecutionComponent))
	{
		WeaponComponent->Shutdown();
		return FailInitialization(EWuwaPawnAbilityInitState::ActorInfoReady,
		                          EWuwaPawnAbilityInitFailureReason::MissingCombatExecutionComponent);
	}

	if (!CombatExecutionComponent->Initialize(Character->GetMesh(), WeaponComponent))
	{
		WeaponComponent->Shutdown();
		return FailInitialization(EWuwaPawnAbilityInitState::ActorInfoReady,
		                          EWuwaPawnAbilityInitFailureReason::CombatExecutionInitializationFailed);
	}

	if (Character->HasAuthority() && !bAuthorityAbilitySetGranted)
	{
		UWuwaAbilitySet* AbilitySet = CombatProfile->GetDefaultAbilitySet();
		if (!IsValid(AbilitySet))
		{
			return FailInitialization(EWuwaPawnAbilityInitState::ActorInfoReady,
			                          EWuwaPawnAbilityInitFailureReason::MissingDefaultAbilitySet);
		}

		if (!GrantedAbilitySetHandles.IsEmpty())
		{
			GrantedAbilitySetHandles.TakeFromAbilitySystem(AbilitySystemComponent);
		}

		if (!AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &GrantedAbilitySetHandles, Character))
		{
			return FailInitialization(EWuwaPawnAbilityInitState::ActorInfoReady,
			                          EWuwaPawnAbilityInitFailureReason::AbilitySetGrantFailed);
		}

		bAuthorityAbilitySetGranted = true;
		++AbilitySetGrantGeneration;
	}

	UWuwaHealthComponent* HealthComponent = Character->GetHealthComponent();
	if (!IsValid(HealthComponent))
	{
		return FailInitialization(EWuwaPawnAbilityInitState::ActorInfoReady,
		                          EWuwaPawnAbilityInitFailureReason::MissingHealthComponent);
	}

	if (!HealthComponent->Initialize(AbilitySystemComponent, CombatProfile->GetDeadEffectClass()))
	{
		return FailInitialization(EWuwaPawnAbilityInitState::ActorInfoReady,
		                          EWuwaPawnAbilityInitFailureReason::HealthComponentInitializationFailed);
	}

	UWuwaPoiseComponent* PoiseComponent = Character->GetPoiseComponent();
	if (!IsValid(PoiseComponent))
	{
		HealthComponent->Shutdown();
		return FailInitialization(EWuwaPawnAbilityInitState::ActorInfoReady,
		                          EWuwaPawnAbilityInitFailureReason::MissingPoiseComponent);
	}

	if (!PoiseComponent->Initialize(AbilitySystemComponent))
	{
		HealthComponent->Shutdown();
		return FailInitialization(EWuwaPawnAbilityInitState::ActorInfoReady,
		                          EWuwaPawnAbilityInitFailureReason::PoiseComponentInitializationFailed);
	}

	InitState = EWuwaPawnAbilityInitState::GameplayReady;
	LastFailureReason = EWuwaPawnAbilityInitFailureReason::None;
	return true;
}

bool UWuwaPawnAbilityInitComponent::DoesActorInfoMatch() const
{
	const AWuwaCharacter* Character = BoundCharacter.Get();
	const AWuwaPlayerState* PlayerState = BoundPlayerState.Get();
	const UWuwaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get();
	return IsValid(Character) && IsValid(PlayerState) && IsValid(AbilitySystemComponent) &&
	       AbilitySystemComponent->GetOwnerActor() == PlayerState &&
	       AbilitySystemComponent->GetAvatarActor() == Character;
}
