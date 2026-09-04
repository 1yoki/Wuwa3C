// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Data/WuwaWeaponDefinition.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Misc/DataValidation.h"

namespace
{
bool IsFiniteTransform(const FTransform& Transform)
{
	const FVector Translation = Transform.GetTranslation();
	const FVector Scale = Transform.GetScale3D();
	const FQuat Rotation = Transform.GetRotation();
	return !Translation.ContainsNaN() && !Scale.ContainsNaN() && Rotation.IsNormalized() && !Scale.IsNearlyZero() &&
	       FMath::Abs(Scale.X) <= 100.0 && FMath::Abs(Scale.Y) <= 100.0 && FMath::Abs(Scale.Z) <= 100.0;
}
}

bool UWuwaWeaponDefinition::IsRuntimeValid() const
{
	if (!WeaponTag.IsValid() || !IsValid(StaticMesh) || CharacterAttachSocket.IsNone() || TraceBaseSocket.IsNone() ||
	    TraceTipSocket.IsNone() || TraceBaseSocket == TraceTipSocket || !IsFiniteTransform(RelativeTransform))
	{
		return false;
	}

	if (!IsValid(ScabbardStaticMesh) || ScabbardCharacterAttachSocket.IsNone() || SheathedWeaponSocket.IsNone() ||
	    !IsFiniteTransform(ScabbardRelativeTransform) || !IsFiniteTransform(SheathedWeaponRelativeTransform))
	{
		return false;
	}

	if (ScabbardStaticMesh->FindSocket(SheathedWeaponSocket) == nullptr)
	{
		return false;
	}

	const UStaticMeshSocket* BaseSocket = StaticMesh->FindSocket(TraceBaseSocket);
	const UStaticMeshSocket* TipSocket = StaticMesh->FindSocket(TraceTipSocket);
	return IsValid(BaseSocket) && IsValid(TipSocket) &&
	       !BaseSocket->RelativeLocation.Equals(TipSocket->RelativeLocation, KINDA_SMALL_NUMBER);
}

#if WITH_EDITOR

EDataValidationResult UWuwaWeaponDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!IsRuntimeValid())
	{
		Context.AddError(FText::FromString(TEXT("WeaponDefinition 的标签、网格、挂载点、Trace Socket 或相对变换无效")));
		Result = EDataValidationResult::Invalid;
	}

	return Result == EDataValidationResult::Invalid ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}

#endif
