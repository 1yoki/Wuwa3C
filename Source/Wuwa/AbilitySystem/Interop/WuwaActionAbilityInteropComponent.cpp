#include "AbilitySystem/Interop/WuwaActionAbilityInteropComponent.h"

#include "AbilitySystem/Input/WuwaAbilityInputRouterComponent.h"
#include "AbilitySystem/Runtime/WuwaAbilitySystemComponent.h"
#include "AbilitySystem/WuwaAbilitySystemLog.h"
#include "Actions/Network/WuwaActionNetworkComponent.h"
#include "Core/WuwaGameplayTags.h"
#include "Core/WuwaStateTagComponent.h"

UWuwaActionAbilityInteropComponent::UWuwaActionAbilityInteropComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

bool UWuwaActionAbilityInteropComponent::Initialize(UWuwaAbilitySystemComponent* InAbilitySystemComponent,
                                                    UWuwaStateTagComponent* InStateTagComponent,
                                                    UWuwaActionNetworkComponent* InActionNetworkComponent,
                                                    UWuwaAbilityInputRouterComponent* InAbilityInputRouterComponent)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !IsValid(InAbilitySystemComponent) || !IsValid(InStateTagComponent) ||
	    !IsValid(InActionNetworkComponent) || !IsValid(InAbilityInputRouterComponent) ||
	    InAbilitySystemComponent->GetAvatarActor() != OwnerActor || InStateTagComponent->GetOwner() != OwnerActor ||
	    InActionNetworkComponent->GetOwner() != OwnerActor || InAbilityInputRouterComponent->GetOwner() != OwnerActor ||
	    !InAbilityInputRouterComponent->IsInitialized())
	{
		UE_LOG(LogWuwaAbility,
		       Error,
		       TEXT("Action/Ability Interop 初始化失败：绑定对象不完整或不属于当前 Avatar。Owner=%s, ASC=%s, "
		            "ASCAvatar=%s, StateTags=%s, ActionNetwork=%s, InputRouter=%s, InputReady=%s"),
		       *GetNameSafe(OwnerActor),
		       *GetNameSafe(InAbilitySystemComponent),
		       *GetNameSafe(IsValid(InAbilitySystemComponent) ? InAbilitySystemComponent->GetAvatarActor() : nullptr),
		       *GetNameSafe(InStateTagComponent),
		       *GetNameSafe(InActionNetworkComponent),
		       *GetNameSafe(InAbilityInputRouterComponent),
		       IsValid(InAbilityInputRouterComponent) && InAbilityInputRouterComponent->IsInitialized()
		           ? TEXT("true")
		           : TEXT("false"));
		return false;
	}

	const bool bSameBinding = BoundAbilitySystemComponent.Get() == InAbilitySystemComponent &&
	                          BoundStateTagComponent.Get() == InStateTagComponent &&
	                          BoundActionNetworkComponent.Get() == InActionNetworkComponent &&
	                          BoundAbilityInputRouterComponent.Get() == InAbilityInputRouterComponent &&
	                          AttackingTagDelegateHandle.IsValid() && DeadTagDelegateHandle.IsValid() &&
	                          StaggeredTagDelegateHandle.IsValid() && MoveBlockTagDelegateHandle.IsValid();
	if (bSameBinding)
	{
		SynchronizeMirror(WuwaGameplayTags::State_Combat_Attacking,
		                  InAbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::State_Combat_Attacking),
		                  AttackingMirrorHandle);
		SynchronizeMirror(WuwaGameplayTags::State_Combat_Dead,
		                  InAbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::State_Combat_Dead),
		                  DeadMirrorHandle);
		HandleStaggeredTagChanged(
		    WuwaGameplayTags::State_Combat_Staggered,
		    InAbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::State_Combat_Staggered));
		HandleMoveBlockTagChanged(WuwaGameplayTags::Block_Input_Move,
		                          InAbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::Block_Input_Move));
		return IsInitialized();
	}

	Shutdown();
	BoundAbilitySystemComponent = InAbilitySystemComponent;
	BoundStateTagComponent = InStateTagComponent;
	BoundActionNetworkComponent = InActionNetworkComponent;
	BoundAbilityInputRouterComponent = InAbilityInputRouterComponent;
	AttackingTagDelegateHandle =
	    InAbilitySystemComponent
	        ->RegisterGameplayTagEvent(WuwaGameplayTags::State_Combat_Attacking, EGameplayTagEventType::NewOrRemoved)
	        .AddUObject(this, &ThisClass::HandleAttackingTagChanged);
	DeadTagDelegateHandle =
	    InAbilitySystemComponent
	        ->RegisterGameplayTagEvent(WuwaGameplayTags::State_Combat_Dead, EGameplayTagEventType::NewOrRemoved)
	        .AddUObject(this, &ThisClass::HandleDeadTagChanged);
	StaggeredTagDelegateHandle =
	    InAbilitySystemComponent
	        ->RegisterGameplayTagEvent(WuwaGameplayTags::State_Combat_Staggered, EGameplayTagEventType::NewOrRemoved)
	        .AddUObject(this, &ThisClass::HandleStaggeredTagChanged);
	MoveBlockTagDelegateHandle =
	    InAbilitySystemComponent
	        ->RegisterGameplayTagEvent(WuwaGameplayTags::Block_Input_Move, EGameplayTagEventType::NewOrRemoved)
	        .AddUObject(this, &ThisClass::HandleMoveBlockTagChanged);

	if (!AttackingTagDelegateHandle.IsValid() || !DeadTagDelegateHandle.IsValid() ||
	    !StaggeredTagDelegateHandle.IsValid() || !MoveBlockTagDelegateHandle.IsValid())
	{
		UE_LOG(LogWuwaAbility,
		       Error,
		       TEXT("Action/Ability Interop 初始化失败：ASC Tag Delegate 未完整绑定。Owner=%s"),
		       *GetNameSafe(OwnerActor));
		Shutdown();
		return false;
	}

	SynchronizeMirror(WuwaGameplayTags::State_Combat_Attacking,
	                  InAbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::State_Combat_Attacking),
	                  AttackingMirrorHandle);
	SynchronizeMirror(WuwaGameplayTags::State_Combat_Dead,
	                  InAbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::State_Combat_Dead),
	                  DeadMirrorHandle);
	HandleStaggeredTagChanged(WuwaGameplayTags::State_Combat_Staggered,
	                          InAbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::State_Combat_Staggered));
	HandleMoveBlockTagChanged(WuwaGameplayTags::Block_Input_Move,
	                          InAbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::Block_Input_Move));

	const bool bInitialMirrorValid =
	    (InAbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::State_Combat_Attacking) <= 0 ||
	     AttackingMirrorHandle.IsValid()) &&
	    (InAbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::State_Combat_Dead) <= 0 ||
	     DeadMirrorHandle.IsValid()) &&
	    (InAbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::State_Combat_Staggered) <= 0 ||
	     StaggeredMirrorHandle.IsValid()) &&
	    (InAbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::Block_Input_Move) <= 0 ||
	     MoveBlockMirrorHandle.IsValid());
	if (!bInitialMirrorValid)
	{
		UE_LOG(LogWuwaAbility,
		       Error,
		       TEXT("Action/Ability Interop 初始化失败：初始 Combat 状态无法建立 Legacy 镜像。Owner=%s"),
		       *GetNameSafe(OwnerActor));
		Shutdown();
		return false;
	}

	++InitializationGeneration;
	return true;
}

void UWuwaActionAbilityInteropComponent::Shutdown()
{
	const bool bHadBinding = BoundAbilitySystemComponent.IsValid() || BoundStateTagComponent.IsValid() ||
	                         BoundActionNetworkComponent.IsValid() || BoundAbilityInputRouterComponent.IsValid() ||
	                         AttackingTagDelegateHandle.IsValid() || DeadTagDelegateHandle.IsValid() ||
	                         StaggeredTagDelegateHandle.IsValid() || MoveBlockTagDelegateHandle.IsValid() ||
	                         AttackingMirrorHandle.IsValid() || DeadMirrorHandle.IsValid() ||
	                         StaggeredMirrorHandle.IsValid() || MoveBlockMirrorHandle.IsValid();
	if (!bHadBinding)
	{
		return;
	}

	UWuwaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get();
	if (IsValid(AbilitySystemComponent))
	{
		if (AttackingTagDelegateHandle.IsValid())
		{
			AbilitySystemComponent
			    ->RegisterGameplayTagEvent(WuwaGameplayTags::State_Combat_Attacking,
			                               EGameplayTagEventType::NewOrRemoved)
			    .Remove(AttackingTagDelegateHandle);
		}
		if (DeadTagDelegateHandle.IsValid())
		{
			AbilitySystemComponent
			    ->RegisterGameplayTagEvent(WuwaGameplayTags::State_Combat_Dead, EGameplayTagEventType::NewOrRemoved)
			    .Remove(DeadTagDelegateHandle);
		}
		if (StaggeredTagDelegateHandle.IsValid())
		{
			AbilitySystemComponent
			    ->RegisterGameplayTagEvent(WuwaGameplayTags::State_Combat_Staggered,
			                               EGameplayTagEventType::NewOrRemoved)
			    .Remove(StaggeredTagDelegateHandle);
		}
		if (MoveBlockTagDelegateHandle.IsValid())
		{
			AbilitySystemComponent
			    ->RegisterGameplayTagEvent(WuwaGameplayTags::Block_Input_Move, EGameplayTagEventType::NewOrRemoved)
			    .Remove(MoveBlockTagDelegateHandle);
		}
	}
	AttackingTagDelegateHandle.Reset();
	DeadTagDelegateHandle.Reset();
	StaggeredTagDelegateHandle.Reset();
	MoveBlockTagDelegateHandle.Reset();

	UWuwaStateTagComponent* StateTagComponent = BoundStateTagComponent.Get();
	if (IsValid(StateTagComponent))
	{
		if (AttackingMirrorHandle.IsValid() && !StateTagComponent->ReleaseTag(AttackingMirrorHandle))
		{
			UE_LOG(LogWuwaAbility,
			       Error,
			       TEXT("Action/Ability Interop 无法释放攻击镜像。Owner=%s"),
			       *GetNameSafe(GetOwner()));
		}
		if (DeadMirrorHandle.IsValid() && !StateTagComponent->ReleaseTag(DeadMirrorHandle))
		{
			UE_LOG(LogWuwaAbility,
			       Error,
			       TEXT("Action/Ability Interop 无法释放死亡镜像。Owner=%s"),
			       *GetNameSafe(GetOwner()));
		}
		if (StaggeredMirrorHandle.IsValid() && !StateTagComponent->ReleaseTag(StaggeredMirrorHandle))
		{
			UE_LOG(LogWuwaAbility,
			       Error,
			       TEXT("Action/Ability Interop 无法释放硬直镜像。Owner=%s"),
			       *GetNameSafe(GetOwner()));
		}
		if (MoveBlockMirrorHandle.IsValid() && !StateTagComponent->ReleaseTag(MoveBlockMirrorHandle))
		{
			UE_LOG(LogWuwaAbility,
			       Error,
			       TEXT("Action/Ability Interop 无法释放移动输入阻止镜像。Owner=%s"),
			       *GetNameSafe(GetOwner()));
		}
	}
	else if (AttackingMirrorHandle.IsValid() || DeadMirrorHandle.IsValid() || StaggeredMirrorHandle.IsValid() ||
	         MoveBlockMirrorHandle.IsValid())
	{
		UE_LOG(LogWuwaAbility,
		       Error,
		       TEXT("Action/Ability Interop 清理时 StateTagComponent 已失效。Owner=%s"),
		       *GetNameSafe(GetOwner()));
	}

	AttackingMirrorHandle.Reset();
	DeadMirrorHandle.Reset();
	StaggeredMirrorHandle.Reset();
	MoveBlockMirrorHandle.Reset();
	BoundAbilitySystemComponent.Reset();
	BoundStateTagComponent.Reset();
	BoundActionNetworkComponent.Reset();
	BoundAbilityInputRouterComponent.Reset();
	++ShutdownGeneration;
}

bool UWuwaActionAbilityInteropComponent::IsInitialized() const
{
	const AActor* OwnerActor = GetOwner();
	const UWuwaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get();
	const UWuwaStateTagComponent* StateTagComponent = BoundStateTagComponent.Get();
	const UWuwaActionNetworkComponent* ActionNetworkComponent = BoundActionNetworkComponent.Get();
	const UWuwaAbilityInputRouterComponent* AbilityInputRouterComponent = BoundAbilityInputRouterComponent.Get();
	if (!IsValid(OwnerActor) || !IsValid(AbilitySystemComponent) || !IsValid(StateTagComponent) ||
	    !IsValid(ActionNetworkComponent) || !IsValid(AbilityInputRouterComponent) ||
	    AbilitySystemComponent->GetAvatarActor() != OwnerActor || StateTagComponent->GetOwner() != OwnerActor ||
	    ActionNetworkComponent->GetOwner() != OwnerActor || AbilityInputRouterComponent->GetOwner() != OwnerActor ||
	    !AbilityInputRouterComponent->IsInitialized() || !AttackingTagDelegateHandle.IsValid() ||
	    !DeadTagDelegateHandle.IsValid() || !StaggeredTagDelegateHandle.IsValid() ||
	    !MoveBlockTagDelegateHandle.IsValid())
	{
		return false;
	}

	const bool bAttackingMirrorMatches =
	    (AbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::State_Combat_Attacking) > 0) ==
	    AttackingMirrorHandle.IsValid();
	const bool bDeadMirrorMatches = (AbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::State_Combat_Dead) >
	                                 0) == DeadMirrorHandle.IsValid();
	const bool bStaggeredMirrorMatches =
	    (AbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::State_Combat_Staggered) > 0) ==
	    StaggeredMirrorHandle.IsValid();
	const bool bMoveBlockMirrorMatches =
	    (AbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::Block_Input_Move) > 0) ==
	    MoveBlockMirrorHandle.IsValid();
	return bAttackingMirrorMatches && bDeadMirrorMatches && bStaggeredMirrorMatches && bMoveBlockMirrorMatches &&
	       (!AttackingMirrorHandle.IsValid() ||
	        StateTagComponent->HasTag(WuwaGameplayTags::State_Combat_Attacking, true)) &&
	       (!DeadMirrorHandle.IsValid() || StateTagComponent->HasTag(WuwaGameplayTags::State_Combat_Dead, true)) &&
	       (!StaggeredMirrorHandle.IsValid() ||
	        StateTagComponent->HasTag(WuwaGameplayTags::State_Combat_Staggered, true)) &&
	       (!MoveBlockMirrorHandle.IsValid() || StateTagComponent->HasTag(WuwaGameplayTags::Block_Input_Move, true));
}

FWuwaActionAbilityInteropRuntimeSnapshot UWuwaActionAbilityInteropComponent::GetRuntimeSnapshot() const
{
	const UWuwaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get();
	const UWuwaStateTagComponent* StateTagComponent = BoundStateTagComponent.Get();

	FWuwaActionAbilityInteropRuntimeSnapshot Snapshot;
	Snapshot.bInitialized = IsInitialized();
	Snapshot.AttackingTagCount =
	    IsValid(AbilitySystemComponent)
	        ? AbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::State_Combat_Attacking)
	        : 0;
	Snapshot.DeadTagCount = IsValid(AbilitySystemComponent)
	                            ? AbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::State_Combat_Dead)
	                            : 0;
	Snapshot.StaggeredTagCount =
	    IsValid(AbilitySystemComponent)
	        ? AbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::State_Combat_Staggered)
	        : 0;
	Snapshot.MoveBlockTagCount = IsValid(AbilitySystemComponent)
	                                 ? AbilitySystemComponent->GetGameplayTagCount(WuwaGameplayTags::Block_Input_Move)
	                                 : 0;
	Snapshot.bAttackingMirrored = IsValid(StateTagComponent) && AttackingMirrorHandle.IsValid() &&
	                              StateTagComponent->HasTag(WuwaGameplayTags::State_Combat_Attacking, true);
	Snapshot.bDeadMirrored = IsValid(StateTagComponent) && DeadMirrorHandle.IsValid() &&
	                         StateTagComponent->HasTag(WuwaGameplayTags::State_Combat_Dead, true);
	Snapshot.bStaggeredMirrored = IsValid(StateTagComponent) && StaggeredMirrorHandle.IsValid() &&
	                              StateTagComponent->HasTag(WuwaGameplayTags::State_Combat_Staggered, true);
	Snapshot.bMoveBlockMirrored = IsValid(StateTagComponent) && MoveBlockMirrorHandle.IsValid() &&
	                              StateTagComponent->HasTag(WuwaGameplayTags::Block_Input_Move, true);
	Snapshot.StaggerInvalidationCount = StaggerInvalidationCount;
	Snapshot.InitializationGeneration = InitializationGeneration;
	Snapshot.ShutdownGeneration = ShutdownGeneration;
	return Snapshot;
}

void UWuwaActionAbilityInteropComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Shutdown();
	Super::EndPlay(EndPlayReason);
}

void UWuwaActionAbilityInteropComponent::HandleAttackingTagChanged(const FGameplayTag Tag, const int32 NewCount)
{
	if (Tag != WuwaGameplayTags::State_Combat_Attacking)
	{
		UE_LOG(LogWuwaAbility,
		       Error,
		       TEXT("Action/Ability Interop 收到错误的攻击标签事件。Owner=%s, Tag=%s"),
		       *GetNameSafe(GetOwner()),
		       *Tag.ToString());
		return;
	}
	SynchronizeMirror(Tag, NewCount, AttackingMirrorHandle);
}

void UWuwaActionAbilityInteropComponent::HandleDeadTagChanged(const FGameplayTag Tag, const int32 NewCount)
{
	if (Tag != WuwaGameplayTags::State_Combat_Dead)
	{
		UE_LOG(LogWuwaAbility,
		       Error,
		       TEXT("Action/Ability Interop 收到错误的死亡标签事件。Owner=%s, Tag=%s"),
		       *GetNameSafe(GetOwner()),
		       *Tag.ToString());
		return;
	}
	SynchronizeMirror(Tag, NewCount, DeadMirrorHandle);
}

void UWuwaActionAbilityInteropComponent::HandleStaggeredTagChanged(const FGameplayTag Tag, const int32 NewCount)
{
	if (Tag != WuwaGameplayTags::State_Combat_Staggered)
	{
		UE_LOG(LogWuwaAbility,
		       Error,
		       TEXT("Action/Ability Interop 收到错误的硬直标签事件。Owner=%s, Tag=%s"),
		       *GetNameSafe(GetOwner()),
		       *Tag.ToString());
		return;
	}

	const bool bWasMirrored = StaggeredMirrorHandle.IsValid();
	SynchronizeMirror(Tag, NewCount, StaggeredMirrorHandle);
	if (NewCount > 0 && !bWasMirrored && StaggeredMirrorHandle.IsValid())
	{
		HandleStaggeredEntered();
	}
}

void UWuwaActionAbilityInteropComponent::HandleMoveBlockTagChanged(const FGameplayTag Tag, const int32 NewCount)
{
	if (Tag != WuwaGameplayTags::Block_Input_Move)
	{
		UE_LOG(LogWuwaAbility,
		       Error,
		       TEXT("Action/Ability Interop 收到错误的移动输入阻止标签事件。Owner=%s, Tag=%s"),
		       *GetNameSafe(GetOwner()),
		       *Tag.ToString());
		return;
	}
	SynchronizeMirror(Tag, NewCount, MoveBlockMirrorHandle);
}

void UWuwaActionAbilityInteropComponent::HandleStaggeredEntered()
{
	UWuwaAbilityInputRouterComponent* AbilityInputRouter = BoundAbilityInputRouterComponent.Get();
	UWuwaActionNetworkComponent* ActionNetwork = BoundActionNetworkComponent.Get();
	if (!IsValid(AbilityInputRouter) || !AbilityInputRouter->IsInitialized() || !IsValid(ActionNetwork))
	{
		UE_LOG(LogWuwaAbility,
		       Error,
		       TEXT("Action/Ability Interop 无法处理硬直进入：控制依赖已失效。Owner=%s, ActionNetwork=%s, "
		            "InputRouter=%s, InputReady=%s"),
		       *GetNameSafe(GetOwner()),
		       *GetNameSafe(ActionNetwork),
		       *GetNameSafe(AbilityInputRouter),
		       IsValid(AbilityInputRouter) && AbilityInputRouter->IsInitialized() ? TEXT("true") : TEXT("false"));
		return;
	}

	AbilityInputRouter->ClearAbilityInput();
	ActionNetwork->InvalidateAvatarGenerations(EWuwaGrappleMovementEndReason::Staggered,
	                                           EWuwaActionEndReason::Staggered);
	++StaggerInvalidationCount;
}

void UWuwaActionAbilityInteropComponent::SynchronizeMirror(const FGameplayTag& Tag,
                                                           const int32 NewCount,
                                                           FWuwaStateTagHandle& MirrorHandle)
{
	UWuwaStateTagComponent* StateTagComponent = BoundStateTagComponent.Get();
	if (!Tag.IsValid() || NewCount < 0 || !IsValid(StateTagComponent))
	{
		UE_LOG(LogWuwaAbility,
		       Error,
		       TEXT("Action/Ability Interop 拒绝无效镜像同步。Owner=%s, Tag=%s, Count=%d, StateTags=%s"),
		       *GetNameSafe(GetOwner()),
		       *Tag.ToString(),
		       NewCount,
		       *GetNameSafe(StateTagComponent));
		return;
	}

	if (NewCount > 0)
	{
		if (!MirrorHandle.IsValid())
		{
			MirrorHandle = StateTagComponent->AcquireTag(Tag);
			if (!MirrorHandle.IsValid())
			{
				UE_LOG(LogWuwaAbility,
				       Error,
				       TEXT("Action/Ability Interop 无法取得 Legacy 镜像。Owner=%s, Tag=%s, Count=%d"),
				       *GetNameSafe(GetOwner()),
				       *Tag.ToString(),
				       NewCount);
			}
		}
		return;
	}

	if (MirrorHandle.IsValid() && !StateTagComponent->ReleaseTag(MirrorHandle))
	{
		UE_LOG(LogWuwaAbility,
		       Error,
		       TEXT("Action/Ability Interop 无法释放 Legacy 镜像。Owner=%s, Tag=%s"),
		       *GetNameSafe(GetOwner()),
		       *Tag.ToString());
	}
}

bool UWuwaActionAbilityInteropComponent::HasBlockingActiveAbility(const EWuwaAbilityInterruptSource Source) const
{
	const UWuwaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get();

	if (!IsValid(AbilitySystemComponent))
	{
		return false;
	}

	return AbilitySystemComponent->HasBlockingActiveAbility(Source);
}

bool UWuwaActionAbilityInteropComponent::CanInterruptActiveAbility(const EWuwaAbilityInterruptSource Source,
                                                                   const FGameplayTag& IncomingActionTag) const
{
	const UWuwaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get();

	if (!IsValid(AbilitySystemComponent))
	{
		return false;
	}

	return AbilitySystemComponent->CanInterruptActiveAbility(Source, IncomingActionTag);
}

bool UWuwaActionAbilityInteropComponent::TryInterruptActiveAbility(const EWuwaAbilityInterruptSource Source,
                                                                   const FGameplayTag& IncomingActionTag)
{
	UWuwaAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get();

	if (!IsValid(AbilitySystemComponent))
	{
		return false;
	}

	return AbilitySystemComponent->TryInterruptActiveAbility(Source, IncomingActionTag);
}
