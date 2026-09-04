// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Data/WuwaMeleeAttackDefinition.h"

// #include "ToolMenusEditor.h"
#include "Animation/AnimMontage.h"
#include "Combat/Animation/WuwaAnimNotifyState_ComboWindow.h"
#include "Combat/Animation/WuwaAnimNotifyState_CombatHitWindow.h"
#include "Core/WuwaGameplayTags.h"
#include "Misc/DataValidation.h"

/** @return 指定索引要求的固定 Section 名称 */
FName GetRequiredSectionName(const int32 StepIndex)
{
	static const FName SectionNames[UWuwaMeleeAttackDefinition::RequiredStepCount] = {
	    TEXT("Attack_1"),
	    TEXT("Attack_2"),
	    TEXT("Attack_3"),
	};
	return StepIndex >= 0 && StepIndex < UWuwaMeleeAttackDefinition::RequiredStepCount ? SectionNames[StepIndex]
	                                                                                   : NAME_None;
}

/** @return 单段数值与语义配置是否有效 */
bool IsStepValid(const FWuwaMeleeAttackStep& Step, const int32 StepIndex)
{
	return Step.MontageSection == GetRequiredSectionName(StepIndex) && FMath::IsFinite(Step.BaseDamage) &&
	       Step.BaseDamage >= 0.0f && FMath::IsFinite(Step.PoiseDamage) && Step.PoiseDamage >= 0.0f &&
	       Step.HitWindowCount >= 1 && Step.HitWindowCount <= 8 && FMath::IsFinite(Step.TraceSpec.TraceRadius) &&
	       Step.TraceSpec.TraceRadius > 0.0f && Step.TraceSpec.BladeSampleCount >= 2 &&
	       Step.TraceSpec.BladeSampleCount <= 16 && Step.MaxTargets >= 1 &&
	       Step.MaxTargets <= WuwaCombatLimits::MaxMeleeTargets &&
	       Step.SwingCueTag == WuwaGameplayTags::GameplayCue_Combat_Sword_Swing &&
	       Step.HitCueTag == WuwaGameplayTags::GameplayCue_Combat_Sword_Hit;
}

/** @return Notify 是否完整落在指定 Section 内 */
bool IsNotifyInsideSection(const UAnimMontage& Montage, const FAnimNotifyEvent& NotifyEvent, const int32 StepIndex)
{
	float SectionStart = 0.0f;
	float SectionEnd = 0.0f;
	Montage.GetSectionStartAndEndTime(StepIndex, SectionStart, SectionEnd);
	return NotifyEvent.GetDuration() > 0.0f && NotifyEvent.GetTriggerTime() >= SectionStart - KINDA_SMALL_NUMBER &&
	       NotifyEvent.GetEndTriggerTime() <= SectionEnd + KINDA_SMALL_NUMBER &&
	       NotifyEvent.GetTriggerTime() < NotifyEvent.GetEndTriggerTime();
}

/**
 * 验证 Montage 中所有动画 Segment 与 Root Motion 策略是否一致
 */
bool IsRootMotionPolicyValid(const UAnimMontage& Montage,
                             const TArray<FAnimSegment>& Segments,
                             const EWuwaAttackRootMotionPolicy RootMotionPolicy)
{
	if (Segments.IsEmpty())
	{
		return false;
	}

	bool bAnySourceHasRootMotion = false;

	for (const FAnimSegment& Segment : Segments)
	{
		const UAnimSequenceBase* SourceSequence = Segment.GetAnimReference();

		if (!IsValid(SourceSequence) || SourceSequence->GetSkeleton() != Montage.GetSkeleton())
		{
			return false;
		}

		bAnySourceHasRootMotion |= SourceSequence->HasRootMotion();
	}

	switch (RootMotionPolicy)
	{
		case EWuwaAttackRootMotionPolicy::InPlace:
			// InPlace：Montage 中所有动画都不能提供 Root Motion
			return !Montage.HasRootMotion() && !bAnySourceHasRootMotion;

		case EWuwaAttackRootMotionPolicy::MontageDriven:
			// MontageDriven：Montage 至少要包含 Root Motion
			return Montage.HasRootMotion() && bAnySourceHasRootMotion;

		case EWuwaAttackRootMotionPolicy::Unspecified:
		default:
			return false;
	}
}

/** @return Montage 的 Section、Segment、Notify 与 Root Motion 契约是否有效 */
bool IsMontageContractValid(const UAnimMontage& Montage,
                            const EWuwaAttackRootMotionPolicy RootMotionPolicy,
                            const TArray<FWuwaMeleeAttackStep>& Steps)
{
	if (Montage.GetNumSections() != UWuwaMeleeAttackDefinition::RequiredStepCount ||
	    Montage.CompositeSections.Num() != UWuwaMeleeAttackDefinition::RequiredStepCount ||
	    Montage.SlotAnimTracks.Num() != 1 || Montage.SlotAnimTracks[0].SlotName != FName(TEXT("DefaultSlot")))
	{
		return false;
	}

	const TArray<FAnimSegment>& Segments = Montage.SlotAnimTracks[0].AnimTrack.AnimSegments;

	// 不再要求 Segment == 3。
	// 只要求至少存在一个动画 Segment。
	if (Segments.IsEmpty())
	{
		return false;
	}

	if (!IsRootMotionPolicyValid(Montage, Segments, RootMotionPolicy))
	{
		return false;
	}

	float SectionStarts[UWuwaMeleeAttackDefinition::RequiredStepCount] = {};
	float SectionEnds[UWuwaMeleeAttackDefinition::RequiredStepCount] = {};

	for (int32 StepIndex = 0; StepIndex < UWuwaMeleeAttackDefinition::RequiredStepCount; ++StepIndex)
	{
		const FCompositeSection& Section = Montage.CompositeSections[StepIndex];

		if (Montage.GetSectionName(StepIndex) != GetRequiredSectionName(StepIndex))
		{
			return false;
		}

		// 连招跳转由 Ability 运行时调用
		// CurrentMontageSetNextSectionName 控制，
		// 因此资产本身保持 None。
		if (!Section.NextSectionName.IsNone())
		{
			return false;
		}

		Montage.GetSectionStartAndEndTime(StepIndex, SectionStarts[StepIndex], SectionEnds[StepIndex]);

		if (!FMath::IsFinite(SectionStarts[StepIndex]) || !FMath::IsFinite(SectionEnds[StepIndex]) ||
		    SectionStarts[StepIndex] < 0.0f || SectionEnds[StepIndex] <= SectionStarts[StepIndex])
		{
			return false;
		}
	}

	int32 SegmentCounts[UWuwaMeleeAttackDefinition::RequiredStepCount] = {0, 0, 0};

	for (const FAnimSegment& Segment : Segments)
	{
		const UAnimSequenceBase* SourceSequence = Segment.GetAnimReference();

		if (!IsValid(SourceSequence))
		{
			return false;
		}

		// 所有动画必须属于 Montage Skeleton
		if (SourceSequence->GetSkeleton() != Montage.GetSkeleton())
		{
			return false;
		}

		// Segment 自身必须有效
		if (!FMath::IsFinite(Segment.StartPos) || !FMath::IsFinite(Segment.AnimPlayRate) ||
		    Segment.AnimPlayRate <= 0.0f || Segment.LoopingCount < 1)
		{
			return false;
		}

		const float SegmentStart = Segment.StartPos;
		const float SegmentEnd = Segment.GetEndPos();
		const float SegmentLength = Segment.GetLength();

		if (!FMath::IsFinite(SegmentStart) || !FMath::IsFinite(SegmentEnd) || !FMath::IsFinite(SegmentLength) ||
		    SegmentLength <= 0.0f || SegmentEnd <= SegmentStart)
		{
			return false;
		}

		bool bFoundOwningSection = false;

		for (int32 StepIndex = 0; StepIndex < UWuwaMeleeAttackDefinition::RequiredStepCount; ++StepIndex)
		{
			const float SectionStart = SectionStarts[StepIndex];
			const float SectionEnd = SectionEnds[StepIndex];

			const bool bInsideSection =
			    SegmentStart >= SectionStart - KINDA_SMALL_NUMBER && SegmentEnd <= SectionEnd + KINDA_SMALL_NUMBER;

			if (bInsideSection)
			{
				++SegmentCounts[StepIndex];
				bFoundOwningSection = true;
				break;
			}
		}

		// 不允许一个动画 Segment 横跨两个 Attack Section。
		if (!bFoundOwningSection)
		{
			return false;
		}
	}

	// 三个 Section 每个至少必须有一个动画 Segment
	for (int32 StepIndex = 0; StepIndex < UWuwaMeleeAttackDefinition::RequiredStepCount; ++StepIndex)
	{
		if (SegmentCounts[StepIndex] <= 0)
		{
			return false;
		}
	}

	// ------------------------------------------------------------
	// 5. HitWindow / ComboWindow
	// ------------------------------------------------------------

	int32 HitWindowCounts[UWuwaMeleeAttackDefinition::RequiredStepCount] = {0, 0, 0};

	int32 ComboWindowCounts[UWuwaMeleeAttackDefinition::RequiredStepCount] = {0, 0, 0};

	for (const FAnimNotifyEvent& NotifyEvent : Montage.Notifies)
	{
		if (const UWuwaAnimNotifyState_CombatHitWindow* HitWindow =
		        Cast<UWuwaAnimNotifyState_CombatHitWindow>(NotifyEvent.NotifyStateClass))
		{
			const int32 StepIndex = HitWindow->StepIndex;

			if (StepIndex < 0 || StepIndex >= UWuwaMeleeAttackDefinition::RequiredStepCount ||
			    !IsNotifyInsideSection(Montage, NotifyEvent, StepIndex))
			{
				return false;
			}

			++HitWindowCounts[StepIndex];
		}
		else if (const UWuwaAnimNotifyState_ComboWindow* ComboWindow =
		             Cast<UWuwaAnimNotifyState_ComboWindow>(NotifyEvent.NotifyStateClass))
		{
			const int32 StepIndex = ComboWindow->StepIndex;

			if (StepIndex < 0 || StepIndex >= UWuwaMeleeAttackDefinition::RequiredStepCount - 1 ||
			    !IsNotifyInsideSection(Montage, NotifyEvent, StepIndex))
			{
				return false;
			}

			++ComboWindowCounts[StepIndex];
		}
	}

	for (int32 StepIndex = 0; StepIndex < UWuwaMeleeAttackDefinition::RequiredStepCount; ++StepIndex)
	{
		// Montage 里的 HitWindow 数量必须与 AttackDefinition 配置一致
		if (HitWindowCounts[StepIndex] != Steps[StepIndex].HitWindowCount)
		{
			return false;
		}
	}

	return ComboWindowCounts[0] == 1 && ComboWindowCounts[1] == 1 && ComboWindowCounts[2] == 0;
}

bool UWuwaMeleeAttackDefinition::IsRuntimeValid() const
{
	return AttackTag == WuwaGameplayTags::Ability_Combat_Attack_Light && IsValid(Montage) &&
	       FMath::IsFinite(PlayRate) && PlayRate > 0.0f && FacingPolicy == EWuwaAttackFacingPolicy::PreserveFacing &&
	       Steps.Num() == RequiredStepCount && IsStepValid(Steps[0], 0) && IsStepValid(Steps[1], 1) &&
	       IsStepValid(Steps[2], 2) && IsMontageContractValid(*Montage, RootMotionPolicy, Steps);
}

#if WITH_EDITOR

EDataValidationResult UWuwaMeleeAttackDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	bool bRootMotionPolicyError = false;
	if (RootMotionPolicy == EWuwaAttackRootMotionPolicy::Unspecified)
	{
		Context.AddError(
		    FText::FromString(TEXT("MeleeAttackDefinition 必须明确选择 InPlace 或 MontageDriven Root Motion 策略")));
		bRootMotionPolicyError = true;
	}
	else if (IsValid(Montage) && Montage->SlotAnimTracks.Num() == 1 &&
	         Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() > 0)
	{
		const TArray<FAnimSegment>& Segments = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments;

		if (!IsRootMotionPolicyValid(*Montage, Segments, RootMotionPolicy))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("MeleeAttackDefinition 的 Root Motion "
			                                                        "策略与 Montage 中的动画序列不一致。"
			                                                        "Policy=%d, MontageHasRootMotion=%d"),
			                                                   static_cast<int32>(RootMotionPolicy),
			                                                   Montage->HasRootMotion() ? 1 : 0)));

			bRootMotionPolicyError = true;
		}
	}
	if (!IsRuntimeValid())
	{
		if (!bRootMotionPolicyError)
		{
			Context.AddError(FText::FromString(
			    TEXT("MeleeAttackDefinition 的严格三段数据、Montage、Notify、Slot 或 Skeleton 配置无效")));
		}
		Result = EDataValidationResult::Invalid;
	}

	return Result == EDataValidationResult::Invalid ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}

#endif
