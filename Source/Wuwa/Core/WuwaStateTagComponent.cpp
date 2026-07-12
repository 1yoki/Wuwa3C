#include "Core/WuwaStateTagComponent.h"
#include "Wuwa.h"

UWuwaStateTagComponent::UWuwaStateTagComponent()
{
    // 标签状态由事件驱动，不需要组件每帧 Tick。
    PrimaryComponentTick.bCanEverTick = false;
}

void UWuwaStateTagComponent::AddTag(const FGameplayTag &Tag)
{
    if (!Tag.IsValid())
    {
        UE_LOG(LogWuwa, Warning, TEXT("AddTag 被传入无效 Gameplay Tag。Owner=%s"), *GetNameSafe(GetOwner()));
        return;
    }

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

void UWuwaStateTagComponent::RemoveTag(const FGameplayTag &Tag)
{
    if (!Tag.IsValid())
    {
        UE_LOG(LogWuwa, Warning, TEXT("RemoveTag 被传入无效 Gameplay Tag。Owner=%s"), *GetNameSafe(GetOwner()));
        return;
    }

    int32 *RefCount = TagRefCounts.Find(Tag);

    if (!RefCount || *RefCount <= 0)
    {
        // 不允许没有对应 AddTag 的 RemoveTag 静默通过。
        UE_LOG(LogWuwa, Warning, TEXT("尝试释放未持有的状态标签。Owner=%s, Tag=%s"), *GetNameSafe(GetOwner()), *Tag.ToString());
        return;
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