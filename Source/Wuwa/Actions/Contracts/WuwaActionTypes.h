#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Messaging/WuwaMessageTypes.h"
#include "Movement/Network/WuwaCharacterNetworkMoveTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "WuwaActionTypes.generated.h"

class UWuwaActionDefinition;

/** Action Resolve 阶段的结果状态 */
UENUM(BlueprintType)
enum class EWuwaActionResolutionStatus : uint8
{
	/** Provider 已生成唯一不可变意图 */
	Resolved,

	/** Provider 没有匹配当前输入与快照 */
	NoMatch,

	/** Provider 已认领输入但领域条件拒绝生成 Action */
	Rejected,

	/** Provider 内部存在多个同优先级候选 */
	Ambiguous,

	/** 输入、配置或返回值违反 Resolve 契约 */
	Invalid
};

/** Action 请求的高层结果 */
UENUM(BlueprintType)
enum class EWuwaActionRequestStatus : uint8
{
	/** Action 已成功启动 */
	Started,

	/** Action 保留在严格 FIFO 中 */
	Buffered,

	/** Action 被永久拒绝 */
	Rejected
};

/** Action 请求被拒绝的明确原因 */
UENUM(BlueprintType)
enum class EWuwaActionRejectionReason : uint8
{
	/** 没有拒绝 */
	None,

	/** Definition 无效 */
	InvalidDefinition,

	/** 缺少必需状态标签 */
	MissingRequiredTag,

	/** 命中阻止状态标签 */
	BlockedByTag,

	/** 当前 Combat Attack 正在活动 */
	BlockedByCombatAttack,

	/** 当前 Pawn 正处于 Combat Stagger 状态 */
	BlockedByCombatStagger,

	/** 当前 Pawn 已进入 Combat Dead 状态 */
	BlockedByCombatDeath,

	/** 优先级不足 */
	Priority,

	/** 双向取消规则不允许替换 */
	CancellationRule,

	/** Action 仍在冷却 */
	Cooldown,

	/** 冻结上下文无效 */
	InvalidContext,

	/** 缺少必需 Capability */
	MissingCapability,

	/** Capability Prepare 失败 */
	CapabilityPrepareFailed,

	/** Capability Commit 失败 */
	CapabilityCommitFailed,

	/** 消息引用了过期 Action Handle */
	StaleActionHandle,

	/** FIFO 已达到容量 */
	QueueFull,

	/** 同一来源序号已经提交，或序号发生倒退 */
	DuplicateRequest,

	/** 当前 SavedMove 的一次性网络命令槽已经占用 */
	NetworkCommandSlotOccupied,

	/** 当前网络阶段不支持该 Action */
	UnsupportedNetworkCommand,

	/** 服务端拒绝了本地预测 Action */
	NetworkRejected
};

/** Action 的统一结束原因 */
UENUM(BlueprintType)
enum class EWuwaActionEndReason : uint8
{
	/** 尚未结束 */
	None,

	/** 正常完成 */
	Completed,

	/** 主动取消 */
	Cancelled,

	/** 被外部事实或更高优先级 Action 中断 */
	Interrupted,

	/** 执行失败 */
	Failed,

	/** Owner 正在销毁 */
	OwnerDestroyed,

	/** Pawn 已进入死亡流程 */
	Death,

	/** Pawn 进入硬直流程 */
	Staggered
};

/** Action 收到指定事件后的通用完成规则 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaActionCompletionRule
{
	GENERATED_BODY()

	/** 触发结束的 Action Event */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Completion", meta = (Categories = "Action.Event"))
	FGameplayTag EventTag;

	/** 事件命中后使用的结束原因 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Completion")
	EWuwaActionEndReason EndReason = EWuwaActionEndReason::Completed;
};

/** Action Instance 的显式生命周期状态 */
UENUM(BlueprintType)
enum class EWuwaActionInstanceState : uint8
{
	/** 尚未进入运行时 */
	None,

	/** 正在执行无副作用 Prepare */
	Preparing,

	/** 已提交并持有运行资源 */
	Active,

	/** 正在按固定顺序释放资源 */
	Finishing,

	/** 已完成清理 */
	Finished
};

/** Action 解析与执行共同使用的移动环境条件 */
UENUM(BlueprintType)
enum class EWuwaMovementActionCondition : uint8
{
	/** 必须接地 */
	Grounded,

	/** 必须处于 Falling */
	Falling,

	/** 必须处于 Falling 或 Grapple */
	Airborne
};

/** 每次 Action 执行的唯一身份 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaActionHandle
{
	GENERATED_BODY()

	/** 当前 Character Action Coordinator 内唯一编号 */
	UPROPERTY(BlueprintReadOnly, Category = "Action")
	int64 Value = 0;

	/** @return Handle 是否可以用于运行时消息 */
	bool IsValid() const
	{
		return Value > 0;
	}

	/** @return 两个 Handle 是否引用同一次执行 */
	bool operator==(const FWuwaActionHandle& Other) const
	{
		return Value == Other.Value;
	}

	/** @return 两个 Handle 是否引用不同执行 */
	bool operator!=(const FWuwaActionHandle& Other) const
	{
		return !(*this == Other);
	}
};

/** 输入边沿冻结的 Action 上下文 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaActionContext
{
	GENERATED_BODY()

	/** WASD 输入边沿快照 */
	UPROPERTY(BlueprintReadOnly, Category = "Action Context")
	FVector2D InputDirection = FVector2D::ZeroVector;

	/** 解析规则得到的世界方向 */
	UPROPERTY(BlueprintReadOnly, Category = "Action Context")
	FVector WorldDirection = FVector::ZeroVector;

	/** 触发时角色朝向 */
	UPROPERTY(BlueprintReadOnly, Category = "Action Context")
	FVector FacingDirection = FVector::ForwardVector;

	/** 触发时 MovementMode */
	UPROPERTY(BlueprintReadOnly, Category = "Action Context")
	TEnumAsByte<EMovementMode> MovementMode = MOVE_None;

	/** 触发时 CustomMovementMode */
	UPROPERTY(BlueprintReadOnly, Category = "Action Context")
	uint8 CustomMovementMode = 0;

	/** 来源对象使用弱引用 */
	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> SourceObject;

	/** 可选目标使用弱引用 */
	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> TargetObject;

	/** 领域 Definition 自行验证和读取的强类型值载荷 */
	UPROPERTY()
	FInstancedStruct DomainPayload;
};

/** Resolve 阶段使用的只读角色快照 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaActionResolutionSnapshot
{
	GENERATED_BODY()

	/** 角色水平朝向 */
	UPROPERTY(BlueprintReadOnly, Category = "Action Resolution")
	FVector FacingDirection = FVector::ForwardVector;

	/** 观察方向，用于解释二维移动输入 */
	UPROPERTY(BlueprintReadOnly, Category = "Action Resolution")
	FRotator ViewRotation = FRotator::ZeroRotator;

	/** 采样时的 MovementMode */
	UPROPERTY(BlueprintReadOnly, Category = "Action Resolution")
	TEnumAsByte<EMovementMode> MovementMode = MOVE_None;

	/** 采样时的 CustomMovementMode */
	UPROPERTY(BlueprintReadOnly, Category = "Action Resolution")
	uint8 CustomMovementMode = 0;

	/** 是否处于可执行 Grounded Action 的状态 */
	UPROPERTY(BlueprintReadOnly, Category = "Action Resolution")
	bool bIsGrounded = false;

	/** 是否处于可执行 Airborne Action 的状态 */
	UPROPERTY(BlueprintReadOnly, Category = "Action Resolution")
	bool bIsAirborne = false;

	/** 冻结 Action Context 时使用的角色来源 */
	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> SourceObject;

	/** @return 快照是否具有可用朝向 */
	bool IsValid() const
	{
		const bool bFacingFinite = FMath::IsFinite(FacingDirection.X) && FMath::IsFinite(FacingDirection.Y) &&
		                           FMath::IsFinite(FacingDirection.Z);
		return bFacingFinite && !FacingDirection.IsNearlyZero() && !ViewRotation.ContainsNaN() &&
		       SourceObject.IsValid();
	}
};

/** 已解析完成且不会在 FIFO 中重新解释的 Action 请求 */
USTRUCT()
struct WUWA_API FWuwaActionRequest
{
	GENERATED_BODY()

	/** 原始输入或系统调用的消息头 */
	UPROPERTY()
	FWuwaMessageHeader Header;

	/** Definition 在请求与运行期间保持强引用 */
	UPROPERTY()
	TObjectPtr<UWuwaActionDefinition> Definition = nullptr;

	/** 输入边沿冻结的执行上下文 */
	UPROPERTY()
	FWuwaActionContext Context;

	/** 跨端对应同一 Action 请求的稳定 Generation */
	UPROPERTY()
	FWuwaNetworkActionGeneration NetworkGeneration;

	/** @return 请求是否包含有效消息头和 Definition */
	bool IsValid() const
	{
		return Header.IsValid() && Definition != nullptr;
	}
};

/** Gameplay、日志和 Debug 共用的 Action 请求结果 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaActionResult
{
	GENERATED_BODY()

	/** 请求最终状态 */
	UPROPERTY(BlueprintReadOnly, Category = "Action Result")
	EWuwaActionRequestStatus Status = EWuwaActionRequestStatus::Rejected;

	/** 拒绝原因 */
	UPROPERTY(BlueprintReadOnly, Category = "Action Result")
	EWuwaActionRejectionReason RejectionReason = EWuwaActionRejectionReason::InvalidDefinition;

	/** 请求对应的 ActionTag */
	UPROPERTY(BlueprintReadOnly, Category = "Action Result")
	FGameplayTag ActionTag;

	/** 成功开始时分配的唯一 Handle */
	UPROPERTY(BlueprintReadOnly, Category = "Action Result")
	FWuwaActionHandle ActionHandle;

	/** 来源消息序号 */
	UPROPERTY(BlueprintReadOnly, Category = "Action Result")
	int32 SourceSequence = 0;

	/** 跨端对应同一 Action 请求的稳定 Generation */
	UPROPERTY(BlueprintReadOnly, Category = "Action Result")
	FWuwaNetworkActionGeneration NetworkGeneration;

	/** @return Action 是否已经成功启动 */
	bool HasStarted() const
	{
		return Status == EWuwaActionRequestStatus::Started;
	}
};

/** Action Coordinator 提供给表现与调试层的只读快照 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaActionRuntimeSnapshot
{
	GENERATED_BODY()

	/** 当前活动 Action Handle */
	UPROPERTY(BlueprintReadOnly, Category = "Action")
	FWuwaActionHandle ActiveHandle;

	/** 当前活动 ActionTag */
	UPROPERTY(BlueprintReadOnly, Category = "Action")
	FGameplayTag ActiveActionTag;

	/** 当前活动 Action 的跨端 Generation */
	UPROPERTY(BlueprintReadOnly, Category = "Action")
	FWuwaNetworkActionGeneration ActiveNetworkGeneration;

	/** 当前 Action 生命周期状态 */
	UPROPERTY(BlueprintReadOnly, Category = "Action")
	EWuwaActionInstanceState InstanceState = EWuwaActionInstanceState::None;

	/** 当前 Action 已成功提交的 CapabilityTag */
	UPROPERTY(BlueprintReadOnly, Category = "Action")
	FGameplayTagContainer ActiveCapabilityTags;

	/** 当前活动实例是否持有通用独占状态标签 */
	UPROPERTY(BlueprintReadOnly, Category = "Action")
	bool bOwnsExclusiveStateTag = false;

	/** 严格 FIFO 当前条目数 */
	UPROPERTY(BlueprintReadOnly, Category = "Action")
	int32 QueueCount = 0;

	/** 最近一次请求结果 */
	UPROPERTY(BlueprintReadOnly, Category = "Action")
	FWuwaActionResult LastResult;

	/** 最近一次结束原因 */
	UPROPERTY(BlueprintReadOnly, Category = "Action")
	EWuwaActionEndReason LastEndReason = EWuwaActionEndReason::None;
};

FORCEINLINE uint32 GetTypeHash(const FWuwaActionHandle& Handle)
{
	return GetTypeHash(Handle.Value);
}
