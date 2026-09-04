// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Runtime/WuwaWeaponComponent.h"

#include "Combat/Data/WuwaWeaponDefinition.h"
#include "Combat/WuwaCombatLog.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

UWuwaWeaponComponent::UWuwaWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

bool UWuwaWeaponComponent::Initialize(USkeletalMeshComponent* CharacterMesh,
                                      UStaticMeshComponent* WeaponMesh,
                                      UStaticMeshComponent* ScabbardMesh,
                                      const UWuwaWeaponDefinition* Definition)
{
	if (BoundCharacterMesh.Get() == CharacterMesh && BoundWeaponMesh.Get() == WeaponMesh &&
	    BoundDefinition.Get() == Definition && IsInitialized())
	{
		return true;
	}

	Shutdown();

	if (!IsValid(CharacterMesh) || !IsValid(WeaponMesh) || !IsValid(Definition) || !Definition->IsRuntimeValid() ||
	    !CharacterMesh->DoesSocketExist(Definition->GetCharacterAttachSocket()) ||
	    !Definition->GetStaticMesh()->FindSocket(Definition->GetTraceBaseSocket()) ||
	    !Definition->GetStaticMesh()->FindSocket(Definition->GetTraceTipSocket()) ||
	    !Definition->GetScabbardStaticMesh()->FindSocket(Definition->GetSheathedWeaponSocket()))
	{
		UE_LOG(LogWuwaCombat,
		       Warning,
		       TEXT("武器装配失败：CharacterMesh、WeaponMesh、Definition 或挂载点无效。Owner=%s, AttachSocket=%s"),
		       *GetNameSafe(GetOwner()),
		       IsValid(Definition) ? *Definition->GetCharacterAttachSocket().ToString() : TEXT("None"));
		return false;
	}

	WeaponMesh->SetStaticMesh(Definition->GetStaticMesh());
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);
	ScabbardMesh->SetStaticMesh(Definition->GetScabbardStaticMesh());
	ScabbardMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ScabbardMesh->SetGenerateOverlapEvents(false);

	ScabbardMesh->AttachToComponent(CharacterMesh,
	                                FAttachmentTransformRules::SnapToTargetNotIncludingScale,
	                                Definition->GetScabbardCharacterAttachSocket());

	ScabbardMesh->SetRelativeTransform(Definition->GetScabbardRelativeTransform());

	WeaponMesh->AttachToComponent(CharacterMesh,
	                              FAttachmentTransformRules::SnapToTargetNotIncludingScale,
	                              Definition->GetCharacterAttachSocket());
	WeaponMesh->SetRelativeTransform(Definition->GetRelativeTransform());

	BoundCharacterMesh = CharacterMesh;
	BoundWeaponMesh = WeaponMesh;
	BoundScabbardMesh = ScabbardMesh;
	BoundDefinition = Definition;

	if (!AttachWeaponToScabbard())
	{
		Shutdown();
		return false;
	}

	FVector Base = FVector::ZeroVector;
	FVector Tip = FVector::ZeroVector;
	if (!GetBladeWorldEndpoints(Base, Tip))
	{
		UE_LOG(LogWuwaCombat, Error, TEXT("武器装配失败：剑刃世界端点无效或重合。Owner=%s"), *GetNameSafe(GetOwner()));
		Shutdown();
		return false;
	}

	return true;
}

void UWuwaWeaponComponent::Shutdown()
{
	UStaticMeshComponent* WeaponMesh = BoundWeaponMesh.Get();
	if (IsValid(WeaponMesh))
	{
		WeaponMesh->SetStaticMesh(nullptr);
		WeaponMesh->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		WeaponMesh->SetRelativeTransform(FTransform::Identity);
	}

	UStaticMeshComponent* ScabbardMesh = BoundScabbardMesh.Get();

	if (IsValid(ScabbardMesh))
	{
		ScabbardMesh->SetStaticMesh(nullptr);
		ScabbardMesh->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		ScabbardMesh->SetRelativeTransform(FTransform::Identity);
	}

	BoundCharacterMesh.Reset();
	BoundWeaponMesh.Reset();
	BoundScabbardMesh.Reset();
	BoundDefinition.Reset();

	WeaponAttachLocation = EWuwaWeaponAttachLocation::Hand;
}

bool UWuwaWeaponComponent::GetBladeWorldEndpoints(FVector& OutBase, FVector& OutTip) const
{
	const UWuwaWeaponDefinition* Definition = BoundDefinition.Get();
	if (!IsValid(Definition))
	{
		OutBase = FVector::ZeroVector;
		OutTip = FVector::ZeroVector;
		return false;
	}

	return GetBladeWorldEndpoints(Definition->GetTraceBaseSocket(), Definition->GetTraceTipSocket(), OutBase, OutTip);
}

bool UWuwaWeaponComponent::GetBladeWorldEndpoints(const FName TraceBaseSocket,
                                                  const FName TraceTipSocket,
                                                  FVector& OutBase,
                                                  FVector& OutTip) const
{
	OutBase = FVector::ZeroVector;
	OutTip = FVector::ZeroVector;

	const UStaticMeshComponent* WeaponMesh = BoundWeaponMesh.Get();
	if (!IsValid(WeaponMesh) || TraceBaseSocket.IsNone() || TraceTipSocket.IsNone() ||
	    TraceBaseSocket == TraceTipSocket || !WeaponMesh->DoesSocketExist(TraceBaseSocket) ||
	    !WeaponMesh->DoesSocketExist(TraceTipSocket))
	{
		return false;
	}

	OutBase = WeaponMesh->GetSocketLocation(TraceBaseSocket);
	OutTip = WeaponMesh->GetSocketLocation(TraceTipSocket);
	return !OutBase.ContainsNaN() && !OutTip.ContainsNaN() && !OutBase.Equals(OutTip, KINDA_SMALL_NUMBER);
}

bool UWuwaWeaponComponent::AttachWeaponToHand()
{
	USkeletalMeshComponent* CharacterMesh = BoundCharacterMesh.Get();

	UStaticMeshComponent* WeaponMesh = BoundWeaponMesh.Get();

	const UWuwaWeaponDefinition* Definition = BoundDefinition.Get();

	if (!IsValid(CharacterMesh) || !IsValid(WeaponMesh) || !IsValid(Definition))
	{
		return false;
	}

	if (!CharacterMesh->DoesSocketExist(Definition->GetCharacterAttachSocket()))
	{
		return false;
	}

	WeaponMesh->AttachToComponent(CharacterMesh,
	                              FAttachmentTransformRules::SnapToTargetNotIncludingScale,
	                              Definition->GetCharacterAttachSocket());

	WeaponMesh->SetRelativeTransform(Definition->GetRelativeTransform());

	WeaponAttachLocation = EWuwaWeaponAttachLocation::Hand;

	return true;
}

bool UWuwaWeaponComponent::AttachWeaponToScabbard()
{
	UStaticMeshComponent* WeaponMesh = BoundWeaponMesh.Get();

	UStaticMeshComponent* ScabbardMesh = BoundScabbardMesh.Get();

	const UWuwaWeaponDefinition* Definition = BoundDefinition.Get();

	if (!IsValid(WeaponMesh) || !IsValid(ScabbardMesh) || !IsValid(Definition))
	{
		return false;
	}

	if (!ScabbardMesh->DoesSocketExist(Definition->GetSheathedWeaponSocket()))
	{
		return false;
	}

	WeaponMesh->AttachToComponent(
	    ScabbardMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Definition->GetSheathedWeaponSocket());

	WeaponMesh->SetRelativeTransform(Definition->GetSheathedWeaponRelativeTransform());

	WeaponAttachLocation = EWuwaWeaponAttachLocation::Scabbard;

	return true;
}

bool UWuwaWeaponComponent::SetWeaponAttachLocation(EWuwaWeaponAttachLocation NewLocation)
{
	if (WeaponAttachLocation == NewLocation)
	{
		return true;
	}

	switch (NewLocation)
	{
		case EWuwaWeaponAttachLocation::Hand:
			return AttachWeaponToHand();

		case EWuwaWeaponAttachLocation::Scabbard:
			return AttachWeaponToScabbard();

		default:
			return false;
	}
}

bool UWuwaWeaponComponent::IsInitialized() const
{
	FVector Base = FVector::ZeroVector;
	FVector Tip = FVector::ZeroVector;
	return BoundCharacterMesh.IsValid() && BoundWeaponMesh.IsValid() && BoundDefinition.IsValid() &&
	       GetBladeWorldEndpoints(Base, Tip);
}

const UWuwaWeaponDefinition* UWuwaWeaponComponent::GetWeaponDefinition() const
{
	return BoundDefinition.Get();
}

FWuwaWeaponRuntimeSnapshot UWuwaWeaponComponent::GetRuntimeSnapshot() const
{
	FWuwaWeaponRuntimeSnapshot Snapshot;
	const UStaticMeshComponent* WeaponMesh = BoundWeaponMesh.Get();
	const UWuwaWeaponDefinition* Definition = BoundDefinition.Get();
	Snapshot.bInitialized = IsInitialized();
	Snapshot.WeaponDefinitionName = GetNameSafe(Definition);
	if (IsValid(Definition))
	{
		Snapshot.CharacterAttachSocket = Definition->GetCharacterAttachSocket();
		Snapshot.bIsPlaceholder = Definition->IsPlaceholder();
		Snapshot.bTraceBaseSocketValid =
		    IsValid(WeaponMesh) && WeaponMesh->DoesSocketExist(Definition->GetTraceBaseSocket());
		Snapshot.bTraceTipSocketValid =
		    IsValid(WeaponMesh) && WeaponMesh->DoesSocketExist(Definition->GetTraceTipSocket());
		GetBladeWorldEndpoints(Snapshot.TraceBaseWorldPosition, Snapshot.TraceTipWorldPosition);
	}
	return Snapshot;
}

void UWuwaWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Shutdown();
	Super::EndPlay(EndPlayReason);
}
