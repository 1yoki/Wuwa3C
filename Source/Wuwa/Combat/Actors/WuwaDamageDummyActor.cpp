// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Actors/WuwaDamageDummyActor.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AbilitySystem/Effects/WuwaCombatGameplayEffects.h"
#include "AbilitySystem/Runtime/WuwaAbilitySystemComponent.h"
#include "Combat/Attributes/WuwaCombatSet.h"
#include "Combat/Attributes/WuwaHealthSet.h"
#include "Combat/Attributes/WuwaPoiseSet.h"
#include "Combat/Contracts/WuwaCombatTypes.h"
#include "Combat/Health/WuwaHealthComponent.h"
#include "Combat/Poise/WuwaPoiseComponent.h"
#include "Combat/WuwaCombatLog.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "UI/WuwaWorldHealthBarComponent.h"

AWuwaDamageDummyActor::AWuwaDamageDummyActor()
{
	bReplicates = true;
	SetReplicateMovement(false);

	CollisionCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCapsule"));
	SetRootComponent(CollisionCapsule);
	CollisionCapsule->InitCapsuleSize(42.f, 96.f);
	CollisionCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionCapsule->SetCollisionObjectType(ECC_Pawn);
	CollisionCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionCapsule->SetCollisionResponseToChannel(WuwaCombatCollision::MeleeTraceChannel, ECR_Block);

	DisplayMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("DisplayMesh"));
	DisplayMesh->SetupAttachment(CollisionCapsule);
	DisplayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DisplayMesh->SetGenerateOverlapEvents(false);

	AbilitySystemComponent = CreateDefaultSubobject<UWuwaAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	HealthSet = CreateDefaultSubobject<UWuwaHealthSet>(TEXT("HealthSet"));
	PoiseSet = CreateDefaultSubobject<UWuwaPoiseSet>(TEXT("PoiseSet"));
	CombatSet = CreateDefaultSubobject<UWuwaCombatSet>(TEXT("CombatSet"));
	HealthComponent = CreateDefaultSubobject<UWuwaHealthComponent>(TEXT("HealthComponent"));
	PoiseComponent = CreateDefaultSubobject<UWuwaPoiseComponent>(TEXT("PoiseComponent"));
	WorldHealthBarComponent = CreateDefaultSubobject<UWuwaWorldHealthBarComponent>(TEXT("WorldHealthBarComponent"));
	WorldHealthBarComponent->SetupAttachment(CollisionCapsule);

	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	DeadEffectClass = UWuwaGameplayEffect_Dead::StaticClass();
}

UAbilitySystemComponent* AWuwaDamageDummyActor::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UWuwaAbilitySystemComponent* AWuwaDamageDummyActor::GetWuwaAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

const UWuwaHealthSet* AWuwaDamageDummyActor::GetHealthSet() const
{
	return HealthSet;
}

const UWuwaPoiseSet* AWuwaDamageDummyActor::GetPoiseSet() const
{
	return PoiseSet;
}

const UWuwaCombatSet* AWuwaDamageDummyActor::GetCombatSet() const
{
	return CombatSet;
}

UWuwaHealthComponent* AWuwaDamageDummyActor::GetHealthComponent() const
{
	return HealthComponent;
}

UWuwaPoiseComponent* AWuwaDamageDummyActor::GetPoiseComponent() const
{
	return PoiseComponent;
}

USkeletalMeshComponent* AWuwaDamageDummyActor::GetDisplayMesh() const
{
	return DisplayMesh;
}

UWuwaWorldHealthBarComponent* AWuwaDamageDummyActor::GetWorldHealthBarComponent() const
{
	return WorldHealthBarComponent;
}

UAnimMontage* AWuwaDamageDummyActor::GetHitReactionMontage() const
{
	return HitReactionMontage;
}

FWuwaCombatTargetResponse
AWuwaDamageDummyActor::EvaluateCombatTarget_Implementation(const FWuwaCombatTargetQuery& Query) const
{
	FWuwaCombatTargetResponse Response;
	if (!Query.WindowHandle.IsValid() || !Query.SourceActor.IsValid() || !Query.InstigatorActor.IsValid() ||
	    !Query.AttackTag.IsValid() || Query.SourceActor.Get() == this || Query.InstigatorActor.Get() == this ||
	    IsActorBeingDestroyed())
	{
		Response.RejectionReason = EWuwaCombatTargetRejectionReason::InvalidQuery;
		return Response;
	}

	if (!bDamageRuntimeReady || bDestroyRequested || !IsValid(HealthComponent) || HealthComponent->IsDeadOrDying())
	{
		Response.RejectionReason = EWuwaCombatTargetRejectionReason::CombatTargetDisabled;
		return Response;
	}

	Response.bCanReceiveCombatHit = true;
	Response.RejectionReason = EWuwaCombatTargetRejectionReason::None;
	return Response;
}

UWuwaAbilitySystemComponent* AWuwaDamageDummyActor::GetDamageReceiverAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UWuwaHealthComponent* AWuwaDamageDummyActor::GetDamageReceiverHealthComponent() const
{
	return HealthComponent;
}

UWuwaPoiseComponent* AWuwaDamageDummyActor::GetDamageReceiverPoiseComponent() const
{
	return PoiseComponent;
}

void AWuwaDamageDummyActor::BeginPlay()
{
	Super::BeginPlay();
	bDamageRuntimeReady = InitializeDamageRuntime();
	if (!bDamageRuntimeReady)
	{
		SetActorEnableCollision(false);
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("伤害假人初始化失败，已关闭受击碰撞。Owner=%s, Authority=%s"),
		       *GetNameSafe(this),
		       HasAuthority() ? TEXT("true") : TEXT("false"));
	}
}

void AWuwaDamageDummyActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bDamageRuntimeReady = false;
	UnbindHealthChangedFactDelegate();
	UnbindDeathFactDelegate();
	if (IsValid(HealthComponent))
	{
		HealthComponent->Shutdown();
	}
	if (IsValid(PoiseComponent))
	{
		PoiseComponent->Shutdown();
	}
	if (IsValid(AbilitySystemComponent) && AbilitySystemComponent->GetAvatarActor() == this)
	{
		AbilitySystemComponent->ClearActorInfo();
	}

	Super::EndPlay(EndPlayReason);
}

bool AWuwaDamageDummyActor::InitializeDamageRuntime()
{
	if (!IsValid(AbilitySystemComponent) || !IsValid(HealthSet) || !IsValid(PoiseSet) || !IsValid(CombatSet) ||
	    !IsValid(HealthComponent) || !IsValid(PoiseComponent) || !IsValid(DeadEffectClass))
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("伤害假人缺少必需默认子对象或死亡效果。Owner=%s, ASC=%s, HealthSet=%s, PoiseSet=%s, CombatSet=%s, "
		            "Health=%s, Poise=%s, DeadEffect=%s"),
		       *GetNameSafe(this),
		       *GetNameSafe(AbilitySystemComponent),
		       *GetNameSafe(HealthSet),
		       *GetNameSafe(PoiseSet),
		       *GetNameSafe(CombatSet),
		       *GetNameSafe(HealthComponent),
		       *GetNameSafe(PoiseComponent),
		       *GetNameSafe(DeadEffectClass.Get()));
		return false;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	const bool bActorInfoValid =
	    AbilitySystemComponent->GetOwnerActor() == this && AbilitySystemComponent->GetAvatarActor() == this &&
	    AbilitySystemComponent->HasAttributeSetForAttribute(UWuwaHealthSet::GetHealthAttribute()) &&
	    AbilitySystemComponent->HasAttributeSetForAttribute(UWuwaPoiseSet::GetPoiseAttribute()) &&
	    AbilitySystemComponent->HasAttributeSetForAttribute(UWuwaCombatSet::GetDefenseAttribute());
	if (!bActorInfoValid)
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("伤害假人 ASC ActorInfo 或 AttributeSet 注册无效。Owner=%s, ASCOwner=%s, Avatar=%s"),
		       *GetNameSafe(this),
		       *GetNameSafe(AbilitySystemComponent->GetOwnerActor()),
		       *GetNameSafe(AbilitySystemComponent->GetAvatarActor()));
		AbilitySystemComponent->ClearActorInfo();
		return false;
	}

	if (HasAuthority() && !InitializeAuthorityAttributes())
	{
		AbilitySystemComponent->ClearActorInfo();
		return false;
	}

	BindDeathFactDelegate();
	if (!DeathFactDelegateHandle.IsValid())
	{
		AbilitySystemComponent->ClearActorInfo();
		return false;
	}

	if (!HealthComponent->Initialize(AbilitySystemComponent, DeadEffectClass))
	{
		UnbindDeathFactDelegate();
		AbilitySystemComponent->ClearActorInfo();
		return false;
	}
	if (!PoiseComponent->Initialize(AbilitySystemComponent))
	{
		HealthComponent->Shutdown();
		UnbindDeathFactDelegate();
		AbilitySystemComponent->ClearActorInfo();
		return false;
	}

	BindHealthChangedFactDelegate();
	if (!HealthChangedFactDelegateHandle.IsValid())
	{
		PoiseComponent->Shutdown();
		HealthComponent->Shutdown();
		UnbindDeathFactDelegate();
		AbilitySystemComponent->ClearActorInfo();
		return false;
	}

	return true;
}

bool AWuwaDamageDummyActor::InitializeAuthorityAttributes()
{
	if (!HasAuthority() || !FMath::IsFinite(InitialMaxHealth) || InitialMaxHealth <= 0.f ||
	    !FMath::IsFinite(InitialMaxPoise) || InitialMaxPoise <= 0.f || !FMath::IsFinite(InitialDefense) ||
	    InitialDefense < 0.f)
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("伤害假人拒绝非法属性初值。Owner=%s, Authority=%s, MaxHealth=%f, MaxPoise=%f, Defense=%f"),
		       *GetNameSafe(this),
		       HasAuthority() ? TEXT("true") : TEXT("false"),
		       InitialMaxHealth,
		       InitialMaxPoise,
		       InitialDefense);
		return false;
	}

	AbilitySystemComponent->SetNumericAttributeBase(UWuwaHealthSet::GetMaxHealthAttribute(), InitialMaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UWuwaHealthSet::GetHealthAttribute(), InitialMaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UWuwaPoiseSet::GetMaxPoiseAttribute(), InitialMaxPoise);
	AbilitySystemComponent->SetNumericAttributeBase(UWuwaPoiseSet::GetPoiseAttribute(), InitialMaxPoise);
	AbilitySystemComponent->SetNumericAttributeBase(UWuwaCombatSet::GetDefenseAttribute(), InitialDefense);

	const bool bAttributesValid = FMath::IsNearlyEqual(HealthSet->GetMaxHealth(), InitialMaxHealth) &&
	                              FMath::IsNearlyEqual(HealthSet->GetHealth(), InitialMaxHealth) &&
	                              FMath::IsNearlyEqual(PoiseSet->GetMaxPoise(), InitialMaxPoise) &&
	                              FMath::IsNearlyEqual(PoiseSet->GetPoise(), InitialMaxPoise) &&
	                              FMath::IsNearlyEqual(CombatSet->GetDefense(), InitialDefense);
	if (!bAttributesValid)
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("伤害假人属性初值未按契约收敛。Owner=%s, Health=%.2f/%.2f, Poise=%.2f/%.2f, Defense=%.2f"),
		       *GetNameSafe(this),
		       HealthSet->GetHealth(),
		       HealthSet->GetMaxHealth(),
		       PoiseSet->GetPoise(),
		       PoiseSet->GetMaxPoise(),
		       CombatSet->GetDefense());
	}
	return bAttributesValid;
}

void AWuwaDamageDummyActor::BindDeathFactDelegate()
{
	UnbindDeathFactDelegate();
	if (!IsValid(HealthComponent))
	{
		UE_LOG(
		    LogWuwaCombat, Error, TEXT("伤害假人无法绑定死亡事实：HealthComponent 无效。Owner=%s"), *GetNameSafe(this));
		return;
	}

	DeathFactDelegateHandle = HealthComponent->OnDeathFact().AddUObject(this, &ThisClass::HandleDeathFact);
	if (!DeathFactDelegateHandle.IsValid())
	{
		UE_LOG(LogWuwaCombat, Error, TEXT("伤害假人无法绑定死亡事实委托。Owner=%s"), *GetNameSafe(this));
	}
}

void AWuwaDamageDummyActor::UnbindDeathFactDelegate()
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

void AWuwaDamageDummyActor::BindHealthChangedFactDelegate()
{
	UnbindHealthChangedFactDelegate();
	if (!IsValid(HealthComponent) || !HealthComponent->IsInitialized())
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("伤害假人无法绑定生命变化事实：HealthComponent 未初始化。Owner=%s"),
		       *GetNameSafe(this));
		return;
	}

	HealthChangedFactDelegateHandle =
	    HealthComponent->OnHealthChanged().AddUObject(this, &ThisClass::HandleHealthChangedFact);
	if (!HealthChangedFactDelegateHandle.IsValid())
	{
		UE_LOG(LogWuwaCombat, Error, TEXT("伤害假人无法绑定生命变化事实委托。Owner=%s"), *GetNameSafe(this));
	}
}

void AWuwaDamageDummyActor::UnbindHealthChangedFactDelegate()
{
	if (!HealthChangedFactDelegateHandle.IsValid())
	{
		return;
	}
	if (IsValid(HealthComponent))
	{
		HealthComponent->OnHealthChanged().Remove(HealthChangedFactDelegateHandle);
	}
	HealthChangedFactDelegateHandle.Reset();
}

void AWuwaDamageDummyActor::HandleHealthChangedFact(const FWuwaHealthChangedFact& Fact)
{
	if (Fact.AffectedActor.Get() != this)
	{
		UE_LOG(LogWuwaCombat,
		       Warning,
		       TEXT("伤害假人拒绝其它 Actor 的生命变化事实。Owner=%s, AffectedActor=%s"),
		       *GetNameSafe(this),
		       *GetNameSafe(Fact.AffectedActor.Get()));
		return;
	}

	if (Fact.bInitialSync || Fact.CurrentHealth >= Fact.PreviousHealth || Fact.CurrentHealth <= 0.f ||
	    bDestroyRequested)
	{
		return;
	}

	PlayHitReactionMontage();
}

void AWuwaDamageDummyActor::PlayHitReactionMontage()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UAnimInstance* AnimInstance = IsValid(DisplayMesh) ? DisplayMesh->GetAnimInstance() : nullptr;
	if (!IsValid(DisplayMesh) || !IsValid(DisplayMesh->GetSkeletalMeshAsset()) || !IsValid(AnimInstance) ||
	    !IsValid(HitReactionMontage))
	{
		if (!bHitReactionWarningEmitted)
		{
			bHitReactionWarningEmitted = true;
			UE_LOG(LogWuwaCombat,
			       Warning,
			       TEXT("伤害假人无法播放受击 Montage：骨骼网格、AnimInstance 或 Montage 未配置。Owner=%s, "
			            "DisplayMesh=%s, SkeletalMesh=%s, AnimInstance=%s, Montage=%s"),
			       *GetNameSafe(this),
			       *GetNameSafe(DisplayMesh),
			       *GetNameSafe(IsValid(DisplayMesh) ? DisplayMesh->GetSkeletalMeshAsset() : nullptr),
			       *GetNameSafe(AnimInstance),
			       *GetNameSafe(HitReactionMontage));
		}
		return;
	}

	const float PlayedDuration =
	    AnimInstance->Montage_Play(HitReactionMontage, 1.f, EMontagePlayReturnType::MontageLength, 0.f, true);
	if (!FMath::IsFinite(PlayedDuration) || PlayedDuration <= 0.f)
	{
		if (!bHitReactionWarningEmitted)
		{
			bHitReactionWarningEmitted = true;
			UE_LOG(
			    LogWuwaCombat,
			    Warning,
			    TEXT("伤害假人受击 Montage 播放失败，请检查 Skeleton 与 Slot。Owner=%s, SkeletalMesh=%s, Montage=%s"),
			    *GetNameSafe(this),
			    *GetNameSafe(DisplayMesh->GetSkeletalMeshAsset()),
			    *GetNameSafe(HitReactionMontage));
		}
		return;
	}

	bHitReactionWarningEmitted = false;
}

void AWuwaDamageDummyActor::HandleDeathFact(const FWuwaDeathFact& Fact)
{
	if (!HasAuthority() || !Fact.bAuthority)
	{
		return;
	}
	if (Fact.DeadActor.Get() != this || Fact.DeathSequence <= 0)
	{
		UE_LOG(LogWuwaCombat,
		       Warning,
		       TEXT("伤害假人拒绝无效死亡事实。Owner=%s, DeadActor=%s, Sequence=%d"),
		       *GetNameSafe(this),
		       *GetNameSafe(Fact.DeadActor.Get()),
		       Fact.DeathSequence);
		return;
	}
	if (bDestroyRequested)
	{
		return;
	}

	bDestroyRequested = true;
	bDamageRuntimeReady = false;
	SetActorEnableCollision(false);
	if (!Destroy())
	{
		UE_LOG(LogWuwaCombat,
		       Error,
		       TEXT("伤害假人死亡后销毁请求失败。Owner=%s, Sequence=%d"),
		       *GetNameSafe(this),
		       Fact.DeathSequence);
	}
}
