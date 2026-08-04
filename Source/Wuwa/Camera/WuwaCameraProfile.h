#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "Camera/WuwaCameraTypes.h"
#include "WuwaCameraProfile.generated.h"

/*
 * 相机参数的唯一资产权威。
 * Profile 只允许 Exploration 与 LockOn，不保存任何 Gameplay 运行状态。
 */

UCLASS(BlueprintType)
class WUWA_API UWuwaCameraProfile : public UDataAsset
{
    GENERATED_BODY()

public:
    UWuwaCameraProfile();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Modes", meta = (TitleProperty = "ModeTag"))
    TArray<FWuwaCameraModeConfig> ModeConfigs;

    // SpringArm 原生 Sphere Sweep 的半径。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Collision", meta = (ClampMin = "1.0", Units = "cm"))
    float ProbeSize = 12.f;

    // SpringArm 原生 Probe 是唯一的相机碰撞查询
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Collision")
    TEnumAsByte<ECollisionChannel> ProbeChannel = ECC_Camera;

    // 只控制碰撞距离的离墙恢复，不承担模式 Blend 或 Camera Lag。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Collision", meta = (ClampMin = "0.01"))
    float CollisionRecoveryInterpSpeed = 8.f;

    // 精确查询，不允许父标签Camera匹配具体模式
    const FWuwaCameraModeConfig *FindModeConfig(const FGameplayTag &ModeTag) const;

    // Conponent 初始化时调用；运行期间不应每帧重新验证资产
    bool IsRuntimeValid() const;

#if WITH_EDITOR
    // Editor 校验只报告配置错误，不修改资产或运行时 Camera 状态。
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};