// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Input/WuwaAbilityInputRouterComponent.h"

#include "AbilitySystem/Abilities/WuwaGameplayAbility.h"
#include "AbilitySystem/Runtime/WuwaAbilitySystemComponent.h"
#include "AbilitySystem/WuwaAbilitySystemLog.h"
#include "Core/WuwaGameplayTags.h"

UWuwaAbilityInputRouterComponent::UWuwaAbilityInputRouterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

bool UWuwaAbilityInputRouterComponent::Initialize(UWuwaAbilitySystemComponent* InAbilitySystemComponent)
{
	if (!IsValid(InAbilitySystemComponent))
	{
		UE_LOG(
		    LogWuwaAbility, Warning, TEXT("Ability 输入路由初始化失败：ASC 无效。Owner=%s"), *GetNameSafe(GetOwner()));
		return false;
	}

	if (AbilitySystemComponent.Get() == InAbilitySystemComponent)
	{
		return true;
	}

	Shutdown();
	AbilitySystemComponent = InAbilitySystemComponent;
	return true;
}

void UWuwaAbilityInputRouterComponent::Shutdown()
{
	ClearAbilityInput();
	AbilitySystemComponent.Reset();
	LastAcceptedSequence = 0;
	LastAcceptedInputTag = FGameplayTag();
	LastActivationSpecHandle = FGameplayAbilitySpecHandle();
	LastForwardedInputSpecHandle = FGameplayAbilitySpecHandle();
	bLastActivationSucceeded = false;
	LastActivationFailure = EWuwaAbilityActivationDebugFailure::None;
}

void UWuwaAbilityInputRouterComponent::AbilityInputTagPressed(const FGameplayTag InputTag,
                                                              const FWuwaMessageHeader& Header)
{
	if (!AcceptInput(InputTag, Header))
	{
		return;
	}

	TArray<FGameplayAbilitySpecHandle> MatchingHandles;
	GatherMatchingAbilitySpecHandles(InputTag, MatchingHandles);
	for (const FGameplayAbilitySpecHandle Handle : MatchingHandles)
	{
		InputPressedSpecHandles.AddUnique(Handle);
		InputHeldSpecHandles.AddUnique(Handle);
	}
}

void UWuwaAbilityInputRouterComponent::AbilityInputTagReleased(const FGameplayTag InputTag,
                                                               const FWuwaMessageHeader& Header)
{
	if (!AcceptInput(InputTag, Header))
	{
		return;
	}

	TArray<FGameplayAbilitySpecHandle> MatchingHandles;
	GatherMatchingAbilitySpecHandles(InputTag, MatchingHandles);
	for (const FGameplayAbilitySpecHandle Handle : MatchingHandles)
	{
		InputReleasedSpecHandles.AddUnique(Handle);
		InputHeldSpecHandles.Remove(Handle);
	}
}

void UWuwaAbilityInputRouterComponent::ProcessAbilityInput(const float DeltaTime, const bool bGamePaused)
{
	(void)DeltaTime;

	UWuwaAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	const bool bHasPendingInput =
	    !InputPressedSpecHandles.IsEmpty() || !InputReleasedSpecHandles.IsEmpty() || !InputHeldSpecHandles.IsEmpty();
	if (!IsValid(ASC))
	{
		if (bHasPendingInput)
		{
			RecordActivationResult(
			    FGameplayAbilitySpecHandle(), false, EWuwaAbilityActivationDebugFailure::NotInitialized);
		}
		ClearAbilityInput();
		return;
	}
	if (bGamePaused)
	{
		ClearAbilityInput();
		return;
	}
	if (ASC->HasMatchingGameplayTag(WuwaGameplayTags::State_Combat_Dead))
	{
		if (bHasPendingInput)
		{
			const FGameplayAbilitySpecHandle Handle =
			    !InputPressedSpecHandles.IsEmpty() ? InputPressedSpecHandles[0] : FGameplayAbilitySpecHandle();
			RecordActivationResult(Handle, false, EWuwaAbilityActivationDebugFailure::DeadState);
		}
		ClearAbilityInput();
		return;
	}
	if (ASC->HasMatchingGameplayTag(WuwaGameplayTags::State_Combat_Staggered))
	{
		if (bHasPendingInput)
		{
			const FGameplayAbilitySpecHandle Handle =
			    !InputPressedSpecHandles.IsEmpty() ? InputPressedSpecHandles[0] : FGameplayAbilitySpecHandle();
			RecordActivationResult(Handle, false, EWuwaAbilityActivationDebugFailure::StaggeredState);
		}
		ClearAbilityInput();
		return;
	}

	for (const FGameplayAbilitySpecHandle Handle : InputPressedSpecHandles)
	{
		FGameplayAbilitySpec* AbilitySpec = ASC->FindAbilitySpecFromHandle(Handle);
		if (AbilitySpec == nullptr || !IsValid(AbilitySpec->Ability))
		{
			RecordActivationResult(Handle, false, EWuwaAbilityActivationDebugFailure::InvalidSpec);
			continue;
		}

		AbilitySpec->InputPressed = true;
		if (AbilitySpec->IsActive())
		{
			ASC->AbilitySpecInputPressed(*AbilitySpec);
			RecordActiveInputForwarded(Handle);
			continue;
		}

		const UWuwaGameplayAbility* WuwaAbility = Cast<UWuwaGameplayAbility>(AbilitySpec->Ability);
		if (IsValid(WuwaAbility) &&
		    WuwaAbility->GetActivationPolicy() == EWuwaAbilityActivationPolicy::OnInputTriggered)
		{
			++ActivationAttemptCount;
			const bool bActivated = ASC->TryActivateAbility(Handle);
			if (bActivated)
			{
				++ActivationSuccessCount;
			}
			RecordActivationResult(Handle,
			                       bActivated,
			                       bActivated ? EWuwaAbilityActivationDebugFailure::None
			                                  : EWuwaAbilityActivationDebugFailure::TryActivateAbilityRejected);
		}
	}

	for (const FGameplayAbilitySpecHandle Handle : InputReleasedSpecHandles)
	{
		FGameplayAbilitySpec* AbilitySpec = ASC->FindAbilitySpecFromHandle(Handle);
		if (AbilitySpec == nullptr)
		{
			continue;
		}

		AbilitySpec->InputPressed = false;
		if (AbilitySpec->IsActive())
		{
			ASC->AbilitySpecInputReleased(*AbilitySpec);
		}
	}

	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
}

void UWuwaAbilityInputRouterComponent::ClearAbilityInput()
{
	UWuwaAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (IsValid(ASC))
	{
		for (const FGameplayAbilitySpecHandle Handle : InputHeldSpecHandles)
		{
			FGameplayAbilitySpec* AbilitySpec = ASC->FindAbilitySpecFromHandle(Handle);
			if (AbilitySpec == nullptr)
			{
				continue;
			}

			AbilitySpec->InputPressed = false;
			if (AbilitySpec->IsActive())
			{
				ASC->AbilitySpecInputReleased(*AbilitySpec);
			}
		}
	}

	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputHeldSpecHandles.Reset();
}

FWuwaAbilityInputRouterRuntimeSnapshot UWuwaAbilityInputRouterComponent::GetRuntimeSnapshot() const
{
	FWuwaAbilityInputRouterRuntimeSnapshot Snapshot;
	Snapshot.bInitialized = AbilitySystemComponent.IsValid();
	Snapshot.AbilitySystemComponentName = GetNameSafe(AbilitySystemComponent.Get());
	Snapshot.LastAcceptedSequence = LastAcceptedSequence;
	Snapshot.LastAcceptedInputTag = LastAcceptedInputTag;
	Snapshot.LastActivationSpecHandle = LastActivationSpecHandle;
	Snapshot.LastForwardedInputSpecHandle = LastForwardedInputSpecHandle;
	Snapshot.bLastActivationSucceeded = bLastActivationSucceeded;
	Snapshot.LastActivationFailure = LastActivationFailure;
	Snapshot.PressedSpecCount = InputPressedSpecHandles.Num();
	Snapshot.ReleasedSpecCount = InputReleasedSpecHandles.Num();
	Snapshot.HeldSpecCount = InputHeldSpecHandles.Num();
	Snapshot.ActivationAttemptCount = ActivationAttemptCount;
	Snapshot.ActivationSuccessCount = ActivationSuccessCount;
	Snapshot.ActiveInputForwardCount = ActiveInputForwardCount;
	return Snapshot;
}

bool UWuwaAbilityInputRouterComponent::IsInitialized() const
{
	return AbilitySystemComponent.IsValid();
}

void UWuwaAbilityInputRouterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Shutdown();
	Super::EndPlay(EndPlayReason);
}

void UWuwaAbilityInputRouterComponent::GatherMatchingAbilitySpecHandles(
    const FGameplayTag& InputTag, TArray<FGameplayAbilitySpecHandle>& OutHandles) const
{
	OutHandles.Reset();
	const UWuwaAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!IsValid(ASC))
	{
		return;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : ASC->GetActivatableAbilities())
	{
		if (AbilitySpec.Ability != nullptr && AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			OutHandles.Add(AbilitySpec.Handle);
		}
	}
}

bool UWuwaAbilityInputRouterComponent::AcceptInput(const FGameplayTag& InputTag, const FWuwaMessageHeader& Header)
{
	if (!AbilitySystemComponent.IsValid() || !InputTag.IsValid() || !Header.IsValid())
	{
		return false;
	}

	if (Header.Sequence <= LastAcceptedSequence)
	{
		UE_LOG(LogWuwaAbility,
		       Warning,
		       TEXT("Ability 输入序号重复或倒退。Owner=%s, Input=%s, Sequence=%d, Last=%d"),
		       *GetNameSafe(GetOwner()),
		       *InputTag.ToString(),
		       Header.Sequence,
		       LastAcceptedSequence);
		return false;
	}

	LastAcceptedSequence = Header.Sequence;
	LastAcceptedInputTag = InputTag;
	return true;
}

void UWuwaAbilityInputRouterComponent::RecordActivationResult(const FGameplayAbilitySpecHandle Handle,
                                                              const bool bSucceeded,
                                                              const EWuwaAbilityActivationDebugFailure Failure)
{
	LastActivationSpecHandle = Handle;
	bLastActivationSucceeded = bSucceeded;
	LastActivationFailure = Failure;
}

void UWuwaAbilityInputRouterComponent::RecordActiveInputForwarded(const FGameplayAbilitySpecHandle Handle)
{
	LastForwardedInputSpecHandle = Handle;
	++ActiveInputForwardCount;
}
