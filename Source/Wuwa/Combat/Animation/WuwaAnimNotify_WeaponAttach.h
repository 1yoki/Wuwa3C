#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Combat/Runtime/WuwaWeaponComponent.h"
#include "WuwaAnimNotify_WeaponAttach.generated.h"

UCLASS()
class WUWA_API UWuwaAnimNotify_WeaponAttach : public UAnimNotify
{
	GENERATED_BODY()
public:
	virtual void Notify(USkeletalMeshComponent* MeshComp,
	                    UAnimSequenceBase* Animation,
	                    const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

private:
	/** Notify 触发后武器应该挂在哪里 */
	UPROPERTY(EditAnywhere, Category = "Wuwa|Combat|Weapon")
	EWuwaWeaponAttachLocation TargetLocation = EWuwaWeaponAttachLocation::Hand;
};
