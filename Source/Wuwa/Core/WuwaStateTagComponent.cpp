#include "Core/WuwaStateTagComponent.h"
#include "Wuwa.h"

UWuwaStateTagComponent::UWuwaStateTagComponent()
{
    // 标签状态由事件驱动，不需要组件每帧 Tick。
    PrimaryComponentTick.bCanEverTick = false;
}

FWuwaStateTagHandle UWuwaStateTagComponent::AcquireTag(const FGameplayTag &Tag)
{
    if (!Tag.IsValid())
    {
        UE_LOG(LogWuwa, Warning, TEXT("AcquireTag 被传入无效 Gameplay Tag。Owner=%s"), *GetNameSafe(GetOwner()));
        return FWuwaStateTagHandle();
    }

    FGuid HandleId = FGuid::NewGuid();

    // 极低概率发生 GUID 冲突时重新生成，确保 Handle 在本组件内唯一。
    while (HandleTags.Contains(HandleId))
    {
        HandleId = FGuid::NewGuid();
    }

    HandleTags.Add(HandleId, Tag);
    IncrementTagRef(Tag);

    return FWuwaStateTagHandle(HandleId, Tag);
}

bool UWuwaStateTagComponent::ReleaseTag(FWuwaStateTagHandle &Handle)
{
    if (!Handle.IsValid())
    {
        UE_LOG(LogWuwa, Warning, TEXT("ReleaseTag 被传入无效 Handle。Owner=%s"), *GetNameSafe(GetOwner()));
        return false;
    }

    const FGameplayTag *AcquiredTag = HandleTags.Find(Handle.Id);

    if (!AcquiredTag || *AcquiredTag != Handle.Tag)
    {
        UE_LOG(
            LogWuwa,
            Warning,
            TEXT("尝试释放未知或不匹配的状态标签 Handle。Owner=%s, Tag=%s, Id=%s"),
            *GetNameSafe(GetOwner()),
            *Handle.Tag.ToString(),
            *Handle.Id.ToString());
        return false;
    }

    const FGameplayTag TagToRelease = *AcquiredTag;
    const bool bReleased = DecrementTagRef(TagToRelease);

    if (bReleased)
    {
        HandleTags.Remove(Handle.Id);
        Handle.Reset();
    }

    return bReleased;
}

void UWuwaStateTagComponent::AddTag(const FGameplayTag &Tag)
{
    if (!Tag.IsValid())
    {
        UE_LOG(LogWuwa, Warning, TEXT("AddTag 被传入无效 Gameplay Tag。Owner=%s"), *GetNameSafe(GetOwner()));
        return;
    }

    // 旧 API 也记录自己的来源次数，不能误释放 Handle 持有的引用。
    int32 &LegacyRefCount = LegacyTagRefCounts.FindOrAdd(Tag);
    ++LegacyRefCount;

    // 保留 Day 3 API，已有 Movement 调用无需在 Part 1 同步重构。
    IncrementTagRef(Tag);
}

void UWuwaStateTagComponent::RemoveTag(const FGameplayTag &Tag)
{
    if (!Tag.IsValid())
    {
        UE_LOG(LogWuwa, Warning, TEXT("RemoveTag 被传入无效 Gameplay Tag。Owner=%s"), *GetNameSafe(GetOwner()));
        return;
    }

    int32 *LegacyRefCount = LegacyTagRefCounts.Find(Tag);

    if (!LegacyRefCount || *LegacyRefCount <= 0)
    {
        UE_LOG(LogWuwa, Warning, TEXT("尝试通过 RemoveTag 释放非 Legacy 来源标签。Owner=%s, Tag=%s"), *GetNameSafe(GetOwner()), *Tag.ToString());
        return;
    }

    --(*LegacyRefCount);

    if (*LegacyRefCount == 0)
    {
        LegacyTagRefCounts.Remove(Tag);
    }

    DecrementTagRef(Tag);
}

void UWuwaStateTagComponent::IncrementTagRef(const FGameplayTag &Tag)
{
    // FindOrAdd 会在标签不存在时创建一个值为 0 的计数。
    int32 &RefCount = TagRefCounts.FindOrAdd(Tag);
    ++RefCount;

    // 只有第一次取得引用时，才真正修改容器并广播事件。
    if (RefCount == 1)
    {
        ActiveTags.AddTag(Tag);
        OnStateTagChanged.Broadcast(Tag, true);

        UE_LOG(LogWuwa, Verbose, TEXT("状态标签已添加。Owner=%s, Tag=%s"), *GetNameSafe(GetOwner()), *Tag.ToString());
    }
}

bool UWuwaStateTagComponent::DecrementTagRef(const FGameplayTag &Tag)
{
    int32 *RefCount = TagRefCounts.Find(Tag);

    if (!RefCount || *RefCount <= 0)
    {
        // 不允许没有对应来源的释放静默通过。
        UE_LOG(LogWuwa, Warning, TEXT("尝试释放未持有的状态标签。Owner=%s, Tag=%s"), *GetNameSafe(GetOwner()), *Tag.ToString());
        return false;
    }

    --(*RefCount);

    if (*RefCount == 0)
    {
        // 引用归零后再从计数表和活动容器中移除。
        TagRefCounts.Remove(Tag);
        ActiveTags.RemoveTag(Tag);

        OnStateTagChanged.Broadcast(Tag, false);

        UE_LOG(LogWuwa, Verbose, TEXT("状态标签已移除。Owner=%s, Tag=%s"), *GetNameSafe(GetOwner()), *Tag.ToString());
    }

    return true;
}

bool UWuwaStateTagComponent::HasTag(const FGameplayTag &Tag, const bool bExactMatch) const
{
    if (!Tag.IsValid())
    {
        return false;
    }

    // Exact 为 true 时只匹配完全相同的标签。
    // 否则 State.Locomotion.Sprinting 也能匹配 State.Locomotion。
    return bExactMatch ? ActiveTags.HasTagExact(Tag) : ActiveTags.HasTag(Tag);
}

bool UWuwaStateTagComponent::HasAny(const FGameplayTagContainer &Tags) const
{
    // HasAny 默认支持 Gameplay Tag 的父子层级匹配。
    return ActiveTags.HasAny(Tags);
}

FGameplayTagContainer UWuwaStateTagComponent::GetActiveTags() const
{
    // 返回副本，外部修改不会影响组件内部状态。
    return ActiveTags;
}

int32 UWuwaStateTagComponent::GetTagSourceCount(const FGameplayTag &Tag) const
{
    const int32 *RefCount = TagRefCounts.Find(Tag);
    return RefCount ? *RefCount : 0;
}
