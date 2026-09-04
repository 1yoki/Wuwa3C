// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Presentation/WuwaGameplayCue_SwordHit.h"

#include "Combat/WuwaCombatLog.h"

bool UWuwaGameplayCue_SwordHit::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	if (!IsValid(Target) || Parameters.Location.ContainsNaN() || Parameters.Normal.ContainsNaN())
	{
		UE_LOG(LogWuwaCombat, Warning, TEXT("轻剑 Hit Cue 拒绝无效目标或空间参数。Target=%s"), *GetNameSafe(Target));
		return false;
	}

	return true;
}
