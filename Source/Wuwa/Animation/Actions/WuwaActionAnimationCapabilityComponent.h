#pragma once

#include "CoreMinimal.h"
#include "Actions/Contracts/WuwaActionCapability.h"
#include "Actions/Contracts/WuwaActionPresentationTypes.h"
#include "Components/ActorComponent.h"
#include "WuwaActionAnimationCapabilityComponent.generated.h"

class AWuwaCharacter;
class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class USkeletalMeshComponent;
class UWuwaActionDefinition;
class UWuwaCharacterMessageDispatcherComponent;

/** Montage 播放、结束事实与安全停止的独立 Action 能力 */
UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaActionAnimationCapabilityComponent : public UActorComponent, public IWuwaActionCapability
{
	GENERATED_BODY()

public:
	UWuwaActionAnimationCapabilityComponent();

	/**
     * 注入角色和消息出口
     * @param InCharacter 当前组件所属角色
     * @param InDispatcher 角色消息调度器
     * @return 是否完成初始化
     */
	bool Initialize(AWuwaCharacter* InCharacter, UWuwaCharacterMessageDispatcherComponent* InDispatcher);

	/** @return 是否拥有 Mesh、AnimInstance 与消息出口 */
	bool IsInitialized() const;

	/**
     * 由通用 AnimNotify 发布配置的 Action Event
     * @param EventTag Notify 配置的事件语义
     * @param MeshComp 触发 Notify 的 Mesh
     * @param Animation 触发 Notify 的动画资产
     * @return 是否属于当前 Action 并成功发布
     */
	bool PublishNotifyEvent(const FGameplayTag& EventTag,
	                        const USkeletalMeshComponent* MeshComp,
	                        const UAnimSequenceBase* Animation);

	/**
     * 为 Simulated Proxy 播放不创建 ActionHandle 的只读 Montage
     * @param Generation		跨端稳定动作序号
     * @param Definition		本地 Registry Definition
     * @param ServerStartTime	服务端同步开始时刻
     * @return 是否开始或保持了唯一远端表现
     */
	bool PlayReplicatedPresentation(const FWuwaNetworkActionGeneration& Generation,
	                                UWuwaActionDefinition* Definition,
	                                float ServerStartTime);

	/** @param Generation 需要停止的远端表现 Generation */
	void StopReplicatedPresentation(const FWuwaNetworkActionGeneration& Generation);

	/** @return Simulated Proxy 实际开始远端 Montage 的累计次数 */
	int32 GetReplicatedPresentationStartCount() const
	{
		return ReplicatedPresentationStartCount;
	}

	/** @return 当前是否持有不带 ActionHandle 的远端 Montage */
	bool HasReplicatedPresentation() const;

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

	/** 引擎回调事实只能通过 Dispatcher 上报 */
	UPROPERTY(Transient)
	TWeakObjectPtr<UWuwaCharacterMessageDispatcherComponent> Dispatcher;

	/** 当前动画资源所属 Action */
	UPROPERTY(Transient)
	FWuwaActionHandle ActiveHandle;

	/** 当前动画资源所属 Action 标签 */
	UPROPERTY(Transient)
	FGameplayTag ActiveActionTag;

	/** 当前正在播放或等待清理的 Montage */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	/** 当前 Montage 所属 AnimInstance */
	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> ActiveAnimInstance;

	/** 当前 Animation Spec 的运行时副本 */
	UPROPERTY(Transient)
	FWuwaActionAnimationSpec ActiveSpec;

	/** 当前只读远端表现 Generation */
	FWuwaNetworkActionGeneration ReplicatedPresentationGeneration;

	/** 当前只读远端 Montage */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ReplicatedPresentationMontage;

	/** 当前只读远端 Montage 所属 AnimInstance */
	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> ReplicatedPresentationAnimInstance;

	/** 当前只读远端 Animation Spec */
	UPROPERTY(Transient)
	FWuwaActionAnimationSpec ReplicatedPresentationSpec;

	/** Simulated Proxy 实际开始远端 Montage 的累计次数 */
	int32 ReplicatedPresentationStartCount = 0;

	/** @return 角色 Mesh 当前 AnimInstance */
	UAnimInstance* ResolveAnimInstance() const;

	/** @return Authority Mesh 是否具备不可见时推进 Gameplay Montage 的策略 */
	bool EnsureAuthorityMontageTickPolicy();

	/**
     * 计算 Montage 播放速率
     * @param Message Action 准备或提交请求
     * @param Spec Animation 配置
     * @param OutPlayRate 接收播放速率
     * @return 是否得到有限正值
     */
	static bool
	ResolvePlayRate(const FWuwaActionRequest& Request, const FWuwaActionAnimationSpec& Spec, float& OutPlayRate);

	/** 解除回调并停止仍在播放的本次 Montage */
	void ReleaseActiveAnimation();

	/** 停止不产生 Gameplay Event 的只读远端 Montage */
	void ReleaseReplicatedPresentation();

	/** 当前 Montage 已经结束时只发布事实 */
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);
};
