#pragma once

#include "CoreMinimal.h"
#include "Actions/Contracts/WuwaActionTypes.h"
#include "WuwaActionMessages.generated.h"

/** Resolve 阶段输出的不可变 Action 意图 */
USTRUCT()
struct WUWA_API FWuwaResolvedActionIntent
{
	GENERATED_BODY()

	/** 已冻结 Definition 与执行上下文的请求 */
	UPROPERTY()
	FWuwaActionRequest Request;

	/** 严格 FIFO 中的绝对失效时间 */
	UPROPERTY()
	double ExpireAt = 0.0;

	/** 本规则消费的输入标签 */
	UPROPERTY()
	FGameplayTagContainer ConsumedInputTags;

	/** @return 意图是否能够进入严格 FIFO */
	bool IsValid() const
	{
		return Request.IsValid() && ExpireAt >= Request.Header.CreatedAt;
	}

	/**
     * 判断意图是否已经失效
     * @param CurrentTime 当前 World 时间
     * @return 是否失效
     */
	bool IsExpired(const double CurrentTime) const
	{
		return ExpireAt < CurrentTime;
	}
};

/** 单个 Provider 对输入边沿的无副作用解析结果 */
USTRUCT()
struct WUWA_API FWuwaActionIntentResolution
{
	GENERATED_BODY()

	/** Provider 返回的契约状态 */
	UPROPERTY()
	EWuwaActionResolutionStatus Status = EWuwaActionResolutionStatus::NoMatch;

	/** Resolved 状态下唯一有效的不可变意图 */
	UPROPERTY()
	FWuwaResolvedActionIntent Intent;

	/** Rejected 或 Invalid 状态下的可选领域诊断 */
	UPROPERTY()
	FInstancedStruct DiagnosticPayload;
};

/** Dispatcher 在 Finalize 阶段发布的只读解析结果 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaActionIntentResolutionResult
{
	GENERATED_BODY()

	/** 原输入边沿的消息头 */
	UPROPERTY(BlueprintReadOnly, Category = "Action Resolution")
	FWuwaMessageHeader Header;

	/** 本次解析的输入语义 */
	UPROPERTY(BlueprintReadOnly, Category = "Action Resolution", meta = (Categories = "Input"))
	FGameplayTag InputTag;

	/** 认领该输入的 Provider */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Action Resolution")
	TWeakObjectPtr<UObject> ProviderObject;

	/** Provider 的最终契约状态 */
	UPROPERTY(BlueprintReadOnly, Category = "Action Resolution")
	EWuwaActionResolutionStatus Status = EWuwaActionResolutionStatus::NoMatch;

	/** Provider 返回的只读领域诊断 */
	UPROPERTY(BlueprintReadOnly, Category = "Action Resolution")
	FInstancedStruct DiagnosticPayload;
};

/** Action Capability 的无副作用准备命令 */
USTRUCT()
struct WUWA_API FWuwaActionPrepareMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FWuwaActionHandle Handle;

	/** Prepare 通过后将被结束的旧实例，空值表示当前无替换 */
	UPROPERTY()
	FWuwaActionHandle ReplacingHandle;

	UPROPERTY()
	FWuwaActionRequest Request;
};

/** Action Capability 的资源提交命令 */
USTRUCT()
struct WUWA_API FWuwaActionCommitMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FWuwaActionHandle Handle;

	UPROPERTY()
	FWuwaActionRequest Request;
};

/** Action Capability 的统一停止命令 */
USTRUCT()
struct WUWA_API FWuwaActionStopMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FWuwaActionHandle Handle;

	UPROPERTY()
	EWuwaActionEndReason EndReason = EWuwaActionEndReason::None;
};

/** 引擎回调或能力模块上报的 Action 事实 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaActionEventMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Action Event")
	FWuwaMessageHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "Action Event")
	FWuwaActionHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "Action Event", meta = (Categories = "Action.Event"))
	FGameplayTag EventTag;

	/** 事件发生时的可选连续移动输入 */
	UPROPERTY(BlueprintReadOnly, Category = "Action Event")
	FVector2D MoveIntent = FVector2D::ZeroVector;

	/** MovementMode 变化前的模式，非移动事件可保持 None */
	UPROPERTY(BlueprintReadOnly, Category = "Action Event")
	TEnumAsByte<EMovementMode> PreviousMovementMode = MOVE_None;

	/** MovementMode 变化后的模式，非移动事件可保持 None */
	UPROPERTY(BlueprintReadOnly, Category = "Action Event")
	TEnumAsByte<EMovementMode> MovementMode = MOVE_None;

	/** MovementMode 变化前的自定义模式 */
	UPROPERTY(BlueprintReadOnly, Category = "Action Event")
	uint8 PreviousCustomMovementMode = 0;

	/** MovementMode 变化后的自定义模式 */
	UPROPERTY(BlueprintReadOnly, Category = "Action Event")
	uint8 CustomMovementMode = 0;

	/** @return 事件是否引用有效 Action */
	bool IsValid() const
	{
		return Header.IsValid() && Handle.IsValid() && EventTag.IsValid();
	}
};

/** Action 完成全部资源释放后的事实 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaActionFinalizedMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Action")
	FWuwaActionHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "Action")
	FGameplayTag ActionTag;

	UPROPERTY(BlueprintReadOnly, Category = "Action")
	EWuwaActionEndReason EndReason = EWuwaActionEndReason::None;

	/** 触发本次事件驱动结束的事实标签 */
	UPROPERTY(BlueprintReadOnly, Category = "Action")
	FGameplayTag TriggerEventTag;

	/** 触发本次事件驱动结束的二维移动输入 */
	UPROPERTY(BlueprintReadOnly, Category = "Action")
	FVector2D TriggerMoveIntent = FVector2D::ZeroVector;

	/** 跨端对应同一 Action 请求的稳定 Generation */
	UPROPERTY(BlueprintReadOnly, Category = "Action")
	FWuwaNetworkActionGeneration NetworkGeneration;
};

/** Capability Prepare 或 Commit 的诊断结果 */
USTRUCT()
struct WUWA_API FWuwaActionCapabilityResult
{
	GENERATED_BODY()

	/** 操作是否成功 */
	UPROPERTY()
	bool bSucceeded = false;

	/** 失败时返回给 Coordinator 的统一原因 */
	UPROPERTY()
	EWuwaActionRejectionReason RejectionReason = EWuwaActionRejectionReason::CapabilityPrepareFailed;

	/** @return 成功结果 */
	static FWuwaActionCapabilityResult Success()
	{
		FWuwaActionCapabilityResult Result;
		Result.bSucceeded = true;
		Result.RejectionReason = EWuwaActionRejectionReason::None;
		return Result;
	}

	/**
     * 创建失败结果
     * @param Reason 统一拒绝原因
     * @return 失败结果
     */
	static FWuwaActionCapabilityResult Failure(const EWuwaActionRejectionReason Reason)
	{
		FWuwaActionCapabilityResult Result;
		Result.RejectionReason = Reason;
		return Result;
	}
};
