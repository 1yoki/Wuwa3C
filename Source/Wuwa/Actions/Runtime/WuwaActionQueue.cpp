#include "Actions/Runtime/WuwaActionQueue.h"

bool FWuwaActionQueue::Enqueue(const FWuwaResolvedActionIntent& Intent,
                               const int32 MaxDepth,
                               EWuwaActionRejectionReason& OutReason)
{
	OutReason = EWuwaActionRejectionReason::None;

	if (!Intent.IsValid() || MaxDepth <= 0)
	{
		OutReason = EWuwaActionRejectionReason::InvalidContext;
		return false;
	}

	const TWeakObjectPtr<UObject> SourceObject = Intent.Request.Header.SourceObject;
	const int32* LastAcceptedSequence =
	    SourceObject.IsValid() ? LastAcceptedSequenceBySource.Find(SourceObject) : &LastAnonymousAcceptedSequence;
	if (LastAcceptedSequence != nullptr && Intent.Request.Header.Sequence <= *LastAcceptedSequence)
	{
		OutReason = EWuwaActionRejectionReason::DuplicateRequest;
		return false;
	}

	if (Items.Num() >= MaxDepth)
	{
		OutReason = EWuwaActionRejectionReason::QueueFull;
		return false;
	}

	Items.Add(Intent);
	if (SourceObject.IsValid())
	{
		LastAcceptedSequenceBySource.FindOrAdd(SourceObject) = Intent.Request.Header.Sequence;
	}
	else
	{
		LastAnonymousAcceptedSequence = Intent.Request.Header.Sequence;
	}
	return true;
}

int32 FWuwaActionQueue::Expire(const double CurrentTime, TArray<FWuwaResolvedActionIntent>& OutExpired)
{
	OutExpired.Reset();
	return Items.RemoveAll(
	    [CurrentTime, &OutExpired](const FWuwaResolvedActionIntent& Intent)
	    {
		    if (!Intent.IsExpired(CurrentTime))
		    {
			    return false;
		    }
		    OutExpired.Add(Intent);
		    return true;
	    });
}

const FWuwaResolvedActionIntent* FWuwaActionQueue::Peek() const
{
	return Items.IsEmpty() ? nullptr : &Items[0];
}

bool FWuwaActionQueue::PopFront(FWuwaResolvedActionIntent& OutIntent)
{
	if (Items.IsEmpty())
	{
		return false;
	}

	OutIntent = MoveTemp(Items[0]);
	Items.RemoveAt(0);
	return true;
}

void FWuwaActionQueue::Reset(TArray<FWuwaResolvedActionIntent>& OutRemoved)
{
	OutRemoved = MoveTemp(Items);
	Items.Reset();
	LastAcceptedSequenceBySource.Reset();
	LastAnonymousAcceptedSequence = 0;
}
