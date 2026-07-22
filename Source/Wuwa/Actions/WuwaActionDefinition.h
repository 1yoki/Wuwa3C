#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Actions/WuwaActionTypes.h"
#include "WuwaActionDefinition.generated.h"

class UAnimMontage;

UCLASS(BlueprintType)
class WUWA_API UWuwaActionDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    // 唯一标识该动作的语义标签
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action", meta = (Categories = "Action"))
    FGameplayTag ActionTag;

    // 优先级只参与仲裁，不能单独授权动作打断
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action", meta = (ClampMin = "0"))
    int32 Priority = 0;

    // 开始动作前必须拥有的 Gameplay Tag
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Tags")
    FGameplayTagContainer RequiredTags;

    // 拥有其中任意一个 Tag 就会拒绝动作
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Tags")
    FGameplayTagContainer BlockedTags;

    // 动作开始后由 Router 取得的状态标签。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Tags")
    FGameplayTagContainer GrantedTags;

    // 动作暂时无法开始时允许等待的时长
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Buffer", meta = (ClampMin = "0.0", Units = "s"))
    float BufferTime = 0.0f;

    // 动作成功启动后，到允许同一 ActionTag 再次启动的最短时间
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Cooldown", meta = (ClampMin = "0.0", Units = "s"))
    float CooldownDuration = 0.0f;

    // 当前动作允许打断的动作标签
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Cancel", meta = (Categories = "Action"))
    FGameplayTagContainer CanCancelActions;

    // 允许打断当前动作的动作标签
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Cancel", meta = (Categories = "Action"))
    FGameplayTagContainer CanBeCancelledBy;

    // 指定动作位移由哪个系统负责
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Movement")
    EWuwaActionMovementPolicy MovementPolicy = EWuwaActionMovementPolicy::None;

    // 供 RootMotionSource 位移策略使用
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Movement|Root Motion Source", meta = (EditCondition = "MovementPolicy == EWuwaActionMovementPolicy::RootMotionSource"))
    FWuwaRootMotionSourceConfig RootMotionSourceConfig;

    // Montage 只作为 Executor 的表现
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Animation")
    TObjectPtr<UAnimMontage> Montage = nullptr;

    // 提供运行时可调用的基础合法性判断
    bool IsRuntimeValid() const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};