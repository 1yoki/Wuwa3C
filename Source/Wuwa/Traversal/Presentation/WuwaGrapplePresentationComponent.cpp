#include "Traversal/Presentation/WuwaGrapplePresentationComponent.h"

#include "Actions/Runtime/WuwaActionCoordinatorComponent.h"
#include "Actions/Network/WuwaActionNetworkComponent.h"
#include "Camera/WuwaCameraModeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Core/WuwaGameplayTags.h"
#include "GameFramework/GameStateBase.h"
#include "Messaging/WuwaCharacterMessageDispatcherComponent.h"
#include "Movement/WuwaCharacterMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Net/UnrealNetwork.h"
#include "Engine/StaticMesh.h"
#include "Traversal/Actions/WuwaGrappleCapabilityComponent.h"
#include "WuwaCharacter.h"
#include "Wuwa.h"

UWuwaGrapplePresentationComponent::UWuwaGrapplePresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(true);
}

bool UWuwaGrapplePresentationComponent::Initialize(AWuwaCharacter* InCharacter,
                                                   UWuwaCharacterMovementComponent* InMovementComponent,
                                                   UWuwaGrappleCapabilityComponent* InGrappleCapability,
                                                   UWuwaCharacterMessageDispatcherComponent* InDispatcher,
                                                   UWuwaActionCoordinatorComponent* InCoordinator,
                                                   UWuwaActionNetworkComponent* InActionNetwork,
                                                   UWuwaCameraModeComponent* InCameraMode)
{
	ResetRuntimeState();
	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		GrapplePresentationState.Reset();
	}
	if (!IsValid(InCharacter) || InCharacter != GetOwner() || !IsValid(InMovementComponent) ||
	    InMovementComponent != InCharacter->GetWuwaMovementComponent() || !IsValid(InGrappleCapability) ||
	    InGrappleCapability->GetOwner() != InCharacter || !IsValid(InDispatcher) ||
	    InDispatcher->GetOwner() != InCharacter || !IsValid(InCoordinator) ||
	    InCoordinator->GetOwner() != InCharacter || !IsValid(InActionNetwork) ||
	    InActionNetwork->GetOwner() != InCharacter)
	{
		UE_LOG(LogWuwa,
		       Warning,
		       TEXT("Grapple Presentation 初始化失败，Gameplay 来源无效。Owner=%s"),
		       *GetNameSafe(GetOwner()));
		return false;
	}

	CharacterOwner = InCharacter;
	MovementComponent = InMovementComponent;
	GrappleCapability = InGrappleCapability;
	Dispatcher = InDispatcher;
	Coordinator = InCoordinator;
	ActionNetwork = InActionNetwork;
	CameraMode = IsValid(InCameraMode) && InCameraMode->GetOwner() == InCharacter ? InCameraMode : nullptr;

	InCoordinator->OnActionStarted.AddUObject(this, &UWuwaGrapplePresentationComponent::HandleActionStarted);
	InCoordinator->OnActionFinalized.AddUObject(this, &UWuwaGrapplePresentationComponent::HandleActionFinalized);
	InDispatcher->OnActionEventPublished.AddUObject(this,
	                                                &UWuwaGrapplePresentationComponent::HandleActionEventPublished);
	InMovementComponent->OnGrappleMovementFact.AddUObject(
	    this, &UWuwaGrapplePresentationComponent::HandleGrappleMovementFact);
	bInitialized = true;
	SetComponentTickEnabled(true);
	if (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy && GrapplePresentationState.NetworkGeneration.IsValid())
	{
		OnRep_GrapplePresentationState();
	}
	return true;
}

bool UWuwaGrapplePresentationComponent::IsInitialized() const
{
	return bInitialized && IsValid(CharacterOwner) && IsValid(MovementComponent) && IsValid(GrappleCapability) &&
	       IsValid(ActionNetwork) && Dispatcher.IsValid() && Coordinator.IsValid();
}

void UWuwaGrapplePresentationComponent::InvalidateAvatarPresentation()
{
	if (UWuwaCameraModeComponent* Camera = CameraMode.Get())
	{
		Camera->ForceReleaseCameraFeedbackBySource(this);
	}
	if (IsValid(ActionNetwork))
	{
		ActionNetwork->StopReplicatedActionPresentation(GrapplePresentationState.NetworkGeneration);
	}
	for (FWuwaGrapplePresentationRecord& Record : Records)
	{
		if (Record.bRemotePresentation && IsValid(ActionNetwork) &&
		    Record.NetworkGeneration != GrapplePresentationState.NetworkGeneration)
		{
			ActionNetwork->StopReplicatedActionPresentation(Record.NetworkGeneration);
		}
		DestroyRope(Record);
	}
	Records.Reset();
	GrapplePresentationState.Reset();
	if (IsValid(CharacterOwner) && CharacterOwner->HasAuthority())
	{
		CharacterOwner->ForceNetUpdate();
	}
}

void UWuwaGrapplePresentationComponent::TickComponent(const float DeltaTime,
                                                      const ELevelTick TickType,
                                                      FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsInitialized() || !FMath::IsFinite(DeltaTime) || DeltaTime < 0.f)
	{
		return;
	}

	const FWuwaGrappleMovementSnapshot MovementSnapshot = MovementComponent->GetGrappleMovementSnapshot();
	USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh();

	for (FWuwaGrapplePresentationRecord& Record : Records)
	{
		if (IsValid(Record.RopeComponent) && !Record.bRopeFading &&
		    ((Record.bRemotePresentation && GrapplePresentationState.bActive &&
		      Record.NetworkGeneration == GrapplePresentationState.NetworkGeneration) ||
		     (!Record.bRemotePresentation && MovementSnapshot.Handle == Record.MovementHandle &&
		      MovementSnapshot.bActive)) &&
		    IsValid(Mesh))
		{
			const FVector RopeStart = Mesh->GetSocketLocation(Record.PresentationSpec.HandSocketName);
			Record.RopeComponent->SetVariableVec3(Record.PresentationSpec.RopeStartParameterName, RopeStart);
			Record.RopeComponent->SetVariableVec3(Record.PresentationSpec.RopeEndParameterName,
			                                      Record.VisualAnchorLocation);
			if (IsValid(Record.RopeSplineComponent))
			{
				const FVector RopeTangent = Record.VisualAnchorLocation - RopeStart;
				Record.RopeSplineComponent->SetStartAndEnd(
				    RopeStart, RopeTangent, Record.VisualAnchorLocation, RopeTangent, true);
			}
		}

		if (Record.bRopeFading && IsValid(Record.RopeComponent))
		{
			Record.RopeFadeElapsed += DeltaTime;
			if (IsValid(Record.RopeSplineComponent) && Record.PresentationSpec.RopeFadeOutDuration > KINDA_SMALL_NUMBER)
			{
				const float FadeAlpha =
				    FMath::Clamp(1.f - Record.RopeFadeElapsed / Record.PresentationSpec.RopeFadeOutDuration, 0.f, 1.f);
				const FVector2D RopeScale(0.025f * FadeAlpha, 0.025f * FadeAlpha);
				Record.RopeSplineComponent->SetStartScale(RopeScale, true);
				Record.RopeSplineComponent->SetEndScale(RopeScale, true);
			}
			if (Record.RopeFadeElapsed >= Record.PresentationSpec.RopeFadeOutDuration)
			{
				DestroyRope(Record);
			}
		}
	}

	Records.RemoveAll(
	    [](const FWuwaGrapplePresentationRecord& Record)
	    {
		    return Record.bFinalized && !IsValid(Record.RopeComponent);
	    });
}

void UWuwaGrapplePresentationComponent::HandleActionStarted(const FWuwaActionResult& Result)
{
	if (!IsInitialized() || !Result.HasStarted() || FindRecord(Result.ActionHandle) != nullptr)
	{
		return;
	}

	const UWuwaGrappleActionDefinition* Definition = nullptr;
	FWuwaGrappleActionContext Context;
	if (!GrappleCapability->GetActivePresentationData(Result.ActionHandle, Definition, Context) || !IsValid(Definition))
	{
		if (Result.ActionTag.MatchesTag(WuwaGameplayTags::Action_Traversal_Grapple))
		{
			UE_LOG(LogWuwa,
			       Warning,
			       TEXT("Grapple Presentation 无法读取冻结表现数据。ActionHandle=%lld"),
			       Result.ActionHandle.Value);
		}
		return;
	}

	FWuwaGrapplePresentationRecord& Record = Records.AddDefaulted_GetRef();
	Record.ActionHandle = Result.ActionHandle;
	Record.NetworkGeneration = Result.NetworkGeneration;
	Record.MovementHandle = GrappleCapability->GetRuntimeSnapshot().MovementHandle;
	Record.VisualAnchorLocation = Context.VisualAnchorLocation;
	Record.PresentationSpec = Definition->PresentationSpec;
	if (CharacterOwner->IsLocallyControlled())
	{
		if (UWuwaCameraModeComponent* Camera = CameraMode.Get())
		{
			Record.CameraFeedbackHandle = Camera->AcquireCameraFeedback(Definition->CameraFeedbackSpec, this);
			if (!Record.CameraFeedbackHandle.IsValid())
			{
				UE_LOG(LogWuwa,
				       Warning,
				       TEXT("Grapple Camera Feedback 取得失败，不阻止 Gameplay。ActionHandle=%lld"),
				       Result.ActionHandle.Value);
			}
		}
	}

	if (CharacterOwner->HasAuthority())
	{
		const UWorld* World = GetWorld();
		const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;
		const float WorldTime = IsValid(World) ? World->GetTimeSeconds() : 0.f;
		GrapplePresentationState.NetworkGeneration = Result.NetworkGeneration;
		GrapplePresentationState.AuthorityVisualAnchor = Context.VisualAnchorLocation;
		GrapplePresentationState.Phase = EWuwaGrappleRuntimePhase::Windup;
		GrapplePresentationState.ServerStartTime =
		    IsValid(GameState) ? GameState->GetServerWorldTimeSeconds() : WorldTime;
		GrapplePresentationState.bActive = true;
		GrapplePresentationState.EndReason = EWuwaGrappleMovementEndReason::None;
		CharacterOwner->ForceNetUpdate();
	}
}

void UWuwaGrapplePresentationComponent::HandleActionEventPublished(const FWuwaActionEventMessage& Message)
{
	FWuwaGrapplePresentationRecord* Record = FindRecord(Message.Handle);
	if (Record == nullptr)
	{
		return;
	}

	if (Message.EventTag == WuwaGameplayTags::Action_Event_Traversal_Grapple_Visual_Attach)
	{
		CreateRope(*Record);
	}
	else if (Message.EventTag == WuwaGameplayTags::Action_Event_Traversal_Grapple_Visual_Detach)
	{
		BeginRopeFade(*Record);
	}
	else if (Message.EventTag == WuwaGameplayTags::Action_Event_Traversal_Grapple_Released)
	{
		Record->bReleaseStarted = true;
		BeginRopeFade(*Record);
		if (Record->CameraFeedbackHandle.IsValid())
		{
			if (UWuwaCameraModeComponent* Camera = CameraMode.Get())
			{
				Camera->BeginReleaseCameraFeedback(Record->CameraFeedbackHandle);
			}
			Record->CameraFeedbackHandle.Reset();
		}
	}
}

void UWuwaGrapplePresentationComponent::HandleActionFinalized(const FWuwaActionFinalizedMessage& Message)
{
	FWuwaGrapplePresentationRecord* Record = FindRecord(Message.Handle);
	if (Record == nullptr)
	{
		return;
	}

	Record->bFinalized = true;
	if (!Record->bReleaseStarted && Record->CameraFeedbackHandle.IsValid())
	{
		if (UWuwaCameraModeComponent* Camera = CameraMode.Get())
		{
			Camera->ForceReleaseCameraFeedback(Record->CameraFeedbackHandle);
		}
		Record->CameraFeedbackHandle.Reset();
	}

	BeginRopeFade(*Record);

	if (CharacterOwner->HasAuthority() && GrapplePresentationState.NetworkGeneration == Message.NetworkGeneration &&
	    GrapplePresentationState.bActive)
	{
		GrapplePresentationState.Phase = EWuwaGrappleRuntimePhase::Releasing;
		GrapplePresentationState.bActive = false;
		GrapplePresentationState.EndReason = EWuwaGrappleMovementEndReason::Stopped;
		CharacterOwner->ForceNetUpdate();
	}
}

void UWuwaGrapplePresentationComponent::HandleGrappleMovementFact(const FWuwaGrappleMovementFact& Fact)
{
	if (!IsInitialized() || !CharacterOwner->HasAuthority() || !GrapplePresentationState.NetworkGeneration.IsValid())
	{
		return;
	}

	const FWuwaGrapplePresentationRecord* Record = FindRecord(GrapplePresentationState.NetworkGeneration);
	if (Record == nullptr || Record->MovementHandle != Fact.Handle)
	{
		return;
	}

	if (Fact.FactType == EWuwaGrappleMovementFactType::PhaseChanged)
	{
		GrapplePresentationState.Phase = Fact.Phase;
	}
	else
	{
		GrapplePresentationState.Phase = EWuwaGrappleRuntimePhase::Releasing;
		GrapplePresentationState.bActive = false;
		GrapplePresentationState.EndReason = Fact.EndReason;
	}
	CharacterOwner->ForceNetUpdate();
}

FWuwaGrapplePresentationRecord* UWuwaGrapplePresentationComponent::FindRecord(const FWuwaActionHandle& Handle)
{
	return Records.FindByPredicate(
	    [&Handle](const FWuwaGrapplePresentationRecord& Record)
	    {
		    return Record.ActionHandle == Handle;
	    });
}

FWuwaGrapplePresentationRecord*
UWuwaGrapplePresentationComponent::FindRecord(const FWuwaNetworkActionGeneration& Generation)
{
	return Records.FindByPredicate(
	    [&Generation](const FWuwaGrapplePresentationRecord& Record)
	    {
		    return Record.NetworkGeneration == Generation;
	    });
}

void UWuwaGrapplePresentationComponent::OnRep_GrapplePresentationState()
{
	if (!IsInitialized() || CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		return;
	}

	if (!GrapplePresentationState.NetworkGeneration.IsValid())
	{
		for (FWuwaGrapplePresentationRecord& Record : Records)
		{
			if (Record.bRemotePresentation)
			{
				ActionNetwork->StopReplicatedActionPresentation(Record.NetworkGeneration);
				Record.bFinalized = true;
				BeginRopeFade(Record);
			}
		}
		return;
	}

	if (!GrapplePresentationState.IsPayloadValid())
	{
		UE_LOG(LogWuwa,
		       Error,
		       TEXT("Simulated Proxy 拒绝无效 Grapple 表现状态。Owner=%s, Generation=%d"),
		       *GetNameSafe(CharacterOwner),
		       GrapplePresentationState.NetworkGeneration.Value);
		return;
	}

	FWuwaGrapplePresentationRecord* Record = FindRecord(GrapplePresentationState.NetworkGeneration);
	if (GrapplePresentationState.bActive)
	{
		for (FWuwaGrapplePresentationRecord& ExistingRecord : Records)
		{
			if (ExistingRecord.bRemotePresentation &&
			    ExistingRecord.NetworkGeneration != GrapplePresentationState.NetworkGeneration)
			{
				ActionNetwork->StopReplicatedActionPresentation(ExistingRecord.NetworkGeneration);
				ExistingRecord.bFinalized = true;
				BeginRopeFade(ExistingRecord);
			}
		}

		ActionNetwork->PlayReplicatedActionPresentation(GrapplePresentationState.NetworkGeneration,
		                                                WuwaGameplayTags::Action_Traversal_Grapple_Free,
		                                                GrapplePresentationState.ServerStartTime);

		if (Record == nullptr)
		{
			const UWuwaGrappleActionDefinition* Definition = Cast<UWuwaGrappleActionDefinition>(
			    ActionNetwork->FindRegisteredDefinition(WuwaGameplayTags::Action_Traversal_Grapple_Free));
			if (!IsValid(Definition))
			{
				UE_LOG(LogWuwa,
				       Error,
				       TEXT("Simulated Proxy 缺少 Free Grapple Definition。Owner=%s"),
				       *GetNameSafe(CharacterOwner));
				return;
			}

			Record = &Records.AddDefaulted_GetRef();
			Record->NetworkGeneration = GrapplePresentationState.NetworkGeneration;
			Record->VisualAnchorLocation = GrapplePresentationState.AuthorityVisualAnchor;
			Record->PresentationSpec = Definition->PresentationSpec;
			Record->bRemotePresentation = true;
			CreateRope(*Record);
		}
		else
		{
			Record->VisualAnchorLocation = GrapplePresentationState.AuthorityVisualAnchor;
		}
		return;
	}

	ActionNetwork->StopReplicatedActionPresentation(GrapplePresentationState.NetworkGeneration);
	if (Record != nullptr)
	{
		Record->bReleaseStarted = true;
		Record->bFinalized = true;
		BeginRopeFade(*Record);
	}
}

void UWuwaGrapplePresentationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UWuwaGrapplePresentationComponent, GrapplePresentationState, COND_SkipOwner);
}

bool UWuwaGrapplePresentationComponent::CreateRope(FWuwaGrapplePresentationRecord& Record)
{
	if (IsValid(Record.RopeComponent) || Record.bRopeFading || Record.bFinalized ||
	    !Record.PresentationSpec.IsConfigured() || !Record.PresentationSpec.IsRuntimeValid())
	{
		return false;
	}

	USkeletalMeshComponent* Mesh = IsValid(CharacterOwner) ? CharacterOwner->GetMesh() : nullptr;
	UNiagaraSystem* RopeSystem = Record.PresentationSpec.RopeSystem.LoadSynchronous();
	if (!IsValid(Mesh) || !Mesh->DoesSocketExist(Record.PresentationSpec.HandSocketName) || !IsValid(RopeSystem))
	{
		UE_LOG(LogWuwa,
		       Warning,
		       TEXT("Grapple Rope 创建失败，Socket 或 Niagara 无效。ActionHandle=%lld"),
		       Record.ActionHandle.Value);
		return false;
	}

	UNiagaraComponent* RopeComponent = NewObject<UNiagaraComponent>(this);
	if (!IsValid(RopeComponent))
	{
		return false;
	}

	RopeComponent->SetAsset(RopeSystem);
	RopeComponent->SetAutoDestroy(false);
	RopeComponent->RegisterComponentWithWorld(GetWorld());
	RopeComponent->SetVariableVec3(Record.PresentationSpec.RopeStartParameterName,
	                               Mesh->GetSocketLocation(Record.PresentationSpec.HandSocketName));
	RopeComponent->SetVariableVec3(Record.PresentationSpec.RopeEndParameterName, Record.VisualAnchorLocation);
	RopeComponent->Activate(true);
	Record.RopeComponent = RopeComponent;

	UStaticMesh* RopeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	USplineMeshComponent* RopeSpline = NewObject<USplineMeshComponent>(this);
	if (!IsValid(RopeMesh) || !IsValid(RopeSpline))
	{
		DestroyRope(Record);
		return false;
	}

	RopeSpline->SetMobility(EComponentMobility::Movable);
	RopeSpline->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RopeSpline->SetCastShadow(false);
	RopeSpline->SetStaticMesh(RopeMesh);
	RopeSpline->SetForwardAxis(ESplineMeshAxis::Z, false);
	RopeSpline->RegisterComponentWithWorld(GetWorld());
	RopeSpline->SetWorldTransform(FTransform::Identity);
	const FVector RopeStart = Mesh->GetSocketLocation(Record.PresentationSpec.HandSocketName);
	const FVector RopeTangent = Record.VisualAnchorLocation - RopeStart;
	RopeSpline->SetStartScale(FVector2D(0.025f, 0.025f), false);
	RopeSpline->SetEndScale(FVector2D(0.025f, 0.025f), false);
	RopeSpline->SetStartAndEnd(RopeStart, RopeTangent, Record.VisualAnchorLocation, RopeTangent, true);
	Record.RopeSplineComponent = RopeSpline;
	return true;
}

void UWuwaGrapplePresentationComponent::BeginRopeFade(FWuwaGrapplePresentationRecord& Record)
{
	if (!IsValid(Record.RopeComponent) || Record.bRopeFading)
	{
		return;
	}

	Record.bRopeFading = true;
	Record.RopeFadeElapsed = 0.f;
	Record.RopeComponent->Deactivate();
	if (Record.PresentationSpec.RopeFadeOutDuration <= KINDA_SMALL_NUMBER)
	{
		DestroyRope(Record);
	}
}

void UWuwaGrapplePresentationComponent::DestroyRope(FWuwaGrapplePresentationRecord& Record)
{
	if (IsValid(Record.RopeComponent))
	{
		Record.RopeComponent->DestroyComponent();
	}
	if (IsValid(Record.RopeSplineComponent))
	{
		Record.RopeSplineComponent->DestroyComponent();
	}
	Record.RopeComponent = nullptr;
	Record.RopeSplineComponent = nullptr;
	Record.bRopeFading = false;
	Record.RopeFadeElapsed = 0.f;
}

void UWuwaGrapplePresentationComponent::ResetRuntimeState()
{
	if (UWuwaCharacterMessageDispatcherComponent* DispatcherComponent = Dispatcher.Get())
	{
		DispatcherComponent->OnActionEventPublished.RemoveAll(this);
	}
	if (UWuwaActionCoordinatorComponent* CoordinatorComponent = Coordinator.Get())
	{
		CoordinatorComponent->OnActionStarted.RemoveAll(this);
		CoordinatorComponent->OnActionFinalized.RemoveAll(this);
	}
	if (IsValid(MovementComponent))
	{
		MovementComponent->OnGrappleMovementFact.RemoveAll(this);
	}
	if (UWuwaCameraModeComponent* Camera = CameraMode.Get())
	{
		Camera->ForceReleaseCameraFeedbackBySource(this);
	}
	if (IsValid(ActionNetwork))
	{
		ActionNetwork->StopReplicatedActionPresentation(GrapplePresentationState.NetworkGeneration);
	}

	for (FWuwaGrapplePresentationRecord& Record : Records)
	{
		if (Record.bRemotePresentation && IsValid(ActionNetwork) &&
		    Record.NetworkGeneration != GrapplePresentationState.NetworkGeneration)
		{
			ActionNetwork->StopReplicatedActionPresentation(Record.NetworkGeneration);
		}
		DestroyRope(Record);
	}
	Records.Reset();
	CameraMode.Reset();
	Coordinator.Reset();
	Dispatcher.Reset();
	GrappleCapability = nullptr;
	ActionNetwork = nullptr;
	MovementComponent = nullptr;
	CharacterOwner = nullptr;
	bInitialized = false;
	SetComponentTickEnabled(false);
}

void UWuwaGrapplePresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetRuntimeState();
	GrapplePresentationState.Reset();
	Super::EndPlay(EndPlayReason);
}
