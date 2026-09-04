// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystemInterface.h"
#include "Combat/Contracts/WuwaCombatTargetInterface.h"
#include "Combat/Contracts/WuwaDamageReceiverInterface.h"
#include "GameFramework/Actor.h"
#include "WuwaDamageDummyActor.generated.h"

class UCapsuleComponent;
class UAbilitySystemComponent;
class UAnimMontage;
class UGameplayEffect;
class USkeletalMeshComponent;
class UWuwaAbilitySystemComponent;
class UWuwaCombatSet;
class UWuwaHealthComponent;
class UWuwaHealthSet;
class UWuwaPoiseComponent;
class UWuwaPoiseSet;
class UWuwaWorldHealthBarComponent;
struct FWuwaDeathFact;
struct FWuwaHealthChangedFact;

/** 自持受伤 GAS 上下文且死亡后销毁的轻量战斗假人 */
UCLASS(Blueprintable)
class WUWA_API AWuwaDamageDummyActor : public AActor,
                                       public IAbilitySystemInterface,
                                       public IWuwaCombatTargetInterface,
                                       public IWuwaDamageReceiverInterface
{
	GENERATED_BODY()

public:
	/** 创建只包含碰撞、显示与受伤运行时的默认子对象 */
	AWuwaDamageDummyActor();

	//~ Begin IAbilitySystemInterface Interface
	/** @return 假人自身持有的 ASC */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface Interface

	/** @return 假人自身持有的 Wuwa ASC */
	UWuwaAbilitySystemComponent* GetWuwaAbilitySystemComponent() const;

	/** @return 假人生命属性集 */
	const UWuwaHealthSet* GetHealthSet() const;

	/** @return 假人韧性属性集 */
	const UWuwaPoiseSet* GetPoiseSet() const;

	/** @return 假人战斗数值属性集 */
	const UWuwaCombatSet* GetCombatSet() const;

	/** @return 假人生命门面 */
	UWuwaHealthComponent* GetHealthComponent() const;

	/** @return 假人韧性门面 */
	UWuwaPoiseComponent* GetPoiseComponent() const;

	/** @return 假人的骨骼显示组件 */
	USkeletalMeshComponent* GetDisplayMesh() const;

	/** @return 假人的世界血条组件 */
	UWuwaWorldHealthBarComponent* GetWorldHealthBarComponent() const;

	/** @return 假人的非致死受击蒙太奇 */
	UAnimMontage* GetHitReactionMontage() const;

	//~ Begin IWuwaCombatTargetInterface Interface
	/**
	 * 评估假人是否接受当前中性战斗命中
	 *
	 * @param Query	服务端权威命中查询
	 * @return 假人的当前受击资格
	 */
	virtual FWuwaCombatTargetResponse
	EvaluateCombatTarget_Implementation(const FWuwaCombatTargetQuery& Query) const override;
	//~ End IWuwaCombatTargetInterface Interface

	//~ Begin IWuwaDamageReceiverInterface Interface
	/** @return 假人的伤害接收 ASC */
	virtual UWuwaAbilitySystemComponent* GetDamageReceiverAbilitySystemComponent() const override;

	/** @return 假人的伤害接收生命门面 */
	virtual UWuwaHealthComponent* GetDamageReceiverHealthComponent() const override;

	/** @return 假人的伤害接收韧性门面 */
	virtual UWuwaPoiseComponent* GetDamageReceiverPoiseComponent() const override;
	//~ End IWuwaDamageReceiverInterface Interface

protected:
	//~ Begin AActor Interface
	/** 初始化假人自持受伤运行时 */
	virtual void BeginPlay() override;

	/** 对称释放假人受伤运行时 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

private:
	/** 阻挡近战检测通道的根胶囊体 */
	UPROPERTY(VisibleAnywhere,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|DamageDummy",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> CollisionCapsule;

	/** 由关卡实例或 Blueprint 子类指定资产的无碰撞网格 */
	UPROPERTY(VisibleAnywhere,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|DamageDummy",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> DisplayMesh;

	/** 假人自身持有并参与复制的 ASC */
	UPROPERTY(VisibleAnywhere, Category = "Wuwa|Combat|DamageDummy")
	TObjectPtr<UWuwaAbilitySystemComponent> AbilitySystemComponent;

	/** 假人生命属性集 */
	UPROPERTY()
	TObjectPtr<UWuwaHealthSet> HealthSet;

	/** 假人韧性属性集 */
	UPROPERTY()
	TObjectPtr<UWuwaPoiseSet> PoiseSet;

	/** 假人战斗数值属性集 */
	UPROPERTY()
	TObjectPtr<UWuwaCombatSet> CombatSet;

	/** 绑定假人 ASC 并发布生命与死亡事实的组件 */
	UPROPERTY(VisibleAnywhere, Category = "Wuwa|Combat|DamageDummy")
	TObjectPtr<UWuwaHealthComponent> HealthComponent;

	/** 绑定假人 ASC 并发布破韧事实的组件 */
	UPROPERTY(VisibleAnywhere, Category = "Wuwa|Combat|DamageDummy")
	TObjectPtr<UWuwaPoiseComponent> PoiseComponent;

	/** 订阅真实生命变化的世界血条表现组件 */
	UPROPERTY(VisibleAnywhere,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|DamageDummy",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaWorldHealthBarComponent> WorldHealthBarComponent;

	/** 非致死生命下降时播放的受击蒙太奇 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|DamageDummy|Presentation",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> HitReactionMontage;

	/** Authority 初始化的最大生命值 */
	UPROPERTY(EditAnywhere,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|DamageDummy",
	          meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float InitialMaxHealth = 100.f;

	/** Authority 初始化的最大韧性值 */
	UPROPERTY(EditAnywhere,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|DamageDummy",
	          meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float InitialMaxPoise = 30.f;

	/** Authority 初始化的防御力 */
	UPROPERTY(EditAnywhere,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|DamageDummy",
	          meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float InitialDefense = 5.f;

	/** 授予死亡状态标签的无限 GameplayEffect */
	UPROPERTY(EditDefaultsOnly, Category = "Wuwa|Combat|DamageDummy")
	TSubclassOf<UGameplayEffect> DeadEffectClass;

	/** HealthComponent 死亡事实委托 Handle */
	FDelegateHandle DeathFactDelegateHandle;

	/** HealthComponent 生命变化事实委托 Handle */
	FDelegateHandle HealthChangedFactDelegateHandle;

	/** 受伤运行时是否已完整初始化 */
	bool bDamageRuntimeReady = false;

	/** Authority 是否已提交销毁请求 */
	bool bDestroyRequested = false;

	/** 是否已经输出受击蒙太奇配置报警 */
	bool bHitReactionWarningEmitted = false;

	/** @return ASC、属性与生命组件是否完成完整装配 */
	bool InitializeDamageRuntime();

	/** @return Authority 是否成功写入全部受伤初值 */
	bool InitializeAuthorityAttributes();

	/** 绑定假人唯一死亡事实委托 */
	void BindDeathFactDelegate();

	/** 解绑假人死亡事实委托 */
	void UnbindDeathFactDelegate();

	/** 绑定假人唯一生命变化事实委托 */
	void BindHealthChangedFactDelegate();

	/** 解绑假人生命变化事实委托 */
	void UnbindHealthChangedFactDelegate();

	/**
	 * 接收真实生命变化并筛选非致死伤害表现
	 *
	 * @param Fact	当前生命组件发布的生命变化事实
	 */
	void HandleHealthChangedFact(const FWuwaHealthChangedFact& Fact);

	/** 在具备有效骨骼表现上下文时从头播放受击蒙太奇 */
	void PlayHitReactionMontage();

	/**
	 * 在 Authority 关闭受击并销毁死亡假人
	 *
	 * @param Fact	当前生命组件发布的死亡事实
	 * @return 无
	 */
	void HandleDeathFact(const FWuwaDeathFact& Fact);
};
