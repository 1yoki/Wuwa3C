#include "Targeting/WuwaTargetingTypes.h"

bool FWuwaTargetScoreBreakdown::IsFinite() const
{
    return FMath::IsFinite(Distance) && Distance >= 0.f &&
           FMath::IsFinite(ViewAngleDegrees) && ViewAngleDegrees >= 0.f &&
           FMath::IsFinite(ViewSpaceHorizontal) &&
           FMath::IsFinite(ViewSpaceVertical) &&
           FMath::IsFinite(DistanceScore) && DistanceScore >= 0.f && DistanceScore <= 1.f &&
           FMath::IsFinite(ViewAlignmentScore) && ViewAlignmentScore >= 0.f && ViewAlignmentScore <= 1.f &&
           FMath::IsFinite(RetentionBonus) && RetentionBonus >= 0.f &&
           FMath::IsFinite(TotalScore) && TotalScore >= 0.f;
}

bool FWuwaTargetCandidate::IsValid() const
{
    return TargetActor.IsValid() && !TargetPoint.ContainsNaN() && Score.IsFinite() && Score.bVisible;
}

bool FWuwaTargetContext::HasValidTarget() const
{
    return Mode != EWuwaTargetingMode::None && TargetActor.IsValid() && !TargetPoint.ContainsNaN();
}

bool WuwaTargetingRules::CalculateScore(const FWuwaTargetScoreInput &Input, FWuwaTargetScoreBreakdown &OutScore)
{
    // 失败时清空输出，调用者不能误用上一次候选的评分。
    OutScore = FWuwaTargetScoreBreakdown();

    const bool bFiniteInput =
        FMath::IsFinite(Input.Distance) &&
        FMath::IsFinite(Input.SearchRadius) &&
        FMath::IsFinite(Input.ViewAngleDegrees) &&
        FMath::IsFinite(Input.MaxAcquireAngleDegrees) &&
        FMath::IsFinite(Input.ViewSpaceHorizontal) &&
        FMath::IsFinite(Input.ViewSpaceVertical) &&
        FMath::IsFinite(Input.DistanceWeight) &&
        FMath::IsFinite(Input.ViewAlignmentWeight) &&
        FMath::IsFinite(Input.RetentionBonus);

    const float TotalWeight = Input.DistanceWeight + Input.ViewAlignmentWeight;

    const bool bValidRange =
        Input.Distance >= 0.f && Input.SearchRadius > 0.f && Input.Distance <= Input.SearchRadius &&
        Input.ViewAngleDegrees >= 0.f && Input.MaxAcquireAngleDegrees > 0.f &&
        Input.ViewAngleDegrees <= Input.MaxAcquireAngleDegrees &&
        Input.DistanceWeight >= 0.f && Input.ViewAlignmentWeight >= 0.f &&
        FMath::IsFinite(TotalWeight) && TotalWeight > 0.f &&
        Input.RetentionBonus >= 0.f;

    if (!bFiniteInput || !bValidRange)
    {
        return false;
    }

    OutScore.Distance = Input.Distance;
    OutScore.ViewAngleDegrees = Input.ViewAngleDegrees;
    OutScore.ViewSpaceHorizontal = Input.ViewSpaceHorizontal;
    OutScore.ViewSpaceVertical = Input.ViewSpaceVertical;
    OutScore.DistanceScore = 1.f - FMath::Clamp(Input.Distance / Input.SearchRadius, 0.f, 1.f);
    OutScore.ViewAlignmentScore = 1.f - FMath::Clamp(Input.ViewAngleDegrees / Input.MaxAcquireAngleDegrees, 0.f, 1.f);
    OutScore.RetentionBonus = Input.RetentionBonus;
    OutScore.TotalScore = Input.DistanceWeight * OutScore.DistanceScore +
                          Input.ViewAlignmentWeight * OutScore.ViewAlignmentScore +
                          Input.RetentionBonus;
    // CalculateScore 只接受已经通过可见性过滤的候选。
    OutScore.bVisible = true;

    return OutScore.IsFinite();
}

int32 WuwaTargetingRules::SelectBestCandidateIndex(const TArray<FWuwaTargetScoreBreakdown> &CandidateScores)
{
    int32 BestIndex = INDEX_NONE;

    for (int32 Index = 0; Index < CandidateScores.Num(); ++Index)
    {
        const FWuwaTargetScoreBreakdown &Candidate = CandidateScores[Index];

        if (!Candidate.IsFinite() || !Candidate.bVisible)
        {
            continue;
        }

        if (BestIndex == INDEX_NONE)
        {
            BestIndex = Index;
            continue;
        }

        const FWuwaTargetScoreBreakdown &Best = CandidateScores[BestIndex];

        const bool bHigherTotal = Candidate.TotalScore > Best.TotalScore + UE_KINDA_SMALL_NUMBER;
        const bool bEqualTotal = FMath::IsNearlyEqual(Candidate.TotalScore, Best.TotalScore, UE_KINDA_SMALL_NUMBER);
        const bool bBetterAngle = bEqualTotal && Candidate.ViewAngleDegrees < Best.ViewAngleDegrees - UE_KINDA_SMALL_NUMBER;
        const bool bEqualAngle = FMath::IsNearlyEqual(Candidate.ViewAngleDegrees, Best.ViewAngleDegrees, UE_KINDA_SMALL_NUMBER);
        const bool bNearer = bEqualTotal && bEqualAngle && Candidate.Distance < Best.Distance - UE_KINDA_SMALL_NUMBER;

        if (bHigherTotal || bBetterAngle || bNearer)
        {
            BestIndex = Index;
        }
    }

    return BestIndex;
}

int32 WuwaTargetingRules::SelectSwitchCandidateIndex(
    const TArray<FWuwaTargetScoreBreakdown> &CandidateScores,
    const int32 CurrentCandidateIndex,
    const float Direction,
    const float MinimumHorizontalDelta,
    const float VerticalPenaltyWeight,
    const float DistancePenaltyWeight)
{
    const bool bValidParameters =
        CandidateScores.IsValidIndex(CurrentCandidateIndex) &&
        FMath::IsFinite(Direction) && !FMath::IsNearlyZero(Direction) &&
        FMath::IsFinite(MinimumHorizontalDelta) && MinimumHorizontalDelta >= 0.f &&
        FMath::IsFinite(VerticalPenaltyWeight) && VerticalPenaltyWeight >= 0.f &&
        FMath::IsFinite(DistancePenaltyWeight) && DistancePenaltyWeight >= 0.f;

    if (!bValidParameters)
    {
        return INDEX_NONE;
    }

    const FWuwaTargetScoreBreakdown &Current = CandidateScores[CurrentCandidateIndex];

    if (!Current.IsFinite())
    {
        return INDEX_NONE;
    }

    const float DirectionSign = Direction > 0.f ? 1.f : -1.f;
    // 左右选择与重叠降级必须共享完全相同的水平阈值。
    const float RequiredHorizontalDelta = FMath::Max(MinimumHorizontalDelta, UE_KINDA_SMALL_NUMBER);
    int32 BestIndex = INDEX_NONE;
    float BestCost = MAX_flt;

    for (int32 Index = 0; Index < CandidateScores.Num(); ++Index)
    {
        if (Index == CurrentCandidateIndex)
        {
            // 当前硬目标只能作为切换原点，不能再次被选中。
            continue;
        }

        const FWuwaTargetScoreBreakdown &Candidate = CandidateScores[Index];

        if (!Candidate.IsFinite() || !Candidate.bVisible)
        {
            continue;
        }

        const float DirectionalHorizontalDelta =
            (Candidate.ViewSpaceHorizontal - Current.ViewSpaceHorizontal) * DirectionSign;

        if (!FMath::IsFinite(DirectionalHorizontalDelta) || DirectionalHorizontalDelta < RequiredHorizontalDelta)
        {
            continue;
        }

        const float VerticalDelta = FMath::Abs(Candidate.ViewSpaceVertical - Current.ViewSpaceVertical);
        const float NormalizedDistanceDelta = FMath::Abs(Candidate.DistanceScore - Current.DistanceScore);
        const float SwitchCost = DirectionalHorizontalDelta +
                                 VerticalPenaltyWeight * VerticalDelta +
                                 DistancePenaltyWeight * NormalizedDistanceDelta;

        if (!FMath::IsFinite(SwitchCost))
        {
            continue;
        }

        const bool bLowerCost = SwitchCost < BestCost - UE_KINDA_SMALL_NUMBER;
        const bool bEqualCost = FMath::IsNearlyEqual(SwitchCost, BestCost, UE_KINDA_SMALL_NUMBER);
        const bool bHigherTargetScore = bEqualCost &&
                                        (BestIndex == INDEX_NONE || Candidate.TotalScore > CandidateScores[BestIndex].TotalScore + UE_KINDA_SMALL_NUMBER);

        if (BestIndex == INDEX_NONE || bLowerCost || bHigherTargetScore)
        {
            BestIndex = Index;
            BestCost = SwitchCost;
        }
    }

    if (BestIndex != INDEX_NONE)
    {
        // 只要存在明确的画面左/右候选，就保持原有空间切换语义。
        return BestIndex;
    }

    // 画面水平重叠时没有可靠的左/右关系。
    // 只有第一阶段完全失败后，才把同一水平层的目标按角色距离解释为深度层级：
    // Direction < 0（Q）选择相邻的更近目标，Direction > 0（E）选择相邻的更远目标。
    int32 BestOverlapIndex = INDEX_NONE;
    float BestDirectionalDistanceDelta = MAX_flt;
    float BestOverlapVerticalDelta = MAX_flt;

    for (int32 Index = 0; Index < CandidateScores.Num(); ++Index)
    {
        if (Index == CurrentCandidateIndex)
        {
            continue;
        }

        const FWuwaTargetScoreBreakdown &Candidate = CandidateScores[Index];

        if (!Candidate.IsFinite() || !Candidate.bVisible)
        {
            continue;
        }

        const float HorizontalDelta = Candidate.ViewSpaceHorizontal - Current.ViewSpaceHorizontal;

        if (!FMath::IsFinite(HorizontalDelta) || FMath::Abs(HorizontalDelta) >= RequiredHorizontalDelta)
        {
            // 这里仅处理无法形成明确左右关系的水平重叠候选。
            continue;
        }

        const float DirectionalDistanceDelta = (Candidate.Distance - Current.Distance) * DirectionSign;

        if (!FMath::IsFinite(DirectionalDistanceDelta) || DirectionalDistanceDelta < UE_KINDA_SMALL_NUMBER)
        {
            // Q 只接受更近层，E 只接受更远层；不反向回绕。
            continue;
        }

        const float VerticalDelta = FMath::Abs(Candidate.ViewSpaceVertical - Current.ViewSpaceVertical);
        const bool bAdjacentDepth = DirectionalDistanceDelta < BestDirectionalDistanceDelta - UE_KINDA_SMALL_NUMBER;
        const bool bEqualDepth = FMath::IsNearlyEqual(
            DirectionalDistanceDelta,
            BestDirectionalDistanceDelta,
            UE_KINDA_SMALL_NUMBER);
        const bool bSmallerVerticalDelta = bEqualDepth &&
                                           VerticalDelta < BestOverlapVerticalDelta - UE_KINDA_SMALL_NUMBER;
        const bool bEqualVerticalDelta = bEqualDepth &&
                                         FMath::IsNearlyEqual(
                                             VerticalDelta,
                                             BestOverlapVerticalDelta,
                                             UE_KINDA_SMALL_NUMBER);
        const bool bHigherTargetScore = bEqualVerticalDelta &&
                                        (BestOverlapIndex == INDEX_NONE ||
                                         Candidate.TotalScore > CandidateScores[BestOverlapIndex].TotalScore + UE_KINDA_SMALL_NUMBER);

        if (BestOverlapIndex == INDEX_NONE || bAdjacentDepth || bSmallerVerticalDelta || bHigherTargetScore)
        {
            BestOverlapIndex = Index;
            BestDirectionalDistanceDelta = DirectionalDistanceDelta;
            BestOverlapVerticalDelta = VerticalDelta;
        }
    }

    return BestOverlapIndex;
}
