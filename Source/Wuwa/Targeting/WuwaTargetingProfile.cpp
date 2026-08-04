#include "Targeting/WuwaTargetingProfile.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UWuwaTargetingProfile::UWuwaTargetingProfile()
{
    CandidateObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
    CandidateObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
}

bool UWuwaTargetingProfile::IsRuntimeValid() const
{
    if (CandidateObjectTypes.IsEmpty())
    {
        return false;
    }

    for (const TEnumAsByte<EObjectTypeQuery> ObjectType : CandidateObjectTypes)
    {
        if (static_cast<int32>(ObjectType.GetValue()) >= static_cast<int32>(ObjectTypeQuery_MAX))
        {
            return false;
        }
    }

    const bool bValidRanges =
        FMath::IsFinite(SearchRadius) && SearchRadius > 0.f &&
        FMath::IsFinite(MaxAcquireAngleDegrees) && MaxAcquireAngleDegrees > 0.f && MaxAcquireAngleDegrees <= 180.f &&
        FMath::IsFinite(HardLockReleaseDistance) && HardLockReleaseDistance >= SearchRadius &&
        FMath::IsFinite(OcclusionGraceTime) && OcclusionGraceTime >= 0.f &&
        FMath::IsFinite(CandidateRefreshInterval) && CandidateRefreshInterval > 0.f;

    const float TotalScoreWeight = DistanceWeight + ViewAlignmentWeight;
    const bool bValidScore =
        FMath::IsFinite(DistanceWeight) && DistanceWeight >= 0.f &&
        FMath::IsFinite(ViewAlignmentWeight) && ViewAlignmentWeight >= 0.f &&
        FMath::IsFinite(TotalScoreWeight) && TotalScoreWeight > 0.f &&
        FMath::IsFinite(CurrentSoftTargetBonus) && CurrentSoftTargetBonus >= 0.f;

    const bool bValidSwitch =
        FMath::IsFinite(SwitchMinimumHorizontalDelta) && SwitchMinimumHorizontalDelta >= 0.f &&
        FMath::IsFinite(SwitchVerticalPenaltyWeight) && SwitchVerticalPenaltyWeight >= 0.f &&
        FMath::IsFinite(SwitchDistancePenaltyWeight) && SwitchDistancePenaltyWeight >= 0.f;

    const ECollisionChannel TraceChannel = VisibilityTraceChannel.GetValue();
    const bool bValidTraceChannel = TraceChannel >= ECC_WorldStatic && TraceChannel < ECC_MAX;

    return bValidRanges && bValidScore && bValidSwitch && bValidTraceChannel;
}

#if WITH_EDITOR

EDataValidationResult UWuwaTargetingProfile::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    if (Result != EDataValidationResult::Invalid)
    {
        Result = EDataValidationResult::Valid;
    }

    auto Check = [&Context, &Result](const bool bCondition, const TCHAR *Message)
    {
        if (!bCondition)
        {
            Context.AddError(FText::FromString(Message));
            Result = EDataValidationResult::Invalid;
        }
    };

    Check(!CandidateObjectTypes.IsEmpty(), TEXT("CandidateObjectTypes 至少需要一个对象类型"));

    for (const TEnumAsByte<EObjectTypeQuery> ObjectType : CandidateObjectTypes)
    {
        Check(static_cast<int32>(ObjectType.GetValue()) < static_cast<int32>(ObjectTypeQuery_MAX),
              TEXT("CandidateObjectTypes 包含无效对象类型"));
    }

    Check(FMath::IsFinite(SearchRadius) && SearchRadius > 0.f, TEXT("SearchRadius 必须是有限正数"));
    Check(FMath::IsFinite(MaxAcquireAngleDegrees) && MaxAcquireAngleDegrees > 0.f && MaxAcquireAngleDegrees <= 180.f,
          TEXT("MaxAcquireAngleDegrees 必须在 (0, 180] 范围内"));
    Check(FMath::IsFinite(HardLockReleaseDistance) && HardLockReleaseDistance >= SearchRadius,
          TEXT("HardLockReleaseDistance 必须是有限值且不能小于 SearchRadius"));
    Check(FMath::IsFinite(OcclusionGraceTime) && OcclusionGraceTime >= 0.f,
          TEXT("OcclusionGraceTime 必须是有限非负数"));
    Check(FMath::IsFinite(CandidateRefreshInterval) && CandidateRefreshInterval > 0.f,
          TEXT("CandidateRefreshInterval 必须是有限正数"));

    Check(FMath::IsFinite(DistanceWeight) && DistanceWeight >= 0.f,
          TEXT("DistanceWeight 必须是有限非负数"));
    Check(FMath::IsFinite(ViewAlignmentWeight) && ViewAlignmentWeight >= 0.f,
          TEXT("ViewAlignmentWeight 必须是有限非负数"));
    Check(FMath::IsFinite(DistanceWeight + ViewAlignmentWeight) && DistanceWeight + ViewAlignmentWeight > 0.f,
          TEXT("DistanceWeight 与 ViewAlignmentWeight 不能同时为 0"));
    Check(FMath::IsFinite(CurrentSoftTargetBonus) && CurrentSoftTargetBonus >= 0.f,
          TEXT("CurrentSoftTargetBonus 必须是有限非负数"));

    Check(FMath::IsFinite(SwitchMinimumHorizontalDelta) && SwitchMinimumHorizontalDelta >= 0.f,
          TEXT("SwitchMinimumHorizontalDelta 必须是有限非负数"));
    Check(FMath::IsFinite(SwitchVerticalPenaltyWeight) && SwitchVerticalPenaltyWeight >= 0.f,
          TEXT("SwitchVerticalPenaltyWeight 必须是有限非负数"));
    Check(FMath::IsFinite(SwitchDistancePenaltyWeight) && SwitchDistancePenaltyWeight >= 0.f,
          TEXT("SwitchDistancePenaltyWeight 必须是有限非负数"));

    const ECollisionChannel TraceChannel = VisibilityTraceChannel.GetValue();
    Check(TraceChannel >= ECC_WorldStatic && TraceChannel < ECC_MAX,
          TEXT("VisibilityTraceChannel 必须是有效碰撞通道"));

    return Result;
}

#endif
