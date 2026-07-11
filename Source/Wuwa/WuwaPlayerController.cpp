// Copyright Epic Games, Inc. All Rights Reserved.

#include "WuwaPlayerController.h"
#include "WuwaCharacter.h"
#include "InputMappingContext.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputActionValue.h"
#include "Input/WuwaInputConfig.h"

#include "Blueprint/UserWidget.h"
#include "Wuwa.h"
#include "Widgets/Input/SVirtualJoystick.h"

void AWuwaPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// only spawn touch controls on local player controllers
	if (ShouldUseTouchControls() && IsLocalPlayerController())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);
		}
		else
		{

			UE_LOG(LogWuwa, Error, TEXT("Could not spawn mobile controls widget."));
		}
	}
}

void AWuwaPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsLocalPlayerController())
	{
		return;
	}

	if (!InputConfig)
	{
		UE_LOG(
			LogWuwa,
			Error,
			TEXT("InputConfig is not assigned on %s"),
			*GetNameSafe(this));

		return;
	}

	// 添加 IMC_Gameplay
	if (UEnhancedInputLocalPlayerSubsystem *Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
				GetLocalPlayer()))
	{
		if (InputConfig->GameplayMappingContext)
		{
			Subsystem->AddMappingContext(
				InputConfig->GameplayMappingContext,
				0);
		}
		else
		{
			UE_LOG(
				LogWuwa,
				Error,
				TEXT("GameplayMappingContext is not assigned in %s"),
				*GetNameSafe(InputConfig));
		}
	}

	UEnhancedInputComponent *EnhancedInputComponent =
		Cast<UEnhancedInputComponent>(InputComponent);

	if (!EnhancedInputComponent)
	{
		UE_LOG(
			LogWuwa,
			Error,
			TEXT("InputComponent is not UEnhancedInputComponent"));

		return;
	}

	// 持续输入
	EnhancedInputComponent->BindAction(
		InputConfig->MoveAction,
		ETriggerEvent::Triggered,
		this,
		&AWuwaPlayerController::Input_Move);

	EnhancedInputComponent->BindAction(
		InputConfig->MoveAction,
		ETriggerEvent::Completed,
		this,
		&AWuwaPlayerController::Input_MoveCompleted);

	EnhancedInputComponent->BindAction(
		InputConfig->MoveAction,
		ETriggerEvent::Canceled,
		this,
		&AWuwaPlayerController::Input_MoveCompleted);

	EnhancedInputComponent->BindAction(
		InputConfig->LookAction,
		ETriggerEvent::Triggered,
		this,
		&AWuwaPlayerController::Input_Look);

	// 跳跃
	EnhancedInputComponent->BindAction(
		InputConfig->JumpAction,
		ETriggerEvent::Started,
		this,
		&AWuwaPlayerController::Input_JumpPressed);

	EnhancedInputComponent->BindAction(
		InputConfig->JumpAction,
		ETriggerEvent::Completed,
		this,
		&AWuwaPlayerController::Input_JumpReleased);

	// 冲刺按住/松开
	EnhancedInputComponent->BindAction(
		InputConfig->SprintAction,
		ETriggerEvent::Started,
		this,
		&AWuwaPlayerController::Input_SprintPressed);

	EnhancedInputComponent->BindAction(
		InputConfig->SprintAction,
		ETriggerEvent::Completed,
		this,
		&AWuwaPlayerController::Input_SprintReleased);

	// 单次动作意图
	EnhancedInputComponent->BindAction(
		InputConfig->DodgeAction,
		ETriggerEvent::Started,
		this,
		&AWuwaPlayerController::Input_DodgePressed);

	EnhancedInputComponent->BindAction(
		InputConfig->AttackAction,
		ETriggerEvent::Started,
		this,
		&AWuwaPlayerController::Input_AttackPressed);

	EnhancedInputComponent->BindAction(
		InputConfig->GrappleAction,
		ETriggerEvent::Started,
		this,
		&AWuwaPlayerController::Input_GrapplePressed);

	EnhancedInputComponent->BindAction(
		InputConfig->LockTargetAction,
		ETriggerEvent::Started,
		this,
		&AWuwaPlayerController::Input_LockTargetPressed);

	EnhancedInputComponent->BindAction(
		InputConfig->SwitchTargetAction,
		ETriggerEvent::Triggered,
		this,
		&AWuwaPlayerController::Input_SwitchTarget);
}

bool AWuwaPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void AWuwaPlayerController::Input_Move(const FInputActionValue &Value)
{
	InputIntent.MoveIntent = Value.Get<FVector2D>();
}

void AWuwaPlayerController::Input_MoveCompleted(const FInputActionValue &)
{
	InputIntent.MoveIntent = FVector2D::ZeroVector;
}

void AWuwaPlayerController::Input_Look(const FInputActionValue &Value)
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
	InputIntent.bSprintHeld = true;
}

void AWuwaPlayerController::Input_SprintReleased()
{
	InputIntent.bSprintHeld = false;
}

void AWuwaPlayerController::Input_DodgePressed()
{
	InputIntent.bDodgePressed = true;
}

void AWuwaPlayerController::Input_AttackPressed()
{
	InputIntent.bAttackPressed = true;
}

void AWuwaPlayerController::Input_GrapplePressed()
{
	InputIntent.bGrapplePressed = true;
}

void AWuwaPlayerController::Input_LockTargetPressed()
{
	InputIntent.bLockTargetPressed = true;
}

void AWuwaPlayerController::Input_SwitchTarget(const FInputActionValue &Value)
{
	InputIntent.SwitchTargetAxis = Value.Get<float>();
}

void AWuwaPlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	ProcessInputIntent();

	Super::PostProcessInput(DeltaTime, bGamePaused);
}

void AWuwaPlayerController::ProcessInputIntent()
{
	AWuwaCharacter *ControlledCharacter =
		Cast<AWuwaCharacter>(GetPawn());

	if (!ControlledCharacter)
	{
		InputIntent.ResetTransientInputs();
		return;
	}

	ControlledCharacter->DoMove(
		InputIntent.MoveIntent.X,
		InputIntent.MoveIntent.Y);

	ControlledCharacter->DoLook(
		InputIntent.LookIntent.X,
		InputIntent.LookIntent.Y);

	if (InputIntent.bJumpPressed)
	{
		ControlledCharacter->DoJumpStart();
	}

	if (InputIntent.bJumpReleased)
	{
		ControlledCharacter->DoJumpEnd();
	}

	// 暂时只验证这些输入意图
	if (InputIntent.bDodgePressed)
	{
		UE_LOG(LogWuwa, Display, TEXT("Input Intent: Dodge"));
	}

	if (InputIntent.bAttackPressed)
	{
		UE_LOG(LogWuwa, Display, TEXT("Input Intent: Attack"));
	}

	if (InputIntent.bGrapplePressed)
	{
		UE_LOG(LogWuwa, Display, TEXT("Input Intent: Grapple"));
	}

	if (InputIntent.bLockTargetPressed)
	{
		UE_LOG(LogWuwa, Display, TEXT("Input Intent: Lock Target"));
	}

	if (!FMath::IsNearlyZero(InputIntent.SwitchTargetAxis))
	{
		UE_LOG(
			LogWuwa,
			Display,
			TEXT("Input Intent: Switch Target, Axis=%.2f"),
			InputIntent.SwitchTargetAxis);
	}

	// 清理单帧输入，保留 MoveIntent 和 bSprintHeld
	InputIntent.ResetTransientInputs();
}