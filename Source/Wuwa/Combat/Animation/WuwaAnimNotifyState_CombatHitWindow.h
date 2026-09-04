// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "WuwaAnimNotifyState_CombatHitWindow.generated.h"

/** 将动画有效帧转换为 GAS HitWindow Begin/End Event 的无副作用 NotifyState */
UCLASS(CollapseCategories, meta = (DisplayName = "Wuwa Combat Hit Window"))
class WUWA_API UWuwaAnimNotifyState_CombatHitWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** 创建使用原生分支点调度的命中窗口 */
	UWuwaAnimNotifyState_CombatHitWindow();

	/** 当前命中窗口所属攻击段索引 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat", meta = (ClampMin = "0", ClampMax = "2"))
	uint8 StepIndex = 0;

	//~ Begin UAnimNotifyState Interface
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp,
	                         UAnimSequenceBase* Animation,
	                         float TotalDuration,
	                         const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp,
	                       UAnimSequenceBase* Animation,
	                       const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
	//~ End UAnimNotifyState Interface

private:
	/** 各 Mesh 与攻击段当前有效 HitWindow 对应的 Montage 实例编号 */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TMap<uint8, int32>> ActiveMeshStepMontageInstanceIds;
};
