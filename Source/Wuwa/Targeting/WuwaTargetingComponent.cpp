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
#include "Wuwa.h"

UWuwaTargetingComponent::UWuwaTargetingComponent()
{
    // 候选刷新后续使用 Profile 驱动的 Timer，不为组件开启逐帧 Tick。
    PrimaryComponentTick.bCanEverTick = false;
}

bool UWuwaTargetingComponent::Initialize(AActor *InRequester, UWuwaStateTagComponent *InStateTagComponent, const UWuwaTargetingProfile *InProfile)
{
    // 初始化必须从干净状态开始；以后增加外部资源时仍由同一入口保证对称清理。
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

    // Profile 只在初始化边界读取一次，后续候选刷新使用稳定快照。
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

    // 首次刷新发生在完整依赖提交之后；无候选不代表组件初始化失败。
    RefreshSoftTarget();

    // 后续刷新只由本组件拥有的唯一 Timer 驱动。
    StartCandidateRefreshTimer();

    return true;
}

bool UWuwaTargetingComponent::IsInitialized() const
{
    return bInitialized && Requester.IsValid() && StateTagComponent.IsValid();
}

namespace
{
    int32 AdvanceTargetContextRevision(const int32 CurrentRevision)
    {
        // Revision 只用于判断语义状态是否变化；极端情况下饱和，避免有符号整数溢出。
        return CurrentRevision < MAX_int32 ? CurrentRevision + 1 : MAX_int32;
    }

    FString GetTargetDebugName(const AActor *Actor)
    {
        if (!IsValid(Actor))
        {
            return TEXT("None");
        }

#if WITH_EDITOR
        // Editor/PIE 优先显示 World Outliner 中配置的 Actor Label。
        const FString ActorLabel = Actor->GetActorLabel();

        if (!ActorLabel.IsEmpty())
        {
            return ActorLabel;
        }
#endif

        // 非 Editor 构建没有 Actor Label，回退到稳定可用的 UObject 名称。
        return Actor->GetName();
    }
}

bool UWuwaTargetingComponent::CaptureViewSnapshot(FWuwaTargetingViewSnapshot &OutView) const
{
    OutView = FWuwaTargetingViewSnapshot();

    AActor *RequesterActor = Requester.Get();

    if (!IsValid(RequesterActor))
    {
        return false;
    }

    FVector ViewLocation = FVector::ZeroVector;
    FRotator ViewRotation = FRotator::ZeroRotator;

    const APawn *RequesterPawn = Cast<APawn>(RequesterActor);
    APlayerController *PlayerController = RequesterPawn ? Cast<APlayerController>(RequesterPawn->GetController()) : nullptr;

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

void UWuwaTargetingComponent::GatherCandidates(const FWuwaTargetingViewSnapshot &View, TArray<FWuwaTargetCandidate> &OutCandidates) const
{
    OutCandidates.Reset();

    UWorld *World = GetWorld();
    AActor *RequesterActor = Requester.Get();

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

    World->OverlapMultiByObjectType(
        OverlapResults,
        RequesterActor->GetActorLocation(),
        FQuat::Identity,
        ObjectQueryParams,
        FCollisionShape::MakeSphere(RuntimeConfig.SearchRadius),
        QueryParams);

    // 一个 Actor 可能有多个碰撞组件，必须先去重，避免重复参与评分。
    TArray<AActor *> CandidateActors;

    for (const FOverlapResult &OverlapResult : OverlapResults)
    {
        AActor *CandidateActor = OverlapResult.GetActor();

        if (!IsValid(CandidateActor) || CandidateActor == RequesterActor)
        {
            continue;
        }

        CandidateActors.AddUnique(CandidateActor);
    }

    for (AActor *CandidateActor : CandidateActors)
    {
        if (!IsValid(CandidateActor))
        {
            continue;
        }

        FWuwaTargetCandidate Candidate;
        EWuwaTargetingFailureReason FailureReason = EWuwaTargetingFailureReason::None;

        if (EvaluateCandidate(*CandidateActor, View, Candidate, FailureReason))
        {
            // MoveTemp 避免 TArray 复制，FWuwaTargetCandidate 可能包含较大评分结构。
            OutCandidates.Add(MoveTemp(Candidate));
        }
    }

    // Overlap 返回顺序不是选择权威；先按 Actor 名称稳定排序，再使用纯规则处理同分候选。
    OutCandidates.Sort(
        [](const FWuwaTargetCandidate &Left,
           const FWuwaTargetCandidate &Right)
        {
            const AActor *LeftActor = Left.TargetActor.Get();
            const AActor *RightActor = Right.TargetActor.Get();

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

bool UWuwaTargetingComponent::EvaluateCandidate(AActor &CandidateActor, const FWuwaTargetingViewSnapshot &View, FWuwaTargetCandidate &OutCandidate, EWuwaTargetingFailureReason &OutFailureReason) const
{
    OutCandidate = FWuwaTargetCandidate();
    OutFailureReason = EWuwaTargetingFailureReason::None;

    AActor *RequesterActor = Requester.Get();

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

    // 计算视角时使用反余弦函数，避免在极端情况下出现 NaN。
    const float ViewAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(ViewForwardDot));

    if (!FMath::IsFinite(ViewAngleDegrees) || ViewAngleDegrees > RuntimeConfig.MaxAcquireAngleDegrees)
    {
        OutFailureReason = EWuwaTargetingFailureReason::OutsideAcquireAngle;
        return false;
    }

    // 计算视空间坐标，供左右切换使用；不依赖 viewport 分辨率。
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

    // 当前软锁目标在评分中获得额外加分，鼓励保持软锁状态。
    ScoreInput.RetentionBonus = TargetContext.Mode == EWuwaTargetingMode::Soft && TargetContext.TargetActor.Get() == &CandidateActor ? RuntimeConfig.CurrentSoftTargetBonus : 0.f;

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

bool UWuwaTargetingComponent::IsCandidateVisible(const AActor &CandidateActor, const FVector &TargetPoint, const FWuwaTargetingViewSnapshot &View) const
{
    UWorld *World = GetWorld();
    AActor *RequesterActor = Requester.Get();

    if (!IsValid(World) || !IsValid(RequesterActor) || TargetPoint.ContainsNaN() || !View.IsValid())
    {
        return false;
    }

    // 视点到目标点的射线必须不被任何阻挡物遮挡，才能认为候选可见。
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WuwaTargetingVisibility), true, RequesterActor);
    QueryParams.AddIgnoredActor(RequesterActor);

    FHitResult HitResult;

    // LineTraceSingleByChannel 实现了阻挡物的碰撞检测
    const bool bBlockingHit = World->LineTraceSingleByChannel(
        HitResult,
        View.Location,
        TargetPoint,
        RuntimeConfig.VisibilityTraceChannel,
        QueryParams);

    // 没有阻挡，或第一处阻挡就是候选自身，都视为可见。
    return !bBlockingHit || (HitResult.GetActor() == &CandidateActor);
}

bool UWuwaTargetingComponent::BuildHardTargetSwitchOriginScore(const FWuwaTargetingViewSnapshot &View, FWuwaTargetScoreBreakdown &OutScore) const
{
    OutScore = FWuwaTargetScoreBreakdown();

    const AActor *RequesterActor = Requester.Get();

    if (!IsValid(RequesterActor) ||
        TargetContext.Mode != EWuwaTargetingMode::Hard ||
        !TargetContext.HasValidTarget() ||
        !View.IsValid())
    {
        return false;
    }

    const FVector TargetPoint = TargetContext.TargetPoint;

    const bool bFiniteTargetPoint = FMath::IsFinite(TargetPoint.X) &&
                                    FMath::IsFinite(TargetPoint.Y) &&
                                    FMath::IsFinite(TargetPoint.Z);

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

    if (!FMath::IsFinite(ViewAngleDegrees) || !FMath::IsFinite(ViewSpaceHorizontal) || !FMath::IsFinite(ViewSpaceVertical) || !FMath::IsFinite(Distance))
    {
        return false;
    }

    // 当前 Hard Target 是切换方向的权威原点，必须保存其实时几何坐标。
    // 如果只计算归一化分数而遗漏这些字段，规则会错误地以画面中心为原点。
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
        // Hard Context 是当前权威，周期 Soft 刷新不能把它降级。
        Result.bSucceeded = TargetContext.HasValidTarget();
        Result.FailureReason = Result.bSucceeded ? EWuwaTargetingFailureReason::None
                                                 : EWuwaTargetingFailureReason::InvalidCandidate;
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

    for (const FWuwaTargetCandidate &Candidate : Candidates)
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

    if (!IsInitialized())
    {
        LastFailureReason =
            EWuwaTargetingFailureReason::NotInitialized;

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
        // Hard Context 必须始终对应本组件持有的唯一标签来源。
        LastFailureReason = EWuwaTargetingFailureReason::StateCommitFailed;

        Result.FailureReason = LastFailureReason;
        return Result;
    }

    // 输入发生时立即验证当前目标，不能基于上一轮 Timer 的过期目标点切换。
    ValidateCurrentHardTarget();

    if (TargetContext.Mode != EWuwaTargetingMode::Hard || !TargetContext.HasValidTarget())
    {
        Result.Context = TargetContext;
        Result.FailureReason =
            LastFailureReason != EWuwaTargetingFailureReason::None
                ? LastFailureReason
                : EWuwaTargetingFailureReason::NoHardLock;
        return Result;
    }

    AActor *CurrentTarget = TargetContext.TargetActor.Get();

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

    for (const FWuwaTargetCandidate &Candidate : GatheredCandidates)
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
        WuwaTargetingRules::SelectSwitchCandidateIndex(
            SwitchScores,
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

    const FWuwaTargetCandidate &SelectedCandidate = SwitchCandidates[SelectedCandidateIndex];

    AActor *NewTarget = SelectedCandidate.TargetActor.Get();

    if (!IsValid(NewTarget) || NewTarget == CurrentTarget)
    {
        // 目标在切换过程中被销毁或失效，原 Context、标签和销毁绑定完全不变。
        LastFailureReason = EWuwaTargetingFailureReason::InvalidCandidate;

        Result.FailureReason = LastFailureReason;
        Result.Context = TargetContext;
        return Result;
    }

    const FString PreviousTargetName = GetTargetDebugName(CurrentTarget);

    // 新绑定成功前不触碰 Context；失败时旧 Hard Target 仍有完整销毁观察。
    if (!BindHardTargetDestroyed(NewTarget))
    {
        LastFailureReason = EWuwaTargetingFailureReason::StateCommitFailed;

        Result.FailureReason = LastFailureReason;
        Result.Context = TargetContext;
        return Result;
    }

    // HardLock 标签句柄不释放、不重新取得；只原子替换目标事实。
    TargetContext.TargetActor = SelectedCandidate.TargetActor;
    TargetContext.TargetPoint = SelectedCandidate.TargetPoint;
    TargetContext.Score = SelectedCandidate.Score;
    TargetContext.Mode = EWuwaTargetingMode::Hard;
    TargetContext.Revision = AdvanceTargetContextRevision(TargetContext.Revision);

    LastFailureReason = EWuwaTargetingFailureReason::None;

    UE_LOG(
        LogWuwa,
        Log,
        TEXT("Hard Target 已切换。Owner=%s, PreviousTarget=%s, NewTarget=%s, Direction=%.2f, Revision=%d"),
        *GetNameSafe(GetOwner()),
        *PreviousTargetName,
        *GetTargetDebugName(NewTarget),
        Direction,
        TargetContext.Revision);

    OnTargetContextChanged.Broadcast(TargetContext);

    Result.bSucceeded = true;
    Result.FailureReason = EWuwaTargetingFailureReason::None;
    Result.Context = TargetContext;

    return Result;
}

FWuwaTargetingResult UWuwaTargetingComponent::TryEnterHardLock()
{
    FWuwaTargetingResult Result;
    Result.Context = TargetContext;

    if (!IsInitialized())
    {
        LastFailureReason =
            EWuwaTargetingFailureReason::NotInitialized;

        Result.FailureReason = LastFailureReason;
        return Result;
    }

    if (TargetContext.Mode == EWuwaTargetingMode::Hard && TargetContext.HasValidTarget())
    {
        // 重复请求保持幂等，不能重复取得标签句柄。
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

    // 按下锁定时立即刷新，不能依赖上一轮 Timer 的过期 Soft Context
    const FWuwaTargetingResult SoftResult = RefreshSoftTarget();

    if (!SoftResult.bSucceeded || TargetContext.Mode != EWuwaTargetingMode::Soft || !TargetContext.HasValidTarget())
    {
        return SoftResult;
    }

    UWuwaStateTagComponent *Tags = StateTagComponent.Get();

    if (!IsValid(Tags))
    {
        LastFailureReason = EWuwaTargetingFailureReason::NotInitialized;

        Result.FailureReason = LastFailureReason;
        Result.Context = TargetContext;
        return Result;
    }

    AActor *NewHardTarget = TargetContext.TargetActor.Get();

    if (!IsValid(NewHardTarget))
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

    TargetContext.Mode = EWuwaTargetingMode::Hard;

    TargetContext.Revision = AdvanceTargetContextRevision(TargetContext.Revision);

    LastFailureReason = EWuwaTargetingFailureReason::None;

    UE_LOG(
        LogWuwa,
        Log,
        TEXT("Hard Lock 已进入。Owner=%s, Target=%s, Revision=%d"),
        *GetNameSafe(GetOwner()),
        *GetTargetDebugName(TargetContext.TargetActor.Get()),
        TargetContext.Revision);

    OnTargetContextChanged.Broadcast(TargetContext);

    Result.bSucceeded = true;
    Result.FailureReason = EWuwaTargetingFailureReason::None;
    Result.Context = TargetContext;

    return Result;
}

bool UWuwaTargetingComponent::BindHardTargetDestroyed(AActor *TargetActor)
{
    if (!IsValid(TargetActor))
    {
        return false;
    }

    AActor *PreviousTarget = BoundHardTarget.Get();

    if (PreviousTarget == TargetActor)
    {
        // 重复绑定保持幂等，但连续遮挡时间必须归属于当前目标的新验证周期。
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
    if (AActor *TargetActor = BoundHardTarget.Get())
    {
        TargetActor->OnDestroyed.RemoveDynamic(this, &UWuwaTargetingComponent::HandleHardTargetDestroyed);
    }

    BoundHardTarget.Reset();

    // 委托和遮挡时间属于同一个 Hard Target，切换、解锁和重置都必须同步清理。
    HardTargetOcclusionStartTimeSeconds = -1.0;
}

void UWuwaTargetingComponent::HandleHardTargetDestroyed(AActor *DestroyedActor)
{
    if (TargetContext.Mode != EWuwaTargetingMode::Hard)
    {
        return;
    }

    UE_LOG(
        LogWuwa,
        Log,
        TEXT("Hard Target 已销毁，开始异常清理。Owner=%s, Target=%s"),
        *GetNameSafe(GetOwner()),
        *GetTargetDebugName(DestroyedActor));

    // 该委托只绑定当前 Hard Target，因此回调可以直接解除当前 Hard 状态。
    const FWuwaTargetingResult ClearResult = ClearHardLock();

    if (!ClearResult.bSucceeded)
    {
        UE_LOG(
            LogWuwa,
            Error,
            TEXT("Hard Target 销毁后的清理失败。Owner=%s, Reason=%s"),
            *GetNameSafe(GetOwner()),
            *UEnum::GetValueAsString(
                ClearResult.FailureReason));
    }
}

void UWuwaTargetingComponent::ValidateCurrentHardTarget()
{
    if (TargetContext.Mode != EWuwaTargetingMode::Hard)
    {
        return;
    }

    AActor *RequesterActor = Requester.Get();
    AActor *TargetActor = TargetContext.TargetActor.Get();

    if (!IsValid(RequesterActor) || !IsValid(TargetActor))
    {
        ExitHardLockForValidationFailure(
            EWuwaTargetingFailureReason::InvalidCandidate);
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

    // FVector 在当前引擎版本没有可直接使用的 IsFinite 成员，因此逐分量检查。
    const bool bFiniteTargetPoint = FMath::IsFinite(TargetPoint.X) && FMath::IsFinite(TargetPoint.Y) &&
                                    FMath::IsFinite(TargetPoint.Z);

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

    UWorld *World = GetWorld();

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
            TEXT("Hard Target 验证失败，但 Hard 状态清理失败。Owner=%s, Target=%s, ValidationReason=%s, ClearReason=%s"),
            *GetNameSafe(GetOwner()),
            *PreviousTargetName,
            *UEnum::GetValueAsString(FailureReason),
            *UEnum::GetValueAsString(ClearResult.FailureReason));
        return;
    }

    // 清理成功后保留触发解除的领域原因，方便定位超距、资格或遮挡问题。
    LastFailureReason = FailureReason;

    UE_LOG(
        LogWuwa,
        Log,
        TEXT("Hard Target 验证失败，已安全解除。Owner=%s, Target=%s, Reason=%s"),
        *GetNameSafe(GetOwner()),
        *PreviousTargetName,
        *UEnum::GetValueAsString(FailureReason));
}

bool UWuwaTargetingComponent::ReleaseHardLockTag()
{
    if (!HardLockTagHandle.IsValid())
    {
        return true;
    }

    UWuwaStateTagComponent *Tags = StateTagComponent.Get();

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

    const FString PreviousTargetName =
        GetTargetDebugName(
            TargetContext.TargetActor.Get());

    const int32 PreviousRevision =
        TargetContext.Revision;

    // 暂时切到未发布的 None，允许 RefreshSoftTarget 选择最终 Soft。
    // 此处不广播，外部消费者不能观察到中间态。
    TargetContext = FWuwaTargetContext();
    TargetContext.Revision = PreviousRevision;

    const FWuwaTargetingResult SoftResult =
        RefreshSoftTarget();

    if (SoftResult.bSucceeded)
    {
        // CommitSoftTarget 已把 Revision 增加一次并广播最终 Soft Context。
        LastFailureReason =
            EWuwaTargetingFailureReason::None;

        UE_LOG(
            LogWuwa,
            Log,
            TEXT("Hard Lock 已清除并恢复 Soft。Owner=%s, PreviousTarget=%s, NewTarget=%s, Revision=%d"),
            *GetNameSafe(GetOwner()),
            *PreviousTargetName,
            *GetTargetDebugName(TargetContext.TargetActor.Get()),
            TargetContext.Revision);

        Result.bSucceeded = true;
        Result.FailureReason =
            EWuwaTargetingFailureReason::None;
        Result.Context = TargetContext;
        return Result;
    }

    // 没有可恢复的 Soft Target 时，提交唯一一次 Hard→None 语义变化。
    TargetContext = FWuwaTargetContext();
    TargetContext.Revision =
        AdvanceTargetContextRevision(
            PreviousRevision);

    LastFailureReason =
        EWuwaTargetingFailureReason::None;

    UE_LOG(
        LogWuwa,
        Log,
        TEXT("Hard Lock 已清除且当前无 Soft Target。Owner=%s, PreviousTarget=%s, Revision=%d"),
        *GetNameSafe(GetOwner()),
        *PreviousTargetName,
        TargetContext.Revision);

    OnTargetContextChanged.Broadcast(TargetContext);

    Result.bSucceeded = true;
    Result.FailureReason =
        EWuwaTargetingFailureReason::None;
    Result.Context = TargetContext;

    return Result;
}

void UWuwaTargetingComponent::CommitSoftTarget(const FWuwaTargetCandidate &Candidate)
{
    if (!Candidate.IsValid())
    {
        return;
    }

    const AActor *PreviousTarget = TargetContext.TargetActor.Get();

    const bool bSemanticChange = TargetContext.Mode != EWuwaTargetingMode::Soft || PreviousTarget != Candidate.TargetActor.Get();

    const int32 PreviousRevision = TargetContext.Revision;

    TargetContext.TargetActor = Candidate.TargetActor;
    TargetContext.TargetPoint = Candidate.TargetPoint;
    TargetContext.Mode = EWuwaTargetingMode::Soft;
    TargetContext.Score = Candidate.Score;

    TargetContext.Revision = bSemanticChange ? AdvanceTargetContextRevision(PreviousRevision) : PreviousRevision;

    if (!bSemanticChange)
    {
        // // 同一目标只更新实时点和评分，不刷 Revision 或事件。
        return;
    }

    UE_LOG(
        LogWuwa,
        Log,
        TEXT("Soft Target 已更新。Owner=%s, Target=%s, Score=%.3f, Revision=%d"),
        *GetNameSafe(GetOwner()),
        *GetTargetDebugName(TargetContext.TargetActor.Get()),
        TargetContext.Score.TotalScore,
        TargetContext.Revision);

    // 目标或模式发生语义变化时，广播事件通知订阅者。
    OnTargetContextChanged.Broadcast(TargetContext);
}

void UWuwaTargetingComponent::ClearSoftTarget()
{
    if (TargetContext.Mode != EWuwaTargetingMode::Soft)
    {
        return;
    }

    const FString PreviousTargetName = GetNameSafe(TargetContext.TargetActor.Get());

    const int32 NextRevision = AdvanceTargetContextRevision(TargetContext.Revision);

    TargetContext = FWuwaTargetContext();
    TargetContext.Revision = NextRevision;

    // 清理原因由 RefreshSoftTarget 写入 LastFailureReason；
    // Context 清理只负责释放目标事实并通知消费者。
    OnTargetContextChanged.Broadcast(TargetContext);
}

void UWuwaTargetingComponent::StartCandidateRefreshTimer()
{
    // 防止重复初始化创建多个循环 Timer。
    ClearCandidateRefreshTimer();

    UWorld *World = GetWorld();

    if (!IsInitialized() ||
        !IsValid(World) ||
        !FMath::IsFinite(RuntimeConfig.CandidateRefreshInterval) ||
        RuntimeConfig.CandidateRefreshInterval <= 0.f)
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        CandidateRefreshTimerHandle,
        this,
        &UWuwaTargetingComponent::HandleCandidateRefreshTimer,
        RuntimeConfig.CandidateRefreshInterval,
        true,
        RuntimeConfig.CandidateRefreshInterval);
}

void UWuwaTargetingComponent::ClearCandidateRefreshTimer()
{
    if (UWorld *World = GetWorld())
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

    // Hard Context 是当前权威，周期 Soft 刷新不能把它降级。
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
            UE_LOG(
                LogWuwa,
                Error,
                TEXT("Targeting 重置时释放 HardLock 标签失败。Owner=%s"),
                *GetNameSafe(GetOwner()));

            HardLockTagHandle.Reset();
        }
    }

    // 运行态必须由权威组件统一清理。
    bInitialized = false;
    Requester.Reset();
    StateTagComponent = nullptr;
    RuntimeConfig = FWuwaTargetingRuntimeConfig();
    TargetContext = FWuwaTargetContext();

    LastFailureReason = EWuwaTargetingFailureReason::NotInitialized;
}

void UWuwaTargetingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Owner 退出时先释放 Targeting 自己拥有的状态，再进入父类销毁流程
    ResetRuntimeState();

    Super::EndPlay(EndPlayReason);
}