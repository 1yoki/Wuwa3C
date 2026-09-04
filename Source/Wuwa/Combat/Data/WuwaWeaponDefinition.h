// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WuwaWeaponDefinition.generated.h"

class UStaticMesh;

/** 可替换武器网格、挂载和剑刃采样点定义 */
UCLASS(BlueprintType, Const)
class WUWA_API UWuwaWeaponDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** @return 武器语义标签 */
	const FGameplayTag& GetWeaponTag() const
	{
		return WeaponTag;
	}

	/** @return 武器静态网格 */
	UStaticMesh* GetStaticMesh() const
	{
		return StaticMesh;
	}

	/** @return 角色挂载骨骼或插槽 */
	FName GetCharacterAttachSocket() const
	{
		return CharacterAttachSocket;
	}

	/** @return 武器相对挂载变换 */
	const FTransform& GetRelativeTransform() const
	{
		return RelativeTransform;
	}

	/** @return 剑刃根部采样 Socket */
	FName GetTraceBaseSocket() const
	{
		return TraceBaseSocket;
	}

	/** @return 剑刃尖端采样 Socket */
	FName GetTraceTipSocket() const
	{
		return TraceTipSocket;
	}

	/** @return 是否明确标记为占位武器 */
	bool IsPlaceholder() const
	{
		return bIsPlaceholder;
	}

	/** @return 运行时关键字段和 StaticMesh Socket 是否有效 */
	bool IsRuntimeValid() const;

	UStaticMesh* GetScabbardStaticMesh() const
	{
		return ScabbardStaticMesh;
	}

	FName GetScabbardCharacterAttachSocket() const
	{
		return ScabbardCharacterAttachSocket;
	}

	const FTransform& GetScabbardRelativeTransform() const
	{
		return ScabbardRelativeTransform;
	}

	FName GetSheathedWeaponSocket() const
	{
		return SheathedWeaponSocket;
	}

	const FTransform& GetSheathedWeaponRelativeTransform() const
	{
		return SheathedWeaponRelativeTransform;
	}

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
	//~ End UObject Interface
#endif

private:
	/** 武器语义标签 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Weapon",
	          meta = (AllowPrivateAccess = "true", Categories = "Weapon"))
	FGameplayTag WeaponTag;

	/** 实际挂载的静态网格 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Weapon",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMesh> StaticMesh;

	/** 角色右手挂载骨骼或插槽 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Weapon",
	          meta = (AllowPrivateAccess = "true"))
	FName CharacterAttachSocket = TEXT("hand_r");

	/** 武器相对右手的变换 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Weapon",
	          meta = (AllowPrivateAccess = "true"))
	FTransform RelativeTransform = FTransform::Identity;

	/** StaticMesh 上的剑刃根部 Socket */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Weapon",
	          meta = (AllowPrivateAccess = "true"))
	FName TraceBaseSocket = TEXT("Trace_Base");

	/** StaticMesh 上的剑刃尖端 Socket */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Weapon",
	          meta = (AllowPrivateAccess = "true"))
	FName TraceTipSocket = TEXT("Trace_Tip");

	/** 刀鞘静态网格 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Weapon|Scabbard",
	          meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMesh> ScabbardStaticMesh;

	/** 刀鞘挂在角色左手的 Socket */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Weapon|Scabbard",
	          meta = (AllowPrivateAccess = "true"))
	FName ScabbardCharacterAttachSocket = TEXT("Sheath_L");

	/** 刀鞘相对左手 Socket 的修正 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Weapon|Scabbard",
	          meta = (AllowPrivateAccess = "true"))
	FTransform ScabbardRelativeTransform = FTransform::Identity;

	/** 武器收刀后挂到刀鞘上的 Socket */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Weapon|Scabbard",
	          meta = (AllowPrivateAccess = "true"))
	FName SheathedWeaponSocket = TEXT("Sword_Sheathed");

	/** 武器挂到刀鞘 Socket 后的额外修正 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Weapon|Scabbard",
	          meta = (AllowPrivateAccess = "true"))
	FTransform SheathedWeaponRelativeTransform = FTransform::Identity;

	/** 是否为第一阶段占位资产 */
	UPROPERTY(EditDefaultsOnly,
	          BlueprintReadOnly,
	          Category = "Wuwa|Combat|Weapon",
	          meta = (AllowPrivateAccess = "true"))
	bool bIsPlaceholder = false;
};
