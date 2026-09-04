#include "Traversal/Data/WuwaGrappleActionDefinition.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/Notifies/WuwaAnimNotify_ActionEvent.h"
#include "Core/WuwaGameplayTags.h"
#include "Curves/CurveFloat.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
bool IsFiniteNormalizedCurve(const UCurveFloat* Curve, const bool bRequireMonotonic)
{
	if (!IsValid(Curve) || Curve->FloatCurve.IsEmpty())
	{
		return false;
	}

	float MinimumTime = 0.f;
	float MaximumTime = 0.f;
	Curve->FloatCurve.GetTimeRange(MinimumTime, MaximumTime);
	if (!FMath::IsNearlyEqual(MinimumTime, 0.f) || !FMath::IsNearlyEqual(MaximumTime, 1.f))
	{
		return false;
	}

	float PreviousValue = Curve->GetFloatValue(0.f);
	if (!FMath::IsFinite(PreviousValue) || !FMath::IsNearlyZero(PreviousValue, 0.001f))
	{
		return false;
	}

	constexpr int32 ValidationSampleCount = 32;
	for (int32 SampleIndex = 1; SampleIndex <= ValidationSampleCount; ++SampleIndex)
	{
		const float Time = static_cast<float>(SampleIndex) / static_cast<float>(ValidationSampleCount);
		const float Value = Curve->GetFloatValue(Time);
		if (!FMath::IsFinite(Value) || (bRequireMonotonic && Value + 0.001f < PreviousValue))
		{
			return false;
		}
		PreviousValue = Value;
	}

	return FMath::IsNearlyEqual(PreviousValue, 1.f, 0.001f);
}

bool HasCompletionRule(const UWuwaGrappleActionDefinition& Definition,
                       const FGameplayTag& EventTag,
                       const EWuwaActionEndReason EndReason)
{
	EWuwaActionEndReason ResolvedReason = EWuwaActionEndReason::None;
	return Definition.ResolveCompletionRule(EventTag, ResolvedReason) && ResolvedReason == EndReason;
}

bool HasVisualNotify(const UAnimMontage* Montage, const FGameplayTag& EventTag)
{
	if (!IsValid(Montage))
	{
		return false;
	}

	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		const UWuwaAnimNotify_ActionEvent* Notify = Cast<UWuwaAnimNotify_ActionEvent>(NotifyEvent.Notify);
		if (IsValid(Notify) && Notify->EventTag == EventTag)
		{
			return true;
		}
	}

	return false;
}
}

void UWuwaGrappleActionDefinition::GatherRequiredCapabilityTags(FGameplayTagContainer& OutCapabilityTags) const
{
	OutCapabilityTags.Reset();
	OutCapabilityTags.AddTag(WuwaGameplayTags::Action_Capability_Traversal_Grapple);

	if (AnimationSpec.Montage != nullptr)
	{
		OutCapabilityTags.AddTag(WuwaGameplayTags::Action_Capability_Animation);
	}
}

const FWuwaActionAnimationSpec* UWuwaGrappleActionDefinition::GetAnimationSpec() const
{
	return AnimationSpec.Montage != nullptr ? &AnimationSpec : nullptr;
}

float UWuwaGrappleActionDefinition::GetDesiredAnimationDuration() const
{
	return AnimationSpec.bMatchActionDuration ? MovementSpec.WindupDuration + MovementSpec.PullDuration : 0.f;
}

bool UWuwaGrappleActionDefinition::IsContextValid(const FWuwaActionContext& Context) const
{
	if (!Super::IsContextValid(Context))
	{
		return false;
	}

	const FWuwaGrappleActionContext* GrappleContext = Context.DomainPayload.GetPtr<FWuwaGrappleActionContext>();
	return GrappleContext != nullptr && GrappleContext->IsRuntimeValid() &&
	       FMath::IsNearlyEqual(GrappleContext->ForwardTravelDistance, MovementSpec.ForwardTravelDistance) &&
	       FMath::IsNearlyEqual(GrappleContext->VerticalTravelHeight, MovementSpec.VerticalTravelHeight) &&
	       GrappleContext->TrajectoryScale >= QuerySpec.MinimumTrajectoryScale &&
	       FVector::DistSquared(GrappleContext->QueryStartLocation, GrappleContext->VisualAnchorLocation) >
	           UE_SMALL_NUMBER;
}

bool UWuwaGrappleActionDefinition::IsRuntimeValid() const
{
	if (!Super::IsRuntimeValid() || !ActionTag.MatchesTag(WuwaGameplayTags::Action_Traversal_Grapple) ||
	    !GrantedTags.HasTagExact(WuwaGameplayTags::State_Traversal_Grappling) || !QuerySpec.IsRuntimeValid() ||
	    !MovementSpec.IsRuntimeValid() || !IsFiniteNormalizedCurve(MovementSpec.ForwardDistanceCurve, true) ||
	    !IsFiniteNormalizedCurve(MovementSpec.VerticalDistanceCurve, false) || !CameraFeedbackSpec.IsRuntimeValid() ||
	    !PresentationSpec.IsRuntimeValid())
	{
		return false;
	}

	if (QuerySpec.VisualAnchorForwardDistance <= MovementSpec.ForwardTravelDistance ||
	    QuerySpec.MinimumForwardTravelDistance > MovementSpec.ForwardTravelDistance)
	{
		return false;
	}

	if (AnimationSpec.Montage != nullptr &&
	    (AnimationSpec.LifetimePolicy != EWuwaActionAnimationLifetimePolicy::ActionControlled ||
	     !AnimationSpec.IsRuntimeValid()))
	{
		return false;
	}

	if (PresentationSpec.IsConfigured() &&
	    (!IsValid(AnimationSpec.Montage) ||
	     !HasVisualNotify(AnimationSpec.Montage, WuwaGameplayTags::Action_Event_Traversal_Grapple_Visual_Attach) ||
	     !HasVisualNotify(AnimationSpec.Montage, WuwaGameplayTags::Action_Event_Traversal_Grapple_Visual_Detach)))
	{
		return false;
	}

	return HasCompletionRule(
	           *this, WuwaGameplayTags::Action_Event_Traversal_Grapple_Released, EWuwaActionEndReason::Completed) &&
	       HasCompletionRule(
	           *this, WuwaGameplayTags::Action_Event_Traversal_Grapple_Landed, EWuwaActionEndReason::Completed) &&
	       HasCompletionRule(
	           *this, WuwaGameplayTags::Action_Event_Traversal_Grapple_Blocked, EWuwaActionEndReason::Interrupted) &&
	       HasCompletionRule(
	           *this, WuwaGameplayTags::Action_Event_Traversal_Grapple_TimedOut, EWuwaActionEndReason::Failed) &&
	       HasCompletionRule(
	           *this, WuwaGameplayTags::Action_Event_Animation_Interrupted, EWuwaActionEndReason::Interrupted);
}

#if WITH_EDITOR

EDataValidationResult UWuwaGrappleActionDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (IsValid(AnimationSpec.Montage) &&
	    AnimationSpec.LifetimePolicy != EWuwaActionAnimationLifetimePolicy::ActionControlled)
	{
		Context.AddError(FText::FromString(TEXT("Grapple Montage 必须使用 ActionControlled 生命周期策略")));
		Result = EDataValidationResult::Invalid;
	}

	if (IsValid(AnimationSpec.Montage) && AnimationSpec.Montage->bEnableAutoBlendOut)
	{
		Context.AddError(FText::FromString(TEXT("ActionControlled Grapple Montage 必须关闭 Enable Auto Blend Out")));
		Result = EDataValidationResult::Invalid;
	}

	if (!IsRuntimeValid())
	{
		Context.AddError(
		    FText::FromString(TEXT("Grapple Action Definition 的标签、查询、轨迹、完成规则或表现配置无效")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}

#endif
