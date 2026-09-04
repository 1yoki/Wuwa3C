#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "WuwaAnimNotifyState_AbilityInterruptWindow.generated.h"

UCLASS(CollapseCategories, meta = (DisplayName = "Wuwa Combo Window"))
class WUWA_API UWuwaAnimNotifyState_AbilityInterruptWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** 当前连招窗口所属攻击段索引 */
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Interrupt", meta = (ClampMin = "0"))
	int32 WindowId = 0;

private:
	/** 各 Mesh 与攻击段当前有效 ComboWindow 对应的 Montage 实例编号 */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TMap<uint8, int32>> ActiveMeshStepMontageInstanceIds;
};
