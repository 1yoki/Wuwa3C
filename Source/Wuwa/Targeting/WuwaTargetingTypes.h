#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Messaging/WuwaMessageTypes.h"
#include "WuwaTargetingTypes.generated.h"

class AActor;

/** 对外 Target Context 当前表达的锁定层级。 */
UENUM(BlueprintType)
enum class EWuwaTargetingMode : uint8
{
	None UMETA(DisplayName = "None"),
	Soft UMETA(DisplayName = "Soft Lock"),
	Hard UMETA(DisplayName = "Hard Lock")
};

/** Targeting 请求或候选验证失败时使用的结构化原因。 */
UENUM(BlueprintType)
enum class EWuwaTargetingFailureReason : uint8
{
	None UMETA(DisplayName = "None"),
	NotInitialized UMETA(DisplayName = "Not Initialized"),
	InvalidProfile UMETA(DisplayName = "Invalid Profile"),
	InvalidRequester UMETA(DisplayName = "Invalid Requester"),
	InvalidView UMETA(DisplayName = "Invalid View"),
	InvalidCandidate UMETA(DisplayName = "Invalid Candidate"),
	StateCommitFailed UMETA(DisplayName = "State Commit Failed"),
	NotTargetable UMETA(DisplayName = "Not Targetable"),
	OutOfRange UMETA(DisplayName = "Out Of Range"),
	OutsideAcquireAngle UMETA(DisplayName = "Outside Acquire Angle"),
	Occluded UMETA(DisplayName = "Occluded"),
	NoCandidate UMETA(DisplayName = "No Candidate"),
	NoHardLock UMETA(DisplayName = "No Hard Lock"),
	InvalidDirection UMETA(DisplayName = "Invalid Direction")
};

/** 客户端可提交给服务端的有限 Targeting 命令 */
UENUM(BlueprintType)
enum class EWuwaTargetingNetworkCommandKind : uint8
{
	/** 进入硬锁 */
	EnterHardLock,

	/** 切换硬锁目标 */
	SwitchHardTarget,

	/** 清除硬锁 */
	ClearHardLock
};

/** Targeting 低频可靠网络命令 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaTargetingNetworkCommand
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Network")
	EWuwaTargetingNetworkCommandKind Kind = EWuwaTargetingNetworkCommandKind::EnterHardLock;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Network")
	int32 CommandSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Network")
	TObjectPtr<AActor> RequestedTarget = nullptr;

	/** @return 命令字段是否完整且可跨网络裁决 */
	bool IsValid() const;
};

/** 服务端只复制给 Owner 的 Targeting 裁决状态 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaReplicatedTargetingAuthorityState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Network")
	int32 ProcessedCommandSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Network")
	bool bAccepted = true;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Network")
	EWuwaTargetingFailureReason FailureReason = EWuwaTargetingFailureReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Network")
	EWuwaTargetingMode Mode = EWuwaTargetingMode::None;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Network")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Network")
	int32 ContextSequence = 0;

	/** @return 裁决字段是否构成有效权威快照 */
	bool IsValid() const;
};

/**
 * 单个候选的可解释评分明细。
 * 可见性是过滤事实而不是连续权重，因此不参与 TotalScore 计算。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaTargetScoreBreakdown
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Score")
	float Distance = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Score")
	float ViewAngleDegrees = 0.f;

	// 相机视空间的水平/垂直投影坐标，供左右切换使用，不依赖 viewport 分辨率。
	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Score")
	float ViewSpaceHorizontal = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Score")
	float ViewSpaceVertical = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Score")
	float DistanceScore = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Score")
	float ViewAlignmentScore = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Score")
	float RetentionBonus = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Score")
	float TotalScore = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Score")
	bool bVisible = false;

	bool IsFinite() const;
};

/** World Query 得到并完成过滤、评分后的候选快照。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaTargetCandidate
{
	GENERATED_BODY()

	// 候选只保存弱引用，评分缓存不能延长目标 Actor 生命周期。
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Targeting|Candidate")
	TWeakObjectPtr<AActor> TargetActor;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Candidate")
	FVector TargetPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Candidate")
	FWuwaTargetScoreBreakdown Score;

	bool IsValid() const;
};

/**
 * Targeting 唯一对外只读状态。
 * Character/Movement、Camera 和 Combat 只能消费该副本，不拥有另一套目标选择状态。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaTargetContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Targeting|Context")
	TWeakObjectPtr<AActor> TargetActor;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Context")
	FVector TargetPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Context")
	EWuwaTargetingMode Mode = EWuwaTargetingMode::None;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Context")
	FWuwaTargetScoreBreakdown Score;

	// 每次目标或模式发生语义变化时递增；目标点逐帧移动不会单独递增。
	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Context")
	int32 Revision = 0;

	bool HasValidTarget() const;
};

/** Toggle/Switch 等语义请求的结构化返回值。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaTargetingResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Result")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Result")
	EWuwaTargetingFailureReason FailureReason = EWuwaTargetingFailureReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Result")
	FWuwaTargetContext Context;
};

/** Targeting 即时命令完成后发布的只读事实 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaTargetingCommandFact
{
	GENERATED_BODY()

	/** 原始输入消息的顺序与来源 */
	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Command")
	FWuwaMessageHeader Header;

	/** 已处理的 Targeting 输入语义 */
	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Command", meta = (Categories = "Input"))
	FGameplayTag InputTag;

	/** Targeting 权威返回的结构化结果 */
	UPROPERTY(BlueprintReadOnly, Category = "Targeting|Command")
	FWuwaTargetingResult Result;
};

/** Targeting 命令事实的原生观察通知 */
DECLARE_MULTICAST_DELEGATE_OneParam(FWuwaTargetingCommandFactNativeSignature, const FWuwaTargetingCommandFact&);

/** 不依赖 World 的评分输入，供 Component 和自动化测试共同使用。 */
struct WUWA_API FWuwaTargetScoreInput
{
	float Distance = 0.f;
	float SearchRadius = 0.f;
	float ViewAngleDegrees = 0.f;
	float MaxAcquireAngleDegrees = 0.f;
	float ViewSpaceHorizontal = 0.f;
	float ViewSpaceVertical = 0.f;
	float DistanceWeight = 0.f;
	float ViewAlignmentWeight = 0.f;
	float RetentionBonus = 0.f;
};

namespace WuwaTargetingRules
{
/** 计算一个已经通过可见性过滤的候选分数。 */
WUWA_API bool CalculateScore(const FWuwaTargetScoreInput& Input, FWuwaTargetScoreBreakdown& OutScore);

/** 从评分数组中稳定选择最高分；完全相同时保持较小索引。 */
WUWA_API int32 SelectBestCandidateIndex(const TArray<FWuwaTargetScoreBreakdown>& CandidateScores);

/**
     * 以当前硬目标为原点选择相对左/右候选。
     * Direction < 0 表示左，Direction > 0 表示右；失败返回 INDEX_NONE。
     * 明确左右候选不存在时，水平重叠目标按角色距离降级：
     * Direction < 0 选择相邻更近层，Direction > 0 选择相邻更远层，不回绕。
     */
WUWA_API int32 SelectSwitchCandidateIndex(const TArray<FWuwaTargetScoreBreakdown>& CandidateScores,
                                          int32 CurrentCandidateIndex,
                                          float Direction,
                                          float MinimumHorizontalDelta,
                                          float VerticalPenaltyWeight,
                                          float DistancePenaltyWeight);
}
