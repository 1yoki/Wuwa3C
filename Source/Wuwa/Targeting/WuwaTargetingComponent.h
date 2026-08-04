#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Targeting/WuwaTargetingTypes.h"

#include "TimerManager.h"

#include "Core/WuwaStateTagTypes.h"

#include "WuwaTargetingComponent.generated.h"

class AActor;
class UWuwaTargetingProfile;
class UWuwaStateTagComponent;

/**
 * Profile 的运行时配置副本。
 * Targeting 初始化后只读取该快照，避免 Tick 或请求期间反复读取可变 Data Asset。
 */

struct FWuwaTargetingRuntimeConfig
{
    TArray<TEnumAsByte<EObjectTypeQuery>> CandidateObjectTypes;

    float SearchRadius = 0.f;
    float MaxAcquireAngleDegrees = 0.f;
    float HardLockReleaseDistance = 0.f;

    ECollisionChannel VisibilityTraceChannel = ECC_Visibility;
    float OcclusionGraceTime = 0.f;
    float CandidateRefreshInterval = 0.f;

    float DistanceWeight = 0.f;
    float ViewAlignmentWeight = 0.f;
    float CurrentSoftTargetBonus = 0.f;

    float SwitchMinimumHorizontalDelta = 0.f;
    float SwitchVerticalPenaltyWeight = 0.f;
    float SwitchDistancePenaltyWeight = 0.f;
};

/**
 * 单次候选刷新使用的相机视图快照。
 * 一次刷新中的所有候选必须使用同一份视图事实。
 */
struct FWuwaTargetingViewSnapshot
{
    FVector Location = FVector::ZeroVector;
    FVector Forward = FVector::ForwardVector;
    FVector Right = FVector::RightVector;
    FVector Up = FVector::UpVector;

    bool IsValid() const
    {
        return !Location.ContainsNaN() &&
               !Forward.ContainsNaN() &&
               !Right.ContainsNaN() &&
               !Up.ContainsNaN() &&
               !Forward.IsNearlyZero() &&
               !Right.IsNearlyZero() &&
               !Up.IsNearlyZero();
    }
};

/**
 * 只有目标或锁定模式发生语义变化时才广播。
 * 移动目标的 TargetPoint 更新不应单独产生事件。
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWuwaTargetContextChangedSignature,
                                            const FWuwaTargetContext &, TargetContext);

/**
 * 当前角色目标选择状态的唯一权威。
 *
 * Component 负责候选、软锁、硬锁和切换；
 * Character、Movement、Camera 与 Combat 只能读取 Target Context。
 */

UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaTargetingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWuwaTargetingComponent();

    /**
     * 由 Character Composition Root 显式调用。
     * Requester 必须是组件 Owner，Profile 必须通过运行时校验。
     */
    bool Initialize(AActor *InRequester, UWuwaStateTagComponent *InStateTagComponent, const UWuwaTargetingProfile *InProfile);

    UFUNCTION(BlueprintPure, Category = "Wuwa|Targeting")
    bool IsInitialized() const;

    // 立即执行一次候选刷新并提交Soft Context
    UFUNCTION(BlueprintCallable, Category = "Wuwa|Targeting")
    FWuwaTargetingResult RefreshSoftTarget();

    // 立即刷新 Soft Target，并将有效目标原子提升为 Hard Lock。标签句柄取得失败时不得提交 Hard Context。
    UFUNCTION(BlueprintCallable, Category = "Wuwa|Targeting")
    FWuwaTargetingResult TryEnterHardLock();

    // Lock 输入的唯一 Toggle 入口。未处于 Hard 时尝试进入；已经 Hard 时主动解除并立即恢复 Soft。
    UFUNCTION(BlueprintCallable, Category = "Wuwa|Targeting")
    FWuwaTargetingResult ToggleHardLock();

    // 只在 Hard 模式中按相机视空间方向切换目标；失败时必须保留原 Hard Context。
    UFUNCTION(BlueprintCallable, Category = "Wuwa|Targeting")
    FWuwaTargetingResult SwitchHardTarget(float Direction);

    // 主动或异常离开 Hard Lock；释放本组件标签和销毁绑定后，原子恢复 Soft/None。
    UFUNCTION(BlueprintCallable, Category = "Wuwa|Targeting")
    FWuwaTargetingResult ClearHardLock();

    // 返回 Context 副本，调用方不能修改 Targeting 内部状态。
    UFUNCTION(BlueprintPure, Category = "Wuwa|Targeting")
    FWuwaTargetContext GetTargetContext() const
    {
        return TargetContext;
    }

    UFUNCTION(BlueprintPure, Category = "Wuwa|Targeting")
    EWuwaTargetingFailureReason GetLastFailureReason() const
    {
        return LastFailureReason;
    }

    UPROPERTY(BlueprintAssignable, Category = "Wuwa|Targeting")
    FWuwaTargetContextChangedSignature OnTargetContextChanged;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // 从 PlayerController POV 获取本次刷新唯一使用的视图快照。
    bool CaptureViewSnapshot(FWuwaTargetingViewSnapshot &OutView) const;

    // 收集、去重并评估搜索半径内的全部候选。
    void GatherCandidates(const FWuwaTargetingViewSnapshot &View, TArray<FWuwaTargetCandidate> &OutCandidates) const;

    // 对单个 Actor 执行接口、距离、视角、可见性和评分检查。
    bool EvaluateCandidate(AActor &CandidateActor, const FWuwaTargetingViewSnapshot &View, FWuwaTargetCandidate &OutCandidate, EWuwaTargetingFailureReason &OutFailureReason) const;

    // 可见性是新候选的准入条件，不参与连续评分。
    bool IsCandidateVisible(const AActor &CandidateActor, const FVector &TargetPoint, const FWuwaTargetingViewSnapshot &View) const;

    // 为当前 Hard Target 生成切换规则使用的视空间原点。
    // 当前目标允许位于 Soft 搜索半径或获取角之外，但必须位于相机前方。
    bool BuildHardTargetSwitchOriginScore(const FWuwaTargetingViewSnapshot &View, FWuwaTargetScoreBreakdown &OutScore) const;

    // 提交有效 Soft Target；只有目标或模式改变时才增加 Revision 并广播
    void CommitSoftTarget(const FWuwaTargetCandidate &Candidate);

    // 没有候选或视图失效时，只清理当前 Soft Context。
    void ClearSoftTarget();

    // 只释放 Targeting 自己取得的 HardLock 标签句柄。
    bool ReleaseHardLockTag();

    UPROPERTY(Transient)
    FWuwaStateTagHandle HardLockTagHandle;

    // // 原子替换 Hard Target 的销毁观察；新目标无效时不得解绑旧目标。
    bool BindHardTargetDestroyed(AActor *TargetActor);

    void UnbindHardTargetDestroyed();

    UFUNCTION()
    void HandleHardTargetDestroyed(AActor *DestroyedActor);

    // 周期验证当前 Hard Target；Hard 不再应用初始获取角度和 SearchRadius。
    void ValidateCurrentHardTarget();

    // 验证失败统一经过 ClearHardLock，防止标签、委托和 Context 分裂。
    void ExitHardLockForValidationFailure(EWuwaTargetingFailureReason FailureReason);

    // 弱引用不能延长目标生命周期，只用于对称解绑当前委托。
    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> BoundHardTarget;

    // 小于 0 表示当前未处于连续遮挡；只记录当前 Hard Target 的遮挡区间。
    double HardTargetOcclusionStartTimeSeconds = -1.0;

    // 使用 Profile 快照中的刷新间隔启动唯一循环 Timer。
    void StartCandidateRefreshTimer();

    // 重新初始化与 EndPlay 必须清除 Timer，防止失效组件继续刷新。
    void ClearCandidateRefreshTimer();

    // Timer 根据当前 Context 分流：Hard 做存续验证，Soft/None 刷新候选。
    void HandleCandidateRefreshTimer();

    FTimerHandle CandidateRefreshTimerHandle;

    /**
     * 清空本组件拥有的运行态。
     * 后续加入 Timer、目标销毁委托和标签句柄后，也必须汇总到这里清理。
     */
    void ResetRuntimeState();

    // Requester 使用弱引用，Targeting 不能延长角色或其他 Owner 的生命周期。
    TWeakObjectPtr<AActor> Requester;

    // StateTagComponent 是标签事实权威；Targeting 只能释放自己取得的 Handle。
    UPROPERTY(Transient)
    TWeakObjectPtr<UWuwaStateTagComponent> StateTagComponent;

    // 初始化后不再读取原始 Data Asset。
    FWuwaTargetingRuntimeConfig RuntimeConfig;

    // 对外发布唯一目标读取副本，Character、Movement、Camera 和 Combat 只能消费该副本。
    UPROPERTY(Transient)
    FWuwaTargetContext TargetContext;

    UPROPERTY(Transient)
    EWuwaTargetingFailureReason LastFailureReason = EWuwaTargetingFailureReason::NotInitialized;

    bool bInitialized = false;
};