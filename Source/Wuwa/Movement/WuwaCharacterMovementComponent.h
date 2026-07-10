#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "WuwaCharacterMovementComponent.generated.h"

UCLASS()

class WUWA_API UWuwaCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
};