#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementReplication.h"
#include "Movement/Network/WuwaCharacterNetworkMoveTypes.h"

/** Wuwa SavedMove 的变长网络数据 */
struct WUWA_API FWuwaCharacterNetworkMoveData final : public FCharacterNetworkMoveData
{
	using Super = FCharacterNetworkMoveData;

	FWuwaPendingNetworkMovementCommand NetworkCommand;

	//~ Begin FCharacterNetworkMoveData Interface
	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement,
	                       FArchive& Archive,
	                       UPackageMap* PackageMap,
	                       ENetworkMoveType MoveType) override;
	//~ End FCharacterNetworkMoveData Interface
};

/** 持久保存 New、Pending 与 Old 三份 Wuwa MoveData */
struct WUWA_API FWuwaCharacterNetworkMoveDataContainer final : public FCharacterNetworkMoveDataContainer
{
	FWuwaCharacterNetworkMoveDataContainer();

private:
	FWuwaCharacterNetworkMoveData MoveData[3];
};

/** 在引擎移动 Ack 或 Correction 后附加有限动作结果 */
struct WUWA_API FWuwaCharacterMoveResponseDataContainer final : public FCharacterMoveResponseDataContainer
{
	using Super = FCharacterMoveResponseDataContainer;

	FWuwaNetworkActionResponse NetworkResponse;

	//~ Begin FCharacterMoveResponseDataContainer Interface
	virtual void ServerFillResponseData(const UCharacterMovementComponent& CharacterMovement,
	                                    const FClientAdjustment& PendingAdjustment) override;
	virtual bool
	Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Archive, UPackageMap* PackageMap) override;
	//~ End FCharacterMoveResponseDataContainer Interface
};
