#pragma once

#include "CoreMinimal.h"
#include "Camera/WuwaCameraTypes.h"

class UWuwaCameraProfile;

/*
 * 不依赖 World 的 Camera Mode 请求容器与确定性选择器。
 * Stack 不读取 Targeting、不计算构图，也不修改任何 Gameplay 状态。
 */

class WUWA_API FWuwaCameraModeStack
{
public:
    /*
     * 复制已经通过运行时校验的 Profile 配置。
     * Stack 不持有 Data Asset，防止运行期间资产修改改变仲裁结果。
     */
    bool Initialize(const UWuwaCameraProfile *Profile);

    // 清除配置和请求，供初始化失败或 Owner 生命周期结束时使用。
    void Reset();

    bool IsInitialized() const
    {
        return bInitialized;
    }

    /*
     * 为有效弱来源取得一次模式请求。
     * Exploration 是无请求 Fallback，因此不能被 Acquire。
     */
    FWuwaCameraModeRequestHandle AcquireMode(const FGameplayTag &ModeTag, UObject *Source);

    /*
     * 只释放与 Id、ModeTag 都精确匹配的请求。
     * 成功后使调用方持有的 Handle 失效。
     */
    bool ReleaseMode(FWuwaCameraModeRequestHandle &Handle);

    /*
     * 对称清除一个来源持有的全部请求。
     * 外部 Handle 是值类型，统一清理后由原持有者自行 Reset。
     */
    int32 ReleaseBySource(UObject *Source);

    // 删除弱来源已经销毁的请求，返回删除数量。
    int32 PruneInvalidSources();

    // Owner EndPlay 使用；不保留任何运行时请求。
    void ClearRequests();

    // 返回当前胜出模式；无有效请求时返回 Exploration。
    FGameplayTag GetActiveModeTag() const;

    const FWuwaCameraModeConfig *GetActiveModeConfig() const;
    const FWuwaCameraModeConfig *FindModeConfig(const FGameplayTag &ModeTag) const;

    // 判断调用方已经持有的 Handle 当前是否为胜出请求。
    bool IsActiveRequest(const FWuwaCameraModeRequestHandle &Handle) const;

    // 包含尚未执行 Prune 的失效来源请求。
    int32 GetStoredRequestCount() const
    {
        return Requests.Num();
    }

    int32 GetRequestCountForSource(UObject *Source) const;

private:
    struct FRuntimeRequest
    {
        FGameplayTag ModeTag;
        TWeakObjectPtr<UObject> Source;
        uint64 AcquireSerial = 0;
    };

    const FRuntimeRequest *FindWinningRequest(FGuid *OutHandleId = nullptr) const;

    TMap<FGameplayTag, FWuwaCameraModeConfig> RuntimeConfigs;
    TMap<FGuid, FRuntimeRequest> Requests;

    FGameplayTag FallbackModeTag;
    uint64 LastAcquireSerial = 0;
    bool bInitialized = false;
};