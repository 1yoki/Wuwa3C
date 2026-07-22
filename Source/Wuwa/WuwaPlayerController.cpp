// Copyright Epic Games, Inc. All Rights Reserved.

#include "WuwaPlayerController.h"
#include "WuwaCharacter.h"
#include "InputMappingContext.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputActionValue.h"
#include "Input/WuwaInputConfig.h"

#include "Core/WuwaGameplayTags.h"
#include "Engine/World.h"

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

	// Sprint 只在按下边沿生成一次命令。
	EnhancedInputComponent->BindAction(
		InputConfig->SprintAction,
		ETriggerEvent::Started,
		this,
		&AWuwaPlayerController::Input_SprintPressed);

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
		ETriggerEvent::Started,
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
	InputIntent.bSprintPressed = true;
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
	AWuwaCharacter *ControlledCharacter = Cast<AWuwaCharacter>(GetPawn());

	if (!ControlledCharacter)
	{
		InputIntent.ResetTransientInputs();
		return;
	}

	// 离散命令会同步进入 Router；Sprint 成功时会在本帧立即授予移动阻断标签。
	SubmitTransientInputCommands(*ControlledCharacter);

	// Controller 始终保留真实 MoveIntent，Character 根据状态标签决定是否向下传递。
	ControlledCharacter->SetLocomotionIntent(InputIntent.MoveIntent);

	const bool bAirDoubleJumpActionActive = ControlledCharacter->IsAirDoubleJumpActionActive();

	ControlledCharacter->DoLook(InputIntent.LookIntent.X, InputIntent.LookIntent.Y);

	// 防止 Sprint 与 Jump 同帧时，二段跳之后又写入一次普通跳跃缓存
	if (InputIntent.bJumpPressed && !bAirDoubleJumpActionActive)
	{
		ControlledCharacter->DoJumpStart();
	}

	if (InputIntent.bJumpReleased)
	{
		ControlledCharacter->DoJumpEnd();
	}

	// 清理单帧输入，保留 MoveIntent。
	InputIntent.ResetTransientInputs();
}

double AWuwaPlayerController::GetInputCommandTime() const
{
	const UWorld *World = GetWorld();

	return World ? static_cast<double>(World->GetTimeSeconds()) : 0.0;
}

// 实现Command工厂
FWuwaInputCommand AWuwaPlayerController::BuildInputCommand(const FGameplayTag &InputTag, const FVector2D &Direction)
{
	FWuwaInputCommand Command;

	Command.InputTag = InputTag;
	Command.PressedAt = GetInputCommandTime();

	// 有效时间由InputConfig配置，允许在蓝图中调整
	Command.ValidDuration = InputConfig ? InputConfig->DefaultCommandValidDuration : 0.f;

	// 对角输入归一化到最大长度1
	Command.Direction = Direction.GetClampedToMaxSize(1.f);

	Command.Sequence = NextInputCommandSequence++;

	return Command;
}

// 实现单条命令提交
bool AWuwaPlayerController::SubmitSemanticInputCommand(AWuwaCharacter &ControlledCharacter, const FGameplayTag &InputTag, const FVector2D &Direction)
{
	const FWuwaInputCommand Command = BuildInputCommand(InputTag, Direction);

	if (!Command.IsValid())
	{
		UE_LOG(
			LogWuwa,
			Warning,
			TEXT("Invalid Input Command: Tag=%s"),
			*InputTag.ToString());

		return false;
	}

	const bool bSubmitted = ControlledCharacter.SubmitInputCommand(Command);

	if (bSubmitted)
	{
		// 显示 Command 的关键快照
		UE_LOG(LogWuwa, Display, TEXT("Input Command Submitted: Tag=%s, Seq=%u, Dir=(%.2f, %.2f)"),
			   *Command.InputTag.ToString(),
			   Command.Sequence,
			   Command.Direction.X,
			   Command.Direction.Y);
	}

	return bSubmitted;
}

// 将本帧边沿转换为Command并提交给角色
void AWuwaPlayerController::SubmitTransientInputCommands(AWuwaCharacter &ControlledCharacter)
{
	// 所有普通动作都快照本帧WASD输入方向，便于后续动作使用。
	const FVector2D MoveDirection = InputIntent.MoveIntent;

	if (InputIntent.bSprintPressed)
	{
		SubmitSemanticInputCommand(ControlledCharacter, WuwaGameplayTags::Input_Sprint, MoveDirection);
	}

	if (InputIntent.bJumpPressed)
	{
		SubmitSemanticInputCommand(ControlledCharacter, WuwaGameplayTags::Input_Jump, MoveDirection);
	}

	if (InputIntent.bDodgePressed)
	{
		SubmitSemanticInputCommand(ControlledCharacter, WuwaGameplayTags::Input_Dodge, MoveDirection);
	}

	if (InputIntent.bAttackPressed)
	{
		SubmitSemanticInputCommand(ControlledCharacter, WuwaGameplayTags::Input_Attack, MoveDirection);
	}

	if (InputIntent.bGrapplePressed)
	{
		SubmitSemanticInputCommand(ControlledCharacter, WuwaGameplayTags::Input_Grapple, MoveDirection);
	}

	if (InputIntent.bLockTargetPressed)
	{
		SubmitSemanticInputCommand(ControlledCharacter, WuwaGameplayTags::Input_LockTarget, MoveDirection);
	}

	if (!FMath::IsNearlyZero(InputIntent.SwitchTargetAxis))
	{
		const FVector2D SwitchDirection(InputIntent.SwitchTargetAxis, 0.f);
		SubmitSemanticInputCommand(ControlledCharacter, WuwaGameplayTags::Input_SwitchTarget, SwitchDirection);
	}
}