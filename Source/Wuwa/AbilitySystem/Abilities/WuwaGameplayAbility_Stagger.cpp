// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Abilities/WuwaGameplayAbility_Stagger.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/Runtime/WuwaAbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Combat/Attributes/WuwaHealthSet.h"
#include "Combat/Attributes/WuwaPoiseSet.h"
#include "Combat/Health/WuwaHealthComponent.h"
#include "Combat/WuwaCombatLog.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/WuwaGameplayTags.h"
#include "GameplayEffect.h"
#include "WuwaCharacter.h"

UWuwaGameplayAbility_Stagger::UWuwaGameplayAbility_Stagger()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(WuwaGameplayTags::Ability_Combat);
	AssetTags.AddTag(WuwaGameplayTags::Ability_Combat_Reaction);
	AssetTags.AddTag(WuwaGameplayTags::Ability_Combat_Reaction_Stagger);
	SetAssetTags(AssetTags);
	WuwaBlockedStateTags.AddTag(WuwaGameplayTags::State_Combat_Dead);
	WuwaBlockedStateTags.AddTag(WuwaGameplayTags::State_Combat_Staggered);

	FAbilityTriggerData& TriggerData = AbilityTriggers.AddDefaulted_GetRef();
	TriggerData.TriggerTag = WuwaGameplayTags::Event_Combat_Poise_Broken;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
}

FWuwaStaggerRuntimeSnapshot UWuwaGameplayAbility_Stagger::GetRuntimeSnapshot() const
{
	const UWuwaAbilitySystemComponent* AbilitySystemComponent = ActiveAbilitySystemComponent.Get();
	if (!IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent = GetWuwaAbilitySystemComponentFromActorInfo();
	}

	FWuwaStaggerRuntimeSnapshot Snapshot;
	Snapshot.bActive = IsActive();
	Snapshot.bCleanupComplete = bCleanupComplete;
	Snapshot.bStateEffectActive = ActiveStateEffectHandle.IsValid() && IsValid(AbilitySystemComponent) &&
	                              AbilitySystemComponent->GetActiveGameplayEffect(ActiveStateEffectHandle) != nullptr;
	Snapshot.ActivationCount = ActivationCount;
	Snapshot.RefreshCount = RefreshCount;
	Snapshot.CleanupCount = CleanupCount;
	Snapshot.PoiseResetCount = PoiseResetCount;
	Snapshot.StateTagCount = IsValid(AbilitySystemComponent)
	                             ? AbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::State_Combat_Staggered)
	                             : 0;
	Snapshot.MoveBlockTagCount = IsValid(AbilitySystemComponent)
	                                 ? AbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::Block_Input_Move)
	                                 : 0;
	Snapshot.LastEndReason = LastEndReason;
	Snapshot.Montage = HitReactMontage;
	return Snapshot;
}

bool UWuwaGameplayAbility_Stagger::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                                      const FGameplayAbilityActorInfo* ActorInfo,
                                                      const FGameplayTagContainer* SourceTags,
                                                      const FGameplayTagContainer* TargetTags,
                                                      FGameplayTagContainer* OptionalRelevantTags) const
{
	const bool bSuperCanActivate =
	    Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
	const bool bContextValid = ValidateActivationContext(ActorInfo);
	if (!bSuperCanActivate || !bContextValid)
	{
		const AActor* Avatar = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;
		UE_LOG(LogWuwaCombat,
		       Warning,
		       TEXT("硬直拒绝激活。Ability=%s, Super=%s, Context=%s, Avatar=%s, Authority=%s"),
		       *GetNameSafe(this),
		       bSuperCanActivate ? TEXT("true") : TEXT("false"),
		       bContextValid ? TEXT("true") : TEXT("false"),
		       *GetNameSafe(Avatar),
		       IsValid(Avatar) && Avatar->HasAuthority() ? TEXT("true") : TEXT("false"));
	}
	return bSuperCanActivate && bContextValid;
}

void UWuwaGameplayAbility_Stagger::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                                   const FGameplayAbilityActorInfo* ActorInfo,
                                                   const FGameplayAbilityActivationInfo ActivationInfo,
                                                   const FGameplayEventData* TriggerEventData)
{
	bFinishRequested = false;
	bCleanupInProgress = false;
	bRestartingMontage = false;
	bCleanupComplete = false;
	LastEndReason = EWuwaStaggerEndReason::None;
	ActiveStateEffectHandle.Invalidate();

	const AActor* Avatar = ActorInfo != nullptr ? ActorInfo->AvatarActor.Get() : nullptr;
	if (IsValid(Avatar) && !Avatar->HasAuthority())
	{
		if (!ValidateOwningClientPresentationContext(ActorInfo, ActivationInfo, TriggerEventData))
		{
			UE_LOG(LogWuwaCombat,
			       Error,
			       TEXT("硬直拥有者客户端表现上下文无效。Ability=%s, Avatar=%s, Mode=%d, Event=%s"),
			       *GetNameSafe(this),
			       *GetNameSafe(Avatar),
			       static_cast<int32>(ActivationInfo.ActivationMode),
			       TriggerEventData != nullptr ? *TriggerEventData->EventTag.ToString() : TEXT("None"));
			LastEndReason = EWuwaStaggerEndReason::StateEffectFailed;
			EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
			return;
		}

		AWuwaCharacter* Character = CastChecked<AWuwaCharacter>(ActorInfo->AvatarActor.Get());
		ActiveAvatar = Character;
		ActiveMesh = Character->GetMesh();
		if (!StartStaggerMontage())
		{
			LastEndReason = EWuwaStaggerEndReason::MontageFailed;
			EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
			return;
		}
		if (!BindHealthChangedFact())
		{
			LastEndReason = EWuwaStaggerEndReason::RefreshBindingFailed;
			EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
			return;
		}

		return;
	}

	if (!ValidateActivationContext(ActorInfo) || TriggerEventData == nullptr ||
	    TriggerEventData->EventTag != WuwaGameplayTags::Event_Combat_Poise_Broken)
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("硬直激活失败：权威上下文、配置或触发事件无效。Ability=%s, Event=%s"),
		       *GetNameSafe(this),
		       TriggerEventData != nullptr ? *TriggerEventData->EventTag.ToString() : TEXT("None"));
		LastEndReason = EWuwaStaggerEndReason::StateEffectFailed;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AWuwaCharacter* Character = CastChecked<AWuwaCharacter>(ActorInfo->AvatarActor.Get());
	UWuwaAbilitySystemComponent* AbilitySystemComponent =
	    CastChecked<UWuwaAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get());
	ActiveAvatar = Character;
	ActiveAbilitySystemComponent = AbilitySystemComponent;
	ActiveMesh = Character->GetMesh();
	++ActivationCount;

	FGameplayTagContainer AttackTags;
	AttackTags.AddTag(WuwaGameplayTags::Ability_Combat_Attack);
	AbilitySystemComponent->CancelAbilities(&AttackTags, nullptr, this);

	if (!ApplyStaggerState())
	{
		LastEndReason = EWuwaStaggerEndReason::StateEffectFailed;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!StartStaggerMontage())
	{
		LastEndReason = EWuwaStaggerEndReason::MontageFailed;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!BindHealthChangedFact())
	{
		LastEndReason = EWuwaStaggerEndReason::RefreshBindingFailed;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
}

void UWuwaGameplayAbility_Stagger::CancelAbility(const FGameplayAbilitySpecHandle Handle,
                                                 const FGameplayAbilityActorInfo* ActorInfo,
                                                 const FGameplayAbilityActivationInfo ActivationInfo,
                                                 const bool bReplicateCancelAbility)
{
	if (IsActive() && CanBeCanceled())
	{
		const UWuwaAbilitySystemComponent* AbilitySystemComponent = ActiveAbilitySystemComponent.Get();
		if (IsValid(AbilitySystemComponent) &&
		    AbilitySystemComponent->HasMatchingGameplayTag(WuwaGameplayTags::State_Combat_Dead))
		{
			LastEndReason = EWuwaStaggerEndReason::Death;
		}
		else if (ActorInfo == nullptr || ActorInfo->AvatarActor.Get() != ActiveAvatar.Get())
		{
			LastEndReason = EWuwaStaggerEndReason::AvatarChanged;
		}
		else
		{
			LastEndReason = EWuwaStaggerEndReason::Cancelled;
		}
		bFinishRequested = true;
	}
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

void UWuwaGameplayAbility_Stagger::EndAbility(const FGameplayAbilitySpecHandle Handle,
                                              const FGameplayAbilityActorInfo* ActorInfo,
                                              const FGameplayAbilityActivationInfo ActivationInfo,
                                              const bool bReplicateEndAbility,
                                              const bool bWasCancelled)
{
	bFinishRequested = true;
	if (LastEndReason == EWuwaStaggerEndReason::None)
	{
		LastEndReason = bWasCancelled ? EWuwaStaggerEndReason::Cancelled : EWuwaStaggerEndReason::Completed;
	}
	CleanupStaggerState();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UWuwaGameplayAbility_Stagger::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo,
                                                   const FGameplayAbilitySpec& Spec)
{
	bFinishRequested = true;
	LastEndReason = EWuwaStaggerEndReason::AbilityRemoved;
	CleanupStaggerState();
	Super::OnRemoveAbility(ActorInfo, Spec);
}

bool UWuwaGameplayAbility_Stagger::ValidateActivationContext(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const AWuwaCharacter* Character =
	    ActorInfo != nullptr ? Cast<AWuwaCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UWuwaAbilitySystemComponent* AbilitySystemComponent =
	    ActorInfo != nullptr ? Cast<UWuwaAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	const USkeletalMeshComponent* Mesh = IsValid(Character) ? Character->GetMesh() : nullptr;
	const float Health = IsValid(AbilitySystemComponent)
	                         ? AbilitySystemComponent->GetNumericAttribute(UWuwaHealthSet::GetHealthAttribute())
	                         : 0.f;
	return IsValid(Character) && Character->HasAuthority() && IsValid(AbilitySystemComponent) &&
	       AbilitySystemComponent->GetAvatarActor() == Character && IsValid(Mesh) && IsValid(Mesh->GetAnimInstance()) &&
	       IsValid(StaggerStateEffectClass) && IsValid(ResetPoiseEffectClass) && IsValid(HitReactMontage) &&
	       FMath::IsFinite(Health) && Health > 0.f &&
	       !AbilitySystemComponent->HasMatchingGameplayTag(WuwaGameplayTags::State_Combat_Dead) &&
	       !AbilitySystemComponent->HasMatchingGameplayTag(WuwaGameplayTags::State_Combat_Staggered);
}

bool UWuwaGameplayAbility_Stagger::ValidateOwningClientPresentationContext(
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo& ActivationInfo,
    const FGameplayEventData* TriggerEventData) const
{
	const AWuwaCharacter* Character =
	    ActorInfo != nullptr ? Cast<AWuwaCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UWuwaAbilitySystemComponent* AbilitySystemComponent =
	    ActorInfo != nullptr ? Cast<UWuwaAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	const USkeletalMeshComponent* Mesh = IsValid(Character) ? Character->GetMesh() : nullptr;
	return ActivationInfo.ActivationMode == EGameplayAbilityActivationMode::Confirmed && TriggerEventData != nullptr &&
	       TriggerEventData->EventTag == WuwaGameplayTags::Event_Combat_Poise_Broken && IsValid(Character) &&
	       !Character->HasAuthority() && ActorInfo->IsLocallyControlled() && IsValid(AbilitySystemComponent) &&
	       AbilitySystemComponent->GetAvatarActor() == Character && IsValid(Mesh) && IsValid(Mesh->GetAnimInstance()) &&
	       IsValid(HitReactMontage);
}

bool UWuwaGameplayAbility_Stagger::IsOwningClientPresentationRuntime() const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const AWuwaCharacter* Character = Cast<AWuwaCharacter>(ActiveAvatar.Get());
	const UWuwaAbilitySystemComponent* AbilitySystemComponent =
	    ActorInfo != nullptr ? Cast<UWuwaAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	const USkeletalMeshComponent* Mesh = IsValid(Character) ? Character->GetMesh() : nullptr;
	return IsActive() && !bFinishRequested && !bCleanupInProgress && IsValid(Character) && !Character->HasAuthority() &&
	       ActorInfo != nullptr && ActorInfo->IsLocallyControlled() && ActorInfo->AvatarActor.Get() == Character &&
	       IsValid(AbilitySystemComponent) && AbilitySystemComponent->GetAvatarActor() == Character && IsValid(Mesh) &&
	       ActiveMesh.Get() == Mesh && IsValid(Mesh->GetAnimInstance()) && IsValid(HitReactMontage);
}

bool UWuwaGameplayAbility_Stagger::ApplyStaggerState()
{
	UWuwaAbilitySystemComponent* AbilitySystemComponent = ActiveAbilitySystemComponent.Get();
	if (!IsValid(AbilitySystemComponent) || !IsValid(StaggerStateEffectClass) || ActiveStateEffectHandle.IsValid())
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("硬直状态效果应用被拒绝。ASC=%s, Effect=%s, ExistingHandle=%s"),
		       *GetNameSafe(AbilitySystemComponent),
		       *GetNameSafe(StaggerStateEffectClass),
		       *ActiveStateEffectHandle.ToString());
		return false;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	const FGameplayEffectSpecHandle SpecHandle =
	    AbilitySystemComponent->MakeOutgoingSpec(StaggerStateEffectClass, GetAbilityLevel(), EffectContext);
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("硬直状态效果无法创建 Spec。ASC=%s, Effect=%s"),
		       *GetNameSafe(AbilitySystemComponent),
		       *GetNameSafe(StaggerStateEffectClass));
		return false;
	}

	ActiveStateEffectHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	const bool bApplied = ActiveStateEffectHandle.IsValid() &&
	                      AbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::State_Combat_Staggered) > 0 &&
	                      AbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::Block_Input_Move) > 0;
	if (!bApplied)
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("硬直状态效果应用失败或标签不完整。ASC=%s, Handle=%s, Staggered=%d, MoveBlock=%d"),
		       *GetNameSafe(AbilitySystemComponent),
		       *ActiveStateEffectHandle.ToString(),
		       AbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::State_Combat_Staggered),
		       AbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::Block_Input_Move));
	}
	return bApplied;
}

bool UWuwaGameplayAbility_Stagger::StartStaggerMontage()
{
	if (!IsValid(HitReactMontage) || !ActiveMesh.IsValid() || !IsValid(ActiveMesh->GetAnimInstance()))
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("硬直 Montage 启动失败：配置或 Mesh 无效。Avatar=%s, Montage=%s, Mesh=%s"),
		       *GetNameSafe(ActiveAvatar.Get()),
		       *GetNameSafe(HitReactMontage),
		       *GetNameSafe(ActiveMesh.Get()));
		return false;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
	    this, FName(TEXT("StaggerMontage")), HitReactMontage, 1.f, NAME_None, true, 0.f, 0.f, true);
	if (!IsValid(MontageTask))
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("硬直 Montage 任务创建失败。Avatar=%s, Montage=%s"),
		       *GetNameSafe(ActiveAvatar.Get()),
		       *GetNameSafe(HitReactMontage));
		return false;
	}

	if (ActiveAvatar.IsValid() && ActiveAvatar->HasAuthority())
	{
		MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageCompleted);
		MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
		MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageCancelled);
	}
	MontageTask->ReadyForActivation();
	return IsActive();
}

bool UWuwaGameplayAbility_Stagger::BindHealthChangedFact()
{
	AWuwaCharacter* Character = Cast<AWuwaCharacter>(ActiveAvatar.Get());
	UWuwaHealthComponent* HealthComponent = IsValid(Character) ? Character->GetHealthComponent() : nullptr;
	const bool bAuthorityRuntime = IsValid(Character) && Character->HasAuthority();
	const bool bOwningClientPresentationRuntime = IsOwningClientPresentationRuntime();
	if (!IsValid(Character) || (!bAuthorityRuntime && !bOwningClientPresentationRuntime) || !IsValid(HealthComponent) ||
	    HealthChangedDelegateHandle.IsValid())
	{
		UE_LOG(
		    LogWuwaCombat,
		    Error,
		    TEXT(
		        "硬直无法绑定真实生命变化事实。Avatar=%s, Health=%s, Authority=%s, OwningClient=%s, ExistingHandle=%s"),
		    *GetNameSafe(Character),
		    *GetNameSafe(HealthComponent),
		    bAuthorityRuntime ? TEXT("true") : TEXT("false"),
		    bOwningClientPresentationRuntime ? TEXT("true") : TEXT("false"),
		    HealthChangedDelegateHandle.IsValid() ? TEXT("true") : TEXT("false"));
		return false;
	}

	ActiveHealthComponent = HealthComponent;
	HealthChangedDelegateHandle =
	    HealthComponent->OnHealthChanged().AddUObject(this, &ThisClass::HandleHealthChangedFact);
	if (!HealthChangedDelegateHandle.IsValid())
	{
		ActiveHealthComponent.Reset();
		UE_LOG(LogWuwaCombat, Error, TEXT("硬直绑定真实生命变化事实失败。Avatar=%s"), *GetNameSafe(Character));
		return false;
	}
	return true;
}

void UWuwaGameplayAbility_Stagger::UnbindHealthChangedFact()
{
	UWuwaHealthComponent* HealthComponent = ActiveHealthComponent.Get();
	if (IsValid(HealthComponent) && HealthChangedDelegateHandle.IsValid())
	{
		HealthComponent->OnHealthChanged().Remove(HealthChangedDelegateHandle);
	}
	HealthChangedDelegateHandle.Reset();
	ActiveHealthComponent.Reset();
}

bool UWuwaGameplayAbility_Stagger::RestartStaggerMontage()
{
	const bool bAuthorityRuntime = ActiveAvatar.IsValid() && ActiveAvatar->HasAuthority();
	const bool bOwningClientPresentationRuntime = IsOwningClientPresentationRuntime();
	if (!IsActive() || bFinishRequested || bCleanupInProgress || bRestartingMontage || !ActiveAvatar.IsValid() ||
	    (!bAuthorityRuntime && !bOwningClientPresentationRuntime))
	{
		return false;
	}

	bRestartingMontage = true;
	if (IsValid(MontageTask))
	{
		MontageTask->OnCompleted.RemoveDynamic(this, &ThisClass::HandleMontageCompleted);
		MontageTask->OnInterrupted.RemoveDynamic(this, &ThisClass::HandleMontageInterrupted);
		MontageTask->OnCancelled.RemoveDynamic(this, &ThisClass::HandleMontageCancelled);
		MontageTask->EndTask();
	}
	MontageTask = nullptr;

	if (ActiveMesh.IsValid())
	{
		UAnimInstance* AnimInstance = ActiveMesh->GetAnimInstance();
		if (IsValid(AnimInstance) && IsValid(HitReactMontage) && AnimInstance->Montage_IsPlaying(HitReactMontage))
		{
			AnimInstance->Montage_Stop(0.f, HitReactMontage);
		}
	}

	const bool bStarted = StartStaggerMontage();
	bRestartingMontage = false;
	if (!bStarted)
	{
		return false;
	}

	if (bAuthorityRuntime)
	{
		++RefreshCount;
	}
	return true;
}

bool UWuwaGameplayAbility_Stagger::ShouldResetPoise() const
{
	const UWuwaAbilitySystemComponent* AbilitySystemComponent = ActiveAbilitySystemComponent.Get();
	const AActor* Avatar = ActiveAvatar.Get();
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (LastEndReason != EWuwaStaggerEndReason::Completed || !IsValid(AbilitySystemComponent) || !IsValid(Avatar) ||
	    ActorInfo == nullptr || ActorInfo->AvatarActor.Get() != Avatar ||
	    AbilitySystemComponent->GetAvatarActor() != Avatar ||
	    AbilitySystemComponent->HasMatchingGameplayTag(WuwaGameplayTags::State_Combat_Dead))
	{
		return false;
	}

	const float Health = AbilitySystemComponent->GetNumericAttribute(UWuwaHealthSet::GetHealthAttribute());
	return FMath::IsFinite(Health) && Health > 0.f;
}

bool UWuwaGameplayAbility_Stagger::ApplyPoiseReset()
{
	UWuwaAbilitySystemComponent* AbilitySystemComponent = ActiveAbilitySystemComponent.Get();
	if (!IsValid(AbilitySystemComponent) || !IsValid(ResetPoiseEffectClass))
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("硬直正常结束无法重置韧性：ASC 或效果无效。ASC=%s, Effect=%s"),
		       *GetNameSafe(AbilitySystemComponent),
		       *GetNameSafe(ResetPoiseEffectClass));
		return false;
	}

	const float MaxPoise = AbilitySystemComponent->GetNumericAttribute(UWuwaPoiseSet::GetMaxPoiseAttribute());
	if (!FMath::IsFinite(MaxPoise) || MaxPoise < 0.f)
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("硬直正常结束拒绝非法 MaxPoise。Avatar=%s, MaxPoise=%f"),
		       *GetNameSafe(ActiveAvatar.Get()),
		       MaxPoise);
		return false;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	const FGameplayEffectSpecHandle SpecHandle =
	    AbilitySystemComponent->MakeOutgoingSpec(ResetPoiseEffectClass, GetAbilityLevel(), EffectContext);
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("韧性重置效果无法创建 Spec。ASC=%s, Effect=%s"),
		       *GetNameSafe(AbilitySystemComponent),
		       *GetNameSafe(ResetPoiseEffectClass));
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(WuwaGameplayTags::Data_Poise_Reset, MaxPoise);
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	const float ResetPoise = AbilitySystemComponent->GetNumericAttribute(UWuwaPoiseSet::GetPoiseAttribute());
	if (!FMath::IsNearlyEqual(ResetPoise, MaxPoise))
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("韧性重置效果执行后数值不一致。Avatar=%s, Poise=%f, MaxPoise=%f"),
		       *GetNameSafe(ActiveAvatar.Get()),
		       ResetPoise,
		       MaxPoise);
		return false;
	}

	++PoiseResetCount;
	return true;
}

void UWuwaGameplayAbility_Stagger::CleanupStaggerState()
{
	if (bCleanupInProgress || bCleanupComplete)
	{
		return;
	}

	bCleanupInProgress = true;
	bRestartingMontage = false;
	UnbindHealthChangedFact();
	const bool bResetPoise = ShouldResetPoise();
	if (IsValid(MontageTask))
	{
		MontageTask->EndTask();
	}
	MontageTask = nullptr;

	if (ActiveMesh.IsValid())
	{
		UAnimInstance* AnimInstance = ActiveMesh->GetAnimInstance();
		if (IsValid(AnimInstance) && IsValid(HitReactMontage) && AnimInstance->Montage_IsPlaying(HitReactMontage))
		{
			AnimInstance->Montage_Stop(0.1f, HitReactMontage);
		}
	}

	UWuwaAbilitySystemComponent* AbilitySystemComponent = ActiveAbilitySystemComponent.Get();
	if (ActiveStateEffectHandle.IsValid())
	{
		if (!IsValid(AbilitySystemComponent) ||
		    !AbilitySystemComponent->RemoveActiveGameplayEffect(ActiveStateEffectHandle))
		{
			UE_LOG(LogWuwaCombat,
			       Error,
			       TEXT("硬直清理无法移除唯一状态效果。Avatar=%s, ASC=%s, Handle=%s"),
			       *GetNameSafe(ActiveAvatar.Get()),
			       *GetNameSafe(AbilitySystemComponent),
			       *ActiveStateEffectHandle.ToString());
		}
		ActiveStateEffectHandle.Invalidate();
	}

	if (bResetPoise && !ApplyPoiseReset())
	{
		LastEndReason = EWuwaStaggerEndReason::ResetEffectFailed;
	}

	if (IsValid(AbilitySystemComponent) &&
	    (AbilitySystemComponent->HasMatchingGameplayTag(WuwaGameplayTags::State_Combat_Staggered) ||
	     AbilitySystemComponent->HasMatchingGameplayTag(WuwaGameplayTags::Block_Input_Move)))
	{
		UE_LOG(LogWuwaCombat,
		       Warning,
		       TEXT("硬直清理后 ASC 仍有控制标签。Avatar=%s, Staggered=%d, MoveBlock=%d"),
		       *GetNameSafe(ActiveAvatar.Get()),
		       AbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::State_Combat_Staggered),
		       AbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::Block_Input_Move));
	}

	++CleanupCount;
	bCleanupComplete = true;
	bCleanupInProgress = false;
	ActiveAvatar.Reset();
	ActiveAbilitySystemComponent.Reset();
	ActiveMesh.Reset();
}

void UWuwaGameplayAbility_Stagger::FinishStagger(const EWuwaStaggerEndReason Reason, const bool bCancelled)
{
	if (bFinishRequested || !IsActive())
	{
		return;
	}

	bFinishRequested = true;
	LastEndReason = Reason;
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, bCancelled);
}

void UWuwaGameplayAbility_Stagger::HandleMontageCompleted()
{
	if (bRestartingMontage)
	{
		return;
	}
	FinishStagger(EWuwaStaggerEndReason::Completed, false);
}

void UWuwaGameplayAbility_Stagger::HandleMontageInterrupted()
{
	if (bRestartingMontage)
	{
		return;
	}
	FinishStagger(EWuwaStaggerEndReason::Interrupted, true);
}

void UWuwaGameplayAbility_Stagger::HandleMontageCancelled()
{
	if (bRestartingMontage)
	{
		return;
	}
	FinishStagger(EWuwaStaggerEndReason::Cancelled, true);
}

void UWuwaGameplayAbility_Stagger::HandleHealthChangedFact(const FWuwaHealthChangedFact& Fact)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UWuwaAbilitySystemComponent* CurrentAbilitySystemComponent =
	    ActorInfo != nullptr ? Cast<UWuwaAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	UWuwaAbilitySystemComponent* AuthorityAbilitySystemComponent = ActiveAbilitySystemComponent.Get();
	AActor* Avatar = ActiveAvatar.Get();
	const bool bCommonRefresh = IsActive() && !bFinishRequested && !bCleanupInProgress && !Fact.bInitialSync &&
	                            Fact.AffectedActor.Get() == Avatar && FMath::IsFinite(Fact.PreviousHealth) &&
	                            FMath::IsFinite(Fact.CurrentHealth) && Fact.CurrentHealth > 0.f &&
	                            Fact.CurrentHealth < Fact.PreviousHealth;
	const bool bAuthorityRefresh =
	    bCommonRefresh && IsValid(Avatar) && Avatar->HasAuthority() && IsValid(AuthorityAbilitySystemComponent) &&
	    AuthorityAbilitySystemComponent == CurrentAbilitySystemComponent &&
	    AuthorityAbilitySystemComponent->GetAvatarActor() == Avatar &&
	    !AuthorityAbilitySystemComponent->HasMatchingGameplayTag(WuwaGameplayTags::State_Combat_Dead);
	const bool bOwningClientPresentationRefresh =
	    bCommonRefresh && IsOwningClientPresentationRuntime() && IsValid(CurrentAbilitySystemComponent) &&
	    CurrentAbilitySystemComponent->GetAvatarActor() == Avatar &&
	    !CurrentAbilitySystemComponent->HasMatchingGameplayTag(WuwaGameplayTags::State_Combat_Dead);
	if (!bAuthorityRefresh && !bOwningClientPresentationRefresh)
	{
		return;
	}

	if (!RestartStaggerMontage())
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("硬直收到有效刷新事实但无法重播 Montage。Avatar=%s, Previous=%.2f, Current=%.2f, Authority=%s"),
		       *GetNameSafe(Avatar),
		       Fact.PreviousHealth,
		       Fact.CurrentHealth,
		       bAuthorityRefresh ? TEXT("true") : TEXT("false"));
		if (bAuthorityRefresh)
		{
			FinishStagger(EWuwaStaggerEndReason::MontageFailed, true);
		}
		else
		{
			LastEndReason = EWuwaStaggerEndReason::MontageFailed;
			EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), false, true);
		}
	}
}
