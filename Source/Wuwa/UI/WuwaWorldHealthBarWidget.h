// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "WuwaWorldHealthBarWidget.generated.h"

class UProgressBar;
class UTextBlock;

/** 显示真实生命值的最小世界血条控件 */
UCLASS(Abstract)
class WUWA_API UWuwaWorldHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 更新进度与生命文本
	 *
	 * @param CurrentHealth	当前真实生命值
	 * @param MaxHealth		当前真实最大生命值
	 */
	void SetHealth(float CurrentHealth, float MaxHealth);

private:
	/** 生命进度条 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	/** 生命数值文本 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthText;
};
