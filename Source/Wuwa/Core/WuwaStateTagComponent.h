#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Core/WuwaStateTagTypes.h"
#include "WuwaStateTagComponent.generated.h"

// 状态标签发生实际变化时广播
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWuwaStateTagChangedSignature, FGameplayTag, Tag, bool, bAdded);

/*
 * 角色活动 Gameplay Tags 的统一聚合组件。
 * 该组件只保存标签事实，不负责决定角色能否冲刺、攻击或闪避。
 * 具体状态仍由 Movement、Action Router 等状态拥有者决定。
 */
UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaStateTagComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWuwaStateTagComponent();

    /*
     * 为一个明确来源取得标签引用，并返回只能释放一次的 Handle。
     * Day 5 起的新动作生命周期应优先使用该 API。
     */
    FWuwaStateTagHandle AcquireTag(const FGameplayTag &Tag);

    /* 释放 AcquireTag 创建的引用；成功后会使传入 Handle 失效。 */
    bool ReleaseTag(FWuwaStateTagHandle &Handle);

    /*
     * 取得一次标签引用。
     * 只有 Gameplay C++ 状态拥有者可以调用。
     */
    void AddTag(const FGameplayTag &Tag);

    /*
     * 释放一次标签引用。
     * 引用计数归零后，标签才会从 ActiveTags 中移除。
     */
    void RemoveTag(const FGameplayTag &Tag);

    /* 查询是否拥有指定标签，默认支持父子标签匹配 */
    UFUNCTION(BlueprintPure, Category = "Wuwa|State Tags")
    bool HasTag(const FGameplayTag &Tag, bool bExactMatch = false) const;

    /* 查询当前标签是否与给定集合中的任意标签匹配 */
    UFUNCTION(BlueprintPure, Category = "Wuwa|State Tags")
    bool HasAny(const FGameplayTagContainer &Tags) const;

    /*
     * 返回当前活动标签的副本。
     * 返回副本可以防止外部直接修改组件内部容器。
     */
    UFUNCTION(BlueprintPure, Category = "Wuwa|State Tags")
    FGameplayTagContainer GetActiveTags() const;

    /* 返回精确标签当前被多少个来源持有，供只读 Debug 使用。 */
    UFUNCTION(BlueprintPure, Category = "Wuwa|State Tags")
    int32 GetTagSourceCount(const FGameplayTag &Tag) const;

    /* 只有标签真正加入或移除时才广播。 */
    UPROPERTY(BlueprintAssignable, Category = "Wuwa|State Tags")
    FWuwaStateTagChangedSignature OnStateTagChanged;

private:
    void IncrementTagRef(const FGameplayTag &Tag);
    bool DecrementTagRef(const FGameplayTag &Tag);

    /* 对外提供的活动标签集合。 */
    UPROPERTY(VisibleInstanceOnly, Transient, Category = "State Tags", meta = (AllowPrivateAccess = "true"))
    FGameplayTagContainer ActiveTags;

    /* 每个标签当前被多少个状态来源持有。 */
    TMap<FGameplayTag, int32> TagRefCounts;

    /* 兼容 AddTag/RemoveTag 的来源计数，防止旧 API 误释放 Handle 持有的引用。 */
    TMap<FGameplayTag, int32> LegacyTagRefCounts;

    /* 将来源 Handle 绑定到它实际取得的标签，防止跨来源误释放。 */
    TMap<FGuid, FGameplayTag> HandleTags;
};
