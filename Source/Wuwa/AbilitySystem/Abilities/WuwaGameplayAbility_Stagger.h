// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/WuwaGameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "WuwaGameplayAbility_Stagger.generated.h"

class AActor;
class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class UGameplayEffect;
class USkeletalMeshComponent;
class UWuwaAbilitySystemComponent;
class UWuwaHealthComponent;
struct FWuwaHealthChangedFact;

/** 硬直 Ability 的结束原因 */
UENUM(BlueprintType)
enum class EWuwaStaggerEndReason : uint8
{
	/** 尚未结束 */
	None,

	/** 受击 Montage 正常完成 */
	Completed,

	/** Ability 被外部取消 */
	Cancelled,

	/** 受击 Montage 被中断 */
	Interrupted,

	/** 死亡优先级取消硬直 */
	Death,

	/** Avatar 身份已经变化 */
	AvatarChanged,

	/** 受击 Montage 无法启动 */
	MontageFailed,

	/** 硬直状态效果无法应用 */
	StateEffectFailed,

	/** 正常结束时韧性重置失败 */
	ResetEffectFailed,

	/** AbilitySpec 被移除 */
	AbilityRemoved,

	/** 真实生命变化事实绑定失败 */
	RefreshBindingFailed
};

/** 硬直 Ability 当前或最近一次运行证据 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaStaggerRuntimeSnapshot
{
	GENERATED_BODY()

	/** Ability 当前是否活动 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Stagger")
	bool bActive = false;

	/** 当前激活的清理是否已完成 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Stagger")
	bool bCleanupComplete = true;

	/** 是否仍持有唯一硬直状态效果 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Stagger")
	bool bStateEffectActive = false;

	/** 成功进入激活流程的累计次数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Stagger")
	int32 ActivationCount = 0;

	/** 活动期间由非致死真实掉血触发的累计重播次数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Stagger")
	int32 RefreshCount = 0;

	/** 完成幂等清理的累计次数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Stagger")
	int32 CleanupCount = 0;

	/** 成功通过 GameplayEffect 重置韧性的累计次数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Stagger")
	int32 PoiseResetCount = 0;

	/** ASC 当前硬直标签计数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Stagger")
	int32 StateTagCount = 0;

	/** ASC 当前移动输入阻止标签计数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Stagger")
	int32 MoveBlockTagCount = 0;

	/** 最近一次结束原因 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Stagger")
	EWuwaStaggerEndReason LastEndReason = EWuwaStaggerEndReason::None;

	/** 当前配置的受击 Montage */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Stagger")
	TObjectPtr<UAnimMontage> Montage = nullptr;
};

/** 由权威破韧事件启动的硬直受击反应 Ability */
UCLASS(Blueprintable)
class WUWA_API UWuwaGameplayAbility_Stagger : public UWuwaGameplayAbility
{
	GENERATED_BODY()

public:
	/** 配置事件触发、服务端启动与硬直标签契约 */
	UWuwaGameplayAbility_Stagger();

	/** @return 当前或最近一次硬直生命周期快照 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Stagger")
	FWuwaStaggerRuntimeSnapshot GetRuntimeSnapshot() const;

	/** @return 当前配置的硬直状态效果 */
	TSubclassOf<UGameplayEffect> GetStaggerStateEffectClass() const
	{
		return StaggerStateEffectClass;
	}

	/** @return 当前配置的韧性重置效果 */
	TSubclassOf<UGameplayEffect> GetResetPoiseEffectClass() const
	{
		return ResetPoiseEffectClass;
	}

	/** @return 当前配置的受击 Montage */
	UAnimMontage* GetHitReactMontage() const
	{
		return HitReactMontage;
	}

	/** @return 当前配置的事件触发器 */
	const TArray<FAbilityTriggerData>& GetConfiguredTriggers() const
	{
		return AbilityTriggers;
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
	/** 硬直期间唯一应用到 Avatar ASC 的无限状态效果 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Stagger",
	          meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> StaggerStateEffectClass;

	/** 正常结束时应用到 Avatar ASC 的瞬时韧性重置效果 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Stagger",
	          meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> ResetPoiseEffectClass;

	/** 硬直期间播放的受击 Montage */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Stagger",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> HitReactMontage;

	/** 当前受击 Montage 任务 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	/** 当前激活冻结的生命事实源 */
	TWeakObjectPtr<UWuwaHealthComponent> ActiveHealthComponent;

	/** 当前激活的生命变化事实委托 Handle */
	FDelegateHandle HealthChangedDelegateHandle;

	/** 当前激活冻结的 Avatar */
	TWeakObjectPtr<AActor> ActiveAvatar;

	/** 当前激活冻结的 ASC */
	TWeakObjectPtr<UWuwaAbilitySystemComponent> ActiveAbilitySystemComponent;

	/** 当前激活冻结的 Mesh */
	TWeakObjectPtr<USkeletalMeshComponent> ActiveMesh;

	/** 当前激活唯一持有的状态效果 Handle */
	FActiveGameplayEffectHandle ActiveStateEffectHandle;

	/** 当前激活是否已请求结束 */
	bool bFinishRequested = false;

	/** 当前是否正在执行幂等清理 */
	bool bCleanupInProgress = false;

	/** 当前是否正在替换旧 Montage Task */
	bool bRestartingMontage = false;

	/** 当前激活的幂等清理是否已完成 */
	bool bCleanupComplete = true;

	/** 成功进入激活流程的累计次数 */
	int32 ActivationCount = 0;

	/** 活动期间由非致死真实掉血触发的累计重播次数 */
	int32 RefreshCount = 0;

	/** 完成幂等清理的累计次数 */
	int32 CleanupCount = 0;

	/** 成功通过 GameplayEffect 重置韧性的累计次数 */
	int32 PoiseResetCount = 0;

	/** 当前或最近一次结束原因 */
	EWuwaStaggerEndReason LastEndReason = EWuwaStaggerEndReason::None;

	/**
	 * 验证硬直激活所需的权威上下文与配置
	 *
	 * @param ActorInfo	待验证的 Ability Actor 上下文
	 * @return 是否满足激活契约
	 */
	bool ValidateActivationContext(const FGameplayAbilityActorInfo* ActorInfo) const;

	/**
	 * 验证拥有客户端硬直表现所需的服务端确认上下文
	 *
	 * @param ActorInfo		待验证的 Ability Actor 上下文
	 * @param ActivationInfo	本次服务端确认的激活信息
	 * @param TriggerEventData	本次激活携带的事件数据
	 * @return 是否满足拥有客户端表现契约
	 */
	bool ValidateOwningClientPresentationContext(const FGameplayAbilityActorInfo* ActorInfo,
	                                             const FGameplayAbilityActivationInfo& ActivationInfo,
	                                             const FGameplayEventData* TriggerEventData) const;

	/** @return 当前是否为服务端已确认的拥有客户端表现运行时 */
	bool IsOwningClientPresentationRuntime() const;

	/** @return 是否成功应用并唯一持有硬直状态效果 */
	bool ApplyStaggerState();

	/** @return 是否成功创建并启动受击 Montage 任务 */
	bool StartStaggerMontage();

	/** @return 是否成功绑定当前 Avatar 的真实生命变化事实 */
	bool BindHealthChangedFact();

	/** 解绑当前激活的真实生命变化事实 */
	void UnbindHealthChangedFact();

	/** @return 是否成功从头重播当前受击 Montage */
	bool RestartStaggerMontage();

	/** @return 当前结束原因与运行上下文是否允许恢复韧性 */
	bool ShouldResetPoise() const;

	/** @return 是否成功通过瞬时效果将韧性重置为当前 MaxPoise */
	bool ApplyPoiseReset();

	/** 执行任务、Montage、状态效果与可选韧性恢复的幂等清理 */
	void CleanupStaggerState();

	/**
	 * 记录原因并结束当前硬直
	 *
	 * @param Reason		明确结束原因
	 * @param bCancelled	是否按取消语义结束
	 * @return 无
	 */
	void FinishStagger(EWuwaStaggerEndReason Reason, bool bCancelled);

	/** 受击 Montage 正常完成 */
	UFUNCTION()
	void HandleMontageCompleted();

	/** 受击 Montage 被中断 */
	UFUNCTION()
	void HandleMontageInterrupted();

	/** 受击 Montage 任务被取消 */
	UFUNCTION()
	void HandleMontageCancelled();

	/**
	 * 消费活动硬直期间的真实生命变化事实
	 *
	 * @param Fact	当前 Avatar 的生命变化事实
	 */
	void HandleHealthChangedFact(const FWuwaHealthChangedFact& Fact);
};
