#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Movement/WuwaMovementTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWuwaLandingClassificationTest,
    "Wuwa.Movement.Landing.Classification",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FWuwaLandingClassificationTest::RunTest(
    const FString &Parameters)
{
    constexpr float Threshold = 900.f;

    TestTrue(
        TEXT("低于阈值应为 Light"),
        WuwaMovementRules::ClassifyLanding(
            Threshold - 1.f,
            Threshold) == EWuwaLandingType::Light);

    TestTrue(
        TEXT("等于阈值应为 Heavy"),
        WuwaMovementRules::ClassifyLanding(
            Threshold,
            Threshold) == EWuwaLandingType::Heavy);

    TestTrue(
        TEXT("高于阈值应为 Heavy"),
        WuwaMovementRules::ClassifyLanding(
            Threshold + 1.f,
            Threshold) == EWuwaLandingType::Heavy);

    return true;
}

#endif