// Copyright Epic Games, Inc. All Rights Reserved.

#include "WuwaPlayerController.h"

#include "Core/WuwaGameplayTags.h"
#include "Debug/WuwaDebugVisualizationComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Input/WuwaInputConfig.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Network/LAN/WuwaLanSessionSubsystem.h"
#include "UI/LAN/SWuwaLanRoomMenu.h"
#include "WuwaCharacter.h"
#include "Wuwa.h"

AWuwaPlayerController::AWuwaPlayerController()
{
	DebugVisualizationComponent =
	    CreateDefaultSubobject<UWuwaDebugVisualizationComponent>(TEXT("DebugVisualizationComponent"));
}

void AWuwaPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (ShouldShowLanRoomMenu())
	{
		ShowLanRoomMenu();
	}
	else if (IsLocalPlayerController())
	{
		RestoreGameInputMode();
	}
}

void AWuwaPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveLanRoomMenu();
	Super::EndPlay(EndPlayReason);
}

void AWuwaPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsLocalPlayerController())
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	if (IsValid(InputComponent))
	{
		InputComponent->BindKey(EKeys::F10, IE_Pressed, this, &AWuwaPlayerController::CycleDebugVisualizationMode);
	}
#endif

	if (!IsValid(InputConfig))
	{
		UE_LOG(LogWuwa, Error, TEXT("InputConfig 未配置。Controller=%s"), *GetNameSafe(this));
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
	    ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!IsValid(Subsystem) || !IsValid(InputConfig->GameplayMappingContext.Get()))
	{
		UE_LOG(LogWuwa, Error, TEXT("GameplayMappingContext 无效。Controller=%s"), *GetNameSafe(this));
		return;
	}
	Subsystem->AddMappingContext(InputConfig->GameplayMappingContext, 0);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!IsValid(EnhancedInput))
	{
		UE_LOG(LogWuwa, Error, TEXT("InputComponent 不是 EnhancedInputComponent。Controller=%s"), *GetNameSafe(this));
		return;
	}

	EnhancedInput->BindAction(
	    InputConfig->MoveAction, ETriggerEvent::Triggered, this, &AWuwaPlayerController::Input_Move);
	EnhancedInput->BindAction(
	    InputConfig->MoveAction, ETriggerEvent::Completed, this, &AWuwaPlayerController::Input_MoveCompleted);
	EnhancedInput->BindAction(
	    InputConfig->MoveAction, ETriggerEvent::Canceled, this, &AWuwaPlayerController::Input_MoveCompleted);
	EnhancedInput->BindAction(
	    InputConfig->LookAction, ETriggerEvent::Triggered, this, &AWuwaPlayerController::Input_Look);
	EnhancedInput->BindAction(
	    InputConfig->JumpAction, ETriggerEvent::Started, this, &AWuwaPlayerController::Input_JumpPressed);
	EnhancedInput->BindAction(
	    InputConfig->JumpAction, ETriggerEvent::Completed, this, &AWuwaPlayerController::Input_JumpReleased);
	EnhancedInput->BindAction(
	    InputConfig->SprintAction, ETriggerEvent::Started, this, &AWuwaPlayerController::Input_SprintPressed);
	EnhancedInput->BindAction(
	    InputConfig->AttackAction, ETriggerEvent::Started, this, &AWuwaPlayerController::Input_AttackPressed);
	EnhancedInput->BindAction(
	    InputConfig->AttackAction, ETriggerEvent::Completed, this, &AWuwaPlayerController::Input_AttackReleased);
	EnhancedInput->BindAction(
	    InputConfig->AttackAction, ETriggerEvent::Canceled, this, &AWuwaPlayerController::Input_AttackReleased);
	EnhancedInput->BindAction(
	    InputConfig->GrappleAction, ETriggerEvent::Started, this, &AWuwaPlayerController::Input_GrapplePressed);
	EnhancedInput->BindAction(
	    InputConfig->LockTargetAction, ETriggerEvent::Started, this, &AWuwaPlayerController::Input_LockTargetPressed);
	EnhancedInput->BindAction(
	    InputConfig->SwitchTargetAction, ETriggerEvent::Started, this, &AWuwaPlayerController::Input_SwitchTarget);
}

void AWuwaPlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	SubmitInputFrame(DeltaTime, bGamePaused);
	Super::PostProcessInput(DeltaTime, bGamePaused);
}

void AWuwaPlayerController::Input_Move(const FInputActionValue& Value)
{
	InputIntent.MoveIntent = Value.Get<FVector2D>();
}

void AWuwaPlayerController::Input_MoveCompleted(const FInputActionValue& Value)
{
	(void)Value;
	InputIntent.MoveIntent = FVector2D::ZeroVector;
}

void AWuwaPlayerController::Input_Look(const FInputActionValue& Value)
{
	InputIntent.LookIntent = Value.Get<FVector2D>();
}

void AWuwaPlayerController::Input_JumpPressed()
{
	InputIntent.bJumpPressed = true;
}

void AWuwaPlayerController::Input_JumpReleased()
{
	InputIntent.bJumpReleased = true;
}

void AWuwaPlayerController::Input_SprintPressed()
{
	InputIntent.bSprintPressed = true;
}

void AWuwaPlayerController::Input_AttackPressed()
{
	InputIntent.bAttackPressed = true;
}

void AWuwaPlayerController::Input_AttackReleased()
{
	InputIntent.bAttackReleased = true;
}

void AWuwaPlayerController::Input_GrapplePressed()
{
	InputIntent.bGrapplePressedThisFrame = true;
}

void AWuwaPlayerController::Input_LockTargetPressed()
{
	InputIntent.bLockTargetPressed = true;
}

void AWuwaPlayerController::Input_SwitchTarget(const FInputActionValue& Value)
{
	InputIntent.SwitchTargetAxis = Value.Get<float>();
}

void AWuwaPlayerController::CycleDebugVisualizationMode()
{
	if (IsValid(DebugVisualizationComponent))
	{
		DebugVisualizationComponent->CycleVisualizationMode();
	}
}

void AWuwaPlayerController::SubmitInputFrame(const float DeltaTime, const bool bGamePaused)
{
	AWuwaCharacter* ControlledCharacter = Cast<AWuwaCharacter>(GetPawn());
	if (!IsValid(ControlledCharacter))
	{
		InputIntent.ResetTransientInputs();
		return;
	}

	FWuwaInputFrame Frame;
	Frame.FrameNumber = GFrameCounter;
	Frame.CapturedAt = GetInputCommandTime();
	Frame.DeltaTime = DeltaTime;
	Frame.bGamePaused = bGamePaused;
	Frame.MoveIntent =
	    InputIntent.MoveIntent.ContainsNaN() ? FVector2D::ZeroVector : InputIntent.MoveIntent.GetClampedToMaxSize(1.f);
	Frame.LookIntent = InputIntent.LookIntent.ContainsNaN() ? FVector2D::ZeroVector : InputIntent.LookIntent;

	if (InputIntent.bSprintPressed)
	{
		Frame.Commands.Add(
		    BuildInputCommand(WuwaGameplayTags::Input_Sprint, EWuwaInputCommandTrigger::Pressed, Frame.MoveIntent));
	}

	if (InputIntent.bJumpPressed)
	{
		Frame.Commands.Add(
		    BuildInputCommand(WuwaGameplayTags::Input_Jump, EWuwaInputCommandTrigger::Pressed, Frame.MoveIntent));
	}

	if (InputIntent.bJumpReleased)
	{
		Frame.Commands.Add(
		    BuildInputCommand(WuwaGameplayTags::Input_Jump, EWuwaInputCommandTrigger::Released, Frame.MoveIntent));
	}

	if (InputIntent.bAttackPressed)
	{
		Frame.Commands.Add(
		    BuildInputCommand(WuwaGameplayTags::Input_Attack, EWuwaInputCommandTrigger::Pressed, Frame.MoveIntent));
	}

	if (InputIntent.bAttackReleased)
	{
		Frame.Commands.Add(
		    BuildInputCommand(WuwaGameplayTags::Input_Attack, EWuwaInputCommandTrigger::Released, Frame.MoveIntent));
	}

	if (InputIntent.bGrapplePressedThisFrame)
	{
		Frame.Commands.Add(
		    BuildInputCommand(WuwaGameplayTags::Input_Grapple, EWuwaInputCommandTrigger::Pressed, Frame.MoveIntent));
	}

	if (InputIntent.bLockTargetPressed)
	{
		Frame.Commands.Add(
		    BuildInputCommand(WuwaGameplayTags::Input_LockTarget, EWuwaInputCommandTrigger::Pressed, Frame.MoveIntent));
	}

	if (!FMath::IsNearlyZero(InputIntent.SwitchTargetAxis))
	{
		Frame.Commands.Add(BuildInputCommand(WuwaGameplayTags::Input_SwitchTarget,
		                                     EWuwaInputCommandTrigger::Pressed,
		                                     FVector2D(InputIntent.SwitchTargetAxis, 0.f)));
	}

	ControlledCharacter->SubmitInputFrame(Frame);
	InputIntent.ResetTransientInputs();
}

FWuwaInputCommand AWuwaPlayerController::BuildInputCommand(const FGameplayTag& InputTag,
                                                           const EWuwaInputCommandTrigger Trigger,
                                                           const FVector2D& Direction)
{
	FWuwaInputCommand Command;
	Command.Header.FrameNumber = GFrameCounter;
	Command.Header.Sequence = NextInputCommandSequence++;
	Command.Header.CreatedAt = GetInputCommandTime();
	Command.Header.SourceObject = this;
	Command.InputTag = InputTag;
	Command.Trigger = Trigger;
	Command.PressedAt = Command.Header.CreatedAt;
	Command.ValidDuration = IsValid(InputConfig) ? InputConfig->DefaultCommandValidDuration : 0.f;
	Command.Direction = Direction.ContainsNaN() ? FVector2D::ZeroVector : Direction.GetClampedToMaxSize(1.f);

	if (NextInputCommandSequence <= 0)
	{
		NextInputCommandSequence = 1;
	}

	return Command;
}

double AWuwaPlayerController::GetInputCommandTime() const
{
	return GetWorld() != nullptr ? static_cast<double>(GetWorld()->GetTimeSeconds()) : 0.0;
}

bool AWuwaPlayerController::ShouldShowLanRoomMenu() const
{
	return IsLocalPlayerController() && GetWorld() != nullptr && GetWorld()->GetNetMode() == NM_Standalone;
}

void AWuwaPlayerController::ShowLanRoomMenu()
{
	if (LanRoomMenu.IsValid())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UWuwaLanSessionSubsystem* SessionSubsystem =
	    IsValid(GameInstance) ? GameInstance->GetSubsystem<UWuwaLanSessionSubsystem>() : nullptr;
	UGameViewportClient* GameViewport = GEngine != nullptr ? GEngine->GameViewport : nullptr;
	if (!IsValid(SessionSubsystem) || !IsValid(GameViewport))
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("无法显示局域网房间菜单。SessionSubsystem=%s, GameViewport=%s"),
		       *GetNameSafe(SessionSubsystem),
		       *GetNameSafe(GameViewport));
		return;
	}

	SAssignNew(LanRoomMenu, SWuwaLanRoomMenu).SessionSubsystem(SessionSubsystem);
	GameViewport->AddViewportWidgetContent(LanRoomMenu.ToSharedRef(), 1000);

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(LanRoomMenu);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void AWuwaPlayerController::RemoveLanRoomMenu()
{
	if (!LanRoomMenu.IsValid())
	{
		return;
	}

	UGameViewportClient* GameViewport = GEngine != nullptr ? GEngine->GameViewport : nullptr;
	if (IsValid(GameViewport))
	{
		GameViewport->RemoveViewportWidgetContent(LanRoomMenu.ToSharedRef());
	}
	LanRoomMenu.Reset();
	RestoreGameInputMode();
}

void AWuwaPlayerController::RestoreGameInputMode()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
}
