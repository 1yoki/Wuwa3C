// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/WuwaGameplayAbility.h"
#include "Combat/Contracts/WuwaCombatFacts.h"
#include "Combat/Contracts/WuwaCombatTypes.h"
#include "Combat/Data/WuwaMeleeAttackDefinition.h"
#include "WuwaGameplayAbility_MeleeAttack.generated.h"

class AActor;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitInputPress;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class USkeletalMeshComponent;
class UWuwaCombatExecutionComponent;
class UWuwaAbilitySystemComponent;
class UGameplayEffect;

/** 本地预测的单次轻剑攻击 Ability */
UCLASS(Blueprintable)
class WUWA_API UWuwaGameplayAbility_MeleeAttack : public UWuwaGameplayAbility
{
	GENERATED_BODY()

public:
	/** 配置攻击标签、本地预测和按 Actor 实例化 */
	UWuwaGameplayAbility_MeleeAttack();

	/** @return 当前或最近一次攻击生命周期快照 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Attack")
	FWuwaMeleeAttackRuntimeSnapshot GetRuntimeSnapshot() const;

	/** @return 当前配置的 AttackDefinition */
	const UWuwaMeleeAttackDefinition* GetAttackDefinition() const
	{
		return AttackDefinition;
	}

	/** @return 当前配置的伤害 GameplayEffect */
	TSubclassOf<UGameplayEffect> GetDamageEffectClass() const
	{
		return DamageEffectClass;
	}

	//~ Begin UGameplayAbility Interface
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle,
	                                const FGameplayAbilityActorInfo* ActorInfo,
	                                const FGameplayTagContainer* SourceTags = nullptr,
	                                const FGameplayTagContainer* TargetTags = nullptr,
	                                FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle,
	                             const FGameplayAbilityActorInfo* ActorInfo,
	                             FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;
	/**
	 * 记录活动 Ability 的窗口外输入拒绝
	 *
	 * @param Handle			Ability 规格句柄
	 * @param ActorInfo		Ability Actor 上下文
	 * @param ActivationInfo	本次激活信息
	 * @return 无
	 */
	virtual void InputPressed(FGameplayAbilitySpecHandle Handle,
	                          const FGameplayAbilityActorInfo* ActorInfo,
	                          FGameplayAbilityActivationInfo ActivationInfo) override;
	/**
	 * 记录外部取消原因并执行标准取消流程
	 *
	 * @param Handle				Ability 规格句柄
	 * @param ActorInfo			Ability Actor 上下文
	 * @param ActivationInfo		本次激活信息
	 * @param bReplicateCancelAbility	是否向远端复制取消
	 * @return 无
	 */
	virtual void CancelAbility(FGameplayAbilitySpecHandle Handle,
	                           const FGameplayAbilityActorInfo* ActorInfo,
	                           FGameplayAbilityActivationInfo ActivationInfo,
	                           bool bReplicateCancelAbility) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle,
	                        const FGameplayAbilityActorInfo* ActorInfo,
	                        FGameplayAbilityActivationInfo ActivationInfo,
	                        bool bReplicateEndAbility,
	                        bool bWasCancelled) override;
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	//~ End UGameplayAbility Interface

private:
	/** 当前攻击定义 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Attack",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaMeleeAttackDefinition> AttackDefinition;

	/** Authority 命中后应用到目标 ASC 的伤害效果 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Attack",
	          meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** 当前 Montage 播放任务 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	/** 当前 HitWindow Begin 事件任务 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> HitWindowBeginTask;

	/** 当前 HitWindow End 事件任务 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> HitWindowEndTask;

	/** 当前连招输入等待任务 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitInputPress> ComboInputTask;

	/** 当前 ComboWindow Begin 事件任务 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ComboWindowBeginTask;

	/** 当前 ComboWindow End 事件任务 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ComboWindowEndTask;

	/** 当前攻击冻结的 Avatar Mesh */
	TWeakObjectPtr<USkeletalMeshComponent> ActiveMesh;

	/** 当前攻击冻结的 Montage */
	TWeakObjectPtr<UAnimMontage> ActiveMontage;

	/** 当前攻击段索引 */
	int32 CurrentStepIndex = INDEX_NONE;

	/** 当前激活实际进入的攻击段数量 */
	int32 ExecutedStepCount = 0;

	/** 当前段在进入边界冻结的数据 */
	FWuwaMeleeAttackStep FrozenCurrentStep;

	/** 当前段是否已经完成数据冻结 */
	bool bHasFrozenCurrentStep = false;

	/** 当前 Montage Section */
	FName CurrentSection;

	/** 已提交的下一 Montage Section */
	FName NextSection;

	/** 当前段连招窗口是否开放 */
	bool bComboWindowOpen = false;

	/** 唯一输入缓冲槽是否占用 */
	bool bBufferedNextStep = false;

	/** 下一 Section 是否已提交给 ASC */
	bool bTransitionCommitted = false;

	/** 当前或最近一次攻击接受的连招输入数量 */
	int32 AcceptedComboInputCount = 0;

	/** 当前或最近一次攻击拒绝的连招输入数量 */
	int32 RejectedComboInputCount = 0;

	/** 最近一次连招输入拒绝原因 */
	EWuwaMeleeComboInputRejectReason LastComboInputRejectReason = EWuwaMeleeComboInputRejectReason::None;

	/** 三段命中窗口 Begin 计数 */
	TArray<int32> PerStepHitWindowBeginCounts;

	/** 三段命中窗口 End 计数 */
	TArray<int32> PerStepHitWindowEndCounts;

	/** 三段接受的命中事实计数 */
	TArray<int32> PerStepHitFactCounts;

	/** 三段成功应用伤害的计数 */
	TArray<int32> PerStepDamageApplicationCounts;

	/** 三段最近创建的窗口句柄 */
	TArray<FWuwaCombatWindowHandle> PerStepCombatWindowHandles;

	/** 当前攻击窗口状态 */
	EWuwaMeleeAttackWindowState WindowState = EWuwaMeleeAttackWindowState::Idle;

	/** 最近结束原因 */
	EWuwaMeleeAttackEndReason LastEndReason = EWuwaMeleeAttackEndReason::None;

	/** 当前或最近一次合法 Begin 数量 */
	int32 HitWindowBeginCount = 0;

	/** 当前或最近一次合法 End 数量 */
	int32 HitWindowEndCount = 0;

	/** 最近 AttackDefinition 名称 */
	FString LastAttackDefinitionName;

	/** 最近 Montage 名称 */
	FString LastMontageName;

	/** 当前或最近一次 AbilitySpec Handle 的安全字符串 */
	FString LastAbilitySpecHandle;

	/** 当前或最近一次 PredictionKey 的安全摘要 */
	FString LastPredictionKeySummary;

	/** Authority 当前活动的 CombatWindow */
	FWuwaCombatWindowHandle ActiveCombatWindowHandle;

	/** 最近一次成功创建的 CombatWindow 编号 */
	uint32 LastCombatWindowHandleValue = 0;

	/** CombatExecution 命中事实委托 Handle */
	FDelegateHandle CombatHitFactDelegateHandle;

	/** 当前或最近攻击接受的权威命中事实数量 */
	int32 ReceivedHitFactCount = 0;

	/** 当前或最近攻击拒绝的旧命中事实数量 */
	int32 RejectedHitFactCount = 0;

	/** 当前或最近攻击成功应用伤害的次数 */
	int32 SuccessfulDamageApplicationCount = 0;

	/** 当前或最近攻击拒绝应用伤害的次数 */
	int32 RejectedDamageApplicationCount = 0;

	/** 当前攻击已经接受的不同目标 */
	TSet<TWeakObjectPtr<AActor>> ReceivedHitTargets;

	/** 最近一次接受的目标名称 */
	FString LastCombatHitTargetName;

	/** 当前 Ability 是否已添加 ASC 攻击状态标签 */
	bool bOwnsAttackingGameplayTag = false;

	/** 是否正在执行统一清理 */
	bool bCleanupInProgress = false;

	/** 是否已经请求结束当前 Ability */
	bool bEndRequested = false;

	/** @return ActorInfo、Definition、Montage、Section 和 Mesh 是否满足激活门禁 */
	bool ValidateActivationContext(const FGameplayAbilityActorInfo* ActorInfo) const;

	/** @return 是否成功创建并激活 Begin/End Gameplay Event 等待任务 */
	bool StartHitWindowEventTasks();

	/** @return 是否成功创建并激活 ComboWindow Begin/End 事件任务 */
	bool StartComboWindowEventTasks();

	/** @return 当前段是否成功创建标准输入等待任务 */
	bool StartComboInputTask();

	/**
	 * 验证 HitWindow 事件来源
	 *
	 * @param Payload	事件负载
	 * @return 是否来自当前 Avatar、Mesh 和 Montage
	 */
	bool IsCurrentStepEvent(const FGameplayEventData& Payload) const;

	/**
	 * 验证窗口 End 事件来源
	 *
	 * @param Payload	事件负载
	 * @return 是否来自当前 Avatar、Mesh 以及正在播放或刚自然结束的 Montage
	 */
	bool IsCurrentStepEndEvent(const FGameplayEventData& Payload) const;

	/**
	 * 把 GameplayEvent 数值转换为严格攻击段索引
	 *
	 * @param Payload		事件负载
	 * @param OutStepIndex	输出的攻击段索引
	 * @return 数值是否为一至三对应的有限整数
	 */
	static bool DecodeStepIndex(const FGameplayEventData& Payload, int32& OutStepIndex);

	/**
	 * 使用已确认的 Montage Section 进入下一攻击段
	 *
	 * @param EventStepIndex	事件携带的攻击段索引
	 * @return 当前段或下一段是否通过进入验证
	 */
	bool TryEnterStepFromEvent(int32 EventStepIndex);

	/**
	 * 冻结指定攻击段的数据
	 *
	 * @param StepIndex	待冻结的攻击段索引
	 * @return 攻击段是否存在且有效
	 */
	bool FreezeCurrentStep(int32 StepIndex);

	/**
	 * 记录一次连招输入拒绝
	 *
	 * @param Reason	拒绝原因
	 */
	void RecordComboInputRejected(EWuwaMeleeComboInputRejectReason Reason);

	/** @return 已进入与未进入攻击段的窗口计数是否完整 */
	bool ValidateCompletedStepWindows() const;

	/** @return 当前已验证 Avatar 的 CombatExecution 窄门面 */
	UWuwaCombatExecutionComponent* ResolveCombatExecutionComponent() const;

	/**
	 * 构造当前 AttackDefinition 的冻结窗口请求
	 *
	 * @param OutRequest	接收冻结窗口请求
	 * @return Definition、Weapon 和 Socket 是否有效
	 */
	bool BuildMeleeWindowRequest(FWuwaMeleeWindowRequest& OutRequest) const;

	/** @return Authority 是否成功创建并保存唯一 CombatWindow */
	bool OpenAuthorityCombatWindow();

	/**
	 * 关闭 Authority 当前 CombatWindow
	 *
	 * @param Reason	窗口结束原因
	 * @return 没有窗口或当前窗口成功关闭时返回 true
	 */
	bool CloseAuthorityCombatWindow(EWuwaCombatWindowEndReason Reason);

	/** @return 是否为 Authority 绑定了唯一命中事实委托 */
	bool BindCombatHitFactDelegate();

	/** 解绑当前命中事实委托 */
	void UnbindCombatHitFactDelegate();

	/**
	 * 验证权威命中事实是否属于当前 Ability 窗口
	 *
	 * @param Fact	待验证命中事实
	 * @return 是否属于当前活动窗口和 Avatar
	 */
	bool IsCurrentCombatHitFact(const FWuwaCombatHitFact& Fact) const;

	/**
	 * 接收 CombatExecution 发布的权威命中事实
	 *
	 * @param Fact	权威命中事实
	 */
	void HandleCombatHitFact(const FWuwaCombatHitFact& Fact);

	/**
	 * 解析并验证命中事实对应的目标 ASC
	 *
	 * @param Fact		权威命中事实
	 * @param OutTargetASC	接收已验证目标 ASC
	 * @return 目标是否仍可接受本次伤害
	 */
	bool ResolveDamageTarget(const FWuwaCombatHitFact& Fact, UWuwaAbilitySystemComponent*& OutTargetASC) const;

	/**
	 * 构造并应用唯一伤害 GameplayEffect
	 *
	 * @param Fact	权威命中事实
	 * @return 伤害效果是否成功执行
	 */
	bool ApplyDamageToTarget(const FWuwaCombatHitFact& Fact);

	/**
	 * 在目标 ASC 上执行 Authority Hit Cue
	 *
	 * @param Fact		权威命中事实
	 * @param TargetASC	已验证目标 ASC
	 */
	void ExecuteAuthorityHitCue(const FWuwaCombatHitFact& Fact, UWuwaAbilitySystemComponent& TargetASC) const;

	/**
	 * 请求以统一路径结束 Ability
	 *
	 * @param Reason		结束原因
	 * @param bCancelled	是否按取消语义结束
	 */
	void FinishAbility(EWuwaMeleeAttackEndReason Reason, bool bCancelled);

	/** 执行任务、Montage 和双侧攻击状态的幂等清理 */
	void CleanupAttackState();

	/** 接收合法 HitWindow Begin Event */
	UFUNCTION()
	void HandleHitWindowBegin(FGameplayEventData Payload);

	/** 接收合法 HitWindow End Event */
	UFUNCTION()
	void HandleHitWindowEnd(FGameplayEventData Payload);

	/** 接收活动 Ability 的新按下输入 */
	UFUNCTION()
	void HandleComboInputPressed(float TimeWaited);

	/** 接收合法 ComboWindow Begin Event */
	UFUNCTION()
	void HandleComboWindowBegin(FGameplayEventData Payload);

	/** 接收合法 ComboWindow End Event */
	UFUNCTION()
	void HandleComboWindowEnd(FGameplayEventData Payload);

	/** Montage 正常播放完成 */
	UFUNCTION()
	void HandleMontageCompleted();

	/** Montage 被中断 */
	UFUNCTION()
	void HandleMontageInterrupted();

	/** Montage 任务被取消 */
	UFUNCTION()
	void HandleMontageCancelled();
};
