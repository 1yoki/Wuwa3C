// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayCueNotify_Static.h"
#include "WuwaGameplayCue_SwordHit.generated.h"

/** 第一阶段轻剑命中的无 Gameplay 副作用占位表现 */
UCLASS(Blueprintable)
class WUWA_API UWuwaGameplayCue_SwordHit : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	//~ Begin UGameplayCueNotify_Static Interface
	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;
	//~ End UGameplayCueNotify_Static Interface
};
