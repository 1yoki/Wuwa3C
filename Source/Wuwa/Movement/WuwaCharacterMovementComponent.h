#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Messaging/WuwaCharacterMessageHandler.h"
#include "Movement/Network/WuwaCharacterNetworkMoveData.h"
#include "Movement/WuwaMovementTypes.h"
#include "Core/WuwaStateTagTypes.h"
#include "Traversal/Contracts/WuwaTraversalTypes.h"
#include "WuwaCharacterMovementComponent.generated.h"

class UWuwaMovementProfile;
class UWuwaMovementActionCapabilityComponent;
class UWuwaStateTagComponent;
class UWuwaTargetingComponent;
struct FWuwaGrappleSavedMoveState;
struct FWuwaMovementActionNetworkReplayState;

/** CharacterMovement 内部唯一的 Grapple CustomMode */
UENUM(BlueprintType)
enum class EWuwaCustomMovementMode : uint8
{
	/** 没有自定义移动原语 */
	None = 0,

	/** 使用共享曲线执行无锚点钩锁 */
	Grapple = 1
};

// 广播已经发生的着陆事件。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWuwaLandingEventSignature, const FWuwaLandingEvent&, LandingEvent);

// 广播 Movement Component 已经完成的真实移动模式变化
DECLARE_MULTICAST_DELEGATE_FourParams(FWuwaMovementModeChangedSignature, EMovementMode, uint8, EMovementMode, uint8);

/** CharacterMovement 已提交的 Grapple 阶段或结束事实 */
DECLARE_MULTICAST_DELEGATE_OneParam(FWuwaGrappleMovementFactSignature, const FWuwaGrappleMovementFact&);

/** 服务端同步处理一次性移动命令 */
DECLARE_DELEGATE_RetVal_OneParam(FWuwaNetworkActionResponse,
                                 FWuwaNetworkCommandProcessor,
                                 const FWuwaPendingNetworkMovementCommand&);

/** Owning Client 消费随移动响应返回的动作结果 */
DECLARE_DELEGATE_OneParam(FWuwaNetworkMoveResponseConsumer, const FWuwaNetworkActionResponse&);

/** Owning Client 在校正重演时消费一次性动作命令 */
DECLARE_DELEGATE_OneParam(FWuwaNetworkMoveReplayConsumer, const FWuwaPendingNetworkMovementCommand&);

UCLASS()
class WUWA_API UWuwaCharacterMovementComponent : public UCharacterMovementComponent, public IWuwaCharacterMessageHandler
{
	GENERATED_BODY()
public:
	UWuwaCharacterMovementComponent();

	//~ Begin UObject Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End UObject Interface

	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Movement")
	FWuwaLandingEventSignature OnLandedEvent;

	// 参数依次为 PreviousMovementMode、PreviousCustomMode、NewMovementMode、NewCustomMode
	// 这是 C++ 运行时事实事件，不允许订阅者反向决定 MovementMode
	FWuwaMovementModeChangedSignature OnWuwaMovementModeChanged;

	/** Grapple Capability 只读消费的 Movement 事实 */
	FWuwaGrappleMovementFactSignature OnGrappleMovementFact;

	// 设置统一状态标签组件。
	void SetStateTagComponent(UWuwaStateTagComponent* InStateTagComponent);

	// Character Composition Root 注入同 Owner 的目标权威；Movement 只读消费 Context。
	bool SetTargetingComponent(UWuwaTargetingComponent* InTargetingComponent);

	// 复制 Profile 中的参数，不在Tick中每帧修改。
	bool ApplyMovementProfile(const UWuwaMovementProfile* Profile);

	/**
	 * 排队当前动作物理帧使用的胶囊朝向
	 * @param DesiredFacingYaw	目标世界 Yaw
	 * @param bAllowSnap		是否允许跳过 RotationRate 瞬时对齐
	 * @return 是否接受有限朝向
	 */
	bool QueueActionFacing(float DesiredFacingYaw, bool bAllowSnap);

	/**
	 * 写入当前 SavedMove 的唯一一次性命令槽
	 * @param Command 待发送的有限移动命令
	 * @return 是否成功占用或合并命令槽
	 */
	bool QueueNetworkMovementCommand(const FWuwaPendingNetworkMovementCommand& Command);

	/** @return 当前 SavedMove 是否允许写入一次性网络命令 */
	bool CanQueueNetworkMovementCommand() const;

	/** @return 分配跨 Jump 与 Legacy Action 共用的单调 Generation */
	FWuwaNetworkActionGeneration AllocateNetworkActionGeneration();

	/** @return 当前 SavedMove 需要捕获的命令与连续移动标志 */
	FWuwaPendingNetworkMovementCommand GetPendingNetworkMovementCommandForSavedMove() const;

	/** @return 原子取走当前命令槽并保留本地首次模拟副本 */
	FWuwaPendingNetworkMovementCommand CapturePendingNetworkMovementCommandForSavedMove();

	/**
	 * 在 CharacterMovement Move 边界准备 MovementAction RMS 起点
	 * @param Command		当前 Move 携带的网络移动命令
	 * @param MoveTimeStamp	当前 Move 的客户端结束时间
	 * @param DeltaTime		当前 Move 的模拟时长
	 * @return 命令无需处理或 RMS 起点已经校验或对齐
	 */
	bool PrepareMovementActionRootMotionStartAtMoveBoundary(const FWuwaPendingNetworkMovementCommand& Command,
	                                                        float MoveTimeStamp,
	                                                        float DeltaTime);

	/**
	 * 在 SavedMove 捕获退出前状态后执行 Owning Client 首次物理退出
	 * @param Command	当前 SavedMove 捕获的网络移动命令
	 * @return 命令无需处理或已经成功应用
	 */
	bool ApplyLocalPredictedMovementActionExitAtSavedMoveBoundary(const FWuwaPendingNetworkMovementCommand& Command);

	/**
	 * 在校正重演前恢复 SavedMove 命令
	 * @param Command 当前重演 Move 捕获的命令
	 */
	void RestoreNetworkMovementCommandForReplay(const FWuwaPendingNetworkMovementCommand& Command);

	/** @return 最近由 PrepMoveFor 恢复的独立 Replay 命令 */
	const FWuwaPendingNetworkMovementCommand& GetRestoredNetworkMovementCommandForReplay() const
	{
		return ReplayNetworkMovementCommand;
	}

	/**
	 * 安装唯一服务端命令处理器
	 * @param BindingOwner 处理器生命周期拥有者
	 * @param Processor 同步命令处理器
	 * @return 是否完成唯一绑定
	 */
	bool BindNetworkCommandProcessor(UObject* BindingOwner, const FWuwaNetworkCommandProcessor& Processor);

	/** @param BindingOwner 请求解除绑定的拥有者 */
	void UnbindNetworkCommandProcessor(const UObject* BindingOwner);

	/**
	 * 安装唯一客户端响应消费者
	 * @param BindingOwner 消费者生命周期拥有者
	 * @param Consumer 响应消费者
	 * @return 是否完成唯一绑定
	 */
	bool BindNetworkMoveResponseConsumer(UObject* BindingOwner, const FWuwaNetworkMoveResponseConsumer& Consumer);

	/** @param BindingOwner 请求解除绑定的拥有者 */
	void UnbindNetworkMoveResponseConsumer(const UObject* BindingOwner);

	/**
	 * 安装唯一校正重演消费者
	 * @param BindingOwner	消费者生命周期拥有者
	 * @param Consumer		重演消费者
	 * @return 是否完成唯一绑定
	 */
	bool BindNetworkMoveReplayConsumer(UObject* BindingOwner, const FWuwaNetworkMoveReplayConsumer& Consumer);

	/** @param BindingOwner 请求解除绑定的拥有者 */
	void UnbindNetworkMoveReplayConsumer(const UObject* BindingOwner);

	/**
	 * 安装唯一普通 MovementAction 物理重演入口
	 * @param InProvider	同角色的 MovementAction Capability
	 * @return 是否完成唯一绑定
	 */
	bool BindMovementActionNetworkProvider(UWuwaMovementActionCapabilityComponent* InProvider);

	/** @param InProvider 请求解除绑定的 MovementAction Capability */
	void UnbindMovementActionNetworkProvider(const UWuwaMovementActionCapabilityComponent* InProvider);

	/**
	 * 捕获当前 Move 起点的普通 MovementAction 物理状态
	 * @param OutState	接收最小物理重演状态
	 */
	void CaptureMovementActionNetworkReplayState(FWuwaMovementActionNetworkReplayState& OutState) const;

	/**
	 * 在 SavedMove 重演前恢复普通 MovementAction 物理状态
	 * @param State	当前历史 Move 捕获的状态
	 */
	void RestoreMovementActionNetworkReplayState(const FWuwaMovementActionNetworkReplayState& State);

	/** @return 取走等待附加到下一个移动响应的动作结果 */
	FWuwaNetworkActionResponse TakePendingNetworkActionResponse();

	//~ Begin UCharacterMovementComponent Network Interface
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void MoveAutonomous(float ClientTimeStamp,
	                            float DeltaTime,
	                            uint8 CompressedFlags,
	                            const FVector& NewAcceleration) override;
	virtual void ClientHandleMoveResponse(const FCharacterMoveResponseDataContainer& MoveResponse) override;
	//~ End UCharacterMovementComponent Network Interface

	// 接收持续移动输入。
	void SetLocomotionIntent(const FVector2D& MoveIntent);

	// 请求普通跳跃、土狼跳跃或写入跳跃输入缓存。
	bool RequestJump();

	/** 结束普通跳跃的持续输入 */
	void ReleaseJump();

	/** @return 当前服务端或预测物理事实是否允许普通跳跃 */
	bool CanAttemptWuwaJump() const;

	/**
	 * 写入有限时长的落地跳跃缓存
	 * @param RequestGeneration		当前跳跃请求 Generation
	 * @param RequestedRemainingTime	请求的剩余缓存秒数
	 * @return 是否持有可消费缓存
	 */
	bool QueueBufferedJump(int32 RequestGeneration, float RequestedRemainingTime);

	/** @return 是否在当前物理安全边界提交了缓存 Jump intent */
	bool ConsumeBufferedJumpAtMovementBoundary();

	// 检查当前是否仍有一次空中 Sprint 二段跳预算，只供 Executor 做启动前检查，不消费预算
	bool CanPerformAirDoubleJump() const;

	/**
	 * 使用明确的空中冲量配置提交二段跳原语
	 * @param WorldDirection 冻结的水平世界方向
	 * @param JumpType AirSprint 或 AirBackflip
	 * @return 是否原子提交速度和预算
	 */
	bool RequestAirJump(const FVector& WorldDirection, EWuwaJumpType JumpType);

	/**
	 * 校正重演时恢复 AirJump 速度但不重复消费预算
	 * @param WorldDirection		冻结的水平世界方向
	 * @param JumpType			AirSprint 或 AirBackflip
	 * @param AirCycleGeneration	命令所属普通跳跃滞空 Generation
	 * @return 是否恢复了有限速度
	 */
	bool ReplayAirJump(const FVector& WorldDirection, EWuwaJumpType JumpType, int32 AirCycleGeneration);

	/** @return 当前普通跳跃滞空 Generation */
	int32 GetAirCycleGeneration() const
	{
		return AirActionState.AirCycleGeneration;
	}

	/**
	 * 原子取得 Grapple CustomMode 和独立 MovementHandle
	 * @param Request	冻结 Movement 请求
	 * @param OutHandle	接收新 MovementHandle
	 * @return 是否成功启动
	 */
	bool StartGrappleMovement(const FWuwaGrappleMovementRequest& Request, FWuwaGrappleMovementHandle& OutHandle);

	/**
	 * 幂等停止指定 Grapple Movement
	 * @param Handle	需要停止的 MovementHandle
	 * @return 是否停止了匹配实例
	 */
	bool StopGrappleMovement(const FWuwaGrappleMovementHandle& Handle);

	/**
	 * 以明确原因幂等停止指定 Grapple Movement
	 * @param Handle		需要停止的 MovementHandle
	 * @param EndReason	明确的停止原因
	 * @return 是否停止了匹配实例
	 */
	bool StopGrappleMovement(const FWuwaGrappleMovementHandle& Handle, EWuwaGrappleMovementEndReason EndReason);

	/**
	 * 捕获当前 Move 起点的 Grapple 本地重演状态
	 * @param OutState	接收不含 UObject 与本机 Handle 的纯标量状态
	 */
	void CaptureGrappleNetworkReplayState(FWuwaGrappleSavedMoveState& OutState) const;

	/**
	 * 在 SavedMove 重演前恢复 Grapple 本地物理状态
	 * @param State	保存的纯标量 Grapple 状态
	 */
	void RestoreGrappleNetworkReplayState(const FWuwaGrappleSavedMoveState& State);

	/** @return 当前或最近一次 Grapple Movement 只读快照 */
	FWuwaGrappleMovementSnapshot GetGrappleMovementSnapshot() const;

	/** @return 当前是否持有有效 Grapple MovementHandle 和 CustomMode */
	bool IsGrappleMovementActive() const;

	/** @return 当前 MovementMode 是否为 Grapple CustomMode */
	bool IsInGrappleMovementMode() const;

	/** @return 当前是否持有 Grapple 运行资源 */
	bool HasGrappleRuntime() const;

	/** @return 当前是否处于物理下落或空中自定义移动状态 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Movement")
	bool IsAirborneState() const;

	/**
	 * 按批准顺序组合正常释放出口速度
	 * @param Spec	Movement 配置
	 * @param Context	冻结领域上下文
	 * @param BaseTangentVelocity	T=1 的共享轨迹切线
	 * @return 满足方向保底并限制最大长度的出口速度
	 */
	static FVector CalculateGrappleExitVelocity(const FWuwaGrappleMovementSpec& Spec,
	                                            const FWuwaGrappleActionContext& Context,
	                                            const FVector& BaseTangentVelocity);

	//~ Begin IWuwaCharacterMessageHandler Interface
	virtual void GatherHandledInputTags(FGameplayTagContainer& OutInputTags) const override;
	virtual FWuwaCommandDispatchResult HandleInputCommand(const FWuwaInputCommand& Command,
	                                                      const FWuwaInputFrame& InputFrame) override;
	//~ End IWuwaCharacterMessageHandler Interface

	// 返回最近一次落地事件的只读副本
	const FWuwaLandingEvent& GetLastLandingEvent() const
	{
		return LastLandingEvent;
	}

	// Dash Handoff 完成后，用该时刻的真实输入尝试进入短暂 Sprint Run。
	bool EnterSprintRun(const FVector2D& MoveIntent);

	void ExitSprintRun();

	UFUNCTION(BlueprintPure, Category = "Wuwa|Movement")
	FWuwaLocomotionSnapshot GetLocomotionSnapshot() const;

	/** @return 当前可由网络移动重演的最大速度 */
	virtual float GetMaxSpeed() const override;

	/**
	 * 使用可重演加速度计算当前速度档位
	 * @param DeltaTime             	当前移动步长
	 * @param Friction              	当前摩擦系数
	 * @param bFluid                	是否使用流体摩擦
	 * @param BrakingDeceleration   	当前制动减速度
	 */
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;

	/** @return 当前可由网络移动重演的速度档位 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Movement")
	EWuwaLocomotionSpeedMode ResolveLocomotionSpeedMode() const;

	/** @return 当前网络移动基线只读快照 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Movement|Network")
	FWuwaNetworkMovementBaselineSnapshot GetNetworkBaselineSnapshot() const;

	FORCEINLINE bool IsSprinting() const
	{
		return bIsSprinting;
	}

protected:
	virtual void BeginPlay() override;

	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void
	TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void ProcessLanded(const FHitResult& Hit, float RemainingTime, int32 Iterations) override;

	virtual void OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData,
	                                        float TimeStamp,
	                                        FVector NewLocation,
	                                        FVector NewVelocity,
	                                        UPrimitiveComponent* NewBase,
	                                        FName NewBaseBoneName,
	                                        bool bHasBase,
	                                        bool bBaseRelativePosition,
	                                        uint8 ServerMovementMode,
	                                        FVector ServerGravityDirection) override;

	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

	virtual void PhysicsRotation(float DeltaTime) override;

	/**
	 * 通过原生 Jump intent 提交普通跳跃
	 * @param bReplayingMoves	是否正在重演 SavedMove
	 * @param DeltaTime			当前移动时间片
	 * @return 是否成功进入普通跳跃
	 */
	virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;

	// Hard Lock 时提供目标朝向，其余状态保持 UE 原生移动朝向。
	virtual FRotator ComputeOrientToMovementRotation(const FRotator& CurrentRotation,
	                                                 float DeltaTime,
	                                                 FRotator& DeltaRotation) const override;

private:
	/** 当前组件持有的持久 Packed Move 容器 */
	FWuwaCharacterNetworkMoveDataContainer WuwaNetworkMoveDataContainer;

	/** 当前组件持有的持久 MoveResponse 容器 */
	FWuwaCharacterMoveResponseDataContainer WuwaMoveResponseDataContainer;

	/** 等待下一个 SavedMove 捕获的一次性命令 */
	FWuwaPendingNetworkMovementCommand PendingNetworkMovementCommand;

	/** 当前校正重演使用且不得回写发送槽的命令 */
	FWuwaPendingNetworkMovementCommand ReplayNetworkMovementCommand;

	/** SavedMove 捕获后仅供本地首次模拟使用的命令 */
	FWuwaPendingNetworkMovementCommand LocalCapturedNetworkMovementCommand;

	/** 当前物理步需要应用的网络朝向 */
	float ActiveNetworkFacingYaw = 0.f;

	/** 当前物理步是否持有网络朝向 */
	bool bHasActiveNetworkFacing = false;

	/** 当前网络朝向是否允许一次性瞬时对齐 */
	bool bAllowActiveNetworkFacingSnap = false;

	/** 服务端最近已经消费的一次性 Generation */
	FWuwaNetworkActionGeneration LastProcessedNetworkGeneration;

	/** Owning Client 为 Jump 与 Legacy Action 分配的下一个 Generation */
	int32 NextLocalNetworkGeneration = 1;

	/** 等待写入 MoveResponse 的有限动作结果 */
	FWuwaNetworkActionResponse PendingNetworkActionResponse;

	/** 唯一命令处理器的弱拥有者 */
	TWeakObjectPtr<UObject> NetworkCommandProcessorOwner;

	/** 唯一客户端响应消费者的弱拥有者 */
	TWeakObjectPtr<UObject> NetworkMoveResponseConsumerOwner;

	/** 唯一校正重演消费者的弱拥有者 */
	TWeakObjectPtr<UObject> NetworkMoveReplayConsumerOwner;

	/** 唯一普通 MovementAction 物理重演入口 */
	TWeakObjectPtr<UWuwaMovementActionCapabilityComponent> MovementActionNetworkProvider;

	/** 当前服务端同步命令处理器 */
	FWuwaNetworkCommandProcessor NetworkCommandProcessor;

	/** 当前 Owning Client 响应消费者 */
	FWuwaNetworkMoveResponseConsumer NetworkMoveResponseConsumer;

	/** 当前 Owning Client 校正重演消费者 */
	FWuwaNetworkMoveReplayConsumer NetworkMoveReplayConsumer;

	/**
	 * 将 MoveData 元数据提交到本次模拟
	 * @param Command 当前 Move 的扩展数据
	 */
	void ApplyNetworkMovementCommand(const FWuwaPendingNetworkMovementCommand& Command, bool bIsReplay);

	/**
	 * 在服务端幂等处理一次性命令
	 * @param Command 当前 Move 的一次性命令
	 */
	FWuwaNetworkActionResponse ProcessAuthorityNetworkCommand(const FWuwaPendingNetworkMovementCommand& Command);

	/** CharacterMovement 独占的 Grapple 运行资源 */
	struct FWuwaGrappleMovementRuntime
	{
		FWuwaGrappleMovementHandle Handle;
		FWuwaGrappleMovementRequest Request;
		FVector CommitStartLocation = FVector::ZeroVector;
		FVector PreviousAppliedOffset = FVector::ZeroVector;
		FVector LastBaseTangentVelocity = FVector::ZeroVector;
		float ElapsedTime = 0.f;
		float AccumulatedLateralOffset = 0.f;
		float LateralVelocity = 0.f;
		EWuwaGrappleRuntimePhase Phase = EWuwaGrappleRuntimePhase::None;
	};

	/** 当前 Grapple Movement 运行资源 */
	FWuwaGrappleMovementRuntime GrappleRuntime;

	/** 校正重演跨越 Grapple 启动边沿时保留的短时本地请求 */
	FWuwaGrappleMovementRuntime GrappleReplayRuntimeCache;

	/** 当前或最近一次 Grapple Movement 只读快照 */
	FWuwaGrappleMovementSnapshot GrappleMovementSnapshot;

	/** 下一个 CharacterMovement 局部 Handle */
	int64 NextGrappleMovementHandle = 1;

	/** 使用共享轨迹推进当前 Grapple */
	void PhysGrapple(float DeltaTime, int32 Iterations);

	/**
	 * 收口 Movement、释放 CustomMode 并广播唯一 Ended 事实
	 * @param EndReason	Movement 结束原因
	 * @param ExitVelocity	Released 时的出口速度
	 */
	void FinishGrappleMovement(EWuwaGrappleMovementEndReason EndReason,
	                           const FVector& ExitVelocity = FVector::ZeroVector);

	/** @param NewPhase 进入并广播的新运行阶段 */
	void EnterGrapplePhase(EWuwaGrappleRuntimePhase NewPhase);

	/** @param NormalizedPullTime 当前共享轨迹归一化时间 */
	void RefreshGrappleMovementSnapshot(float NormalizedPullTime);

	bool CanMaintainSprintRun() const;
	void SetSprinting(bool bNewSprinting);

	// 所有空中二段跳最终进入这里，保证速度和预算原子提交
	bool CommitAirJumpVelocity(const FVector& WorldDirection,
	                           float HorizontalSpeed,
	                           float VerticalSpeed,
	                           EWuwaJumpType JumpType,
	                           bool bConsumeBudget);

	bool IsWithCoyoteTime(double CurrentTime) const;

	/** @return 当前 Combat 状态是否阻止普通 Jump 与 Jump Buffer */
	bool IsJumpBlockedByCombatState() const;

	/** 将 Authority 当前移动表现事实收敛到唯一复制状态 */
	void RefreshAuthorityLocomotionPresentationState();

	/** @return 当前角色是否应使用权威复制的移动表现事实 */
	bool ShouldUseReplicatedLocomotionPresentationState() const;

	/**
	 * 将已验证的权威表现事实覆盖到 Simulated Proxy 快照
	 * @param InOutSnapshot	待更新的移动快照
	 */
	void ApplyReplicatedLocomotionPresentationState(FWuwaLocomotionSnapshot& InOutSnapshot) const;

	/**
	 * 验证新到达的权威移动表现事实
	 * @param PreviousState	复制前的表现状态
	 */
	UFUNCTION()
	void OnRep_LocomotionPresentationState(const FWuwaReplicatedLocomotionPresentationState& PreviousState);

	/**
	 * 处理 Jump 输入与活动 GameplayAbility 的打断关系。
	 *
	 * @return
	 * true  = 当前允许继续执行正常 Jump；
	 * false = Jump 被活动 Ability 阻止，本次输入应被消费。
	 */
	bool TryInterruptAbilityForJump();

	/** 仅向 Simulated Proxy 复制的权威移动表现事实 */
	UPROPERTY(ReplicatedUsing = OnRep_LocomotionPresentationState)
	FWuwaReplicatedLocomotionPresentationState ReplicatedLocomotionPresentationState;

	/** 当前复制表现事实是否已通过结构校验 */
	bool bReplicatedLocomotionPresentationStateValid = false;

	UPROPERTY(Transient)
	FWuwaAirActionState AirActionState;
	UPROPERTY(Transient)
	FWuwaLandingEvent LastLandingEvent;
	UPROPERTY(Transient)
	EWuwaJumpType LastJumpType = EWuwaJumpType::None;

	/** 当前 DoJump 是否正在重演可信 SavedMove */
	bool bReplayingWuwaJump = false;

	// 每次成功起跳递增
	int32 JumpSequence = 0;

	double GetMovementTime() const;

	void RefreshLocomotionState();

	float ConfiguredWalkSpeed = 250.f;
	float ConfiguredRunSpeed = 500.f;
	float ConfiguredSprintSpeed = 700.f;
	float ConfiguredSprintRunDeceleration = 2000.f;
	float ConfiguredAnalogRunThreshold = 0.5f;

	// 保存二段跳的运行时向上速度。
	float ConfiguredDoubleJumpZVelocity = 650.f;

	// 保存二段跳的运行时水平速度。
	float ConfiguredDoubleJumpForwardSpeed = 400.f;

	// 保存后空翻运行时向上速度。
	float ConfiguredBackflipZVelocity = 650.f;

	// 保存后空翻运行时向后推进速度。
	float ConfiguredBackflipBackwardSpeed = 400.f;

	// 保存每次滞空允许的最大跳跃次数。
	int32 ConfiguredMaxJumpCount = 2;

	// 保存土狼时间的运行时配置。
	float ConfiguredCoyoteTime = 0.15f;

	// 保存跳跃输入缓存的有效时间。
	float ConfiguredJumpBufferTime = 0.10f;

	// 保存重落地向下冲击速度阈值，不在 Tick 中读取 Data Asset。
	float ConfiguredHeavyLandingVelocityThreshold = 900.f;

	float MoveInputMagnitude = 0.f;
	FVector2D CurrentLocomotionIntent = FVector2D::ZeroVector;
	bool bIsSprinting = false;

	FGameplayTagContainer RuntimeSprintBlockedTags;

	UPROPERTY(Transient)
	TObjectPtr<UWuwaStateTagComponent> StateTagComponent;

	/** SprintRun 状态持有的唯一标签句柄 */
	FWuwaStateTagHandle SprintingTagHandle;

	/** 已收到的服务端移动校正数量 */
	int32 NetworkCorrectionCount = 0;

	/** 最近一次服务端移动校正距离 */
	float LastNetworkCorrectionDistance = 0.f;

	/** 最大服务端移动校正距离 */
	float MaxNetworkCorrectionDistance = 0.f;

	/** 服务端移动校正距离累计值 */
	double TotalNetworkCorrectionDistance = 0.0;

	/** 第一次服务端移动校正的真实时间 */
	double FirstNetworkCorrectionRealTime = -1.0;

	/** 最近一次服务端移动校正的真实时间 */
	double LastNetworkCorrectionRealTime = -1.0;

	// 弱引用不改变 Targeting 生命周期；Movement 不能成为目标状态拥有者。
	UPROPERTY(Transient)
	TWeakObjectPtr<UWuwaTargetingComponent> TargetingComponent;
};
