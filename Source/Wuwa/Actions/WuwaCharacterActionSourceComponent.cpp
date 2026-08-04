#include "Actions/WuwaCharacterActionSourceComponent.h"

#include "Actions/WuwaActionDefinition.h"
#include "Core/WuwaGameplayTags.h"
#include "Movement/WuwaCharacterMovementComponent.h"
#include "Math/RotationMatrix.h"
#include "WuwaCharacter.h"
#include "Wuwa.h"

UWuwaCharacterActionSourceComponent::UWuwaCharacterActionSourceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UWuwaCharacterActionSourceComponent::IsInitialized() const
{
    return IsValid(CharacterOwner.Get()) && IsValid(MovementComponent.Get());
}

bool UWuwaCharacterActionSourceComponent::Initialize(AWuwaCharacter *InCharacter, UWuwaCharacterMovementComponent *InMovementComponent)
{
    CharacterOwner = nullptr;
    MovementComponent = nullptr;

    if (!IsValid(InCharacter) || !IsValid(InMovementComponent) || InCharacter != GetOwner() || InMovementComponent != InCharacter->GetWuwaMovementComponent())
    {
        UE_LOG(LogWuwa, Error, TEXT("Character Action Source初始化失败，依赖不匹配。Owner=%s"),
               *GetNameSafe(GetOwner()));
        return false;
    }

    const bool bDirectionalValid =
        IsValid(DirectionalDoubleJumpDefinition.Get()) &&
        DirectionalDoubleJumpDefinition->IsRuntimeValid() &&
        DirectionalDoubleJumpDefinition->ActionTag == WuwaGameplayTags::Action_Movement_DoubleJump_Directional;

    const bool bBackflipValid =
        IsValid(BackflipDoubleJumpDefinition.Get()) &&
        BackflipDoubleJumpDefinition->IsRuntimeValid() &&
        BackflipDoubleJumpDefinition->ActionTag == WuwaGameplayTags::Action_Movement_DoubleJump_Backflip;

    const bool bGroundDashValid =
        IsValid(GroundDashDefinition.Get()) &&
        GroundDashDefinition->IsRuntimeValid() &&
        GroundDashDefinition->ActionTag == WuwaGameplayTags::Action_Movement_Dash_Forward &&
        GroundDashDefinition->MovementPolicy == EWuwaActionMovementPolicy::RootMotionSource;

    const bool bGroundBackstepValid =
        IsValid(GroundBackstepDefinition.Get()) &&
        GroundBackstepDefinition->IsRuntimeValid() &&
        GroundBackstepDefinition->ActionTag == WuwaGameplayTags::Action_Movement_Backstep &&
        GroundBackstepDefinition->MovementPolicy == EWuwaActionMovementPolicy::RootMotionSource;

    // 所有动作分支必须同时具备有效 Definition
    if (!bDirectionalValid || !bBackflipValid || !bGroundDashValid || !bGroundBackstepValid)
    {
        UE_LOG(LogWuwa, Error, TEXT("Character Action Source初始化失败，动作 Definition 无效。Owner=%s"),
               *GetNameSafe(GetOwner()));
        return false;
    }

    CharacterOwner = InCharacter;
    MovementComponent = InMovementComponent;

    return true;
}

bool UWuwaCharacterActionSourceComponent::BuildActionRequest(const FWuwaInputCommand &Command, FWuwaActionRequest &OutRequest) const
{
    OutRequest = FWuwaActionRequest();

    if (!IsInitialized() || !Command.IsValid() || Command.InputTag != WuwaGameplayTags::Input_Sprint)
    {
        return false;
    }

    const EMovementMode MovementModeSnapshot = MovementComponent->MovementMode;

    const uint8 CustomMovementModeSnapshot = MovementComponent->CustomMovementMode;

    const FVector FacingDirectionSnapshot = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();

    if (FacingDirectionSnapshot.IsNearlyZero() || FacingDirectionSnapshot.ContainsNaN())
    {
        return false;
    }

    /*
    const bool bHasWASDDirection = !Command.Direction.IsNearlyZero();

    UWuwaActionDefinition *SelectedDefinition = nullptr;
    */

    const bool bFiniteInputDirection = FMath::IsFinite(Command.Direction.X) && FMath::IsFinite(Command.Direction.Y);

    if (!bFiniteInputDirection)
    {
        return false;
    }

    const bool bHasWASDDirection = !Command.Direction.IsNearlyZero();
    FVector InputWorldDirectionSnapshot = FVector::ZeroVector;

    if (bHasWASDDirection)
    {
        // 与 Character::DoMove 使用同一相机 Yaw 基底：
        // Input.X 表示左右，Input.Y 表示前后。

        const FRotator ControlRotation = CharacterOwner->GetControlRotation();

        if (ControlRotation.ContainsNaN())
        {
            return false;
        }

        const FRotator YawRotation(0.f, ControlRotation.Yaw, 0.f);

        const FRotationMatrix ViewRotationMatrix(YawRotation);

        const FVector CameraForward = ViewRotationMatrix.GetUnitAxis(EAxis::X);

        const FVector CameraRight = ViewRotationMatrix.GetUnitAxis(EAxis::Y);

        InputWorldDirectionSnapshot = CameraForward * Command.Direction.Y + CameraRight * Command.Direction.X;

        InputWorldDirectionSnapshot.Z = 0.f;

        const bool bFiniteWorldDirection = FMath::IsFinite(InputWorldDirectionSnapshot.X) && FMath::IsFinite(InputWorldDirectionSnapshot.Y);

        if (!bFiniteWorldDirection || InputWorldDirectionSnapshot.IsNearlyZero())
        {
            return false;
        }

        // 对角输入只决定方向，动作距离和速度仍由 Definition/Profile 权威配置。
        InputWorldDirectionSnapshot = InputWorldDirectionSnapshot.GetSafeNormal2D();
    }

    UWuwaActionDefinition *SelectedDefinition = nullptr;

    FVector WorldDirectionSnapshot = FVector::ZeroVector;
    const TCHAR *BranchName = TEXT("Invalid");

    if (MovementModeSnapshot == MOVE_Falling)
    {
        SelectedDefinition = bHasWASDDirection ? DirectionalDoubleJumpDefinition.Get() : BackflipDoubleJumpDefinition.Get();

        WorldDirectionSnapshot = bHasWASDDirection ? InputWorldDirectionSnapshot : -FacingDirectionSnapshot;

        BranchName = bHasWASDDirection ? TEXT("AirDirectional") : TEXT("AirBackflip");
    }
    else if (MovementComponent->IsMovingOnGround() && (MovementModeSnapshot == MOVE_Walking || MovementModeSnapshot == MOVE_NavWalking))
    {
        SelectedDefinition = bHasWASDDirection ? GroundDashDefinition.Get() : GroundBackstepDefinition.Get();

        WorldDirectionSnapshot = bHasWASDDirection ? InputWorldDirectionSnapshot : -FacingDirectionSnapshot;

        BranchName = bHasWASDDirection ? TEXT("GroundDash") : TEXT("GroundBackstep");
    }
    else
    {
        return false;
    }

    if (!IsValid(SelectedDefinition) || WorldDirectionSnapshot.IsNearlyZero() || WorldDirectionSnapshot.ContainsNaN())
    {
        return false;
    }

    // 生成动作请求
    OutRequest.ActionTag = SelectedDefinition->ActionTag;
    OutRequest.Definition = SelectedDefinition;
    OutRequest.SourceInputSequence = Command.Sequence;
    OutRequest.Context.InputDirection = Command.Direction;
    OutRequest.Context.WorldDirection = WorldDirectionSnapshot.GetSafeNormal2D();
    OutRequest.Context.FacingDirection = FacingDirectionSnapshot;
    OutRequest.Context.MovementMode = MovementModeSnapshot;
    OutRequest.Context.CustomMovementMode = CustomMovementModeSnapshot;
    OutRequest.Context.SourceObject = CharacterOwner.Get();
    OutRequest.Context.SourceInputSequence = Command.Sequence;

    UE_LOG(LogWuwa, Display, TEXT("Action Request Built: Branch=%s Action=%s InputSequence=%d"),
           BranchName, *OutRequest.ActionTag.ToString(), Command.Sequence);

    return OutRequest.IsValid();
}

void UWuwaCharacterActionSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CharacterOwner = nullptr;
    MovementComponent = nullptr;

    Super::EndPlay(EndPlayReason);
}