// Copyright Epic Games, Inc. All Rights Reserved.

#include "WuwaCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Core/WuwaStateTagComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/WuwaCameraModeComponent.h"
#include "Camera/WuwaCameraProfile.h"
#include "Camera/WuwaSpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/WuwaGameplayTags.h"

#include "Input/WuwaInputBufferComponent.h"
#include "Engine/World.h"

#include "Targeting/WuwaTargetingComponent.h"
#include "Targeting/WuwaTargetingProfile.h"

#include "Movement/WuwaCharacterMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Movement/WuwaMovementActionExecutorComponent.h"
#include "Movement/WuwaMovementProfile.h"

#include "Actions/WuwaCharacterActionSourceComponent.h"
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
	// 创建一个 State Tag 组件。
	StateTagComponent = CreateDefaultSubobject<UWuwaStateTagComponent>(TEXT("StateTagComponent"));

	// 创建一个 Input Buffer 组件。
	InputBufferComponent = CreateDefaultSubobject<UWuwaInputBufferComponent>(TEXT("InputBufferComponent"));

	// 创建一个 Action Router 组件
	ActionRouterComponent = CreateDefaultSubobject<UWuwaActionRouterComponent>(TEXT("ActionRouterComponent"));

	// 创建一个 Action Source 组件
	ActionSourceComponent = CreateDefaultSubobject<UWuwaCharacterActionSourceComponent>(TEXT("ActionSourceComponent"));
	
	// 创建一个 Action Executor 组件
	MovementActionExecutorComponent = CreateDefaultSubobject<UWuwaMovementActionExecutorComponent>(TEXT("MovementActionExecutorComponent"));

	// 创建一个 Targeting 组件
	TargetingComponent = CreateDefaultSubobject<UWuwaTargetingComponent>(TEXT("TargetingComponent"));

	// 创建一个 Camera Mode 组件
	CameraModeComponent = CreateDefaultSubobject<UWuwaCameraModeComponent>(TEXT("CameraModeComponent"));
	
	// 创建一个 Camera Boom 组件
	
	
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
	CameraBoom = CreateDefaultSubobject<UWuwaSpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Mode Component 将负责模式参数 Blend，SpringArm 不再叠加第二层 Lag
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->bEnableCameraRotationLag = false;

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
	
	// 先检查是否有 Movement 参数配置
	if (!MovementProfile)
	{
		UE_LOG(LogWuwa, Error, TEXT("未配置 MovementProfile。Owner=%s"), *GetNameSafe(this));
		return;
	}

	UWuwaCharacterMovementComponent *Movement = GetWuwaMovementComponent();

	if (!Movement)
	{
		UE_LOG(LogWuwa, Error, TEXT("自定义 Movement Component 无效。Owner=%s"), *GetNameSafe(this));
		return;
	}

	// MovementComponent 连接 StateTagComponent
	Movement->SetStateTagComponent(StateTagComponent);
	
	// Profile 配置仅在初始化时应用。
	Movement->ApplyMovementProfile(MovementProfile);
	
	// 初始化 Action Router 并连接 Input Buffer、StateTagComponent
	if (!ActionRouterComponent || !ActionRouterComponent->Initialize(InputBufferComponent, StateTagComponent))
	{
		UE_LOG(LogWuwa, Error, TEXT("Action Router 初始化失败。Owner=%s"), *GetNameSafe(this));
		return;
	}

	// 初始化 Action Source 并连接 MovementComponent
	if (!ActionSourceComponent || !ActionSourceComponent->Initialize(this, Movement) ||
		!ActionRouterComponent->SetActionSource(ActionSourceComponent))
	{
		UE_LOG(
			LogWuwa,
			Error,
			TEXT("真实 Action Source 装配失败。Owner=%s"),
			*GetNameSafe(this));

		return;
	}

	// 初始化 Action Executor 并连接 MovementComponent、Action Router
	if (!MovementActionExecutorComponent || !MovementActionExecutorComponent->Initialize(this, Movement, ActionRouterComponent) || !ActionRouterComponent->RegisterExecutor(MovementActionExecutorComponent))
	{
		UE_LOG(
			LogWuwa,
			Error,
			TEXT("Movement Action Executor 装配失败。Owner=%s"),
			*GetNameSafe(this));

		return;
	}

	// 初始化 TargetingComponent 并连接 StateTagComponent
	if (!TargetingComponent || !TargetingProfile || !TargetingComponent->Initialize(this, StateTagComponent, TargetingProfile))
	{
		const EWuwaTargetingFailureReason FailureReason = TargetingComponent ? TargetingComponent->GetLastFailureReason() : EWuwaTargetingFailureReason::NotInitialized;

		UE_LOG(
			LogWuwa,
			Error,
			TEXT("Targeting Component 装配失败。Owner=%s, FailureReason=%s"),
			*GetNameSafe(this),
			*UEnum::GetValueAsString(FailureReason));

		return;
	}

	// MovementComponent 连接 TargetingComponent，只读消费同 Owner 的 Target Context
	if (!Movement->SetTargetingComponent(TargetingComponent))
	{
		UE_LOG(
			LogWuwa,
			Error,
			TEXT("Movement Targeting Consumer 装配失败。Owner=%s"),
			*GetNameSafe(this));

		return;
	}
	
	// 初始化 CameraModeComponent 并连接 TargetingComponent, 只读消费同 Owner 的 Target Context
	if (!CameraModeComponent || !CameraProfile || !CameraBoom->ConfigureCollision(CameraProfile->ProbeSize, CameraProfile->ProbeChannel.GetValue(), CameraProfile->CollisionRecoveryInterpSpeed) || !CameraModeComponent->Initialize(CameraProfile, TargetingComponent, CameraBoom, FollowCamera))
	{
		UE_LOG(
			LogWuwa,
			Error,
			TEXT("Camera Mode Component 装配失败。Owner=%s"),
			*GetNameSafe(this));

		return;
	}
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

bool AWuwaCharacter::IsMoveInputBlocked() const
{
	return IsValid(StateTagComponent.Get()) && StateTagComponent->HasTag(WuwaGameplayTags::Block_Input_Move, true);
}

void AWuwaCharacter::SetLocomotionIntent(const FVector2D &MoveIntent)
{
	// 先保存 Controller 的真实输入；Block.Input.Move 只能阻止向下传递，不能清除快照
	CurrentMoveIntent = MoveIntent.GetClampedToMaxSize(1.f);
	
	const bool bMoveBlocked = IsMoveInputBlocked();
	
	// Backstep 到达移动取消窗口后，真实 WASD 可以结束动作
	if (UWuwaMovementActionExecutorComponent* Executor = MovementActionExecutorComponent.Get())
	{
		Executor->TryCancelBackstepByMoveIntent(CurrentMoveIntent);
	}

	const FVector2D EffectiveIntent = bMoveBlocked ? FVector2D::ZeroVector : CurrentMoveIntent;

	if (UWuwaCharacterMovementComponent *Movement = GetWuwaMovementComponent())
	{
		// Movement Component 决定 Walk/Run 速度。
		Movement->SetLocomotionIntent(EffectiveIntent);
	}

	if (bMoveBlocked)
	{
		return;
	}

	DoMove(EffectiveIntent.X, EffectiveIntent.Y);
}

bool AWuwaCharacter::SubmitInputCommand(const FWuwaInputCommand &Command)
{
	if (!Command.IsValid())
	{
		UE_LOG(
			LogWuwa,
			Warning,
			TEXT("Character 拒绝无效 Input Command。Owner=%s, Input=%s, Sequence=%u"),
			*GetNameSafe(this),
			*Command.InputTag.ToString(),
			Command.Sequence);

		return false;
	}

	if (Command.InputTag == WuwaGameplayTags::Input_LockTarget)
	{
		// Lock 是非独占目标状态请求，不进入动作 FIFO，也不打断当前 Action。
		if (!TargetingComponent || !TargetingComponent->IsInitialized())
		{
			UE_LOG(
				LogWuwa,
				Error,
				TEXT("Lock Target Command 缺少有效 Targeting Component。Owner=%s"),
				*GetNameSafe(this));

			return false;
		}

		const FWuwaTargetingResult Result = TargetingComponent->ToggleHardLock();

		// 若没候选目标，按锁定键恢复默认视角
		if (!Result.bSucceeded && Result.FailureReason == EWuwaTargetingFailureReason::NoCandidate)
		{
			CameraModeComponent->RequestExplorationRecenter();
			// 无候选等合法拒绝只在输入边沿记录，不产生每帧日志。
			UE_LOG(
				LogWuwa,
				Verbose,
				TEXT("Lock Target Command 被拒绝。Owner=%s, Reason=%s, Sequence=%u"),
				*GetNameSafe(this),
				*UEnum::GetValueAsString(Result.FailureReason),
				Command.Sequence);
		}

		return Result.bSucceeded;
	}

	if (Command.InputTag == WuwaGameplayTags::Input_SwitchTarget)
	{
		// Switch 与 Lock 同属非独占目标状态请求，不进入动作 FIFO 或 Router。
		if (!TargetingComponent || !TargetingComponent->IsInitialized())
		{
			UE_LOG(
				LogWuwa,
				Error,
				TEXT("Switch Target Command 缺少有效 Targeting Component。Owner=%s"),
				*GetNameSafe(this));

			return false;
		}

		const FWuwaTargetingResult Result = TargetingComponent->SwitchHardTarget(Command.Direction.X);

		if (!Result.bSucceeded)
		{
			// 无 Hard 或方向上无候选属于合法拒绝，只在输入边沿记录。
			UE_LOG(
				LogWuwa,
				Verbose,
				TEXT("Switch Target Command 被拒绝。Owner=%s, Direction=%.2f, Reason=%s, Sequence=%u"),
				*GetNameSafe(this),
				Command.Direction.X,
				*UEnum::GetValueAsString(
					Result.FailureReason),
				Command.Sequence);
		}

		return Result.bSucceeded;
	}

	if (!InputBufferComponent)
	{
		UE_LOG(LogWuwa, Error, TEXT("未找到 InputBufferComponent。Owner=%s"), *GetNameSafe(this));
		return false;
	}

	// Character 只负责转交
	// 独占动作命令进入唯一 FIFO。
	const bool bPushed = InputBufferComponent->Push(Command);

	if (bPushed && ActionRouterComponent && ActionRouterComponent->IsInitialized())
	{
		// 新命令入队后立即尝试处理 FIFO 队首
		ActionRouterComponent->TryConsumeBuffer();
	}

	return bPushed;
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
	// 防止蓝图或其他调用方绕过 SetLocomotionIntent 直接注入移动。
	if (IsMoveInputBlocked())
	{
		return;
	}

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

void AWuwaCharacter::DoLook(const float Yaw, const float Pitch)
{
	/*
	 * Exploration 由手动 Look 持有 ControlRotation；
	 * LockOn 则由 CameraModeComponent 持有。
	 */
	
	UWuwaCameraModeComponent* CameraMode = CameraModeComponent.Get();
	
	if (IsValid(CameraModeComponent.Get()) && CameraMode->HasViewRotationAuthority())
	{
		return;
	}
	
	constexpr float LookInterruptThreshold = 0.001f;
	
	const bool bHasManualLookInput = FMath::Abs(Yaw) > LookInterruptThreshold ||
		FMath::Abs(Pitch) > LookInterruptThreshold;
	
	if (!bHasManualLookInput)
	{
		return;
	}
	
	if (IsValid(CameraMode))
	{
		CameraMode->CancelExplorationRecenter();
	}

	if (GetController() != nullptr)
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

bool AWuwaCharacter::IsAirDoubleJumpActionActive() const
{
	const UWuwaActionRouterComponent *Router = ActionRouterComponent.Get();

	if (!IsValid(Router) || !Router->HasActiveAction())
	{
		return false;
	}

	const FGameplayTag ActiveActionTag = Router->GetCurrentActionTag();

	return ActiveActionTag == WuwaGameplayTags::Action_Movement_DoubleJump_Directional ||
		   ActiveActionTag == WuwaGameplayTags::Action_Movement_DoubleJump_Backflip;
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