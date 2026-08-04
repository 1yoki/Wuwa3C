#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Targeting/WuwaTargetingProfile.h"
#include "Targeting/WuwaTargetingTypes.h"
#include "UObject/UObjectGlobals.h"

#include <limits>

namespace
{
    FWuwaTargetScoreBreakdown MakeSwitchScore(
        const float Horizontal,
        const float Vertical,
        const float DistanceScore,
        const float TotalScore)
    {
        FWuwaTargetScoreBreakdown Score;
        Score.Distance = (1.f - DistanceScore) * 1000.f;
        Score.ViewAngleDegrees = FMath::Abs(Horizontal) * 30.f;
        Score.ViewSpaceHorizontal = Horizontal;
        Score.ViewSpaceVertical = Vertical;
        Score.DistanceScore = DistanceScore;
        Score.ViewAlignmentScore = 1.f;
        Score.TotalScore = TotalScore;
        Score.bVisible = true;
        return Score;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWuwaTargetingScoreAndSwitchTest,
    "Wuwa.Targeting.Rules.ScoreAndSwitch",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FWuwaTargetingScoreAndSwitchTest::RunTest(const FString &Parameters)
{
    (void)Parameters;

    UWuwaTargetingProfile *Profile = NewObject<UWuwaTargetingProfile>();
    TestNotNull(TEXT("默认 Targeting Profile 应可创建"), Profile);
    TestTrue(TEXT("默认 Targeting Profile 应通过运行时校验"), Profile && Profile->IsRuntimeValid());

    if (Profile)
    {
        Profile->HardLockReleaseDistance = Profile->SearchRadius - 1.f;
        TestFalse(TEXT("硬锁解除距离小于搜索半径时 Profile 应无效"), Profile->IsRuntimeValid());

        Profile->HardLockReleaseDistance = Profile->SearchRadius;
        Profile->DistanceWeight = 0.f;
        Profile->ViewAlignmentWeight = 0.f;
        TestFalse(TEXT("距离与视角权重同时为零时 Profile 应无效"), Profile->IsRuntimeValid());
    }

    FWuwaTargetScoreInput BestInput;
    BestInput.SearchRadius = 1000.f;
    BestInput.MaxAcquireAngleDegrees = 60.f;
    BestInput.DistanceWeight = 0.4f;
    BestInput.ViewAlignmentWeight = 0.6f;
    BestInput.RetentionBonus = 0.2f;

    FWuwaTargetScoreBreakdown BestScore;
    TestTrue(TEXT("零距离且正对镜头的候选应成功评分"), WuwaTargetingRules::CalculateScore(BestInput, BestScore));
    TestTrue(TEXT("零距离的距离分应为 1"), FMath::IsNearlyEqual(BestScore.DistanceScore, 1.f));
    TestTrue(TEXT("零视角的对齐分应为 1"), FMath::IsNearlyEqual(BestScore.ViewAlignmentScore, 1.f));
    TestTrue(TEXT("TotalScore 应包含保持加成"), FMath::IsNearlyEqual(BestScore.TotalScore, 1.2f));

    FWuwaTargetScoreInput BoundaryInput = BestInput;
    BoundaryInput.Distance = BoundaryInput.SearchRadius;
    BoundaryInput.ViewAngleDegrees = BoundaryInput.MaxAcquireAngleDegrees;
    BoundaryInput.RetentionBonus = 0.f;

    FWuwaTargetScoreBreakdown BoundaryScore;
    TestTrue(TEXT("搜索半径和获取视角边界应允许评分"), WuwaTargetingRules::CalculateScore(BoundaryInput, BoundaryScore));
    TestTrue(TEXT("搜索半径边界的距离分应为 0"), FMath::IsNearlyZero(BoundaryScore.DistanceScore));
    TestTrue(TEXT("获取视角边界的对齐分应为 0"), FMath::IsNearlyZero(BoundaryScore.ViewAlignmentScore));

    FWuwaTargetScoreInput NoRetentionInput = BestInput;
    NoRetentionInput.RetentionBonus = 0.f;
    FWuwaTargetScoreBreakdown NoRetentionScore;
    TestTrue(TEXT("无保持加成评分应成功"), WuwaTargetingRules::CalculateScore(NoRetentionInput, NoRetentionScore));
    TestTrue(
        TEXT("当前软目标应只增加配置的保持分"),
        FMath::IsNearlyEqual(
            BestScore.TotalScore,
            NoRetentionScore.TotalScore + BestInput.RetentionBonus,
            UE_KINDA_SMALL_NUMBER));

    FWuwaTargetScoreInput OutOfRangeInput = BestInput;
    OutOfRangeInput.Distance = OutOfRangeInput.SearchRadius + 1.f;
    FWuwaTargetScoreBreakdown RejectedScore;
    TestFalse(TEXT("超出搜索半径的候选应拒绝评分"), WuwaTargetingRules::CalculateScore(OutOfRangeInput, RejectedScore));

    FWuwaTargetScoreInput NonFiniteInput = BestInput;
    NonFiniteInput.ViewSpaceHorizontal = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("非有限视空间坐标应拒绝评分"), WuwaTargetingRules::CalculateScore(NonFiniteInput, RejectedScore));

    TArray<FWuwaTargetScoreBreakdown> RankedScores;
    RankedScores.Add(MakeSwitchScore(0.f, 0.f, 0.5f, 0.4f));
    RankedScores.Add(MakeSwitchScore(0.2f, 0.f, 0.5f, 0.9f));
    RankedScores.Add(MakeSwitchScore(0.3f, 0.f, 0.5f, 0.9f));
    RankedScores[1].ViewAngleDegrees = 20.f;
    RankedScores[2].ViewAngleDegrees = 10.f;

    FWuwaTargetScoreBreakdown InvisibleHighScore = MakeSwitchScore(0.f, 0.f, 1.f, 10.f);
    InvisibleHighScore.bVisible = false;
    RankedScores.Add(InvisibleHighScore);

    TestEqual(TEXT("选择应跳过不可见候选，并在总分相同时优先更靠近镜头中心的候选"),
              WuwaTargetingRules::SelectBestCandidateIndex(RankedScores),
              2);

    TArray<FWuwaTargetScoreBreakdown> SwitchScores;
    SwitchScores.Add(MakeSwitchScore(0.f, 0.f, 0.5f, 0.5f));      // Current
    SwitchScores.Add(MakeSwitchScore(-0.25f, 0.02f, 0.6f, 0.7f)); // Left
    SwitchScores.Add(MakeSwitchScore(0.20f, 0.03f, 0.6f, 0.8f));  // Near Right
    SwitchScores.Add(MakeSwitchScore(0.65f, 0.f, 0.8f, 1.0f));    // Far Right

    const int32 LeftIndex = WuwaTargetingRules::SelectSwitchCandidateIndex(
        SwitchScores, 0, -1.f, 0.05f, 0.5f, 0.25f);
    TestEqual(TEXT("向左切换应选择当前目标左侧候选"), LeftIndex, 1);

    const int32 RightIndex = WuwaTargetingRules::SelectSwitchCandidateIndex(
        SwitchScores, 0, 1.f, 0.05f, 0.5f, 0.25f);
    TestEqual(TEXT("向右切换应选择方向成本更低的近侧候选"), RightIndex, 2);
    TestTrue(TEXT("切换规则不能再次选择当前目标"), RightIndex != 0 && LeftIndex != 0);

    TestEqual(TEXT("零方向应明确返回无候选"),
              WuwaTargetingRules::SelectSwitchCandidateIndex(SwitchScores, 0, 0.f, 0.05f, 0.5f, 0.25f),
              INDEX_NONE);

    TestEqual(TEXT("方向上没有满足最小水平差的候选时应失败"),
              WuwaTargetingRules::SelectSwitchCandidateIndex(SwitchScores, 0, 1.f, 1.f, 0.5f, 0.25f),
              INDEX_NONE);

    TArray<FWuwaTargetScoreBreakdown> OverlapScores;
    OverlapScores.Add(MakeSwitchScore(0.00f, 0.00f, 0.5f, 0.5f));  // Current，Distance=500
    OverlapScores.Add(MakeSwitchScore(0.01f, 0.02f, 0.7f, 0.8f));  // 更近两层，Distance=300
    OverlapScores.Add(MakeSwitchScore(-0.01f, 0.01f, 0.6f, 0.7f)); // 相邻更近层，Distance=400
    OverlapScores.Add(MakeSwitchScore(0.02f, 0.01f, 0.4f, 0.6f));  // 相邻更远层，Distance=600
    OverlapScores.Add(MakeSwitchScore(-0.02f, 0.02f, 0.2f, 0.4f)); // 更远两层，Distance=800

    TestEqual(
        TEXT("没有左侧候选时，Q 应在水平重叠目标中选择相邻更近层"),
        WuwaTargetingRules::SelectSwitchCandidateIndex(OverlapScores, 0, -1.f, 0.05f, 0.5f, 0.25f),
        2);

    TestEqual(
        TEXT("没有右侧候选时，E 应在水平重叠目标中选择相邻更远层"),
        WuwaTargetingRules::SelectSwitchCandidateIndex(OverlapScores, 0, 1.f, 0.05f, 0.5f, 0.25f),
        3);

    TArray<FWuwaTargetScoreBreakdown> PrimaryDirectionScores = OverlapScores;
    PrimaryDirectionScores.Add(MakeSwitchScore(0.20f, 0.0f, 0.9f, 1.0f));
    TestEqual(
        TEXT("存在明确右侧候选时必须优先左右切换，不得进入重叠深度降级"),
        WuwaTargetingRules::SelectSwitchCandidateIndex(PrimaryDirectionScores, 0, 1.f, 0.05f, 0.5f, 0.25f),
        5);

    TestEqual(
        TEXT("重叠层级已经最近时，Q 不应反向回绕"),
        WuwaTargetingRules::SelectSwitchCandidateIndex(OverlapScores, 1, -1.f, 0.05f, 0.5f, 0.25f),
        INDEX_NONE);

    TestEqual(
        TEXT("重叠层级已经最远时，E 不应反向回绕"),
        WuwaTargetingRules::SelectSwitchCandidateIndex(OverlapScores, 4, 1.f, 0.05f, 0.5f, 0.25f),
        INDEX_NONE);

    TArray<FWuwaTargetScoreBreakdown> CurrentOnlyScores;
    CurrentOnlyScores.Add(SwitchScores[0]);
    TestEqual(TEXT("只有当前目标、没有其他候选时切换应失败"),
              WuwaTargetingRules::SelectSwitchCandidateIndex(CurrentOnlyScores, 0, 1.f, 0.05f, 0.5f, 0.25f),
              INDEX_NONE);

    return true;
}

#endif
