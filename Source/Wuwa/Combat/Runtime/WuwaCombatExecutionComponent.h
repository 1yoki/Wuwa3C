// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Combat/Contracts/WuwaCombatFacts.h"
#include "Components/ActorComponent.h"
#include "WuwaCombatExecutionComponent.generated.h"

class AActor;
class USkeletalMeshComponent;
class UWuwaWeaponComponent;
struct FHitResult;

/** 服务端权威剑刃 Sweep 与命中事实发布组件 */
UCLASS(ClassGroup = "Wuwa")
class WUWA_API UWuwaCombatExecutionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 创建默认关闭 Tick 且不复制的命中执行组件 */
	UWuwaCombatExecutionComponent();

	/**
	 * 幂等装配角色骨骼网格与武器端点来源
	 *
	 * @param CharacterMesh	角色骨骼网格
	 * @param WeaponComponent	武器端点来源
	 * @return 是否完成有效装配
	 */
	bool Initialize(USkeletalMeshComponent* CharacterMesh, UWuwaWeaponComponent* WeaponComponent);

	/**
	 * 终止活动窗口并释放装配引用
	 *
	 * @param Reason	关闭原因
	 * @return 无
	 */
	void Shutdown(EWuwaCombatWindowEndReason Reason);

	/**
	 * 在 Authority 创建唯一近战命中窗口
	 *
	 * @param Request	开始边界冻结的轨迹请求
	 * @return 成功时返回非零窗口句柄
	 */
	FWuwaCombatWindowHandle BeginMeleeWindow(const FWuwaMeleeWindowRequest& Request);

	/**
	 * 使用当前句柄结束近战命中窗口
	 *
	 * @param Handle	待结束窗口
	 * @param Reason	结束原因
	 * @return 是否结束了当前活动窗口
	 */
	bool EndMeleeWindow(FWuwaCombatWindowHandle Handle, EWuwaCombatWindowEndReason Reason);

	/**
	 * 幂等终止当前所有命中窗口
	 *
	 * @param Reason	终止原因
	 * @return 无
	 */
	void AbortAllWindows(EWuwaCombatWindowEndReason Reason);

	/** @return 是否完成有效依赖装配 */
	bool IsInitialized() const;

	/** @return 当前是否存在活动命中窗口 */
	bool HasActiveWindow() const;

	/** @return 当前或最近一次权威命中窗口快照 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Execution")
	FWuwaCombatExecutionRuntimeSnapshot GetRuntimeSnapshot() const;

	/** @return 权威命中事实的中性原生委托 */
	FWuwaCombatHitFactDelegate& OnCombatHitFact();

protected:
	//~ Begin UActorComponent Interface
	virtual void
	TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

private:
	/** 当前绑定的角色骨骼网格 */
	TWeakObjectPtr<USkeletalMeshComponent> BoundCharacterMesh;

	/** 当前绑定的武器端点来源 */
	TWeakObjectPtr<UWuwaWeaponComponent> BoundWeaponComponent;

	/** 当前依赖是否完成装配 */
	bool bInitialized = false;

	/** 当前活动窗口句柄 */
	FWuwaCombatWindowHandle ActiveWindowHandle;

	/** 当前窗口冻结请求 */
	FWuwaMeleeWindowRequest FrozenRequest;

	/** 上一帧剑刃根部位置 */
	FVector PreviousBase = FVector::ZeroVector;

	/** 上一帧剑刃尖端位置 */
	FVector PreviousTip = FVector::ZeroVector;

	/** 当前剑刃根部位置 */
	FVector CurrentBase = FVector::ZeroVector;

	/** 当前剑刃尖端位置 */
	FVector CurrentTip = FVector::ZeroVector;

	/** 当前窗口已经发布过事实的 Actor */
	TSet<TWeakObjectPtr<AActor>> HitActors;

	/** 当前或最近窗口执行的 Sweep 查询数量 */
	int32 SweepCount = 0;

	/** 当前或最近窗口发布的不同目标数量 */
	int32 HitCount = 0;

	/** 最近发布的权威命中事实 */
	FWuwaCombatHitFact LastHitFact;

	/** 最近窗口结束原因 */
	EWuwaCombatWindowEndReason LastEndReason = EWuwaCombatWindowEndReason::None;

	/** 最近窗口失败原因 */
	EWuwaCombatWindowFailureReason LastFailureReason = EWuwaCombatWindowFailureReason::None;

	/** 是否已建立上一帧剑刃姿态 */
	bool bHasPreviousBladePose = false;

	/** 是否已保存并提升 Mesh Tick 选项 */
	bool bMeshTickOptionElevated = false;

	/** 提升前的 Mesh Tick 选项数值 */
	uint8 OriginalMeshTickOption = 0;

	/** 下一个待分配的非零窗口编号 */
	uint32 NextWindowId = 1;

	/** 权威命中事实委托 */
	FWuwaCombatHitFactDelegate CombatHitFactDelegate;

	/** @return 冻结请求的全部字段是否有效 */
	bool ValidateWindowRequest(const FWuwaMeleeWindowRequest& Request) const;

	/** @return 是否能从冻结 Socket 读取有限且不重合的剑刃端点 */
	bool ReadFrozenBladeEndpoints(FVector& OutBase, FVector& OutTip) const;

	/** 执行当前帧全部剑刃采样 Sweep */
	void ExecuteBladeSweeps();

	/**
	 * 过滤并发布当前 Sweep 的命中事实
	 *
	 * @param Hits	当前采样产生的命中结果
	 * @return 无
	 */
	void ProcessSweepHits(const TArray<FHitResult>& Hits);

	/**
	 * 构造提交给中性目标资格契约的查询
	 *
	 * @return 当前活动窗口的中性目标查询
	 */
	FWuwaCombatTargetQuery BuildCombatTargetQuery() const;

	/**
	 * 通过中性 Contract 评估候选目标
	 *
	 * @param TargetActor	候选目标
	 * @return Contract 是否明确接受当前命中
	 */
	bool EvaluateCombatTarget(AActor& TargetActor) const;

	/**
	 * 发布已经通过资格和去重验证的命中事实
	 *
	 * @param Hit	权威碰撞结果
	 * @param TargetActor	已接受的目标
	 * @return 无
	 */
	void PublishHitFact(const FHitResult& Hit, AActor& TargetActor);

	/** @return 是否成功提升并保存服务器骨骼 Tick 选项 */
	bool ElevateMeshTickOption();

	/** 恢复窗口开始前的服务器骨骼 Tick 选项 */
	void RestoreMeshTickOption();

	/** 关闭 Tick 并清空活动窗口资源 */
	void ResetActiveWindow();

	/** @return 新分配的非零单调窗口句柄 */
	FWuwaCombatWindowHandle AllocateWindowHandle();

	/** @return 当前冻结请求生效后的不同目标上限 */
	int32 GetEffectiveMaxTargets() const;
};
