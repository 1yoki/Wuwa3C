#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Camera/WuwaCameraFramingRules.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWuwaCameraFramingCombatAxisTest,
    "Wuwa.Camera.Framing.CombatAxis",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FWuwaCameraFramingCombatAxisTest::RunTest(
    const FString &Parameters)
{
    (void)Parameters;

    float CombatAxisYaw = 0.0f;

    TestTrue(
        TEXT("水平战斗轴应忽略玩家与目标的高度差"),
        WuwaCameraFramingRules::CalculateHorizontalCombatAxisYaw(
            FVector(100.0, 50.0, -500.0),
            FVector(100.0, 250.0, 1500.0),
            CombatAxisYaw));

    TestTrue(
        TEXT("正 Y 战斗轴的 Yaw 应为 90 度"),
        FMath::IsNearlyEqual(CombatAxisYaw, 90.0f));

    TestTrue(
        TEXT("负 X 战斗轴应通过规则校验"),
        WuwaCameraFramingRules::CalculateHorizontalCombatAxisYaw(
            FVector::ZeroVector,
            FVector(-100.0, 0.0, 500.0),
            CombatAxisYaw));

    TestTrue(
        TEXT("负 X 战斗轴应正确处理正负 180 度边界"),
        FMath::IsNearlyEqual(FMath::Abs(CombatAxisYaw), 180.0f));

    CombatAxisYaw = 123.0f;

    TestFalse(
        TEXT("玩家与目标水平重合时不能生成稳定轨道 Yaw"),
        WuwaCameraFramingRules::CalculateHorizontalCombatAxisYaw(
            FVector(0.0, 0.0, 0.0),
            FVector(0.0, 0.0, 500.0),
            CombatAxisYaw));

    TestTrue(
        TEXT("战斗轴计算失败时必须清除旧输出"),
        FMath::IsNearlyZero(CombatAxisYaw));

    float CameraProjection = 0.0f;

    TestTrue(
        TEXT("玩家背后的 Camera 应通过投影规则校验"),
        WuwaCameraFramingRules::CalculateCameraCombatAxisProjection(
            FVector::ZeroVector,
            FVector(500.0, 0.0, 0.0),
            FVector(-400.0, 100.0, 200.0),
            CameraProjection));

    TestTrue(
        TEXT("玩家背后的 Camera 投影必须为负"),
        CameraProjection < 0.0f);

    TestTrue(
        TEXT("目标侧的 Camera 应通过投影规则校验"),
        WuwaCameraFramingRules::CalculateCameraCombatAxisProjection(
            FVector::ZeroVector,
            FVector(500.0, 0.0, 0.0),
            FVector(100.0, -200.0, 200.0),
            CameraProjection));

    TestTrue(
        TEXT("越过玩家平面的 Camera 投影必须为正"),
        CameraProjection > 0.0f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWuwaCameraFramingAngularCorrectionTest,
    "Wuwa.Camera.Framing.AngularCorrection",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FWuwaCameraFramingAngularCorrectionTest::RunTest(
    const FString &Parameters)
{
    (void)Parameters;

    FWuwaCameraAngleCorrectionInput Input;
    Input.DeadZoneHalfAngleDegrees = 15.0f;
    Input.MaxCorrectionSpeedDegrees = 120.0f;
    Input.DeltaTime = 0.1f;

    float CorrectedAngle = 0.0f;

    Input.CurrentAngleDegrees = 10.0f;
    Input.DesiredAngleDegrees = 20.0f;

    TestTrue(
        TEXT("安全区内的目标应通过规则校验"),
        WuwaCameraFramingRules::CalculateBoundedAngleCorrection(
            Input,
            CorrectedAngle));

    TestTrue(
        TEXT("安全区内应保持当前镜头角度"),
        FMath::IsNearlyEqual(CorrectedAngle, 10.0f));

    Input.CurrentAngleDegrees = 0.0f;
    Input.DesiredAngleDegrees = 45.0f;

    TestTrue(
        TEXT("安全区外的目标应通过规则校验"),
        WuwaCameraFramingRules::CalculateBoundedAngleCorrection(
            Input,
            CorrectedAngle));

    TestTrue(
        TEXT("修正量应受最大角速度限制"),
        FMath::IsNearlyEqual(CorrectedAngle, 12.0f));

    Input.DesiredAngleDegrees = 20.0f;
    Input.DeltaTime = 1.0f;

    WuwaCameraFramingRules::CalculateBoundedAngleCorrection(
        Input,
        CorrectedAngle);

    TestTrue(
        TEXT("镜头只应修正到安全区边界，不应精确追到目标"),
        FMath::IsNearlyEqual(CorrectedAngle, 5.0f));

    Input.CurrentAngleDegrees = 170.0f;
    Input.DesiredAngleDegrees = -170.0f;
    Input.DeadZoneHalfAngleDegrees = 0.0f;
    Input.MaxCorrectionSpeedDegrees = 10.0f;
    Input.DeltaTime = 1.0f;

    WuwaCameraFramingRules::CalculateBoundedAngleCorrection(
        Input,
        CorrectedAngle);

    TestTrue(
        TEXT("跨越正负180度时应沿最短方向修正"),
        FMath::IsNearlyEqual(
            FMath::FindDeltaAngleDegrees(
                170.0f,
                CorrectedAngle),
            10.0f));

    Input.DesiredAngleDegrees =
        std::numeric_limits<float>::quiet_NaN();

    CorrectedAngle = 123.0f;

    TestFalse(
        TEXT("非有限角度必须被拒绝"),
        WuwaCameraFramingRules::CalculateBoundedAngleCorrection(
            Input,
            CorrectedAngle));

    TestTrue(
        TEXT("规则失败时必须清除旧输出"),
        FMath::IsNearlyZero(CorrectedAngle));

    Input.DesiredAngleDegrees = -170.0f;
    Input.DeltaTime = -0.1f;

    TestFalse(
        TEXT("负 DeltaTime 必须被拒绝"),
        WuwaCameraFramingRules::CalculateBoundedAngleCorrection(
            Input,
            CorrectedAngle));

    return true;
}

#endif
