#include "Movement/Network/WuwaSavedMoveCharacter.h"

#include "GameFramework/Character.h"
#include "Movement/WuwaCharacterMovementComponent.h"

void FWuwaSavedMove_Character::Clear()
{
	Super::Clear();
	SavedNetworkCommand.Reset();
	SavedGrappleState.Reset();
	SavedMovementActionState.Reset();
}

void FWuwaSavedMove_Character::SetMoveFor(ACharacter* Character,
                                          const float InDeltaTime,
                                          const FVector& NewAcceleration,
                                          FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAcceleration, ClientData);

	UWuwaCharacterMovementComponent* Movement =
	    Character ? Cast<UWuwaCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	SavedNetworkCommand =
	    Movement ? Movement->CapturePendingNetworkMovementCommandForSavedMove() : FWuwaPendingNetworkMovementCommand();
	if (Movement)
	{
		Movement->PrepareMovementActionRootMotionStartAtMoveBoundary(SavedNetworkCommand, TimeStamp, InDeltaTime);
		Movement->CaptureGrappleNetworkReplayState(SavedGrappleState);
		Movement->CaptureMovementActionNetworkReplayState(SavedMovementActionState);
		Movement->ApplyLocalPredictedMovementActionExitAtSavedMoveBoundary(SavedNetworkCommand);
	}
	else
	{
		SavedGrappleState.Reset();
		SavedMovementActionState.Reset();
	}
	if (SavedNetworkCommand.IsOneShot() || SavedGrappleState.bActive)
	{
		bForceNoCombine = true;
	}
}

void FWuwaSavedMove_Character::PrepMoveFor(ACharacter* Character)
{
	Super::PrepMoveFor(Character);

	UWuwaCharacterMovementComponent* Movement =
	    Character ? Cast<UWuwaCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	if (Movement)
	{
		Movement->RestoreGrappleNetworkReplayState(SavedGrappleState);
		Movement->RestoreMovementActionNetworkReplayState(SavedMovementActionState);
		Movement->RestoreNetworkMovementCommandForReplay(SavedNetworkCommand);
	}
}

uint8 FWuwaSavedMove_Character::GetCompressedFlags() const
{
	uint8 CompressedFlags = Super::GetCompressedFlags();
	if (SavedNetworkCommand.Kind == EWuwaNetworkMovementCommandKind::Jump)
	{
		if (SavedNetworkCommand.JumpType == EWuwaJumpRequestKind::Pressed)
		{
			CompressedFlags |= FLAG_JumpPressed;
		}
		else if (SavedNetworkCommand.JumpType == EWuwaJumpRequestKind::Released)
		{
			CompressedFlags &= ~FLAG_JumpPressed;
		}
	}
	return CompressedFlags;
}

bool FWuwaSavedMove_Character::CanCombineWith(const FSavedMovePtr& NewMove,
                                              ACharacter* Character,
                                              const float MaxDelta) const
{
	const FWuwaSavedMove_Character* NewWuwaMove = static_cast<const FWuwaSavedMove_Character*>(NewMove.Get());
	if (!NewWuwaMove || SavedNetworkCommand.IsOneShot() || NewWuwaMove->SavedNetworkCommand.IsOneShot() ||
	    SavedGrappleState.bActive || NewWuwaMove->SavedGrappleState.bActive)
	{
		return false;
	}

	const bool bSamePresence = SavedNetworkCommand.HasData() == NewWuwaMove->SavedNetworkCommand.HasData();
	const bool bSameContinuousFlags = SavedNetworkCommand.Flags == NewWuwaMove->SavedNetworkCommand.Flags;
	const bool bSameMovementActionState =
	    SavedMovementActionState.IsEquivalentForCombine(NewWuwaMove->SavedMovementActionState);
	return bSamePresence && bSameContinuousFlags && bSameMovementActionState &&
	       Super::CanCombineWith(NewMove, Character, MaxDelta);
}

FWuwaNetworkPredictionData_Client_Character::FWuwaNetworkPredictionData_Client_Character(
    const UCharacterMovementComponent& ClientMovement)
    : Super(ClientMovement)
{
}

FSavedMovePtr FWuwaNetworkPredictionData_Client_Character::AllocateNewMove()
{
	return MakeShared<FWuwaSavedMove_Character>();
}
