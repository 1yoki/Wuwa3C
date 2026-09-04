#pragma once

#include "CoreMinimal.h"
#include "Actions/Contracts/WuwaActionMessages.h"
#include "Actions/Network/WuwaActionNetworkTypes.h"
#include "Components/ActorComponent.h"
#include "WuwaActionNetworkComponent.generated.h"

class AWuwaCharacter;
class UWuwaActionAnimationCapabilityComponent;
class UWuwaActionCoordinatorComponent;
class UWuwaActionDefinition;
class UWuwaActionRuleSet;
class UWuwaCharacterMovementComponent;
class UWuwaGrappleActionDefinition;
class UWuwaGrappleCapabilityComponent;
class UWuwaMovementActionCapabilityComponent;
class UWuwaMovementActionDefinition;
class UWuwaTraversalProfile;

/** Legacy Movement Action 的预测、服务端裁决与远端表现唯一网络入口 */
UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaActionNetworkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWuwaActionNetworkComponent();

	/**
     * 注入 Action 网络链路的唯一依赖并构建 Registry
     * @param InCharacter			当前组件所属角色
     * @param InCoordinator		本机 Action Coordinator
     * @param InMovement			自定义 CharacterMovement
     * @param InMovementCapability	Movement Action Capability
     * @param InAnimationCapability	Animation Action Capability
     * @param InActionRuleSet		Legacy Movement Action 规则
     * @param InTraversalProfile	可选 Traversal 配置
     * @return 是否完成 Registry、委托和复制依赖装配
     */
	bool Initialize(AWuwaCharacter* InCharacter,
	                UWuwaActionCoordinatorComponent* InCoordinator,
	                UWuwaCharacterMovementComponent* InMovement,
	                UWuwaMovementActionCapabilityComponent* InMovementCapability,
	                UWuwaActionAnimationCapabilityComponent* InAnimationCapability,
	                const UWuwaActionRuleSet* InActionRuleSet,
	                const UWuwaTraversalProfile* InTraversalProfile);

	/**
     * 在 Traversal Runtime 完成装配后绑定唯一 Grapple Capability
     * @param InGrappleCapability	同一角色的 Grapple 事务能力
     * @return 是否完成网络 Grapple 运行依赖绑定
     */
	bool BindGrappleCapability(UWuwaGrappleCapabilityComponent* InGrappleCapability);

	/**
     * 替代 Dispatcher 直提 Coordinator 的唯一运行时入口
     * @param Intent	Provider 已冻结的意图
     * @return 本地预测、Authority 启动或明确拒绝结果
     */
	FWuwaActionResult RouteResolvedIntent(const FWuwaResolvedActionIntent& Intent);

	/**
     * 服务端同步处理 CMC Packed Move 中的一次性命令
     * @param Command	当前有限移动命令
     * @return 当前 Generation 的接受或拒绝响应
     */
	FWuwaNetworkActionResponse ProcessServerMovementCommand(const FWuwaPendingNetworkMovementCommand& Command);

	/** @param Response Owning Client 收到的有限移动响应 */
	void HandleMoveResponse(const FWuwaNetworkActionResponse& Response);

	/** @param Result 本机 Coordinator 已启动的 Action 事实 */
	void HandleAuthorityActionStarted(const FWuwaActionResult& Result);

	/** @param Message 本机 Coordinator 已完成清理的 Action 事实 */
	void HandleAuthorityActionFinalized(const FWuwaActionFinalizedMessage& Message);

	/** 应用服务端复制的只读 Legacy Action 表现 */
	UFUNCTION()
	void HandleRep_LegacyActionPresentation();

	/**
     * 为 Simulated Proxy 播放本地 Registry 中的只读 Action Montage
     * @param Generation		跨端稳定动作序号
     * @param ActionTag		本地 Registry Definition 查询键
     * @param ServerStartTime	服务端同步世界时间中的启动时刻
     * @return 是否开始或保持了唯一远端 Montage
     */
	bool PlayReplicatedActionPresentation(const FWuwaNetworkActionGeneration& Generation,
	                                      const FGameplayTag& ActionTag,
	                                      float ServerStartTime);

	/**
     * 停止匹配 Generation 的只读远端 Montage
     * @param Generation	需要停止的远端表现序号
     * @return 无返回值
     */
	void StopReplicatedActionPresentation(const FWuwaNetworkActionGeneration& Generation);

	/**
     * 取消指定本地预测 Generation 并释放全部 Action 资源
     * @param Generation		需要取消的预测 Generation
     * @param GrappleEndReason	Grapple Movement 使用的网络停止原因
     * @return 是否取消了匹配的本地 Action
     */
	bool CancelPredictedGeneration(const FWuwaNetworkActionGeneration& Generation,
	                               EWuwaGrappleMovementEndReason GrappleEndReason);

	/**
     * 清理死亡、换 Pawn 或 EndPlay 前的全部 Generation 运行态
     * @param GrappleEndReason	Grapple Movement 的明确清理原因
     * @param ActionEndReason	Coordinator 的明确清理原因
     */
	void InvalidateAvatarGenerations(EWuwaGrappleMovementEndReason GrappleEndReason,
	                                 EWuwaActionEndReason ActionEndReason);

	/** @return Legacy Action 网络运行时只读证据 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Network Action")
	FWuwaActionNetworkRuntimeSnapshot GetRuntimeSnapshot() const;

	/**
     * 从本地只读 Registry 查询 Definition
     * @param ActionTag	跨端动作标签
     * @return 唯一 Definition，未注册时为空
     */
	const UWuwaActionDefinition* FindRegisteredDefinition(const FGameplayTag& ActionTag) const;

protected:
	//~ Begin UActorComponent Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

private:
	/** 当前组件所属角色 */
	UPROPERTY(Transient)
	TObjectPtr<AWuwaCharacter> CharacterOwner;

	/** 本机独占 Action 生命周期 */
	UPROPERTY(Transient)
	TObjectPtr<UWuwaActionCoordinatorComponent> Coordinator;

	/** Packed Move 与移动校正入口 */
	UPROPERTY(Transient)
	TObjectPtr<UWuwaCharacterMovementComponent> Movement;

	/** RMS 和 AirJump 本地执行能力 */
	UPROPERTY(Transient)
	TObjectPtr<UWuwaMovementActionCapabilityComponent> MovementCapability;

	/** 本地与 Simulated Proxy Montage 执行能力 */
	UPROPERTY(Transient)
	TObjectPtr<UWuwaActionAnimationCapabilityComponent> AnimationCapability;

	/** Grapple Action 与 CustomMode 的事务能力 */
	UPROPERTY(Transient)
	TObjectPtr<UWuwaGrappleCapabilityComponent> GrappleCapability;

	/** ActionTag 到 Server Definition 的唯一 Registry */
	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UWuwaActionDefinition>> DefinitionRegistry;

	/** 服务端复制给非 Owner 的只读表现状态 */
	UPROPERTY(ReplicatedUsing = HandleRep_LegacyActionPresentation)
	FWuwaReplicatedLegacyActionPresentation LegacyActionPresentation;

	/** 等待本地启动或服务端响应的命令 */
	TMap<int32, FWuwaPendingNetworkMovementCommand> PendingCommandsByGeneration;

	/** 当前本机 Coordinator Action 的跨端 Generation */
	FWuwaNetworkActionGeneration ActiveLocalGeneration;

	/** 当前本机 Coordinator Action 的本地 Handle */
	FWuwaActionHandle ActiveLocalHandle;

	/** 最近服务端处理的 Legacy Generation */
	FWuwaNetworkActionGeneration LastAuthorityGeneration;

	/** Grapple 预测、权威 Query 与校正只读证据 */
	UPROPERTY(Transient)
	FWuwaGrappleNetworkRuntimeSnapshot GrappleNetworkSnapshot;

	/** 服务端已消费的最近 AirJump Generation */
	FWuwaNetworkActionGeneration LastAuthorityAirJumpGeneration;

	/** 服务端已消费的最近 AirCycle Generation */
	int32 LastAuthorityAirCycleGeneration = 0;

	/** 最近完成清理的服务端动作启动序号 */
	FWuwaNetworkActionGeneration LastFinalizedAuthorityActionGeneration;

	/** 生命周期清理期间禁止产生预测取消命令 */
	bool bSuppressPredictedCancelEmission = false;

	/** 同一输入帧已经接受的 Legacy 命令帧号 */
	uint64 LastLegacyRouteFrame = 0;

	/** 最近一次移动响应 */
	FWuwaNetworkActionResponse LastResponse;

	/** Coordinator 启动通知句柄 */
	FDelegateHandle ActionStartedDelegateHandle;

	/** Coordinator 清理通知句柄 */
	FDelegateHandle ActionFinalizedDelegateHandle;

	int32 LocalStartCount = 0;
	int32 AuthorityProcessorCount = 0;
	int32 AcceptedResponseCount = 0;
	int32 RejectedResponseCount = 0;
	int32 AcceptedActionExitResponseCount = 0;
	int32 RejectedActionExitResponseCount = 0;

	/** Avatar 运行态集中失效的累计次数 */
	int32 AvatarInvalidationCount = 0;

	/** 最近一次 Avatar 失效使用的 Action 结束原因 */
	EWuwaActionEndReason LastAvatarInvalidationActionEndReason = EWuwaActionEndReason::None;

	/** 最近一次 Avatar 失效使用的 Grapple 结束原因 */
	EWuwaGrappleMovementEndReason LastAvatarInvalidationGrappleEndReason = EWuwaGrappleMovementEndReason::None;

	/** @return 是否拥有全部本机运行依赖 */
	bool IsInitialized() const;

	/** 解除 CMC 与 Coordinator 的全部唯一绑定 */
	void ShutdownBindings();

	/**
     * 从一个 RuleSet 合并 ActionTag Registry
     * @param RuleSet	需要审计的规则集合
     * @return 是否没有空 Tag、非法 Definition 或同 Tag 冲突
     */
	bool AddRuleSetToRegistry(const UWuwaActionRuleSet* RuleSet);

	/**
     * 构造 Autonomous Proxy 的有限 Legacy 命令
     * @param Intent		已分配 Generation 的本地意图
     * @param OutCommand	接收有限网络命令
     * @return 字段是否满足 Packed Move 契约
     */
	bool BuildLegacyMovementCommand(const FWuwaResolvedActionIntent& Intent,
	                                FWuwaPendingNetworkMovementCommand& OutCommand) const;

	/**
     * 构造只包含受限 Seed 的 Grapple 移动命令
     * @param Intent		客户端已经完成本地 Query 的预测意图
     * @param OutCommand	接收禁止携带完整 Context 的 Grapple 命令
     * @return Seed 是否满足 Packed Move 契约
     */
	bool BuildGrappleMovementCommand(const FWuwaResolvedActionIntent& Intent,
	                                 FWuwaPendingNetworkMovementCommand& OutCommand);

	/**
     * 构造本地移动取消命令
     * @param Message		已完成的预测动作事实
     * @param MoveCancelIntent	触发取消的移动输入
     * @param OutCommand		接收有限取消命令
     * @return 字段是否满足 Packed Move 契约
     */
	/**
     * 从最终事实解析允许进入网络协议的退出类型
     * @param Message 已完成的预测动作事实
     * @param OutExitKind 接收有限退出类型
     * @return 是否由配置事件合法触发
     */
	bool ResolveLegacyActionExitKind(const FWuwaActionFinalizedMessage& Message,
	                                 EWuwaLegacyActionExitKind& OutExitKind) const;

	/**
     * 构造本地通用动作退出命令
     * @param Message 已完成的预测动作事实
     * @param OutCommand 接收有限退出命令
     * @return 字段是否满足 Packed Move 契约
     */
	bool BuildLegacyActionExitCommand(const FWuwaActionFinalizedMessage& Message,
	                                  FWuwaPendingNetworkMovementCommand& OutCommand);

	/**
     * 将预测取消排入下一个 SavedMove
     * @param Message		已完成的预测动作事实
     * @param MoveCancelIntent	触发取消的移动输入
     * @return 是否成功占用命令槽
     */
	/**
     * 将预测退出排入下一个 SavedMove
     * @param Message 已完成的预测动作事实
     * @return 是否成功占用命令槽
     */
	bool QueuePredictedLegacyActionExit(const FWuwaActionFinalizedMessage& Message);

	/**
     * 服务端处理已经通过移动协议校验的取消命令
     * @param Command	当前取消命令
     * @return 接受或拒绝响应
     */
	/**
     * 服务端处理已经通过移动协议校验的退出命令
     * @param Command 当前退出命令
     * @return 接受或拒绝响应
     */
	FWuwaNetworkActionResponse ProcessServerLegacyActionExit(const FWuwaPendingNetworkMovementCommand& Command);

	/**
     * 使用服务器 Definition 与当前 CMC 事实处理 Grapple Seed
     * @param Command	客户端有限 Grapple Seed
     * @return 服务端 Query 接受或拒绝响应
     */
	FWuwaNetworkActionResponse ProcessServerGrappleCommand(const FWuwaPendingNetworkMovementCommand& Command);

	/**
     * 在本地动作提交前排队冻结朝向
     * @param Command	当前动作移动命令
     * @param Definition	本地注册的移动动作定义
     * @return 是否成功提交所需朝向
     */
	bool ApplyLocalActionFacing(const FWuwaPendingNetworkMovementCommand& Command,
	                            const UWuwaMovementActionDefinition* Definition) const;

	/**
     * 使用 N3 DesiredFacingYaw 通道提交 Grapple 起始朝向
     * @param Command	有限 Grapple Seed
     * @return 朝向字段是否一致并成功排队
     */
	bool ApplyLocalGrappleFacing(const FWuwaPendingNetworkMovementCommand& Command) const;

	/**
     * 使用服务端物理事实和 Registry Definition 重建意图
     * @param Command		客户端有限命令
     * @param Definition	服务端 Definition
     * @param OutIntent	接收服务端权威意图
     * @return 上下文是否通过服务端复核
     */
	bool RebuildAuthorityIntent(const FWuwaPendingNetworkMovementCommand& Command,
	                            UWuwaMovementActionDefinition* Definition,
	                            FWuwaResolvedActionIntent& OutIntent) const;

	/**
     * 使用服务端 Query 结果重建不可变 Grapple 意图
     * @param Command	客户端有限 Grapple Seed
     * @param Definition	服务端 Registry Definition
     * @param OutIntent	接收权威 Grapple 意图
     * @param OutContext	接收服务端 Query Context
     * @return Query 与意图契约是否全部通过
     */
	bool RebuildAuthorityGrappleIntent(const FWuwaPendingNetworkMovementCommand& Command,
	                                   UWuwaGrappleActionDefinition* Definition,
	                                   FWuwaResolvedActionIntent& OutIntent,
	                                   FWuwaGrappleActionContext& OutContext);

	/**
     * 对照服务端 Grapple 摘要决定继续预测或交给 CMC 校正
     * @param Command	本地预测 Seed
     * @param Response	服务端接受响应
     * @return 权威摘要是否处于允许继续预测的容差内
     */
	bool ReconcileAcceptedGrapple(const FWuwaPendingNetworkMovementCommand& Command,
	                              const FWuwaNetworkActionResponse& Response);

	/** @param Command 校正重演恢复的一次性 Legacy 命令 */
	void HandlePredictedMoveReplay(const FWuwaPendingNetworkMovementCommand& Command);

	/** @return 当前服务端同步世界时间 */
	float GetSynchronizedServerTime() const;

	/** @param Generation 已经不再等待响应的 Generation */
	void RemovePendingGeneration(const FWuwaNetworkActionGeneration& Generation);
};
