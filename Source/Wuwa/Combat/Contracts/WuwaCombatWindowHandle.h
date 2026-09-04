// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WuwaCombatWindowHandle.generated.h"

/** 服务端权威命中窗口的稳定句柄 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCombatWindowHandle
{
	GENERATED_BODY()

	/** 0 表示无效，其他值由 CombatExecution 单调分配 */
	UPROPERTY(VisibleAnywhere, Category = "Wuwa|Combat|Execution")
	uint32 Value = 0;

	/** @return 当前句柄是否有效 */
	bool IsValid() const
	{
		return Value != 0;
	}

	/** 清空当前句柄 */
	void Reset()
	{
		Value = 0;
	}

	/** @return 两个句柄是否指向同一窗口 */
	bool operator==(const FWuwaCombatWindowHandle& Other) const
	{
		return Value == Other.Value;
	}

	/** @return 两个句柄是否指向不同窗口 */
	bool operator!=(const FWuwaCombatWindowHandle& Other) const
	{
		return !(*this == Other);
	}
};
