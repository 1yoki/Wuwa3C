#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "WuwaActionTypes.generated.h"

class UWuwaActionDefinition;

// 描述一次动作请求最终进入的高层结果。
UENUM(BlueprintType)
enum class EWuwaActionRequestStatus : uint8
{
    Started UMETA(DisplayName = "Started"),
    Buffered UMETA(DisplayName = "Buffered"),
    Rejected UMETA(DisplayName = "Rejected")
};

// Rejected 请求必须提供一个可调试的明确原因。
UENUM(BlueprintType)
enum class EWuwaActionRejectionReason : uint8
{
    None UMETA(DisplayName = "None"),
    InvalidDefinition UMETA(DisplayName = "Invalid Definition"),
    MissingRequiredTag UMETA(DisplayName = "Missing Required Tag"),
    BlockedByTag UMETA(DisplayName = "Blocked By Tag"),
    Priority UMETA(DisplayName = "Priority"),
    CancellationRule UMETA(DisplayName = "Cancellation Rule"),
    Cooldown UMETA(DisplayName = "Cooldown"),
    InvalidContext UMETA(DisplayName = "Invalid Context"),
    NoExecutor UMETA(DisplayName = "No Executor"),
    ExecutorFailed UMETA(DisplayName = "Executor Failed")
};

// 所有动作都必须通过其中一个原因结束，None 只表示尚未结束。
UENUM(BlueprintType)
enum class EWuwaActionEndReason : uint8
{
    None UMETA(DisplayName = "None"),
    Completed UMETA(DisplayName = "Completed"),
    Cancelled UMETA(DisplayName = "Cancelled"),
    Interrupted UMETA(DisplayName = "Interrupted"),
    Failed UMETA(DisplayName = "Failed"),
    OwnerDestroyed UMETA(DisplayName = "Owner Destroyed")
};

// 每个动作只能选择一个主位移权威。
UENUM(BlueprintType)
enum class EWuwaActionMovementPolicy : uint8
{
    None UMETA(DisplayName = "None"),
    CharacterMovement UMETA(DisplayName = "Character Movement"),
    RootMotionMontage UMETA(DisplayName = "Root Motion Montage"),
    MotionWarpedRootMotion UMETA(DisplayName = "Motion Warped Root Motion"),
    RootMotionSource UMETA(DisplayName = "Root Motion Source")
};

/*
 * 动作请求开始时冻结的只读上下文。
 * 输入和方向在命令产生时快照，动作执行期间不再从 Controller 反查。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaActionContext
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Action Context")
    FVector2D InputDirection = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Action Context")
    FVector WorldDirection = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Action Context")
    FVector FacingDirection = FVector::ForwardVector;

    UPROPERTY(BlueprintReadOnly, Category = "Action Context")
    TEnumAsByte<EMovementMode> MovementMode = MOVE_None;

    UPROPERTY(BlueprintReadOnly, Category = "Action Context")
    uint8 CustomMovementMode = 0;

    // 只保存弱引用，Context 不延长来源或目标对象的生命周期。
    UPROPERTY(Transient)
    TWeakObjectPtr<UObject> SourceObject;

    UPROPERTY(Transient)
    TWeakObjectPtr<UObject> TargetObject;

    // 与触发本请求的 Input Command 对应，用于防止重复消费。
    UPROPERTY()
    uint32 SourceInputSequence = 0;
};

// Definition 是只读配置，Router 接受后复制到受 GC 跟踪的运行态。
USTRUCT()
struct WUWA_API FWuwaActionRequest
{
    GENERATED_BODY()

    UPROPERTY()
    FGameplayTag ActionTag;

    UPROPERTY()
    TObjectPtr<UWuwaActionDefinition> Definition = nullptr;

    UPROPERTY()
    FWuwaActionContext Context;

    UPROPERTY()
    uint32 SourceInputSequence = 0;

    bool IsValid() const
    {
        return ActionTag.IsValid() && Definition != nullptr;
    }
};

// 可供 Gameplay、日志和 Debug HUD 共同消费的请求结果。
USTRUCT(BlueprintType)
struct WUWA_API FWuwaActionResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Action Result")
    EWuwaActionRequestStatus Status = EWuwaActionRequestStatus::Rejected;

    UPROPERTY(BlueprintReadOnly, Category = "Action Result")
    EWuwaActionRejectionReason RejectionReason = EWuwaActionRejectionReason::InvalidDefinition;

    UPROPERTY(BlueprintReadOnly, Category = "Action Result")
    FGameplayTag ActionTag;

    UPROPERTY()
    uint32 SourceInputSequence = 0;

    bool HasStarted() const
    {
        return Status == EWuwaActionRequestStatus::Started;
    }
};
