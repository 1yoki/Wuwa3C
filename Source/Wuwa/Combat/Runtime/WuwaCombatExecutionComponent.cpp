// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Runtime/WuwaCombatExecutionComponent.h"

#include "Combat/Contracts/WuwaCombatTargetInterface.h"
#include "Combat/Contracts/WuwaCombatTypes.h"
#include "Combat/Runtime/WuwaWeaponComponent.h"
#include "Combat/WuwaCombatLog.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UWuwaCombatExecutionComponent::UWuwaCombatExecutionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(false);
}

bool UWuwaCombatExecutionComponent::Initialize(USkeletalMeshComponent* CharacterMesh,
                                               UWuwaWeaponComponent* WeaponComponent)
{
	if (bInitialized && BoundCharacterMesh.Get() == CharacterMesh && BoundWeaponComponent.Get() == WeaponComponent &&
	    IsInitialized())
	{
		return true;
	}

	Shutdown(EWuwaCombatWindowEndReason::AvatarChanged);

	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !IsValid(CharacterMesh) || !IsValid(WeaponComponent) || CharacterMesh->GetOwner() != Owner ||
	    WeaponComponent->GetOwner() != Owner || !WeaponComponent->IsInitialized())
	{
		LastFailureReason = !IsValid(CharacterMesh) ? EWuwaCombatWindowFailureReason::InvalidCharacterMesh
		                                            : EWuwaCombatWindowFailureReason::InvalidWeapon;
		UE_LOG(LogWuwaCombat,
		       Warning,
		       TEXT("CombatExecution 初始化失败：Mesh、Weapon 或 Owner 关系无效。Owner=%s, FailureReason=%d"),
		       *GetNameSafe(Owner),
		       static_cast<int32>(LastFailureReason));
		return false;
	}

	BoundCharacterMesh = CharacterMesh;
	BoundWeaponComponent = WeaponComponent;
	bInitialized = true;
	LastFailureReason = EWuwaCombatWindowFailureReason::None;
	SetComponentTickEnabled(false);
	AddTickPrerequisiteComponent(CharacterMesh);
	return true;
}

void UWuwaCombatExecutionComponent::Shutdown(const EWuwaCombatWindowEndReason Reason)
{
	AbortAllWindows(Reason);
	RestoreMeshTickOption();
	SetComponentTickEnabled(false);

	if (USkeletalMeshComponent* CharacterMesh = BoundCharacterMesh.Get())
	{
		RemoveTickPrerequisiteComponent(CharacterMesh);
	}

	BoundCharacterMesh.Reset();
	BoundWeaponComponent.Reset();
	bInitialized = false;
	ActiveWindowHandle.Reset();
	bHasPreviousBladePose = false;
	HitActors.Reset();
}

FWuwaCombatWindowHandle UWuwaCombatExecutionComponent::BeginMeleeWindow(const FWuwaMeleeWindowRequest& Request)
{
	LastFailureReason = EWuwaCombatWindowFailureReason::None;
	AActor* Owner = GetOwner();
	if (!IsInitialized())
	{
		LastFailureReason = EWuwaCombatWindowFailureReason::NotInitialized;
	}
	else if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		LastFailureReason = EWuwaCombatWindowFailureReason::NotAuthority;
	}
	else if (HasActiveWindow())
	{
		LastFailureReason = EWuwaCombatWindowFailureReason::WindowAlreadyActive;
	}
	else if (!ValidateWindowRequest(Request))
	{
		LastFailureReason = EWuwaCombatWindowFailureReason::InvalidRequest;
	}

	if (LastFailureReason != EWuwaCombatWindowFailureReason::None)
	{
		UE_LOG(LogWuwaCombat,
		       Warning,
		       TEXT("CombatExecution 拒绝创建命中窗口。Owner=%s, Role=%d, FailureReason=%d"),
		       *GetNameSafe(Owner),
		       IsValid(Owner) ? static_cast<int32>(Owner->GetLocalRole()) : INDEX_NONE,
		       static_cast<int32>(LastFailureReason));
		return FWuwaCombatWindowHandle();
	}

	FrozenRequest = Request;
	FrozenRequest.MaxTargets = GetEffectiveMaxTargets();
	if (!ElevateMeshTickOption())
	{
		LastFailureReason = EWuwaCombatWindowFailureReason::InvalidCharacterMesh;
		return FWuwaCombatWindowHandle();
	}

	if (!ReadFrozenBladeEndpoints(PreviousBase, PreviousTip))
	{
		LastFailureReason = EWuwaCombatWindowFailureReason::InvalidInitialBladePose;
		RestoreMeshTickOption();
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("CombatExecution 无法读取初始剑刃姿态。Owner=%s, BaseSocket=%s, TipSocket=%s"),
		       *GetNameSafe(Owner),
		       *FrozenRequest.TraceBaseSocket.ToString(),
		       *FrozenRequest.TraceTipSocket.ToString());
		return FWuwaCombatWindowHandle();
	}

	CurrentBase = PreviousBase;
	CurrentTip = PreviousTip;
	bHasPreviousBladePose = true;
	HitActors.Reset();
	SweepCount = 0;
	HitCount = 0;
	LastHitFact = FWuwaCombatHitFact();
	LastEndReason = EWuwaCombatWindowEndReason::None;
	ActiveWindowHandle = AllocateWindowHandle();
	SetComponentTickEnabled(true);

	return ActiveWindowHandle;
}

bool UWuwaCombatExecutionComponent::EndMeleeWindow(const FWuwaCombatWindowHandle Handle,
                                                   const EWuwaCombatWindowEndReason Reason)
{
	if (!Handle.IsValid() || !HasActiveWindow() || Handle != ActiveWindowHandle)
	{
		LastFailureReason = EWuwaCombatWindowFailureReason::StaleWindowHandle;
		UE_LOG(LogWuwaCombat,
		       Warning,
		       TEXT("CombatExecution 忽略旧窗口结束请求。Owner=%s, Requested=%u, Active=%u"),
		       *GetNameSafe(GetOwner()),
		       Handle.Value,
		       ActiveWindowHandle.Value);
		return false;
	}

	LastEndReason = Reason;
	LastFailureReason = EWuwaCombatWindowFailureReason::None;
	ResetActiveWindow();
	return true;
}

void UWuwaCombatExecutionComponent::AbortAllWindows(const EWuwaCombatWindowEndReason Reason)
{
	if (HasActiveWindow())
	{
		EndMeleeWindow(ActiveWindowHandle, Reason);
		return;
	}

	SetComponentTickEnabled(false);
	RestoreMeshTickOption();
}

bool UWuwaCombatExecutionComponent::IsInitialized() const
{
	return bInitialized && BoundCharacterMesh.IsValid() && BoundWeaponComponent.IsValid() &&
	       BoundWeaponComponent->IsInitialized();
}

bool UWuwaCombatExecutionComponent::HasActiveWindow() const
{
	return ActiveWindowHandle.IsValid();
}

FWuwaCombatExecutionRuntimeSnapshot UWuwaCombatExecutionComponent::GetRuntimeSnapshot() const
{
	FWuwaCombatExecutionRuntimeSnapshot Snapshot;
	const AActor* Owner = GetOwner();
	const USkeletalMeshComponent* CharacterMesh = BoundCharacterMesh.Get();
	Snapshot.bInitialized = IsInitialized();
	Snapshot.bAuthority = IsValid(Owner) && Owner->HasAuthority();
	Snapshot.bWindowActive = HasActiveWindow();
	Snapshot.ActiveWindowHandle = ActiveWindowHandle;
	Snapshot.LastEndReason = LastEndReason;
	Snapshot.LastFailureReason = LastFailureReason;
	Snapshot.AttackTag = FrozenRequest.AttackTag;
	Snapshot.StepIndex = FrozenRequest.StepIndex;
	Snapshot.TraceRadius = FrozenRequest.TraceRadius;
	Snapshot.BladeSampleCount = FrozenRequest.BladeSampleCount;
	Snapshot.MaxTargets = FrozenRequest.MaxTargets;
	Snapshot.SweepCount = SweepCount;
	Snapshot.HitCount = HitCount;
	Snapshot.PreviousBase = PreviousBase;
	Snapshot.PreviousTip = PreviousTip;
	Snapshot.CurrentBase = CurrentBase;
	Snapshot.CurrentTip = CurrentTip;
	Snapshot.LastHitFact = LastHitFact;
	Snapshot.bHasPreviousBladePose = bHasPreviousBladePose;
	Snapshot.bMeshTickOptionElevated = bMeshTickOptionElevated;
	Snapshot.OriginalMeshTickOption = OriginalMeshTickOption;
	Snapshot.CurrentMeshTickOption =
	    IsValid(CharacterMesh) ? static_cast<uint8>(CharacterMesh->VisibilityBasedAnimTickOption) : 0;
	return Snapshot;
}

FWuwaCombatHitFactDelegate& UWuwaCombatExecutionComponent::OnCombatHitFact()
{
	return CombatHitFactDelegate;
}

void UWuwaCombatExecutionComponent::TickComponent(const float DeltaTime,
                                                  const ELevelTick TickType,
                                                  FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!HasActiveWindow())
	{
		SetComponentTickEnabled(false);
		return;
	}

	AActor* Owner = GetOwner();
	if (!IsInitialized() || !IsValid(Owner) || !Owner->HasAuthority())
	{
		LastFailureReason = IsValid(Owner) && !Owner->HasAuthority() ? EWuwaCombatWindowFailureReason::NotAuthority
		                                                             : EWuwaCombatWindowFailureReason::NotInitialized;
		AbortAllWindows(EWuwaCombatWindowEndReason::ExplicitAbort);
		return;
	}

	if (!ReadFrozenBladeEndpoints(CurrentBase, CurrentTip))
	{
		LastFailureReason = EWuwaCombatWindowFailureReason::InvalidWeapon;
		UE_LOG(LogWuwaCombat, Error, TEXT("CombatExecution 活动窗口丢失剑刃端点。Owner=%s"), *GetNameSafe(Owner));
		AbortAllWindows(EWuwaCombatWindowEndReason::ExplicitAbort);
		return;
	}

	if (!bHasPreviousBladePose)
	{
		PreviousBase = CurrentBase;
		PreviousTip = CurrentTip;
		bHasPreviousBladePose = true;
		return;
	}

	ExecuteBladeSweeps();
	PreviousBase = CurrentBase;
	PreviousTip = CurrentTip;
}

void UWuwaCombatExecutionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Shutdown(EWuwaCombatWindowEndReason::EndPlay);
	Super::EndPlay(EndPlayReason);
}

bool UWuwaCombatExecutionComponent::ValidateWindowRequest(const FWuwaMeleeWindowRequest& Request) const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) && Request.SourceActor.Get() == Owner && Request.InstigatorActor.IsValid() &&
	       Request.AttackTag.IsValid() && Request.StepIndex <= 2 && FMath::IsFinite(Request.TraceRadius) &&
	       Request.TraceRadius > 0.0f && Request.BladeSampleCount >= 2 && Request.BladeSampleCount <= 16 &&
	       Request.MaxTargets >= 1 && Request.MaxTargets <= WuwaCombatLimits::MaxMeleeTargets &&
	       !Request.TraceBaseSocket.IsNone() && !Request.TraceTipSocket.IsNone() &&
	       Request.TraceBaseSocket != Request.TraceTipSocket && BoundWeaponComponent.IsValid() &&
	       BoundWeaponComponent->IsInitialized();
}

bool UWuwaCombatExecutionComponent::ReadFrozenBladeEndpoints(FVector& OutBase, FVector& OutTip) const
{
	const UWuwaWeaponComponent* WeaponComponent = BoundWeaponComponent.Get();
	return IsValid(WeaponComponent) &&
	       WeaponComponent->GetBladeWorldEndpoints(
	           FrozenRequest.TraceBaseSocket, FrozenRequest.TraceTipSocket, OutBase, OutTip) &&
	       !OutBase.ContainsNaN() && !OutTip.ContainsNaN();
}

void UWuwaCombatExecutionComponent::ExecuteBladeSweeps()
{
	if (HitActors.Num() >= GetEffectiveMaxTargets())
	{
		return;
	}

	UWorld* World = GetWorld();
	AActor* SourceActor = FrozenRequest.SourceActor.Get();
	if (!IsValid(World) || !IsValid(SourceActor))
	{
		LastFailureReason = EWuwaCombatWindowFailureReason::WorldUnavailable;
		AbortAllWindows(EWuwaCombatWindowEndReason::ExplicitAbort);
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WuwaCombatMeleeSweep), false, SourceActor);
	QueryParams.AddIgnoredActor(SourceActor);
	if (AActor* InstigatorActor = FrozenRequest.InstigatorActor.Get())
	{
		QueryParams.AddIgnoredActor(InstigatorActor);
	}

	const FCollisionShape SweepShape = FCollisionShape::MakeSphere(FrozenRequest.TraceRadius);
	for (int32 SampleIndex = 0; SampleIndex < FrozenRequest.BladeSampleCount; ++SampleIndex)
	{
		const float Alpha = static_cast<float>(SampleIndex) / static_cast<float>(FrozenRequest.BladeSampleCount - 1);
		const FVector PreviousPoint = FMath::Lerp(PreviousBase, PreviousTip, Alpha);
		const FVector CurrentPoint = FMath::Lerp(CurrentBase, CurrentTip, Alpha);
		TArray<FHitResult> Hits;
		World->SweepMultiByChannel(Hits,
		                           PreviousPoint,
		                           CurrentPoint,
		                           FQuat::Identity,
		                           WuwaCombatCollision::MeleeTraceChannel,
		                           SweepShape,
		                           QueryParams);
		++SweepCount;
		ProcessSweepHits(Hits);
		if (HitActors.Num() >= GetEffectiveMaxTargets())
		{
			break;
		}
	}
}

void UWuwaCombatExecutionComponent::ProcessSweepHits(const TArray<FHitResult>& Hits)
{
	for (const FHitResult& Hit : Hits)
	{
		AActor* TargetActor = Hit.GetActor();
		UPrimitiveComponent* HitComponent = Hit.GetComponent();
		const TWeakObjectPtr<AActor> TargetKey(TargetActor);
		if (!IsValid(TargetActor) || TargetActor == FrozenRequest.SourceActor.Get() ||
		    TargetActor == FrozenRequest.InstigatorActor.Get() || HitActors.Contains(TargetKey) ||
		    !IsValid(HitComponent) ||
		    HitComponent->GetCollisionResponseToChannel(WuwaCombatCollision::MeleeTraceChannel) != ECR_Block ||
		    !TargetActor->GetClass()->ImplementsInterface(UWuwaCombatTargetInterface::StaticClass()) ||
		    !EvaluateCombatTarget(*TargetActor))
		{
			continue;
		}

		HitActors.Add(TargetKey);
		PublishHitFact(Hit, *TargetActor);
		if (HitActors.Num() >= GetEffectiveMaxTargets())
		{
			break;
		}
	}
}

FWuwaCombatTargetQuery UWuwaCombatExecutionComponent::BuildCombatTargetQuery() const
{
	FWuwaCombatTargetQuery Query;
	Query.WindowHandle = ActiveWindowHandle;
	Query.SourceActor = FrozenRequest.SourceActor;
	Query.InstigatorActor = FrozenRequest.InstigatorActor;
	Query.AttackTag = FrozenRequest.AttackTag;
	return Query;
}

bool UWuwaCombatExecutionComponent::EvaluateCombatTarget(AActor& TargetActor) const
{
	const FWuwaCombatTargetResponse Response =
	    IWuwaCombatTargetInterface::Execute_EvaluateCombatTarget(&TargetActor, BuildCombatTargetQuery());
	if (!Response.bCanReceiveCombatHit || Response.RejectionReason != EWuwaCombatTargetRejectionReason::None)
	{
		return false;
	}
	return true;
}

void UWuwaCombatExecutionComponent::PublishHitFact(const FHitResult& Hit, AActor& TargetActor)
{
	FWuwaCombatHitFact Fact;
	Fact.WindowHandle = ActiveWindowHandle;
	Fact.StepIndex = FrozenRequest.StepIndex;
	Fact.SourceActor = FrozenRequest.SourceActor;
	Fact.TargetActor = &TargetActor;
	Fact.ImpactPoint = FVector(Hit.ImpactPoint);
	if (Fact.ImpactPoint.ContainsNaN())
	{
		Fact.ImpactPoint = TargetActor.GetActorLocation();
	}
	Fact.ImpactNormal = !Hit.ImpactNormal.ContainsNaN() && !Hit.ImpactNormal.IsNearlyZero()
	                        ? Hit.ImpactNormal.GetSafeNormal()
	                        : FVector::UpVector;
	Fact.BoneName = Hit.BoneName;
	LastHitFact = Fact;
	++HitCount;
	CombatHitFactDelegate.Broadcast(Fact);
}

bool UWuwaCombatExecutionComponent::ElevateMeshTickOption()
{
	USkeletalMeshComponent* CharacterMesh = BoundCharacterMesh.Get();
	AActor* Owner = GetOwner();
	if (!IsValid(CharacterMesh) || !IsValid(Owner) || !Owner->HasAuthority())
	{
		return false;
	}

	if (!bMeshTickOptionElevated)
	{
		OriginalMeshTickOption = static_cast<uint8>(CharacterMesh->VisibilityBasedAnimTickOption);
		bMeshTickOptionElevated = true;
	}
	CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	return true;
}

void UWuwaCombatExecutionComponent::RestoreMeshTickOption()
{
	if (!bMeshTickOptionElevated)
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh = BoundCharacterMesh.Get();
	if (IsValid(CharacterMesh))
	{
		CharacterMesh->VisibilityBasedAnimTickOption =
		    static_cast<EVisibilityBasedAnimTickOption>(OriginalMeshTickOption);
	}
	else
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("CombatExecution 无法恢复 Mesh Tick 选项：CharacterMesh 已失效。Owner=%s"),
		       *GetNameSafe(GetOwner()));
	}
	bMeshTickOptionElevated = false;
}

void UWuwaCombatExecutionComponent::ResetActiveWindow()
{
	SetComponentTickEnabled(false);
	RestoreMeshTickOption();
	ActiveWindowHandle.Reset();
	bHasPreviousBladePose = false;
	HitActors.Reset();
}

FWuwaCombatWindowHandle UWuwaCombatExecutionComponent::AllocateWindowHandle()
{
	FWuwaCombatWindowHandle Handle;
	Handle.Value = NextWindowId++;
	if (NextWindowId == 0)
	{
		NextWindowId = 1;
	}
	return Handle;
}

int32 UWuwaCombatExecutionComponent::GetEffectiveMaxTargets() const
{
	return FrozenRequest.bAllowMultipleTargets
	           ? FMath::Clamp(FrozenRequest.MaxTargets, 1, WuwaCombatLimits::MaxMeleeTargets)
	           : 1;
}
