// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Contracts/WuwaCombatTargetInterface.h"
#include "Combat/Contracts/WuwaDamageReceiverInterface.h"
#include "GameFramework/Character.h"
#include "Input/WuwaInputTypes.h"
#include "Movement/WuwaMovementTypes.h"
#include "WuwaCharacter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UWuwaActionAnimationCapabilityComponent;
class UWuwaActionAbilityInteropComponent;
class UWuwaActionCoordinatorComponent;
class UWuwaActionNetworkComponent;
class UWuwaActionRuleIntentProviderComponent;
class UWuwaAbilityInputRouterComponent;
class UWuwaActionRuleSet;
class UWuwaCameraModeComponent;
class UWuwaCameraProfile;
class UWuwaCharacterMessageDispatcherComponent;
class UWuwaCharacterMovementComponent;
class UWuwaCombatExecutionComponent;
class UWuwaHealthComponent;
class UWuwaPoiseComponent;
class UWuwaWeaponComponent;
class UWuwaMovementActionCapabilityComponent;
class UWuwaMovementProfile;
class UWuwaPawnAbilityInitComponent;
class UWuwaGrappleCapabilityComponent;
class UWuwaGrapplePresentationComponent;
class UWuwaSpringArmComponent;
class UWuwaStateTagComponent;
class UWuwaTargetingComponent;
class UWuwaTargetingProfile;
class UWuwaTraversalActionIntentProviderComponent;
class UWuwaTraversalProfile;
class UWuwaWorldHealthBarComponent;
struct FWuwaActionResolutionSnapshot;

/** 角色 Composition Root 与只读 Gameplay 门面 */
UCLASS(Abstract)
class WUWA_API AWuwaCharacter : public ACharacter,
                                public IWuwaCombatTargetInterface,
                                public IWuwaDamageReceiverInterface
{
	GENERATED_BODY()

public:
	/** 创建角色的稳定默认子对象布局 */
	explicit AWuwaCharacter(const FObjectInitializer& ObjectInitializer);

	/**
     * 提交 PlayerController 构造的完整输入帧
     * @param InputFrame 连续输入和离散边沿的不可变快照
     * @return 是否进入角色消息管线
     */
	bool SubmitInputFrame(const FWuwaInputFrame& InputFrame);

	/**
     * 将二维移动输入转换为相机相对世界输入
     * @param Right 水平输入
     * @param Forward 前后输入
     */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoMove(float Right, float Forward);

	/**
     * 将观察输入提交给当前相机权威
     * @param Yaw 水平观察输入
     * @param Pitch 垂直观察输入
     */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** @return 所有状态标签的唯一事实组件 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|State")
	UWuwaStateTagComponent* GetStateTagComponent() const
	{
		return StateTagComponent;
	}

	/** @return 独占 Action 生命周期协调器 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Action")
	UWuwaActionCoordinatorComponent* GetActionCoordinatorComponent() const
	{
		return ActionCoordinatorComponent;
	}

	/** @return 当前角色目标权威 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Targeting")
	UWuwaTargetingComponent* GetTargetingComponent() const
	{
		return TargetingComponent;
	}

	/** @return 当前相机模式组件 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Camera")
	UWuwaCameraModeComponent* GetCameraModeComponent() const
	{
		return CameraModeComponent;
	}

	/** @return 自定义 CharacterMovement */
	UWuwaCharacterMovementComponent* GetWuwaMovementComponent() const;

	/** @return 当前 Pawn 的能力系统初始化组件 */
	UWuwaPawnAbilityInitComponent* GetPawnAbilityInitComponent() const
	{
		return PawnAbilityInitComponent;
	}

	/** @return GAS Combat 状态与 Legacy StateTag 的唯一镜像组件 */
	UWuwaActionAbilityInteropComponent* GetActionAbilityInteropComponent() const
	{
		return ActionAbilityInteropComponent;
	}

	/** @return Legacy Movement Action 的唯一网络入口 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Action")
	UWuwaActionNetworkComponent* GetActionNetworkComponent() const
	{
		return ActionNetworkComponent;
	}

	/** @return 当前 Pawn 的 Ability 输入路由组件 */
	UWuwaAbilityInputRouterComponent* GetAbilityInputRouterComponent() const
	{
		return AbilityInputRouterComponent;
	}

	/** @return 当前武器静态网格组件 */
	UStaticMeshComponent* GetWeaponMeshComponent() const
	{
		return WeaponMeshComponent;
	}

	/** @return 当前刀鞘静态网格组件 */
	UStaticMeshComponent* GetScabbardMeshComponent() const
	{
		return ScabbardMeshComponent;
	}

	/** @return 当前武器装配组件 */
	UWuwaWeaponComponent* GetWeaponComponent() const
	{
		return WeaponComponent;
	}

	/** @return 当前服务端权威命中执行组件 */
	UWuwaCombatExecutionComponent* GetCombatExecutionComponent() const
	{
		return CombatExecutionComponent;
	}

	/** @return 当前 Pawn 的生命与死亡事实组件 */
	UWuwaHealthComponent* GetHealthComponent() const
	{
		return HealthComponent;
	}

	/** @return 当前 Pawn 的真实生命世界血条组件 */
	UWuwaWorldHealthBarComponent* GetWorldHealthBarComponent() const
	{
		return WorldHealthBarComponent;
	}

	/** @return 当前 Pawn 的韧性与破韧事实组件 */
	UWuwaPoiseComponent* GetPoiseComponent() const
	{
		return PoiseComponent;
	}

	//~ Begin IWuwaCombatTargetInterface
	/**
     * 评估当前 Character 是否接受中性 Combat Hit
     *
     * @param Query	服务端权威命中查询
     * @return 中性目标资格响应
     */
	virtual FWuwaCombatTargetResponse
	EvaluateCombatTarget_Implementation(const FWuwaCombatTargetQuery& Query) const override;
	//~ End IWuwaCombatTargetInterface

	//~ Begin IWuwaDamageReceiverInterface Interface
	/** @return 当前 Character 已绑定的 PlayerState ASC */
	virtual UWuwaAbilitySystemComponent* GetDamageReceiverAbilitySystemComponent() const override;

	/** @return 当前 Character 的生命门面 */
	virtual UWuwaHealthComponent* GetDamageReceiverHealthComponent() const override;

	/** @return 当前 Character 的韧性门面 */
	virtual UWuwaPoiseComponent* GetDamageReceiverPoiseComponent() const override;
	//~ End IWuwaDamageReceiverInterface Interface

	/** @return Controller 最近一帧提交的真实移动输入 */
	const FVector2D& GetCurrentMoveIntent() const
	{
		return CurrentMoveIntent;
	}

	/** @return 动画与 Debug 使用的只读移动快照 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Movement")
	FWuwaLocomotionSnapshot GetLocomotionSnapshot() const;

protected:
	//~ Begin ACharacter Interface
	/** @return 当前 Wuwa Movement 是否允许原生普通跳跃尝试 */
	virtual bool CanJumpInternal_Implementation() const override;
	//~ End ACharacter Interface

	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	//~ Begin APawn Interface
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_PlayerState() override;
	virtual void OnRep_Controller() override;
	//~ End APawn Interface

private:
	/** Camera Boom */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaSpringArmComponent> CameraBoom;

	/** Follow Camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	/** 全部活动 GameplayTag 的唯一拥有者 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaStateTagComponent> StateTagComponent;

	/** 角色范围内有序消息和阶段调度入口 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaCharacterMessageDispatcherComponent> MessageDispatcherComponent;

	/** 独占 Action 的严格 FIFO 与事务生命周期 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaActionCoordinatorComponent> ActionCoordinatorComponent;

	/** Legacy Movement Action 的预测、权威与远端表现入口 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaActionNetworkComponent> ActionNetworkComponent;

	/** 现有四个 Action 的 RuleSet Intent Provider */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaActionRuleIntentProviderComponent> ActionRuleIntentProviderComponent;

	/** CharacterMovement 原语和 RMS 执行能力 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaMovementActionCapabilityComponent> MovementActionCapabilityComponent;

	/** Montage 表现执行能力 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaActionAnimationCapabilityComponent> ActionAnimationCapabilityComponent;

	/** 将 Input.Grapple 解析为冻结 Traversal Intent 的组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaTraversalActionIntentProviderComponent> TraversalActionIntentProviderComponent;

	/** Grapple Action 与 Movement Primitive 的事务桥接组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaGrappleCapabilityComponent> GrappleCapabilityComponent;

	/** Grapple 绳索与 Camera Feedback 的可选消费者 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaGrapplePresentationComponent> GrapplePresentationComponent;

	/** 目标候选、软锁与硬锁权威 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaTargetingComponent> TargetingComponent;

	/** Camera Mode 和 Rig 状态权威 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaCameraModeComponent> CameraModeComponent;

	/** PlayerState ASC 与当前 Character Avatar 的幂等初始化边界 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaPawnAbilityInitComponent> PawnAbilityInitComponent;

	/** GAS Combat 状态与 Legacy StateTag 的唯一镜像边界 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaActionAbilityInteropComponent> ActionAbilityInteropComponent;

	/** Dispatcher 与 ASC 之间唯一的 Ability 输入路由 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaAbilityInputRouterComponent> AbilityInputRouterComponent;

	/** 角色右手挂载的武器静态网格 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> WeaponMeshComponent;

	/** 武器定义装配和剑刃端点读取组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaWeaponComponent> WeaponComponent;

	/** 角色左手挂载的刀鞘静态网格 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ScabbardMeshComponent;

	/** 服务端权威剑刃 Sweep 与命中事实发布组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaCombatExecutionComponent> CombatExecutionComponent;

	/** 绑定 ASC Health 并发布类型化死亡事实的组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaHealthComponent> HealthComponent;

	/** 订阅真实 Health Fact 的本地世界血条表现组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaWorldHealthBarComponent> WorldHealthBarComponent;

	/** 绑定 ASC Poise 并发布类型化破韧事实的组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaPoiseComponent> PoiseComponent;

	/** HealthComponent 死亡事实委托 Handle */
	FDelegateHandle DeathFactDelegateHandle;

	/** 最近已经消费的死亡事实序号 */
	int32 LastHandledDeathSequence = 0;

	/** 当前 Character 是否继续接受中性 Combat Hit */
	bool bCombatTargetEnabled = true;

	/** 当前角色使用的移动参数 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaMovementProfile> MovementProfile;

	/** 当前角色使用的目标参数 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaTargetingProfile> TargetingProfile;

	/** 当前角色使用的 Camera 参数 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaCameraProfile> CameraProfile;

	/** 输入到 Action Definition 的纯数据解析规则 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaActionRuleSet> ActionRuleSet;

	/** 可选 Traversal 装配配置，缺失时只禁用 Grapple */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Traversal", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaTraversalProfile> TraversalProfile;

	/** Controller 最近一帧的真实 WASD 输入 */
	UPROPERTY(Transient)
	FVector2D CurrentMoveIntent = FVector2D::ZeroVector;

	/** @return 当前 Action 是否阻止 Locomotion 消费移动输入 */
	bool IsMoveInputBlocked() const;

	/** 绑定唯一 HealthComponent 死亡事实委托 */
	void BindDeathFactDelegate();

	/** 解绑 HealthComponent 死亡事实委托 */
	void UnbindDeathFactDelegate();

	/**
	 * 把类型化死亡事实分发给各领域窄接口
	 *
	 * @param Fact	当前 Pawn 的死亡事实
	 */
	void HandleDeathFact(const FWuwaDeathFact& Fact);

	/**
     * 更新连续移动与观察输入
     * @param InputFrame 当前完整输入帧
     */
	void ApplyContinuousInput(const FWuwaInputFrame& InputFrame);

	/** @return Resolve 阶段只读角色快照 */
	FWuwaActionResolutionSnapshot BuildActionResolutionSnapshot() const;

	/**
     * 装配 CharacterMovement 核心运行时
     * @param Movement	当前角色的唯一 Movement Component
     * @return 是否完成核心移动装配
     */
	bool InitializeMovementRuntime(UWuwaCharacterMovementComponent* Movement);

	/**
     * 装配 Dispatcher、Coordinator 与通用 Action 能力
     * @param Movement	当前角色的唯一 Movement Component
     * @return 是否完成核心 Action 装配
     */
	bool InitializeActionRuntime(UWuwaCharacterMovementComponent* Movement);

	/**
     * 装配 Targeting 并向 Movement 注入只读目标权威
     * @param Movement	当前角色的唯一 Movement Component
     * @return 是否完成核心 Targeting 装配
     */
	bool InitializeTargetingRuntime(UWuwaCharacterMovementComponent* Movement);

	/** @return 是否完成核心 Camera 装配 */
	bool InitializeCameraRuntime();

	/**
     * 尝试装配可选 Traversal 运行时
     * @param Movement	当前角色的唯一 Movement Component
     * @return Grapple 是否可用
     */
	bool TryInitializeTraversalRuntime(UWuwaCharacterMovementComponent* Movement);

	/**
     * 根据当前帧移动输入尝试打断正在阻止普通移动的 GameplayAbility。
     *
     * 这里只负责请求 Ability Cancel，
     * 实际 Ability 清理由 Ability 自己的 EndAbility 完成。
     */
	void TryInterruptAbilityFromMoveInput(const FWuwaInputFrame& InputFrame);
};
