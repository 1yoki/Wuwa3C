#include "Camera/Feedback/WuwaCameraFeedbackStack.h"

#include "Curves/CurveFloat.h"

namespace
{
float EvaluateCurve(const UCurveFloat* Curve, const float NormalizedTime)
{
	if (!IsValid(Curve))
	{
		return FMath::Clamp(NormalizedTime, 0.f, 1.f);
	}

	return FMath::Clamp(Curve->GetFloatValue(FMath::Clamp(NormalizedTime, 0.f, 1.f)), 0.f, 1.f);
}
}

FWuwaCameraFeedbackRequestHandle FWuwaCameraFeedbackStack::AcquireFeedback(const FWuwaCameraFeedbackSpec& Spec,
                                                                           UObject* SourceObject)
{
	if (!Spec.IsRuntimeValid() || !IsValid(SourceObject))
	{
		return FWuwaCameraFeedbackRequestHandle();
	}

	FEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.Handle.Value = NextHandleValue++;
	Entry.Spec = Spec;
	Entry.BlendInCurve.Reset(Spec.BlendInCurve);
	Entry.BlendOutCurve.Reset(Spec.BlendOutCurve);
	Entry.SourceObject = SourceObject;
	Entry.Lifecycle = EWuwaCameraFeedbackLifecycle::Active;
	Entry.ElapsedTime = 0.f;
	Entry.ReleaseStartWeight = Spec.BlendInTime <= KINDA_SMALL_NUMBER ? 1.f : 0.f;
	return Entry.Handle;
}

bool FWuwaCameraFeedbackStack::BeginReleaseFeedback(const FWuwaCameraFeedbackRequestHandle& Handle)
{
	for (FEntry& Entry : Entries)
	{
		if (Entry.Handle != Handle || Entry.Lifecycle == EWuwaCameraFeedbackLifecycle::Removed)
		{
			continue;
		}

		if (Entry.Lifecycle == EWuwaCameraFeedbackLifecycle::Active)
		{
			Entry.ReleaseStartWeight = EvaluateWeight(Entry);
			Entry.Lifecycle = Entry.Spec.HoldAfterRelease > KINDA_SMALL_NUMBER
			                      ? EWuwaCameraFeedbackLifecycle::Holding
			                      : EWuwaCameraFeedbackLifecycle::BlendingOut;
			Entry.ElapsedTime = 0.f;
		}
		return true;
	}

	return false;
}

bool FWuwaCameraFeedbackStack::ForceReleaseFeedback(const FWuwaCameraFeedbackRequestHandle& Handle)
{
	const int32 RemovedCount = Entries.RemoveAll(
	    [&Handle](const FEntry& Entry)
	    {
		    return Entry.Handle == Handle;
	    });
	return RemovedCount > 0;
}

int32 FWuwaCameraFeedbackStack::ForceReleaseBySource(const UObject* SourceObject)
{
	if (SourceObject == nullptr)
	{
		return 0;
	}

	return Entries.RemoveAll(
	    [SourceObject](const FEntry& Entry)
	    {
		    return Entry.SourceObject.Get() == SourceObject;
	    });
}

const FWuwaCameraFeedbackOutput& FWuwaCameraFeedbackStack::Update(const float DeltaTime)
{
	if (!FMath::IsFinite(DeltaTime) || DeltaTime < 0.f)
	{
		return Output;
	}

	for (FEntry& Entry : Entries)
	{
		if (!Entry.SourceObject.IsValid())
		{
			Entry.Lifecycle = EWuwaCameraFeedbackLifecycle::Removed;
			continue;
		}

		Entry.ElapsedTime += DeltaTime;
		if (Entry.Lifecycle == EWuwaCameraFeedbackLifecycle::Holding &&
		    Entry.ElapsedTime >= Entry.Spec.HoldAfterRelease)
		{
			Entry.Lifecycle = EWuwaCameraFeedbackLifecycle::BlendingOut;
			Entry.ElapsedTime = FMath::Max(0.f, Entry.ElapsedTime - Entry.Spec.HoldAfterRelease);
		}

		if (Entry.Lifecycle == EWuwaCameraFeedbackLifecycle::BlendingOut &&
		    (Entry.Spec.BlendOutTime <= KINDA_SMALL_NUMBER || Entry.ElapsedTime >= Entry.Spec.BlendOutTime))
		{
			Entry.Lifecycle = EWuwaCameraFeedbackLifecycle::Removed;
		}
	}

	Entries.RemoveAll(
	    [](const FEntry& Entry)
	    {
		    return Entry.Lifecycle == EWuwaCameraFeedbackLifecycle::Removed;
	    });

	Entries.StableSort(
	    [](const FEntry& Left, const FEntry& Right)
	    {
		    return Left.Spec.Priority > Right.Spec.Priority;
	    });

	Output = FWuwaCameraFeedbackOutput();
	for (const FEntry& Entry : Entries)
	{
		const float Weight = EvaluateWeight(Entry);
		Output.FOVOffset += Entry.Spec.FOVOffset * Weight;
		Output.ArmLengthOffset += Entry.Spec.ArmLengthOffset * Weight;
		Output.PivotOffset += Entry.Spec.PivotOffset * Weight;

		const float EffectiveLagMultiplier = FMath::Lerp(1.f, Entry.Spec.LocationLagSpeedMultiplier, Weight);
		Output.LocationLagSpeedMultiplier = FMath::Min(Output.LocationLagSpeedMultiplier, EffectiveLagMultiplier);
		Output.LocationLagAlpha = FMath::Max(Output.LocationLagAlpha, Weight);
		Output.bHasFeedback = true;
	}

	return Output;
}

void FWuwaCameraFeedbackStack::Reset()
{
	Entries.Reset();
	Output = FWuwaCameraFeedbackOutput();
	NextHandleValue = 1;
}

EWuwaCameraFeedbackLifecycle
FWuwaCameraFeedbackStack::GetLifecycle(const FWuwaCameraFeedbackRequestHandle& Handle) const
{
	for (const FEntry& Entry : Entries)
	{
		if (Entry.Handle == Handle)
		{
			return Entry.Lifecycle;
		}
	}

	return EWuwaCameraFeedbackLifecycle::Removed;
}

float FWuwaCameraFeedbackStack::EvaluateWeight(const FEntry& Entry)
{
	switch (Entry.Lifecycle)
	{
		case EWuwaCameraFeedbackLifecycle::Active:
			if (Entry.Spec.BlendInTime <= KINDA_SMALL_NUMBER)
			{
				return 1.f;
			}
			return EvaluateCurve(Entry.BlendInCurve.Get(), Entry.ElapsedTime / Entry.Spec.BlendInTime);

		case EWuwaCameraFeedbackLifecycle::Holding:
			return Entry.ReleaseStartWeight;

		case EWuwaCameraFeedbackLifecycle::BlendingOut:
			if (Entry.Spec.BlendOutTime <= KINDA_SMALL_NUMBER)
			{
				return 0.f;
			}
			return Entry.ReleaseStartWeight *
			       (1.f - EvaluateCurve(Entry.BlendOutCurve.Get(), Entry.ElapsedTime / Entry.Spec.BlendOutTime));

		case EWuwaCameraFeedbackLifecycle::Removed:
		default:
			return 0.f;
	}
}
