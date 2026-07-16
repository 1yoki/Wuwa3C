// Copyright Epic Games, Inc. All Rights Reserved.

#include "WuwaCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Core/WuwaStateTagComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"

#include "Input/WuwaInputBufferComponent.h"
#include "Engine/World.h"

#include "Movement/WuwaCharacterMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Movement/WuwaMovementProfile.h"

#include "Actions/WuwaActionRouterComponent.h"

#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Wuwa.h"

// 使用自定义 Character Movement Component。
AWuwaCharacter::AWuwaCharacter(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer.SetDefaultSubobjectClass<UWuwaCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// 创建统一状态标签组件。
	StateTagComponent = CreateDefaultSubobject<UWuwaStateTagComponent>(TEXT("StateTagComponent"));

	// Character 负责创建并持有唯一输入缓存组件。
	InputBufferComponent = CreateDefaultSubobject<UWuwaInputBufferComponent>(TEXT("InputBufferComponent"));

	// Character 创建唯一 Action Router。
	ActionRouterComponent = CreateDefaultSubobject<UWuwaActionRouterComponent>(TEXT("ActionRouterComponent"));

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character)
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

UWuwaCharacterMovementComponent *AWuwaCharacter::GetWuwaMovementComponent() const
{
	return Cast<UWuwaCharacterMovementComponent>(GetCharacterMovement());
}

double AWuwaCharacter::GetInputCommandTime() const
{
	const UWorld *World = GetWorld();

	return World ? static_cast<double>(World->GetTimeSeconds()) : 0.0;
}

void AWuwaCharacter::BeginPlay()
{
	Super::BeginPlay();

	UWuwaCharacterMovementComponent *Movement = GetWuwaMovementComponent();

	if (!Movement)
	{
		UE_LOG(LogWuwa, Error, TEXT("自定义 Movement Component 无效。Owner=%s"), *GetNameSafe(this));
		return;
	}

	// Character 负责连接同级组件。
	Movement->SetStateTagComponent(StateTagComponent);
	// Character 显式连接 Router 与同级组件
	if (!ActionRouterComponent || !ActionRouterComponent->Initialize(InputBufferComponent, StateTagComponent))
	{
		UE_LOG(LogWuwa, Error, TEXT("Action Router 初始化失败。Owner=%s"), *GetNameSafe(this));
		return;
	}

	if (!MovementProfile)
	{
		UE_LOG(LogWuwa, Error, TEXT("未配置 MovementProfile。Owner=%s"), *GetNameSafe(this));
		return;
	}

	// Profile 仅在初始化时应用。
	Movement->ApplyMovementProfile(MovementProfile);
}

FWuwaLocomotionSnapshot AWuwaCharacter::GetLocomotionSnapshot() const
{
	const UWuwaCharacterMovementComponent *Movement = GetWuwaMovementComponent();

	if (!Movement)
	{
		return FWuwaLocomotionSnapshot();
	}

	// 返回副本，表现层无法修改运行时状态。
	return Movement->GetLocomotionSnapshot();
}

void AWuwaCharacter::SetLocomotionIntent(const FVector2D &MoveIntent)
{
	// 保证对角输入量不超过 1。
	const FVector2D ClampedIntent = MoveIntent.GetClampedToMaxSize(1.f);

	if (UWuwaCharacterMovementComponent *Movement = GetWuwaMovementComponent())
	{
		// Movement Component 决定 Walk/Run 速度。
		Movement->SetLocomotionIntent(ClampedIntent);
	}

	DoMove(ClampedIntent.X, ClampedIntent.Y);
}

bool AWuwaCharacter::SubmitInputCommand(const FWuwaInputCommand &Command)
{
	if (!InputBufferComponent)
	{
		UE_LOG(LogWuwa, Error, TEXT("未找到 InputBufferComponent。Owner=%s"), *GetNameSafe(this));
		return false;
	}

	// Character 只负责转交
	const bool bPushed = InputBufferComponent->Push(Command);

	if (bPushed && ActionRouterComponent && ActionRouterComponent->IsInitialized())
	{
		// 新命令入队后立即尝试处理 FIFO 队首
		ActionRouterComponent->TryConsumeBuffer();
	}

	return bPushed;
}

bool AWuwaCharacter::PeekInputCommand(FWuwaInputCommand &OutCommand)
{
	if (!InputBufferComponent)
	{
		OutCommand = FWuwaInputCommand();
		return false;
	}

	return InputBufferComponent->Peek(GetInputCommandTime(), OutCommand);
}

bool AWuwaCharacter::ConsumeInputCommand(uint32 Sequence, FWuwaInputCommand &OutCommand)
{
	if (!InputBufferComponent)
	{
		OutCommand = FWuwaInputCommand();
		return false;
	}

	return InputBufferComponent->Consume(GetInputCommandTime(), Sequence, OutCommand);
}

int32 AWuwaCharacter::ClearInputCommandsByTag(const FGameplayTag &InputTag)
{
	if (!InputBufferComponent)
	{
		return 0;
	}

	return InputBufferComponent->ClearByTag(InputTag);
}

TArray<FWuwaInputCommand> AWuwaCharacter::GetBufferedInputCommands()
{
	if (!InputBufferComponent)
	{
		return {};
	}

	return InputBufferComponent->GetDebugSnapshot(GetInputCommandTime());
}

void AWuwaCharacter::DoMove(float Right, float Forward)
{
	// 将二维输入转换为相机朝向的世界移动。
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AWuwaCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AWuwaCharacter::DoJumpStart()
{
	// signal the character to jump
	if (UWuwaCharacterMovementComponent *Movement = GetWuwaMovementComponent())
	{
		Movement->RequestJump();
	}
}

// 可用来实现长按跳跃时的持续上升，或者在落地前缓存跳跃请求。
void AWuwaCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

// 把当前帧的世界移动方向提交给空中二段跳。
bool AWuwaCharacter::DoSprintPressed()
{
	UWuwaCharacterMovementComponent *Movement = GetWuwaMovementComponent();

	if (!Movement)
	{
		return false;
	}

	// 使用当前帧积累的世界移动方向
	const FVector DesiredWorldDirection = GetPendingMovementInputVector();
	return Movement->RequestAirSprint(DesiredWorldDirection);
}