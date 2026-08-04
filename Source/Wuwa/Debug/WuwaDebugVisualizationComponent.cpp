#include "Debug/WuwaDebugVisualizationComponent.h"

#include "Actions/WuwaActionRouterComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/WuwaStateTagComponent.h"
#include "Debug/DebugDrawService.h"
#include "DrawDebugHelpers.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Input/WuwaInputTypes.h"
#include "Movement/WuwaCharacterMovementComponent.h"
#include "Movement/WuwaMovementTypes.h"
#include "Targeting/WuwaTargetingComponent.h"
#include "Targeting/WuwaTargetingTypes.h"

#include "Wuwa.h"
#include "WuwaCharacter.h"

namespace
{
template <typename TEnum>
FString GetEnumValueName(const TEnum Value)
{
    const UEnum *Enum = StaticEnum<TEnum>();
    return IsValid(Enum) ? Enum->GetNameStringByValue(static_cast<int64>(Value)) : TEXT("Unknown");
}

FString GetActorDebugName(const AActor *Actor)
{
    if (!IsValid(Actor))
    {
        return TEXT("None");
    }

#if WITH_EDITOR
    const FString ActorLabel = Actor->GetActorLabel();

    if (!ActorLabel.IsEmpty())
    {
        return ActorLabel;
    }
#endif

    return Actor->GetName();
}

FColor GetTargetModeColor(const EWuwaTargetingMode Mode)
{
    switch (Mode)
    {
    case EWuwaTargetingMode::Soft:
        return FColor::Yellow;

    case EWuwaTargetingMode::Hard:
        return FColor::Red;

    case EWuwaTargetingMode::None:
    default:
        return FColor(180, 180, 180);
    }
}
} // namespace

UWuwaDebugVisualizationComponent::UWuwaDebugVisualizationComponent()
{
    // 世界空间图形需要逐帧重画，但关闭模式不能产生 Tick 成本。
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UWuwaDebugVisualizationComponent::BeginPlay()
{
    Super::BeginPlay();

#if !UE_BUILD_SHIPPING
    APlayerController *OwnerController = Cast<APlayerController>(GetOwner());

    // Debug 叠层只属于本地视口。
    // 远端 Controller 或错误 Owner 不能注册屏幕绘制回调。
    if (IsValid(OwnerController) && OwnerController->IsLocalController())
    {
        RegisterCanvasDelegate();
    }
#endif

    SetComponentTickEnabled(RequiresWorldTick(VisualizationMode));
}

void UWuwaDebugVisualizationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 必须先解除静态 Debug Draw Service 中的回调，
    // 再允许 Component 进入销毁流程。
    UnregisterCanvasDelegate();

    Super::EndPlay(EndPlayReason);
}

void UWuwaDebugVisualizationComponent::CycleVisualizationMode()
{
    switch (VisualizationMode)
    {
    case EWuwaDebugVisualizationMode::Disabled:
        SetVisualizationMode(EWuwaDebugVisualizationMode::Movement);
        break;

    case EWuwaDebugVisualizationMode::Movement:
        SetVisualizationMode(EWuwaDebugVisualizationMode::Targeting);
        break;

    case EWuwaDebugVisualizationMode::Targeting:
        SetVisualizationMode(EWuwaDebugVisualizationMode::All);
        break;

    case EWuwaDebugVisualizationMode::All:
    default:
        SetVisualizationMode(EWuwaDebugVisualizationMode::Disabled);
        break;
    }
}

void UWuwaDebugVisualizationComponent::SetVisualizationMode(const EWuwaDebugVisualizationMode NewMode)
{
    // Debug 模式不产生 Gameplay 事件，也不修改任何权威状态。
    VisualizationMode = NewMode;

    // 只有需要世界空间图形的模式才启用 Tick；关闭后图形在下一帧自动消失。
    SetComponentTickEnabled(RequiresWorldTick(VisualizationMode));
}

void UWuwaDebugVisualizationComponent::TickComponent(
    const float DeltaTime,
    const ELevelTick TickType,
    FActorComponentTickFunction *ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if !UE_BUILD_SHIPPING
    if (!RequiresWorldTick(VisualizationMode))
    {
        return;
    }

    const APlayerController *OwnerController = Cast<APlayerController>(GetOwner());

    if (!IsValid(OwnerController) || !OwnerController->IsLocalController())
    {
        return;
    }

    if (const AWuwaCharacter *Character = ResolveCharacter(*OwnerController))
    {
        if (IncludesMovement(VisualizationMode))
        {
            DrawMovementWorld(*Character);
        }

        if (IncludesTargeting(VisualizationMode))
        {
            DrawTargetingWorld(*Character, *OwnerController);
        }
    }
#endif
}

void UWuwaDebugVisualizationComponent::RegisterCanvasDelegate()
{
    if (CanvasDelegateHandle.IsValid())
    {
        return;
    }

    CanvasDelegateHandle = UDebugDrawService::Register(
        TEXT("Game"),
        FDebugDrawDelegate::CreateUObject(this, &UWuwaDebugVisualizationComponent::DrawDebugCanvas));

    if (!CanvasDelegateHandle.IsValid())
    {
        UE_LOG(
            LogWuwa,
            Error,
            TEXT("Debug Visualization Canvas 注册失败。Owner=%s"),
            *GetNameSafe(GetOwner()));
    }
}

void UWuwaDebugVisualizationComponent::UnregisterCanvasDelegate()
{
    if (!CanvasDelegateHandle.IsValid())
    {
        return;
    }

    UDebugDrawService::Unregister(CanvasDelegateHandle);
    CanvasDelegateHandle.Reset();
}

void UWuwaDebugVisualizationComponent::DrawDebugCanvas(UCanvas *Canvas, APlayerController *PlayerController)
{
    if (VisualizationMode == EWuwaDebugVisualizationMode::Disabled ||
        !IsValid(Canvas) ||
        !IsValid(PlayerController) ||
        PlayerController != GetOwner() ||
        GEngine == nullptr)
    {
        return;
    }

    UFont *DebugFont = GEngine->GetSmallFont();

    if (!IsValid(DebugFont))
    {
        return;
    }

    const FString Header = FString::Printf(
        TEXT("WUWA DEBUG | %s | F10: SWITCH MODE"),
        GetModeDisplayName(VisualizationMode));

    Canvas->SetDrawColor(FColor(80, 255, 120, 255));
    Canvas->DrawText(DebugFont, Header, 24.0f, 24.0f, 1.0f, 1.0f);

    float NextY = 44.0f;

    if (IncludesMovement(VisualizationMode))
    {
        DrawMovementCanvas(*Canvas, *DebugFont, *PlayerController, NextY);
    }

    if (IncludesTargeting(VisualizationMode))
    {
        DrawTargetingCanvas(*Canvas, *DebugFont, *PlayerController, NextY);
    }
}

void UWuwaDebugVisualizationComponent::DrawMovementCanvas(
    UCanvas &Canvas,
    UFont &Font,
    APlayerController &PlayerController,
    float &InOutY) const
{
    AWuwaCharacter *Character = ResolveCharacter(PlayerController);

    if (!IsValid(Character))
    {
        DrawCanvasLine(Canvas, Font, TEXT("MOVEMENT | NO WUWA CHARACTER"), FColor::Red, InOutY);
        return;
    }

    const FWuwaLocomotionSnapshot Movement = Character->GetLocomotionSnapshot();
    const UWuwaCharacterMovementComponent *MovementComponent = Character->GetWuwaMovementComponent();
    const FVector2D MoveIntent = Character->GetCurrentMoveIntent();
    const FString MovementMode = IsValid(MovementComponent) ? MovementComponent->GetMovementName() : TEXT("Invalid");

    DrawCanvasLine(
        Canvas,
        Font,
        FString::Printf(
            TEXT("MOVEMENT | Mode=%s Ground=%d Falling=%d Sprint=%d"),
            *MovementMode,
            Movement.bIsMovingOnGround ? 1 : 0,
            Movement.bIsFalling ? 1 : 0,
            Movement.bIsSprinting ? 1 : 0),
        FColor(80, 255, 120),
        InOutY);

    DrawCanvasLine(
        Canvas,
        Font,
        FString::Printf(
            TEXT("KINEMATICS | Speed=%.1f VZ=%.1f Direction=%.1f Input=%.2f"),
            Movement.HorizontalSpeed,
            Movement.VerticalVelocity,
            Movement.Direction,
            Movement.InputMagnitude),
        FColor::Yellow,
        InOutY);

    DrawCanvasLine(
        Canvas,
        Font,
        FString::Printf(
            TEXT("MOVE INTENT | Right=%.2f Forward=%.2f"),
            MoveIntent.X,
            MoveIntent.Y),
        FColor::Cyan,
        InOutY);

    DrawCanvasLine(
        Canvas,
        Font,
        FString::Printf(
            TEXT("JUMP | Count=%d Last=%s Sequence=%d"),
            Movement.JumpCount,
            *GetEnumValueName(Movement.LastJumpType),
            Movement.JumpSequence),
        FColor(255, 170, 60),
        InOutY);

    DrawCanvasLine(
        Canvas,
        Font,
        FString::Printf(
            TEXT("LANDING | Last=%s Impact=%.1f FallDistance=%.1f Sequence=%d"),
            *GetEnumValueName(Movement.LastLandingType),
            Movement.LastLandingVelocity,
            Movement.LastFallDistance,
            Movement.LandingSequence),
        FColor(255, 170, 60),
        InOutY);

    const UWuwaActionRouterComponent *Router = Character->GetActionRouterComponent();

    if (IsValid(Router))
    {
        const FString CurrentAction = Router->HasActiveAction()
                                          ? Router->GetCurrentActionTag().ToString()
                                          : TEXT("None");
        const FWuwaActionResult LastResult = Router->GetLastResult();

        DrawCanvasLine(
            Canvas,
            Font,
            FString::Printf(TEXT("ACTION | Current=%s LastEnd=%s"),
                            *CurrentAction,
                            *GetEnumValueName(Router->GetLastEndReason())),
            FColor(255, 120, 220),
            InOutY);

        const FString LastRequest = LastResult.SourceInputSequence == 0
                                        ? TEXT("None")
                                        : FString::Printf(
                                              TEXT("%s Status=%s Reject=%s Sequence=%u"),
                                              *LastResult.ActionTag.ToString(),
                                              *GetEnumValueName(LastResult.Status),
                                              *GetEnumValueName(LastResult.RejectionReason),
                                              LastResult.SourceInputSequence);

        DrawCanvasLine(
            Canvas,
            Font,
            FString::Printf(TEXT("LAST REQUEST | %s"), *LastRequest),
            FColor(255, 120, 220),
            InOutY);
    }
    else
    {
        DrawCanvasLine(Canvas, Font, TEXT("ACTION | ROUTER INVALID"), FColor::Red, InOutY);
    }

    // 使用 Input Buffer 专门提供的 Debug Snapshot；只返回副本，不消费任何有效命令。
    const TArray<FWuwaInputCommand> BufferedCommands = Character->GetBufferedInputCommands();

    if (BufferedCommands.IsEmpty())
    {
        DrawCanvasLine(Canvas, Font, TEXT("FIFO | Count=0"), FColor(180, 180, 180), InOutY);
    }
    else
    {
        const FWuwaInputCommand &Front = BufferedCommands[0];
        const UWorld *World = PlayerController.GetWorld();
        const double CurrentTime = IsValid(World) ? static_cast<double>(World->GetTimeSeconds()) : Front.PressedAt;

        DrawCanvasLine(
            Canvas,
            Font,
            FString::Printf(
                TEXT("FIFO | Count=%d Front=%s Sequence=%u Remaining=%.3f Dir=(%.2f, %.2f)"),
                BufferedCommands.Num(),
                *Front.InputTag.ToString(),
                Front.Sequence,
                Front.GetRemainingTime(CurrentTime),
                Front.Direction.X,
                Front.Direction.Y),
            FColor(180, 180, 180),
            InOutY);
    }

    const UWuwaStateTagComponent *StateTags = Character->GetStateTagComponent();

    if (!IsValid(StateTags))
    {
        DrawCanvasLine(Canvas, Font, TEXT("TAGS | COMPONENT INVALID"), FColor::Red, InOutY);
    }
    else
    {
        TArray<FGameplayTag> Tags;
        StateTags->GetActiveTags().GetGameplayTagArray(Tags);
        Tags.Sort([](const FGameplayTag &Left, const FGameplayTag &Right)
                  { return Left.ToString() < Right.ToString(); });

        DrawCanvasLine(
            Canvas,
            Font,
            FString::Printf(TEXT("TAGS | Count=%d"), Tags.Num()),
            FColor(170, 140, 255),
            InOutY);

        for (const FGameplayTag &Tag : Tags)
        {
            DrawCanvasLine(
                Canvas,
                Font,
                FString::Printf(TEXT("  %s x%d"), *Tag.ToString(), StateTags->GetTagSourceCount(Tag)),
                FColor(170, 140, 255),
                InOutY);
        }
    }

    DrawCanvasLine(
        Canvas,
        Font,
        TEXT("WORLD | Blue=Capsule Green=Facing Yellow=Velocity Cyan=Input"),
        FColor::White,
        InOutY);
}

void UWuwaDebugVisualizationComponent::DrawMovementWorld(const AWuwaCharacter &Character) const
{
    const UWorld *World = Character.GetWorld();
    const UCapsuleComponent *Capsule = Character.GetCapsuleComponent();

    if (!IsValid(World) || !IsValid(Capsule))
    {
        return;
    }

    const FVector Origin = Character.GetActorLocation() + FVector(0.0, 0.0, 20.0);

    DrawDebugCapsule(
        World,
        Capsule->GetComponentLocation(),
        Capsule->GetScaledCapsuleHalfHeight(),
        Capsule->GetScaledCapsuleRadius(),
        Capsule->GetComponentQuat(),
        FColor::Blue,
        false,
        -1.0f,
        0,
        1.5f);

    const FVector FacingDirection = Character.GetActorForwardVector().GetSafeNormal();

    if (!FacingDirection.IsNearlyZero() && !FacingDirection.ContainsNaN())
    {
        DrawDebugDirectionalArrow(
            World,
            Origin,
            Origin + FacingDirection * 160.0,
            24.0f,
            FColor::Green,
            false,
            -1.0f,
            0,
            2.5f);
    }

    const FVector Velocity = Character.GetVelocity();

    if (!Velocity.IsNearlyZero() && !Velocity.ContainsNaN())
    {
        const float VelocityArrowLength = FMath::Clamp(Velocity.Size() * 0.15f, 60.0f, 300.0f);

        DrawDebugDirectionalArrow(
            World,
            Origin,
            Origin + Velocity.GetSafeNormal() * VelocityArrowLength,
            24.0f,
            FColor::Yellow,
            false,
            -1.0f,
            0,
            2.5f);
    }

    const FVector2D MoveIntent = Character.GetCurrentMoveIntent();
    const FRotator ControlYaw(0.0, Character.GetControlRotation().Yaw, 0.0);
    const FVector CameraForward = FRotationMatrix(ControlYaw).GetUnitAxis(EAxis::X);
    const FVector CameraRight = FRotationMatrix(ControlYaw).GetUnitAxis(EAxis::Y);
    FVector InputWorldDirection = CameraForward * MoveIntent.Y + CameraRight * MoveIntent.X;
    InputWorldDirection.Z = 0.0;

    if (!InputWorldDirection.IsNearlyZero() && !InputWorldDirection.ContainsNaN())
    {
        const float InputArrowLength = 140.0f * FMath::Clamp(MoveIntent.Size(), 0.0f, 1.0f);

        DrawDebugDirectionalArrow(
            World,
            Origin,
            Origin + InputWorldDirection.GetSafeNormal2D() * InputArrowLength,
            24.0f,
            FColor::Cyan,
            false,
            -1.0f,
            0,
            2.5f);
    }
}

void UWuwaDebugVisualizationComponent::DrawTargetingCanvas(
    UCanvas &Canvas,
    UFont &Font,
    APlayerController &PlayerController,
    float &InOutY) const
{
    AWuwaCharacter *Character = ResolveCharacter(PlayerController);

    if (!IsValid(Character))
    {
        DrawCanvasLine(Canvas, Font, TEXT("TARGETING | NO WUWA CHARACTER"), FColor::Red, InOutY);
        return;
    }

    const UWuwaTargetingComponent *Targeting = Character->GetTargetingComponent();

    if (!IsValid(Targeting))
    {
        DrawCanvasLine(Canvas, Font, TEXT("TARGETING | COMPONENT INVALID"), FColor::Red, InOutY);
        return;
    }

    const FWuwaTargetContext Context = Targeting->GetTargetContext();
    const AActor *TargetActor = Context.TargetActor.Get();
    const bool bHasValidTarget = Context.HasValidTarget();
    const FColor ModeColor = GetTargetModeColor(Context.Mode);

    DrawCanvasLine(
        Canvas,
        Font,
        FString::Printf(
            TEXT("TARGETING | Mode=%s Valid=%d Target=%s Revision=%d"),
            *GetEnumValueName(Context.Mode),
            bHasValidTarget ? 1 : 0,
            *GetActorDebugName(TargetActor),
            Context.Revision),
        ModeColor,
        InOutY);

    DrawCanvasLine(
        Canvas,
        Font,
        FString::Printf(
            TEXT("TARGET POINT | X=%.1f Y=%.1f Z=%.1f"),
            Context.TargetPoint.X,
            Context.TargetPoint.Y,
            Context.TargetPoint.Z),
        ModeColor,
        InOutY);

    DrawCanvasLine(
        Canvas,
        Font,
        FString::Printf(
            TEXT("SCORE | Distance=%.1f Angle=%.1f Total=%.3f Visible=%d"),
            Context.Score.Distance,
            Context.Score.ViewAngleDegrees,
            Context.Score.TotalScore,
            Context.Score.bVisible ? 1 : 0),
        FColor(255, 170, 60),
        InOutY);

    DrawCanvasLine(
        Canvas,
        Font,
        FString::Printf(
            TEXT("SCORE DETAIL | Distance=%.3f View=%.3f Retention=%.3f ViewSpace=(%.3f, %.3f)"),
            Context.Score.DistanceScore,
            Context.Score.ViewAlignmentScore,
            Context.Score.RetentionBonus,
            Context.Score.ViewSpaceHorizontal,
            Context.Score.ViewSpaceVertical),
        FColor(255, 170, 60),
        InOutY);

    DrawCanvasLine(
        Canvas,
        Font,
        FString::Printf(
            TEXT("LAST FAILURE | %s"),
            *GetEnumValueName(Targeting->GetLastFailureReason())),
        Targeting->GetLastFailureReason() == EWuwaTargetingFailureReason::None
            ? FColor(180, 180, 180)
            : FColor::Red,
        InOutY);

    DrawCanvasLine(
        Canvas,
        Font,
        TEXT("WORLD | Yellow=Soft Red=Hard Purple=ViewForward Orange=ViewToTarget"),
        FColor::White,
        InOutY);

    DrawTargetScreenMarker(Canvas, PlayerController, *Character);
}

void UWuwaDebugVisualizationComponent::DrawTargetingWorld(
    const AWuwaCharacter &Character,
    const APlayerController &PlayerController) const
{
    const UWorld *World = Character.GetWorld();
    const UWuwaTargetingComponent *Targeting = Character.GetTargetingComponent();

    if (!IsValid(World) || !IsValid(Targeting))
    {
        return;
    }

    const FWuwaTargetContext Context = Targeting->GetTargetContext();

    if (!Context.HasValidTarget() || Context.TargetPoint.ContainsNaN())
    {
        return;
    }

    const FColor TargetColor = GetTargetModeColor(Context.Mode);
    const FVector CharacterOrigin = Character.GetActorLocation() + FVector(0.0, 0.0, 30.0);

    DrawDebugSphere(
        World,
        Context.TargetPoint,
        28.0f,
        16,
        TargetColor,
        false,
        -1.0f,
        0,
        1.5f);

    DrawDebugDirectionalArrow(
        World,
        CharacterOrigin,
        Context.TargetPoint,
        32.0f,
        TargetColor,
        false,
        -1.0f,
        0,
        0.0f);

    DrawDebugLine(
        World,
        Context.TargetPoint - FVector(0.0, 0.0, 55.0),
        Context.TargetPoint + FVector(0.0, 0.0, 55.0),
        TargetColor,
        false,
        -1.0f,
        0,
        0.0f);

    FVector ViewLocation = FVector::ZeroVector;
    FRotator ViewRotation = FRotator::ZeroRotator;
    PlayerController.GetPlayerViewPoint(ViewLocation, ViewRotation);

    if (!ViewLocation.ContainsNaN() && !ViewRotation.ContainsNaN())
    {
        const FVector ViewForward = ViewRotation.Vector().GetSafeNormal();
        const FVector ViewToTarget = Context.TargetPoint - ViewLocation;

        if (!ViewForward.IsNearlyZero() && !ViewForward.ContainsNaN())
        {
            // 不能从精确 CameraLocation 绘制带厚度线段：起点贴住近裁剪面时，
            // 线段四边形会被投影放大成遮挡画面的巨大色块。
            const FVector SafeViewOrigin = ViewLocation + ViewForward * 50.0f;

            DrawDebugDirectionalArrow(
                World,
                SafeViewOrigin,
                SafeViewOrigin + ViewForward * 180.0f,
                24.0f,
                FColor(200, 80, 255),
                false,
                -1.0f,
                0,
                0.0f);
        }

        if (!ViewToTarget.IsNearlyZero() && !ViewToTarget.ContainsNaN())
        {
            // 起点沿当前目标方向移出近裁剪区域；该线只表达几何关系，
            // 不代表重新执行 Visibility Trace。
            const FVector SafeTargetLineOrigin = ViewLocation + ViewToTarget.GetSafeNormal() * 50.0f;

            DrawDebugLine(
                World,
                SafeTargetLineOrigin,
                Context.TargetPoint,
                FColor(255, 120, 40),
                false,
                -1.0f,
                0,
                0.0f);
        }
    }

}

void UWuwaDebugVisualizationComponent::DrawTargetScreenMarker(
    UCanvas &Canvas,
    APlayerController &PlayerController,
    const AWuwaCharacter &Character) const
{
    const UWuwaTargetingComponent *Targeting = Character.GetTargetingComponent();

    if (!IsValid(Targeting))
    {
        return;
    }

    const FWuwaTargetContext Context = Targeting->GetTargetContext();
    FVector2D ScreenPosition = FVector2D::ZeroVector;

    if (!Context.HasValidTarget() ||
        Context.TargetPoint.ContainsNaN() ||
        !PlayerController.ProjectWorldLocationToScreen(Context.TargetPoint, ScreenPosition, false) ||
        ScreenPosition.X < 0.0 ||
        ScreenPosition.Y < 0.0 ||
        ScreenPosition.X > Canvas.SizeX ||
        ScreenPosition.Y > Canvas.SizeY)
    {
        return;
    }

    const FLinearColor MarkerColor(GetTargetModeColor(Context.Mode));
    constexpr float InnerGap = 5.0f;
    constexpr float OuterRadius = 16.0f;
    const float Thickness = Context.Mode == EWuwaTargetingMode::Hard ? 3.0f : 2.0f;

    Canvas.K2_DrawLine(
        ScreenPosition + FVector2D(-OuterRadius, 0.0),
        ScreenPosition + FVector2D(-InnerGap, 0.0),
        Thickness,
        MarkerColor);
    Canvas.K2_DrawLine(
        ScreenPosition + FVector2D(InnerGap, 0.0),
        ScreenPosition + FVector2D(OuterRadius, 0.0),
        Thickness,
        MarkerColor);
    Canvas.K2_DrawLine(
        ScreenPosition + FVector2D(0.0, -OuterRadius),
        ScreenPosition + FVector2D(0.0, -InnerGap),
        Thickness,
        MarkerColor);
    Canvas.K2_DrawLine(
        ScreenPosition + FVector2D(0.0, InnerGap),
        ScreenPosition + FVector2D(0.0, OuterRadius),
        Thickness,
        MarkerColor);
}

AWuwaCharacter *UWuwaDebugVisualizationComponent::ResolveCharacter(const APlayerController &PlayerController) const
{
    return Cast<AWuwaCharacter>(PlayerController.GetPawn());
}

void UWuwaDebugVisualizationComponent::DrawCanvasLine(
    UCanvas &Canvas,
    UFont &Font,
    const FString &Text,
    const FColor &Color,
    float &InOutY)
{
    Canvas.SetDrawColor(Color);
    Canvas.DrawText(&Font, Text, 24.0f, InOutY, 1.0f, 1.0f);
    InOutY += 15.0f;
}

bool UWuwaDebugVisualizationComponent::IncludesMovement(const EWuwaDebugVisualizationMode Mode)
{
    return Mode == EWuwaDebugVisualizationMode::Movement || Mode == EWuwaDebugVisualizationMode::All;
}

bool UWuwaDebugVisualizationComponent::IncludesTargeting(const EWuwaDebugVisualizationMode Mode)
{
    return Mode == EWuwaDebugVisualizationMode::Targeting || Mode == EWuwaDebugVisualizationMode::All;
}

bool UWuwaDebugVisualizationComponent::RequiresWorldTick(const EWuwaDebugVisualizationMode Mode)
{
    return IncludesMovement(Mode) || IncludesTargeting(Mode);
}

const TCHAR *UWuwaDebugVisualizationComponent::GetModeDisplayName(const EWuwaDebugVisualizationMode Mode)
{
    switch (Mode)
    {
    case EWuwaDebugVisualizationMode::Movement:
        return TEXT("MOVEMENT");

    case EWuwaDebugVisualizationMode::Targeting:
        return TEXT("TARGETING");

    case EWuwaDebugVisualizationMode::All:
        return TEXT("ALL");

    case EWuwaDebugVisualizationMode::Disabled:
    default:
        return TEXT("DISABLED");
    }
}
