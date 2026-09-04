#include "Movement/Network/WuwaCharacterNetworkMoveData.h"

#include "Movement/Network/WuwaSavedMoveCharacter.h"
#include "Movement/WuwaCharacterMovementComponent.h"

namespace
{
constexpr uint32 NetworkMovementCommandKindCount = 5;
constexpr uint32 JumpRequestKindCount = 3;
constexpr uint32 LegacyActionExitKindCount = 3;
constexpr uint32 NetworkActionRejectReasonCount = 8;

bool SerializeGeneration(FArchive& Archive, FWuwaNetworkActionGeneration& Generation)
{
	uint32 PackedValue = Archive.IsSaving() && Generation.IsValid() ? static_cast<uint32>(Generation.Value) : 0;
	Archive.SerializeIntPacked(PackedValue);
	if (Archive.IsLoading())
	{
		if (PackedValue > static_cast<uint32>(MAX_int32))
		{
			Archive.SetError();
			return false;
		}
		Generation.Value = static_cast<int32>(PackedValue);
	}
	return !Archive.IsError();
}

uint16 QuantizeYaw(const float Yaw)
{
	return FRotator::CompressAxisToShort(FRotator::NormalizeAxis(Yaw));
}

float DequantizeYaw(const uint16 QuantizedYaw)
{
	return FRotator::DecompressAxisFromShort(QuantizedYaw);
}

bool SerializeNormalizedInput(FArchive& Archive, FVector2D& Input)
{
	uint8 QuantizedX =
	    Archive.IsSaving()
	        ? static_cast<uint8>(static_cast<int8>(FMath::RoundToInt(FMath::Clamp(Input.X, -1.f, 1.f) * 127.f)))
	        : 0;
	uint8 QuantizedY =
	    Archive.IsSaving()
	        ? static_cast<uint8>(static_cast<int8>(FMath::RoundToInt(FMath::Clamp(Input.Y, -1.f, 1.f) * 127.f)))
	        : 0;
	Archive.SerializeBits(&QuantizedX, 8);
	Archive.SerializeBits(&QuantizedY, 8);
	if (Archive.IsLoading())
	{
		Input.X = static_cast<float>(static_cast<int8>(QuantizedX)) / 127.f;
		Input.Y = static_cast<float>(static_cast<int8>(QuantizedY)) / 127.f;
	}
	return !Archive.IsError();
}

bool SerializeNetworkCommand(FArchive& Archive, UPackageMap* PackageMap, FWuwaPendingNetworkMovementCommand& Command)
{
	bool bHasCommand = Archive.IsSaving() && Command.HasData();
	Archive.SerializeBits(&bHasCommand, 1);
	if (!bHasCommand)
	{
		if (Archive.IsLoading())
		{
			Command.Reset();
		}
		return !Archive.IsError();
	}

	if (Archive.IsLoading())
	{
		Command.Reset();
	}

	uint32 Kind = Archive.IsSaving() ? static_cast<uint32>(Command.Kind) : 0;
	Archive.SerializeInt(Kind, NetworkMovementCommandKindCount);
	if (Archive.IsLoading())
	{
		Command.Kind = static_cast<EWuwaNetworkMovementCommandKind>(Kind);
	}

	Archive.SerializeBits(&Command.Flags, 3);
	if (Command.Kind != EWuwaNetworkMovementCommandKind::None && !SerializeGeneration(Archive, Command.Generation))
	{
		return false;
	}

	if (Command.Kind == EWuwaNetworkMovementCommandKind::Jump)
	{
		uint32 JumpType = Archive.IsSaving() ? static_cast<uint32>(Command.JumpType) : 0;
		Archive.SerializeInt(JumpType, JumpRequestKindCount);
		if (Archive.IsLoading())
		{
			Command.JumpType = static_cast<EWuwaJumpRequestKind>(JumpType);
		}
	}
	else if (Command.Kind == EWuwaNetworkMovementCommandKind::LegacyAction)
	{
		bool bTagSuccess = true;
		Command.ActionTag.NetSerialize(Archive, PackageMap, bTagSuccess);
		uint16 DirectionYaw = Archive.IsSaving() ? QuantizeYaw(Command.WorldDirection.Rotation().Yaw) : 0;
		Archive.SerializeBits(&DirectionYaw, 16);
		if (Archive.IsLoading())
		{
			const float DirectionRadians = FMath::DegreesToRadians(DequantizeYaw(DirectionYaw));
			Command.WorldDirection = FVector(FMath::Cos(DirectionRadians), FMath::Sin(DirectionRadians), 0.f);
		}
		if (!bTagSuccess)
		{
			Archive.SetError();
			return false;
		}

		uint32 PackedAirCycle = Archive.IsSaving() ? static_cast<uint32>(Command.AirCycleGeneration) : 0;
		Archive.SerializeIntPacked(PackedAirCycle);
		if (Archive.IsLoading())
		{
			if (PackedAirCycle > static_cast<uint32>(MAX_int32))
			{
				Archive.SetError();
				return false;
			}
			Command.AirCycleGeneration = static_cast<int32>(PackedAirCycle);
		}
	}
	else if (Command.Kind == EWuwaNetworkMovementCommandKind::Grapple)
	{
		bool bTagSuccess = true;
		Command.ActionTag.NetSerialize(Archive, PackageMap, bTagSuccess);
		if (!bTagSuccess || !SerializeNormalizedInput(Archive, Command.GrappleInputDirection))
		{
			Archive.SetError();
			return false;
		}

		uint16 ViewYaw = Archive.IsSaving() ? QuantizeYaw(Command.GrappleViewYaw) : 0;
		uint16 ViewPitch = Archive.IsSaving() ? QuantizeYaw(Command.GrappleViewPitch) : 0;
		Archive.SerializeBits(&ViewYaw, 16);
		Archive.SerializeBits(&ViewPitch, 16);
		if (Archive.IsLoading())
		{
			Command.GrappleViewYaw = FRotator::NormalizeAxis(DequantizeYaw(ViewYaw));
			Command.GrappleViewPitch = FRotator::NormalizeAxis(DequantizeYaw(ViewPitch));
		}

		uint8 PredictedScale = Archive.IsSaving()
		                           ? static_cast<uint8>(FMath::RoundToInt(
		                                 FMath::Clamp(Command.ClientPredictedGrappleScale, 0.f, 1.f) * 255.f))
		                           : 0;
		Archive.SerializeBits(&PredictedScale, 8);
		if (Archive.IsLoading())
		{
			Command.ClientPredictedGrappleScale = static_cast<float>(PredictedScale) / 255.f;
		}

		uint32 QueryFrame = Archive.IsSaving() ? static_cast<uint32>(Command.ClientGrappleQueryFrame) : 0;
		Archive.SerializeIntPacked(QueryFrame);
		if (Archive.IsLoading())
		{
			if (QueryFrame > static_cast<uint32>(MAX_int32))
			{
				Archive.SetError();
				return false;
			}
			Command.ClientGrappleQueryFrame = static_cast<int32>(QueryFrame);
		}
	}
	else if (Command.Kind == EWuwaNetworkMovementCommandKind::LegacyActionExit)
	{
		if (!SerializeGeneration(Archive, Command.TargetActionGeneration))
		{
			return false;
		}

		bool bTagSuccess = true;
		Command.ActionTag.NetSerialize(Archive, PackageMap, bTagSuccess);
		uint32 ExitKind = Archive.IsSaving() ? static_cast<uint32>(Command.ExitKind) : 0;
		Archive.SerializeInt(ExitKind, LegacyActionExitKindCount);
		if (Archive.IsLoading())
		{
			Command.ExitKind = static_cast<EWuwaLegacyActionExitKind>(ExitKind);
		}
		if (!bTagSuccess || !SerializeNormalizedInput(Archive, Command.ExitMoveIntent))
		{
			Archive.SetError();
			return false;
		}
	}

	if (Command.HasFlag(EWuwaNetworkMovementCommandFlags::HasDesiredFacing))
	{
		uint16 FacingYaw = Archive.IsSaving() ? QuantizeYaw(Command.DesiredFacingYaw) : 0;
		Archive.SerializeBits(&FacingYaw, 16);
		if (Archive.IsLoading())
		{
			Command.DesiredFacingYaw = DequantizeYaw(FacingYaw);
		}
	}

	if (!Command.IsPayloadValid())
	{
		Archive.SetError();
		return false;
	}
	return !Archive.IsError();
}
}

void FWuwaCharacterNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove,
                                                              const ENetworkMoveType MoveType)
{
	Super::ClientFillNetworkMoveData(ClientMove, MoveType);
	const FWuwaSavedMove_Character& WuwaMove = static_cast<const FWuwaSavedMove_Character&>(ClientMove);
	NetworkCommand = WuwaMove.SavedNetworkCommand;
}

bool FWuwaCharacterNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement,
                                              FArchive& Archive,
                                              UPackageMap* PackageMap,
                                              const ENetworkMoveType MoveType)
{
	if (Archive.IsSaving() && NetworkCommand.HasData() && !NetworkCommand.IsPayloadValid())
	{
		Archive.SetError();
		return false;
	}
	if (!Super::Serialize(CharacterMovement, Archive, PackageMap, MoveType))
	{
		return false;
	}
	return SerializeNetworkCommand(Archive, PackageMap, NetworkCommand);
}

FWuwaCharacterNetworkMoveDataContainer::FWuwaCharacterNetworkMoveDataContainer()
{
	NewMoveData = &MoveData[0];
	PendingMoveData = &MoveData[1];
	OldMoveData = &MoveData[2];
}

void FWuwaCharacterMoveResponseDataContainer::ServerFillResponseData(
    const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment)
{
	Super::ServerFillResponseData(CharacterMovement, PendingAdjustment);
	UWuwaCharacterMovementComponent* WuwaMovement =
	    const_cast<UWuwaCharacterMovementComponent*>(Cast<UWuwaCharacterMovementComponent>(&CharacterMovement));
	NetworkResponse = WuwaMovement ? WuwaMovement->TakePendingNetworkActionResponse() : FWuwaNetworkActionResponse();
}

bool FWuwaCharacterMoveResponseDataContainer::Serialize(UCharacterMovementComponent& CharacterMovement,
                                                        FArchive& Archive,
                                                        UPackageMap* PackageMap)
{
	if (Archive.IsSaving() && NetworkResponse.bHasGrappleAuthoritySummary &&
	    !NetworkResponse.IsGrappleAuthoritySummaryValid())
	{
		Archive.SetError();
		return false;
	}

	if (!Super::Serialize(CharacterMovement, Archive, PackageMap))
	{
		return false;
	}

	Archive.SerializeBits(&NetworkResponse.bHasResponse, 1);
	if (!NetworkResponse.bHasResponse)
	{
		if (Archive.IsLoading())
		{
			NetworkResponse = FWuwaNetworkActionResponse();
		}
		return !Archive.IsError();
	}

	if (!SerializeGeneration(Archive, NetworkResponse.ProcessedGeneration))
	{
		return false;
	}
	Archive.SerializeBits(&NetworkResponse.bAccepted, 1);

	uint32 RejectReason = Archive.IsSaving() ? static_cast<uint32>(NetworkResponse.RejectReason) : 0;
	Archive.SerializeInt(RejectReason, NetworkActionRejectReasonCount);
	if (Archive.IsLoading())
	{
		NetworkResponse.RejectReason = static_cast<EWuwaNetworkActionRejectReason>(RejectReason);
	}

	if (!SerializeGeneration(Archive, NetworkResponse.AuthorityGeneration))
	{
		return false;
	}
	Archive.SerializeBits(&NetworkResponse.AuthorityMovementMode, 8);

	Archive.SerializeBits(&NetworkResponse.bHasGrappleAuthoritySummary, 1);
	if (NetworkResponse.bHasGrappleAuthoritySummary)
	{
		uint8 TrajectoryScale =
		    Archive.IsSaving() ? static_cast<uint8>(FMath::RoundToInt(
		                             FMath::Clamp(NetworkResponse.AuthorityGrappleTrajectoryScale, 0.f, 1.f) * 255.f))
		                       : 0;
		uint16 TravelYaw = Archive.IsSaving() ? QuantizeYaw(NetworkResponse.AuthorityGrappleTravelYaw) : 0;
		Archive.SerializeBits(&TrajectoryScale, 8);
		Archive.SerializeBits(&TravelYaw, 16);
		if (Archive.IsLoading())
		{
			NetworkResponse.AuthorityGrappleTrajectoryScale = static_cast<float>(TrajectoryScale) / 255.f;
			NetworkResponse.AuthorityGrappleTravelYaw = FRotator::NormalizeAxis(DequantizeYaw(TravelYaw));
		}

		bool bLocationSuccess = true;
		NetworkResponse.AuthorityGrappleStartLocation.NetSerialize(Archive, PackageMap, bLocationSuccess);
		if (!bLocationSuccess)
		{
			Archive.SetError();
			return false;
		}
	}
	else if (Archive.IsLoading())
	{
		NetworkResponse.AuthorityGrappleTrajectoryScale = 0.f;
		NetworkResponse.AuthorityGrappleTravelYaw = 0.f;
		NetworkResponse.AuthorityGrappleStartLocation = FVector::ZeroVector;
	}

	const bool bValidResponse =
	    NetworkResponse.ProcessedGeneration.IsValid() &&
	    (!NetworkResponse.bAccepted || NetworkResponse.RejectReason == EWuwaNetworkActionRejectReason::None) &&
	    (NetworkResponse.bAccepted || NetworkResponse.RejectReason != EWuwaNetworkActionRejectReason::None) &&
	    (!NetworkResponse.bHasGrappleAuthoritySummary || NetworkResponse.IsGrappleAuthoritySummaryValid());
	if (!bValidResponse)
	{
		Archive.SetError();
		return false;
	}
	return !Archive.IsError();
}
