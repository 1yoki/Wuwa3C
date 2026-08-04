#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Camera/WuwaCameraModeStack.h"
#include "Camera/WuwaCameraProfile.h"
#include "Core/WuwaGameplayTags.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWuwaCameraModeStackLifecycleTest,
    "Wuwa.Camera.ModeStack.Lifecycle",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FWuwaCameraModeStackLifecycleTest::RunTest(
    const FString &Parameters)
{
    (void)Parameters;

    FWuwaCameraModeStack Stack;

    TestFalse(
        TEXT("空 Profile 不应初始化 Stack"),
        Stack.Initialize(nullptr));

    UWuwaCameraProfile *Profile =
        NewObject<UWuwaCameraProfile>();

    TestNotNull(
        TEXT("默认 Camera Profile 应可创建"),
        Profile);

    TestTrue(
        TEXT("默认 Camera Profile 应通过运行时校验"),
        Profile && Profile->IsRuntimeValid());

    TestTrue(
        TEXT("合法 Profile 应初始化 Stack"),
        Profile && Stack.Initialize(Profile));

    TestTrue(
        TEXT("初始化后 Stack 应有效"),
        Stack.IsInitialized());

    TestTrue(
        TEXT("无请求时必须回退 Exploration"),
        Stack.GetActiveModeTag() ==
            WuwaGameplayTags::Camera_Exploration);

    TestNotNull(
        TEXT("Fallback 应能取得 Exploration 配置"),
        Stack.GetActiveModeConfig());

    // 使用独立 UObject 身份模拟多个请求来源，不依赖 World。
    UWuwaCameraProfile *SourceA =
        NewObject<UWuwaCameraProfile>();
    UWuwaCameraProfile *SourceB =
        NewObject<UWuwaCameraProfile>();

    FWuwaCameraModeRequestHandle ExplorationHandle =
        Stack.AcquireMode(
            WuwaGameplayTags::Camera_Exploration,
            SourceA);

    TestFalse(
        TEXT("Exploration 是 Fallback，不能被 Acquire"),
        ExplorationHandle.IsValid());

    FWuwaCameraModeRequestHandle NullSourceHandle =
        Stack.AcquireMode(
            WuwaGameplayTags::Camera_LockOn,
            nullptr);

    TestFalse(
        TEXT("空来源不能取得 LockOn 请求"),
        NullSourceHandle.IsValid());

    TestEqual(
        TEXT("非法请求不能进入 Stack"),
        Stack.GetStoredRequestCount(),
        0);

    FWuwaCameraModeRequestHandle HandleA1 =
        Stack.AcquireMode(
            WuwaGameplayTags::Camera_LockOn,
            SourceA);

    TestTrue(
        TEXT("合法来源应取得 LockOn Handle"),
        HandleA1.IsValid());

    TestTrue(
        TEXT("存在 LockOn 请求时应选择 LockOn"),
        Stack.GetActiveModeTag() ==
            WuwaGameplayTags::Camera_LockOn);

    TestTrue(
        TEXT("首个请求应成为当前请求"),
        Stack.IsActiveRequest(HandleA1));

    FWuwaCameraModeRequestHandle HandleA2 =
        Stack.AcquireMode(
            WuwaGameplayTags::Camera_LockOn,
            SourceA);

    TestTrue(
        TEXT("同一来源可重复取得独立 Handle"),
        HandleA2.IsValid() &&
            HandleA2.Id != HandleA1.Id);

    TestEqual(
        TEXT("同一来源的两个 Handle 必须分别保存"),
        Stack.GetRequestCountForSource(SourceA),
        2);

    TestTrue(
        TEXT("同优先级时较新的请求应胜出"),
        Stack.IsActiveRequest(HandleA2));

    TestFalse(
        TEXT("较新的同级请求存在时，旧请求不应胜出"),
        Stack.IsActiveRequest(HandleA1));

    FWuwaCameraModeRequestHandle MismatchedHandle =
        HandleA1;
    MismatchedHandle.ModeTag =
        WuwaGameplayTags::Camera_Exploration;

    TestFalse(
        TEXT("Id 相同但 ModeTag 不匹配时不能释放"),
        Stack.ReleaseMode(MismatchedHandle));

    FWuwaCameraModeRequestHandle UnknownHandle(
        FGuid::NewGuid(),
        WuwaGameplayTags::Camera_LockOn);

    TestFalse(
        TEXT("未知 Id 不能误删已有请求"),
        Stack.ReleaseMode(UnknownHandle));

    TestEqual(
        TEXT("错误 Handle 不能改变请求数量"),
        Stack.GetStoredRequestCount(),
        2);

    TestTrue(
        TEXT("应能精确释放较新的 Handle"),
        Stack.ReleaseMode(HandleA2));

    TestFalse(
        TEXT("成功释放后调用方 Handle 必须失效"),
        HandleA2.IsValid());

    TestFalse(
        TEXT("重复释放同一 Handle 应失败"),
        Stack.ReleaseMode(HandleA2));

    TestTrue(
        TEXT("释放较新请求后应稳定回到旧请求"),
        Stack.IsActiveRequest(HandleA1));

    FWuwaCameraModeRequestHandle HandleB =
        Stack.AcquireMode(
            WuwaGameplayTags::Camera_LockOn,
            SourceB);

    TestTrue(
        TEXT("第二来源应取得独立请求"),
        HandleB.IsValid());

    TestTrue(
        TEXT("第二来源的较新请求应胜出"),
        Stack.IsActiveRequest(HandleB));

    TestEqual(
        TEXT("ReleaseBySource 应清除 SourceA 的全部请求"),
        Stack.ReleaseBySource(SourceA),
        1);

    TestEqual(
        TEXT("SourceA 清理后不应保留请求"),
        Stack.GetRequestCountForSource(SourceA),
        0);

    TestTrue(
        TEXT("清理 SourceA 后 SourceB 请求仍应生效"),
        Stack.IsActiveRequest(HandleB));

    TestEqual(
        TEXT("重复按来源清理应返回零"),
        Stack.ReleaseBySource(SourceA),
        0);

    /*
     * 模拟来源异常销毁。
     * 弱来源失效后即使尚未 Prune，也不能继续成为当前请求。
     */
    UWuwaCameraProfile *DestroyedSource =
        NewObject<UWuwaCameraProfile>();

    FWuwaCameraModeRequestHandle DestroyedHandle =
        Stack.AcquireMode(
            WuwaGameplayTags::Camera_LockOn,
            DestroyedSource);

    TestTrue(
        TEXT("待销毁来源的请求取得前应有效"),
        Stack.IsActiveRequest(DestroyedHandle));

    DestroyedSource->MarkAsGarbage();

    TestFalse(
        TEXT("来源失效后请求应立即退出仲裁"),
        Stack.IsActiveRequest(DestroyedHandle));

    TestTrue(
        TEXT("失效来源退出后应回到仍有效的 SourceB 请求"),
        Stack.IsActiveRequest(HandleB));

    TestEqual(
        TEXT("Prune 应删除一个失效来源请求"),
        Stack.PruneInvalidSources(),
        1);

    TestEqual(
        TEXT("Prune 后只应保留 SourceB 请求"),
        Stack.GetStoredRequestCount(),
        1);

    TestTrue(
        TEXT("释放最后一个有效请求应成功"),
        Stack.ReleaseMode(HandleB));

    TestTrue(
        TEXT("最后请求释放后必须回到 Exploration"),
        Stack.GetActiveModeTag() ==
            WuwaGameplayTags::Camera_Exploration);

    UWuwaCameraProfile *SourceC =
        NewObject<UWuwaCameraProfile>();

    FWuwaCameraModeRequestHandle HandleC =
        Stack.AcquireMode(
            WuwaGameplayTags::Camera_LockOn,
            SourceC);

    TestTrue(
        TEXT("ClearRequests 前请求应有效"),
        HandleC.IsValid());

    Stack.ClearRequests();

    TestEqual(
        TEXT("ClearRequests 必须删除全部运行时请求"),
        Stack.GetStoredRequestCount(),
        0);

    TestTrue(
        TEXT("ClearRequests 后应回到 Exploration"),
        Stack.GetActiveModeTag() ==
            WuwaGameplayTags::Camera_Exploration);

    TestFalse(
        TEXT("统一清理后旧 Handle 不得释放其他请求"),
        Stack.ReleaseMode(HandleC));

    Stack.Reset();

    TestFalse(
        TEXT("Reset 后 Stack 应回到未初始化状态"),
        Stack.IsInitialized());

    TestFalse(
        TEXT("Reset 后不应返回有效模式"),
        Stack.GetActiveModeTag().IsValid());

    TestNull(
        TEXT("Reset 后不应返回模式配置"),
        Stack.GetActiveModeConfig());

    return true;
}

#endif