// Copyright Epic Games, Inc. All Rights Reserved.

#include "WuwaCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Core/WuwaStateTagComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"

#include "Movement/WuwaCharacterMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Movement/WuwaMovementProfile.h"

#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Wuwa.h"

// 使用自定义 Character Movement Component。
AWuwaCharacter::AWuwaCharacter(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer.SetDefaultSubobjectClass<
																					UWuwaCharacterMovementComponent>(
																				  ACharacter::CharacterMovementComponentName))
{
	// 创建统一状态标签组件。
	StateTagComponent = CreateDefaultSubobject<UWuwaStateTagComponent>(TEXT("StateTagComponent"));

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
	GetCharacterMovement()->JumpZVelocity = 500.f;
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
	Jump();
}

void AWuwaCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}
