#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Actions/WuwaActionSource.h"
#include "WuwaCharacterActionSourceComponent.generated.h"

class AWuwaCharacter;
class UWuwaActionDefinition;
class UWuwaCharacterMovementComponent;

UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaCharacterActionSourceComponent : public UActorComponent, public IWuwaActionSource
{
    GENERATED_BODY()

public:
    UWuwaCharacterActionSourceComponent();

    // Character作为Composition Root 显示注入依赖
    bool Initialize(AWuwaCharacter *InCharacter, UWuwaCharacterMovementComponent *InMovementComponent);

    virtual bool BuildActionRequest(const FWuwaInputCommand &Command, FWuwaActionRequest &OutRequest) const override;

    bool IsInitialized() const;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Actions|Double Jump", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UWuwaActionDefinition> DirectionalDoubleJumpDefinition = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Actions|Double Jump", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UWuwaActionDefinition> BackflipDoubleJumpDefinition = nullptr;

    // 地面有方向 Sprint 使用的前冲动作配置
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Actions|Ground Sprint", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UWuwaActionDefinition> GroundDashDefinition = nullptr;

    // 地面无方向 Sprint 使用的后撤动作配置
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Actions|Ground Sprint", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UWuwaActionDefinition> GroundBackstepDefinition = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<AWuwaCharacter> CharacterOwner;

    UPROPERTY(Transient)
    TObjectPtr<UWuwaCharacterMovementComponent> MovementComponent;
};
