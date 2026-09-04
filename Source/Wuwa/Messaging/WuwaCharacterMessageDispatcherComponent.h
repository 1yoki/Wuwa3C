#pragma once

#include "CoreMinimal.h"
#include "Actions/Contracts/WuwaActionIntentProvider.h"
#include "Actions/Contracts/WuwaActionMessages.h"
#include "Components/ActorComponent.h"
#include "Input/WuwaInputTypes.h"
#include "WuwaCharacterMessageDispatcherComponent.generated.h"

class UWuwaActionCoordinatorComponent;
class UWuwaActionNetworkComponent;
class UWuwaAbilityInputRouterComponent;

/** Intent Resolution 进入 Finalize 后的原生通知 */
DECLARE_MULTICAST_DELEGATE_OneParam(FWuwaActionIntentResolutionFinalizedNativeSignature,
                                    const FWuwaActionIntentResolutionResult&);

/** Action Event 成功进入有序事实队列后的只读通知 */
DECLARE_MULTICAST_DELEGATE_OneParam(FWuwaActionEventPublishedNativeSignature, const FWuwaActionEventMessage&);

/** 角色范围内唯一的有序消息入口与阶段调度器 */
UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaCharacterMessageDispatcherComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWuwaCharacterMessageDispatcherComponent();

	/**
     * 注入独占 Action 协调器
     * @param InActionCoordinator 角色级 Action Coordinator
     * @return 是否完成初始化
     */
	bool Initialize(UWuwaActionCoordinatorComponent* InActionCoordinator);

	/**
     * 注入 Exclusive 阶段的唯一网络动作入口
     * @param InActionNetworkComponent Character 持有的 Action Network Component
     * @return 是否完成初始化
     */
	bool InitializeActionNetwork(UWuwaActionNetworkComponent* InActionNetworkComponent);

	/**
     * 注入 Exclusive 阶段之后执行的 Ability 输入路由
     * @param InAbilityInputRouter Character 持有的唯一 Ability 输入路由
     * @return 是否完成初始化
     */
	bool InitializeAbilityInputRouter(UWuwaAbilityInputRouterComponent* InAbilityInputRouter);

	/**
     * 注册输入标签唯一的 Action Intent Provider
     * @param ProviderObject	实现 IWuwaActionIntentProvider 的对象
     * @return 是否注册成功
     */
	bool RegisterIntentProvider(UObject* ProviderObject);

	/**
     * 注销已注册的 Action Intent Provider
     * @param ProviderObject	已注册 Provider
     * @return 是否注销成功
     */
	bool UnregisterIntentProvider(UObject* ProviderObject);

	/**
     * 注册即时 Locomotion、Targeting 或 Camera 命令处理者
     * @param HandlerObject 实现 IWuwaCharacterMessageHandler 的对象
     * @return 是否注册成功
     */
	bool RegisterImmediateHandler(UObject* HandlerObject);

	/**
     * 注销即时命令处理者
     * @param HandlerObject 已注册处理者
     * @return 是否注销成功
     */
	bool UnregisterImmediateHandler(UObject* HandlerObject);

	/** 在连续输入和解析快照更新前清理已发生的引擎事实 */
	void BeginInputFrame();

	/**
     * 按固定阶段处理 PlayerController 提交的一帧输入
     * @param InputFrame 完整输入帧
     * @param ResolutionSnapshot Resolve 阶段只读角色快照
     */
	void ProcessInputFrame(const FWuwaInputFrame& InputFrame, const FWuwaActionResolutionSnapshot& ResolutionSnapshot);

	/**
     * 将引擎回调转换为待排序的 Action 事实
     * @param Handle 事件所属 Action
     * @param EventTag 事件语义
     * @param MoveIntent 可选连续移动输入
     * @param PreviousMovementMode 变化前移动模式
     * @param MovementMode 变化后移动模式
     * @param PreviousCustomMovementMode 变化前自定义模式
     * @param CustomMovementMode 变化后自定义模式
     * @param SourceObject 事实来源
     * @return 是否成功发布
     */
	bool PublishActionEvent(const FWuwaActionHandle& Handle,
	                        const FGameplayTag& EventTag,
	                        const FVector2D& MoveIntent = FVector2D::ZeroVector,
	                        EMovementMode PreviousMovementMode = MOVE_None,
	                        EMovementMode MovementMode = MOVE_None,
	                        uint8 PreviousCustomMovementMode = 0,
	                        uint8 CustomMovementMode = 0,
	                        UObject* SourceObject = nullptr);

	/** @return 最近一次提交的连续移动输入 */
	const FVector2D& GetCurrentMoveIntent() const
	{
		return CurrentMoveIntent;
	}

	/** @return 最近一次进入 Finalize 的 Provider 解析结果 */
	const FWuwaActionIntentResolutionResult& GetLastIntentResolutionResult() const
	{
		return LastIntentResolutionResult;
	}

	/** Provider 解析结果进入 Finalize 后的通知 */
	FWuwaActionIntentResolutionFinalizedNativeSignature OnIntentResolutionFinalized;

	/** Action Event 成功进入事实队列后的只读通知 */
	FWuwaActionEventPublishedNativeSignature OnActionEventPublished;

protected:
	//~ Begin UActorComponent Interface
	virtual void
	TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

private:
	/** 独占 Action 消息的唯一消费端 */
	UPROPERTY(Transient)
	TObjectPtr<UWuwaActionCoordinatorComponent> ActionCoordinator;

	/** Exclusive 阶段的唯一动作路由 */
	UPROPERTY(Transient)
	TWeakObjectPtr<UWuwaActionNetworkComponent> ActionNetworkComponent;

	/** Exclusive 阶段之后执行的 Ability 输入路由 */
	UPROPERTY(Transient)
	TWeakObjectPtr<UWuwaAbilityInputRouterComponent> AbilityInputRouter;

	/** 按 exact InputTag 唯一注册的 Action Intent Provider */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> IntentProviders;

	/** 每个输入标签只允许一个即时处理者 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> ImmediateHandlers;

	/** 引擎回调等待 FactDrain 阶段处理的事实 */
	UPROPERTY(Transient)
	TArray<FWuwaActionEventMessage> PendingActionFacts;

	/** 等待 Finalize 阶段发布的 Provider 解析结果 */
	UPROPERTY(Transient)
	TArray<FWuwaActionIntentResolutionResult> PendingIntentResolutionResults;

	/** 最近一次进入 Finalize 的 Provider 解析结果 */
	UPROPERTY(Transient)
	FWuwaActionIntentResolutionResult LastIntentResolutionResult;

	/** 最近一帧连续移动输入 */
	FVector2D CurrentMoveIntent = FVector2D::ZeroVector;

	/** Dispatcher 自己发布事实时使用的单调序号 */
	int32 NextFactSequence = 1;

	/** 防止事实分发回调同步递归 */
	bool bIsDrainingFacts = false;

	/** 按消息序号处理当前全部事实 */
	void DrainPendingFacts();

	/** 按产生顺序发布当前帧的 Provider 解析结果 */
	void DrainIntentResolutionResults();

	/**
     * 查找 exact InputTag 的唯一 Provider
     * @param InputTag	输入语义
     * @return 已注册 Provider，未认领时返回空
     */
	UObject* FindIntentProvider(const FGameplayTag& InputTag) const;

	/**
     * 记录等待 Finalize 发布的 Provider 解析结果
     * @param Command	原输入边沿
     * @param ProviderObject	认领输入的 Provider
     * @param Resolution	Provider 返回结果
     */
	void QueueIntentResolutionResult(const FWuwaInputCommand& Command,
	                                 UObject* ProviderObject,
	                                 const FWuwaActionIntentResolution& Resolution);

	/** 将最新连续移动输入作为 Action 事实发送 */
	void DispatchContinuousMoveIntent();

	/**
     * 将未被 Action 规则消费的命令发送给唯一即时处理者
     * @param Command 输入边沿命令
     * @param InputFrame 完整输入帧
     * @return 分发结果
     */
	FWuwaCommandDispatchResult DispatchImmediateCommand(const FWuwaInputCommand& Command,
	                                                    const FWuwaInputFrame& InputFrame);

	/**
     * 创建 Dispatcher 来源的消息头
     * @param SourceObject 事实来源
     * @return 带帧号、时间与单调序号的消息头
     */
	FWuwaMessageHeader MakeFactHeader(UObject* SourceObject);
};
