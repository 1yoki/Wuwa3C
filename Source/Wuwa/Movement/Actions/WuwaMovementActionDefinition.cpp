#include "Movement/Actions/WuwaMovementActionDefinition.h"

#include "Core/WuwaGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
/**
 * 检查 Movement Action 是否显式声明指定的动画完成规则
 * @param Definition	待检查的 Movement Action Definition
 * @param EventTag	动画事件标签
 * @param EndReason	期望的结束原因
 * @return 规则存在且映射一致时返回 true
 */
bool HasAnimationCompletionRule(const UWuwaMovementActionDefinition& Definition,
                                const FGameplayTag& EventTag,
                                const EWuwaActionEndReason EndReason)
{
	EWuwaActionEndReason ResolvedReason = EWuwaActionEndReason::None;
	return Definition.ResolveCompletionRule(EventTag, ResolvedReason) && ResolvedReason == EndReason;
}
}

void UWuwaMovementActionDefinition::GatherRequiredCapabilityTags(FGameplayTagContainer& OutCapabilityTags) const
{
	OutCapabilityTags.Reset();
	OutCapabilityTags.AddTag(WuwaGameplayTags::Action_Capability_Movement);

	if (AnimationSpec.Montage != nullptr)
	{
		OutCapabilityTags.AddTag(WuwaGameplayTags::Action_Capability_Animation);
	}
}

const FWuwaActionAnimationSpec* UWuwaMovementActionDefinition::GetAnimationSpec() const
{
	return AnimationSpec.Montage != nullptr ? &AnimationSpec : nullptr;
}

float UWuwaMovementActionDefinition::GetDesiredAnimationDuration() const
{
	if (AnimationSpec.bMatchActionDuration && MovementDriver == EWuwaMovementActionDriver::RootMotionSource)
	{
		return RootMotionSourceConfig.Duration;
	}

	return 0.f;
}

bool UWuwaMovementActionDefinition::IsRuntimeValid() const
{
	if (!Super::IsRuntimeValid() || !AnimationSpec.IsRuntimeValid() ||
	    !HasAnimationCompletionRule(
	        *this, WuwaGameplayTags::Action_Event_Animation_Completed, EWuwaActionEndReason::Completed) ||
	    !HasAnimationCompletionRule(
	        *this, WuwaGameplayTags::Action_Event_Animation_Interrupted, EWuwaActionEndReason::Interrupted))
	{
		return false;
	}

	if (MovementDriver == EWuwaMovementActionDriver::RootMotionSource && !RootMotionSourceConfig.IsRuntimeValid())
	{
		return false;
	}

	FGameplayTagContainer SeenEvents;

	for (const FWuwaMovementActionEventBinding& Binding : EventBindings)
	{
		if (!Binding.EventTag.IsValid() || SeenEvents.HasTagExact(Binding.EventTag))
		{
			return false;
		}

		SeenEvents.AddTag(Binding.EventTag);
	}

	return true;
}

#if WITH_EDITOR

EDataValidationResult UWuwaMovementActionDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!IsRuntimeValid())
	{
		Context.AddError(FText::FromString(TEXT("Movement Action Definition 的驱动、动画或事件配置无效")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}

#endif
