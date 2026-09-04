#include "Combat/Animation/WuwaAnimNotify_WeaponAttach.h"

#include "WuwaCharacter.h"
#include "Components/SkeletalMeshComponent.h"

void UWuwaAnimNotify_WeaponAttach::Notify(USkeletalMeshComponent* MeshComp,
                                          UAnimSequenceBase* Animation,
                                          const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AWuwaCharacter* Character = IsValid(MeshComp) ? Cast<AWuwaCharacter>(MeshComp->GetOwner()) : nullptr;

	if (!IsValid(Character) || MeshComp != Character->GetMesh())
	{
		return;
	}

	UWuwaWeaponComponent* WeaponComponent = Character->GetWeaponComponent();

	if (!IsValid(WeaponComponent) || !WeaponComponent->IsInitialized())
	{
		return;
	}

	WeaponComponent->SetWeaponAttachLocation(TargetLocation);
}

FString UWuwaAnimNotify_WeaponAttach::GetNotifyName_Implementation() const
{
	switch (TargetLocation)
	{
		case EWuwaWeaponAttachLocation::Hand:
			return TEXT("Weapon -> Hand");

		case EWuwaWeaponAttachLocation::Scabbard:
			return TEXT("Weapon -> Scabbard");

		default:
			return TEXT("Weapon Attach");
	}
}
