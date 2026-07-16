#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "WuwaStateTagTypes.generated.h"

/*
 * 标识某个 Gameplay 状态来源取得的一次标签引用。
 * Handle 只负责标识所有权，实际标签与引用计数仍由 StateTagComponent 持有。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaStateTagHandle
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State Tags")
    FGuid Id;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State Tags")
    FGameplayTag Tag;

    FWuwaStateTagHandle() = default;

    FWuwaStateTagHandle(const FGuid &InId, const FGameplayTag &InTag)
        : Id(InId), Tag(InTag)
    {
    }

    bool IsValid() const
    {
        return Id.IsValid() && Tag.IsValid();
    }

    void Reset()
    {
        Id.Invalidate();
        Tag = FGameplayTag();
    }
};
