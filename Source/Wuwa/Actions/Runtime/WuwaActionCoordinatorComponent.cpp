#include "Actions/Runtime/WuwaActionCoordinatorComponent.h"

#include "AbilitySystem/Contracts/WuwaAbilityTypes.h"
#include "AbilitySystem/Interop/WuwaActionAbilityInteropComponent.h"
#include "Actions/Contracts/WuwaActionCapability.h"
#include "Actions/Data/WuwaActionDefinition.h"
#include "Core/WuwaGameplayTags.h"
#include "Core/WuwaStateTagComponent.h"
#include "Engine/World.h"
#include "Templates/UnrealTemplate.h"
#include "Wuwa.h"

void FWuwaActiveActionInstance::ResetAfterCleanup()
{
	Handle = FWuwaActionHandle();
	Request = FWuwaActionRequest();
	CommittedCapabilities.Reset();
	GrantedTagHandles.Reset();
	ExclusiveStateTagHandle.Reset();
	State = EWuwaActionInstanceState::None;
}

UWuwaActionCoordinatorComponent::UWuwaActionCoordinatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UWuwaActionCoordinatorComponent::Initialize(UWuwaStateTagComponent* InStateTagComponent)
{
	if (!IsValid(InStateTagComponent))
	{
		StateTagComponent = nullptr;

		UE_LOG(LogWuwa, Error, TEXT("Action Coordinator 初始化失败，缺少状态组件。Owner=%s"), *GetNameSafe(GetOwner()));
		return false;
	}

	StateTagComponent = InStateTagComponent;
	return true;
}

bool UWuwaActionCoordinatorComponent::IsInitialized() const
{
	return IsValid(StateTagComponent);
}

bool UWuwaActionCoordinatorComponent::RegisterCapability(UObject* CapabilityObject)
{
	IWuwaActionCapability* Capability =
	    IsValid(CapabilityObject) ? Cast<IWuwaActionCapability>(CapabilityObject) : nullptr;
	if (Capability == nullptr || !Capability->GetActionCapabilityTag().IsValid())
	{
		UE_LOG(LogWuwa, Warning, TEXT("拒绝注册无效 Action Capability。Owner=%s"), *GetNameSafe(GetOwner()));
		return false;
	}

	for (UObject* RegisteredObject : RegisteredCapabilities)
	{
		const IWuwaActionCapability* Registered = Cast<IWuwaActionCapability>(RegisteredObject);
		if (RegisteredObject == CapabilityObject)
		{
			return true;
		}

		if (Registered != nullptr && Registered->GetActionCapabilityTag() == Capability->GetActionCapabilityTag())
		{
			UE_LOG(LogWuwa,
			       Error,
			       TEXT("同一 CapabilityTag 只能有一个处理者。Tag=%s"),
			       *Capability->GetActionCapabilityTag().ToString());
			return false;
		}
	}

	RegisteredCapabilities.Add(CapabilityObject);
	return true;
}

bool UWuwaActionCoordinatorComponent::UnregisterCapability(UObject* CapabilityObject)
{
	if (!IsValid(CapabilityObject) || ActiveInstance.CommittedCapabilities.Contains(CapabilityObject))
	{
		return false;
	}

	return RegisteredCapabilities.Remove(CapabilityObject) > 0;
}

bool UWuwaActionCoordinatorComponent::BindAbilityInterop(UWuwaActionAbilityInteropComponent* InAbilityInterop)
{
	if (!IsValid(InAbilityInterop))
	{
		AbilityInterop.Reset();
		return false;
	}

	AbilityInterop = InAbilityInterop;
	return true;
}

FWuwaActionResult UWuwaActionCoordinatorComponent::EnqueueIntent(const FWuwaResolvedActionIntent& Intent)
{
	FWuwaActionResult Result;
	Result.ActionTag = Intent.Request.Definition != nullptr ? Intent.Request.Definition->ActionTag : FGameplayTag();
	Result.SourceSequence = Intent.Request.Header.Sequence;
	Result.NetworkGeneration = Intent.Request.NetworkGeneration;

	EWuwaActionRejectionReason RejectionReason = EWuwaActionRejectionReason::None;
	const bool bEnqueued = Queue.Enqueue(Intent, MaxQueueDepth, RejectionReason);
	if (!bEnqueued)
	{
		Result.RejectionReason = RejectionReason;
		LastResult = Result;
		return Result;
	}

	Result.Status = EWuwaActionRequestStatus::Buffered;
	Result.RejectionReason = EWuwaActionRejectionReason::None;
	LastResult = Result;
	return Result;
}

FWuwaActionResult UWuwaActionCoordinatorComponent::StartAuthoritativeIntent(const FWuwaResolvedActionIntent& Intent)
{
	if (!IsInitialized() || bIsPumpingQueue)
	{
		FWuwaActionResult Result;
		Result.ActionTag = Intent.Request.Definition != nullptr ? Intent.Request.Definition->ActionTag : FGameplayTag();
		Result.SourceSequence = Intent.Request.Header.Sequence;
		Result.NetworkGeneration = Intent.Request.NetworkGeneration;
		Result.RejectionReason = EWuwaActionRejectionReason::InvalidContext;
		LastResult = Result;
		return Result;
	}

	TGuardValue<bool> PumpGuard(bIsPumpingQueue, true);
	bool bTemporaryFailure = false;
	LastResult = TryStartIntent(Intent, bTemporaryFailure);
	return LastResult;
}

FWuwaActionResult UWuwaActionCoordinatorComponent::EnqueueAction(const FWuwaMessageHeader& Header,
                                                                 UWuwaActionDefinition* Definition,
                                                                 const FWuwaActionContext& Context)
{
	FWuwaResolvedActionIntent Intent;
	Intent.Request.Header = Header;
	Intent.Request.Definition = Definition;
	Intent.Request.Context = Context;
	Intent.ExpireAt = Header.CreatedAt + static_cast<double>(IsValid(Definition) ? Definition->BufferTime : 0.f);
	return EnqueueIntent(Intent);
}

void UWuwaActionCoordinatorComponent::PumpQueue()
{
	if (!IsInitialized() || bIsPumpingQueue)
	{
		return;
	}

	TGuardValue<bool> PumpGuard(bIsPumpingQueue, true);
	TArray<FWuwaResolvedActionIntent> ExpiredIntents;
	Queue.Expire(GetActionTime(), ExpiredIntents);

	while (const FWuwaResolvedActionIntent* Front = Queue.Peek())
	{
		bool bTemporaryFailure = false;
		const FWuwaActionResult Result = TryStartIntent(*Front, bTemporaryFailure);
		LastResult = Result;

		if (Result.HasStarted())
		{
			FWuwaResolvedActionIntent StartedIntent;
			if (!Queue.PopFront(StartedIntent))
			{
				UE_LOG(LogWuwa, Error, TEXT("Action 已启动但 FIFO 队首无法提交。Owner=%s"), *GetNameSafe(GetOwner()));
				FinishCurrentInternal(EWuwaActionEndReason::Failed);
			}
			return;
		}

		if (bTemporaryFailure && !Front->IsExpired(GetActionTime()))
		{
			LastResult.Status = EWuwaActionRequestStatus::Buffered;
			return;
		}

		FWuwaResolvedActionIntent DiscardedIntent;
		if (!Queue.PopFront(DiscardedIntent))
		{
			return;
		}
	}
}

double UWuwaActionCoordinatorComponent::GetActionTime() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? static_cast<double>(World->GetTimeSeconds()) : 0.0;
}

bool UWuwaActionCoordinatorComponent::CanStartBase(const FWuwaActionRequest& Request,
                                                   EWuwaActionRejectionReason& OutReason) const
{
	OutReason = EWuwaActionRejectionReason::None;
	const UWuwaActionDefinition* Definition = Request.Definition;

	if (!IsInitialized() || !Request.IsValid() || !IsValid(Definition) || !Definition->IsRuntimeValid())
	{
		OutReason = EWuwaActionRejectionReason::InvalidDefinition;
		return false;
	}

	if (!Definition->IsContextValid(Request.Context))
	{
		OutReason = EWuwaActionRejectionReason::InvalidContext;
		return false;
	}

	const FGameplayTagContainer ActiveTags = StateTagComponent->GetActiveTags();
	if (ActiveTags.HasTagExact(WuwaGameplayTags::State_Combat_Dead))
	{
		OutReason = EWuwaActionRejectionReason::BlockedByCombatDeath;

		return false;
	}

	if (ActiveTags.HasTagExact(WuwaGameplayTags::State_Combat_Staggered))
	{
		OutReason = EWuwaActionRejectionReason::BlockedByCombatStagger;

		return false;
	}

	bool bWillInterruptBlockingAbility = false;

	if (AbilityInterop.IsValid() && AbilityInterop->HasBlockingActiveAbility(EWuwaAbilityInterruptSource::Action))
	{
		// 当前确实存在阻挡 Action 的 GAS Ability。
		if (!AbilityInterop->CanInterruptActiveAbility(EWuwaAbilityInterruptSource::Action, Definition->ActionTag))
		{
			OutReason = EWuwaActionRejectionReason::BlockedByCombatAttack;

			return false;
		}

		bWillInterruptBlockingAbility = true;
	}
	else if (ActiveTags.HasTagExact(WuwaGameplayTags::State_Combat_Attacking))
	{
		// Legacy fallback：有攻击状态，但不存在可以负责取消它的 GAS Ability。
		OutReason = EWuwaActionRejectionReason::BlockedByCombatAttack;

		return false;
	}

	FGameplayTagContainer ProjectedActiveTags = ActiveTags;

	if (bWillInterruptBlockingAbility)
	{
		// MeleeAttack::EndAbility
		ProjectedActiveTags.RemoveTag(WuwaGameplayTags::State_Combat_Attacking);

		// UWuwaGameplayAbility::CleanupInterruptRuntime()
		// 会释放 Ability 自己持有的 Block.Input.Move。
		ProjectedActiveTags.RemoveTag(WuwaGameplayTags::Block_Input_Move);
	}

	if (!ProjectedActiveTags.HasAll(Definition->RequiredTags))
	{
		OutReason = EWuwaActionRejectionReason::MissingRequiredTag;

		return false;
	}

	if (ProjectedActiveTags.HasAny(Definition->BlockedTags))
	{
		OutReason = EWuwaActionRejectionReason::BlockedByTag;

		return false;
	}

	const double* CooldownExpireAt = CooldownExpireAtByAction.Find(Definition->ActionTag);
	if (CooldownExpireAt != nullptr && GetActionTime() < *CooldownExpireAt)
	{
		OutReason = EWuwaActionRejectionReason::Cooldown;
		return false;
	}

	return CanInterruptCurrent(Request, OutReason);
}

bool UWuwaActionCoordinatorComponent::CanInterruptCurrent(const FWuwaActionRequest& Request,
                                                          EWuwaActionRejectionReason& OutReason) const
{
	if (!HasActiveAction())
	{
		return true;
	}

	const UWuwaActionDefinition* Incoming = Request.Definition;
	const UWuwaActionDefinition* Current = ActiveInstance.Request.Definition;
	if (!IsValid(Incoming) || !IsValid(Current))
	{
		OutReason = EWuwaActionRejectionReason::InvalidContext;
		return false;
	}

	if (Incoming->Priority < Current->Priority)
	{
		OutReason = EWuwaActionRejectionReason::Priority;
		return false;
	}

	const bool bIncomingAllowsCancel = Current->ActionTag.MatchesAny(Incoming->CanCancelActions);
	const bool bCurrentAllowsIncoming = Incoming->ActionTag.MatchesAny(Current->CanBeCancelledBy);
	if (!bIncomingAllowsCancel || !bCurrentAllowsIncoming)
	{
		OutReason = EWuwaActionRejectionReason::CancellationRule;
		return false;
	}

	return true;
}

bool UWuwaActionCoordinatorComponent::ResolveRequiredCapabilities(const FWuwaActionRequest& Request,
                                                                  TArray<UObject*>& OutCapabilities,
                                                                  EWuwaActionRejectionReason& OutReason) const
{
	OutCapabilities.Reset();
	OutReason = EWuwaActionRejectionReason::None;

	FGameplayTagContainer RequiredTags;
	Request.Definition->GatherRequiredCapabilityTags(RequiredTags);

	for (const FGameplayTag& RequiredTag : RequiredTags)
	{
		UObject* MatchedObject = nullptr;

		for (UObject* CapabilityObject : RegisteredCapabilities)
		{
			const IWuwaActionCapability* Capability = Cast<IWuwaActionCapability>(CapabilityObject);
			if (Capability != nullptr && Capability->GetActionCapabilityTag() == RequiredTag)
			{
				MatchedObject = CapabilityObject;
				break;
			}
		}

		if (MatchedObject == nullptr)
		{
			OutReason = EWuwaActionRejectionReason::MissingCapability;
			return false;
		}

		OutCapabilities.Add(MatchedObject);
	}

	OutCapabilities.Sort(
	    [](const UObject& Left, const UObject& Right)
	    {
		    const IWuwaActionCapability* LeftCapability = Cast<IWuwaActionCapability>(&Left);
		    const IWuwaActionCapability* RightCapability = Cast<IWuwaActionCapability>(&Right);
		    return LeftCapability != nullptr && RightCapability != nullptr &&
		           LeftCapability->GetActionCapabilityCommitOrder() < RightCapability->GetActionCapabilityCommitOrder();
	    });

	return true;
}

bool UWuwaActionCoordinatorComponent::IsTemporaryRejection(const EWuwaActionRejectionReason Reason)
{
	return Reason == EWuwaActionRejectionReason::MissingRequiredTag ||
	       Reason == EWuwaActionRejectionReason::BlockedByTag || Reason == EWuwaActionRejectionReason::Priority ||
	       Reason == EWuwaActionRejectionReason::CancellationRule;
}

FWuwaActionResult UWuwaActionCoordinatorComponent::TryStartIntent(const FWuwaResolvedActionIntent& Intent,
                                                                  bool& OutTemporaryFailure)
{
	OutTemporaryFailure = false;

	FWuwaActionResult Result;
	Result.ActionTag = Intent.Request.Definition != nullptr ? Intent.Request.Definition->ActionTag : FGameplayTag();
	Result.SourceSequence = Intent.Request.Header.Sequence;
	Result.NetworkGeneration = Intent.Request.NetworkGeneration;

	EWuwaActionRejectionReason RejectionReason = EWuwaActionRejectionReason::None;
	if (!CanStartBase(Intent.Request, RejectionReason))
	{
		Result.RejectionReason = RejectionReason;
		OutTemporaryFailure = IsTemporaryRejection(RejectionReason);
		return Result;
	}

	{
		TArray<UObject*> Capabilities;
		FWuwaActionHandle NewHandle;
		{
			if (!ResolveRequiredCapabilities(Intent.Request, Capabilities, RejectionReason))
			{
				Result.RejectionReason = RejectionReason;
				return Result;
			}
			NewHandle.Value = NextActionHandle++;
			if (NextActionHandle <= 0)
			{
				NextActionHandle = 1;
			}

			FWuwaActionPrepareMessage PrepareMessage;
			PrepareMessage.Handle = NewHandle;
			PrepareMessage.ReplacingHandle = ActiveInstance.Handle;
			PrepareMessage.Request = Intent.Request;

			for (UObject* CapabilityObject : Capabilities)
			{
				const IWuwaActionCapability* Capability = Cast<IWuwaActionCapability>(CapabilityObject);
				const FWuwaActionCapabilityResult CapabilityResult =
				    Capability != nullptr
				        ? Capability->PrepareAction(PrepareMessage)
				        : FWuwaActionCapabilityResult::Failure(EWuwaActionRejectionReason::MissingCapability);
				if (!CapabilityResult.bSucceeded)
				{
					Result.RejectionReason = CapabilityResult.RejectionReason == EWuwaActionRejectionReason::None
					                             ? EWuwaActionRejectionReason::CapabilityPrepareFailed
					                             : CapabilityResult.RejectionReason;
					return Result;
				}
			}
		}

		if (AbilityInterop.IsValid() && AbilityInterop->HasBlockingActiveAbility(EWuwaAbilityInterruptSource::Action))
		{
			const FGameplayTag IncomingActionTag = Intent.Request.Definition->ActionTag;

			if (!AbilityInterop->TryInterruptActiveAbility(EWuwaAbilityInterruptSource::Action, IncomingActionTag))
			{
				Result.RejectionReason = EWuwaActionRejectionReason::BlockedByCombatAttack;

				return Result;
			}
		}

		if (HasActiveAction() && !FinishCurrentInternal(EWuwaActionEndReason::Interrupted))
		{
			Result.RejectionReason = EWuwaActionRejectionReason::InvalidContext;
			return Result;
		}

		ActiveInstance.Handle = NewHandle;
		ActiveInstance.Request = Intent.Request;
		ActiveInstance.State = EWuwaActionInstanceState::Preparing;

		ActiveInstance.ExclusiveStateTagHandle =
		    StateTagComponent->AcquireTag(WuwaGameplayTags::State_Action_ExclusiveActive);
		if (!ActiveInstance.ExclusiveStateTagHandle.IsValid())
		{
			UE_LOG(LogWuwa,
			       Error,
			       TEXT("Action 无法取得通用独占状态。Owner=%s, Action=%s, Handle=%lld"),
			       *GetNameSafe(GetOwner()),
			       *Intent.Request.Definition->ActionTag.ToString(),
			       NewHandle.Value);
			ActiveInstance.ResetAfterCleanup();
			Result.RejectionReason = EWuwaActionRejectionReason::InvalidContext;
			return Result;
		}

		for (const FGameplayTag& GrantedTag : Intent.Request.Definition->GrantedTags)
		{
			FWuwaStateTagHandle TagHandle = StateTagComponent->AcquireTag(GrantedTag);
			if (!TagHandle.IsValid())
			{
				ReleaseGrantedTagHandles(ActiveInstance.GrantedTagHandles);
				ReleaseExclusiveStateTagHandle();
				ActiveInstance.ResetAfterCleanup();
				Result.RejectionReason = EWuwaActionRejectionReason::InvalidDefinition;
				return Result;
			}
			ActiveInstance.GrantedTagHandles.Add(MoveTemp(TagHandle));
		}
		FWuwaActionCommitMessage CommitMessage;
		CommitMessage.Handle = NewHandle;
		CommitMessage.Request = Intent.Request;

		for (UObject* CapabilityObject : Capabilities)
		{
			IWuwaActionCapability* Capability = Cast<IWuwaActionCapability>(CapabilityObject);
			const FWuwaActionCapabilityResult CapabilityResult =
			    Capability != nullptr
			        ? Capability->CommitAction(CommitMessage)
			        : FWuwaActionCapabilityResult::Failure(EWuwaActionRejectionReason::MissingCapability);
			if (!CapabilityResult.bSucceeded)
			{
				if (Capability != nullptr)
				{
					Capability->RollbackAction(NewHandle);
				}

				for (int32 Index = ActiveInstance.CommittedCapabilities.Num() - 1; Index >= 0; --Index)
				{
					if (IWuwaActionCapability* Committed =
					        Cast<IWuwaActionCapability>(ActiveInstance.CommittedCapabilities[Index]))
					{
						Committed->RollbackAction(NewHandle);
					}
				}

				ReleaseGrantedTagHandles(ActiveInstance.GrantedTagHandles);
				ReleaseExclusiveStateTagHandle();
				ActiveInstance.ResetAfterCleanup();
				Result.RejectionReason = CapabilityResult.RejectionReason == EWuwaActionRejectionReason::None
				                             ? EWuwaActionRejectionReason::CapabilityCommitFailed
				                             : CapabilityResult.RejectionReason;
				return Result;
			}

			ActiveInstance.CommittedCapabilities.Add(CapabilityObject);
		}

		ActiveInstance.State = EWuwaActionInstanceState::Active;
		LastEndReason = EWuwaActionEndReason::None;

		if (Intent.Request.Definition->CooldownDuration > 0.f)
		{
			double& ExpireAt = CooldownExpireAtByAction.FindOrAdd(Intent.Request.Definition->ActionTag);
			ExpireAt = FMath::Max(ExpireAt,
			                      GetActionTime() + static_cast<double>(Intent.Request.Definition->CooldownDuration));
		}

		Result.Status = EWuwaActionRequestStatus::Started;
		Result.RejectionReason = EWuwaActionRejectionReason::None;
		Result.ActionHandle = NewHandle;
		OnActionStarted.Broadcast(Result);

		return Result;
	}
}

bool UWuwaActionCoordinatorComponent::HandleActionEvent(const FWuwaActionEventMessage& Message)
{
	if (!Message.IsValid() || !HasActiveAction() || Message.Handle != ActiveInstance.Handle ||
	    ActiveInstance.State != EWuwaActionInstanceState::Active)
	{
		return false;
	}

	EWuwaActionEndReason RequestedEndReason = EWuwaActionEndReason::None;

	for (UObject* CapabilityObject : ActiveInstance.CommittedCapabilities)
	{
		IWuwaActionCapability* Capability = Cast<IWuwaActionCapability>(CapabilityObject);
		if (Capability == nullptr)
		{
			continue;
		}

		const EWuwaActionEndReason CapabilityReason = Capability->HandleActionEvent(Message);
		if (CapabilityReason != EWuwaActionEndReason::None && RequestedEndReason == EWuwaActionEndReason::None)
		{
			RequestedEndReason = CapabilityReason;
		}
		else if (CapabilityReason != EWuwaActionEndReason::None && CapabilityReason != RequestedEndReason)
		{
			UE_LOG(LogWuwa,
			       Warning,
			       TEXT("同一 Action Event 产生多个结束原因，保留首个。Event=%s"),
			       *Message.EventTag.ToString());
		}
	}

	EWuwaActionEndReason DefinitionReason = EWuwaActionEndReason::None;
	if (ActiveInstance.Request.Definition->ResolveCompletionRule(Message.EventTag, DefinitionReason) &&
	    RequestedEndReason == EWuwaActionEndReason::None)
	{
		RequestedEndReason = DefinitionReason;
	}

	if (RequestedEndReason != EWuwaActionEndReason::None)
	{
		FinishCurrentInternal(RequestedEndReason, &Message);
		PumpQueue();
	}

	return true;
}

bool UWuwaActionCoordinatorComponent::FinishCurrent(const EWuwaActionEndReason EndReason)
{
	const bool bFinished = FinishCurrentInternal(EndReason);
	if (bFinished && EndReason != EWuwaActionEndReason::OwnerDestroyed)
	{
		PumpQueue();
	}
	return bFinished;
}

void UWuwaActionCoordinatorComponent::AbortAllActions(const EWuwaActionEndReason EndReason)
{
	if (EndReason == EWuwaActionEndReason::None)
	{
		UE_LOG(LogWuwa, Warning, TEXT("Action 全部终止被拒绝：结束原因无效。Owner=%s"), *GetNameSafe(GetOwner()));
		return;
	}

	TArray<FWuwaResolvedActionIntent> RemovedIntents;
	Queue.Reset(RemovedIntents);
	if (HasActiveAction())
	{
		FinishCurrentInternal(EndReason);
	}
}

bool UWuwaActionCoordinatorComponent::FinishCurrentInternal(const EWuwaActionEndReason EndReason,
                                                            const FWuwaActionEventMessage* TriggerEvent)
{
	if (!HasActiveAction() || EndReason == EWuwaActionEndReason::None || bIsFinishingCurrent)
	{
		return false;
	}

	TGuardValue<bool> FinishGuard(bIsFinishingCurrent, true);
	FWuwaActionFinalizedMessage FinalizedMessage;
	ActiveInstance.State = EWuwaActionInstanceState::Finishing;

	FWuwaActionStopMessage StopMessage;
	StopMessage.Handle = ActiveInstance.Handle;
	StopMessage.EndReason = EndReason;

	for (int32 Index = ActiveInstance.CommittedCapabilities.Num() - 1; Index >= 0; --Index)
	{
		if (IWuwaActionCapability* Capability =
		        Cast<IWuwaActionCapability>(ActiveInstance.CommittedCapabilities[Index]))
		{
			Capability->StopAction(StopMessage);
		}
	}

	ReleaseGrantedTagHandles(ActiveInstance.GrantedTagHandles);
	ReleaseExclusiveStateTagHandle();

	FinalizedMessage.Handle = ActiveInstance.Handle;
	FinalizedMessage.ActionTag = ActiveInstance.Request.Definition->ActionTag;
	FinalizedMessage.EndReason = EndReason;
	FinalizedMessage.NetworkGeneration = ActiveInstance.Request.NetworkGeneration;
	if (TriggerEvent != nullptr && TriggerEvent->EventTag.IsValid())
	{
		FinalizedMessage.TriggerEventTag = TriggerEvent->EventTag;
		const bool bFiniteMoveIntent =
		    FMath::IsFinite(TriggerEvent->MoveIntent.X) && FMath::IsFinite(TriggerEvent->MoveIntent.Y);
		FinalizedMessage.TriggerMoveIntent =
		    bFiniteMoveIntent ? TriggerEvent->MoveIntent.GetClampedToMaxSize(1.f) : FVector2D::ZeroVector;
	}

	for (UObject* CapabilityObject : ActiveInstance.CommittedCapabilities)
	{
		if (IWuwaActionCapability* Capability = Cast<IWuwaActionCapability>(CapabilityObject))
		{
			Capability->HandleActionFinalized(FinalizedMessage);
		}
	}

	ActiveInstance.State = EWuwaActionInstanceState::Finished;
	LastEndReason = EndReason;
	ActiveInstance.ResetAfterCleanup();
	OnActionFinalized.Broadcast(FinalizedMessage);

	return true;
}

void UWuwaActionCoordinatorComponent::ReleaseGrantedTagHandles(TArray<FWuwaStateTagHandle>& Handles)
{
	if (!IsValid(StateTagComponent))
	{
		Handles.Reset();
		return;
	}

	for (int32 Index = Handles.Num() - 1; Index >= 0; --Index)
	{
		if (Handles[Index].IsValid())
		{
			StateTagComponent->ReleaseTag(Handles[Index]);
		}
	}
	Handles.Reset();
}

void UWuwaActionCoordinatorComponent::ReleaseExclusiveStateTagHandle()
{
	if (!ActiveInstance.ExclusiveStateTagHandle.IsValid())
	{
		return;
	}

	if (!IsValid(StateTagComponent))
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Action 独占状态清理失败：StateTagComponent 已失效。Owner=%s, Handle=%lld"),
		       *GetNameSafe(GetOwner()),
		       ActiveInstance.Handle.Value);
		ActiveInstance.ExclusiveStateTagHandle.Reset();
		return;
	}

	if (!StateTagComponent->ReleaseTag(ActiveInstance.ExclusiveStateTagHandle))
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Action 独占状态清理失败：Handle 无法释放。Owner=%s, Handle=%lld"),
		       *GetNameSafe(GetOwner()),
		       ActiveInstance.Handle.Value);
	}
}

FWuwaActionRuntimeSnapshot UWuwaActionCoordinatorComponent::GetRuntimeSnapshot() const
{
	FWuwaActionRuntimeSnapshot Snapshot;
	Snapshot.ActiveHandle = ActiveInstance.Handle;
	Snapshot.ActiveActionTag =
	    ActiveInstance.Request.Definition != nullptr ? ActiveInstance.Request.Definition->ActionTag : FGameplayTag();
	Snapshot.ActiveNetworkGeneration = ActiveInstance.Request.NetworkGeneration;
	Snapshot.InstanceState = ActiveInstance.State;
	Snapshot.bOwnsExclusiveStateTag = ActiveInstance.ExclusiveStateTagHandle.IsValid();
	for (UObject* CapabilityObject : ActiveInstance.CommittedCapabilities)
	{
		if (const IWuwaActionCapability* Capability = Cast<IWuwaActionCapability>(CapabilityObject))
		{
			Snapshot.ActiveCapabilityTags.AddTag(Capability->GetActionCapabilityTag());
		}
	}
	Snapshot.QueueCount = Queue.Num();
	Snapshot.LastResult = LastResult;
	Snapshot.LastEndReason = LastEndReason;
	return Snapshot;
}

void UWuwaActionCoordinatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasActiveAction())
	{
		FinishCurrentInternal(EWuwaActionEndReason::OwnerDestroyed);
	}

	TArray<FWuwaResolvedActionIntent> RemovedIntents;
	Queue.Reset(RemovedIntents);
	CooldownExpireAtByAction.Reset();
	RegisteredCapabilities.Reset();
	StateTagComponent = nullptr;

	Super::EndPlay(EndPlayReason);
}
