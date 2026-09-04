#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Messaging/WuwaCharacterMessageHandler.h"
#include "Targeting/WuwaTargetingTypes.h"

#include "TimerManager.h"

#include "Core/WuwaStateTagTypes.h"

#include "WuwaTargetingComponent.generated.h"

class AActor;
class UWuwaTargetingProfile;
class UWuwaStateTagComponent;

/** Targeting 运行时配置 */

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

/** 单次候选刷新使用的相机视图 */
struct FWuwaTargetingViewSnapshot
{
	FVector Location = FVector::ZeroVector;
	FVector Forward = FVector::ForwardVector;
	FVector Right = FVector::RightVector;
	FVector Up = FVector::UpVector;

	bool IsValid() const
	{
		return !Location.ContainsNaN() && !Forward.ContainsNaN() && !Right.ContainsNaN() && !Up.ContainsNaN() &&
		       !Forward.IsNearlyZero() && !Right.IsNearlyZero() && !Up.IsNearlyZero();
	}
};

/** 目标或锁定模式变化时广播 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWuwaTargetContextChangedSignature,
                                            const FWuwaTargetContext&,
                                            TargetContext);

/** 管理角色的候选目标、软锁和硬锁状态 */

UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaTargetingComponent : public UActorComponent, public IWuwaCharacterMessageHandler
{
	GENERATED_BODY()

public:
	UWuwaTargetingComponent();

	/**
     * 初始化目标选择运行态
     * @param InRequester			必须与组件 Owner 相同
     * @param InStateTagComponent	状态标签组件
     * @param InProfile			目标选择配置
     * @return 依赖与配置是否有效
     */
	bool Initialize(AActor* InRequester,
	                UWuwaStateTagComponent* InStateTagComponent,
	                const UWuwaTargetingProfile* InProfile);

	/** @return 组件是否已完成初始化 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Targeting")
	bool IsInitialized() const;

	/** @return 本次软锁刷新结果 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Targeting")
	FWuwaTargetingResult RefreshSoftTarget();

	/** @return 进入硬锁的结果 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Targeting")
	FWuwaTargetingResult TryEnterHardLock();

	/** @return 切换硬锁状态的结果 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Targeting")
	FWuwaTargetingResult ToggleHardLock();

	/**
     * 按相机视空间方向切换硬锁目标
     * @param Direction	水平切换方向
     * @return 本次切换结果
     */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Targeting")
	FWuwaTargetingResult SwitchHardTarget(float Direction);

	/** @return 解除硬锁并恢复软锁的结果 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Targeting")
	FWuwaTargetingResult ClearHardLock();

	/** @return 当前目标上下文副本 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Targeting")
	FWuwaTargetContext GetTargetContext() const
	{
		return TargetContext;
	}

	/** @return 最近一次失败原因 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Targeting")
	EWuwaTargetingFailureReason GetLastFailureReason() const
	{
		return LastFailureReason;
	}

	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Targeting")
	FWuwaTargetContextChangedSignature OnTargetContextChanged;

	/** Targeting 即时命令处理完成后的只读事实 */
	FWuwaTargetingCommandFactNativeSignature OnTargetingCommandFact;

	//~ Begin IWuwaCharacterMessageHandler Interface
	virtual void GatherHandledInputTags(FGameplayTagContainer& OutInputTags) const override;
	virtual FWuwaCommandDispatchResult HandleInputCommand(const FWuwaInputCommand& Command,
	                                                      const FWuwaInputFrame& InputFrame) override;
	//~ End IWuwaCharacterMessageHandler Interface

protected:
	//~ Begin UActorComponent Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

private:
	/**
     * 提交本地预测后的有限网络命令
     * @param Kind		命令类型
     * @param RequestedTarget	客户端预测目标
     * @return 命令是否进入权威链路
     */
	bool SubmitTargetingCommand(EWuwaTargetingNetworkCommandKind Kind, AActor* RequestedTarget);

	/** @param Command 客户端提交的有限 Targeting 命令 */
	UFUNCTION(Server, Reliable)
	void ServerSubmitTargetingCommand(const FWuwaTargetingNetworkCommand& Command);

	/** @param Command 服务端当前需要裁决的有限命令 */
	void ProcessAuthorityTargetingCommand(const FWuwaTargetingNetworkCommand& Command);

	/**
     * 使用服务端世界事实复核请求目标
     * @param RequestedTarget	客户端请求目标
     * @param OutCandidate	接收服务端重新计算的候选
     * @return 目标是否满足现有范围、视角、遮挡和资格规则
     */
	bool ValidateAuthorityTarget(AActor* RequestedTarget, FWuwaTargetCandidate& OutCandidate) const;

	/**
     * 写入 OwnerOnly 权威状态
     * @param ProcessedCommandSequence	已处理命令序号
     * @param bAccepted			服务端是否接受
     * @param FailureReason		拒绝原因
     * @param bAdvanceContextSequence	是否发生权威上下文变化
     * @return 状态是否有效并进入复制
     */
	bool CommitReplicatedAuthorityState(int32 ProcessedCommandSequence,
	                                    bool bAccepted,
	                                    EWuwaTargetingFailureReason FailureReason,
	                                    bool bAdvanceContextSequence);

	/** 应用最新 OwnerOnly Targeting 裁决 */
	UFUNCTION()
	void OnRep_TargetingAuthorityState();

	/**
     * 将本机预测上下文对账为服务端状态
     * @param AuthorityState	有限权威状态
     * @return 标签、目标绑定和上下文是否全部收敛
     */
	bool ReconcilePredictedTargetContext(const FWuwaReplicatedTargetingAuthorityState& AuthorityState);

	/** @return 是否取得有效视图 */
	bool CaptureViewSnapshot(FWuwaTargetingViewSnapshot& OutView) const;

	void GatherCandidates(const FWuwaTargetingViewSnapshot& View, TArray<FWuwaTargetCandidate>& OutCandidates) const;

	bool EvaluateCandidate(AActor& CandidateActor,
	                       const FWuwaTargetingViewSnapshot& View,
	                       FWuwaTargetCandidate& OutCandidate,
	                       EWuwaTargetingFailureReason& OutFailureReason) const;

	/** @return 候选是否未被遮挡 */
	bool IsCandidateVisible(const AActor& CandidateActor,
	                        const FVector& TargetPoint,
	                        const FWuwaTargetingViewSnapshot& View) const;

	/** @return 当前硬锁目标是否可作为切换原点 */
	bool BuildHardTargetSwitchOriginScore(const FWuwaTargetingViewSnapshot& View,
	                                      FWuwaTargetScoreBreakdown& OutScore) const;

	void CommitSoftTarget(const FWuwaTargetCandidate& Candidate);

	void ClearSoftTarget();

	/**
     * 记录最近一次成功刷新的 Soft Target
     * @return 快照是否完成更新
     */
	bool CacheValidSoftTargetSnapshot();

	/**
     * 取得可供紧邻锁定输入消费的 Soft Target 快照
     * @param OutSnapshot 接收最近有效快照
     * @return 快照目标与时效是否均有效
     */
	bool TryGetRecentSoftTargetSnapshot(FWuwaTargetContext& OutSnapshot) const;

	/** @return 是否释放硬锁标签 */
	bool ReleaseHardLockTag();

	UPROPERTY(Transient)
	FWuwaStateTagHandle HardLockTagHandle;

	/** @return 是否完成目标销毁委托替换 */
	bool BindHardTargetDestroyed(AActor* TargetActor);

	void UnbindHardTargetDestroyed();

	UFUNCTION()
	void HandleHardTargetDestroyed(AActor* DestroyedActor);

	void ValidateCurrentHardTarget();

	void ExitHardLockForValidationFailure(EWuwaTargetingFailureReason FailureReason);

	/** 当前绑定销毁委托的硬锁目标 */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> BoundHardTarget;

	/** 连续遮挡开始时间，负值表示当前未遮挡 */
	double HardTargetOcclusionStartTimeSeconds = -1.0;

	void StartCandidateRefreshTimer();

	void ClearCandidateRefreshTimer();

	void HandleCandidateRefreshTimer();

	FTimerHandle CandidateRefreshTimerHandle;

	/** 清空本组件持有的运行资源 */
	void ResetRuntimeState();

	/** 请求目标选择的 Actor */
	TWeakObjectPtr<AActor> Requester;

	/** 状态标签组件 */
	UPROPERTY(Transient)
	TWeakObjectPtr<UWuwaStateTagComponent> StateTagComponent;

	/** 初始化时冻结的配置 */
	FWuwaTargetingRuntimeConfig RuntimeConfig;

	/** 当前目标上下文 */
	UPROPERTY(Transient)
	FWuwaTargetContext TargetContext;

	/** 最近一次成功刷新的 Soft Target 快照 */
	UPROPERTY(Transient)
	FWuwaTargetContext LastValidSoftTargetSnapshot;

	/** 最近有效 Soft Target 快照的世界时间 */
	double LastValidSoftTargetSnapshotAtSeconds = -1.0;

	/** 服务端只复制给 Owner 的 Targeting 裁决 */
	UPROPERTY(ReplicatedUsing = OnRep_TargetingAuthorityState)
	FWuwaReplicatedTargetingAuthorityState ReplicatedTargetingAuthorityState;

	/** Owning Client 下一条低频命令序号 */
	int32 NextLocalTargetingCommandSequence = 1;

	/** 当前机器最近处理或对账的权威命令序号 */
	int32 LastProcessedAuthorityTargetingCommandSequence = 0;

	UPROPERTY(Transient)
	EWuwaTargetingFailureReason LastFailureReason = EWuwaTargetingFailureReason::NotInitialized;

	bool bInitialized = false;
};
