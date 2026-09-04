#include "Animation/Actions/WuwaActionAnimationCapabilityComponent.h"

#include "Actions/Data/WuwaActionDefinition.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/WuwaGameplayTags.h"
#include "GameFramework/GameStateBase.h"
#include "Messaging/WuwaCharacterMessageDispatcherComponent.h"
#include "WuwaCharacter.h"
#include "Wuwa.h"

UWuwaActionAnimationCapabilityComponent::UWuwaActionAnimationCapabilityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UWuwaActionAnimationCapabilityComponent::Initialize(AWuwaCharacter* InCharacter,
                                                         UWuwaCharacterMessageDispatcherComponent* InDispatcher)
{
	ReleaseActiveAnimation();
	CharacterOwner = nullptr;
	Dispatcher = nullptr;

	if (!IsValid(InCharacter) || !IsValid(InDispatcher) || InCharacter != GetOwner() ||
	    !IsValid(InCharacter->GetMesh()) || !IsValid(InCharacter->GetMesh()->GetAnimInstance()))
	{
		UE_LOG(LogWuwa, Error, TEXT("Action Animation Capability 初始化失败。Owner=%s"), *GetNameSafe(GetOwner()));
		return false;
	}

	CharacterOwner = InCharacter;
	Dispatcher = InDispatcher;
	if (!EnsureAuthorityMontageTickPolicy())
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Action Animation Capability 无法建立权威 Montage Tick 基线。Owner=%s"),
		       *GetNameSafe(GetOwner()));
		CharacterOwner = nullptr;
		Dispatcher = nullptr;
		return false;
	}
	return true;
}

bool UWuwaActionAnimationCapabilityComponent::IsInitialized() const
{
	return IsValid(CharacterOwner) && IsValid(ResolveAnimInstance()) && Dispatcher.IsValid();
}

bool UWuwaActionAnimationCapabilityComponent::EnsureAuthorityMontageTickPolicy()
{
	if (!IsValid(CharacterOwner) || !CharacterOwner->HasAuthority())
	{
		return IsValid(CharacterOwner);
	}

	USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh();
	if (!IsValid(Mesh))
	{
		return false;
	}

	switch (Mesh->VisibilityBasedAnimTickOption)
	{
		case EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered:
			Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
			return true;
		case EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered:
		case EVisibilityBasedAnimTickOption::OnlyTickMontagesAndRefreshBonesWhenPlayingMontages:
		case EVisibilityBasedAnimTickOption::AlwaysTickPose:
		case EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones:
			return true;
		default:
			UE_LOG(LogWuwa,
			       Error,
			       TEXT("Authority Mesh 使用未覆盖的动画 Tick 策略。Owner=%s, Policy=%d"),
			       *GetNameSafe(CharacterOwner),
			       static_cast<int32>(Mesh->VisibilityBasedAnimTickOption));
			return false;
	}
}

bool UWuwaActionAnimationCapabilityComponent::PublishNotifyEvent(const FGameplayTag& EventTag,
                                                                 const USkeletalMeshComponent* MeshComp,
                                                                 const UAnimSequenceBase* Animation)
{
	if (!ActiveHandle.IsValid() || !EventTag.IsValid() || !IsInitialized() || MeshComp != CharacterOwner->GetMesh() ||
	    Animation != ActiveMontage)
	{
		return false;
	}

	return Dispatcher->PublishActionEvent(
	    ActiveHandle, EventTag, Dispatcher->GetCurrentMoveIntent(), MOVE_None, MOVE_None, 0, 0, this);
}

bool UWuwaActionAnimationCapabilityComponent::PlayReplicatedPresentation(const FWuwaNetworkActionGeneration& Generation,
                                                                         UWuwaActionDefinition* Definition,
                                                                         const float ServerStartTime)
{
	if (!Generation.IsValid() || !IsValid(Definition) || !FMath::IsFinite(ServerStartTime) || ServerStartTime < 0.f ||
	    ActiveHandle.IsValid() || !IsInitialized())
	{
		UE_LOG(LogWuwa,
		       Warning,
		       TEXT("远端 Legacy Montage 前置条件失败。Owner=%s, Generation=%d, Action=%s"),
		       *GetNameSafe(CharacterOwner),
		       Generation.Value,
		       *GetNameSafe(Definition));
		return false;
	}

	const FWuwaActionAnimationSpec* Spec = Definition->GetAnimationSpec();
	FWuwaActionRequest Request;
	Request.Definition = Definition;
	float PlayRate = 1.f;
	if (Spec == nullptr || !Spec->IsRuntimeValid() || !ResolvePlayRate(Request, *Spec, PlayRate))
	{
		return false;
	}

	if (ReplicatedPresentationGeneration == Generation && ReplicatedPresentationMontage == Spec->Montage &&
	    HasReplicatedPresentation())
	{
		return true;
	}

	ReleaseReplicatedPresentation();

	UAnimInstance* AnimInstance = ResolveAnimInstance();
	const AGameStateBase* GameState =
	    CharacterOwner->GetWorld() != nullptr ? CharacterOwner->GetWorld()->GetGameState() : nullptr;
	const float SynchronizedTime =
	    IsValid(GameState) ? GameState->GetServerWorldTimeSeconds() : CharacterOwner->GetWorld()->GetTimeSeconds();
	const float Elapsed = FMath::Max(0.f, SynchronizedTime - ServerStartTime);
	const float MontageLength = Spec->Montage->GetPlayLength();
	const float StartPosition =
	    FMath::Clamp(Elapsed * PlayRate, 0.f, FMath::Max(0.f, MontageLength - KINDA_SMALL_NUMBER));
	const float PlayedDuration = AnimInstance->Montage_Play(
	    Spec->Montage, PlayRate, EMontagePlayReturnType::MontageLength, StartPosition, false);
	if (!FMath::IsFinite(PlayedDuration) || PlayedDuration <= 0.f)
	{
		return false;
	}

	ReplicatedPresentationGeneration = Generation;
	ReplicatedPresentationMontage = Spec->Montage;
	ReplicatedPresentationAnimInstance = AnimInstance;
	ReplicatedPresentationSpec = *Spec;
	++ReplicatedPresentationStartCount;
	return true;
}

void UWuwaActionAnimationCapabilityComponent::StopReplicatedPresentation(const FWuwaNetworkActionGeneration& Generation)
{
	if (Generation.IsValid() && Generation == ReplicatedPresentationGeneration)
	{
		ReleaseReplicatedPresentation();
	}
}

bool UWuwaActionAnimationCapabilityComponent::HasReplicatedPresentation() const
{
	return ReplicatedPresentationGeneration.IsValid() && IsValid(ReplicatedPresentationMontage) &&
	       IsValid(ReplicatedPresentationAnimInstance) &&
	       ReplicatedPresentationAnimInstance->Montage_IsActive(ReplicatedPresentationMontage);
}

FGameplayTag UWuwaActionAnimationCapabilityComponent::GetActionCapabilityTag() const
{
	return WuwaGameplayTags::Action_Capability_Animation;
}

int32 UWuwaActionAnimationCapabilityComponent::GetActionCapabilityCommitOrder() const
{
	return 100;
}

FWuwaActionCapabilityResult
UWuwaActionAnimationCapabilityComponent::PrepareAction(const FWuwaActionPrepareMessage& Message) const
{
	const UWuwaActionDefinition* Definition = Message.Request.Definition;
	const FWuwaActionAnimationSpec* Spec = IsValid(Definition) ? Definition->GetAnimationSpec() : nullptr;
	float PlayRate = 0.f;

	if (Spec != nullptr && IsValid(Spec->Montage) &&
	    Spec->LifetimePolicy == EWuwaActionAnimationLifetimePolicy::ActionControlled &&
	    Spec->Montage->bEnableAutoBlendOut)
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("ActionControlled Montage 必须关闭 Auto Blend Out。Owner=%s Action=%s Montage=%s"),
		       *GetNameSafe(CharacterOwner),
		       IsValid(Definition) ? *Definition->ActionTag.ToString() : TEXT("Invalid"),
		       *GetNameSafe(Spec->Montage));
	}

	if (!Message.Handle.IsValid() || !Message.Request.IsValid() || !IsInitialized() || Spec == nullptr ||
	    !Spec->IsRuntimeValid() || !ResolvePlayRate(Message.Request, *Spec, PlayRate))
	{
		return FWuwaActionCapabilityResult::Failure(EWuwaActionRejectionReason::CapabilityPrepareFailed);
	}

	return FWuwaActionCapabilityResult::Success();
}

FWuwaActionCapabilityResult
UWuwaActionAnimationCapabilityComponent::CommitAction(const FWuwaActionCommitMessage& Message)
{
	if (ActiveHandle.IsValid())
	{
		return FWuwaActionCapabilityResult::Failure(EWuwaActionRejectionReason::CapabilityCommitFailed);
	}

	FWuwaActionPrepareMessage PrepareMessage;
	PrepareMessage.Handle = Message.Handle;
	PrepareMessage.Request = Message.Request;
	const FWuwaActionCapabilityResult PrepareResult = PrepareAction(PrepareMessage);
	if (!PrepareResult.bSucceeded)
	{
		return PrepareResult;
	}

	UWuwaActionDefinition* Definition = Message.Request.Definition;
	const FWuwaActionAnimationSpec* Spec = Definition->GetAnimationSpec();
	UAnimInstance* AnimInstance = ResolveAnimInstance();
	float PlayRate = 1.f;
	ResolvePlayRate(Message.Request, *Spec, PlayRate);

	const float PlayedDuration =
	    AnimInstance->Montage_Play(Spec->Montage, PlayRate, EMontagePlayReturnType::MontageLength, 0.f, false);
	if (!FMath::IsFinite(PlayedDuration) || PlayedDuration <= 0.f)
	{
		return FWuwaActionCapabilityResult::Failure(EWuwaActionRejectionReason::CapabilityCommitFailed);
	}

	ActiveHandle = Message.Handle;
	ActiveActionTag = Definition->ActionTag;
	ActiveMontage = Spec->Montage;
	ActiveAnimInstance = AnimInstance;
	ActiveSpec = *Spec;

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &UWuwaActionAnimationCapabilityComponent::HandleMontageEnded);
	ActiveAnimInstance->Montage_SetEndDelegate(EndDelegate, ActiveMontage);
	return FWuwaActionCapabilityResult::Success();
}

void UWuwaActionAnimationCapabilityComponent::RollbackAction(const FWuwaActionHandle& Handle)
{
	if (Handle == ActiveHandle)
	{
		ReleaseActiveAnimation();
	}
}

void UWuwaActionAnimationCapabilityComponent::StopAction(const FWuwaActionStopMessage& Message)
{
	if (Message.Handle == ActiveHandle)
	{
		ReleaseActiveAnimation();
	}
}

EWuwaActionEndReason UWuwaActionAnimationCapabilityComponent::HandleActionEvent(const FWuwaActionEventMessage& Message)
{
	(void)Message;
	return EWuwaActionEndReason::None;
}

void UWuwaActionAnimationCapabilityComponent::HandleActionFinalized(const FWuwaActionFinalizedMessage& Message)
{
	(void)Message;
}

UAnimInstance* UWuwaActionAnimationCapabilityComponent::ResolveAnimInstance() const
{
	return IsValid(CharacterOwner) && IsValid(CharacterOwner->GetMesh()) ? CharacterOwner->GetMesh()->GetAnimInstance()
	                                                                     : nullptr;
}

bool UWuwaActionAnimationCapabilityComponent::ResolvePlayRate(const FWuwaActionRequest& Request,
                                                              const FWuwaActionAnimationSpec& Spec,
                                                              float& OutPlayRate)
{
	OutPlayRate = 1.f;

	if (!Spec.IsRuntimeValid() || !IsValid(Request.Definition))
	{
		return false;
	}

	const float DesiredDuration = Request.Definition->GetDesiredAnimationDuration();
	if (Spec.bMatchActionDuration && DesiredDuration > 0.f)
	{
		const float MontageLength = Spec.Montage->GetPlayLength();
		OutPlayRate = MontageLength / DesiredDuration;
	}

	return FMath::IsFinite(OutPlayRate) && OutPlayRate > 0.f;
}

void UWuwaActionAnimationCapabilityComponent::ReleaseActiveAnimation()
{
	UAnimInstance* AnimInstance = ActiveAnimInstance;
	UAnimMontage* Montage = ActiveMontage;

	if (IsValid(AnimInstance) && IsValid(Montage))
	{
		FOnMontageEnded EmptyDelegate;
		AnimInstance->Montage_SetEndDelegate(EmptyDelegate, Montage);

		if (AnimInstance->Montage_IsActive(Montage))
		{
			AnimInstance->Montage_Stop(ActiveSpec.ActionEndBlendOutTime, Montage);
		}
	}

	ActiveHandle = FWuwaActionHandle();
	ActiveActionTag = FGameplayTag();
	ActiveMontage = nullptr;
	ActiveAnimInstance = nullptr;
	ActiveSpec = FWuwaActionAnimationSpec();
}

void UWuwaActionAnimationCapabilityComponent::ReleaseReplicatedPresentation()
{
	if (IsValid(ReplicatedPresentationAnimInstance) && IsValid(ReplicatedPresentationMontage) &&
	    ReplicatedPresentationAnimInstance->Montage_IsActive(ReplicatedPresentationMontage))
	{
		ReplicatedPresentationAnimInstance->Montage_Stop(ReplicatedPresentationSpec.ActionEndBlendOutTime,
		                                                 ReplicatedPresentationMontage);
	}

	ReplicatedPresentationGeneration = FWuwaNetworkActionGeneration();
	ReplicatedPresentationMontage = nullptr;
	ReplicatedPresentationAnimInstance = nullptr;
	ReplicatedPresentationSpec = FWuwaActionAnimationSpec();
}

void UWuwaActionAnimationCapabilityComponent::HandleMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
	if (!ActiveHandle.IsValid() || Montage != ActiveMontage || !Dispatcher.IsValid())
	{
		return;
	}

	const bool bUnexpectedActionControlledEnd =
	    !bInterrupted && ActiveSpec.LifetimePolicy == EWuwaActionAnimationLifetimePolicy::ActionControlled;
	if (bUnexpectedActionControlledEnd)
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("ActionControlled Montage 意外自然结束。Owner=%s Handle=%lld Action=%s Montage=%s"),
		       *GetNameSafe(CharacterOwner),
		       ActiveHandle.Value,
		       *ActiveActionTag.ToString(),
		       *GetNameSafe(Montage));
	}

	Dispatcher->PublishActionEvent(ActiveHandle,
	                               bInterrupted || bUnexpectedActionControlledEnd
	                                   ? WuwaGameplayTags::Action_Event_Animation_Interrupted
	                                   : WuwaGameplayTags::Action_Event_Animation_Completed,
	                               Dispatcher->GetCurrentMoveIntent(),
	                               MOVE_None,
	                               MOVE_None,
	                               0,
	                               0,
	                               this);
}

void UWuwaActionAnimationCapabilityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseActiveAnimation();
	ReleaseReplicatedPresentation();
	Dispatcher = nullptr;
	CharacterOwner = nullptr;

	Super::EndPlay(EndPlayReason);
}
