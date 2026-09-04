#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "WuwaAnimNotify_ActionEvent.generated.h"

/** 将 Montage 时间点转换为通用 Action Event 的无业务分支 Notify */
UCLASS(CollapseCategories, meta = (DisplayName = "Wuwa Action Event"))
class WUWA_API UWuwaAnimNotify_ActionEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** 需要发布给当前 Action 的事件语义 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Event", meta = (Categories = "Action.Event"))
	FGameplayTag EventTag;

	//~ Begin UAnimNotify Interface
	virtual void Notify(USkeletalMeshComponent* MeshComp,
	                    UAnimSequenceBase* Animation,
	                    const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
	//~ End UAnimNotify Interface
};
