// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Combat/Contracts/WuwaCombatTypes.h"
#include "Components/ActorComponent.h"
#include "WuwaWeaponComponent.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class UWuwaWeaponDefinition;

UENUM(BlueprintType)
enum class EWuwaWeaponAttachLocation : uint8
{
	Hand,
	Scabbard
};

/** 武器网格装配和剑刃端点读取组件 */
UCLASS(ClassGroup = "Wuwa")
class WUWA_API UWuwaWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 创建不参与 Tick 和复制的武器组件 */
	UWuwaWeaponComponent();

	/**
	 * 原子验证并装配当前武器
	 *
	 * @param CharacterMesh	角色骨骼网格
	 * @param WeaponMesh		承载武器静态网格的组件
	 * @param Definition		待应用的武器定义
	 * @return 是否完成装配
	 */
	bool Initialize(USkeletalMeshComponent* CharacterMesh,
	                UStaticMeshComponent* WeaponMesh,
	                UStaticMeshComponent* ScabbardMesh,
	                const UWuwaWeaponDefinition* Definition);

	/** 对称卸载当前武器并清空运行时引用 */
	void Shutdown();

	/**
	 * 读取当前剑刃两端世界坐标
	 *
	 * @param OutBase	接收剑刃根部坐标
	 * @param OutTip	接收剑刃尖端坐标
	 * @return 两个 Socket 是否存在且坐标有效、不重合
	 */
	bool GetBladeWorldEndpoints(FVector& OutBase, FVector& OutTip) const;

	/**
	 * 使用窗口冻结的 Socket 读取剑刃两端世界坐标
	 *
	 * @param TraceBaseSocket	冻结的剑刃根部 Socket
	 * @param TraceTipSocket	冻结的剑刃尖端 Socket
	 * @param OutBase			接收剑刃根部坐标
	 * @param OutTip			接收剑刃尖端坐标
	 * @return 两个冻结 Socket 是否存在且坐标有效、不重合
	 */
	bool GetBladeWorldEndpoints(FName TraceBaseSocket, FName TraceTipSocket, FVector& OutBase, FVector& OutTip) const;

	/** 切换武器当前挂载位置 */
	bool SetWeaponAttachLocation(EWuwaWeaponAttachLocation NewLocation);

	/** 武器挂回右手 */
	bool AttachWeaponToHand();

	/** 武器插入刀鞘 */
	bool AttachWeaponToScabbard();

	EWuwaWeaponAttachLocation GetWeaponAttachLocation() const
	{
		return WeaponAttachLocation;
	}

	/** @return 当前武器是否完成有效装配 */
	bool IsInitialized() const;

	/** @return 当前武器定义 */
	const UWuwaWeaponDefinition* GetWeaponDefinition() const;

	/** @return 当前武器只读快照 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Weapon")
	FWuwaWeaponRuntimeSnapshot GetRuntimeSnapshot() const;

protected:
	//~ Begin UActorComponent Interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

private:
	/** 当前角色骨骼网格 */
	TWeakObjectPtr<USkeletalMeshComponent> BoundCharacterMesh;

	/** 当前武器静态网格组件 */
	TWeakObjectPtr<UStaticMeshComponent> BoundWeaponMesh;

	/** 当前武器定义 */
	TWeakObjectPtr<const UWuwaWeaponDefinition> BoundDefinition;

	/** 当前刀鞘 Mesh */
	TWeakObjectPtr<UStaticMeshComponent> BoundScabbardMesh;

	/** 当前武器视觉挂载状态 */
	EWuwaWeaponAttachLocation WeaponAttachLocation = EWuwaWeaponAttachLocation::Hand;
};
