#include "Animation/Notifies/WuwaAnimNotify_ActionEvent.h"

#include "Animation/Actions/WuwaActionAnimationCapabilityComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UWuwaAnimNotify_ActionEvent::Notify(USkeletalMeshComponent* MeshComp,
                                         UAnimSequenceBase* Animation,
                                         const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	UWuwaActionAnimationCapabilityComponent* AnimationCapability =
	    IsValid(Owner) ? Owner->FindComponentByClass<UWuwaActionAnimationCapabilityComponent>() : nullptr;
	if (IsValid(AnimationCapability))
	{
		AnimationCapability->PublishNotifyEvent(EventTag, MeshComp, Animation);
	}
}

FString UWuwaAnimNotify_ActionEvent::GetNotifyName_Implementation() const
{
	return EventTag.IsValid() ? EventTag.ToString() : TEXT("Wuwa Action Event");
}
