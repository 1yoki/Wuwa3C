// Copyright Epic Games, Inc. All Rights Reserved.

#include "WuwaCharacter.h"

#include "Actions/Data/WuwaActionRuleSet.h"
#include "Actions/Network/WuwaActionNetworkComponent.h"
#include "Actions/Runtime/WuwaActionCoordinatorComponent.h"
#include "Actions/Resolution/WuwaActionRuleIntentProviderComponent.h"
#include "AbilitySystem/Input/WuwaAbilityInputRouterComponent.h"
#include "AbilitySystem/Contracts/WuwaAbilityTypes.h"
#include "AbilitySystem/Interop/WuwaActionAbilityInteropComponent.h"
#include "AbilitySystem/Runtime/WuwaPawnAbilityInitComponent.h"
#include "Animation/Actions/WuwaActionAnimationCapabilityComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/WuwaCameraModeComponent.h"
#include "Camera/WuwaCameraProfile.h"
#include "Camera/WuwaSpringArmComponent.h"
#include "Combat/Contracts/WuwaCombatTypes.h"
#include "Combat/Health/WuwaHealthComponent.h"
#include "Combat/Poise/WuwaPoiseComponent.h"
#include "Combat/Runtime/WuwaCombatExecutionComponent.h"
#include "Combat/Runtime/WuwaWeaponComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/WuwaGameplayTags.h"
#include "Core/WuwaStateTagComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Messaging/WuwaCharacterMessageDispatcherComponent.h"
#include "Movement/Actions/WuwaMovementActionCapabilityComponent.h"
#include "Movement/WuwaCharacterMovementComponent.h"
#include "Movement/WuwaMovementProfile.h"
#include "Targeting/WuwaTargetingComponent.h"
#include "Targeting/WuwaTargetingProfile.h"
#include "Traversal/Actions/WuwaGrappleCapabilityComponent.h"
#include "Traversal/Data/WuwaTraversalProfile.h"
#include "Traversal/Presentation/WuwaGrapplePresentationComponent.h"
#include "Traversal/Resolution/WuwaTraversalActionIntentProviderComponent.h"
#include "UI/WuwaWorldHealthBarComponent.h"
#include "Wuwa.h"
#include "WuwaGameMode.h"

AWuwaCharacter::AWuwaCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UWuwaCharacterMovementComponent>(
          ACharacter::CharacterMovementComponentName))
{
	// Control Panel
	StateTagComponent = CreateDefaultSubobject<UWuwaStateTagComponent>(TEXT("StateTagComponent"));
	MessageDispatcherComponent =
	    CreateDefaultSubobject<UWuwaCharacterMessageDispatcherComponent>(TEXT("MessageDispatcherComponent"));
	ActionCoordinatorComponent = CreateDefaultSubobject<UWuwaActionCoordinatorComponent>(TEXT("ActionRouterComponent"));
	ActionNetworkComponent = CreateDefaultSubobject<UWuwaActionNetworkComponent>(TEXT("ActionNetworkComponent"));
	PawnAbilityInitComponent = CreateDefaultSubobject<UWuwaPawnAbilityInitComponent>(TEXT("PawnAbilityInitComponent"));
	AbilityInputRouterComponent =
	    CreateDefaultSubobject<UWuwaAbilityInputRouterComponent>(TEXT("AbilityInputRouterComponent"));
	ActionAbilityInteropComponent =
	    CreateDefaultSubobject<UWuwaActionAbilityInteropComponent>(TEXT("ActionAbilityInteropComponent"));

	// Ability
	MovementActionCapabilityComponent =
	    CreateDefaultSubobject<UWuwaMovementActionCapabilityComponent>(TEXT("MovementActionExecutorComponent"));
	ActionAnimationCapabilityComponent =
	    CreateDefaultSubobject<UWuwaActionAnimationCapabilityComponent>(TEXT("ActionAnimationCapabilityComponent"));
	GrappleCapabilityComponent =
	    CreateDefaultSubobject<UWuwaGrappleCapabilityComponent>(TEXT("GrappleCapabilityComponent"));
	CombatExecutionComponent = CreateDefaultSubobject<UWuwaCombatExecutionComponent>(TEXT("CombatExecutionComponent"));

	// Domain And Presentation
	ActionRuleIntentProviderComponent =
	    CreateDefaultSubobject<UWuwaActionRuleIntentProviderComponent>(TEXT("ActionRuleIntentProviderComponent"));
	TraversalActionIntentProviderComponent = CreateDefaultSubobject<UWuwaTraversalActionIntentProviderComponent>(
	    TEXT("TraversalActionIntentProviderComponent"));
	GrapplePresentationComponent =
	    CreateDefaultSubobject<UWuwaGrapplePresentationComponent>(TEXT("GrapplePresentationComponent"));
	TargetingComponent = CreateDefaultSubobject<UWuwaTargetingComponent>(TEXT("TargetingComponent"));
	CameraModeComponent = CreateDefaultSubobject<UWuwaCameraModeComponent>(TEXT("CameraModeComponent"));

	WeaponMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMeshComponent"));
	WeaponMeshComponent->SetupAttachment(GetMesh());
	WeaponMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMeshComponent->SetGenerateOverlapEvents(false);

	ScabbardMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScabbardMeshComponent"));
	ScabbardMeshComponent->SetupAttachment(GetMesh());
	ScabbardMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ScabbardMeshComponent->SetGenerateOverlapEvents(false);

	WeaponComponent = CreateDefaultSubobject<UWuwaWeaponComponent>(TEXT("WeaponComponent"));
	HealthComponent = CreateDefaultSubobject<UWuwaHealthComponent>(TEXT("HealthComponent"));
	WorldHealthBarComponent = CreateDefaultSubobject<UWuwaWorldHealthBarComponent>(TEXT("WorldHealthBarComponent"));
	WorldHealthBarComponent->SetupAttachment(RootComponent);
	PoiseComponent = CreateDefaultSubobject<UWuwaPoiseComponent>(TEXT("PoiseComponent"));

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(WuwaCombatCollision::MeleeTraceChannel, ECR_Block);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;

	CameraBoom = CreateDefaultSubobject<UWuwaSpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->bEnableCameraRotationLag = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void AWuwaCharacter::BeginPlay()
{
	Super::BeginPlay();

	BindDeathFactDelegate();
	PawnAbilityInitComponent->TryInitializeAbilitySystem();

	UWuwaCharacterMovementComponent* Movement = GetWuwaMovementComponent();
	if (!InitializeMovementRuntime(Movement) || !InitializeTargetingRuntime(Movement) || !InitializeCameraRuntime() ||
	    !InitializeActionRuntime(Movement))
	{
		UE_LOG(LogWuwa, Error, TEXT("Character 核心运行时装配失败。Owner=%s"), *GetNameSafe(this));
		return;
	}

	TryInitializeTraversalRuntime(Movement);
}

void AWuwaCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindDeathFactDelegate();
	PawnAbilityInitComponent->ShutdownAbilitySystem(EWuwaPawnAbilityShutdownReason::EndPlay);
	Super::EndPlay(EndPlayReason);
}

void AWuwaCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	PawnAbilityInitComponent->TryInitializeAbilitySystem();
}

void AWuwaCharacter::UnPossessed()
{
	if (IsValid(ActionNetworkComponent))
	{
		ActionNetworkComponent->InvalidateAvatarGenerations(EWuwaGrappleMovementEndReason::AvatarChanged,
		                                                    EWuwaActionEndReason::Cancelled);
	}
	if (IsValid(GrapplePresentationComponent))
	{
		GrapplePresentationComponent->InvalidateAvatarPresentation();
	}
	PawnAbilityInitComponent->ShutdownAbilitySystem(EWuwaPawnAbilityShutdownReason::Unpossessed);
	Super::UnPossessed();
}

void AWuwaCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	PawnAbilityInitComponent->TryInitializeAbilitySystem();
}

void AWuwaCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	PawnAbilityInitComponent->TryInitializeAbilitySystem();
}

FWuwaCombatTargetResponse AWuwaCharacter::EvaluateCombatTarget_Implementation(const FWuwaCombatTargetQuery& Query) const
{
	FWuwaCombatTargetResponse Response;
	if (!Query.WindowHandle.IsValid() || !Query.SourceActor.IsValid() || !Query.InstigatorActor.IsValid() ||
	    !Query.AttackTag.IsValid() || Query.SourceActor.Get() == this || Query.InstigatorActor.Get() == this ||
	    IsActorBeingDestroyed())
	{
		Response.RejectionReason = EWuwaCombatTargetRejectionReason::InvalidQuery;
		return Response;
	}

	if (!bCombatTargetEnabled)
	{
		Response.RejectionReason = EWuwaCombatTargetRejectionReason::CombatTargetDisabled;
		return Response;
	}

	Response.bCanReceiveCombatHit = true;
	Response.RejectionReason = EWuwaCombatTargetRejectionReason::None;
	return Response;
}

UWuwaAbilitySystemComponent* AWuwaCharacter::GetDamageReceiverAbilitySystemComponent() const
{
	return IsValid(PawnAbilityInitComponent) ? PawnAbilityInitComponent->GetWuwaAbilitySystemComponent() : nullptr;
}

UWuwaHealthComponent* AWuwaCharacter::GetDamageReceiverHealthComponent() const
{
	return HealthComponent;
}

UWuwaPoiseComponent* AWuwaCharacter::GetDamageReceiverPoiseComponent() const
{
	return PoiseComponent;
}

void AWuwaCharacter::BindDeathFactDelegate()
{
	if (!IsValid(HealthComponent) || DeathFactDelegateHandle.IsValid())
	{
		return;
	}

	DeathFactDelegateHandle = HealthComponent->OnDeathFact().AddUObject(this, &ThisClass::HandleDeathFact);
}

void AWuwaCharacter::UnbindDeathFactDelegate()
{
	if (!DeathFactDelegateHandle.IsValid())
	{
		return;
	}

	if (IsValid(HealthComponent))
	{
		HealthComponent->OnDeathFact().Remove(DeathFactDelegateHandle);
	}
	DeathFactDelegateHandle.Reset();
}

void AWuwaCharacter::HandleDeathFact(const FWuwaDeathFact& Fact)
{
	if (Fact.DeadActor.Get() != this || Fact.DeathSequence <= 0 || Fact.DeathSequence <= LastHandledDeathSequence)
	{
		return;
	}

	LastHandledDeathSequence = Fact.DeathSequence;
	bCombatTargetEnabled = false;

	if (IsValid(PawnAbilityInitComponent))
	{
		PawnAbilityInitComponent->HandlePawnDeath(Fact);
	}
	if (IsValid(ActionNetworkComponent))
	{
		ActionNetworkComponent->InvalidateAvatarGenerations(EWuwaGrappleMovementEndReason::Death,
		                                                    EWuwaActionEndReason::Death);
	}
	else if (IsValid(ActionCoordinatorComponent))
	{
		ActionCoordinatorComponent->AbortAllActions(EWuwaActionEndReason::Death);
	}
	if (IsValid(GrapplePresentationComponent))
	{
		GrapplePresentationComponent->InvalidateAvatarPresentation();
	}
	if (IsValid(CombatExecutionComponent))
	{
		CombatExecutionComponent->AbortAllWindows(EWuwaCombatWindowEndReason::Death);
	}
	if (UWuwaCharacterMovementComponent* Movement = GetWuwaMovementComponent())
	{
		Movement->DisableMovement();
	}

	if (Fact.bAuthority)
	{
		if (AWuwaGameMode* GameMode =
		        GetWorld() != nullptr ? Cast<AWuwaGameMode>(GetWorld()->GetAuthGameMode()) : nullptr)
		{
			GameMode->HandlePawnDeath(Fact);
		}
	}
}

bool AWuwaCharacter::InitializeMovementRuntime(UWuwaCharacterMovementComponent* Movement)
{
	if (!IsValid(Movement) || !IsValid(MovementProfile))
	{
		UE_LOG(LogWuwa, Error, TEXT("Movement Runtime 缺少 Movement 或 Profile。Owner=%s"), *GetNameSafe(this));
		return false;
	}

	Movement->SetStateTagComponent(StateTagComponent);
	if (!Movement->ApplyMovementProfile(MovementProfile))
	{
		return false;
	}

	return true;
}

bool AWuwaCharacter::InitializeActionRuntime(UWuwaCharacterMovementComponent* Movement)
{
	if (!IsValid(Movement) || !IsValid(ActionRuleSet) || !IsValid(StateTagComponent) ||
	    !IsValid(ActionCoordinatorComponent) || !IsValid(ActionNetworkComponent) ||
	    !IsValid(MessageDispatcherComponent) || !IsValid(ActionRuleIntentProviderComponent) ||
	    !IsValid(MovementActionCapabilityComponent) || !IsValid(ActionAnimationCapabilityComponent) ||
	    !IsValid(TargetingComponent) || !IsValid(AbilityInputRouterComponent))
	{
		UE_LOG(LogWuwa, Error, TEXT("Action Runtime 缺少核心依赖。Owner=%s"), *GetNameSafe(this));
		return false;
	}

	const bool bInitialized =
	    ActionCoordinatorComponent->Initialize(StateTagComponent) &&
	    MessageDispatcherComponent->Initialize(ActionCoordinatorComponent) &&
	    MessageDispatcherComponent->InitializeAbilityInputRouter(AbilityInputRouterComponent) &&
	    ActionRuleIntentProviderComponent->Initialize(ActionRuleSet) &&
	    MessageDispatcherComponent->RegisterIntentProvider(ActionRuleIntentProviderComponent) &&
	    MovementActionCapabilityComponent->Initialize(this, Movement, MessageDispatcherComponent) &&
	    ActionAnimationCapabilityComponent->Initialize(this, MessageDispatcherComponent) &&
	    ActionCoordinatorComponent->RegisterCapability(ActionAnimationCapabilityComponent) &&
	    ActionCoordinatorComponent->RegisterCapability(MovementActionCapabilityComponent) &&
	    ActionNetworkComponent->Initialize(this,
	                                       ActionCoordinatorComponent,
	                                       Movement,
	                                       MovementActionCapabilityComponent,
	                                       ActionAnimationCapabilityComponent,
	                                       ActionRuleSet,
	                                       TraversalProfile) &&
	    MessageDispatcherComponent->InitializeActionNetwork(ActionNetworkComponent) &&
	    MessageDispatcherComponent->RegisterImmediateHandler(Movement) &&
	    MessageDispatcherComponent->RegisterImmediateHandler(TargetingComponent);
	return bInitialized;
}

bool AWuwaCharacter::InitializeTargetingRuntime(UWuwaCharacterMovementComponent* Movement)
{
	if (!IsValid(Movement) || !IsValid(TargetingComponent) || !IsValid(TargetingProfile) || !IsValid(StateTagComponent))
	{
		UE_LOG(LogWuwa, Error, TEXT("Targeting Runtime 缺少核心依赖。Owner=%s"), *GetNameSafe(this));
		return false;
	}

	if (!TargetingComponent->Initialize(this, StateTagComponent, TargetingProfile) ||
	    !Movement->SetTargetingComponent(TargetingComponent))
	{
		return false;
	}

	return true;
}

bool AWuwaCharacter::InitializeCameraRuntime()
{
	if (!IsValid(CameraProfile) || !IsValid(TargetingComponent) || !IsValid(CameraBoom) || !IsValid(FollowCamera) ||
	    !IsValid(CameraModeComponent))
	{
		UE_LOG(LogWuwa, Error, TEXT("Camera Runtime 缺少核心依赖。Owner=%s"), *GetNameSafe(this));
		return false;
	}

	if (!CameraBoom->ConfigureCollision(CameraProfile->ProbeSize,
	                                    CameraProfile->ProbeChannel.GetValue(),
	                                    CameraProfile->CollisionRecoveryInterpSpeed) ||
	    !CameraModeComponent->Initialize(CameraProfile, TargetingComponent, CameraBoom, FollowCamera))
	{
		return false;
	}

	return true;
}

bool AWuwaCharacter::TryInitializeTraversalRuntime(UWuwaCharacterMovementComponent* Movement)
{
	if (!IsValid(TraversalProfile) || !TraversalProfile->IsRuntimeValid() || !IsValid(Movement) ||
	    !IsValid(TraversalActionIntentProviderComponent) || !IsValid(GrappleCapabilityComponent) ||
	    !IsValid(MessageDispatcherComponent) || !IsValid(ActionCoordinatorComponent) ||
	    !IsValid(ActionNetworkComponent))
	{
		UE_LOG(LogWuwa, Warning, TEXT("Traversal Runtime 未配置，Grapple 已禁用。Owner=%s"), *GetNameSafe(this));
		return false;
	}

	if (!TraversalActionIntentProviderComponent->Initialize(TraversalProfile->TraversalRuleSet) ||
	    !GrappleCapabilityComponent->Initialize(this, Movement, MessageDispatcherComponent) ||
	    !ActionCoordinatorComponent->RegisterCapability(GrappleCapabilityComponent) ||
	    !ActionNetworkComponent->BindGrappleCapability(GrappleCapabilityComponent))
	{
		UE_LOG(LogWuwa, Warning, TEXT("Traversal Runtime 能力装配失败，Grapple 已禁用。Owner=%s"), *GetNameSafe(this));
		return false;
	}

	if (!MessageDispatcherComponent->RegisterIntentProvider(TraversalActionIntentProviderComponent))
	{
		ActionCoordinatorComponent->UnregisterCapability(GrappleCapabilityComponent);
		UE_LOG(LogWuwa, Warning, TEXT("Traversal Provider 注册失败，Grapple 已禁用。Owner=%s"), *GetNameSafe(this));
		return false;
	}

	if (!IsValid(GrapplePresentationComponent) || !GrapplePresentationComponent->Initialize(this,
	                                                                                        Movement,
	                                                                                        GrappleCapabilityComponent,
	                                                                                        MessageDispatcherComponent,
	                                                                                        ActionCoordinatorComponent,
	                                                                                        ActionNetworkComponent,
	                                                                                        CameraModeComponent))
	{
		UE_LOG(
		    LogWuwa, Warning, TEXT("Grapple Presentation 装配失败，Gameplay 保持可用。Owner=%s"), *GetNameSafe(this));
	}

	return true;
}

bool AWuwaCharacter::SubmitInputFrame(const FWuwaInputFrame& InputFrame)
{
	if (!InputFrame.IsValid() || !IsValid(MessageDispatcherComponent))
	{
		return false;
	}

	MessageDispatcherComponent->BeginInputFrame();

	TryInterruptAbilityFromMoveInput(InputFrame);

	ApplyContinuousInput(InputFrame);

	// 获取当前帧角色物理状态快照
	const FWuwaActionResolutionSnapshot ResolutionSnapshot = BuildActionResolutionSnapshot();
	if (!ResolutionSnapshot.IsValid())
	{
		return false;
	}

	MessageDispatcherComponent->ProcessInputFrame(InputFrame, ResolutionSnapshot);
	return true;
}

void AWuwaCharacter::ApplyContinuousInput(const FWuwaInputFrame& InputFrame)
{
	CurrentMoveIntent =
	    InputFrame.MoveIntent.ContainsNaN() ? FVector2D::ZeroVector : InputFrame.MoveIntent.GetClampedToMaxSize(1.f);

	const FVector2D EffectiveMoveIntent = IsMoveInputBlocked() ? FVector2D::ZeroVector : CurrentMoveIntent;

	if (UWuwaCharacterMovementComponent* Movement = GetWuwaMovementComponent())
	{
		Movement->SetLocomotionIntent(EffectiveMoveIntent);
	}

	DoMove(EffectiveMoveIntent.X, EffectiveMoveIntent.Y);
	DoLook(InputFrame.LookIntent.X, InputFrame.LookIntent.Y);
}

FWuwaActionResolutionSnapshot AWuwaCharacter::BuildActionResolutionSnapshot() const
{
	FWuwaActionResolutionSnapshot Snapshot;
	const UWuwaCharacterMovementComponent* Movement = GetWuwaMovementComponent();
	if (!IsValid(Movement))
	{
		return Snapshot;
	}

	Snapshot.FacingDirection = GetActorForwardVector().GetSafeNormal2D();
	Snapshot.ViewRotation = GetController() != nullptr ? GetController()->GetControlRotation() : GetActorRotation();
	Snapshot.MovementMode = Movement->MovementMode;
	Snapshot.CustomMovementMode = Movement->CustomMovementMode;
	Snapshot.bIsGrounded = Movement->IsMovingOnGround() &&
	                       (Movement->MovementMode == MOVE_Walking || Movement->MovementMode == MOVE_NavWalking);
	Snapshot.bIsAirborne = Movement->IsAirborneState();
	Snapshot.SourceObject = const_cast<AWuwaCharacter*>(this);
	return Snapshot;
}

UWuwaCharacterMovementComponent* AWuwaCharacter::GetWuwaMovementComponent() const
{
	return Cast<UWuwaCharacterMovementComponent>(GetCharacterMovement());
}

FWuwaLocomotionSnapshot AWuwaCharacter::GetLocomotionSnapshot() const
{
	const UWuwaCharacterMovementComponent* Movement = GetWuwaMovementComponent();
	return IsValid(Movement) ? Movement->GetLocomotionSnapshot() : FWuwaLocomotionSnapshot();
}

bool AWuwaCharacter::IsMoveInputBlocked() const
{
	return IsValid(StateTagComponent) && StateTagComponent->HasTag(WuwaGameplayTags::Block_Input_Move, true);
}

void AWuwaCharacter::DoMove(const float Right, const float Forward)
{
	if (IsMoveInputBlocked() || GetController() == nullptr)
	{
		return;
	}

	const FRotator YawRotation(0.f, GetController()->GetControlRotation().Yaw, 0.f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	AddMovementInput(ForwardDirection, Forward);
	AddMovementInput(RightDirection, Right);
}

void AWuwaCharacter::DoLook(const float Yaw, const float Pitch)
{
	if (IsValid(CameraModeComponent) && CameraModeComponent->HasViewRotationAuthority())
	{
		return;
	}

	constexpr float LookInterruptThreshold = 0.001f;
	if (FMath::Abs(Yaw) <= LookInterruptThreshold && FMath::Abs(Pitch) <= LookInterruptThreshold)
	{
		return;
	}

	if (IsValid(CameraModeComponent))
	{
		CameraModeComponent->CancelExplorationRecenter();
	}

	if (GetController() != nullptr)
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

bool AWuwaCharacter::CanJumpInternal_Implementation() const
{
	const UWuwaCharacterMovementComponent* Movement = GetWuwaMovementComponent();
	return !IsCrouched() && IsValid(Movement) && Movement->CanAttemptWuwaJump();
}

void AWuwaCharacter::TryInterruptAbilityFromMoveInput(const FWuwaInputFrame& InputFrame)
{
	// 没有 WASD 移动输入，不需要尝试打断。
	if (InputFrame.MoveIntent.IsNearlyZero())
	{
		return;
	}

	UWuwaActionAbilityInteropComponent* Interop = GetActionAbilityInteropComponent();

	if (!IsValid(Interop))
	{
		return;
	}

	// 当前根本没有阻止 Move 的 Ability，
	// 就不需要进入 Interrupt 查询。
	if (!Interop->HasBlockingActiveAbility(EWuwaAbilityInterruptSource::Move))
	{
		return;
	}

	// Try 内部已经完成：
	//
	// 1. InterruptWindow 是否开启
	// 2. bAllowMoveInterrupt 是否允许
	// 3. CanBeCanceled()
	// 4. 所有 Blocking Ability 是否都能取消
	// 5. 最终 CancelAbility()

	Interop->TryInterruptActiveAbility(EWuwaAbilityInterruptSource::Move);
}
