#pragma once

#include "CoreMinimal.h"
#include "Actions/Contracts/WuwaActionCapability.h"
#include "Components/ActorComponent.h"
#include "Movement/WuwaMovementTypes.h"
#include "Movement/Actions/WuwaMovementActionTypes.h"
#include "Movement/Network/WuwaCharacterNetworkMoveTypes.h"
#include "Movement/Network/WuwaMovementActionNetworkReplayState.h"
#include "WuwaMovementActionCapabilityComponent.generated.h"

class AWuwaCharacter;
class UWuwaCharacterMessageDispatcherComponent;
class UWuwaCharacterMovementComponent;
class UWuwaMovementActionDefinition;
struct FRootMotionSource;

/** CharacterMovement 原语、RMS 与朝向覆盖的统一 Action 能力 */
UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaMovementActionCapabilityComponent : public UActorComponent, public IWuwaActionCapability
{
	GENERATED_BODY()

public:
	UWuwaMovementActionCapabilityComponent();

	/**
     * 注入角色、移动组件与消息出口
     * @param InCharacter 当前组件所属角色
     * @param InMovementComponent 自定义 CharacterMovement
     * @param InDispatcher 角色消息调度器
     * @return 是否完成初始化
     */
	bool Initialize(AWuwaCharacter* InCharacter,
	                UWuwaCharacterMovementComponent* InMovementComponent,
	                UWuwaCharacterMessageDispatcherComponent* InDispatcher);

	/** @return 是否拥有全部运行依赖 */
	bool IsInitialized() const;

	/**
     * 按网络 Generation 注入 RMS 路线验证源
     * @param Generation 跨端稳定动作序号
     * @param ActionTag 动作定义标签
     * @param WorldDirection 冻结的平面世界方向
     * @param Config RMS 位移配置
     * @return 是否取得唯一 RMS 资源
     */
	bool ApplyNetworkRootMotionSourceSpike(const FWuwaNetworkActionGeneration& Generation,
	                                       const FGameplayTag& ActionTag,
	                                       const FVector& WorldDirection,
	                                       const FWuwaRootMotionSourceConfig& Config);

	/**
     * 释放 RMS 路线验证源
     * @param Generation 跨端稳定动作序号
     * @param ActionTag 动作定义标签
     */
	void ReleaseNetworkRootMotionSourceSpike(const FWuwaNetworkActionGeneration& Generation,
	                                         const FGameplayTag& ActionTag);

	/**
     * 构建跨机器一致的 RMS 名称
     * @param Generation 跨端稳定动作序号
     * @param ActionTag 动作定义标签
     * @return WuwaActionNet_Generation_ActionTagHash
     */
	static FName BuildNetworkRootMotionSourceName(const FWuwaNetworkActionGeneration& Generation,
	                                              const FGameplayTag& ActionTag);

	/**
     * 查询指定动作最近实际写入 RMS FinishVelocity 的退出目标
     * @param ActionGeneration	待查询动作的跨端序号
     * @param OutVelocity		接收世界空间退出目标速度
     * @return 是否存在同 Generation 的有限退出目标
     */
	bool GetLastResolvedRootMotionReleaseVelocity(const FWuwaNetworkActionGeneration& ActionGeneration,
	                                              FVector& OutVelocity) const;

	/** @return 当前动作是否配置移动取消窗口 */
	/**
     * 检查当前 Definition 是否配置指定退出响应
     * @param ExitKind 有限退出类型
     * @return 是否存在匹配的事件响应
     */
	bool HasConfiguredActionExit(EWuwaLegacyActionExitKind ExitKind) const;

	/**
     * 在当前 Authority Move 内准备网络动作退出
     * @param Command		当前 Move 携带的退出命令
     * @param OutEndReason	接收 Coordinator 结束原因
     * @return 命令是否匹配当前物理动作并完成准备
     */
	bool PrepareNetworkActionExitTransition(const FWuwaPendingNetworkMovementCommand& Command,
	                                        EWuwaActionEndReason& OutEndReason);

	/**
     * 在 CharacterMovement Move 边界原子应用动作物理退出
     * @param Command	当前 Move 携带的退出命令
     * @param bIsReplay	是否来自校正后的历史 Move 重演
     * @return 是否达到命令指定的物理结束状态
     */
	bool ApplyNetworkActionExitTransition(const FWuwaPendingNetworkMovementCommand& Command, bool bIsReplay);

	/**
     * 校验 Owning Client 待生效 RMS 保持原生 Move 起点
     * @param Command		当前 SavedMove 捕获的启动命令
     * @param MoveStartTime	承载启动命令的 Move 起点时间
     * @return 命令无需校验或 RMS 时间相位有效
     */
	bool ValidateLocalPredictedRootMotionMoveStart(const FWuwaPendingNetworkMovementCommand& Command,
	                                               float MoveStartTime) const;

	/**
     * 将 Authority 新建 RMS 对齐到远端客户端 Move 起点
     * @param Command		当前 Authority 已接受的启动命令
     * @param MoveStartTime	承载启动命令的 Move 起点时间
     * @return 命令无需对齐或 RMS 已完成时间相位对齐
     */
	bool AlignAuthorityRootMotionMoveStart(const FWuwaPendingNetworkMovementCommand& Command, float MoveStartTime);

	/**
     * 捕获当前普通 MovementAction 的最小物理状态
     * @param OutState	接收不含 UObject 与本机资源身份的状态
     */
	void CaptureNetworkReplayState(FWuwaMovementActionNetworkReplayState& OutState) const;

	/**
     * 在 SavedMove 重演前恢复普通 MovementAction 物理状态
     * @param State	当前历史 Move 捕获的状态
     */
	void RestoreNetworkReplayState(const FWuwaMovementActionNetworkReplayState& State);

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

	/** 唯一 Capsule 移动权威 */
	UPROPERTY(Transient)
	TObjectPtr<UWuwaCharacterMovementComponent> MovementComponent;

	/** 引擎事实只能通过 Dispatcher 上报 */
	UPROPERTY(Transient)
	TWeakObjectPtr<UWuwaCharacterMessageDispatcherComponent> Dispatcher;

	/** 当前能力持有的 Action Handle */
	UPROPERTY(Transient)
	FWuwaActionHandle ActiveHandle;

	/** 当前 Movement 策略的强引用 */
	UPROPERTY(Transient)
	TObjectPtr<UWuwaMovementActionDefinition> ActiveDefinition;

	/** 当前 Movement Action 的冻结请求 */
	UPROPERTY(Transient)
	FWuwaActionRequest ActiveRequest;

	/** ApplyRootMotionSource 返回的唯一 ID */
	uint16 ActiveRootMotionSourceId = 0;

	/** 当前 RMS 的跨 Replay 稳定实例名称 */
	FName ActiveRootMotionSourceInstanceName = NAME_None;

	/** 与 Gameplay 生命周期独立的 MovementAction 物理状态 */
	FWuwaMovementActionNetworkReplayState PhysicalRuntime;

	/** 最近成功写入 RMS FinishVelocity 的动作序号 */
	FWuwaNetworkActionGeneration LastResolvedRootMotionReleaseGeneration;

	/** 最近成功写入 RMS FinishVelocity 的世界空间速度 */
	FVector LastResolvedRootMotionReleaseVelocity = FVector::ZeroVector;

	/** 由物理生命周期持有的 MovementAction 退出配置 */
	UPROPERTY(Transient)
	TObjectPtr<UWuwaMovementActionDefinition> PhysicalActionDefinition;

	/** 数据事件已经打开移动取消窗口 */
	bool bMoveCancelWindowOpen = false;

	/** 数据事件已经打开正常完成出口 */
	bool bCompletedExitReady = false;

	/** 当前 Authority Move 已准备的退出序号 */
	FWuwaNetworkActionGeneration PreparedNetworkExitGeneration;

	/** MovementMode 委托句柄 */
	FDelegateHandle MovementModeChangedDelegateHandle;

	/**
     * 验证 Movement Definition 与当前物理事实
     * @param Message 准备消息
     * @return 准备结果
     */
	FWuwaActionCapabilityResult ValidatePrepare(const FWuwaActionPrepareMessage& Message) const;

	/**
     * 创建 Definition 配置的唯一移动驱动
     * @param Message Action 提交消息
     * @return 是否提交成功
     */
	bool CommitMovementDriver(const FWuwaActionCommitMessage& Message);

	/**
     * 创建水平 RMS
     * @param Message Action 提交消息
     * @param Definition Movement Definition
     * @return 是否取得 RMS 资源
     */
	bool ApplyRootMotionSource(const FWuwaActionCommitMessage& Message,
	                           const UWuwaMovementActionDefinition& Definition);

	/**
     * 使用稳定名称创建水平 RMS
     * @param InstanceName RMS 实例名称
     * @param WorldDirection 冻结的平面世界方向
     * @param Config RMS 位移配置
     * @return 是否取得 RMS 资源
     */
	bool ApplyRootMotionSourceWithName(const FName& InstanceName,
	                                   const FVector& WorldDirection,
	                                   const FWuwaRootMotionSourceConfig& Config);

	/** @return 当前是否持有 RMS */
	bool HasActiveRootMotionSource() const;

	/** @return 按稳定 InstanceName 找到的当前 RMS */
	TSharedPtr<FRootMotionSource> ResolveActiveRootMotionSource() const;

	/**
     * 从 RMS 上一步已计算速度解析确定性退出速度
     * @param RootMotionSource	当前稳定身份对应的 RMS
     * @param OutVelocity		接收世界空间退出速度
     * @return 是否取得有限且可应用的退出速度
     */
	bool ResolveRootMotionReleaseVelocity(const TSharedPtr<FRootMotionSource>& RootMotionSource,
	                                      FVector& OutVelocity) const;

	/** 按当前出口策略移除 RMS */
	void ReleaseActiveRootMotionSource();

	/**
     * 取得角色旋转策略所有权
     * @param FacingPolicy Definition 配置的朝向策略
     * @param WorldDirection 需要对齐的世界方向
     * @return 是否取得并应用成功
     */
	bool AcquireFacingRotationOverride(EWuwaActionFacingPolicy FacingPolicy, const FVector& WorldDirection);

	/** 恢复动作前的角色旋转策略 */
	void ReleaseFacingRotationOverride();

	/** 回收本能力持有的全部运行资源 */
	void ReleaseActiveResources();

	/**
     * 判断 Stop 是否等待 CharacterMovement Exit Move
     * @param Message	当前 Coordinator 停止消息
     * @return 是否只结束 Gameplay Runtime 并保留物理资源
     */
	bool ShouldDeferPhysicalFinalize(const FWuwaActionStopMessage& Message) const;

	/**
     * 完成唯一 MovementAction 物理退出
     * @param ExitGeneration	当前退出命令序号，非网络清理时无效
     * @param bIsReplay		是否来自校正重演
     * @return 是否达到物理结束状态
     */
	bool FinalizePhysicalMovementAction(const FWuwaNetworkActionGeneration& ExitGeneration, bool bIsReplay);

	/** 清空物理资源身份并保留最近退出序号 */
	void ResetPhysicalRuntime();

	/** 清空已经完成外部清理的运行记录 */
	void ResetActiveRuntime();

	/**
     * 为正常完成准备 RMS 释放与 Finalized 后出口策略
     * @param MoveIntent 触发完成出口的移动输入
     * @return 当前物理事实是否允许正常完成
     */
	bool PrepareCompletedExit(const FVector2D& MoveIntent);

	/** 清空事件就绪与待处理网络退出状态 */
	void ResetNetworkActionExitState();

	/**
     * 查找事件对应的 Movement 响应
     * @param EventTag Action 事件标签
     * @param OutResponse 接收响应
     * @return 是否命中
     */
	bool ResolveEventResponse(const FGameplayTag& EventTag, EWuwaMovementActionEventResponse& OutResponse) const;

	/** 已发生落地时只发布事实 */
	UFUNCTION()
	void HandleLandedEvent(const FWuwaLandingEvent& LandingEvent);

	/** 已发生 MovementMode 变化时只发布事实 */
	void HandleMovementModeChangedEvent(EMovementMode PreviousMovementMode,
	                                    uint8 PreviousCustomMode,
	                                    EMovementMode NewMovementMode,
	                                    uint8 NewCustomMode);
};
