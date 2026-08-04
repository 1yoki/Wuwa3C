#include "Camera/WuwaCameraModeStack.h"

#include "Camera/WuwaCameraProfile.h"
#include "Core/WuwaGameplayTags.h"

bool FWuwaCameraModeStack::Initialize(const UWuwaCameraProfile *Profile)
{
    Reset();

    if (!::IsValid(Profile) || !Profile->IsRuntimeValid())
    {
        return false;
    }

    RuntimeConfigs.Reserve(Profile->ModeConfigs.Num());

    for (const FWuwaCameraModeConfig &Config : Profile->ModeConfigs)
    {
        RuntimeConfigs.Add(Config.ModeTag, Config);
    }

    FallbackModeTag = WuwaGameplayTags::Camera_Exploration;

    bInitialized = RuntimeConfigs.Contains(FallbackModeTag) && RuntimeConfigs.Contains(WuwaGameplayTags::Camera_LockOn);

    if (!bInitialized)
    {
        Reset();
    }

    return bInitialized;
}

void FWuwaCameraModeStack::Reset()
{
    ClearRequests();
    RuntimeConfigs.Reset();
    FallbackModeTag = FGameplayTag();
    bInitialized = false;
}

FWuwaCameraModeRequestHandle FWuwaCameraModeStack::AcquireMode(const FGameplayTag &ModeTag, UObject *Source)
{
    if (!bInitialized || !ModeTag.IsValid() || ModeTag == FallbackModeTag || !RuntimeConfigs.Contains(ModeTag) || !IsValid(Source))
    {
        return FWuwaCameraModeRequestHandle();
    }

    // uint64 耗尽时拒绝新请求，避免同级请求失去稳定的新旧顺序。
    if (LastAcquireSerial == MAX_uint64)
    {
        return FWuwaCameraModeRequestHandle();
    }

    FGuid HandleId;

    do
    {
        HandleId = FGuid::NewGuid();
    }

    while (!HandleId.IsValid() || Requests.Contains(HandleId));

    FRuntimeRequest Request;
    Request.ModeTag = ModeTag;
    Request.Source = Source;
    Request.AcquireSerial = ++LastAcquireSerial;

    Requests.Add(HandleId, Request);

    return FWuwaCameraModeRequestHandle(HandleId, ModeTag);
}

bool FWuwaCameraModeStack::ReleaseMode(FWuwaCameraModeRequestHandle &Handle)
{
    if (!Handle.IsValid())
    {
        return false;
    }

    const FRuntimeRequest *Request = Requests.Find(Handle.Id);

    if (!Request || Request->ModeTag != Handle.ModeTag)
    {
        return false;
    }

    Requests.Remove(Handle.Id);
    Handle.Reset();

    return true;
}

int32 FWuwaCameraModeStack::ReleaseBySource(UObject *Source)
{
    if (Source == nullptr)
    {
        return 0;
    }

    /*
     * 比较 Weak Object Index/Serial，而不是只调用 Get()。
     * 来源进入销毁阶段时 Get() 可能已经为空，但统一清理仍应准确命中。
     */
    const TWeakObjectPtr<UObject> SourceKey(Source);
    int32 ReleasedCount = 0;

    for (auto Iterator = Requests.CreateIterator(); Iterator; ++Iterator)
    {
        if (Iterator.Value().Source.HasSameIndexAndSerialNumber(SourceKey))
        {
            Iterator.RemoveCurrent();
            ++ReleasedCount;
        }
    }
    return ReleasedCount;
}

int32 FWuwaCameraModeStack::PruneInvalidSources()
{
    int32 RemovedCount = 0;

    for (auto Iterator = Requests.CreateIterator(); Iterator; ++Iterator)
    {
        if (!Iterator.Value().Source.IsValid())
        {
            Iterator.RemoveCurrent();
            ++RemovedCount;
        }
    }
    return RemovedCount;
}

void FWuwaCameraModeStack::ClearRequests()
{
    Requests.Reset();
    LastAcquireSerial = 0;
}

FGameplayTag FWuwaCameraModeStack::GetActiveModeTag() const
{
    if (!bInitialized)
    {
        return FGameplayTag();
    }

    const FRuntimeRequest *WinningRequest = FindWinningRequest();

    return WinningRequest ? WinningRequest->ModeTag : FallbackModeTag;
}

const FWuwaCameraModeConfig *FWuwaCameraModeStack::GetActiveModeConfig() const
{
    return FindModeConfig(GetActiveModeTag());
}

const FWuwaCameraModeConfig *FWuwaCameraModeStack::FindModeConfig(const FGameplayTag &ModeTag) const
{
    if (!bInitialized || !ModeTag.IsValid())
    {
        return nullptr;
    }

    return RuntimeConfigs.Find(ModeTag);
}

bool FWuwaCameraModeStack::IsActiveRequest(const FWuwaCameraModeRequestHandle &Handle) const
{
    if (!Handle.IsValid())
    {
        return false;
    }

    FGuid WinningHandleId;
    const FRuntimeRequest *WinningRequest = FindWinningRequest(&WinningHandleId);

    return WinningRequest && WinningHandleId == Handle.Id && WinningRequest->ModeTag == Handle.ModeTag;
}

int32 FWuwaCameraModeStack::GetRequestCountForSource(UObject *Source) const
{
    if (Source == nullptr)
    {
        return 0;
    }

    const TWeakObjectPtr<UObject> SourceKey(Source);
    int32 RequestCount = 0;

    for (const TPair<FGuid, FRuntimeRequest> &Pair : Requests)
    {
        if (Pair.Value.Source.HasSameIndexAndSerialNumber(SourceKey))
        {
            ++RequestCount;
        }
    }

    return RequestCount;
}

const FWuwaCameraModeStack::FRuntimeRequest *FWuwaCameraModeStack::FindWinningRequest(FGuid *OutHandleId) const
{
    const FRuntimeRequest *WinningRequest = nullptr;
    FGuid WinningHandleId;
    int32 WinningPriority = MIN_int32;

    for (const TPair<FGuid, FRuntimeRequest> &Pair : Requests)
    {
        const FRuntimeRequest &Candidate = Pair.Value;

        // 失效来源即使尚未 Prune，也不能继续影响当前模式。
        if (!Candidate.Source.IsValid())
        {
            continue;
        }

        const FWuwaCameraModeConfig *Config = RuntimeConfigs.Find(Candidate.ModeTag);

        if (Config == nullptr)
        {
            continue;
        }

        const bool bHigherPriority = Config->Priority > WinningPriority;

        const bool bNewerAtSamePriority =
            WinningRequest != nullptr && Config->Priority == WinningPriority &&
            Candidate.AcquireSerial > WinningRequest->AcquireSerial;

        if (WinningRequest == nullptr || bHigherPriority || bNewerAtSamePriority)
        {
            WinningRequest = &Candidate;
            WinningHandleId = Pair.Key;
            WinningPriority = Config->Priority;
        }
    }

    if (OutHandleId != nullptr)
    {
        *OutHandleId = WinningRequest ? WinningHandleId : FGuid();
    }

    return WinningRequest;
}