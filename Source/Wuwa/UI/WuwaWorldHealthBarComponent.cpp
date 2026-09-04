// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/WuwaWorldHealthBarComponent.h"

#include "Combat/Health/WuwaHealthComponent.h"
#include "Combat/WuwaCombatLog.h"
#include "Engine/CollisionProfile.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UI/WuwaWorldHealthBarWidget.h"

UWuwaWorldHealthBarComponent::UWuwaWorldHealthBarComponent()
{
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawAtDesiredSize(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetVisibility(false);
	SetRelativeLocation(FVector(0.f, 0.f, 110.f));
	SetIsReplicatedByDefault(false);
	SetTickMode(ETickMode::Automatic);
}

FWuwaWorldHealthBarRuntimeSnapshot UWuwaWorldHealthBarComponent::GetRuntimeSnapshot() const
{
	FWuwaWorldHealthBarRuntimeSnapshot Snapshot;
	Snapshot.bSourceFound = bSourceFound;
	Snapshot.bWidgetCreated = IsValid(GetWidget());
	Snapshot.bScreenPresentationSubmitted =
	    bScreenPresentationUpdateIssued && IsVisible() && IsScreenPresentationContextReady();
	Snapshot.bVisible = IsVisible();
	Snapshot.LastCurrentHealth = LastCurrentHealth;
	Snapshot.LastMaxHealth = LastMaxHealth;
	Snapshot.bHideTimerActive = GetWorld() != nullptr && GetWorld()->GetTimerManager().IsTimerActive(HideTimerHandle);
	Snapshot.VisibleDuration = VisibleDuration;
	Snapshot.bHiddenForLocalOwner = IsHiddenForLocalPlayerOwner();
	return Snapshot;
}

float UWuwaWorldHealthBarComponent::GetVisibleDuration() const
{
	return VisibleDuration;
}

void UWuwaWorldHealthBarComponent::SetVisibleDuration(const float InVisibleDuration)
{
	if (!FMath::IsFinite(InVisibleDuration) || InVisibleDuration < 0.1f)
	{
		UE_LOG(LogWuwaCombat,
		       Warning,
		       TEXT("WorldHealthBarComponent 拒绝非法可见时长。Owner=%s, Duration=%f"),
		       *GetNameSafe(GetOwner()),
		       InVisibleDuration);
		return;
	}

	VisibleDuration = InVisibleDuration;
}

void UWuwaWorldHealthBarComponent::BeginPlay()
{
	const bool bDedicatedPresentationWorld = IsDedicatedPresentationWorld();
	if (bDedicatedPresentationWorld)
	{
		SetWidgetClass(nullptr);
		SetTickMode(ETickMode::Disabled);
	}

	Super::BeginPlay();
	SetPresentationVisibility(false);
	if (bDedicatedPresentationWorld)
	{
		return;
	}

	HealthBarWidget = Cast<UWuwaWorldHealthBarWidget>(GetWidget());
	if (GetWidgetClass() != nullptr && !HealthBarWidget.IsValid())
	{
		UE_LOG(LogWuwaCombat,
		       Warning,
		       TEXT("WorldHealthBarComponent 的 WidgetClass 未创建目标原生控件。Owner=%s, WidgetClass=%s"),
		       *GetNameSafe(GetOwner()),
		       *GetNameSafe(GetWidgetClass().Get()));
	}

	UWuwaHealthComponent* Source =
	    GetOwner() != nullptr ? GetOwner()->FindComponentByClass<UWuwaHealthComponent>() : nullptr;
	if (!IsValid(Source))
	{
		if (!bMissingSourceWarningEmitted)
		{
			bMissingSourceWarningEmitted = true;
			UE_LOG(LogWuwaCombat,
			       Warning,
			       TEXT("WorldHealthBarComponent 缺少同 Owner 的 HealthComponent，保持隐藏且不重试。Owner=%s"),
			       *GetNameSafe(GetOwner()));
		}
		return;
	}

	HealthSource = Source;
	bSourceFound = true;
	HealthChangedDelegateHandle = Source->OnHealthChanged().AddUObject(this, &ThisClass::HandleHealthChanged);
	if (Source->IsInitialized())
	{
		const FWuwaHealthRuntimeSnapshot Health = Source->GetRuntimeSnapshot();
		ApplyHealth(Health.Health, Health.MaxHealth, true);
	}
}

void UWuwaWorldHealthBarComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWuwaHealthComponent* Source = HealthSource.Get(); IsValid(Source) && HealthChangedDelegateHandle.IsValid())
	{
		Source->OnHealthChanged().Remove(HealthChangedDelegateHandle);
	}

	HealthChangedDelegateHandle.Reset();
	HealthSource.Reset();
	bSourceFound = false;
	ClearHideTimer();
	SetPresentationVisibility(false);
	HealthBarWidget.Reset();
	SetWidget(nullptr);
	Super::EndPlay(EndPlayReason);
}

void UWuwaWorldHealthBarComponent::HandleHealthChanged(const FWuwaHealthChangedFact& Fact)
{
	if (Fact.AffectedActor.Get() != GetOwner())
	{
		UE_LOG(LogWuwaCombat,
		       Warning,
		       TEXT("WorldHealthBarComponent 拒绝其它 Actor 的生命事实。Owner=%s, AffectedActor=%s"),
		       *GetNameSafe(GetOwner()),
		       *GetNameSafe(Fact.AffectedActor.Get()));
		return;
	}

	ApplyHealth(Fact.CurrentHealth, Fact.MaxHealth, Fact.bInitialSync);
}

void UWuwaWorldHealthBarComponent::ApplyHealth(const float CurrentHealth,
                                               const float MaxHealth,
                                               const bool bInitialSync)
{
	if (!FMath::IsFinite(CurrentHealth) || !FMath::IsFinite(MaxHealth))
	{
		UE_LOG(LogWuwaCombat,
		       Warning,
		       TEXT("WorldHealthBarComponent 拒绝非法生命事实。Owner=%s, Current=%f, Max=%f"),
		       *GetNameSafe(GetOwner()),
		       CurrentHealth,
		       MaxHealth);
		return;
	}

	LastCurrentHealth = CurrentHealth;
	LastMaxHealth = MaxHealth;
	InitWidget();
	if (!HealthBarWidget.IsValid())
	{
		HealthBarWidget = Cast<UWuwaWorldHealthBarWidget>(GetWidget());
	}
	if (UWuwaWorldHealthBarWidget* WorldHealthBarWidget = HealthBarWidget.Get())
	{
		WorldHealthBarWidget->SetHealth(CurrentHealth, MaxHealth);
	}

	bHiddenForLocalOwner = IsHiddenForLocalPlayerOwner();
	if (bInitialSync || bHiddenForLocalOwner)
	{
		ClearHideTimer();
		SetPresentationVisibility(false);
		return;
	}

	SetPresentationVisibility(true);
	if (!IsVisible())
	{
		ClearHideTimer();
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(HideTimerHandle, this, &ThisClass::HideHealthBar, VisibleDuration, false);
	}
}

bool UWuwaWorldHealthBarComponent::IsHiddenForLocalPlayerOwner() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return IsValid(OwnerPawn) && OwnerPawn->IsPlayerControlled() && OwnerPawn->IsLocallyControlled();
}

bool UWuwaWorldHealthBarComponent::IsDedicatedPresentationWorld() const
{
	return IsRunningDedicatedServer() || (GetWorld() != nullptr && GetWorld()->GetNetMode() == NM_DedicatedServer);
}

void UWuwaWorldHealthBarComponent::HideHealthBar()
{
	SetPresentationVisibility(false);
	HideTimerHandle.Invalidate();
}

void UWuwaWorldHealthBarComponent::SetPresentationVisibility(const bool bShouldBeVisible)
{
	if (!bShouldBeVisible)
	{
		bScreenPresentationUpdateIssued = false;
		SetVisibility(false, true);
		UpdateWidget();
		return;
	}

	InitWidget();
	HealthBarWidget = Cast<UWuwaWorldHealthBarWidget>(GetWidget());
	if (GetWorld() == nullptr || GetWidgetClass() == nullptr || !HealthBarWidget.IsValid())
	{
		if (!bPresentationUnavailableWarningEmitted)
		{
			bPresentationUnavailableWarningEmitted = true;
			UE_LOG(LogWuwaCombat,
			       Warning,
			       TEXT("WorldHealthBarComponent 无法提交 Screen Space 表现。Owner=%s, World=%s, WidgetClass=%s, "
			            "Widget=%s"),
			       *GetNameSafe(GetOwner()),
			       *GetNameSafe(GetWorld()),
			       *GetNameSafe(GetWidgetClass().Get()),
			       *GetNameSafe(GetWidget()));
		}

		SetVisibility(false, true);
		bScreenPresentationUpdateIssued = false;
		UpdateWidget();
		return;
	}

	bPresentationUnavailableWarningEmitted = false;
	SetVisibility(true, true);
	UpdateWidget();
	bScreenPresentationUpdateIssued = true;
}

bool UWuwaWorldHealthBarComponent::IsScreenPresentationContextReady() const
{
	const UWorld* World = GetWorld();
	const AActor* OwnerActor = GetOwner();
	const ULocalPlayer* LocalOwnerPlayer = GetOwnerPlayer();
	const UGameViewportClient* GameViewport = IsValid(World) ? World->GetGameViewport() : nullptr;
	return GetWidgetSpace() == EWidgetSpace::Screen && IsValid(World) && World->IsGameWorld() && IsValid(OwnerActor) &&
	       !OwnerActor->IsHidden() && IsValid(GetWidget()) && IsValid(LocalOwnerPlayer) &&
	       IsValid(LocalOwnerPlayer->PlayerController) && IsValid(GameViewport) &&
	       GameViewport->GetGameLayerManager().IsValid();
}

void UWuwaWorldHealthBarComponent::ClearHideTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}
	HideTimerHandle.Invalidate();
}
