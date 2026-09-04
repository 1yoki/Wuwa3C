#include "Movement/WuwaCharacterMovementComponent.h"

#include "Core/WuwaStateTagComponent.h"
#include "Core/WuwaGameplayTags.h"
#include "Movement/Network/WuwaSavedMoveCharacter.h"
#include "Movement/WuwaMovementProfile.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "WuwaCharacter.h"
#include "AbilitySystem/Interop/WuwaActionAbilityInteropComponent.h"
#include "AbilitySystem/Contracts/WuwaAbilityTypes.h"
#include "Movement/Actions/WuwaMovementActionCapabilityComponent.h"
#include "Targeting/WuwaTargetingComponent.h"
#include "Wuwa.h"

namespace
{
// 避免浮点抖动让 Sprint Run 在 RunSpeed 附近多停留一帧。
constexpr float SprintRunExitSpeedTolerance = 5.f;
}

UWuwaCharacterMovementComponent::UWuwaCharacterMovementComponent()
{
	SetIsReplicatedByDefault(true);
	SetNetworkMoveDataContainer(WuwaNetworkMoveDataContainer);
	SetMoveResponseDataContainer(WuwaMoveResponseDataContainer);

	MaxWalkSpeed = ConfiguredRunSpeed;
	MaxAcceleration = 2200.f;
	BrakingDecelerationWalking = 1500.f;
	BrakingDecelerationFalling = 1500.f;
	GroundFriction = 8.f;
	BrakingFrictionFactor = 2.f;
	RotationRate = FRotator(0.f, 720.f, 0.f);
	AirControl = 0.4f;
}

void UWuwaCharacterMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UWuwaCharacterMovementComponent, ReplicatedLocomotionPresentationState, COND_SimulatedOnly);
}

void UWuwaCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	const double CurrentTime = GetMovementTime();

	if (IsMovingOnGround())
	{
		// 初始化土狼时间使用的接地时间
		AirActionState.LastGroundedTime = CurrentTime;
	}

	if (CharacterOwner)
	{
		// 初始化本次空中的高度记录
		AirActionState.FallStartHeight = CharacterOwner->GetActorLocation().Z;
	}

	RefreshLocomotionState();
}

void UWuwaCharacterMovementComponent::SetStateTagComponent(UWuwaStateTagComponent* InStateTagComponent)
{
	if (StateTagComponent == InStateTagComponent)
	{
		return;
	}
	// 切换组件前释放旧标签引用
	if (SprintingTagHandle.IsValid() && StateTagComponent)
	{
		StateTagComponent->ReleaseTag(SprintingTagHandle);
	}

	StateTagComponent = InStateTagComponent;

	// 保持新组件中的标签与当前状态一致。
	if (bIsSprinting && StateTagComponent)
	{
		SprintingTagHandle = StateTagComponent->AcquireTag(WuwaGameplayTags::State_Locomotion_Sprinting);
	}

	RefreshLocomotionState();
}

bool UWuwaCharacterMovementComponent::SetTargetingComponent(UWuwaTargetingComponent* InTargetingComponent)
{
	TargetingComponent.Reset();

	if (!IsValid(InTargetingComponent) || !InTargetingComponent->IsInitialized() ||
	    InTargetingComponent->GetOwner() != GetOwner())
	{
		// Movement 只能读取同一角色装配的 Targeting，禁止跨 Actor 消费目标事实。
		return false;
	}

	TargetingComponent = InTargetingComponent;
	return true;
}

bool UWuwaCharacterMovementComponent::QueueActionFacing(const float DesiredFacingYaw, const bool bAllowSnap)
{
	if (!HasValidData() || !IsValid(CharacterOwner) || !FMath::IsFinite(DesiredFacingYaw))
	{
		return false;
	}

	ActiveNetworkFacingYaw = FRotator::NormalizeAxis(DesiredFacingYaw);
	bHasActiveNetworkFacing = true;
	bAllowActiveNetworkFacingSnap = bAllowSnap;
	if (bAllowSnap)
	{
		PhysicsRotation(0.f);
	}
	return true;
}

bool UWuwaCharacterMovementComponent::QueueNetworkMovementCommand(const FWuwaPendingNetworkMovementCommand& Command)
{
	if (!Command.IsPayloadValid() || Command.Kind == EWuwaNetworkMovementCommandKind::None)
	{
		UE_LOG(LogWuwa, Warning, TEXT("拒绝无效网络移动命令。Owner=%s"), *GetNameSafe(CharacterOwner));
		return false;
	}

	if (PendingNetworkMovementCommand.Kind != EWuwaNetworkMovementCommandKind::None)
	{
		UE_LOG(LogWuwa,
		       Warning,
		       TEXT("网络移动命令槽已经占用。Owner=%s, PendingGeneration=%d, RejectedGeneration=%d"),
		       *GetNameSafe(CharacterOwner),
		       PendingNetworkMovementCommand.Generation.Value,
		       Command.Generation.Value);
		return false;
	}

	PendingNetworkMovementCommand = Command;
	return true;
}

bool UWuwaCharacterMovementComponent::CanQueueNetworkMovementCommand() const
{
	return PendingNetworkMovementCommand.Kind == EWuwaNetworkMovementCommandKind::None;
}

FWuwaNetworkActionGeneration UWuwaCharacterMovementComponent::AllocateNetworkActionGeneration()
{
	FWuwaNetworkActionGeneration Generation;
	if (NextLocalNetworkGeneration <= 0 || NextLocalNetworkGeneration == MAX_int32)
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("网络动作 Generation 已耗尽。Owner=%s, Next=%d"),
		       *GetNameSafe(CharacterOwner),
		       NextLocalNetworkGeneration);
		return Generation;
	}

	Generation.Value = NextLocalNetworkGeneration++;
	return Generation;
}

FWuwaPendingNetworkMovementCommand UWuwaCharacterMovementComponent::GetPendingNetworkMovementCommandForSavedMove() const
{
	FWuwaPendingNetworkMovementCommand Command = PendingNetworkMovementCommand;
	if (bIsSprinting)
	{
		Command.Flags |= static_cast<uint8>(EWuwaNetworkMovementCommandFlags::SprintRun);
	}
	return Command;
}

FWuwaPendingNetworkMovementCommand UWuwaCharacterMovementComponent::CapturePendingNetworkMovementCommandForSavedMove()
{
	LocalCapturedNetworkMovementCommand = GetPendingNetworkMovementCommandForSavedMove();
	PendingNetworkMovementCommand.Reset();
	return LocalCapturedNetworkMovementCommand;
}

bool UWuwaCharacterMovementComponent::PrepareMovementActionRootMotionStartAtMoveBoundary(
    const FWuwaPendingNetworkMovementCommand& Command, const float MoveTimeStamp, const float DeltaTime)
{
	if (Command.Kind != EWuwaNetworkMovementCommandKind::LegacyAction)
	{
		return true;
	}

	const bool bValidMoveTime = FMath::IsFinite(MoveTimeStamp) && FMath::IsFinite(DeltaTime) && DeltaTime > 0.f;
	const float MoveStartTime = MoveTimeStamp - DeltaTime;
	if (!bValidMoveTime || !FMath::IsFinite(MoveStartTime) || MoveStartTime < 0.f)
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("MovementAction RMS Move 边界时间无效。Owner=%s, Generation=%d, Action=%s, MoveTimeStamp=%.6f, "
		            "DeltaTime=%.6f, MoveStartTime=%.6f"),
		       *GetNameSafe(CharacterOwner),
		       Command.Generation.Value,
		       *Command.ActionTag.ToString(),
		       MoveTimeStamp,
		       DeltaTime,
		       MoveStartTime);
		return false;
	}

	const bool bIsLocalPrediction = IsValid(CharacterOwner) && CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy &&
	                                CharacterOwner->IsLocallyControlled();
	const bool bIsRemoteAuthority =
	    IsValid(CharacterOwner) && CharacterOwner->HasAuthority() && !CharacterOwner->IsLocallyControlled();
	if (!bIsLocalPrediction && !bIsRemoteAuthority)
	{
		return true;
	}

	if (!MovementActionNetworkProvider.IsValid())
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("MovementAction RMS Move 边界缺少网络物理提供者。Owner=%s, Generation=%d, Action=%s, "
		            "LocalPrediction=%d, RemoteAuthority=%d"),
		       *GetNameSafe(CharacterOwner),
		       Command.Generation.Value,
		       *Command.ActionTag.ToString(),
		       bIsLocalPrediction ? 1 : 0,
		       bIsRemoteAuthority ? 1 : 0);
		return false;
	}

	const bool bPrepared =
	    bIsLocalPrediction
	        ? MovementActionNetworkProvider->ValidateLocalPredictedRootMotionMoveStart(Command, MoveStartTime)
	        : MovementActionNetworkProvider->AlignAuthorityRootMotionMoveStart(Command, MoveStartTime);
	if (!bPrepared)
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("MovementAction RMS Move 边界准备失败。Owner=%s, Generation=%d, Action=%s, MoveTimeStamp=%.6f, "
		            "DeltaTime=%.6f, MoveStartTime=%.6f, LocalPrediction=%d, RemoteAuthority=%d"),
		       *GetNameSafe(CharacterOwner),
		       Command.Generation.Value,
		       *Command.ActionTag.ToString(),
		       MoveTimeStamp,
		       DeltaTime,
		       MoveStartTime,
		       bIsLocalPrediction ? 1 : 0,
		       bIsRemoteAuthority ? 1 : 0);
		return false;
	}
	return true;
}

bool UWuwaCharacterMovementComponent::ApplyLocalPredictedMovementActionExitAtSavedMoveBoundary(
    const FWuwaPendingNetworkMovementCommand& Command)
{
	if (!Command.IsLegacyActionExit())
	{
		return true;
	}

	const bool bCanApplyLocalPrediction =
	    IsValid(CharacterOwner) && CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy &&
	    CharacterOwner->IsLocallyControlled() && MovementActionNetworkProvider.IsValid();
	if (!bCanApplyLocalPrediction || !MovementActionNetworkProvider->ApplyNetworkActionExitTransition(Command, false))
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Owning Client 无法在 SavedMove 边界应用 MovementAction 退出。Owner=%s, ExitGeneration=%d, "
		            "TargetGeneration=%d, Action=%s"),
		       *GetNameSafe(CharacterOwner),
		       Command.Generation.Value,
		       Command.TargetActionGeneration.Value,
		       *Command.ActionTag.ToString());
		return false;
	}
	return true;
}

void UWuwaCharacterMovementComponent::RestoreNetworkMovementCommandForReplay(
    const FWuwaPendingNetworkMovementCommand& Command)
{
	ReplayNetworkMovementCommand = Command;
	bReplayingWuwaJump =
	    Command.Kind == EWuwaNetworkMovementCommandKind::Jump && Command.JumpType == EWuwaJumpRequestKind::Pressed;
}

bool UWuwaCharacterMovementComponent::BindNetworkCommandProcessor(UObject* BindingOwner,
                                                                  const FWuwaNetworkCommandProcessor& Processor)
{
	if (!IsValid(BindingOwner) || !Processor.IsBound())
	{
		UE_LOG(LogWuwa, Error, TEXT("网络移动命令处理器绑定失败。Owner=%s"), *GetNameSafe(CharacterOwner));
		return false;
	}
	if (NetworkCommandProcessor.IsBound() && NetworkCommandProcessorOwner.Get() != BindingOwner)
	{
		UE_LOG(LogWuwa, Error, TEXT("网络移动命令处理器重复绑定。Owner=%s"), *GetNameSafe(CharacterOwner));
		return false;
	}

	NetworkCommandProcessorOwner = BindingOwner;
	NetworkCommandProcessor = Processor;
	return true;
}

void UWuwaCharacterMovementComponent::UnbindNetworkCommandProcessor(const UObject* BindingOwner)
{
	if (NetworkCommandProcessorOwner.Get() != BindingOwner)
	{
		return;
	}
	NetworkCommandProcessor.Unbind();
	NetworkCommandProcessorOwner.Reset();
}

bool UWuwaCharacterMovementComponent::BindNetworkMoveResponseConsumer(UObject* BindingOwner,
                                                                      const FWuwaNetworkMoveResponseConsumer& Consumer)
{
	if (!IsValid(BindingOwner) || !Consumer.IsBound())
	{
		UE_LOG(LogWuwa, Error, TEXT("网络移动响应消费者绑定失败。Owner=%s"), *GetNameSafe(CharacterOwner));
		return false;
	}
	if (NetworkMoveResponseConsumer.IsBound() && NetworkMoveResponseConsumerOwner.Get() != BindingOwner)
	{
		UE_LOG(LogWuwa, Error, TEXT("网络移动响应消费者重复绑定。Owner=%s"), *GetNameSafe(CharacterOwner));
		return false;
	}

	NetworkMoveResponseConsumerOwner = BindingOwner;
	NetworkMoveResponseConsumer = Consumer;
	return true;
}

void UWuwaCharacterMovementComponent::UnbindNetworkMoveResponseConsumer(const UObject* BindingOwner)
{
	if (NetworkMoveResponseConsumerOwner.Get() != BindingOwner)
	{
		return;
	}
	NetworkMoveResponseConsumer.Unbind();
	NetworkMoveResponseConsumerOwner.Reset();
	NetworkMoveReplayConsumer.Unbind();
	NetworkMoveReplayConsumerOwner.Reset();
}

bool UWuwaCharacterMovementComponent::BindNetworkMoveReplayConsumer(UObject* BindingOwner,
                                                                    const FWuwaNetworkMoveReplayConsumer& Consumer)
{
	if (!IsValid(BindingOwner) || !Consumer.IsBound())
	{
		UE_LOG(LogWuwa, Error, TEXT("网络移动重演消费者绑定失败。Owner=%s"), *GetNameSafe(CharacterOwner));
		return false;
	}
	if (NetworkMoveReplayConsumer.IsBound() && NetworkMoveReplayConsumerOwner.Get() != BindingOwner)
	{
		UE_LOG(LogWuwa, Error, TEXT("网络移动重演消费者重复绑定。Owner=%s"), *GetNameSafe(CharacterOwner));
		return false;
	}

	NetworkMoveReplayConsumerOwner = BindingOwner;
	NetworkMoveReplayConsumer = Consumer;
	return true;
}

void UWuwaCharacterMovementComponent::UnbindNetworkMoveReplayConsumer(const UObject* BindingOwner)
{
	if (NetworkMoveReplayConsumerOwner.Get() != BindingOwner)
	{
		return;
	}
	NetworkMoveReplayConsumer.Unbind();
	NetworkMoveReplayConsumerOwner.Reset();
}

bool UWuwaCharacterMovementComponent::BindMovementActionNetworkProvider(
    UWuwaMovementActionCapabilityComponent* InProvider)
{
	if (!IsValid(InProvider) || InProvider->GetOwner() != CharacterOwner)
	{
		UE_LOG(LogWuwa, Error, TEXT("MovementAction 重演入口绑定失败。Owner=%s"), *GetNameSafe(CharacterOwner));
		return false;
	}
	if (MovementActionNetworkProvider.IsValid() && MovementActionNetworkProvider.Get() != InProvider)
	{
		UE_LOG(LogWuwa, Error, TEXT("MovementAction 重演入口重复绑定。Owner=%s"), *GetNameSafe(CharacterOwner));
		return false;
	}

	MovementActionNetworkProvider = InProvider;
	return true;
}

void UWuwaCharacterMovementComponent::UnbindMovementActionNetworkProvider(
    const UWuwaMovementActionCapabilityComponent* InProvider)
{
	if (MovementActionNetworkProvider.Get() != InProvider)
	{
		if (MovementActionNetworkProvider.IsValid())
		{
			UE_LOG(
			    LogWuwa, Warning, TEXT("MovementAction 重演入口解绑方不匹配。Owner=%s"), *GetNameSafe(CharacterOwner));
		}
		return;
	}
	MovementActionNetworkProvider.Reset();
}

void UWuwaCharacterMovementComponent::CaptureMovementActionNetworkReplayState(
    FWuwaMovementActionNetworkReplayState& OutState) const
{
	OutState.Reset();
	if (MovementActionNetworkProvider.IsValid())
	{
		MovementActionNetworkProvider->CaptureNetworkReplayState(OutState);
		return;
	}

	OutState.bSavedOrientRotationToMovement = bOrientRotationToMovement;
	OutState.bSavedUseControllerDesiredRotation = bUseControllerDesiredRotation;
	OutState.bSavedUseControllerRotationYaw = IsValid(CharacterOwner) && CharacterOwner->bUseControllerRotationYaw;
}

void UWuwaCharacterMovementComponent::RestoreMovementActionNetworkReplayState(
    const FWuwaMovementActionNetworkReplayState& State)
{
	if (MovementActionNetworkProvider.IsValid())
	{
		MovementActionNetworkProvider->RestoreNetworkReplayState(State);
		return;
	}

	if (State.bMovementActionActive)
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("活动 MovementAction SavedMove 缺少物理重演入口。Owner=%s, Generation=%d, Action=%s"),
		       *GetNameSafe(CharacterOwner),
		       State.Generation.Value,
		       *State.ActionTag.ToString());
	}
}

FWuwaNetworkActionResponse UWuwaCharacterMovementComponent::TakePendingNetworkActionResponse()
{
	const FWuwaNetworkActionResponse Response = PendingNetworkActionResponse;
	PendingNetworkActionResponse = FWuwaNetworkActionResponse();
	return Response;
}

FNetworkPredictionData_Client* UWuwaCharacterMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UWuwaCharacterMovementComponent* MutableThis = const_cast<UWuwaCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FWuwaNetworkPredictionData_Client_Character(*this);
	}
	return ClientPredictionData;
}

void UWuwaCharacterMovementComponent::MoveAutonomous(const float ClientTimeStamp,
                                                     const float DeltaTime,
                                                     const uint8 CompressedFlags,
                                                     const FVector& NewAcceleration)
{
	FWuwaPendingNetworkMovementCommand Command = GetPendingNetworkMovementCommandForSavedMove();
	bool bUsedDirectPendingCommand = true;
	bool bIsReplay = false;
	const FWuwaCharacterNetworkMoveData* MoveData =
	    static_cast<const FWuwaCharacterNetworkMoveData*>(GetCurrentNetworkMoveData());
	if (MoveData)
	{
		Command = MoveData->NetworkCommand;
		bUsedDirectPendingCommand = false;
	}
	else if (ReplayNetworkMovementCommand.HasData())
	{
		Command = ReplayNetworkMovementCommand;
		bUsedDirectPendingCommand = false;
		bIsReplay = true;
	}
	else if (LocalCapturedNetworkMovementCommand.HasData())
	{
		Command = LocalCapturedNetworkMovementCommand;
		bUsedDirectPendingCommand = false;
	}

	ApplyNetworkMovementCommand(Command, bIsReplay);
	const bool bAcceptedAuthorityLegacyAction = IsValid(CharacterOwner) && CharacterOwner->HasAuthority() &&
	                                            Command.Kind == EWuwaNetworkMovementCommandKind::LegacyAction &&
	                                            PendingNetworkActionResponse.bHasResponse &&
	                                            PendingNetworkActionResponse.bAccepted &&
	                                            PendingNetworkActionResponse.ProcessedGeneration == Command.Generation;
	FWuwaMovementActionNetworkReplayState AuthorityMovementActionState;
	if (bAcceptedAuthorityLegacyAction && MovementActionNetworkProvider.IsValid())
	{
		MovementActionNetworkProvider->CaptureNetworkReplayState(AuthorityMovementActionState);
	}
	const bool bAcceptedAuthorityRootMotionAction =
	    bAcceptedAuthorityLegacyAction && AuthorityMovementActionState.bMovementActionActive &&
	    AuthorityMovementActionState.Generation == Command.Generation &&
	    AuthorityMovementActionState.ActionTag == Command.ActionTag &&
	    AuthorityMovementActionState.Driver == EWuwaMovementActionDriver::RootMotionSource;
	if (bAcceptedAuthorityRootMotionAction)
	{
		PrepareMovementActionRootMotionStartAtMoveBoundary(Command, ClientTimeStamp, DeltaTime);
	}
	if (bUsedDirectPendingCommand)
	{
		PendingNetworkMovementCommand.Reset();
	}
	ReplayNetworkMovementCommand.Reset();
	LocalCapturedNetworkMovementCommand.Reset();
	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAcceleration);
}

void UWuwaCharacterMovementComponent::ClientHandleMoveResponse(const FCharacterMoveResponseDataContainer& MoveResponse)
{
	Super::ClientHandleMoveResponse(MoveResponse);

	if (&MoveResponse != &WuwaMoveResponseDataContainer)
	{
		UE_LOG(LogWuwa, Error, TEXT("收到非 Wuwa MoveResponse 容器。Owner=%s"), *GetNameSafe(CharacterOwner));
		return;
	}

	const FWuwaNetworkActionResponse& Response = WuwaMoveResponseDataContainer.NetworkResponse;
	if (!Response.bHasResponse)
	{
		return;
	}

	const bool bMatchesJumpRequest = Response.ProcessedGeneration.Value == AirActionState.JumpRequestGeneration ||
	                                 Response.ProcessedGeneration.Value == AirActionState.AirCycleGeneration ||
	                                 Response.ProcessedGeneration.Value == AirActionState.BufferedJumpGeneration;
	if (bMatchesJumpRequest)
	{
		if (!Response.bAccepted)
		{
			if (IsValid(CharacterOwner))
			{
				CharacterOwner->StopJumping();
			}
			if (Response.ProcessedGeneration.Value == AirActionState.BufferedJumpGeneration)
			{
				AirActionState.BufferedJumpRemainingTime = 0.f;
			}
			UE_LOG(LogWuwa,
			       Warning,
			       TEXT("服务端拒绝普通跳跃请求。Owner=%s, Generation=%d, Reason=%d"),
			       *GetNameSafe(CharacterOwner),
			       Response.ProcessedGeneration.Value,
			       static_cast<int32>(Response.RejectReason));
		}
		return;
	}

	if (!NetworkMoveResponseConsumerOwner.IsValid() || !NetworkMoveResponseConsumer.IsBound())
	{
		UE_LOG(LogWuwa,
		       Warning,
		       TEXT("网络移动动作响应没有消费者。Owner=%s, Generation=%d"),
		       *GetNameSafe(CharacterOwner),
		       Response.ProcessedGeneration.Value);
		return;
	}
	NetworkMoveResponseConsumer.Execute(Response);
}

void UWuwaCharacterMovementComponent::ApplyNetworkMovementCommand(const FWuwaPendingNetworkMovementCommand& Command,
                                                                  const bool bIsReplay)
{
	if (Command.HasData() && !Command.IsPayloadValid())
	{
		UE_LOG(LogWuwa, Warning, TEXT("移动模拟拒绝无效扩展数据。Owner=%s"), *GetNameSafe(CharacterOwner));
		if (CharacterOwner && CharacterOwner->HasAuthority() && Command.Generation.IsValid())
		{
			PendingNetworkActionResponse = FWuwaNetworkActionResponse::Rejected(
			    Command.Generation, EWuwaNetworkActionRejectReason::InvalidPayload);
			PendingNetworkActionResponse.AuthorityGeneration = LastProcessedNetworkGeneration;
			PendingNetworkActionResponse.AuthorityMovementMode = PackNetworkMovementMode();
		}
		return;
	}

	SetSprinting(Command.HasFlag(EWuwaNetworkMovementCommandFlags::SprintRun));

	if (Command.HasFlag(EWuwaNetworkMovementCommandFlags::HasDesiredFacing))
	{
		QueueActionFacing(Command.DesiredFacingYaw,
		                  Command.IsOneShot() && Command.HasFlag(EWuwaNetworkMovementCommandFlags::AllowFacingSnap));
	}

	if (CharacterOwner && !CharacterOwner->HasAuthority() && Command.Kind == EWuwaNetworkMovementCommandKind::Jump)
	{
		AirActionState.JumpRequestGeneration = Command.Generation.Value;
		AirActionState.JumpRequestKind = Command.JumpType;
		if (Command.JumpType == EWuwaJumpRequestKind::Pressed)
		{
			if (CanAttemptWuwaJump())
			{
				CharacterOwner->Jump();
			}
			else
			{
				QueueBufferedJump(Command.Generation.Value, ConfiguredJumpBufferTime);
			}
		}
		else if (Command.JumpType == EWuwaJumpRequestKind::Released)
		{
			CharacterOwner->StopJumping();
		}
	}

	if (CharacterOwner && !CharacterOwner->HasAuthority() && bIsReplay &&
	    (Command.Kind == EWuwaNetworkMovementCommandKind::LegacyAction ||
	     Command.Kind == EWuwaNetworkMovementCommandKind::Grapple))
	{
		if (NetworkMoveReplayConsumerOwner.IsValid() && NetworkMoveReplayConsumer.IsBound())
		{
			NetworkMoveReplayConsumer.Execute(Command);
		}
		else
		{
			UE_LOG(LogWuwa,
			       Warning,
			       TEXT("Action 校正重演缺少消费者。Owner=%s, Generation=%d"),
			       *GetNameSafe(CharacterOwner),
			       Command.Generation.Value);
		}
	}

	FWuwaNetworkActionResponse AuthorityResponse;
	if (CharacterOwner && CharacterOwner->HasAuthority() && Command.Kind != EWuwaNetworkMovementCommandKind::None)
	{
		AuthorityResponse = ProcessAuthorityNetworkCommand(Command);
	}

	const bool bShouldApplyLegacyActionExit = Command.Kind == EWuwaNetworkMovementCommandKind::LegacyActionExit &&
	                                          CharacterOwner &&
	                                          (!CharacterOwner->HasAuthority() || AuthorityResponse.bAccepted);
	if (bShouldApplyLegacyActionExit)
	{
		const bool bApplied = MovementActionNetworkProvider.IsValid() &&
		                      MovementActionNetworkProvider->ApplyNetworkActionExitTransition(Command, bIsReplay);
		if (!bApplied)
		{
			UE_LOG(LogWuwa,
			       Error,
			       TEXT("CharacterMovement 无法应用 MovementAction 退出。Owner=%s, ExitGeneration=%d, "
			            "TargetGeneration=%d, Action=%s, Replay=%d"),
			       *GetNameSafe(CharacterOwner),
			       Command.Generation.Value,
			       Command.TargetActionGeneration.Value,
			       *Command.ActionTag.ToString(),
			       bIsReplay ? 1 : 0);
			if (CharacterOwner->HasAuthority())
			{
				PendingNetworkActionResponse = FWuwaNetworkActionResponse::Rejected(
				    Command.Generation, EWuwaNetworkActionRejectReason::ProcessorRejected);
				PendingNetworkActionResponse.AuthorityGeneration = LastProcessedNetworkGeneration;
				PendingNetworkActionResponse.AuthorityMovementMode = PackNetworkMovementMode();
			}
		}
	}
}

FWuwaNetworkActionResponse
UWuwaCharacterMovementComponent::ProcessAuthorityNetworkCommand(const FWuwaPendingNetworkMovementCommand& Command)
{
	FWuwaNetworkActionResponse Response;
	if (Command.Generation.Value < LastProcessedNetworkGeneration.Value)
	{
		Response =
		    FWuwaNetworkActionResponse::Rejected(Command.Generation, EWuwaNetworkActionRejectReason::StaleGeneration);
	}
	else if (Command.Generation == LastProcessedNetworkGeneration)
	{
		Response = FWuwaNetworkActionResponse::Rejected(Command.Generation,
		                                                EWuwaNetworkActionRejectReason::DuplicateGeneration);
	}
	else if (Command.Kind == EWuwaNetworkMovementCommandKind::Jump)
	{
		AirActionState.JumpRequestGeneration = Command.Generation.Value;
		AirActionState.JumpRequestKind = Command.JumpType;

		bool bAccepted = false;
		if (Command.JumpType == EWuwaJumpRequestKind::Pressed)
		{
			if (TryInterruptAbilityForJump() && CanAttemptWuwaJump())
			{
				CharacterOwner->Jump();
				bAccepted = true;
			}
			else
			{
				bAccepted = QueueBufferedJump(Command.Generation.Value, ConfiguredJumpBufferTime);
			}
		}
		else if (Command.JumpType == EWuwaJumpRequestKind::Released)
		{
			CharacterOwner->StopJumping();
			bAccepted = true;
		}

		Response = bAccepted ? FWuwaNetworkActionResponse::Accepted(Command.Generation)
		                     : FWuwaNetworkActionResponse::Rejected(Command.Generation,
		                                                            EWuwaNetworkActionRejectReason::ProcessorRejected);
		LastProcessedNetworkGeneration = Command.Generation;
	}
	else if (!NetworkCommandProcessorOwner.IsValid() || !NetworkCommandProcessor.IsBound())
	{
		Response = FWuwaNetworkActionResponse::Rejected(Command.Generation,
		                                                EWuwaNetworkActionRejectReason::ProcessorUnavailable);
		LastProcessedNetworkGeneration = Command.Generation;
	}
	else
	{
		Response = NetworkCommandProcessor.Execute(Command);
		if (!Response.bHasResponse || Response.ProcessedGeneration != Command.Generation)
		{
			UE_LOG(LogWuwa,
			       Error,
			       TEXT("网络移动命令处理器返回了无效响应。Owner=%s, Generation=%d"),
			       *GetNameSafe(CharacterOwner),
			       Command.Generation.Value);
			Response = FWuwaNetworkActionResponse::Rejected(Command.Generation,
			                                                EWuwaNetworkActionRejectReason::ProcessorRejected);
		}
		LastProcessedNetworkGeneration = Command.Generation;
	}

	if (Command.Kind == EWuwaNetworkMovementCommandKind::Jump && !Response.bAccepted && IsValid(CharacterOwner))
	{
		CharacterOwner->StopJumping();
	}

	if (!Response.bAccepted)
	{
		bHasActiveNetworkFacing = false;
		bAllowActiveNetworkFacingSnap = false;
	}

	Response.AuthorityGeneration = LastProcessedNetworkGeneration;
	Response.AuthorityMovementMode = PackNetworkMovementMode();
	PendingNetworkActionResponse = Response;
	return Response;
}

double UWuwaCharacterMovementComponent::GetMovementTime() const
{
	return GetWorld() ? static_cast<double>(GetWorld()->GetTimeSeconds()) : 0.0;
}

void UWuwaCharacterMovementComponent::PhysicsRotation(const float DeltaTime)
{
	if (!bHasActiveNetworkFacing)
	{
		Super::PhysicsRotation(DeltaTime);
		return;
	}

	if (!HasValidData() || !FMath::IsFinite(ActiveNetworkFacingYaw))
	{
		UE_LOG(LogWuwa, Warning, TEXT("网络朝向包含无效值，已拒绝。Owner=%s"), *GetNameSafe(CharacterOwner));
		bHasActiveNetworkFacing = false;
		bAllowActiveNetworkFacingSnap = false;
		return;
	}

	FRotator CurrentRotation = UpdatedComponent->GetComponentRotation();
	FRotator DesiredRotation = CurrentRotation;
	const float MaximumYawDelta =
	    bAllowActiveNetworkFacingSnap ? 360.f : FMath::Max(0.f, RotationRate.Yaw) * FMath::Max(0.f, DeltaTime);
	DesiredRotation.Yaw =
	    FMath::FixedTurn(FRotator::NormalizeAxis(CurrentRotation.Yaw), ActiveNetworkFacingYaw, MaximumYawDelta);
	DesiredRotation.Pitch = 0.f;
	DesiredRotation.Roll = 0.f;
	MoveUpdatedComponent(FVector::ZeroVector, DesiredRotation, false);

	bHasActiveNetworkFacing = false;
	bAllowActiveNetworkFacingSnap = false;
}

FRotator UWuwaCharacterMovementComponent::ComputeOrientToMovementRotation(const FRotator& CurrentRotation,
                                                                          float DeltaTime,
                                                                          FRotator& DeltaRotation) const
{
	const UWuwaTargetingComponent* Targeting = TargetingComponent.Get();

	if (!IsValid(Targeting) || !Targeting->IsInitialized())
	{
		return Super::ComputeOrientToMovementRotation(CurrentRotation, DeltaTime, DeltaRotation);
	}

	const FWuwaTargetContext TargetContext = Targeting->GetTargetContext();

	if (TargetContext.Mode != EWuwaTargetingMode::Hard || !TargetContext.TargetActor.IsValid())
	{
		// Soft Lock 只提供攻击参考，不拥有角色朝向。
		return Super::ComputeOrientToMovementRotation(CurrentRotation, DeltaTime, DeltaRotation);
	}

	if (!IsValid(CharacterOwner))
	{
		return CurrentRotation;
	}

	const bool bMovementInputBlocked =
	    IsValid(StateTagComponent.Get()) && StateTagComponent->HasTag(WuwaGameplayTags::Block_Input_Move, true);

	const bool bUseHardTargetFacing = IsMovingOnGround() && !bMovementInputBlocked;

	FVector DesiredFacingDirection = FVector::ZeroVector;

	if (bUseHardTargetFacing)
	{
		// 地面普通 Locomotion 的角色朝向由 Hard Target Context 决定。
		DesiredFacingDirection = TargetContext.TargetPoint - CharacterOwner->GetActorLocation();
	}
	else
	{
		// 空中或动作阻断期间不消费目标朝向，
		// 改用 Movement/RMS 已经提交的真实水平速度。
		DesiredFacingDirection = Velocity;
	}

	DesiredFacingDirection.Z = 0.f;

	const bool bFiniteDirection =
	    FMath::IsFinite(DesiredFacingDirection.X) && FMath::IsFinite(DesiredFacingDirection.Y);

	if (!bFiniteDirection || DesiredFacingDirection.IsNearlyZero())
	{
		// 没有可靠平面方向时保持当前 Yaw，避免退化为错误的目标或移动朝向。
		return CurrentRotation;
	}

	FRotator DesiredRotation = CurrentRotation;
	DesiredRotation.Yaw = DesiredFacingDirection.Rotation().Yaw;

	// 不在这里插值；UE PhysicsRotation 会使用 Profile 已应用的 RotationRate。
	return DesiredRotation;

	/*
    FVector ToTarget = TargetContext.TargetPoint - CharacterOwner->GetActorLocation();

    // 锁敌朝向只改变平面 Yaw，目标高度不能让胶囊产生 Pitch/Roll。
    ToTarget.Z = 0.f;

    const bool bFiniteDirection = FMath::IsFinite(ToTarget.X) && FMath::IsFinite(ToTarget.Y);

    if (!bFiniteDirection || ToTarget.IsNearlyZero())
    {
        // Hard Context 尚未被周期校验清理时保持当前朝向，不能退化为面向移动方向。
        return CurrentRotation;
    }

    FRotator DesiredRotation = CurrentRotation;
    DesiredRotation.Yaw = ToTarget.Rotation().Yaw;

    // 不在这里插值；UE PhysicsRotation 会使用 Profile 已应用的 RotationRate。
    return DesiredRotation;
    */
}

// 判断角色离开地面后是否仍处于允许普通跳跃的时间窗口。
bool UWuwaCharacterMovementComponent::IsWithCoyoteTime(const double CurrentTime) const
{
	if (AirActionState.LastGroundedTime < 0.0)
	{
		return false;
	}

	const double TimeSinceGrounded = CurrentTime - AirActionState.LastGroundedTime;

	return TimeSinceGrounded >= 0.0 && TimeSinceGrounded <= ConfiguredCoyoteTime;
}

bool UWuwaCharacterMovementComponent::IsJumpBlockedByCombatState() const
{
	return IsValid(StateTagComponent) && (StateTagComponent->HasTag(WuwaGameplayTags::State_Combat_Attacking, true) ||
	                                      StateTagComponent->HasTag(WuwaGameplayTags::Block_Input_Move, true) ||
	                                      StateTagComponent->HasTag(WuwaGameplayTags::State_Combat_Dead, true));
}

bool UWuwaCharacterMovementComponent::RequestJump()
{
	if (!CharacterOwner || !HasValidData())
	{
		return false;
	}

	if (IsJumpBlockedByCombatState())
	{
		return false;
	}

	if (HasGrappleRuntime() ||
	    (IsValid(StateTagComponent) && StateTagComponent->HasTag(WuwaGameplayTags::State_Traversal_Grappling, true)))
	{
		return false;
	}

	const FWuwaNetworkActionGeneration Generation = AllocateNetworkActionGeneration();
	if (!Generation.IsValid())
	{
		return false;
	}
	const int32 RequestGeneration = Generation.Value;
	AirActionState.JumpRequestGeneration = Generation.Value;
	AirActionState.JumpRequestKind = EWuwaJumpRequestKind::Pressed;

	if (CharacterOwner->IsLocallyControlled() && CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy)
	{
		FWuwaPendingNetworkMovementCommand Command;
		Command.Generation.Value = RequestGeneration;
		Command.Kind = EWuwaNetworkMovementCommandKind::Jump;
		Command.JumpType = EWuwaJumpRequestKind::Pressed;
		if (!QueueNetworkMovementCommand(Command))
		{
			UE_LOG(LogWuwa,
			       Warning,
			       TEXT("普通跳跃无法进入 Packed Move。Owner=%s, Generation=%d"),
			       *GetNameSafe(CharacterOwner),
			       RequestGeneration);
			return false;
		}
	}

	if (CanAttemptWuwaJump())
	{
		CharacterOwner->Jump();
		return true;
	}

	// 保存落地前输入的跳跃请求。
	return QueueBufferedJump(RequestGeneration, ConfiguredJumpBufferTime);
}

void UWuwaCharacterMovementComponent::ReleaseJump()
{
	if (HasGrappleRuntime() ||
	    (IsValid(StateTagComponent) && StateTagComponent->HasTag(WuwaGameplayTags::State_Traversal_Grappling, true)))
	{
		return;
	}

	if (IsValid(CharacterOwner))
	{
		const FWuwaNetworkActionGeneration Generation = AllocateNetworkActionGeneration();
		if (!Generation.IsValid())
		{
			CharacterOwner->StopJumping();
			return;
		}
		const int32 RequestGeneration = Generation.Value;
		AirActionState.JumpRequestGeneration = Generation.Value;
		AirActionState.JumpRequestKind = EWuwaJumpRequestKind::Released;

		if (CharacterOwner->IsLocallyControlled() && CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy)
		{
			FWuwaPendingNetworkMovementCommand Command;
			Command.Generation.Value = RequestGeneration;
			Command.Kind = EWuwaNetworkMovementCommandKind::Jump;
			Command.JumpType = EWuwaJumpRequestKind::Released;
			if (!QueueNetworkMovementCommand(Command))
			{
				UE_LOG(LogWuwa,
				       Warning,
				       TEXT("普通跳跃释放边沿无法进入 Packed Move。Owner=%s, Generation=%d"),
				       *GetNameSafe(CharacterOwner),
				       RequestGeneration);
			}
		}

		CharacterOwner->StopJumping();
	}
}

bool UWuwaCharacterMovementComponent::CanAttemptWuwaJump() const
{
	if (!IsValid(CharacterOwner) || !HasValidData() || MovementMode == MOVE_None || IsJumpBlockedByCombatState() ||
	    HasGrappleRuntime() ||
	    (IsValid(StateTagComponent) && StateTagComponent->HasTag(WuwaGameplayTags::State_Traversal_Grappling, true)))
	{
		return false;
	}

	const bool bGroundJump = IsMovingOnGround();
	const bool bCoyoteJump = IsFalling() && AirActionState.JumpCount == 0 && IsWithCoyoteTime(GetMovementTime());
	return AirActionState.JumpCount < ConfiguredMaxJumpCount && (bReplayingWuwaJump || bGroundJump || bCoyoteJump) &&
	       Super::CanAttemptJump();
}

// 由 Movement Component 设置跳跃速度、模式和跳跃计数
bool UWuwaCharacterMovementComponent::DoJump(const bool bReplayingMoves, const float DeltaTime)
{
	bReplayingWuwaJump = bReplayingMoves && AirActionState.JumpRequestGeneration > 0 &&
	                     AirActionState.JumpRequestKind == EWuwaJumpRequestKind::Pressed;
	if (!CanAttemptWuwaJump())
	{
		bReplayingWuwaJump = false;
		return false;
	}

	const bool bCoyoteJump = !IsMovingOnGround() && IsFalling();
	if (AirActionState.JumpRequestGeneration <= 0)
	{
		const FWuwaNetworkActionGeneration Generation = AllocateNetworkActionGeneration();
		if (!Generation.IsValid())
		{
			bReplayingWuwaJump = false;
			return false;
		}
		AirActionState.JumpRequestGeneration = Generation.Value;
		AirActionState.JumpRequestKind = EWuwaJumpRequestKind::Pressed;
	}
	const int32 RequestGeneration = AirActionState.JumpRequestGeneration;

	const bool bDidJump = Super::DoJump(bReplayingMoves, DeltaTime);
	bReplayingWuwaJump = false;
	if (!bDidJump)
	{
		return false;
	}

	if (!bReplayingMoves && AirActionState.AirCycleGeneration != RequestGeneration)
	{
		AirActionState.AirCycleGeneration = RequestGeneration;
		++AirActionState.JumpCount;
		LastJumpType = bCoyoteJump ? EWuwaJumpType::Coyote : EWuwaJumpType::Ground;
		++JumpSequence;
		RefreshAuthorityLocomotionPresentationState();

		if (AirActionState.BufferedJumpGeneration == RequestGeneration)
		{
			// 成功起跳后清楚缓存
			AirActionState.BufferedJumpRemainingTime = 0.f;
		}
	}

	return true;
}

bool UWuwaCharacterMovementComponent::QueueBufferedJump(const int32 RequestGeneration,
                                                        const float RequestedRemainingTime)
{
	if (!IsValid(CharacterOwner) || !IsFalling() || HasGrappleRuntime() || IsJumpBlockedByCombatState() ||
	    RequestGeneration <= 0 || RequestGeneration <= AirActionState.AirCycleGeneration ||
	    !FMath::IsFinite(RequestedRemainingTime))
	{
		return false;
	}

	const float ClampedRemainingTime = FMath::Clamp(RequestedRemainingTime, 0.f, ConfiguredJumpBufferTime);
	if (ClampedRemainingTime <= 0.f)
	{
		return false;
	}

	if (AirActionState.BufferedJumpGeneration == RequestGeneration)
	{
		return AirActionState.BufferedJumpRemainingTime > 0.f;
	}
	if (AirActionState.BufferedJumpGeneration > RequestGeneration)
	{
		return false;
	}

	AirActionState.BufferedJumpGeneration = RequestGeneration;
	AirActionState.BufferedJumpRemainingTime = ClampedRemainingTime;
	CharacterOwner->StopJumping();
	return true;
}

bool UWuwaCharacterMovementComponent::ConsumeBufferedJumpAtMovementBoundary()
{
	if (IsJumpBlockedByCombatState())
	{
		AirActionState.BufferedJumpGeneration = 0;
		AirActionState.BufferedJumpRemainingTime = 0.f;
		return false;
	}

	if (!IsValid(CharacterOwner) || !IsMovingOnGround() || AirActionState.BufferedJumpGeneration <= 0 ||
	    AirActionState.BufferedJumpRemainingTime <= 0.f ||
	    AirActionState.BufferedJumpGeneration <= AirActionState.AirCycleGeneration)
	{
		return false;
	}

	AirActionState.JumpRequestGeneration = AirActionState.BufferedJumpGeneration;
	AirActionState.JumpRequestKind = EWuwaJumpRequestKind::Pressed;
	AirActionState.BufferedJumpRemainingTime = 0.f;
	CharacterOwner->Jump();
	return true;
}

void UWuwaCharacterMovementComponent::GatherHandledInputTags(FGameplayTagContainer& OutInputTags) const
{
	OutInputTags.Reset();
	OutInputTags.AddTag(WuwaGameplayTags::Input_Jump);
}

FWuwaCommandDispatchResult UWuwaCharacterMovementComponent::HandleInputCommand(const FWuwaInputCommand& Command,
                                                                               const FWuwaInputFrame& InputFrame)
{
	(void)InputFrame;

	FWuwaCommandDispatchResult Result;
	Result.MessageTag = Command.InputTag;

	if (!Command.IsValid() || Command.InputTag != WuwaGameplayTags::Input_Jump)
	{
		Result.Status = EWuwaCommandDispatchStatus::Rejected;
		return Result;
	}

	Result.Status = EWuwaCommandDispatchStatus::Handled;

	if (Command.Trigger == EWuwaInputCommandTrigger::Pressed)
	{
		if (!TryInterruptAbilityForJump())
		{
			return Result;
		}
		RequestJump();
	}
	else
	{
		ReleaseJump();
	}

	// Result.Status = EWuwaCommandDispatchStatus::Handled;
	return Result;
}

bool UWuwaCharacterMovementComponent::CanPerformAirDoubleJump() const
{
	const bool bAirborneActionState = IsFalling() || HasGrappleRuntime();
	return IsValid(CharacterOwner) && HasValidData() && bAirborneActionState && !AirActionState.bAirSprintConsumed &&
	       AirActionState.JumpCount < ConfiguredMaxJumpCount;
}

bool UWuwaCharacterMovementComponent::CommitAirJumpVelocity(const FVector& WorldDirection,
                                                            const float HorizontalSpeed,
                                                            const float VerticalSpeed,
                                                            const EWuwaJumpType JumpType,
                                                            const bool bConsumeBudget)
{
	// 在修改任何运行态前完成全部准入校验，
	const bool bCanApplyVelocity =
	    bConsumeBudget ? CanPerformAirDoubleJump()
	                   : IsValid(CharacterOwner) && HasValidData() && (IsFalling() || HasGrappleRuntime());
	if (!bCanApplyVelocity || WorldDirection.ContainsNaN() || HorizontalSpeed < 0.f || VerticalSpeed <= 0.f ||
	    !FMath::IsFinite(HorizontalSpeed) || !FMath::IsFinite(VerticalSpeed))
	{
		return false;
	}

	// 限定空中二段跳类型
	if (JumpType != EWuwaJumpType::AirSprint && JumpType != EWuwaJumpType::AirBackflip)
	{
		return false;
	}

	const FVector HorizontalDirection = WorldDirection.GetSafeNormal2D();

	if (HorizontalDirection.IsNearlyZero())
	{
		return false;
	}

	// 使用局部副本计算完整结果
	FVector NewVelocity = Velocity;

	NewVelocity.X = HorizontalDirection.X * HorizontalSpeed;
	NewVelocity.Y = HorizontalDirection.Y * HorizontalSpeed;
	NewVelocity.Z = VerticalSpeed;

	// 执行原子提交，速度、预算和表现事件必须保持一致
	Velocity = NewVelocity;

	if (bConsumeBudget)
	{
		AirActionState.bAirSprintConsumed = true;
		// 防止落地缓存标记在空中二段跳后继续触发
		AirActionState.BufferedJumpRemainingTime = 0.f;
		++AirActionState.JumpCount;

		LastJumpType = JumpType;
		++JumpSequence;
		RefreshAuthorityLocomotionPresentationState();
	}

	return true;
}

bool UWuwaCharacterMovementComponent::RequestAirJump(const FVector& WorldDirection, const EWuwaJumpType JumpType)
{
	if (JumpType == EWuwaJumpType::AirSprint)
	{
		return CommitAirJumpVelocity(
		    WorldDirection, ConfiguredDoubleJumpForwardSpeed, ConfiguredDoubleJumpZVelocity, JumpType, true);
	}

	if (JumpType == EWuwaJumpType::AirBackflip)
	{
		return CommitAirJumpVelocity(
		    WorldDirection, ConfiguredBackflipBackwardSpeed, ConfiguredBackflipZVelocity, JumpType, true);
	}

	return false;
}

bool UWuwaCharacterMovementComponent::ReplayAirJump(const FVector& WorldDirection,
                                                    const EWuwaJumpType JumpType,
                                                    const int32 AirCycleGeneration)
{
	if (AirCycleGeneration <= 0 || AirCycleGeneration != AirActionState.AirCycleGeneration)
	{
		UE_LOG(LogWuwa,
		       Warning,
		       TEXT("AirJump 重演拒绝不匹配滞空 Generation。Owner=%s, Command=%d, Current=%d"),
		       *GetNameSafe(CharacterOwner),
		       AirCycleGeneration,
		       AirActionState.AirCycleGeneration);
		return false;
	}

	if (JumpType == EWuwaJumpType::AirSprint)
	{
		return CommitAirJumpVelocity(
		    WorldDirection, ConfiguredDoubleJumpForwardSpeed, ConfiguredDoubleJumpZVelocity, JumpType, false);
	}

	if (JumpType == EWuwaJumpType::AirBackflip)
	{
		return CommitAirJumpVelocity(
		    WorldDirection, ConfiguredBackflipBackwardSpeed, ConfiguredBackflipZVelocity, JumpType, false);
	}

	return false;
}

bool UWuwaCharacterMovementComponent::ApplyMovementProfile(const UWuwaMovementProfile* Profile)
{
	if (!Profile)
	{
		UE_LOG(LogWuwa, Error, TEXT("MovementProfile 为空。Owner=%s"), *GetNameSafe(CharacterOwner));
		return false;
	}

	const bool bHasValidRuntimeValues =
	    FMath::IsFinite(Profile->WalkSpeed) && Profile->WalkSpeed > 0.f && FMath::IsFinite(Profile->RunSpeed) &&
	    Profile->RunSpeed >= Profile->WalkSpeed && FMath::IsFinite(Profile->SprintSpeed) &&
	    Profile->SprintSpeed >= Profile->RunSpeed && FMath::IsFinite(Profile->SprintRunDeceleration) &&
	    Profile->SprintRunDeceleration > 0.f && FMath::IsFinite(Profile->AnalogRunThreshold) &&
	    Profile->AnalogRunThreshold >= 0.f && Profile->AnalogRunThreshold <= 1.f &&
	    FMath::IsFinite(Profile->MaxAcceleration) && Profile->MaxAcceleration > 0.f &&
	    FMath::IsFinite(Profile->BrakingDecelerationWalking) && Profile->BrakingDecelerationWalking > 0.f &&
	    FMath::IsFinite(Profile->BrakingDecelerationFalling) && Profile->BrakingDecelerationFalling > 0.f &&
	    FMath::IsFinite(Profile->GroundFriction) && Profile->GroundFriction >= 0.f &&
	    FMath::IsFinite(Profile->BrakingFrictionFactor) && Profile->BrakingFrictionFactor >= 0.f &&
	    !Profile->RotationRate.ContainsNaN() && FMath::IsFinite(Profile->AirControl) && Profile->AirControl >= 0.f &&
	    Profile->AirControl <= 1.f && FMath::IsFinite(Profile->JumpZVelocity) && Profile->JumpZVelocity > 0.f &&
	    FMath::IsFinite(Profile->DoubleJumpZVelocity) && Profile->DoubleJumpZVelocity > 0.f &&
	    FMath::IsFinite(Profile->DoubleJumpForwardSpeed) && Profile->DoubleJumpForwardSpeed >= 0.f &&
	    FMath::IsFinite(Profile->BackflipZVelocity) && Profile->BackflipZVelocity > 0.f &&
	    FMath::IsFinite(Profile->BackflipBackwardSpeed) && Profile->BackflipBackwardSpeed > 0.f &&
	    Profile->MaxJumpCount >= 1 && FMath::IsFinite(Profile->CoyoteTime) && Profile->CoyoteTime >= 0.f &&
	    FMath::IsFinite(Profile->JumpBufferTime) && Profile->JumpBufferTime >= 0.f &&
	    FMath::IsFinite(Profile->GravityScale) && Profile->GravityScale > 0.f &&
	    FMath::IsFinite(Profile->HeavyLandingVelocityThreshold) && Profile->HeavyLandingVelocityThreshold > 0.f;
	if (!bHasValidRuntimeValues)
	{
		UE_LOG(LogWuwa,
		       Warning,
		       TEXT("MovementProfile 包含无效运行参数，拒绝部分应用。Profile=%s"),
		       *Profile->GetPathName());
		return false;
	}

	// 保存运行时副本，避免每帧访问 Data Asset。
	ConfiguredWalkSpeed = Profile->WalkSpeed;
	ConfiguredRunSpeed = Profile->RunSpeed;
	ConfiguredSprintSpeed = Profile->SprintSpeed;
	ConfiguredSprintRunDeceleration = Profile->SprintRunDeceleration;
	ConfiguredAnalogRunThreshold = Profile->AnalogRunThreshold;
	RuntimeSprintBlockedTags = Profile->SprintBlockedTags;
	MaxWalkSpeed = ConfiguredRunSpeed;

	MaxAcceleration = Profile->MaxAcceleration;
	BrakingDecelerationWalking = Profile->BrakingDecelerationWalking;
	BrakingDecelerationFalling = Profile->BrakingDecelerationFalling;
	GroundFriction = Profile->GroundFriction;
	BrakingFrictionFactor = Profile->BrakingFrictionFactor;
	RotationRate = Profile->RotationRate;
	AirControl = Profile->AirControl;

	// 角色初始化时应用Movement Profile中的跳跃运行时配置。
	JumpZVelocity = Profile->JumpZVelocity;
	ConfiguredDoubleJumpZVelocity = Profile->DoubleJumpZVelocity;
	ConfiguredDoubleJumpForwardSpeed = Profile->DoubleJumpForwardSpeed;
	ConfiguredBackflipZVelocity = Profile->BackflipZVelocity;
	ConfiguredBackflipBackwardSpeed = Profile->BackflipBackwardSpeed;
	ConfiguredMaxJumpCount = Profile->MaxJumpCount;
	ConfiguredCoyoteTime = Profile->CoyoteTime;
	ConfiguredJumpBufferTime = Profile->JumpBufferTime;
	ConfiguredHeavyLandingVelocityThreshold = Profile->HeavyLandingVelocityThreshold;
	GravityScale = Profile->GravityScale;

	RefreshLocomotionState();

	return true;
}

void UWuwaCharacterMovementComponent::TickComponent(float DeltaTime,
                                                    ELevelTick TickType,
                                                    FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!HasValidData())
	{
		return;
	}

	const double CurrentTime = GetMovementTime();

	// 实现缓存跳跃的过期逻辑，避免落地前的跳跃请求无限期保留。
	if (AirActionState.BufferedJumpRemainingTime > 0.f)
	{
		if (!FMath::IsFinite(DeltaTime) || DeltaTime < 0.f)
		{
			UE_LOG(LogWuwa, Warning, TEXT("跳跃缓存收到无效时间片。Owner=%s"), *GetNameSafe(CharacterOwner));
			// 清除已经失效的跳跃请求。
			AirActionState.BufferedJumpRemainingTime = 0.f;
		}
		else
		{
			AirActionState.BufferedJumpRemainingTime =
			    FMath::Max(0.f, AirActionState.BufferedJumpRemainingTime - DeltaTime);
		}
	}

	if (IsFalling() && CharacterOwner)
	{
		// 持续记录本次滞空的最高位置
		AirActionState.FallStartHeight =
		    FMath::Max(AirActionState.FallStartHeight, CharacterOwner->GetActorLocation().Z);
	}

	if (IsMovingOnGround())
	{
		AirActionState.LastGroundedTime = CurrentTime;
		// 落地后立即消费跳跃缓存
		ConsumeBufferedJumpAtMovementBoundary();
	}
}

void UWuwaCharacterMovementComponent::CalcVelocity(const float DeltaTime,
                                                   const float Friction,
                                                   const bool bFluid,
                                                   const float BrakingDeceleration)
{
	const float PreviousHorizontalSpeed = Velocity.Size2D();
	if (bIsSprinting && !CanMaintainSprintRun())
	{
		SetSprinting(false);
	}

	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);

	if (!bIsSprinting)
	{
		RefreshAuthorityLocomotionPresentationState();
		return;
	}

	const float CurrentHorizontalSpeed = Velocity.Size2D();
	if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0.f || !FMath::IsFinite(PreviousHorizontalSpeed) ||
	    !FMath::IsFinite(CurrentHorizontalSpeed))
	{
		UE_LOG(LogWuwa, Warning, TEXT("SprintRun 速度积分收到无效数值。Owner=%s"), *GetNameSafe(CharacterOwner));
		SetSprinting(false);
		RefreshAuthorityLocomotionPresentationState();
		return;
	}

	const float DeceleratedSpeed =
	    FMath::Max(ConfiguredRunSpeed, PreviousHorizontalSpeed - ConfiguredSprintRunDeceleration * DeltaTime);
	float TargetHorizontalSpeed = FMath::Min(CurrentHorizontalSpeed, DeceleratedSpeed);
	const bool bReachedRunSpeed = TargetHorizontalSpeed <= ConfiguredRunSpeed + SprintRunExitSpeedTolerance;
	if (bReachedRunSpeed && CurrentHorizontalSpeed >= ConfiguredRunSpeed)
	{
		TargetHorizontalSpeed = ConfiguredRunSpeed;
	}

	if (CurrentHorizontalSpeed > UE_KINDA_SMALL_NUMBER)
	{
		const FVector NewHorizontalVelocity = Velocity.GetSafeNormal2D() * TargetHorizontalSpeed;
		Velocity.X = NewHorizontalVelocity.X;
		Velocity.Y = NewHorizontalVelocity.Y;
	}

	if (bReachedRunSpeed)
	{
		SetSprinting(false);
	}

	RefreshAuthorityLocomotionPresentationState();
}

// 生成普通轻落地事件、重置所有空中次数，并准备消费跳跃缓存。
void UWuwaCharacterMovementComponent::ProcessLanded(const FHitResult& Hit,
                                                    const float RemainingTime,
                                                    const int32 Iterations)
{
	if (HasGrappleRuntime())
	{
		FinishGrappleMovement(EWuwaGrappleMovementEndReason::Landed);
	}

	const double CurrentTime = GetMovementTime();
	const FVector ImpactVelocity = Velocity;

	const float LandingHeight = CharacterOwner ? CharacterOwner->GetActorLocation().Z : AirActionState.FallStartHeight;

	const float FallDistance = FMath::Max(0.f, AirActionState.FallStartHeight - LandingHeight);

	const int32 NextSequence = LastLandingEvent.Sequence + 1;

	LastLandingEvent = FWuwaLandingEvent();
	LastLandingEvent.ImpactVelocity = ImpactVelocity;
	LastLandingEvent.ImpactSpeed = FMath::Max(0.f, -ImpactVelocity.Z);
	LastLandingEvent.FallDistance = FallDistance;

	// 真实落地来源均为 Normal；Light/Heavy 只由接触前的向下速度分类
	LastLandingEvent.LandingSource = EWuwaLandingSource::Normal;
	LastLandingEvent.LandingType =
	    WuwaMovementRules::ClassifyLanding(LastLandingEvent.ImpactSpeed, ConfiguredHeavyLandingVelocityThreshold);
	LastLandingEvent.Sequence = NextSequence;

	// 落地时重置所有空中次数，避免滞空后继续使用二段跳、下落攻击等。
	AirActionState.ResetBudgetsOnLanding();
	AirActionState.LastGroundedTime = CurrentTime;
	AirActionState.FallStartHeight = LandingHeight;
	RefreshAuthorityLocomotionPresentationState();

	Super::ProcessLanded(Hit, RemainingTime, Iterations);

	ConsumeBufferedJumpAtMovementBoundary();

	// 父类完成落地物理后再广播事实
	OnLandedEvent.Broadcast(LastLandingEvent);
}

void UWuwaCharacterMovementComponent::OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData,
                                                                 const float TimeStamp,
                                                                 const FVector NewLocation,
                                                                 const FVector NewVelocity,
                                                                 UPrimitiveComponent* NewBase,
                                                                 const FName NewBaseBoneName,
                                                                 const bool bHasBase,
                                                                 const bool bBaseRelativePosition,
                                                                 const uint8 ServerMovementMode,
                                                                 const FVector ServerGravityDirection)
{
	const FVector CurrentLocation = IsValid(UpdatedComponent) ? UpdatedComponent->GetComponentLocation() : NewLocation;
	const float CorrectionDistance = FVector::Distance(CurrentLocation, NewLocation);

	Super::OnClientCorrectionReceived(ClientData,
	                                  TimeStamp,
	                                  NewLocation,
	                                  NewVelocity,
	                                  NewBase,
	                                  NewBaseBoneName,
	                                  bHasBase,
	                                  bBaseRelativePosition,
	                                  ServerMovementMode,
	                                  ServerGravityDirection);

	if (!FMath::IsFinite(CorrectionDistance))
	{
		UE_LOG(LogWuwa, Warning, TEXT("网络移动校正距离无效。Owner=%s"), *GetNameSafe(CharacterOwner));
		return;
	}

	const double CurrentRealTime = GetWorld() != nullptr ? GetWorld()->GetRealTimeSeconds() : 0.0;
	if (NetworkCorrectionCount == 0)
	{
		FirstNetworkCorrectionRealTime = CurrentRealTime;
	}

	++NetworkCorrectionCount;
	LastNetworkCorrectionDistance = CorrectionDistance;
	MaxNetworkCorrectionDistance = FMath::Max(MaxNetworkCorrectionDistance, CorrectionDistance);
	TotalNetworkCorrectionDistance += CorrectionDistance;
	LastNetworkCorrectionRealTime = CurrentRealTime;
}

float UWuwaCharacterMovementComponent::GetMaxSpeed() const
{
	if (MovementMode != MOVE_Walking && MovementMode != MOVE_NavWalking)
	{
		return Super::GetMaxSpeed();
	}

	switch (ResolveLocomotionSpeedMode())
	{
		case EWuwaLocomotionSpeedMode::Walk:
			return ConfiguredWalkSpeed;

		case EWuwaLocomotionSpeedMode::Run:
			return ConfiguredRunSpeed;

		case EWuwaLocomotionSpeedMode::SprintRun:
			return FMath::Clamp(Velocity.Size2D(), ConfiguredRunSpeed, ConfiguredSprintSpeed);

		default:
			UE_LOG(LogWuwa,
			       Warning,
			       TEXT("无法解析移动速度档位，回退到 RunSpeed。Owner=%s"),
			       *GetNameSafe(CharacterOwner));
			return ConfiguredRunSpeed;
	}
}

EWuwaLocomotionSpeedMode UWuwaCharacterMovementComponent::ResolveLocomotionSpeedMode() const
{
	if (bIsSprinting)
	{
		return EWuwaLocomotionSpeedMode::SprintRun;
	}

	const float ReplayInputMagnitude = FMath::IsFinite(MaxAcceleration) && MaxAcceleration > UE_KINDA_SMALL_NUMBER
	                                       ? FMath::Clamp(Acceleration.Size2D() / MaxAcceleration, 0.f, 1.f)
	                                       : 0.f;
	return ReplayInputMagnitude >= ConfiguredAnalogRunThreshold ? EWuwaLocomotionSpeedMode::Run
	                                                            : EWuwaLocomotionSpeedMode::Walk;
}

void UWuwaCharacterMovementComponent::SetLocomotionIntent(const FVector2D& MoveIntent)
{
	CurrentLocomotionIntent = MoveIntent.ContainsNaN() ? FVector2D::ZeroVector : MoveIntent.GetClampedToMaxSize(1.f);

	// 对角输入最大按 1 处理。
	MoveInputMagnitude = CurrentLocomotionIntent.Size();
}

bool UWuwaCharacterMovementComponent::EnterSprintRun(const FVector2D& MoveIntent)
{
	if (MoveIntent.ContainsNaN())
	{
		return false;
	}

	// Block.Input.Move 刚由 Router 释放，本次调用先同步真实 WASD，避免等待下一帧输入门面。
	MoveInputMagnitude = FMath::Clamp(MoveIntent.Size(), 0.f, 1.f);

	const bool bBlocked = StateTagComponent && StateTagComponent->HasAny(RuntimeSprintBlockedTags);
	const float HorizontalSpeed = Velocity.Size2D();
	if (bBlocked || MoveInputMagnitude <= 0.1f || !IsMovingOnGround() || !FMath::IsFinite(HorizontalSpeed) ||
	    HorizontalSpeed <= ConfiguredRunSpeed + SprintRunExitSpeedTolerance)
	{
		return false;
	}

	// Sprinting 标签只表示 Dash 出口减速阶段，不代表持续按键可永久保持高速。
	SetSprinting(true);

	return bIsSprinting;
}

void UWuwaCharacterMovementComponent::ExitSprintRun()
{
	SetSprinting(false);
}

bool UWuwaCharacterMovementComponent::CanMaintainSprintRun() const
{
	const bool bBlocked = StateTagComponent && StateTagComponent->HasAny(RuntimeSprintBlockedTags);
	const float HorizontalSpeed = Velocity.Size2D();
	const float ReplayInputMagnitude = FMath::IsFinite(MaxAcceleration) && MaxAcceleration > UE_KINDA_SMALL_NUMBER
	                                       ? FMath::Clamp(Acceleration.Size2D() / MaxAcceleration, 0.f, 1.f)
	                                       : 0.f;

	return !bBlocked && ReplayInputMagnitude > 0.1f && IsMovingOnGround() && FMath::IsFinite(HorizontalSpeed) &&
	       HorizontalSpeed > ConfiguredRunSpeed + SprintRunExitSpeedTolerance;
}

void UWuwaCharacterMovementComponent::RefreshLocomotionState()
{
	const bool bBlocked = StateTagComponent && StateTagComponent->HasAny(RuntimeSprintBlockedTags);
	if (bIsSprinting && (!IsMovingOnGround() || bBlocked))
	{
		SetSprinting(false);
	}
}

void UWuwaCharacterMovementComponent::SetSprinting(const bool bNewSprinting)
{
	// 状态未变化时不重复增删标签。
	if (bIsSprinting == bNewSprinting)
	{
		return;
	}

	// 只在冲刺状态变化时增删标签
	bIsSprinting = bNewSprinting;

	if (!StateTagComponent)
	{
		return;
	}

	if (bIsSprinting)
	{
		SprintingTagHandle = StateTagComponent->AcquireTag(WuwaGameplayTags::State_Locomotion_Sprinting);
	}
	else if (SprintingTagHandle.IsValid())
	{
		StateTagComponent->ReleaseTag(SprintingTagHandle);
	}
}

void UWuwaCharacterMovementComponent::OnMovementModeChanged(const EMovementMode PreviousMovementMode,
                                                            const uint8 PreviousCustomMode)
{
	// 在 Super 前保存旧模式事实，供土狼时间与事件消费者使用
	const bool bWasGrounded = PreviousMovementMode == MOVE_Walking || PreviousMovementMode == MOVE_NavWalking;

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	const EMovementMode NewMovementMode = MovementMode;
	const uint8 NewCustomMode = CustomMovementMode;

	if (PreviousMovementMode == MOVE_Custom &&
	    PreviousCustomMode == static_cast<uint8>(EWuwaCustomMovementMode::Grapple) && HasGrappleRuntime() &&
	    (NewMovementMode != MOVE_Custom || NewCustomMode != static_cast<uint8>(EWuwaCustomMovementMode::Grapple)))
	{
		FinishGrappleMovement(NewMovementMode == MOVE_Walking || NewMovementMode == MOVE_NavWalking
		                          ? EWuwaGrappleMovementEndReason::Landed
		                          : EWuwaGrappleMovementEndReason::Stopped);
	}

	if (bWasGrounded && NewMovementMode == MOVE_Falling)
	{
		AirActionState.LastGroundedTime = GetMovementTime();

		if (CharacterOwner)
		{
			// 记录本次滞空的起始高度
			AirActionState.FallStartHeight = CharacterOwner->GetActorLocation().Z;
		}
	}

	// 先同步 Movement Component 自己拥有的移动状态，再通知外部消费者
	RefreshLocomotionState();

	OnWuwaMovementModeChanged.Broadcast(PreviousMovementMode, PreviousCustomMode, NewMovementMode, NewCustomMode);
}

void UWuwaCharacterMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GrappleRuntime.Handle.IsValid())
	{
		StopGrappleMovement(GrappleRuntime.Handle, EWuwaGrappleMovementEndReason::EndPlay);
	}

	TargetingComponent.Reset();

	NetworkCommandProcessor.Unbind();
	NetworkCommandProcessorOwner.Reset();
	NetworkMoveResponseConsumer.Unbind();
	NetworkMoveResponseConsumerOwner.Reset();
	MovementActionNetworkProvider.Reset();
	PendingNetworkMovementCommand.Reset();
	ReplayNetworkMovementCommand.Reset();
	LocalCapturedNetworkMovementCommand.Reset();
	GrappleReplayRuntimeCache = FWuwaGrappleMovementRuntime();
	PendingNetworkActionResponse = FWuwaNetworkActionResponse();
	bHasActiveNetworkFacing = false;
	bAllowActiveNetworkFacingSnap = false;

	// 销毁前释放持有的冲刺标签
	if (SprintingTagHandle.IsValid() && StateTagComponent)
	{
		StateTagComponent->ReleaseTag(SprintingTagHandle);
	}
	bIsSprinting = false;

	Super::EndPlay(EndPlayReason);
}

// 让动画和 Debug 读取 Movement Component 的结果，而不是维护自己的计数。
FWuwaLocomotionSnapshot UWuwaCharacterMovementComponent::GetLocomotionSnapshot() const
{
	FWuwaLocomotionSnapshot Snapshot;

	Snapshot.Velocity = Velocity;
	Snapshot.Acceleration = GetCurrentAcceleration();
	Snapshot.HorizontalSpeed = Velocity.Size2D();
	Snapshot.InputMagnitude = MoveInputMagnitude;
	Snapshot.SpeedMode = ResolveLocomotionSpeedMode();
	Snapshot.ReplayInputMagnitude = FMath::IsFinite(MaxAcceleration) && MaxAcceleration > UE_KINDA_SMALL_NUMBER
	                                    ? FMath::Clamp(Acceleration.Size2D() / MaxAcceleration, 0.f, 1.f)
	                                    : 0.f;
	Snapshot.bHasLocalInput =
	    IsValid(CharacterOwner) && CharacterOwner->IsLocallyControlled() && MoveInputMagnitude > UE_KINDA_SMALL_NUMBER;
	Snapshot.MovementMode = MovementMode;
	Snapshot.CustomMovementMode = CustomMovementMode;
	Snapshot.bIsMovingOnGround = IsMovingOnGround();
	Snapshot.bIsFalling = IsFalling();
	Snapshot.bIsAirborne = IsAirborneState();
	Snapshot.bIsSprinting = bIsSprinting;

	Snapshot.VerticalVelocity = Velocity.Z;
	Snapshot.JumpCount = AirActionState.JumpCount;
	Snapshot.LastJumpType = LastJumpType;
	Snapshot.JumpSequence = JumpSequence;

	Snapshot.LastLandingType = LastLandingEvent.LandingType;
	Snapshot.LastLandingVelocity = LastLandingEvent.ImpactSpeed;
	Snapshot.LastFallDistance = LastLandingEvent.FallDistance;
	Snapshot.LandingSequence = LastLandingEvent.Sequence;

	if (CharacterOwner && !Velocity.IsNearlyZero())
	{
		// 转为角色局部速度，供动画计算方向
		const FVector LocalVelocity = CharacterOwner->GetActorTransform().InverseTransformVectorNoScale(Velocity);

		Snapshot.Direction = FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X));
	}

	ApplyReplicatedLocomotionPresentationState(Snapshot);

	return Snapshot;
}

void UWuwaCharacterMovementComponent::RefreshAuthorityLocomotionPresentationState()
{
	if (!IsValid(CharacterOwner) || !CharacterOwner->HasAuthority())
	{
		return;
	}

	FWuwaReplicatedLocomotionPresentationState NewState;
	NewState.SpeedMode = ResolveLocomotionSpeedMode();
	NewState.JumpCount = AirActionState.JumpCount;
	NewState.LastJumpType = LastJumpType;
	NewState.JumpSequence = JumpSequence;
	NewState.LastLandingType = LastLandingEvent.LandingType;
	NewState.LastLandingVelocity = LastLandingEvent.ImpactSpeed;
	NewState.LastFallDistance = LastLandingEvent.FallDistance;
	NewState.LandingSequence = LastLandingEvent.Sequence;

	const bool bChanged = NewState.SpeedMode != ReplicatedLocomotionPresentationState.SpeedMode ||
	                      NewState.JumpCount != ReplicatedLocomotionPresentationState.JumpCount ||
	                      NewState.LastJumpType != ReplicatedLocomotionPresentationState.LastJumpType ||
	                      NewState.JumpSequence != ReplicatedLocomotionPresentationState.JumpSequence ||
	                      NewState.LastLandingType != ReplicatedLocomotionPresentationState.LastLandingType ||
	                      NewState.LastLandingVelocity != ReplicatedLocomotionPresentationState.LastLandingVelocity ||
	                      NewState.LastFallDistance != ReplicatedLocomotionPresentationState.LastFallDistance ||
	                      NewState.LandingSequence != ReplicatedLocomotionPresentationState.LandingSequence;
	if (!bChanged)
	{
		return;
	}

	const bool bOneShotPresentationChanged =
	    NewState.JumpSequence != ReplicatedLocomotionPresentationState.JumpSequence ||
	    NewState.LandingSequence != ReplicatedLocomotionPresentationState.LandingSequence;
	ReplicatedLocomotionPresentationState = NewState;

	if (bOneShotPresentationChanged)
	{
		CharacterOwner->ForceNetUpdate();
	}
}

bool UWuwaCharacterMovementComponent::ShouldUseReplicatedLocomotionPresentationState() const
{
	return IsValid(CharacterOwner) && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy &&
	       bReplicatedLocomotionPresentationStateValid;
}

void UWuwaCharacterMovementComponent::ApplyReplicatedLocomotionPresentationState(
    FWuwaLocomotionSnapshot& InOutSnapshot) const
{
	if (!ShouldUseReplicatedLocomotionPresentationState())
	{
		return;
	}

	InOutSnapshot.SpeedMode = ReplicatedLocomotionPresentationState.SpeedMode;
	InOutSnapshot.bIsSprinting = ReplicatedLocomotionPresentationState.SpeedMode == EWuwaLocomotionSpeedMode::SprintRun;
	InOutSnapshot.JumpCount = ReplicatedLocomotionPresentationState.JumpCount;
	InOutSnapshot.LastJumpType = ReplicatedLocomotionPresentationState.LastJumpType;
	InOutSnapshot.JumpSequence = ReplicatedLocomotionPresentationState.JumpSequence;
	InOutSnapshot.LastLandingType = ReplicatedLocomotionPresentationState.LastLandingType;
	InOutSnapshot.LastLandingVelocity = ReplicatedLocomotionPresentationState.LastLandingVelocity;
	InOutSnapshot.LastFallDistance = ReplicatedLocomotionPresentationState.LastFallDistance;
	InOutSnapshot.LandingSequence = ReplicatedLocomotionPresentationState.LandingSequence;
}

void UWuwaCharacterMovementComponent::OnRep_LocomotionPresentationState(
    const FWuwaReplicatedLocomotionPresentationState& PreviousState)
{
	if (!ensureMsgf(IsValid(CharacterOwner), TEXT("移动表现状态复制时缺少 CharacterOwner")))
	{
		bReplicatedLocomotionPresentationStateValid = false;
		return;
	}

	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		UE_LOG(LogWuwa,
		       Warning,
		       TEXT("移动表现状态到达了非 Simulated Proxy。Owner=%s, LocalRole=%d"),
		       *GetNameSafe(CharacterOwner),
		       static_cast<int32>(CharacterOwner->GetLocalRole()));
	}

	const UEnum* SpeedModeEnum = StaticEnum<EWuwaLocomotionSpeedMode>();
	const UEnum* JumpTypeEnum = StaticEnum<EWuwaJumpType>();
	const UEnum* LandingTypeEnum = StaticEnum<EWuwaLandingType>();
	const bool bValidEnums =
	    IsValid(SpeedModeEnum) &&
	    SpeedModeEnum->IsValidEnumValue(static_cast<int64>(ReplicatedLocomotionPresentationState.SpeedMode)) &&
	    IsValid(JumpTypeEnum) &&
	    JumpTypeEnum->IsValidEnumValue(static_cast<int64>(ReplicatedLocomotionPresentationState.LastJumpType)) &&
	    IsValid(LandingTypeEnum) &&
	    LandingTypeEnum->IsValidEnumValue(static_cast<int64>(ReplicatedLocomotionPresentationState.LastLandingType));
	const bool bValidNumbers = ReplicatedLocomotionPresentationState.JumpCount >= 0 &&
	                           ReplicatedLocomotionPresentationState.JumpSequence >= 0 &&
	                           ReplicatedLocomotionPresentationState.LandingSequence >= 0 &&
	                           FMath::IsFinite(ReplicatedLocomotionPresentationState.LastLandingVelocity) &&
	                           ReplicatedLocomotionPresentationState.LastLandingVelocity >= 0.f &&
	                           FMath::IsFinite(ReplicatedLocomotionPresentationState.LastFallDistance) &&
	                           ReplicatedLocomotionPresentationState.LastFallDistance >= 0.f;
	if (!bValidEnums || !bValidNumbers)
	{
		bReplicatedLocomotionPresentationStateValid = false;
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("拒绝非法移动表现状态。Owner=%s, SpeedMode=%d, JumpType=%d, JumpCount=%d, JumpSequence=%d, "
		            "LandingType=%d, LandingSequence=%d"),
		       *GetNameSafe(CharacterOwner),
		       static_cast<int32>(ReplicatedLocomotionPresentationState.SpeedMode),
		       static_cast<int32>(ReplicatedLocomotionPresentationState.LastJumpType),
		       ReplicatedLocomotionPresentationState.JumpCount,
		       ReplicatedLocomotionPresentationState.JumpSequence,
		       static_cast<int32>(ReplicatedLocomotionPresentationState.LastLandingType),
		       ReplicatedLocomotionPresentationState.LandingSequence);
		return;
	}

	if (ReplicatedLocomotionPresentationState.JumpSequence < PreviousState.JumpSequence ||
	    ReplicatedLocomotionPresentationState.LandingSequence < PreviousState.LandingSequence)
	{
		UE_LOG(LogWuwa,
		       Warning,
		       TEXT("移动表现序号发生倒退。Owner=%s, Jump=%d->%d, Landing=%d->%d"),
		       *GetNameSafe(CharacterOwner),
		       PreviousState.JumpSequence,
		       ReplicatedLocomotionPresentationState.JumpSequence,
		       PreviousState.LandingSequence,
		       ReplicatedLocomotionPresentationState.LandingSequence);
	}

	bReplicatedLocomotionPresentationStateValid = true;
}

FWuwaNetworkMovementBaselineSnapshot UWuwaCharacterMovementComponent::GetNetworkBaselineSnapshot() const
{
	FWuwaNetworkMovementBaselineSnapshot Snapshot;
	Snapshot.MovementMode = MovementMode;
	Snapshot.CustomMovementMode = CustomMovementMode;
	Snapshot.Velocity = Velocity;
	Snapshot.Acceleration = Acceleration;
	Snapshot.MaxWalkSpeed = MaxWalkSpeed;
	Snapshot.ResolvedMaxSpeed = GetMaxSpeed();
	Snapshot.LocalInputMagnitude = MoveInputMagnitude;
	Snapshot.CorrectionCount = NetworkCorrectionCount;
	Snapshot.LastCorrectionDistance = LastNetworkCorrectionDistance;
	Snapshot.MaxCorrectionDistance = MaxNetworkCorrectionDistance;
	Snapshot.AverageCorrectionDistance =
	    NetworkCorrectionCount > 0 ? static_cast<float>(TotalNetworkCorrectionDistance / NetworkCorrectionCount) : 0.f;

	if (FirstNetworkCorrectionRealTime >= 0.0 && LastNetworkCorrectionRealTime > FirstNetworkCorrectionRealTime)
	{
		Snapshot.CorrectionsPerSecond = static_cast<float>(
		    NetworkCorrectionCount / (LastNetworkCorrectionRealTime - FirstNetworkCorrectionRealTime));
	}

	if (IsValid(CharacterOwner))
	{
		Snapshot.LocalRole = CharacterOwner->GetLocalRole();
		Snapshot.RemoteRole = CharacterOwner->GetRemoteRole();
		Snapshot.NetMode = static_cast<uint8>(CharacterOwner->GetNetMode());
		Snapshot.bLocallyControlled = CharacterOwner->IsLocallyControlled();
	}

	if (ClientPredictionData != nullptr)
	{
		Snapshot.SavedMoveCount = ClientPredictionData->SavedMoves.Num();
		Snapshot.LastAckedMoveTimestamp =
		    ClientPredictionData->LastAckedMove.IsValid() ? ClientPredictionData->LastAckedMove->TimeStamp : 0.f;
	}

	return Snapshot;
}

bool UWuwaCharacterMovementComponent::IsAirborneState() const
{
	return IsFalling() || IsInGrappleMovementMode();
}

bool UWuwaCharacterMovementComponent::TryInterruptAbilityForJump()
{
	AWuwaCharacter* WuwaCharacter = Cast<AWuwaCharacter>(CharacterOwner);

	if (!IsValid(WuwaCharacter))
	{
		// 没有 WuwaCharacter，自然没有我们自己的 Ability Blocking。
		// 让原有 Jump 逻辑继续处理。
		return true;
	}

	UWuwaActionAbilityInteropComponent* Interop = WuwaCharacter->GetActionAbilityInteropComponent();

	if (!IsValid(Interop))
	{
		return true;
	}

	// 当前没有任何 Ability 阻止 Jump：
	// 普通 Jump 直接继续。

	if (!Interop->HasBlockingActiveAbility(EWuwaAbilityInterruptSource::Move))
	{
		return true;
	}

	// 存在 Blocking Ability。
	return Interop->TryInterruptActiveAbility(EWuwaAbilityInterruptSource::Move);
}
