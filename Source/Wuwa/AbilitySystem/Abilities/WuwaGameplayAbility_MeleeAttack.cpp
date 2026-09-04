// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Abilities/WuwaGameplayAbility_MeleeAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/Runtime/WuwaAbilitySystemComponent.h"
#include "AbilitySystem/Runtime/WuwaPawnAbilityInitComponent.h"
#include "Animation/AnimInstance.h"
#include "Combat/Attributes/WuwaHealthSet.h"
#include "Combat/Attributes/WuwaPoiseSet.h"
#include "Combat/Attributes/WuwaResourceSet.h"
#include "Combat/Contracts/WuwaDamageReceiverInterface.h"
#include "Combat/Health/WuwaHealthComponent.h"
#include "Combat/Poise/WuwaPoiseComponent.h"
#include "Combat/Data/WuwaCombatProfile.h"
#include "Combat/Data/WuwaMeleeAttackDefinition.h"
#include "Combat/Data/WuwaWeaponDefinition.h"
#include "Combat/Runtime/WuwaCombatExecutionComponent.h"
#include "Combat/Runtime/WuwaWeaponComponent.h"
#include "Combat/WuwaCombatLog.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/WuwaGameplayTags.h"
#include "Core/WuwaStateTagComponent.h"
#include "GameplayEffect.h"
#include "WuwaCharacter.h"

UWuwaGameplayAbility_MeleeAttack::UWuwaGameplayAbility_MeleeAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(WuwaGameplayTags::Ability_Combat);
	AssetTags.AddTag(WuwaGameplayTags::Ability_Combat_Attack);
	AssetTags.AddTag(WuwaGameplayTags::Ability_Combat_Attack_Light);
	SetAssetTags(AssetTags);
	WuwaBlockedStateTags.AddTag(WuwaGameplayTags::State_Combat_Dead);
	WuwaBlockedStateTags.AddTag(WuwaGameplayTags::State_Combat_Staggered);
	WuwaBlockedStateTags.AddTag(WuwaGameplayTags::State_Action_ExclusiveActive);

	InterruptPolicy.bBlockMoveWhileActive = true;
	InterruptPolicy.bAllowMoveInterrupt = true;

	InterruptPolicy.bBlockActionWhileActive = true;
	InterruptPolicy.bAllowActionInterrupt = true;

	//InterruptPolicy.AllowedInterruptActionTags.AddTag(WuwaGameplayTags::Action_Movement_Dash_Forward);

	//InterruptPolicy.AllowedInterruptActionTags.AddTag(WuwaGameplayTags::Action_Movement_Backstep);
}

FWuwaMeleeAttackRuntimeSnapshot UWuwaGameplayAbility_MeleeAttack::GetRuntimeSnapshot() const
{
	FWuwaMeleeAttackRuntimeSnapshot Snapshot;
	Snapshot.bActive = IsActive();
	Snapshot.AttackTag = WuwaGameplayTags::Ability_Combat_Attack_Light;
	Snapshot.AbilitySpecHandle = LastAbilitySpecHandle;
	Snapshot.PredictionKeySummary = LastPredictionKeySummary;
	Snapshot.AttackDefinitionName = LastAttackDefinitionName;
	Snapshot.MontageName = LastMontageName;
	Snapshot.WindowState = WindowState;
	Snapshot.CurrentStepIndex = CurrentStepIndex;
	Snapshot.ExecutedStepCount = ExecutedStepCount;
	Snapshot.CurrentSection = CurrentSection;
	Snapshot.NextSection = NextSection;
	Snapshot.bComboWindowOpen = bComboWindowOpen;
	Snapshot.bBufferedNextStep = bBufferedNextStep;
	Snapshot.bTransitionCommitted = bTransitionCommitted;
	Snapshot.BufferedInputCount = bBufferedNextStep ? 1 : 0;
	Snapshot.AcceptedComboInputCount = AcceptedComboInputCount;
	Snapshot.RejectedComboInputCount = RejectedComboInputCount;
	Snapshot.LastComboInputRejectReason = LastComboInputRejectReason;
	Snapshot.PerStepHitWindowBeginCounts = PerStepHitWindowBeginCounts;
	Snapshot.PerStepHitWindowEndCounts = PerStepHitWindowEndCounts;
	Snapshot.PerStepHitFactCounts = PerStepHitFactCounts;
	Snapshot.PerStepDamageApplicationCounts = PerStepDamageApplicationCounts;
	Snapshot.PerStepCombatWindowHandles = PerStepCombatWindowHandles;
	Snapshot.HitWindowBeginCount = HitWindowBeginCount;
	Snapshot.HitWindowEndCount = HitWindowEndCount;
	Snapshot.LastEndReason = LastEndReason;
	Snapshot.bIsPlaceholder = IsValid(AttackDefinition) && AttackDefinition->IsPlaceholder();
	Snapshot.CombatWindowHandle =
	    ActiveCombatWindowHandle.IsValid() ? ActiveCombatWindowHandle.Value : LastCombatWindowHandleValue;
	Snapshot.ReceivedHitFactCount = ReceivedHitFactCount;
	Snapshot.RejectedHitFactCount = RejectedHitFactCount;
	Snapshot.LastCombatHitTargetName = LastCombatHitTargetName;
	Snapshot.DamageApplicationCount = SuccessfulDamageApplicationCount;
	Snapshot.RejectedDamageApplicationCount = RejectedDamageApplicationCount;
	if (const UWuwaCombatExecutionComponent* CombatExecutionComponent = ResolveCombatExecutionComponent())
	{
		Snapshot.SweepCount = CombatExecutionComponent->GetRuntimeSnapshot().SweepCount;
	}
	return Snapshot;
}

bool UWuwaGameplayAbility_MeleeAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                                          const FGameplayAbilityActorInfo* ActorInfo,
                                                          const FGameplayTagContainer* SourceTags,
                                                          const FGameplayTagContainer* TargetTags,
                                                          FGameplayTagContainer* OptionalRelevantTags) const
{
	const bool bSuperCanActivate =
	    Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
	const bool bActivationContextValid = ValidateActivationContext(ActorInfo);
	return bSuperCanActivate && bActivationContextValid;
}

void UWuwaGameplayAbility_MeleeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                                       const FGameplayAbilityActorInfo* ActorInfo,
                                                       const FGameplayAbilityActivationInfo ActivationInfo,
                                                       const FGameplayEventData* TriggerEventData)
{
	(void)TriggerEventData;

	WindowState = EWuwaMeleeAttackWindowState::Activating;
	LastEndReason = EWuwaMeleeAttackEndReason::None;
	CurrentStepIndex = INDEX_NONE;
	ExecutedStepCount = 0;
	bHasFrozenCurrentStep = false;
	CurrentSection = NAME_None;
	NextSection = NAME_None;
	bComboWindowOpen = false;
	bBufferedNextStep = false;
	bTransitionCommitted = false;
	AcceptedComboInputCount = 0;
	RejectedComboInputCount = 0;
	LastComboInputRejectReason = EWuwaMeleeComboInputRejectReason::None;
	PerStepHitWindowBeginCounts.Init(0, UWuwaMeleeAttackDefinition::RequiredStepCount);
	PerStepHitWindowEndCounts.Init(0, UWuwaMeleeAttackDefinition::RequiredStepCount);
	PerStepHitFactCounts.Init(0, UWuwaMeleeAttackDefinition::RequiredStepCount);
	PerStepDamageApplicationCounts.Init(0, UWuwaMeleeAttackDefinition::RequiredStepCount);
	PerStepCombatWindowHandles.Init(FWuwaCombatWindowHandle(), UWuwaMeleeAttackDefinition::RequiredStepCount);
	HitWindowBeginCount = 0;
	HitWindowEndCount = 0;
	ActiveCombatWindowHandle.Reset();
	LastCombatWindowHandleValue = 0;
	ReceivedHitFactCount = 0;
	RejectedHitFactCount = 0;
	SuccessfulDamageApplicationCount = 0;
	RejectedDamageApplicationCount = 0;
	ReceivedHitTargets.Reset();
	LastCombatHitTargetName.Reset();
	bEndRequested = false;
	bCleanupInProgress = false;
	LastAbilitySpecHandle = Handle.ToString();
	LastPredictionKeySummary = ActivationInfo.GetActivationPredictionKey().ToString();

	if (!ValidateActivationContext(ActorInfo))
	{
		LastEndReason = EWuwaMeleeAttackEndReason::MontageFailed;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AWuwaCharacter* Character = CastChecked<AWuwaCharacter>(ActorInfo->AvatarActor.Get());
	ActiveMesh = Character->GetMesh();
	ActiveMontage = AttackDefinition->GetMontage();
	if (!FreezeCurrentStep(0))
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("轻攻击激活失败：无法冻结 Step 0。Definition=%s"),
		       *GetNameSafe(AttackDefinition));
		LastEndReason = EWuwaMeleeAttackEndReason::MontageFailed;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	LastAttackDefinitionName = GetNameSafe(AttackDefinition);
	LastMontageName = GetNameSafe(ActiveMontage.Get());

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		LastEndReason = EWuwaMeleeAttackEndReason::CommitFailed;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!BeginInterruptRuntime(ActiveMontage.Get()))
	{
		LastEndReason = EWuwaMeleeAttackEndReason::MontageFailed;

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);

		return;
	}

	UWuwaAbilitySystemComponent* AbilitySystemComponent = GetWuwaAbilitySystemComponentFromActorInfo();
	if (!IsValid(AbilitySystemComponent))
	{
		LastEndReason = EWuwaMeleeAttackEndReason::AvatarChanged;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	AbilitySystemComponent->AddLooseGameplayTag(
	    WuwaGameplayTags::State_Combat_Attacking, 1, EGameplayTagReplicationState::TagOnly);
	bOwnsAttackingGameplayTag = true;

	if (!BindCombatHitFactDelegate())
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("轻攻击激活失败：Authority 无法绑定 Combat Hit Fact。Avatar=%s"),
		       *GetNameSafe(Character));
		LastEndReason = EWuwaMeleeAttackEndReason::AvatarChanged;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!StartHitWindowEventTasks() || !StartComboWindowEventTasks())
	{
		UE_LOG(LogWuwaCombat, Error, TEXT("轻攻击激活失败：无法创建完整分段事件任务。Ability=%s"), *GetNameSafe(this));
		LastEndReason = EWuwaMeleeAttackEndReason::HitWindowEventRejected;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this,
	                                                                             FName(TEXT("SwordLightMontage")),
	                                                                             ActiveMontage.Get(),
	                                                                             AttackDefinition->GetPlayRate(),
	                                                                             CurrentSection,
	                                                                             true,
	                                                                             0.0f,
	                                                                             0.0f,
	                                                                             true);
	if (!IsValid(MontageTask))
	{
		LastEndReason = EWuwaMeleeAttackEndReason::MontageFailed;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageCancelled);
	WindowState = EWuwaMeleeAttackWindowState::WaitingForWindowBegin;
	MontageTask->ReadyForActivation();
	if (!IsActive())
	{
		return;
	}
}

void UWuwaGameplayAbility_MeleeAttack::InputPressed(const FGameplayAbilitySpecHandle Handle,
                                                    const FGameplayAbilityActorInfo* ActorInfo,
                                                    const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);
	if (!IsActive() || bEndRequested || bComboWindowOpen || CurrentStepIndex < 0)
	{
		return;
	}

	const EWuwaMeleeComboInputRejectReason Reason =
	    CurrentStepIndex >= UWuwaMeleeAttackDefinition::RequiredStepCount - 1
	        ? EWuwaMeleeComboInputRejectReason::LastStep
	        : EWuwaMeleeComboInputRejectReason::OutsideWindow;
	RecordComboInputRejected(Reason);
}

void UWuwaGameplayAbility_MeleeAttack::CancelAbility(const FGameplayAbilitySpecHandle Handle,
                                                     const FGameplayAbilityActorInfo* ActorInfo,
                                                     const FGameplayAbilityActivationInfo ActivationInfo,
                                                     const bool bReplicateCancelAbility)
{
	if (IsActive() && CanBeCanceled())
	{
		bEndRequested = true;
		LastEndReason = EWuwaMeleeAttackEndReason::Cancelled;
	}
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

void UWuwaGameplayAbility_MeleeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle,
                                                  const FGameplayAbilityActorInfo* ActorInfo,
                                                  const FGameplayAbilityActivationInfo ActivationInfo,
                                                  const bool bReplicateEndAbility,
                                                  const bool bWasCancelled)
{
	bEndRequested = true;
	if (LastEndReason == EWuwaMeleeAttackEndReason::None)
	{
		LastEndReason = bWasCancelled ? EWuwaMeleeAttackEndReason::Cancelled : EWuwaMeleeAttackEndReason::Completed;
	}
	CleanupAttackState();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UWuwaGameplayAbility_MeleeAttack::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo,
                                                       const FGameplayAbilitySpec& Spec)
{
	bEndRequested = true;
	LastEndReason = EWuwaMeleeAttackEndReason::AbilityRemoved;
	CleanupAttackState();
	Super::OnRemoveAbility(ActorInfo, Spec);
}

bool UWuwaGameplayAbility_MeleeAttack::ValidateActivationContext(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const AWuwaCharacter* Character =
	    ActorInfo != nullptr ? Cast<AWuwaCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UWuwaPawnAbilityInitComponent* InitComponent =
	    IsValid(Character) ? Character->GetPawnAbilityInitComponent() : nullptr;
	const UWuwaCombatProfile* CombatProfile = IsValid(InitComponent) ? InitComponent->GetCombatProfile() : nullptr;
	const USkeletalMeshComponent* Mesh = IsValid(Character) ? Character->GetMesh() : nullptr;
	const UAnimInstance* AnimInstance = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
	const UWuwaCombatExecutionComponent* CombatExecutionComponent =
	    IsValid(Character) ? Character->GetCombatExecutionComponent() : nullptr;

	return IsValid(Character) && IsValid(InitComponent) && InitComponent->IsGameplayReady() && IsValid(CombatProfile) &&
	       CombatProfile->GetDefaultAttackDefinition() == AttackDefinition && IsValid(AttackDefinition) &&
	       AttackDefinition->IsRuntimeValid() && IsValid(DamageEffectClass) && IsValid(CombatExecutionComponent) &&
	       CombatExecutionComponent->IsInitialized() && IsValid(Mesh) && IsValid(AnimInstance);
}

bool UWuwaGameplayAbility_MeleeAttack::StartHitWindowEventTasks()
{
	HitWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
	    this, WuwaGameplayTags::Event_Combat_HitWindow_Begin, nullptr, false, true);
	HitWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
	    this, WuwaGameplayTags::Event_Combat_HitWindow_End, nullptr, false, true);

	if (!IsValid(HitWindowBeginTask) || !IsValid(HitWindowEndTask))
	{
		return false;
	}

	if (IsValid(HitWindowBeginTask))
	{
		HitWindowBeginTask->EventReceived.AddDynamic(this, &ThisClass::HandleHitWindowBegin);
		HitWindowBeginTask->ReadyForActivation();
	}
	if (IsValid(HitWindowEndTask))
	{
		HitWindowEndTask->EventReceived.AddDynamic(this, &ThisClass::HandleHitWindowEnd);
		HitWindowEndTask->ReadyForActivation();
	}
	return true;
}

bool UWuwaGameplayAbility_MeleeAttack::StartComboWindowEventTasks()
{
	ComboWindowBeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
	    this, WuwaGameplayTags::Event_Combat_ComboWindow_Begin, nullptr, false, true);
	ComboWindowEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
	    this, WuwaGameplayTags::Event_Combat_ComboWindow_End, nullptr, false, true);
	if (!IsValid(ComboWindowBeginTask) || !IsValid(ComboWindowEndTask))
	{
		return false;
	}

	ComboWindowBeginTask->EventReceived.AddDynamic(this, &ThisClass::HandleComboWindowBegin);
	ComboWindowEndTask->EventReceived.AddDynamic(this, &ThisClass::HandleComboWindowEnd);
	ComboWindowBeginTask->ReadyForActivation();
	ComboWindowEndTask->ReadyForActivation();
	return true;
}

bool UWuwaGameplayAbility_MeleeAttack::StartComboInputTask()
{
	if (IsValid(ComboInputTask))
	{
		return true;
	}
	if (!bComboWindowOpen || CurrentStepIndex < 0 ||
	    CurrentStepIndex >= UWuwaMeleeAttackDefinition::RequiredStepCount - 1)
	{
		return false;
	}

	ComboInputTask = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
	if (!IsValid(ComboInputTask))
	{
		return false;
	}
	ComboInputTask->OnPress.AddDynamic(this, &ThisClass::HandleComboInputPressed);
	ComboInputTask->ReadyForActivation();
	return true;
}

bool UWuwaGameplayAbility_MeleeAttack::IsCurrentStepEvent(const FGameplayEventData& Payload) const
{
	const UWuwaAbilitySystemComponent* AbilitySystemComponent = GetWuwaAbilitySystemComponentFromActorInfo();
	return IsCurrentStepEndEvent(Payload) && IsValid(AbilitySystemComponent) &&
	       AbilitySystemComponent->GetCurrentMontage() == ActiveMontage.Get() && ActiveMontage.IsValid();
}

bool UWuwaGameplayAbility_MeleeAttack::IsCurrentStepEndEvent(const FGameplayEventData& Payload) const
{
	const AWuwaCharacter* Character = GetWuwaCharacterFromActorInfo();
	const UWuwaAbilitySystemComponent* AbilitySystemComponent = GetWuwaAbilitySystemComponentFromActorInfo();
	const UAnimMontage* ObservedMontage =
	    IsValid(AbilitySystemComponent) ? AbilitySystemComponent->GetCurrentMontage() : nullptr;
	return IsActive() && IsValid(Character) && IsValid(AbilitySystemComponent) && ActiveMesh.IsValid() &&
	       ActiveMontage.IsValid() && (ObservedMontage == nullptr || ObservedMontage == ActiveMontage.Get()) &&
	       Payload.Instigator == Character && Payload.Target == Character &&
	       Payload.OptionalObject == ActiveMontage.Get() && Payload.OptionalObject2 == ActiveMesh.Get();
}

bool UWuwaGameplayAbility_MeleeAttack::DecodeStepIndex(const FGameplayEventData& Payload, int32& OutStepIndex)
{
	OutStepIndex = INDEX_NONE;
	if (!FMath::IsFinite(Payload.EventMagnitude) || Payload.EventMagnitude < 1.0f ||
	    Payload.EventMagnitude > static_cast<float>(UWuwaMeleeAttackDefinition::RequiredStepCount))
	{
		return false;
	}

	const int32 EncodedStep = FMath::RoundToInt(Payload.EventMagnitude);
	if (!FMath::IsNearlyEqual(Payload.EventMagnitude, static_cast<float>(EncodedStep)))
	{
		return false;
	}
	OutStepIndex = EncodedStep - 1;
	return true;
}

bool UWuwaGameplayAbility_MeleeAttack::TryEnterStepFromEvent(const int32 EventStepIndex)
{
	UWuwaAbilitySystemComponent* AbilitySystemComponent = GetWuwaAbilitySystemComponentFromActorInfo();
	const FWuwaMeleeAttackStep* EventStep =
	    IsValid(AttackDefinition) ? AttackDefinition->GetStep(EventStepIndex) : nullptr;
	if (!IsValid(AbilitySystemComponent) || AbilitySystemComponent->GetCurrentMontage() != ActiveMontage.Get() ||
	    EventStep == nullptr)
	{
		return false;
	}

	const FName ActualSection = AbilitySystemComponent->GetCurrentMontageSectionName();
	if (ActualSection != EventStep->MontageSection)
	{
		return false;
	}
	if (EventStepIndex == CurrentStepIndex)
	{
		return true;
	}
	if (EventStepIndex != CurrentStepIndex + 1 || !bBufferedNextStep || !bTransitionCommitted ||
	    ActiveCombatWindowHandle.IsValid() || WindowState != EWuwaMeleeAttackWindowState::WindowClosed)
	{
		return false;
	}
	if (!FreezeCurrentStep(EventStepIndex))
	{
		return false;
	}

	ReceivedHitTargets.Reset();
	bComboWindowOpen = false;
	bBufferedNextStep = false;
	bTransitionCommitted = false;
	NextSection = NAME_None;
	WindowState = EWuwaMeleeAttackWindowState::WaitingForWindowBegin;

	return true;
}

bool UWuwaGameplayAbility_MeleeAttack::FreezeCurrentStep(const int32 StepIndex)
{
	const FWuwaMeleeAttackStep* Step = IsValid(AttackDefinition) ? AttackDefinition->GetStep(StepIndex) : nullptr;
	if (Step == nullptr || Step->MontageSection.IsNone() || !FMath::IsFinite(Step->BaseDamage) ||
	    Step->BaseDamage < 0.0f)
	{
		return false;
	}

	FrozenCurrentStep = *Step;
	bHasFrozenCurrentStep = true;
	CurrentStepIndex = StepIndex;
	ExecutedStepCount = FMath::Max(ExecutedStepCount, StepIndex + 1);
	CurrentSection = FrozenCurrentStep.MontageSection;
	return true;
}

void UWuwaGameplayAbility_MeleeAttack::RecordComboInputRejected(const EWuwaMeleeComboInputRejectReason Reason)
{
	++RejectedComboInputCount;
	LastComboInputRejectReason = Reason;
}

bool UWuwaGameplayAbility_MeleeAttack::ValidateCompletedStepWindows() const
{
	if (!IsValid(AttackDefinition) || ExecutedStepCount < 1 ||
	    ExecutedStepCount > UWuwaMeleeAttackDefinition::RequiredStepCount ||
	    PerStepHitWindowBeginCounts.Num() != UWuwaMeleeAttackDefinition::RequiredStepCount ||
	    PerStepHitWindowEndCounts.Num() != UWuwaMeleeAttackDefinition::RequiredStepCount)
	{
		return false;
	}

	for (int32 StepIndex = 0; StepIndex < UWuwaMeleeAttackDefinition::RequiredStepCount; ++StepIndex)
	{
		int32 ExpectedCount = 0;

		// 只有真正执行到的 Step 才应该产生 HitWindow
		if (StepIndex < ExecutedStepCount)
		{
			const FWuwaMeleeAttackStep* Step = AttackDefinition->GetStep(StepIndex);

			if (Step == nullptr || Step->HitWindowCount <= 0)
			{
				return false;
			}

			ExpectedCount = Step->HitWindowCount;
		}

		if (PerStepHitWindowBeginCounts[StepIndex] != ExpectedCount ||
		    PerStepHitWindowEndCounts[StepIndex] != ExpectedCount)
		{
			return false;
		}
	}

	return true;
}

UWuwaCombatExecutionComponent* UWuwaGameplayAbility_MeleeAttack::ResolveCombatExecutionComponent() const
{
	AWuwaCharacter* Character = GetWuwaCharacterFromActorInfo();
	return IsValid(Character) ? Character->GetCombatExecutionComponent() : nullptr;
}

bool UWuwaGameplayAbility_MeleeAttack::BuildMeleeWindowRequest(FWuwaMeleeWindowRequest& OutRequest) const
{
	OutRequest = FWuwaMeleeWindowRequest();
	AWuwaCharacter* Character = GetWuwaCharacterFromActorInfo();
	const UWuwaWeaponComponent* WeaponComponent = IsValid(Character) ? Character->GetWeaponComponent() : nullptr;
	const UWuwaWeaponDefinition* WeaponDefinition =
	    IsValid(WeaponComponent) ? WeaponComponent->GetWeaponDefinition() : nullptr;
	if (!IsValid(Character) || !IsValid(AttackDefinition) || !AttackDefinition->IsRuntimeValid() ||
	    !bHasFrozenCurrentStep || CurrentStepIndex < 0 || !IsValid(WeaponComponent) ||
	    !WeaponComponent->IsInitialized() || !IsValid(WeaponDefinition))
	{
		return false;
	}

	OutRequest.SourceActor = Character;
	OutRequest.InstigatorActor = Character;
	OutRequest.AttackTag = AttackDefinition->GetAttackTag();
	OutRequest.StepIndex = static_cast<uint8>(CurrentStepIndex);
	OutRequest.TraceRadius = FrozenCurrentStep.TraceSpec.TraceRadius;
	OutRequest.BladeSampleCount = FrozenCurrentStep.TraceSpec.BladeSampleCount;
	OutRequest.MaxTargets = FrozenCurrentStep.MaxTargets;
	OutRequest.TraceBaseSocket = WeaponDefinition->GetTraceBaseSocket();
	OutRequest.TraceTipSocket = WeaponDefinition->GetTraceTipSocket();
	OutRequest.bAllowMultipleTargets = FrozenCurrentStep.bAllowMultipleTargets;
	return true;
}

bool UWuwaGameplayAbility_MeleeAttack::OpenAuthorityCombatWindow()
{
	AWuwaCharacter* Character = GetWuwaCharacterFromActorInfo();
	if (!IsValid(Character) || !Character->HasAuthority())
	{
		return IsValid(Character);
	}

	if (ActiveCombatWindowHandle.IsValid())
	{
		return false;
	}

	UWuwaCombatExecutionComponent* CombatExecutionComponent = ResolveCombatExecutionComponent();
	FWuwaMeleeWindowRequest Request;
	if (!IsValid(CombatExecutionComponent) || !CombatExecutionComponent->IsInitialized() ||
	    !BuildMeleeWindowRequest(Request))
	{
		return false;
	}

	ActiveCombatWindowHandle = CombatExecutionComponent->BeginMeleeWindow(Request);
	LastCombatWindowHandleValue = ActiveCombatWindowHandle.Value;
	if (ActiveCombatWindowHandle.IsValid() && PerStepCombatWindowHandles.IsValidIndex(CurrentStepIndex))
	{
		PerStepCombatWindowHandles[CurrentStepIndex] = ActiveCombatWindowHandle;
	}
	return ActiveCombatWindowHandle.IsValid();
}

bool UWuwaGameplayAbility_MeleeAttack::CloseAuthorityCombatWindow(const EWuwaCombatWindowEndReason Reason)
{
	if (!ActiveCombatWindowHandle.IsValid())
	{
		return true;
	}

	UWuwaCombatExecutionComponent* CombatExecutionComponent = ResolveCombatExecutionComponent();
	const FWuwaCombatWindowHandle Handle = ActiveCombatWindowHandle;
	ActiveCombatWindowHandle.Reset();
	if (!IsValid(CombatExecutionComponent))
	{
		UE_LOG(
		    LogWuwaCombat, Error, TEXT("轻攻击无法关闭 CombatWindow：执行组件已失效。WindowHandle=%u"), Handle.Value);
		return false;
	}

	return CombatExecutionComponent->EndMeleeWindow(Handle, Reason);
}

bool UWuwaGameplayAbility_MeleeAttack::BindCombatHitFactDelegate()
{
	AWuwaCharacter* Character = GetWuwaCharacterFromActorInfo();
	if (!IsValid(Character) || !Character->HasAuthority())
	{
		return IsValid(Character);
	}

	UnbindCombatHitFactDelegate();
	UWuwaCombatExecutionComponent* CombatExecutionComponent = ResolveCombatExecutionComponent();
	if (!IsValid(CombatExecutionComponent) || !CombatExecutionComponent->IsInitialized())
	{
		return false;
	}

	CombatHitFactDelegateHandle =
	    CombatExecutionComponent->OnCombatHitFact().AddUObject(this, &ThisClass::HandleCombatHitFact);
	return CombatHitFactDelegateHandle.IsValid();
}

void UWuwaGameplayAbility_MeleeAttack::UnbindCombatHitFactDelegate()
{
	if (!CombatHitFactDelegateHandle.IsValid())
	{
		return;
	}

	if (UWuwaCombatExecutionComponent* CombatExecutionComponent = ResolveCombatExecutionComponent())
	{
		CombatExecutionComponent->OnCombatHitFact().Remove(CombatHitFactDelegateHandle);
	}
	CombatHitFactDelegateHandle.Reset();
}

bool UWuwaGameplayAbility_MeleeAttack::IsCurrentCombatHitFact(const FWuwaCombatHitFact& Fact) const
{
	const AWuwaCharacter* Character = GetWuwaCharacterFromActorInfo();
	return IsActive() && !bEndRequested && IsValid(Character) && Character->HasAuthority() &&
	       ActiveCombatWindowHandle.IsValid() && Fact.WindowHandle == ActiveCombatWindowHandle &&
	       Fact.StepIndex == static_cast<uint8>(CurrentStepIndex) && Fact.SourceActor.Get() == Character &&
	       Fact.TargetActor.IsValid() && Fact.TargetActor.Get() != Character;
}

void UWuwaGameplayAbility_MeleeAttack::HandleCombatHitFact(const FWuwaCombatHitFact& Fact)
{
	const TWeakObjectPtr<AActor> TargetKey(Fact.TargetActor.Get());
	const int32 EffectiveMaxTargets =
	    bHasFrozenCurrentStep && FrozenCurrentStep.bAllowMultipleTargets
	        ? FMath::Clamp(FrozenCurrentStep.MaxTargets, 1, WuwaCombatLimits::MaxMeleeTargets)
	        : 1;
	if (!IsCurrentCombatHitFact(Fact) || ReceivedHitTargets.Contains(TargetKey) ||
	    ReceivedHitTargets.Num() >= EffectiveMaxTargets)
	{
		++RejectedHitFactCount;
		return;
	}

	ReceivedHitTargets.Add(TargetKey);
	++ReceivedHitFactCount;
	if (PerStepHitFactCounts.IsValidIndex(CurrentStepIndex))
	{
		++PerStepHitFactCounts[CurrentStepIndex];
	}
	LastCombatHitTargetName = GetNameSafe(Fact.TargetActor.Get());
	const bool bDamageApplied = ApplyDamageToTarget(Fact);
	if (bDamageApplied)
	{
		++SuccessfulDamageApplicationCount;
		if (PerStepDamageApplicationCounts.IsValidIndex(CurrentStepIndex))
		{
			++PerStepDamageApplicationCounts[CurrentStepIndex];
		}
	}
	else
	{
		++RejectedDamageApplicationCount;
	}
}

bool UWuwaGameplayAbility_MeleeAttack::ResolveDamageTarget(const FWuwaCombatHitFact& Fact,
                                                           UWuwaAbilitySystemComponent*& OutTargetASC) const
{
	OutTargetASC = nullptr;
	AActor* TargetActor = Fact.TargetActor.Get();
	if (!IsValid(TargetActor) || TargetActor == GetWuwaCharacterFromActorInfo() || TargetActor->IsActorBeingDestroyed())
	{
		return false;
	}

	IWuwaDamageReceiverInterface* DamageReceiver = Cast<IWuwaDamageReceiverInterface>(TargetActor);
	if (DamageReceiver == nullptr)
	{
		return false;
	}

	UWuwaAbilitySystemComponent* TargetASC = DamageReceiver->GetDamageReceiverAbilitySystemComponent();
	UWuwaHealthComponent* TargetHealthComponent = DamageReceiver->GetDamageReceiverHealthComponent();
	UWuwaPoiseComponent* TargetPoiseComponent = DamageReceiver->GetDamageReceiverPoiseComponent();
	if (!IsValid(TargetASC) || TargetASC->GetAvatarActor() != TargetActor ||
	    !TargetASC->HasAttributeSetForAttribute(UWuwaHealthSet::GetHealthAttribute()) ||
	    !TargetASC->HasAttributeSetForAttribute(UWuwaPoiseSet::GetPoiseAttribute()) ||
	    !IsValid(TargetHealthComponent) || !TargetHealthComponent->IsInitialized() ||
	    TargetHealthComponent->IsDeadOrDying() || !IsValid(TargetPoiseComponent) ||
	    !TargetPoiseComponent->IsInitialized())
	{
		return false;
	}

	OutTargetASC = TargetASC;
	return true;
}

bool UWuwaGameplayAbility_MeleeAttack::ApplyDamageToTarget(const FWuwaCombatHitFact& Fact)
{
	UWuwaAbilitySystemComponent* SourceASC = GetWuwaAbilitySystemComponentFromActorInfo();
	UWuwaAbilitySystemComponent* TargetASC = nullptr;
	if (!IsValid(SourceASC) || !IsValid(AttackDefinition) || !bHasFrozenCurrentStep || !IsValid(DamageEffectClass) ||
	    !ResolveDamageTarget(Fact, TargetASC))
	{
		return false;
	}

	const float BaseDamage = FrozenCurrentStep.BaseDamage;
	const float PoiseDamage = FrozenCurrentStep.PoiseDamage;
	if (!FMath::IsFinite(BaseDamage) || BaseDamage < 0.f || !FMath::IsFinite(PoiseDamage) || PoiseDamage < 0.f)
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("轻攻击拒绝非法伤害输入。BaseDamage=%f, PoiseDamage=%f"),
		       BaseDamage,
		       PoiseDamage);
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(AttackDefinition);
	FGameplayEffectSpecHandle SpecHandle =
	    SourceASC->MakeOutgoingSpec(DamageEffectClass, GetAbilityLevel(), EffectContext);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(WuwaGameplayTags::Data_Damage_Base, BaseDamage);
	SpecHandle.Data->SetSetByCallerMagnitude(WuwaGameplayTags::Data_Damage_Poise, PoiseDamage);
	const FActiveGameplayEffectHandle Handle =
	    SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	if (!Handle.WasSuccessfullyApplied())
	{
		return false;
	}

	ExecuteAuthorityHitCue(Fact, *TargetASC);
	return true;
}

void UWuwaGameplayAbility_MeleeAttack::ExecuteAuthorityHitCue(const FWuwaCombatHitFact& Fact,
                                                              UWuwaAbilitySystemComponent& TargetASC) const
{
	if (!bHasFrozenCurrentStep || !FrozenCurrentStep.HitCueTag.IsValid())
	{
		return;
	}

	FGameplayCueParameters Parameters;
	Parameters.Location = Fact.ImpactPoint;
	Parameters.Normal = Fact.ImpactNormal;
	Parameters.Instigator = GetWuwaCharacterFromActorInfo();
	Parameters.EffectCauser = GetWuwaCharacterFromActorInfo();
	Parameters.SourceObject = AttackDefinition;
	TargetASC.ExecuteGameplayCue(FrozenCurrentStep.HitCueTag, Parameters);
}

void UWuwaGameplayAbility_MeleeAttack::FinishAbility(const EWuwaMeleeAttackEndReason Reason, const bool bCancelled)
{
	if (bEndRequested || !IsActive())
	{
		return;
	}

	bEndRequested = true;
	LastEndReason = Reason;
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, bCancelled);
}

void UWuwaGameplayAbility_MeleeAttack::CleanupAttackState()
{
	if (bCleanupInProgress)
	{
		return;
	}

	bCleanupInProgress = true;
	WindowState = EWuwaMeleeAttackWindowState::Ending;
	if (!CloseAuthorityCombatWindow(EWuwaCombatWindowEndReason::AbilityCleanup))
	{
		UE_LOG(LogWuwaCombat, Error, TEXT("轻攻击清理未能关闭 Authority CombatWindow。Ability=%s"), *GetNameSafe(this));
	}
	UnbindCombatHitFactDelegate();

	if (IsValid(HitWindowBeginTask))
	{
		HitWindowBeginTask->EndTask();
	}
	if (IsValid(HitWindowEndTask))
	{
		HitWindowEndTask->EndTask();
	}
	if (IsValid(ComboInputTask))
	{
		ComboInputTask->EndTask();
	}
	if (IsValid(ComboWindowBeginTask))
	{
		ComboWindowBeginTask->EndTask();
	}
	if (IsValid(ComboWindowEndTask))
	{
		ComboWindowEndTask->EndTask();
	}
	if (IsValid(MontageTask))
	{
		MontageTask->EndTask();
	}
	HitWindowBeginTask = nullptr;
	HitWindowEndTask = nullptr;
	ComboInputTask = nullptr;
	ComboWindowBeginTask = nullptr;
	ComboWindowEndTask = nullptr;
	MontageTask = nullptr;

	AWuwaCharacter* Character = GetWuwaCharacterFromActorInfo();
	UWuwaStateTagComponent* StateTagComponent = nullptr;
	if (IsValid(Character))
	{
		/*
		UAnimInstance* AnimInstance = IsValid(Character->GetMesh()) ? Character->GetMesh()->GetAnimInstance() : nullptr;
		if (IsValid(AnimInstance) && ActiveMontage.IsValid() && AnimInstance->Montage_IsPlaying(ActiveMontage.Get()))
		{
			AnimInstance->Montage_Stop(0.1f, ActiveMontage.Get());
		}
		*/

		UWuwaAbilitySystemComponent* AbilitySystemComponent = GetWuwaAbilitySystemComponentFromActorInfo();
		if (IsValid(AbilitySystemComponent) && ActiveMontage.IsValid() &&
		    AbilitySystemComponent->GetCurrentMontage() == ActiveMontage.Get())
		{
			AbilitySystemComponent->CurrentMontageStop(0.05f);
		}

		StateTagComponent = Character->GetStateTagComponent();
	}
	UWuwaAbilitySystemComponent* AbilitySystemComponent = GetWuwaAbilitySystemComponentFromActorInfo();
	if (IsValid(AbilitySystemComponent) && bOwnsAttackingGameplayTag)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(
		    WuwaGameplayTags::State_Combat_Attacking, 1, EGameplayTagReplicationState::TagOnly);
	}
	bOwnsAttackingGameplayTag = false;
	ActiveMesh.Reset();
	ActiveMontage.Reset();
	CurrentStepIndex = INDEX_NONE;
	bHasFrozenCurrentStep = false;
	FrozenCurrentStep = FWuwaMeleeAttackStep();
	CurrentSection = NAME_None;
	NextSection = NAME_None;
	bComboWindowOpen = false;
	bBufferedNextStep = false;
	bTransitionCommitted = false;
	WindowState = EWuwaMeleeAttackWindowState::Idle;
	bCleanupInProgress = false;
}

void UWuwaGameplayAbility_MeleeAttack::HandleHitWindowBegin(FGameplayEventData Payload)
{
	int32 EventStepIndex = INDEX_NONE;
	if (!IsCurrentStepEvent(Payload))
	{
		return;
	}
	if (!DecodeStepIndex(Payload, EventStepIndex) || !TryEnterStepFromEvent(EventStepIndex))
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("轻攻击拒绝非法 HitWindow Begin。Ability=%s, EventMagnitude=%.3f"),
		       *GetNameSafe(this),
		       Payload.EventMagnitude);
		FinishAbility(EWuwaMeleeAttackEndReason::InvalidStepEvent, true);
		return;
	}
	const bool bCanOpenHitWindow = WindowState == EWuwaMeleeAttackWindowState::WaitingForWindowBegin ||
	                               WindowState == EWuwaMeleeAttackWindowState::WindowClosed;

	if (!bCanOpenHitWindow)
	{
		return;
	}

	// 一个 HitWindow = 一次新的独立伤害阶段
	ReceivedHitTargets.Reset();

	WindowState = EWuwaMeleeAttackWindowState::WindowOpen;
	++HitWindowBeginCount;
	if (PerStepHitWindowBeginCounts.IsValidIndex(CurrentStepIndex))
	{
		++PerStepHitWindowBeginCounts[CurrentStepIndex];
	}
	if (!OpenAuthorityCombatWindow())
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("轻攻击 HitWindow Begin 无法创建 Authority CombatWindow。Avatar=%s"),
		       *GetNameSafe(GetWuwaCharacterFromActorInfo()));
		FinishAbility(EWuwaMeleeAttackEndReason::HitWindowEventRejected, true);
		return;
	}
}

void UWuwaGameplayAbility_MeleeAttack::HandleHitWindowEnd(FGameplayEventData Payload)
{
	int32 EventStepIndex = INDEX_NONE;
	if (!IsCurrentStepEndEvent(Payload))
	{
		return;
	}
	if (!DecodeStepIndex(Payload, EventStepIndex))
	{
		FinishAbility(EWuwaMeleeAttackEndReason::InvalidStepEvent, true);
		return;
	}

	// Jump 后上一攻击段迟到的 End，直接丢弃
	if (EventStepIndex < CurrentStepIndex)
	{

		return;
	}

	if (EventStepIndex != CurrentStepIndex)
	{
		FinishAbility(EWuwaMeleeAttackEndReason::InvalidStepEvent, true);
		return;
	}
	if (WindowState != EWuwaMeleeAttackWindowState::WindowOpen)
	{
		return;
	}

	if (!CloseAuthorityCombatWindow(EWuwaCombatWindowEndReason::NotifyEnded))
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("轻攻击 HitWindow End 无法关闭 Authority CombatWindow。Avatar=%s"),
		       *GetNameSafe(GetWuwaCharacterFromActorInfo()));
		FinishAbility(EWuwaMeleeAttackEndReason::HitWindowEventRejected, true);
		return;
	}
	WindowState = EWuwaMeleeAttackWindowState::WindowClosed;
	++HitWindowEndCount;
	if (PerStepHitWindowEndCounts.IsValidIndex(CurrentStepIndex))
	{
		++PerStepHitWindowEndCounts[CurrentStepIndex];
	}
}

void UWuwaGameplayAbility_MeleeAttack::HandleComboInputPressed(const float TimeWaited)
{
	(void)TimeWaited;

	// 当前 WaitInputPress 已经完成。
	ComboInputTask = nullptr;

	if (!IsActive() || bEndRequested)
	{
		return;
	}

	// 最后一段不存在下一段
	if (CurrentStepIndex >= UWuwaMeleeAttackDefinition::RequiredStepCount - 1)
	{
		RecordComboInputRejected(EWuwaMeleeComboInputRejectReason::LastStep);
		return;
	}

	// 只有 ComboWindow 内输入才能取消后摇
	if (!bComboWindowOpen)
	{
		RecordComboInputRejected(EWuwaMeleeComboInputRejectReason::OutsideWindow);
		return;
	}

	// 已经提交过一次跳段，不接受第二次
	if (bBufferedNextStep)
	{
		RecordComboInputRejected(EWuwaMeleeComboInputRejectReason::BufferFull);
		return;
	}

	UWuwaAbilitySystemComponent* AbilitySystemComponent = GetWuwaAbilitySystemComponentFromActorInfo();

	const FWuwaMeleeAttackStep* NextStep =
	    IsValid(AttackDefinition) ? AttackDefinition->GetStep(CurrentStepIndex + 1) : nullptr;

	if (!IsValid(AbilitySystemComponent) || AbilitySystemComponent->GetCurrentMontage() != ActiveMontage.Get() ||
	    AbilitySystemComponent->GetCurrentMontageSectionName() != CurrentSection || NextStep == nullptr)
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("轻攻击无法立即切换下一 Section。"
		            "StepIndex=%d, CurrentSection=%s"),
		       CurrentStepIndex,
		       *CurrentSection.ToString());

		FinishAbility(EWuwaMeleeAttackEndReason::InvalidStepEvent, true);

		return;
	}

	if (WindowState != EWuwaMeleeAttackWindowState::WindowClosed || ActiveCombatWindowHandle.IsValid())
	{

		RecordComboInputRejected(EWuwaMeleeComboInputRejectReason::OutsideWindow);

		return;
	}

	// 授权下一段
	bBufferedNextStep = true;
	bTransitionCommitted = true;

	NextSection = NextStep->MontageSection;

	++AcceptedComboInputCount;

	LastComboInputRejectReason = EWuwaMeleeComboInputRejectReason::None;

	// 立即跳到下一 Section
	ResetInterruptWindowForTransition();
	AbilitySystemComponent->CurrentMontageJumpToSection(NextSection);

	// 下一攻击段进入自己的 ComboWindow 后，
	// HandleComboWindowBegin() 会重新创建输入 Task。
}

void UWuwaGameplayAbility_MeleeAttack::HandleComboWindowBegin(FGameplayEventData Payload)
{
	int32 EventStepIndex = INDEX_NONE;
	if (!IsCurrentStepEvent(Payload))
	{
		return;
	}
	if (!DecodeStepIndex(Payload, EventStepIndex) ||
	    EventStepIndex >= UWuwaMeleeAttackDefinition::RequiredStepCount - 1 || !TryEnterStepFromEvent(EventStepIndex))
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("轻攻击拒绝非法 ComboWindow Begin。CurrentStep=%d, EventMagnitude=%.3f"),
		       CurrentStepIndex,
		       Payload.EventMagnitude);
		FinishAbility(EWuwaMeleeAttackEndReason::InvalidStepEvent, true);
		return;
	}
	if (bComboWindowOpen)
	{
		return;
	}

	bComboWindowOpen = true;
	if (!StartComboInputTask())
	{
		UE_LOG(LogWuwaCombat, Error, TEXT("轻攻击 ComboWindow Begin 无法创建输入任务。StepIndex=%d"), CurrentStepIndex);
		FinishAbility(EWuwaMeleeAttackEndReason::InvalidStepEvent, true);
		return;
	}
}

void UWuwaGameplayAbility_MeleeAttack::HandleComboWindowEnd(FGameplayEventData Payload)
{
	int32 EventStepIndex = INDEX_NONE;
	if (!IsCurrentStepEndEvent(Payload))
	{
		return;
	}
	if (!DecodeStepIndex(Payload, EventStepIndex))
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("轻攻击拒绝无法解析的 ComboWindow End。"
		            "CurrentStep=%d, EventMagnitude=%.3f"),
		       CurrentStepIndex,
		       Payload.EventMagnitude);

		FinishAbility(EWuwaMeleeAttackEndReason::InvalidStepEvent, true);
		return;
	}

	// Immediate Section Jump 后，上一段的 NotifyEnd 可能延后到达。
	if (EventStepIndex < CurrentStepIndex)
	{

		return;
	}

	// 未来 Step 的 End 才是真正非法事件
	if (EventStepIndex != CurrentStepIndex)
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("轻攻击拒绝非法 ComboWindow End。"
		            "CurrentStep=%d, EventStep=%d"),
		       CurrentStepIndex,
		       EventStepIndex);

		FinishAbility(EWuwaMeleeAttackEndReason::InvalidStepEvent, true);

		return;
	}
	if (!bComboWindowOpen)
	{
		return;
	}

	bComboWindowOpen = false;
	if (IsValid(ComboInputTask))
	{
		ComboInputTask->EndTask();
		ComboInputTask = nullptr;
	}
}

void UWuwaGameplayAbility_MeleeAttack::HandleMontageCompleted()
{
	if (WindowState != EWuwaMeleeAttackWindowState::WindowClosed || bComboWindowOpen || bBufferedNextStep ||
	    bTransitionCommitted || !ValidateCompletedStepWindows())
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("轻攻击 Montage 完成但分段生命周期不完整。Executed=%d, Begin=%d, End=%d, State=%d, ComboOpen=%s, "
		            "Buffered=%s"),
		       ExecutedStepCount,
		       HitWindowBeginCount,
		       HitWindowEndCount,
		       static_cast<int32>(WindowState),
		       bComboWindowOpen ? TEXT("true") : TEXT("false"),
		       bBufferedNextStep ? TEXT("true") : TEXT("false"));
		FinishAbility(EWuwaMeleeAttackEndReason::HitWindowEventRejected, true);
		return;
	}
	FinishAbility(EWuwaMeleeAttackEndReason::Completed, false);
}

void UWuwaGameplayAbility_MeleeAttack::HandleMontageInterrupted()
{
	FinishAbility(EWuwaMeleeAttackEndReason::Interrupted, true);
}

void UWuwaGameplayAbility_MeleeAttack::HandleMontageCancelled()
{
	FinishAbility(EWuwaMeleeAttackEndReason::Cancelled, true);
}
