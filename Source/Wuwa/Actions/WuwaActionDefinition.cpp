#include "Actions/WuwaActionDefinition.h"

#include "Animation/AnimMontage.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UWuwaActionDefinition::IsRuntimeValid() const
{
    if (!ActionTag.IsValid() || Priority < 0 || BufferTime < 0.f)
    {
        return false;
    }

    // 同一状态不能同时作为必需和阻塞标签
    if (RequiredTags.HasAny(BlockedTags))
    {
        return false;
    }

    const bool bNeedsMontage = MovementPolicy == EWuwaActionMovementPolicy::RootMotionMontage ||
                               MovementPolicy == EWuwaActionMovementPolicy::MotionWarpedRootMotion;

    // Montage 位移策略必须提供有效动画
    return !bNeedsMontage || Montage != nullptr;
}

#if WITH_EDITOR

EDataValidationResult UWuwaActionDefinition::IsDataValid(
    FDataValidationContext &Context) const
{
    EDataValidationResult Result =
        Super::IsDataValid(Context);

    if (Result != EDataValidationResult::Invalid)
    {
        Result = EDataValidationResult::Valid;
    }

    // 统一记录配置错误并标记资产无效。
    auto Check =
        [&Context, &Result](
            const bool bCondition,
            const TCHAR *Message)
    {
        if (!bCondition)
        {
            Context.AddError(FText::FromString(Message));
            Result = EDataValidationResult::Invalid;
        }
    };

    Check(
        ActionTag.IsValid(),
        TEXT("ActionTag 必须有效"));

    Check(
        Priority >= 0,
        TEXT("Priority 不能为负数"));

    Check(
        BufferTime >= 0.f,
        TEXT("BufferTime 不能为负数"));

    Check(
        !RequiredTags.HasAny(BlockedTags),
        TEXT("RequiredTags 与 BlockedTags 不能冲突"));

    const bool bNeedsMontage =
        MovementPolicy ==
            EWuwaActionMovementPolicy::RootMotionMontage ||
        MovementPolicy ==
            EWuwaActionMovementPolicy::MotionWarpedRootMotion;

    Check(
        !bNeedsMontage || Montage != nullptr,
        TEXT("当前 MovementPolicy 必须配置 Montage"));

    return Result;
}

#endif