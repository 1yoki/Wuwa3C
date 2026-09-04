#include "Targeting/WuwaTargetingComponent.h"

#include "Core/WuwaStateTagComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Math/RotationMatrix.h"
#include "Targeting/WuwaTargetableInterface.h"
#include "Engine/OverlapResult.h"
#include "Core/WuwaGameplayTags.h"

#include "GameFramework/Actor.h"
#include "Targeting/WuwaTargetingProfile.h"
#include "Net/UnrealNetwork.h"
#include "Wuwa.h"

UWuwaTargetingComponent::UWuwaTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

bool UWuwaTargetingComponent::Initialize(AActor* InRequester,
                                         UWuwaStateTagComponent* InStateTagComponent,
                                         const UWuwaTargetingProfile* InProfile)
{
	ResetRuntimeState();

	if (!IsValid(InRequester) || InRequester != GetOwner())
	{
		LastFailureReason = EWuwaTargetingFailureReason::InvalidRequester;
		return false;
	}

	if (!IsValid(InStateTagComponent) || InStateTagComponent->GetOwner() != InRequester)
	{
		// Targeting 不能向其他 Actor 的标签组件写入 HardLock 事实。
		LastFailureReason = EWuwaTargetingFailureReason::InvalidRequester;
		return false;
	}

	if (!IsValid(InProfile) || !InProfile->IsRuntimeValid())
	{
		LastFailureReason = EWuwaTargetingFailureReason::InvalidProfile;
		return false;
	}

	Requester = InRequester;
	StateTagComponent = InStateTagComponent;

	RuntimeConfig.CandidateObjectTypes = InProfile->CandidateObjectTypes;
	RuntimeConfig.SearchRadius = InProfile->SearchRadius;
	RuntimeConfig.MaxAcquireAngleDegrees = InProfile->MaxAcquireAngleDegrees;
	RuntimeConfig.HardLockReleaseDistance = InProfile->HardLockReleaseDistance;

	RuntimeConfig.VisibilityTraceChannel = InProfile->VisibilityTraceChannel;
	RuntimeConfig.OcclusionGraceTime = InProfile->OcclusionGraceTime;
	RuntimeConfig.CandidateRefreshInterval = InProfile->CandidateRefreshInterval;

	RuntimeConfig.DistanceWeight = InProfile->DistanceWeight;
	RuntimeConfig.ViewAlignmentWeight = InProfile->ViewAlignmentWeight;
	RuntimeConfig.CurrentSoftTargetBonus = InProfile->CurrentSoftTargetBonus;

	RuntimeConfig.SwitchMinimumHorizontalDelta = InProfile->SwitchMinimumHorizontalDelta;
	RuntimeConfig.SwitchVerticalPenaltyWeight = InProfile->SwitchVerticalPenaltyWeight;
	RuntimeConfig.SwitchDistancePenaltyWeight = InProfile->SwitchDistancePenaltyWeight;

	bInitialized = true;
	LastFailureReason = EWuwaTargetingFailureReason::None;

	if (GetOwner()->GetLocalRole() != ROLE_SimulatedProxy)
	{
		RefreshSoftTarget();
		StartCandidateRefreshTimer();
	}

	return true;
}

bool UWuwaTargetingComponent::IsInitialized() const
{
	return bInitialized && Requester.IsValid() && StateTagComponent.IsValid();
}

bool UWuwaTargetingComponent::SubmitTargetingCommand(const EWuwaTargetingNetworkCommandKind Kind,
                                                     AActor* RequestedTarget)
{
	AActor* OwnerActor = GetOwner();
	if (!IsInitialized() || !IsValid(OwnerActor) || OwnerActor->GetLocalRole() == ROLE_SimulatedProxy)
	{
		return false;
	}

	if (OwnerActor->HasAuthority())
	{
		return CommitReplicatedAuthorityState(
		    LastProcessedAuthorityTargetingCommandSequence, true, EWuwaTargetingFailureReason::None, true);
	}

	if (OwnerActor->GetLocalRole() != ROLE_AutonomousProxy || NextLocalTargetingCommandSequence <= 0 ||
	    NextLocalTargetingCommandSequence == MAX_int32)
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Targeting 网络命令序号不可用。Owner=%s, Next=%d"),
		       *GetNameSafe(OwnerActor),
		       NextLocalTargetingCommandSequence);
		return false;
	}

	FWuwaTargetingNetworkCommand Command;
	Command.Kind = Kind;
	Command.CommandSequence = NextLocalTargetingCommandSequence++;
	Command.RequestedTarget = RequestedTarget;
	if (!Command.IsValid())
	{
		UE_LOG(LogWuwa,
		       Warning,
		       TEXT("Targeting 拒绝无效本地网络命令。Owner=%s, Kind=%d, Sequence=%d, Target=%s"),
		       *GetNameSafe(OwnerActor),
		       static_cast<int32>(Kind),
		       Command.CommandSequence,
		       *GetNameSafe(RequestedTarget));
		return false;
	}

	ServerSubmitTargetingCommand(Command);
	return true;
}

void UWuwaTargetingComponent::ServerSubmitTargetingCommand_Implementation(const FWuwaTargetingNetworkCommand& Command)
{
	ProcessAuthorityTargetingCommand(Command);
}

void UWuwaTargetingComponent::ProcessAuthorityTargetingCommand(const FWuwaTargetingNetworkCommand& Command)
{
	AActor* OwnerActor = GetOwner();
	if (!IsInitialized() || !IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	if (!Command.IsValid())
	{
		UE_LOG(LogWuwa,
		       Warning,
		       TEXT("服务端拒绝无效 Targeting 命令。Owner=%s, Sequence=%d"),
		       *GetNameSafe(OwnerActor),
		       Command.CommandSequence);
		if (Command.CommandSequence > 0)
		{
			CommitReplicatedAuthorityState(
			    Command.CommandSequence, false, EWuwaTargetingFailureReason::InvalidCandidate, false);
		}
		return;
	}

	if (Command.CommandSequence <= LastProcessedAuthorityTargetingCommandSequence)
	{
		CommitReplicatedAuthorityState(
		    Command.CommandSequence, false, EWuwaTargetingFailureReason::InvalidRequester, false);
		return;
	}

	LastProcessedAuthorityTargetingCommandSequence = Command.CommandSequence;
	if (Command.Kind == EWuwaTargetingNetworkCommandKind::ClearHardLock)
	{
		FWuwaReplicatedTargetingAuthorityState DesiredState;
		DesiredState.ProcessedCommandSequence = Command.CommandSequence;
		DesiredState.ContextSequence = ReplicatedTargetingAuthorityState.ContextSequence < MAX_int32
		                                   ? ReplicatedTargetingAuthorityState.ContextSequence + 1
		                                   : MAX_int32;
		if (!ReconcilePredictedTargetContext(DesiredState))
		{
			CommitReplicatedAuthorityState(
			    Command.CommandSequence, false, EWuwaTargetingFailureReason::StateCommitFailed, false);
			return;
		}

		CommitReplicatedAuthorityState(Command.CommandSequence, true, EWuwaTargetingFailureReason::None, true);
		return;
	}

	if (Command.Kind == EWuwaTargetingNetworkCommandKind::SwitchHardTarget &&
	    (TargetContext.Mode != EWuwaTargetingMode::Hard || !TargetContext.HasValidTarget()))
	{
		CommitReplicatedAuthorityState(Command.CommandSequence, false, EWuwaTargetingFailureReason::NoHardLock, false);
		return;
	}

	FWuwaTargetCandidate AuthorityCandidate;
	if (!ValidateAuthorityTarget(Command.RequestedTarget, AuthorityCandidate))
	{
		CommitReplicatedAuthorityState(
		    Command.CommandSequence, false, EWuwaTargetingFailureReason::InvalidCandidate, false);
		return;
	}

	FWuwaReplicatedTargetingAuthorityState DesiredState;
	DesiredState.ProcessedCommandSequence = Command.CommandSequence;
	DesiredState.Mode = EWuwaTargetingMode::Hard;
	DesiredState.TargetActor = Command.RequestedTarget;
	DesiredState.ContextSequence = ReplicatedTargetingAuthorityState.ContextSequence < MAX_int32
	                                   ? ReplicatedTargetingAuthorityState.ContextSequence + 1
	                                   : MAX_int32;
	if (!ReconcilePredictedTargetContext(DesiredState))
	{
		CommitReplicatedAuthorityState(
		    Command.CommandSequence, false, EWuwaTargetingFailureReason::StateCommitFailed, false);
		return;
	}

	TargetContext.TargetPoint = AuthorityCandidate.TargetPoint;
	TargetContext.Score = AuthorityCandidate.Score;
	CommitReplicatedAuthorityState(Command.CommandSequence, true, EWuwaTargetingFailureReason::None, true);
}

bool UWuwaTargetingComponent::ValidateAuthorityTarget(AActor* RequestedTarget, FWuwaTargetCandidate& OutCandidate) const
{
	OutCandidate = FWuwaTargetCandidate();
	if (!IsInitialized() || !IsValid(RequestedTarget) || !RequestedTarget->GetIsReplicated())
	{
		return false;
	}

	FWuwaTargetingViewSnapshot View;
	if (!CaptureViewSnapshot(View))
	{
		return false;
	}

	EWuwaTargetingFailureReason FailureReason = EWuwaTargetingFailureReason::None;
	return EvaluateCandidate(*RequestedTarget, View, OutCandidate, FailureReason);
}

bool UWuwaTargetingComponent::CommitReplicatedAuthorityState(const int32 ProcessedCommandSequence,
                                                             const bool bAccepted,
                                                             const EWuwaTargetingFailureReason FailureReason,
                                                             const bool bAdvanceContextSequence)
{
	AActor* OwnerActor = GetOwner();
	if (!IsInitialized() || !IsValid(OwnerActor) || !OwnerActor->HasAuthority() || ProcessedCommandSequence < 0)
	{
		return false;
	}

	FWuwaReplicatedTargetingAuthorityState NewState;
	NewState.ProcessedCommandSequence = ProcessedCommandSequence;
	NewState.bAccepted = bAccepted;
	NewState.FailureReason = bAccepted ? EWuwaTargetingFailureReason::None : FailureReason;
	NewState.ContextSequence = ReplicatedTargetingAuthorityState.ContextSequence;
	if (bAdvanceContextSequence && NewState.ContextSequence < MAX_int32)
	{
		++NewState.ContextSequence;
	}

	AActor* AuthorityTarget = TargetContext.TargetActor.Get();
	if (TargetContext.Mode == EWuwaTargetingMode::Hard && IsValid(AuthorityTarget) &&
	    AuthorityTarget->GetIsReplicated())
	{
		NewState.Mode = EWuwaTargetingMode::Hard;
		NewState.TargetActor = AuthorityTarget;
	}

	if (!NewState.IsValid())
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Targeting 权威状态构造失败。Owner=%s, Sequence=%d, Accepted=%d, Reason=%d"),
		       *GetNameSafe(OwnerActor),
		       ProcessedCommandSequence,
		       bAccepted ? 1 : 0,
		       static_cast<int32>(FailureReason));
		return false;
	}

	ReplicatedTargetingAuthorityState = NewState;
	OwnerActor->ForceNetUpdate();
	return true;
}

void UWuwaTargetingComponent::OnRep_TargetingAuthorityState()
{
	if (!ReplicatedTargetingAuthorityState.IsValid() ||
	    ReplicatedTargetingAuthorityState.ProcessedCommandSequence < LastProcessedAuthorityTargetingCommandSequence ||
	    ReplicatedTargetingAuthorityState.ProcessedCommandSequence + 1 < NextLocalTargetingCommandSequence)
	{
		return;
	}

	if (!ReconcilePredictedTargetContext(ReplicatedTargetingAuthorityState))
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Owner 无法对账 Targeting 权威状态。Owner=%s, Sequence=%d, ContextSequence=%d"),
		       *GetNameSafe(GetOwner()),
		       ReplicatedTargetingAuthorityState.ProcessedCommandSequence,
		       ReplicatedTargetingAuthorityState.ContextSequence);
		return;
	}

	LastProcessedAuthorityTargetingCommandSequence = FMath::Max(
	    LastProcessedAuthorityTargetingCommandSequence, ReplicatedTargetingAuthorityState.ProcessedCommandSequence);
	LastFailureReason = ReplicatedTargetingAuthorityState.bAccepted ? EWuwaTargetingFailureReason::None
	                                                                : ReplicatedTargetingAuthorityState.FailureReason;
}

bool UWuwaTargetingComponent::ReconcilePredictedTargetContext(
    const FWuwaReplicatedTargetingAuthorityState& AuthorityState)
{
	if (!IsInitialized() || !AuthorityState.IsValid())
	{
		return false;
	}

	AActor* AuthorityTarget = AuthorityState.TargetActor;
	FVector AuthorityTargetPoint = FVector::ZeroVector;
	if (AuthorityState.Mode != EWuwaTargetingMode::None)
	{
		if (!IsValid(AuthorityTarget) ||
		    !AuthorityTarget->GetClass()->ImplementsInterface(UWuwaTargetableInterface::StaticClass()))
		{
			return false;
		}

		AuthorityTargetPoint = IWuwaTargetableInterface::Execute_GetTargetingPoint(AuthorityTarget);
		if (AuthorityTargetPoint.ContainsNaN())
		{
			return false;
		}
	}

	if (AuthorityState.Mode == EWuwaTargetingMode::Hard)
	{
		if (!BindHardTargetDestroyed(AuthorityTarget))
		{
			return false;
		}

		if (!HardLockTagHandle.IsValid())
		{
			UWuwaStateTagComponent* Tags = StateTagComponent.Get();
			if (!IsValid(Tags))
			{
				UnbindHardTargetDestroyed();
				return false;
			}

			FWuwaStateTagHandle NewHandle = Tags->AcquireTag(WuwaGameplayTags::State_Targeting_HardLocked);
			if (!NewHandle.IsValid())
			{
				UnbindHardTargetDestroyed();
				return false;
			}
			HardLockTagHandle = MoveTemp(NewHandle);
		}
	}
	else
	{
		if (!ReleaseHardLockTag())
		{
			return false;
		}
		UnbindHardTargetDestroyed();
	}

	const int32 NextRevision = TargetContext.Revision < MAX_int32 ? TargetContext.Revision + 1 : MAX_int32;
	TargetContext = FWuwaTargetContext();
	TargetContext.Mode = AuthorityState.Mode;
	TargetContext.TargetActor = AuthorityTarget;
	TargetContext.TargetPoint = AuthorityTargetPoint;
	TargetContext.Revision = NextRevision;
	LastFailureReason = AuthorityState.bAccepted ? EWuwaTargetingFailureReason::None : AuthorityState.FailureReason;
	OnTargetContextChanged.Broadcast(TargetContext);
	return true;
}

namespace
{
constexpr double SoftTargetSnapshotRefreshIntervalCount = 2.0;

int32 AdvanceTargetContextRevision(const int32 CurrentRevision)
{
	// Revision 只用于判断语义状态是否变化；极端情况下饱和，避免有符号整数溢出。
	return CurrentRevision < MAX_int32 ? CurrentRevision + 1 : MAX_int32;
}

FString GetTargetDebugName(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return TEXT("None");
	}

#if WITH_EDITOR
	const FString ActorLabel = Actor->GetActorLabel();

	if (!ActorLabel.IsEmpty())
	{
		return ActorLabel;
	}
#endif

	return Actor->GetName();
}
}

bool UWuwaTargetingComponent::CaptureViewSnapshot(FWuwaTargetingViewSnapshot& OutView) const
{
	OutView = FWuwaTargetingViewSnapshot();

	AActor* RequesterActor = Requester.Get();

	if (!IsValid(RequesterActor))
	{
		return false;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;

	const APawn* RequesterPawn = Cast<APawn>(RequesterActor);
	APlayerController* PlayerController =
	    RequesterPawn ? Cast<APlayerController>(RequesterPawn->GetController()) : nullptr;

	if (IsValid(PlayerController))
	{
		// Targeting 读取最终玩家视图，但不修改 Camera 或 Control Rotation
		PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else
	{
		// 非玩家控制的 Pawn 或其他 Actor，使用 Actor 视点作为安全退化路径。
		RequesterActor->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	}

	if (ViewLocation.ContainsNaN() || ViewRotation.ContainsNaN())
	{
		return false;
	}

	const FRotationMatrix ViewMatrix(ViewRotation);

	OutView.Location = ViewLocation;
	OutView.Forward = ViewMatrix.GetUnitAxis(EAxis::X);
	OutView.Right = ViewMatrix.GetUnitAxis(EAxis::Y);
	OutView.Up = ViewMatrix.GetUnitAxis(EAxis::Z);

	return OutView.IsValid();
}

void UWuwaTargetingComponent::GatherCandidates(const FWuwaTargetingViewSnapshot& View,
                                               TArray<FWuwaTargetCandidate>& OutCandidates) const
{
	OutCandidates.Reset();

	UWorld* World = GetWorld();
	AActor* RequesterActor = Requester.Get();

	if (!IsValid(World) || !IsValid(RequesterActor) || !View.IsValid())
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	bool bHasValidObjectType = false;

	for (const TEnumAsByte<EObjectTypeQuery> ObjectType : RuntimeConfig.CandidateObjectTypes)
	{
		const ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(ObjectType.GetValue());

		if (CollisionChannel == ECC_MAX)
		{
			continue;
		}

		ObjectQueryParams.AddObjectTypesToQuery(CollisionChannel);
		bHasValidObjectType = true;
	}

	if (!bHasValidObjectType)
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WuwaTargetingGather), false, RequesterActor);

	TArray<FOverlapResult> OverlapResults;

	World->OverlapMultiByObjectType(OverlapResults,
	                                RequesterActor->GetActorLocation(),
	                                FQuat::Identity,
	                                ObjectQueryParams,
	                                FCollisionShape::MakeSphere(RuntimeConfig.SearchRadius),
	                                QueryParams);

	// Overlap 可能为同一 Actor 返回多个组件
	TArray<AActor*> CandidateActors;

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* CandidateActor = OverlapResult.GetActor();

		if (!IsValid(CandidateActor) || CandidateActor == RequesterActor)
		{
			continue;
		}

		CandidateActors.AddUnique(CandidateActor);
	}

	for (AActor* CandidateActor : CandidateActors)
	{
		if (!IsValid(CandidateActor))
		{
			continue;
		}

		FWuwaTargetCandidate Candidate;
		EWuwaTargetingFailureReason FailureReason = EWuwaTargetingFailureReason::None;

		if (EvaluateCandidate(*CandidateActor, View, Candidate, FailureReason))
		{
			OutCandidates.Add(MoveTemp(Candidate));
		}
	}

	// 固定同分候选的选择顺序
	OutCandidates.Sort(
	    [](const FWuwaTargetCandidate& Left, const FWuwaTargetCandidate& Right)
	    {
		    const AActor* LeftActor = Left.TargetActor.Get();
		    const AActor* RightActor = Right.TargetActor.Get();

		    if (!IsValid(LeftActor))
		    {
			    return false;
		    }

		    if (!IsValid(RightActor))
		    {
			    return true;
		    }

		    return Left.TargetActor->GetName().Compare(Right.TargetActor->GetName()) < 0;
	    });
}

bool UWuwaTargetingComponent::EvaluateCandidate(AActor& CandidateActor,
                                                const FWuwaTargetingViewSnapshot& View,
                                                FWuwaTargetCandidate& OutCandidate,
                                                EWuwaTargetingFailureReason& OutFailureReason) const
{
	OutCandidate = FWuwaTargetCandidate();
	OutFailureReason = EWuwaTargetingFailureReason::None;

	AActor* RequesterActor = Requester.Get();

	if (!IsValid(RequesterActor) || !IsValid(&CandidateActor) || &CandidateActor == RequesterActor)
	{
		OutFailureReason = EWuwaTargetingFailureReason::InvalidCandidate;
		return false;
	}

	if (!CandidateActor.GetClass()->ImplementsInterface(UWuwaTargetableInterface::StaticClass()))
	{
		OutFailureReason = EWuwaTargetingFailureReason::NotTargetable;
		return false;
	}

	if (!IWuwaTargetableInterface::Execute_CanBeTargetedBy(&CandidateActor, RequesterActor))
	{
		OutFailureReason = EWuwaTargetingFailureReason::NotTargetable;
		return false;
	}

	const FVector TargetPoint = IWuwaTargetableInterface::Execute_GetTargetingPoint(&CandidateActor);

	if (TargetPoint.ContainsNaN())
	{
		OutFailureReason = EWuwaTargetingFailureReason::InvalidCandidate;
		return false;
	}

	const float Distance = FVector::Distance(RequesterActor->GetActorLocation(), TargetPoint);

	if (!FMath::IsFinite(Distance) || Distance > RuntimeConfig.SearchRadius)
	{
		OutFailureReason = EWuwaTargetingFailureReason::OutOfRange;
		return false;
	}

	const FVector ViewToTarget = TargetPoint - View.Location;

	const float ViewDepth = FVector::DotProduct(ViewToTarget, View.Forward);

	if (ViewToTarget.IsNearlyZero() || !FMath::IsFinite(ViewDepth) || ViewDepth < UE_KINDA_SMALL_NUMBER)
	{
		OutFailureReason = EWuwaTargetingFailureReason::OutsideAcquireAngle;
		return false;
	}

	const FVector ViewDirection = ViewToTarget.GetSafeNormal();

	const float ViewForwardDot = FMath::Clamp(FVector::DotProduct(ViewDirection, View.Forward), -1.f, 1.f);

	const float ViewAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(ViewForwardDot));

	if (!FMath::IsFinite(ViewAngleDegrees) || ViewAngleDegrees > RuntimeConfig.MaxAcquireAngleDegrees)
	{
		OutFailureReason = EWuwaTargetingFailureReason::OutsideAcquireAngle;
		return false;
	}

	const float ViewSpaceVertical = FVector::DotProduct(ViewToTarget, View.Up) / ViewDepth;
	const float ViewSpaceHorizontal = FVector::DotProduct(ViewToTarget, View.Right) / ViewDepth;

	if (!FMath::IsFinite(ViewSpaceHorizontal) || !FMath::IsFinite(ViewSpaceVertical))
	{
		OutFailureReason = EWuwaTargetingFailureReason::InvalidCandidate;
		return false;
	}

	if (!IsCandidateVisible(CandidateActor, TargetPoint, View))
	{
		OutFailureReason = EWuwaTargetingFailureReason::Occluded;
		return false;
	}

	FWuwaTargetScoreInput ScoreInput;
	ScoreInput.Distance = Distance;
	ScoreInput.SearchRadius = RuntimeConfig.SearchRadius;
	ScoreInput.ViewAngleDegrees = ViewAngleDegrees;
	ScoreInput.MaxAcquireAngleDegrees = RuntimeConfig.MaxAcquireAngleDegrees;
	ScoreInput.ViewSpaceHorizontal = ViewSpaceHorizontal;
	ScoreInput.ViewSpaceVertical = ViewSpaceVertical;
	ScoreInput.DistanceWeight = RuntimeConfig.DistanceWeight;
	ScoreInput.ViewAlignmentWeight = RuntimeConfig.ViewAlignmentWeight;

	ScoreInput.RetentionBonus =
	    TargetContext.Mode == EWuwaTargetingMode::Soft && TargetContext.TargetActor.Get() == &CandidateActor
	        ? RuntimeConfig.CurrentSoftTargetBonus
	        : 0.f;

	FWuwaTargetScoreBreakdown Score;
	if (!WuwaTargetingRules::CalculateScore(ScoreInput, Score))
	{
		OutFailureReason = EWuwaTargetingFailureReason::InvalidCandidate;
		return false;
	}

	OutCandidate.TargetActor = &CandidateActor;
	OutCandidate.TargetPoint = TargetPoint;
	OutCandidate.Score = Score;

	return OutCandidate.IsValid();
}

bool UWuwaTargetingComponent::IsCandidateVisible(const AActor& CandidateActor,
                                                 const FVector& TargetPoint,
                                                 const FWuwaTargetingViewSnapshot& View) const
{
	UWorld* World = GetWorld();
	AActor* RequesterActor = Requester.Get();

	if (!IsValid(World) || !IsValid(RequesterActor) || TargetPoint.ContainsNaN() || !View.IsValid())
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WuwaTargetingVisibility), true, RequesterActor);
	QueryParams.AddIgnoredActor(RequesterActor);

	FHitResult HitResult;

	const bool bBlockingHit = World->LineTraceSingleByChannel(
	    HitResult, View.Location, TargetPoint, RuntimeConfig.VisibilityTraceChannel, QueryParams);

	return !bBlockingHit || (HitResult.GetActor() == &CandidateActor);
}

bool UWuwaTargetingComponent::BuildHardTargetSwitchOriginScore(const FWuwaTargetingViewSnapshot& View,
                                                               FWuwaTargetScoreBreakdown& OutScore) const
{
	OutScore = FWuwaTargetScoreBreakdown();

	const AActor* RequesterActor = Requester.Get();

	if (!IsValid(RequesterActor) || TargetContext.Mode != EWuwaTargetingMode::Hard || !TargetContext.HasValidTarget() ||
	    !View.IsValid())
	{
		return false;
	}

	const FVector TargetPoint = TargetContext.TargetPoint;

	const bool bFiniteTargetPoint =
	    FMath::IsFinite(TargetPoint.X) && FMath::IsFinite(TargetPoint.Y) && FMath::IsFinite(TargetPoint.Z);

	if (!bFiniteTargetPoint)
	{
		return false;
	}

	const FVector ViewToTarget = TargetPoint - View.Location;
	const float ViewDepth = FVector::DotProduct(ViewToTarget, View.Forward);

	if (ViewToTarget.IsNearlyZero() || !FMath::IsFinite(ViewDepth) || ViewDepth < UE_KINDA_SMALL_NUMBER)
	{
		// 目标位于相机后方时没有稳定的画面左右关系，拒绝切换但保留 Hard。
		return false;
	}

	const FVector ViewDirection = ViewToTarget.GetSafeNormal();

	const float ForwardDot = FMath::Clamp(FVector::DotProduct(ViewDirection, View.Forward), -1.f, 1.f);

	const float ViewAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(ForwardDot));

	const float ViewSpaceHorizontal = FVector::DotProduct(ViewToTarget, View.Right) / ViewDepth;
	const float ViewSpaceVertical = FVector::DotProduct(ViewToTarget, View.Up) / ViewDepth;

	const float Distance = FVector::Distance(RequesterActor->GetActorLocation(), TargetPoint);

	if (!FMath::IsFinite(ViewAngleDegrees) || !FMath::IsFinite(ViewSpaceHorizontal) ||
	    !FMath::IsFinite(ViewSpaceVertical) || !FMath::IsFinite(Distance))
	{
		return false;
	}

	// 切换规则使用当前目标的实际视空间位置
	OutScore.Distance = Distance;
	OutScore.ViewAngleDegrees = ViewAngleDegrees;
	OutScore.ViewSpaceHorizontal = ViewSpaceHorizontal;
	OutScore.ViewSpaceVertical = ViewSpaceVertical;

	OutScore.DistanceScore = 1.f - FMath::Clamp(Distance / RuntimeConfig.SearchRadius, 0.f, 1.f);
	OutScore.ViewAlignmentScore = 1.f - FMath::Clamp(ViewAngleDegrees / RuntimeConfig.MaxAcquireAngleDegrees, 0.f, 1.f);

	OutScore.RetentionBonus = 0.f;

	OutScore.TotalScore = OutScore.DistanceScore * RuntimeConfig.DistanceWeight +
	                      OutScore.ViewAlignmentScore * RuntimeConfig.ViewAlignmentWeight;

	// Hard Target 在遮挡宽限内仍是有效切换原点；可见性只过滤新候选。
	OutScore.bVisible = true;

	return OutScore.IsFinite();
}

FWuwaTargetingResult UWuwaTargetingComponent::RefreshSoftTarget()
{
	FWuwaTargetingResult Result;
	Result.Context = TargetContext;

	if (!IsInitialized())
	{
		LastFailureReason = EWuwaTargetingFailureReason::NotInitialized;
		Result.FailureReason = LastFailureReason;
		return Result;
	}

	if (TargetContext.Mode == EWuwaTargetingMode::Hard)
	{
		// Soft 刷新不覆盖 Hard Context
		Result.bSucceeded = TargetContext.HasValidTarget();
		Result.FailureReason =
		    Result.bSucceeded ? EWuwaTargetingFailureReason::None : EWuwaTargetingFailureReason::InvalidCandidate;
		Result.Context = TargetContext;

		return Result;
	}

	FWuwaTargetingViewSnapshot View;

	if (!CaptureViewSnapshot(View))
	{
		ClearSoftTarget();

		LastFailureReason = EWuwaTargetingFailureReason::InvalidView;

		Result.Context = TargetContext;
		Result.FailureReason = LastFailureReason;
		return Result;
	}

	TArray<FWuwaTargetCandidate> Candidates;
	GatherCandidates(View, Candidates);

	TArray<FWuwaTargetScoreBreakdown> CandidateScores;

	CandidateScores.Reserve(Candidates.Num());

	for (const FWuwaTargetCandidate& Candidate : Candidates)
	{
		CandidateScores.Add(Candidate.Score);
	}

	const int32 BestCandidateIndex = WuwaTargetingRules::SelectBestCandidateIndex(CandidateScores);

	if (!Candidates.IsValidIndex(BestCandidateIndex))
	{
		ClearSoftTarget();

		LastFailureReason = EWuwaTargetingFailureReason::NoCandidate;

		Result.Context = TargetContext;
		Result.FailureReason = LastFailureReason;
		return Result;
	}

	CommitSoftTarget(Candidates[BestCandidateIndex]);

	LastFailureReason = EWuwaTargetingFailureReason::None;

	Result.Context = TargetContext;
	Result.bSucceeded = true;
	Result.FailureReason = EWuwaTargetingFailureReason::None;

	return Result;
}

FWuwaTargetingResult UWuwaTargetingComponent::ToggleHardLock()
{
	if (TargetContext.Mode == EWuwaTargetingMode::Hard)
	{
		return ClearHardLock();
	}

	return TryEnterHardLock();
}

FWuwaTargetingResult UWuwaTargetingComponent::SwitchHardTarget(const float Direction)
{
	FWuwaTargetingResult Result;
	Result.Context = TargetContext;

	if (IsValid(GetOwner()) && GetOwner()->GetLocalRole() == ROLE_SimulatedProxy)
	{
		LastFailureReason = EWuwaTargetingFailureReason::InvalidRequester;
		Result.FailureReason = LastFailureReason;
		return Result;
	}

	if (!IsInitialized())
	{
		LastFailureReason = EWuwaTargetingFailureReason::NotInitialized;

		Result.FailureReason = LastFailureReason;
		return Result;
	}

	if (!FMath::IsFinite(Direction) || FMath::IsNearlyZero(Direction))
	{
		LastFailureReason = EWuwaTargetingFailureReason::InvalidDirection;

		Result.FailureReason = LastFailureReason;
		return Result;
	}

	if (TargetContext.Mode != EWuwaTargetingMode::Hard || !TargetContext.HasValidTarget())
	{
		LastFailureReason = EWuwaTargetingFailureReason::NoHardLock;

		Result.FailureReason = LastFailureReason;
		return Result;
	}

	if (!HardLockTagHandle.IsValid())
	{
		// 标签句柄必须与 Hard Context 配对
		LastFailureReason = EWuwaTargetingFailureReason::StateCommitFailed;

		Result.FailureReason = LastFailureReason;
		return Result;
	}

	// 输入发生时立即验证当前目标，不能基于上一轮 Timer 的过期目标点切换。
	ValidateCurrentHardTarget();

	if (TargetContext.Mode != EWuwaTargetingMode::Hard || !TargetContext.HasValidTarget())
	{
		Result.Context = TargetContext;
		Result.FailureReason = LastFailureReason != EWuwaTargetingFailureReason::None
		                           ? LastFailureReason
		                           : EWuwaTargetingFailureReason::NoHardLock;
		return Result;
	}

	AActor* CurrentTarget = TargetContext.TargetActor.Get();

	if (!IsValid(CurrentTarget) || BoundHardTarget.Get() != CurrentTarget)
	{
		// Context 与销毁观察不一致时禁止继续切换。
		LastFailureReason = EWuwaTargetingFailureReason::StateCommitFailed;

		Result.FailureReason = LastFailureReason;
		Result.Context = TargetContext;
		return Result;
	}

	FWuwaTargetingViewSnapshot View;

	if (!CaptureViewSnapshot(View))
	{
		LastFailureReason = EWuwaTargetingFailureReason::InvalidView;

		Result.FailureReason = LastFailureReason;
		Result.Context = TargetContext;
		return Result;
	}

	FWuwaTargetScoreBreakdown CurrentOriginScore;

	if (!BuildHardTargetSwitchOriginScore(View, CurrentOriginScore))
	{
		// 当前目标在相机后方等情况没有可靠左右坐标，但原硬锁继续保持。
		LastFailureReason = EWuwaTargetingFailureReason::InvalidView;

		Result.FailureReason = LastFailureReason;
		Result.Context = TargetContext;
		return Result;
	}

	TArray<FWuwaTargetCandidate> GatheredCandidates;
	GatherCandidates(View, GatheredCandidates);

	// 索引 0 固定为当前 Hard Target，后续索引与 SwitchCandidates 相差 1。
	TArray<FWuwaTargetScoreBreakdown> SwitchScores;

	SwitchScores.Reserve(GatheredCandidates.Num() + 1);
	SwitchScores.Add(CurrentOriginScore);

	TArray<FWuwaTargetCandidate> SwitchCandidates;

	SwitchCandidates.Reserve(GatheredCandidates.Num());

	for (const FWuwaTargetCandidate& Candidate : GatheredCandidates)
	{
		if (!Candidate.IsValid() || Candidate.TargetActor.Get() == CurrentTarget)
		{
			// 当前目标只能作为左右坐标原点，不能再次被选中。
			continue;
		}

		SwitchCandidates.Add(Candidate);
		SwitchScores.Add(Candidate.Score);
	}

	const int32 SelectedScoreIndex =
	    WuwaTargetingRules::SelectSwitchCandidateIndex(SwitchScores,
	                                                   0,
	                                                   Direction,
	                                                   RuntimeConfig.SwitchMinimumHorizontalDelta,
	                                                   RuntimeConfig.SwitchVerticalPenaltyWeight,
	                                                   RuntimeConfig.SwitchDistancePenaltyWeight);

	const int32 SelectedCandidateIndex = SelectedScoreIndex - 1;

	if (!SwitchCandidates.IsValidIndex(SelectedCandidateIndex))
	{
		// 方向上没有候选时，原 Context、标签和销毁绑定完全不变。
		LastFailureReason = EWuwaTargetingFailureReason::NoCandidate;

		Result.FailureReason = LastFailureReason;
		Result.Context = TargetContext;
		return Result;
	}

	const FWuwaTargetCandidate& SelectedCandidate = SwitchCandidates[SelectedCandidateIndex];

	AActor* NewTarget = SelectedCandidate.TargetActor.Get();

	if (!IsValid(NewTarget) || NewTarget == CurrentTarget ||
	    (GetOwner()->GetLocalRole() == ROLE_AutonomousProxy && !NewTarget->GetIsReplicated()))
	{
		// 目标在切换过程中被销毁或失效，原 Context、标签和销毁绑定完全不变。
		LastFailureReason = EWuwaTargetingFailureReason::InvalidCandidate;

		Result.FailureReason = LastFailureReason;
		Result.Context = TargetContext;
		return Result;
	}

	// 新绑定成功前不触碰 Context；失败时旧 Hard Target 仍有完整销毁观察。
	if (!BindHardTargetDestroyed(NewTarget))
	{
		LastFailureReason = EWuwaTargetingFailureReason::StateCommitFailed;

		Result.FailureReason = LastFailureReason;
		Result.Context = TargetContext;
		return Result;
	}

	// 切换目标时沿用 HardLock 标签句柄
	TargetContext.TargetActor = SelectedCandidate.TargetActor;
	TargetContext.TargetPoint = SelectedCandidate.TargetPoint;
	TargetContext.Score = SelectedCandidate.Score;
	TargetContext.Mode = EWuwaTargetingMode::Hard;
	TargetContext.Revision = AdvanceTargetContextRevision(TargetContext.Revision);

	LastFailureReason = EWuwaTargetingFailureReason::None;

	OnTargetContextChanged.Broadcast(TargetContext);

	Result.bSucceeded = true;
	Result.FailureReason = EWuwaTargetingFailureReason::None;
	Result.Context = TargetContext;

	if (!SubmitTargetingCommand(EWuwaTargetingNetworkCommandKind::SwitchHardTarget, NewTarget))
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Hard Target 切换结果无法进入权威链路。Owner=%s, Target=%s"),
		       *GetNameSafe(GetOwner()),
		       *GetNameSafe(NewTarget));
	}

	return Result;
}

FWuwaTargetingResult UWuwaTargetingComponent::TryEnterHardLock()
{
	FWuwaTargetingResult Result;
	Result.Context = TargetContext;

	if (IsValid(GetOwner()) && GetOwner()->GetLocalRole() == ROLE_SimulatedProxy)
	{
		LastFailureReason = EWuwaTargetingFailureReason::InvalidRequester;
		Result.FailureReason = LastFailureReason;
		return Result;
	}

	if (!IsInitialized())
	{
		LastFailureReason = EWuwaTargetingFailureReason::NotInitialized;

		Result.FailureReason = LastFailureReason;
		return Result;
	}

	if (TargetContext.Mode == EWuwaTargetingMode::Hard && TargetContext.HasValidTarget())
	{
		LastFailureReason = EWuwaTargetingFailureReason::None;

		Result.bSucceeded = true;
		Result.FailureReason = EWuwaTargetingFailureReason::None;
		Result.Context = TargetContext;
		return Result;
	}

	if (HardLockTagHandle.IsValid())
	{
		// Context 与句柄状态不一致时拒绝继续，避免重复标签来源。
		LastFailureReason = EWuwaTargetingFailureReason::StateCommitFailed;

		Result.FailureReason = LastFailureReason;
		return Result;
	}

	FWuwaTargetContext SoftTargetSnapshot;
	bool bUsesRecentSoftTargetSnapshot = false;
	if (TargetContext.Mode == EWuwaTargetingMode::Soft && TargetContext.HasValidTarget())
	{
		SoftTargetSnapshot = TargetContext;
	}
	else if (TryGetRecentSoftTargetSnapshot(SoftTargetSnapshot))
	{
		bUsesRecentSoftTargetSnapshot = true;
	}
	else
	{
		// 使用输入时刻的候选结果
		const FWuwaTargetingResult SoftResult = RefreshSoftTarget();

		if (!SoftResult.bSucceeded || TargetContext.Mode != EWuwaTargetingMode::Soft || !TargetContext.HasValidTarget())
		{
			return SoftResult;
		}

		SoftTargetSnapshot = TargetContext;
	}

	UWuwaStateTagComponent* Tags = StateTagComponent.Get();

	if (!IsValid(Tags))
	{
		LastFailureReason = EWuwaTargetingFailureReason::NotInitialized;

		Result.FailureReason = LastFailureReason;
		Result.Context = TargetContext;
		return Result;
	}

	AActor* NewHardTarget = SoftTargetSnapshot.TargetActor.Get();

	if (!IsValid(NewHardTarget) ||
	    (GetOwner()->GetLocalRole() == ROLE_AutonomousProxy && !NewHardTarget->GetIsReplicated()))
	{
		LastFailureReason = EWuwaTargetingFailureReason::InvalidCandidate;

		Result.FailureReason = LastFailureReason;
		Result.Context = TargetContext;
		return Result;
	}

	// 先建立销毁观察；后续标签取得失败时必须回滚绑定。
	if (!BindHardTargetDestroyed(NewHardTarget))
	{
		LastFailureReason = EWuwaTargetingFailureReason::StateCommitFailed;

		Result.FailureReason = LastFailureReason;
		Result.Context = TargetContext;
		return Result;
	}

	FWuwaStateTagHandle NewHandle = Tags->AcquireTag(WuwaGameplayTags::State_Targeting_HardLocked);

	if (!NewHandle.IsValid())
	{
		// 标签提交失败时解绑目标，Soft Context 保持不变。
		UnbindHardTargetDestroyed();

		LastFailureReason = EWuwaTargetingFailureReason::StateCommitFailed;

		Result.FailureReason = LastFailureReason;
		Result.Context = TargetContext;
		return Result;
	}

	HardLockTagHandle = MoveTemp(NewHandle);

	if (bUsesRecentSoftTargetSnapshot)
	{
		const int32 CurrentRevision = TargetContext.Revision;
		TargetContext = SoftTargetSnapshot;
		TargetContext.Revision = CurrentRevision;
	}

	TargetContext.Mode = EWuwaTargetingMode::Hard;

	TargetContext.Revision = AdvanceTargetContextRevision(TargetContext.Revision);

	LastFailureReason = EWuwaTargetingFailureReason::None;

	OnTargetContextChanged.Broadcast(TargetContext);

	Result.bSucceeded = true;
	Result.FailureReason = EWuwaTargetingFailureReason::None;
	Result.Context = TargetContext;

	if (!SubmitTargetingCommand(EWuwaTargetingNetworkCommandKind::EnterHardLock, NewHardTarget))
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Hard Lock 结果无法进入权威链路。Owner=%s, Target=%s"),
		       *GetNameSafe(GetOwner()),
		       *GetNameSafe(NewHardTarget));
	}

	return Result;
}

bool UWuwaTargetingComponent::BindHardTargetDestroyed(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	AActor* PreviousTarget = BoundHardTarget.Get();

	if (PreviousTarget == TargetActor)
	{
		// 新验证周期重新计算连续遮挡时间
		HardTargetOcclusionStartTimeSeconds = -1.0;
		return true;
	}

	// 先绑定新目标；只有新目标确认有效后才能移除旧目标观察。
	// Actor 委托在 Game Thread 同步修改，不存在中途并发销毁窗口。
	TargetActor->OnDestroyed.AddUniqueDynamic(this, &UWuwaTargetingComponent::HandleHardTargetDestroyed);

	if (IsValid(PreviousTarget))
	{
		PreviousTarget->OnDestroyed.RemoveDynamic(this, &UWuwaTargetingComponent::HandleHardTargetDestroyed);
	}

	BoundHardTarget = TargetActor;
	HardTargetOcclusionStartTimeSeconds = -1.0;

	return true;
}

void UWuwaTargetingComponent::UnbindHardTargetDestroyed()
{
	if (AActor* TargetActor = BoundHardTarget.Get())
	{
		TargetActor->OnDestroyed.RemoveDynamic(this, &UWuwaTargetingComponent::HandleHardTargetDestroyed);
	}

	BoundHardTarget.Reset();

	// 目标委托与遮挡计时同步清理
	HardTargetOcclusionStartTimeSeconds = -1.0;
}

void UWuwaTargetingComponent::HandleHardTargetDestroyed(AActor* DestroyedActor)
{
	if (TargetContext.Mode != EWuwaTargetingMode::Hard)
	{
		return;
	}

	const FWuwaTargetingResult ClearResult = ClearHardLock();

	if (!ClearResult.bSucceeded)
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Hard Target 销毁后的清理失败。Owner=%s, Reason=%s"),
		       *GetNameSafe(GetOwner()),
		       *UEnum::GetValueAsString(ClearResult.FailureReason));
	}
}

void UWuwaTargetingComponent::ValidateCurrentHardTarget()
{
	if (TargetContext.Mode != EWuwaTargetingMode::Hard)
	{
		return;
	}

	AActor* RequesterActor = Requester.Get();
	AActor* TargetActor = TargetContext.TargetActor.Get();

	if (!IsValid(RequesterActor) || !IsValid(TargetActor))
	{
		ExitHardLockForValidationFailure(EWuwaTargetingFailureReason::InvalidCandidate);
		return;
	}

	if (!TargetActor->GetClass()->ImplementsInterface(UWuwaTargetableInterface::StaticClass()) ||
	    !IWuwaTargetableInterface::Execute_CanBeTargetedBy(TargetActor, RequesterActor))
	{
		// 目标自身决定死亡、阵营变化或临时不可锁定后的资格。
		ExitHardLockForValidationFailure(EWuwaTargetingFailureReason::NotTargetable);
		return;
	}

	const FVector TargetPoint = IWuwaTargetableInterface::Execute_GetTargetingPoint(TargetActor);

	const bool bFiniteTargetPoint =
	    FMath::IsFinite(TargetPoint.X) && FMath::IsFinite(TargetPoint.Y) && FMath::IsFinite(TargetPoint.Z);

	if (!bFiniteTargetPoint)
	{
		ExitHardLockForValidationFailure(EWuwaTargetingFailureReason::InvalidCandidate);
		return;
	}

	const float Distance = FVector::Distance(RequesterActor->GetActorLocation(), TargetPoint);

	if (!FMath::IsFinite(Distance))
	{
		ExitHardLockForValidationFailure(EWuwaTargetingFailureReason::InvalidCandidate);
		return;
	}

	if (Distance > RuntimeConfig.HardLockReleaseDistance)
	{
		// Hard 使用独立释放距离，不能重新套用 Soft 的 SearchRadius。
		ExitHardLockForValidationFailure(EWuwaTargetingFailureReason::OutOfRange);
		return;
	}

	// Actor 和 Mode 没有变化，只刷新最近验证的目标点，不增加 Revision 或广播事件。
	TargetContext.TargetPoint = TargetPoint;

	FWuwaTargetingViewSnapshot View;

	if (!CaptureViewSnapshot(View))
	{
		ExitHardLockForValidationFailure(EWuwaTargetingFailureReason::InvalidView);
		return;
	}

	if (IsCandidateVisible(*TargetActor, TargetPoint, View))
	{
		// 可见性恢复必须重新开始下一段遮挡计时，不能累计非连续遮挡。
		HardTargetOcclusionStartTimeSeconds = -1.0;
		LastFailureReason = EWuwaTargetingFailureReason::None;
		return;
	}

	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		ExitHardLockForValidationFailure(EWuwaTargetingFailureReason::InvalidView);
		return;
	}

	const double CurrentTimeSeconds = static_cast<double>(World->GetTimeSeconds());

	if (HardTargetOcclusionStartTimeSeconds < 0.0)
	{
		// 第一次发现遮挡只开始宽限，不立即解除 Hard。
		HardTargetOcclusionStartTimeSeconds = CurrentTimeSeconds;
		LastFailureReason = EWuwaTargetingFailureReason::None;
		return;
	}

	const double OcclusionDuration = CurrentTimeSeconds - HardTargetOcclusionStartTimeSeconds;

	// 宽限时间内的遮挡不解除 Hard，允许玩家短暂躲避或遮挡物经过。
	if (OcclusionDuration < static_cast<double>(RuntimeConfig.OcclusionGraceTime))
	{
		LastFailureReason = EWuwaTargetingFailureReason::None;
		return;
	}

	ExitHardLockForValidationFailure(EWuwaTargetingFailureReason::Occluded);
}

void UWuwaTargetingComponent::ExitHardLockForValidationFailure(EWuwaTargetingFailureReason FailureReason)
{
	const FString PreviousTargetName = GetTargetDebugName(TargetContext.TargetActor.Get());

	const FWuwaTargetingResult ClearResult = ClearHardLock();

	if (!ClearResult.bSucceeded)
	{
		// ClearHardLock 失败时保持原 Hard 状态，并保留其 StateCommitFailed 原因。
		UE_LOG(
		    LogWuwa,
		    Error,
		    TEXT(
		        "Hard Target 验证失败，但 Hard 状态清理失败。Owner=%s, Target=%s, ValidationReason=%s, ClearReason=%s"),
		    *GetNameSafe(GetOwner()),
		    *PreviousTargetName,
		    *UEnum::GetValueAsString(FailureReason),
		    *UEnum::GetValueAsString(ClearResult.FailureReason));
		return;
	}

	// 清理成功后保留触发解除的领域原因，方便定位超距、资格或遮挡问题。
	LastFailureReason = FailureReason;
}

bool UWuwaTargetingComponent::ReleaseHardLockTag()
{
	if (!HardLockTagHandle.IsValid())
	{
		return true;
	}

	UWuwaStateTagComponent* Tags = StateTagComponent.Get();

	if (!IsValid(Tags))
	{
		return false;
	}

	return Tags->ReleaseTag(HardLockTagHandle);
}

FWuwaTargetingResult UWuwaTargetingComponent::ClearHardLock()
{
	FWuwaTargetingResult Result;
	Result.Context = TargetContext;

	if (IsValid(GetOwner()) && GetOwner()->GetLocalRole() == ROLE_SimulatedProxy)
	{
		LastFailureReason = EWuwaTargetingFailureReason::InvalidRequester;
		Result.FailureReason = LastFailureReason;
		return Result;
	}

	if (!IsInitialized())
	{
		LastFailureReason = EWuwaTargetingFailureReason::NotInitialized;

		Result.FailureReason = LastFailureReason;
		return Result;
	}

	if (TargetContext.Mode != EWuwaTargetingMode::Hard)
	{
		LastFailureReason = EWuwaTargetingFailureReason::NoHardLock;

		Result.FailureReason = LastFailureReason;
		return Result;
	}

	if (!ReleaseHardLockTag())
	{
		// 标签释放失败时保持原 Hard Context，禁止形成标签和 Context 分裂。
		LastFailureReason = EWuwaTargetingFailureReason::StateCommitFailed;

		Result.FailureReason = LastFailureReason;
		Result.Context = TargetContext;
		return Result;
	}

	// 标签事实释放成功后再解绑；若释放失败，仍保留完整 Hard 运行态。
	UnbindHardTargetDestroyed();

	const int32 PreviousRevision = TargetContext.Revision;

	// 中间态不对外广播
	TargetContext = FWuwaTargetContext();
	TargetContext.Revision = PreviousRevision;

	const FWuwaTargetingResult SoftResult = RefreshSoftTarget();

	if (SoftResult.bSucceeded)
	{
		LastFailureReason = EWuwaTargetingFailureReason::None;

		Result.bSucceeded = true;
		Result.FailureReason = EWuwaTargetingFailureReason::None;
		Result.Context = TargetContext;
		if (!SubmitTargetingCommand(EWuwaTargetingNetworkCommandKind::ClearHardLock, nullptr))
		{
			UE_LOG(LogWuwa, Error, TEXT("Hard Lock 清除结果无法进入权威链路。Owner=%s"), *GetNameSafe(GetOwner()));
		}
		return Result;
	}

	TargetContext = FWuwaTargetContext();
	TargetContext.Revision = AdvanceTargetContextRevision(PreviousRevision);

	LastFailureReason = EWuwaTargetingFailureReason::None;

	OnTargetContextChanged.Broadcast(TargetContext);

	Result.bSucceeded = true;
	Result.FailureReason = EWuwaTargetingFailureReason::None;
	Result.Context = TargetContext;

	if (!SubmitTargetingCommand(EWuwaTargetingNetworkCommandKind::ClearHardLock, nullptr))
	{
		UE_LOG(LogWuwa, Error, TEXT("Hard Lock 清除结果无法进入权威链路。Owner=%s"), *GetNameSafe(GetOwner()));
	}

	return Result;
}

void UWuwaTargetingComponent::CommitSoftTarget(const FWuwaTargetCandidate& Candidate)
{
	if (!Candidate.IsValid())
	{
		return;
	}

	const AActor* PreviousTarget = TargetContext.TargetActor.Get();

	const bool bSemanticChange =
	    TargetContext.Mode != EWuwaTargetingMode::Soft || PreviousTarget != Candidate.TargetActor.Get();

	const int32 PreviousRevision = TargetContext.Revision;

	TargetContext.TargetActor = Candidate.TargetActor;
	TargetContext.TargetPoint = Candidate.TargetPoint;
	TargetContext.Mode = EWuwaTargetingMode::Soft;
	TargetContext.Score = Candidate.Score;

	TargetContext.Revision = bSemanticChange ? AdvanceTargetContextRevision(PreviousRevision) : PreviousRevision;

	if (!CacheValidSoftTargetSnapshot())
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Soft Target 快照更新失败。Owner=%s, Target=%s"),
		       *GetNameSafe(GetOwner()),
		       *GetTargetDebugName(TargetContext.TargetActor.Get()));
	}

	if (!bSemanticChange)
	{
		// 同一目标只更新实时数据
		return;
	}

	OnTargetContextChanged.Broadcast(TargetContext);
}

void UWuwaTargetingComponent::ClearSoftTarget()
{
	if (TargetContext.Mode != EWuwaTargetingMode::Soft)
	{
		return;
	}

	const int32 NextRevision = AdvanceTargetContextRevision(TargetContext.Revision);

	TargetContext = FWuwaTargetContext();
	TargetContext.Revision = NextRevision;

	// 清理原因由 RefreshSoftTarget 写入 LastFailureReason；
	// Context 清理只负责释放目标事实并通知消费者。
	OnTargetContextChanged.Broadcast(TargetContext);
}

bool UWuwaTargetingComponent::CacheValidSoftTargetSnapshot()
{
	UWorld* World = GetWorld();
	if (TargetContext.Mode != EWuwaTargetingMode::Soft || !TargetContext.HasValidTarget() || !IsValid(World))
	{
		return false;
	}

	const double CapturedAtSeconds = static_cast<double>(World->GetTimeSeconds());
	if (!FMath::IsFinite(CapturedAtSeconds) || CapturedAtSeconds < 0.0)
	{
		return false;
	}

	LastValidSoftTargetSnapshot = TargetContext;
	LastValidSoftTargetSnapshotAtSeconds = CapturedAtSeconds;
	return true;
}

bool UWuwaTargetingComponent::TryGetRecentSoftTargetSnapshot(FWuwaTargetContext& OutSnapshot) const
{
	OutSnapshot = FWuwaTargetContext();

	const UWorld* World = GetWorld();
	const double MaxAgeSeconds =
	    static_cast<double>(RuntimeConfig.CandidateRefreshInterval) * SoftTargetSnapshotRefreshIntervalCount;
	if (!IsValid(World) || LastValidSoftTargetSnapshot.Mode != EWuwaTargetingMode::Soft ||
	    !LastValidSoftTargetSnapshot.HasValidTarget() || !FMath::IsFinite(LastValidSoftTargetSnapshotAtSeconds) ||
	    LastValidSoftTargetSnapshotAtSeconds < 0.0 || !FMath::IsFinite(MaxAgeSeconds) || MaxAgeSeconds <= 0.0)
	{
		return false;
	}

	const double CurrentTimeSeconds = static_cast<double>(World->GetTimeSeconds());
	const double SnapshotAgeSeconds = CurrentTimeSeconds - LastValidSoftTargetSnapshotAtSeconds;
	if (!FMath::IsFinite(CurrentTimeSeconds) || !FMath::IsFinite(SnapshotAgeSeconds) || SnapshotAgeSeconds < 0.0 ||
	    SnapshotAgeSeconds > MaxAgeSeconds)
	{
		return false;
	}

	OutSnapshot = LastValidSoftTargetSnapshot;
	return true;
}

void UWuwaTargetingComponent::StartCandidateRefreshTimer()
{
	ClearCandidateRefreshTimer();

	UWorld* World = GetWorld();

	if (!IsInitialized() || !IsValid(World) || !FMath::IsFinite(RuntimeConfig.CandidateRefreshInterval) ||
	    RuntimeConfig.CandidateRefreshInterval <= 0.f)
	{
		return;
	}

	World->GetTimerManager().SetTimer(CandidateRefreshTimerHandle,
	                                  this,
	                                  &UWuwaTargetingComponent::HandleCandidateRefreshTimer,
	                                  RuntimeConfig.CandidateRefreshInterval,
	                                  true,
	                                  RuntimeConfig.CandidateRefreshInterval);
}

void UWuwaTargetingComponent::ClearCandidateRefreshTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CandidateRefreshTimerHandle);
	}

	CandidateRefreshTimerHandle.Invalidate();
}

void UWuwaTargetingComponent::HandleCandidateRefreshTimer()
{
	if (!IsInitialized())
	{
		// 依赖已经失效时立即停止循环，不能继续访问 Requester 或目标。
		ClearCandidateRefreshTimer();
		return;
	}

	// Soft 刷新不覆盖 Hard Context
	if (TargetContext.Mode == EWuwaTargetingMode::Hard)
	{
		ValidateCurrentHardTarget();
		return;
	}

	RefreshSoftTarget();
}

void UWuwaTargetingComponent::ResetRuntimeState()
{
	// Timer 必须在 Requester 和运行时配置清空前停止。
	ClearCandidateRefreshTimer();
	UnbindHardTargetDestroyed();

	if (HardLockTagHandle.IsValid())
	{
		if (!ReleaseHardLockTag())
		{
			// Owner 销毁或依赖异常时不能继续保留本地所有权记录。
			// 正常运行路径必须通过 ClearHardLock 对称释放。
			UE_LOG(LogWuwa, Error, TEXT("Targeting 重置时释放 HardLock 标签失败。Owner=%s"), *GetNameSafe(GetOwner()));

			HardLockTagHandle.Reset();
		}
	}

	bInitialized = false;
	Requester.Reset();
	StateTagComponent = nullptr;
	RuntimeConfig = FWuwaTargetingRuntimeConfig();
	TargetContext = FWuwaTargetContext();
	LastValidSoftTargetSnapshot = FWuwaTargetContext();
	LastValidSoftTargetSnapshotAtSeconds = -1.0;
	ReplicatedTargetingAuthorityState = FWuwaReplicatedTargetingAuthorityState();

	LastFailureReason = EWuwaTargetingFailureReason::NotInitialized;
}

void UWuwaTargetingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UWuwaTargetingComponent, ReplicatedTargetingAuthorityState, COND_OwnerOnly);
}

void UWuwaTargetingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetRuntimeState();

	Super::EndPlay(EndPlayReason);
}

void UWuwaTargetingComponent::GatherHandledInputTags(FGameplayTagContainer& OutInputTags) const
{
	OutInputTags.Reset();
	OutInputTags.AddTag(WuwaGameplayTags::Input_LockTarget);
	OutInputTags.AddTag(WuwaGameplayTags::Input_SwitchTarget);
}

FWuwaCommandDispatchResult UWuwaTargetingComponent::HandleInputCommand(const FWuwaInputCommand& Command,
                                                                       const FWuwaInputFrame& InputFrame)
{
	(void)InputFrame;

	FWuwaCommandDispatchResult DispatchResult;
	DispatchResult.MessageTag = Command.InputTag;

	if (!Command.IsValid() || Command.Trigger != EWuwaInputCommandTrigger::Pressed || !IsValid(GetOwner()) ||
	    GetOwner()->GetLocalRole() == ROLE_SimulatedProxy ||
	    (Command.InputTag != WuwaGameplayTags::Input_LockTarget &&
	     Command.InputTag != WuwaGameplayTags::Input_SwitchTarget))
	{
		DispatchResult.Status = EWuwaCommandDispatchStatus::Rejected;
		return DispatchResult;
	}

	FWuwaTargetingCommandFact Fact;
	Fact.Header = Command.Header;
	Fact.InputTag = Command.InputTag;
	Fact.Result = Command.InputTag == WuwaGameplayTags::Input_LockTarget ? ToggleHardLock()
	                                                                     : SwitchHardTarget(Command.Direction.X);

	OnTargetingCommandFact.Broadcast(Fact);
	DispatchResult.Status = EWuwaCommandDispatchStatus::Handled;
	return DispatchResult;
}
