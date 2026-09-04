// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/WuwaWorldHealthBarWidget.h"

#include "Combat/WuwaCombatLog.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UWuwaWorldHealthBarWidget::SetHealth(const float CurrentHealth, const float MaxHealth)
{
	float SafeCurrentHealth = CurrentHealth;
	float SafeMaxHealth = MaxHealth;
	if (!FMath::IsFinite(CurrentHealth) || !FMath::IsFinite(MaxHealth))
	{
		UE_LOG(LogWuwaCombat,
		       Warning,
		       TEXT("WorldHealthBarWidget 收到非法生命值并按零显示。Widget=%s, Current=%f, Max=%f"),
		       *GetNameSafe(this),
		       CurrentHealth,
		       MaxHealth);
		SafeCurrentHealth = 0.f;
		SafeMaxHealth = 0.f;
	}

	if (!IsValid(HealthBar) || !IsValid(HealthText))
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("WorldHealthBarWidget 缺少 HealthBar 或 HealthText 绑定。Widget=%s"),
		       *GetNameSafe(this));
		return;
	}

	const float Percent = SafeMaxHealth > 0.f ? FMath::Clamp(SafeCurrentHealth / SafeMaxHealth, 0.f, 1.f) : 0.f;
	HealthBar->SetPercent(Percent);
	HealthText->SetText(FText::FromString(
	    FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(SafeCurrentHealth), FMath::RoundToInt(SafeMaxHealth))));
}
