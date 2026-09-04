#pragma once

#include "CoreMinimal.h"
#include "Actions/Contracts/WuwaActionMessages.h"
#include "Actions/Runtime/WuwaActionQueue.h"
#include "Components/ActorComponent.h"
#include "Core/WuwaStateTagTypes.h"
#include "WuwaActionCoordinatorComponent.generated.h"

class UWuwaStateTagComponent;
class UWuwaActionAbilityInteropComponent;

/** Action 成功启动后的原生通知 */
DECLARE_MULTICAST_DELEGATE_OneParam(FWuwaActionStartedNativeSignature, const FWuwaActionResult&);

/** Action 完成全部资源释放后的原生通知 */
DECLARE_MULTICAST_DELEGATE_OneParam(FWuwaActionFinalizedNativeSignature, const FWuwaActionFinalizedMessage&);

/** Coordinator 唯一拥有的一次独占 Action 运行态 */
USTRUCT()
struct WUWA_API FWuwaActiveActionInstance
{
	GENERATED_BODY()

	/** 本次执行的唯一身份 */
	UPROPERTY()
	FWuwaActionHandle Handle;

	/** 冻结的请求与 Definition 强引用 */
	UPROPERTY()
	FWuwaActionRequest Request;

	/** 已成功 Commit 的能力，按提交顺序保存 */
	UPROPERTY()
	TArray<TObjectPtr<UObject>> CommittedCapabilities;

	/** 本次执行取得的全部状态标签句柄 */
	UPROPERTY()
	TArray<FWuwaStateTagHandle> GrantedTagHandles;

	/** 本次独占执行取得的通用状态标签 Handle */
	UPROPERTY()
	FWuwaStateTagHandle ExclusiveStateTagHandle;

	/** 显式生命周期状态 */
	UPROPERTY()
	EWuwaActionInstanceState State = EWuwaActionInstanceState::None;

	/** @return 是否已经占用独占 Action 槽位 */
	bool IsOccupied() const
	{
		return Handle.IsValid() && Request.IsValid() && State != EWuwaActionInstanceState::None &&
		       State != EWuwaActionInstanceState::Finished;
	}

	/** 仅在外部资源全部释放后清空记录 */
	void ResetAfterCleanup();
};

/** 角色级独占 Action 的严格 FIFO 与生命周期协调器 */
UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaActionCoordinatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWuwaActionCoordinatorComponent();

	/**
     * 注入角色级状态事实
     * @param InStateTagComponent	状态标签组件
     * @return 是否完成初始化
     */
	bool Initialize(UWuwaStateTagComponent* InStateTagComponent);

	/**
     * 注册一种此前不存在的执行能力
     * @param CapabilityObject 实现 IWuwaActionCapability 的对象
     * @return 是否注册成功
     */
	bool RegisterCapability(UObject* CapabilityObject);

	/**
     * 注销未被当前 Action 使用的能力
     * @param CapabilityObject 已注册能力对象
     * @return 是否注销成功
     */
	bool UnregisterCapability(UObject* CapabilityObject);

	/**
     * 将已经解析的唯一意图追加到严格 FIFO
     * @param Intent 不可变 Action 意图
     * @return 入队结果
     */
	FWuwaActionResult EnqueueIntent(const FWuwaResolvedActionIntent& Intent);

	/**
     * 不进入 FIFO，立即裁决服务端已经重建的网络意图
     * @param Intent	服务端权威意图
     * @return 启动或拒绝结果
     */
	FWuwaActionResult StartAuthoritativeIntent(const FWuwaResolvedActionIntent& Intent);

	/**
     * 让非输入模块直接提交已经选定的普通 Action，而无需修改 Resolver 或 Coordinator。
     * 本方法只追加严格 FIFO；调用方仍须在角色消息生命周期的 Exclusive 阶段调用 PumpQueue。
     * @param Header 调用方提供的发送者内有序消息头
     * @param Definition 已选定的行为定义
     * @param Context 调用边沿冻结的执行上下文
     * @return 入队或拒绝结果
     */
	FWuwaActionResult EnqueueAction(const FWuwaMessageHeader& Header,
	                                UWuwaActionDefinition* Definition,
	                                const FWuwaActionContext& Context);

	/** 按严格 FIFO 尝试启动队首请求 */
	void PumpQueue();

	/**
     * 向当前 Action 的全部能力分发有序事实
     * @param Message Action 事件
     * @return 事件是否引用当前 Action
     */
	bool HandleActionEvent(const FWuwaActionEventMessage& Message);

	/**
     * 使用明确原因结束当前 Action
     * @param EndReason 结束原因
     * @return 是否完成结束
     */
	bool FinishCurrent(EWuwaActionEndReason EndReason);

	/**
	 * 清空 FIFO 并以明确原因幂等终止当前 Action
	 *
	 * @param EndReason	终止当前 Action 的原因
	 * @return 无
	 */
	void AbortAllActions(EWuwaActionEndReason EndReason);

	/** @return 是否已完成依赖注入 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Action")
	bool IsInitialized() const;

	/** @return 是否存在正在准备、活动或结束中的 Action */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Action")
	bool HasActiveAction() const
	{
		return ActiveInstance.IsOccupied();
	}

	/** @return 当前只读运行快照 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Action")
	FWuwaActionRuntimeSnapshot GetRuntimeSnapshot() const;

	// 绑定 Action / Ability 互操作组件。
	bool BindAbilityInterop(UWuwaActionAbilityInteropComponent* InAbilityInterop);

	/** Action 成功启动后的通知 */
	FWuwaActionStartedNativeSignature OnActionStarted;

	/** Action 完成全部资源释放后的通知 */
	FWuwaActionFinalizedNativeSignature OnActionFinalized;

protected:
	//~ Begin UActorComponent Interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

private:
	/** 最大严格 FIFO 深度 */
	UPROPERTY(EditDefaultsOnly, Category = "Action Queue", meta = (ClampMin = "1", ClampMax = "32"))
	int32 MaxQueueDepth = 8;

	/** 全部运行状态标签的唯一事实来源 */
	UPROPERTY(Transient)
	TObjectPtr<UWuwaStateTagComponent> StateTagComponent;

	/** Legacy Action 与 GAS Ability 的互操作桥 */
	TWeakObjectPtr<UWuwaActionAbilityInteropComponent> AbilityInterop;

	/** 已解析请求的严格 FIFO */
	UPROPERTY(Transient)
	FWuwaActionQueue Queue;

	/** 当前唯一独占 Action */
	UPROPERTY(Transient)
	FWuwaActiveActionInstance ActiveInstance;

	/** 按标签唯一注册的能力对象 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> RegisteredCapabilities;

	/** 每种 ActionTag 的冷却截止时间 */
	UPROPERTY(Transient)
	TMap<FGameplayTag, double> CooldownExpireAtByAction;

	/** 最近一次请求结果 */
	UPROPERTY(Transient)
	FWuwaActionResult LastResult;

	/** 最近一次结束原因 */
	UPROPERTY(Transient)
	EWuwaActionEndReason LastEndReason = EWuwaActionEndReason::None;

	/** 下一个 Action Handle，零值永不分配 */
	int64 NextActionHandle = 1;

	/** 防止请求处理同步递归 */
	bool bIsPumpingQueue = false;

	/** 防止结束流程同步递归 */
	bool bIsFinishingCurrent = false;

	/** @return 当前 World 时间 */
	double GetActionTime() const;

	/**
     * 验证请求的通用准入条件
     * @param Request 冻结请求
     * @param OutReason 接收拒绝原因
     * @return 是否通过
     */
	bool CanStartBase(const FWuwaActionRequest& Request, EWuwaActionRejectionReason& OutReason) const;

	/**
     * 验证新请求是否可以替换当前 Action
     * @param Request 新请求
     * @param OutReason 接收拒绝原因
     * @return 是否允许
     */
	bool CanInterruptCurrent(const FWuwaActionRequest& Request, EWuwaActionRejectionReason& OutReason) const;

	/**
     * 解析并排序 Definition 需要的全部能力
     * @param Request 冻结请求
     * @param OutCapabilities 接收能力对象
     * @param OutReason 接收拒绝原因
     * @return 是否拥有全部唯一能力
     */
	bool ResolveRequiredCapabilities(const FWuwaActionRequest& Request,
	                                 TArray<UObject*>& OutCapabilities,
	                                 EWuwaActionRejectionReason& OutReason) const;

	/**
     * 尝试提交一条队首意图
     * @param Intent 队首意图
     * @param OutTemporaryFailure 接收失败是否可等待
     * @return 请求结果
     */
	FWuwaActionResult TryStartIntent(const FWuwaResolvedActionIntent& Intent, bool& OutTemporaryFailure);

	/**
     * 结束当前 Action，但不递归驱动 FIFO
     * @param EndReason 结束原因
     * @param TriggerEvent 触发结束的事件事实，外部结束时为空
     * @return 是否完成结束
     */
	bool FinishCurrentInternal(EWuwaActionEndReason EndReason, const FWuwaActionEventMessage* TriggerEvent = nullptr);

	/**
     * 逆序释放状态标签句柄
     * @param Handles 需要释放并清空的句柄
     */
	void ReleaseGrantedTagHandles(TArray<FWuwaStateTagHandle>& Handles);

	/** 释放当前活动实例的通用独占状态标签 Handle */
	void ReleaseExclusiveStateTagHandle();

	/**
     * 判断拒绝原因是否允许队首继续等待
     * @param Reason 拒绝原因
     * @return 是否为瞬时拒绝
     */
	static bool IsTemporaryRejection(EWuwaActionRejectionReason Reason);
};
