#pragma once

#include "CoreMinimal.h"
#include "Actions/Contracts/WuwaActionCapability.h"
#include "Components/ActorComponent.h"
#include "Traversal/Contracts/WuwaTraversalTypes.h"
#include "WuwaGrappleCapabilityComponent.generated.h"

class AWuwaCharacter;
class UWuwaCharacterMessageDispatcherComponent;
class UWuwaCharacterMovementComponent;
class UWuwaGrappleActionDefinition;

/** Grapple Capability 对外提供的只读 Action/Movement 映射 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaGrappleRuntimeSnapshot
{
	GENERATED_BODY()

	/** 当前 Grapple ActionHandle */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Runtime")
	FWuwaActionHandle ActionHandle;

	/** 当前 Grapple Action 的跨端稳定序号 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Runtime")
	FWuwaNetworkActionGeneration NetworkGeneration;

	/** 当前 Grapple MovementHandle */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Runtime")
	FWuwaGrappleMovementHandle MovementHandle;

	/** 当前 Gameplay 阶段 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Runtime")
	EWuwaGrappleRuntimePhase Phase = EWuwaGrappleRuntimePhase::None;

	/** 最近一次 Movement 结束原因 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Runtime")
	EWuwaGrappleMovementEndReason LastMovementEndReason = EWuwaGrappleMovementEndReason::None;

	/** 正常释放时的最终出口速度 */
	UPROPERTY(BlueprintReadOnly, Category = "Grapple Runtime")
	FVector ExitVelocity = FVector::ZeroVector;
};

/** Grapple Action 与 CharacterMovement 原语之间的事务桥接 */
UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaGrappleCapabilityComponent : public UActorComponent, public IWuwaActionCapability
{
	GENERATED_BODY()

public:
	UWuwaGrappleCapabilityComponent();

	/**
     * 注入同一角色的 Movement 与消息出口
     * @param InCharacter	当前组件所属角色
     * @param InMovementComponent	唯一 Capsule 移动权威
     * @param InDispatcher	Action Event 消息出口
     * @return 是否完成初始化
     */
	bool Initialize(AWuwaCharacter* InCharacter,
	                UWuwaCharacterMovementComponent* InMovementComponent,
	                UWuwaCharacterMessageDispatcherComponent* InDispatcher);

	/** @return 是否持有全部同 Owner 依赖 */
	bool IsInitialized() const;

	/** @return 当前 Grapple Action 与 Movement 的只读映射 */
	const FWuwaGrappleRuntimeSnapshot& GetRuntimeSnapshot() const
	{
		return RuntimeSnapshot;
	}

	/**
     * 读取当前实例冻结的表现数据
     * @param Handle	目标 ActionHandle
     * @param OutDefinition	当前 Grapple Definition
     * @param OutContext	Commit 复核后的冻结领域上下文
     * @return Handle 匹配且数据完整时返回真
     */
	bool GetActivePresentationData(const FWuwaActionHandle& Handle,
	                               const UWuwaGrappleActionDefinition*& OutDefinition,
	                               FWuwaGrappleActionContext& OutContext) const;

	/**
     * 读取指定预测 Generation 当前使用的 Grapple Context
     * @param Generation	目标跨端动作序号
     * @param OutContext	接收当前 Commit 后 Context
     * @return 当前活动请求是否与 Generation 精确匹配
     */
	bool GetActiveNetworkContext(const FWuwaNetworkActionGeneration& Generation,
	                             FWuwaGrappleActionContext& OutContext) const;

	/**
     * 在 Coordinator 收口前以明确原因停止当前 Grapple Movement
     * @param Generation	目标跨端动作序号
     * @param EndReason	网络拒绝、网络校正或 Avatar 生命周期原因
     * @return 是否停止了精确匹配的 Movement
     */
	bool StopActiveGrapple(const FWuwaNetworkActionGeneration& Generation, EWuwaGrappleMovementEndReason EndReason);

	//~ Begin IWuwaActionCapability Interface
	virtual FGameplayTag GetActionCapabilityTag() const override;
	virtual int32 GetActionCapabilityCommitOrder() const override;
	virtual FWuwaActionCapabilityResult PrepareAction(const FWuwaActionPrepareMessage& Message) const override;
	virtual FWuwaActionCapabilityResult CommitAction(const FWuwaActionCommitMessage& Message) override;
	virtual void RollbackAction(const FWuwaActionHandle& Handle) override;
	virtual void StopAction(const FWuwaActionStopMessage& Message) override;
	virtual EWuwaActionEndReason HandleActionEvent(const FWuwaActionEventMessage& Message) override;
	virtual void HandleActionFinalized(const FWuwaActionFinalizedMessage& Message) override;
	//~ End IWuwaActionCapability Interface

protected:
	//~ Begin UActorComponent Interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

private:
	/** 当前组件所属角色 */
	UPROPERTY(Transient)
	TObjectPtr<AWuwaCharacter> CharacterOwner;

	/** Grapple CustomMode 和 MovementHandle 权威 */
	UPROPERTY(Transient)
	TObjectPtr<UWuwaCharacterMovementComponent> MovementComponent;

	/** Movement 事实转换成 Action Event 的唯一出口 */
	UPROPERTY(Transient)
	TWeakObjectPtr<UWuwaCharacterMessageDispatcherComponent> Dispatcher;

	/** 当前能力持有的 ActionHandle */
	UPROPERTY(Transient)
	FWuwaActionHandle ActiveActionHandle;

	/** 当前 Action 对应的独立 MovementHandle */
	UPROPERTY(Transient)
	FWuwaGrappleMovementHandle ActiveMovementHandle;

	/** 当前 Grapple Definition 强引用 */
	UPROPERTY(Transient)
	TObjectPtr<UWuwaGrappleActionDefinition> ActiveDefinition;

	/** 当前 Grapple 的冻结 Action 请求 */
	UPROPERTY(Transient)
	FWuwaActionRequest ActiveRequest;

	/** Stop 后等待 Finalized 的 ActionHandle */
	UPROPERTY(Transient)
	FWuwaActionHandle FinalizingActionHandle;

	/** 表现与 Debug 只读消费的运行快照 */
	UPROPERTY(Transient)
	FWuwaGrappleRuntimeSnapshot RuntimeSnapshot;

	/**
     * 验证 Definition、DomainPayload、物理状态和 Commit 起点
     * @param Message	Prepare 消息
     * @return 准备结果
     */
	FWuwaActionCapabilityResult ValidatePrepare(const FWuwaActionPrepareMessage& Message) const;

	/** 清空已经释放 Movement 资源的活动记录 */
	void ResetActiveRuntime();

	/** @param Fact CharacterMovement 已提交的阶段或结束事实 */
	void HandleGrappleMovementFact(const FWuwaGrappleMovementFact& Fact);
};
