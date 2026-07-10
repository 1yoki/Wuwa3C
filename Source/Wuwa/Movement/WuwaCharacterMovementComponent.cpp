#include "Movement/WuwaCharacterMovementComponent.h"
#include "Wuwa.h"

void UWuwaCharacterMovementComponent::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(
        LogWuwa,
        Display,
        TEXT("Custom MovementComponent active. Owner=%s, Class=%s"),
        *GetNameSafe(GetOwner()),
        *GetClass()->GetName()
    );
}