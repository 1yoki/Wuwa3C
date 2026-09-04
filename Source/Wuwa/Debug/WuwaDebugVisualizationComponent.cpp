#include "Debug/WuwaDebugVisualizationComponent.h"

#include "Actions/Network/WuwaActionNetworkComponent.h"
#include "Actions/Runtime/WuwaActionCoordinatorComponent.h"
#include "AbilitySystem/Abilities/WuwaGameplayAbility_MeleeAttack.h"
#include "AbilitySystem/Abilities/WuwaGameplayAbility_Stagger.h"
#include "AbilitySystem/Input/WuwaAbilityInputRouterComponent.h"
#include "AbilitySystem/Interop/WuwaActionAbilityInteropComponent.h"
#include "AbilitySystem/Runtime/WuwaAbilitySystemComponent.h"
#include "AbilitySystem/Runtime/WuwaPawnAbilityInitComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Camera/WuwaCameraModeComponent.h"
#include "Camera/WuwaSpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Combat/Data/WuwaCombatProfile.h"
#include "Combat/Data/WuwaMeleeAttackDefinition.h"
#include "Combat/Health/WuwaHealthComponent.h"
#include "Combat/Poise/WuwaPoiseComponent.h"
#include "Combat/Runtime/WuwaCombatExecutionComponent.h"
#include "Combat/Runtime/WuwaWeaponComponent.h"
#include "Core/WuwaStateTagComponent.h"
#include "Debug/DebugDrawService.h"
#include "DrawDebugHelpers.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayAbilitySpec.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Input/WuwaInputTypes.h"
#include "Movement/WuwaCharacterMovementComponent.h"
#include "Movement/WuwaMovementTypes.h"
#include "Targeting/WuwaTargetingComponent.h"
#include "Targeting/WuwaTargetingTypes.h"
#include "Traversal/Resolution/WuwaTraversalActionIntentProviderComponent.h"
#include "UI/WuwaWorldHealthBarComponent.h"

#include "Wuwa.h"
#include "WuwaCharacter.h"
#include "WuwaPlayerState.h"

namespace
{
/** @return 调试状态代码对应的中文名称 */
FString GetDebugValueDisplayName(const FString& Value)
{
	static const TMap<FString, FString> DisplayNames = {
	    {TEXT("None"), TEXT("无")},
	    {TEXT("Unknown"), TEXT("未知")},
	    {TEXT("Invalid"), TEXT("无效")},
	    {TEXT("Walking"), TEXT("行走")},
	    {TEXT("NavWalking"), TEXT("导航行走")},
	    {TEXT("Falling"), TEXT("下落")},
	    {TEXT("Swimming"), TEXT("游泳")},
	    {TEXT("Flying"), TEXT("飞行")},
	    {TEXT("Custom"), TEXT("自定义移动")},
	    {TEXT("Ground"), TEXT("地面跳跃")},
	    {TEXT("Coyote"), TEXT("土狼跳跃")},
	    {TEXT("AirSprint"), TEXT("空中定向二段跳")},
	    {TEXT("AirBackflip"), TEXT("空中后空翻")},
	    {TEXT("Light"), TEXT("轻落地")},
	    {TEXT("Heavy"), TEXT("重落地")},
	    {TEXT("Preparing"), TEXT("准备中")},
	    {TEXT("Active"), TEXT("执行中")},
	    {TEXT("Finishing"), TEXT("收尾中")},
	    {TEXT("Finished"), TEXT("已结束")},
	    {TEXT("Completed"), TEXT("正常完成")},
	    {TEXT("Cancelled"), TEXT("已取消")},
	    {TEXT("Interrupted"), TEXT("被中断")},
	    {TEXT("Failed"), TEXT("失败")},
	    {TEXT("OwnerDestroyed"), TEXT("所有者已销毁")},
	    {TEXT("Started"), TEXT("已启动")},
	    {TEXT("Buffered"), TEXT("已缓冲")},
	    {TEXT("Rejected"), TEXT("已拒绝")},
	    {TEXT("InvalidDefinition"), TEXT("配置无效")},
	    {TEXT("MissingRequiredTag"), TEXT("缺少必需标签")},
	    {TEXT("BlockedByTag"), TEXT("被标签阻止")},
	    {TEXT("Priority"), TEXT("优先级不足")},
	    {TEXT("CancellationRule"), TEXT("取消规则阻止")},
	    {TEXT("Cooldown"), TEXT("冷却中")},
	    {TEXT("InvalidContext"), TEXT("上下文无效")},
	    {TEXT("MissingCapability"), TEXT("缺少执行能力")},
	    {TEXT("CapabilityPrepareFailed"), TEXT("执行能力准备失败")},
	    {TEXT("CapabilityCommitFailed"), TEXT("执行能力提交失败")},
	    {TEXT("StaleActionHandle"), TEXT("动作句柄过期")},
	    {TEXT("QueueFull"), TEXT("队列已满")},
	    {TEXT("DuplicateRequest"), TEXT("重复请求")},
	    {TEXT("Soft"), TEXT("软锁定")},
	    {TEXT("Hard"), TEXT("硬锁定")},
	    {TEXT("NotInitialized"), TEXT("未初始化")},
	    {TEXT("InvalidProfile"), TEXT("配置无效")},
	    {TEXT("InvalidRequester"), TEXT("请求者无效")},
	    {TEXT("InvalidView"), TEXT("视点无效")},
	    {TEXT("InvalidCandidate"), TEXT("候选无效")},
	    {TEXT("StateCommitFailed"), TEXT("状态提交失败")},
	    {TEXT("NotTargetable"), TEXT("不可锁定")},
	    {TEXT("OutOfRange"), TEXT("超出距离")},
	    {TEXT("OutsideAcquireAngle"), TEXT("超出获取角度")},
	    {TEXT("Occluded"), TEXT("被遮挡")},
	    {TEXT("NoCandidate"), TEXT("无候选")},
	    {TEXT("NoHardLock"), TEXT("无硬锁定")},
	    {TEXT("InvalidDirection"), TEXT("方向无效")},
	    {TEXT("TooShort"), TEXT("距离过短")},
	    {TEXT("TrajectoryBlocked"), TEXT("轨迹受阻")},
	    {TEXT("NoReleaseClearance"), TEXT("释放点空间不足")},
	    {TEXT("StartDriftTooLarge"), TEXT("起点漂移过大")},
	    {TEXT("InvalidWorld"), TEXT("世界无效")},
	    {TEXT("Action.Movement.Dash.Forward"), TEXT("前冲")},
	    {TEXT("Action.Movement.Backstep"), TEXT("后撤")},
	    {TEXT("Action.Movement.DoubleJump.Directional"), TEXT("空中定向二段跳")},
	    {TEXT("Action.Movement.DoubleJump.Backflip"), TEXT("空中后空翻")},
	    {TEXT("Action.Traversal.Grapple"), TEXT("钩锁")},
	    {TEXT("Action.Traversal.Grapple.Free"), TEXT("自由钩锁")},
	    {TEXT("State.Action.Dash"), TEXT("冲刺动作")},
	    {TEXT("State.Locomotion.Sprinting"), TEXT("冲刺移动")},
	    {TEXT("State.Targeting.HardLocked"), TEXT("硬锁定")},
	    {TEXT("State.Traversal.Grappling"), TEXT("钩锁移动")},
	    {TEXT("Camera.Exploration"), TEXT("探索")},
	    {TEXT("Camera.LockOn"), TEXT("锁定")}};

	if (const FString* DisplayName = DisplayNames.Find(Value))
	{
		return *DisplayName;
	}

	return Value;
}

template <typename TEnum> FString GetEnumValueName(const TEnum Value)
{
	const UEnum* Enum = StaticEnum<TEnum>();
	return IsValid(Enum) ? GetDebugValueDisplayName(Enum->GetNameStringByValue(static_cast<int64>(Value)))
	                     : TEXT("未知");
}

FString GetActorDebugName(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return TEXT("无");
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

/** @return 向量是否只包含有限数值 */
bool IsFiniteVector(const FVector& Value)
{
	return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
}

/** @return 旋转是否只包含有限数值 */
bool IsFiniteRotator(const FRotator& Value)
{
	return FMath::IsFinite(Value.Pitch) && FMath::IsFinite(Value.Yaw) && FMath::IsFinite(Value.Roll);
}

/** @return 网络模式对应的稳定调试名称 */
const TCHAR* GetNetModeDebugName(const ENetMode NetMode)
{
	switch (NetMode)
	{
		case NM_Standalone:
			return TEXT("Standalone");
		case NM_DedicatedServer:
			return TEXT("DedicatedServer");
		case NM_ListenServer:
			return TEXT("ListenServer");
		case NM_Client:
			return TEXT("Client");
		default:
			return TEXT("Unknown");
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
	APlayerController* OwnerController = Cast<APlayerController>(GetOwner());

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
			SetVisualizationMode(EWuwaDebugVisualizationMode::Camera);
			break;

		case EWuwaDebugVisualizationMode::Camera:
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

void UWuwaDebugVisualizationComponent::TickComponent(const float DeltaTime,
                                                     const ELevelTick TickType,
                                                     FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if !UE_BUILD_SHIPPING
	if (!RequiresWorldTick(VisualizationMode))
	{
		return;
	}

	const APlayerController* OwnerController = Cast<APlayerController>(GetOwner());

	if (!IsValid(OwnerController) || !OwnerController->IsLocalController())
	{
		return;
	}

	if (const AWuwaCharacter* Character = ResolveCharacter(*OwnerController))
	{
		DrawCombatExecutionWorld(*Character);
		if (VisualizationMode == EWuwaDebugVisualizationMode::All)
		{
			DrawAllWorldSummary(*Character, *OwnerController);
		}
		else if (IncludesMovement(VisualizationMode))
		{
			DrawMovementWorld(*Character);
		}
		else if (IncludesTargeting(VisualizationMode))
		{
			DrawTargetingWorld(*Character);
		}
		else if (IncludesCamera(VisualizationMode))
		{
			DrawCameraCompositionSpheres(*Character, *OwnerController);
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
	    TEXT("Game"), FDebugDrawDelegate::CreateUObject(this, &UWuwaDebugVisualizationComponent::DrawDebugCanvas));

	if (!CanvasDelegateHandle.IsValid())
	{
		UE_LOG(LogWuwa, Error, TEXT("Debug Visualization Canvas 注册失败。Owner=%s"), *GetNameSafe(GetOwner()));
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

void UWuwaDebugVisualizationComponent::DrawDebugCanvas(UCanvas* Canvas, APlayerController* PlayerController)
{
	if (VisualizationMode == EWuwaDebugVisualizationMode::Disabled || !IsValid(Canvas) || !IsValid(PlayerController) ||
	    PlayerController != GetOwner() || GEngine == nullptr)
	{
		return;
	}

	UFont* DebugFont = GEngine->GetSmallFont();

	if (!IsValid(DebugFont))
	{
		return;
	}

	const FString Header =
	    FString::Printf(TEXT("运行时调试 | %s | F10：切换模式"), GetModeDisplayName(VisualizationMode));

	Canvas->SetDrawColor(FColor(80, 255, 120, 255));
	Canvas->DrawText(DebugFont, Header, 24.0f, 24.0f, 1.0f, 1.0f);

	float NextY = 44.0f;

	if (const AWuwaCharacter* Character = ResolveCharacter(*PlayerController))
	{
		if (VisualizationMode == EWuwaDebugVisualizationMode::All)
		{
			DrawNetworkSummary(*Canvas, *DebugFont, *Character, NextY);
			DrawAbilitySystemSummary(*Canvas, *DebugFont, *Character, NextY);
			DrawAbilityInputSummary(*Canvas, *DebugFont, *Character, NextY);
			DrawHealthSummary(*Canvas, *DebugFont, *Character, NextY);
		}
		DrawCombatSummary(*Canvas, *DebugFont, *Character, NextY);
	}

	if (VisualizationMode == EWuwaDebugVisualizationMode::All)
	{
		DrawAllCanvasSummary(*Canvas, *DebugFont, *PlayerController, NextY);
	}
	else if (IncludesMovement(VisualizationMode))
	{
		DrawMovementCanvas(*Canvas, *DebugFont, *PlayerController, NextY);
	}
	else if (IncludesTargeting(VisualizationMode))
	{
		DrawTargetingCanvas(*Canvas, *DebugFont, *PlayerController, NextY);
	}
	else if (IncludesCamera(VisualizationMode))
	{
		DrawCameraCanvas(*Canvas, *DebugFont, *PlayerController, NextY);
	}
}

void UWuwaDebugVisualizationComponent::DrawAllCanvasSummary(UCanvas& Canvas,
                                                            UFont& Font,
                                                            APlayerController& PlayerController,
                                                            float& InOutY) const
{
	const AWuwaCharacter* Character = ResolveCharacter(PlayerController);
	if (!IsValid(Character))
	{
		DrawCanvasLine(Canvas, Font, TEXT("综合 | 未找到玩家角色"), FColor::Red, InOutY);
		return;
	}

	const FWuwaLocomotionSnapshot Movement = Character->GetLocomotionSnapshot();
	const UWuwaCharacterMovementComponent* MovementComponent = Character->GetWuwaMovementComponent();
	const FString MovementMode =
	    IsValid(MovementComponent) ? GetDebugValueDisplayName(MovementComponent->GetMovementName()) : TEXT("无效");
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("移动 | 模式=%s 接地=%d 速度=%.1f 垂直速度=%.1f"),
	                               *MovementMode,
	                               Movement.bIsMovingOnGround ? 1 : 0,
	                               Movement.HorizontalSpeed,
	                               Movement.VerticalVelocity),
	               IsValid(MovementComponent) ? FColor(80, 255, 120) : FColor::Red,
	               InOutY);

	const UWuwaTargetingComponent* Targeting = Character->GetTargetingComponent();
	const FWuwaTargetContext TargetContext = IsValid(Targeting) ? Targeting->GetTargetContext() : FWuwaTargetContext();
	const bool bHasTarget = TargetContext.HasValidTarget();
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("锁定 | 模式=%s 目标=%s 可见=%d"),
	                               *GetEnumValueName(TargetContext.Mode),
	                               *GetActorDebugName(TargetContext.TargetActor.Get()),
	                               bHasTarget && TargetContext.Score.bVisible ? 1 : 0),
	               IsValid(Targeting) ? GetTargetModeColor(TargetContext.Mode) : FColor::Red,
	               InOutY);

	const UWuwaCameraModeComponent* CameraMode = Character->GetCameraModeComponent();
	const UWuwaSpringArmComponent* CameraBoom = Character->FindComponentByClass<UWuwaSpringArmComponent>();
	const UCameraComponent* FollowCamera = Character->FindComponentByClass<UCameraComponent>();
	const FVector SpringArmPivotLocation =
	    IsValid(CameraBoom) ? CameraBoom->GetComponentLocation() + CameraBoom->TargetOffset : FVector::ZeroVector;
	const float ActualArmLength = IsValid(CameraBoom) && IsValid(FollowCamera) && IsFiniteVector(SpringArmPivotLocation)
	                                  ? FVector::Distance(SpringArmPivotLocation, FollowCamera->GetComponentLocation())
	                                  : 0.f;
	const FGameplayTag CameraModeTag = IsValid(CameraMode) ? CameraMode->GetActiveModeTag() : FGameplayTag();
	const bool bCameraValid = IsValid(CameraMode) && IsValid(CameraBoom) && IsValid(FollowCamera) &&
	                          FMath::IsFinite(ActualArmLength) && FMath::IsFinite(FollowCamera->FieldOfView);
	DrawCanvasLine(
	    Canvas,
	    Font,
	    FString::Printf(TEXT("镜头 | 模式=%s FOV=%.1f 实际臂长=%.1f"),
	                    CameraModeTag.IsValid() ? *GetDebugValueDisplayName(CameraModeTag.ToString()) : TEXT("无效"),
	                    IsValid(FollowCamera) ? FollowCamera->FieldOfView : 0.f,
	                    ActualArmLength),
	    bCameraValid ? FColor::Cyan : FColor::Red,
	    InOutY);

	DrawCanvasLine(Canvas,
	               Font,
	               TEXT("世界图例 | 蓝=角色胶囊 黄/红=锁定目标与连线 天蓝=角色锚点 橙黄=画面中心"),
	               FColor::White,
	               InOutY);
}

void UWuwaDebugVisualizationComponent::DrawAllWorldSummary(const AWuwaCharacter& Character,
                                                           const APlayerController& PlayerController) const
{
	DrawMovementWorld(Character);
	DrawTargetingWorld(Character);
	DrawCameraCompositionSpheres(Character, PlayerController);
}

void UWuwaDebugVisualizationComponent::DrawMovementCanvas(UCanvas& Canvas,
                                                          UFont& Font,
                                                          APlayerController& PlayerController,
                                                          float& InOutY) const
{
	AWuwaCharacter* Character = ResolveCharacter(PlayerController);

	if (!IsValid(Character))
	{
		DrawCanvasLine(Canvas, Font, TEXT("移动 | 未找到玩家角色"), FColor::Red, InOutY);
		return;
	}

	DrawAbilitySystemSummary(Canvas, Font, *Character, InOutY);

	const FWuwaLocomotionSnapshot Movement = Character->GetLocomotionSnapshot();
	const UWuwaCharacterMovementComponent* MovementComponent = Character->GetWuwaMovementComponent();
	const FVector2D MoveIntent = Character->GetCurrentMoveIntent();
	const FString MovementMode =
	    IsValid(MovementComponent) ? GetDebugValueDisplayName(MovementComponent->GetMovementName()) : TEXT("无效");

	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("移动 | 模式=%s 接地=%d 下落=%d 冲刺=%d"),
	                               *MovementMode,
	                               Movement.bIsMovingOnGround ? 1 : 0,
	                               Movement.bIsFalling ? 1 : 0,
	                               Movement.bIsSprinting ? 1 : 0),
	               FColor(80, 255, 120),
	               InOutY);

	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("运动数据 | 速度=%.1f 垂直速度=%.1f 方向=%.1f 输入强度=%.2f"),
	                               Movement.HorizontalSpeed,
	                               Movement.VerticalVelocity,
	                               Movement.Direction,
	                               Movement.InputMagnitude),
	               FColor::Yellow,
	               InOutY);

	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("原始移动意图 | 右=%.2f 前=%.2f"), MoveIntent.X, MoveIntent.Y),
	               FColor::Cyan,
	               InOutY);

	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("跳跃 | 次数=%d 最近=%s 序号=%d"),
	                               Movement.JumpCount,
	                               *GetEnumValueName(Movement.LastJumpType),
	                               Movement.JumpSequence),
	               FColor(255, 170, 60),
	               InOutY);

	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("落地 | 最近=%s 冲击速度=%.1f 下落距离=%.1f 序号=%d"),
	                               *GetEnumValueName(Movement.LastLandingType),
	                               Movement.LastLandingVelocity,
	                               Movement.LastFallDistance,
	                               Movement.LandingSequence),
	               FColor(255, 170, 60),
	               InOutY);

	const UWuwaActionCoordinatorComponent* Coordinator = Character->GetActionCoordinatorComponent();

	if (IsValid(Coordinator))
	{
		const FWuwaActionRuntimeSnapshot Action = Coordinator->GetRuntimeSnapshot();
		const FString CurrentAction =
		    Action.ActiveActionTag.IsValid() ? GetDebugValueDisplayName(Action.ActiveActionTag.ToString()) : TEXT("无");
		const FWuwaActionResult& LastResult = Action.LastResult;

		DrawCanvasLine(Canvas,
		               Font,
		               FString::Printf(TEXT("动作 | 当前=%s 句柄=%lld 状态=%s 最近结束=%s"),
		                               *CurrentAction,
		                               Action.ActiveHandle.Value,
		                               *GetEnumValueName(Action.InstanceState),
		                               *GetEnumValueName(Action.LastEndReason)),
		               FColor(255, 120, 220),
		               InOutY);

		const FString LastRequest = LastResult.SourceSequence == 0
		                                ? TEXT("无")
		                                : FString::Printf(TEXT("%s 状态=%s 拒绝原因=%s 序号=%d"),
		                                                  *GetDebugValueDisplayName(LastResult.ActionTag.ToString()),
		                                                  *GetEnumValueName(LastResult.Status),
		                                                  *GetEnumValueName(LastResult.RejectionReason),
		                                                  LastResult.SourceSequence);

		DrawCanvasLine(
		    Canvas, Font, FString::Printf(TEXT("最近请求 | %s"), *LastRequest), FColor(255, 120, 220), InOutY);
	}
	else
	{
		DrawCanvasLine(Canvas, Font, TEXT("动作 | 协调器无效"), FColor::Red, InOutY);
	}

	const UWuwaStateTagComponent* StateTags = Character->GetStateTagComponent();

	if (!IsValid(StateTags))
	{
		DrawCanvasLine(Canvas, Font, TEXT("状态标签 | 组件无效"), FColor::Red, InOutY);
	}
	else
	{
		TArray<FGameplayTag> Tags;
		StateTags->GetActiveTags().GetGameplayTagArray(Tags);
		Tags.Sort(
		    [](const FGameplayTag& Left, const FGameplayTag& Right)
		    {
			    return Left.ToString() < Right.ToString();
		    });

		DrawCanvasLine(
		    Canvas, Font, FString::Printf(TEXT("状态标签 | 数量=%d"), Tags.Num()), FColor(170, 140, 255), InOutY);

		for (const FGameplayTag& Tag : Tags)
		{
			DrawCanvasLine(Canvas,
			               Font,
			               FString::Printf(TEXT("  %s ×%d"),
			                               *GetDebugValueDisplayName(Tag.ToString()),
			                               StateTags->GetTagSourceCount(Tag)),
			               FColor(170, 140, 255),
			               InOutY);
		}
	}

	const UWuwaTraversalActionIntentProviderComponent* GrappleQuery =
	    Character->FindComponentByClass<UWuwaTraversalActionIntentProviderComponent>();
	if (IsValid(GrappleQuery) && GrappleQuery->GetDebugSnapshot().bHasQuery)
	{
		const FWuwaGrappleQueryDebugSnapshot& Grapple = GrappleQuery->GetDebugSnapshot();
		const FWuwaGrappleQueryResult& Query = Grapple.QueryResult;
		DrawCanvasLine(Canvas,
		               Font,
		               FString::Printf(TEXT("钩锁查询 | 成功=%d 失败原因=%s 缩放=%.3f 阻挡段=%d"),
		                               Query.bSucceeded ? 1 : 0,
		                               *GetEnumValueName(Query.FailureReason),
		                               Query.ResolvedTrajectoryScale,
		                               Query.BlockingSegmentIndex),
		               Query.bSucceeded ? FColor(80, 255, 120) : FColor::Red,
		               InOutY);
		DrawCanvasLine(Canvas,
		               Font,
		               FString::Printf(TEXT("钩锁锚点 | 请求=%s 结果=%s"),
		                               *Query.RequestedVisualAnchorLocation.ToCompactString(),
		                               *Query.ResolvedVisualAnchorLocation.ToCompactString()),
		               FColor(255, 80, 255),
		               InOutY);
		DrawCanvasLine(Canvas,
		               Font,
		               FString::Printf(TEXT("钩锁释放点 | 请求=%s 结果=%s 采样数=%d"),
		                               *Query.RequestedReleaseLocation.ToCompactString(),
		                               *Query.ResolvedReleaseLocation.ToCompactString(),
		                               Grapple.PredictedTrajectoryPoints.Num()),
		               FColor(80, 220, 255),
		               InOutY);
	}

	DrawCanvasLine(
	    Canvas, Font, TEXT("世界图例 | 蓝=胶囊体 绿=朝向 黄=速度 青=输入/轨迹 紫红=锚点"), FColor::White, InOutY);
}

void UWuwaDebugVisualizationComponent::DrawNetworkSummary(UCanvas& Canvas,
                                                          UFont& Font,
                                                          const AWuwaCharacter& Character,
                                                          float& InOutY) const
{
	const UWorld* World = Character.GetWorld();
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("网络 | Local=%s Remote=%s NetMode=%s Authority=%d LocalControl=%d"),
	                               *GetEnumValueName(Character.GetLocalRole()),
	                               *GetEnumValueName(Character.GetRemoteRole()),
	                               GetNetModeDebugName(IsValid(World) ? World->GetNetMode() : NM_Standalone),
	                               Character.HasAuthority() ? 1 : 0,
	                               Character.IsLocallyControlled() ? 1 : 0),
	               IsValid(World) ? FColor(120, 210, 255) : FColor::Red,
	               InOutY);

	const UWuwaCharacterMovementComponent* MovementComponent = Character.GetWuwaMovementComponent();
	if (!IsValid(MovementComponent))
	{
		DrawCanvasLine(Canvas, Font, TEXT("网络移动 | Movement Component 无效"), FColor::Red, InOutY);
		return;
	}

	const FWuwaNetworkMovementBaselineSnapshot Network = MovementComponent->GetNetworkBaselineSnapshot();
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("网络移动 | Mode=%s MaxSpeed=%.1f Input=%.2f SavedMoves=%d Ack=%.3f"),
	                               *MovementComponent->GetMovementName(),
	                               Network.ResolvedMaxSpeed,
	                               Network.LocalInputMagnitude,
	                               Network.SavedMoveCount,
	                               Network.LastAckedMoveTimestamp),
	               FColor(120, 210, 255),
	               InOutY);
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("网络校正 | Count=%d Last=%.2f Max=%.2f Average=%.2f Rate=%.2f/s"),
	                               Network.CorrectionCount,
	                               Network.LastCorrectionDistance,
	                               Network.MaxCorrectionDistance,
	                               Network.AverageCorrectionDistance,
	                               Network.CorrectionsPerSecond),
	               Network.MaxCorrectionDistance <= 20.f ? FColor(80, 255, 120) : FColor::Yellow,
	               InOutY);

	const UWuwaActionNetworkComponent* ActionNetworkComponent = Character.GetActionNetworkComponent();
	if (!IsValid(ActionNetworkComponent))
	{
		DrawCanvasLine(Canvas, Font, TEXT("动作网络 | 组件无效"), FColor::Red, InOutY);
		return;
	}

	const FWuwaActionNetworkRuntimeSnapshot ActionNetwork = ActionNetworkComponent->GetRuntimeSnapshot();
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("动作网络 | Ready=%d Registry=%d LocalGen=%d AuthorityGen=%d PendingExit=%d"),
	                               ActionNetwork.bInitialized ? 1 : 0,
	                               ActionNetwork.RegistryCount,
	                               ActionNetwork.ActiveLocalGeneration.Value,
	                               ActionNetwork.LastAuthorityGeneration.Value,
	                               ActionNetwork.PendingActionExitCommandCount),
	               ActionNetwork.bInitialized ? FColor(120, 210, 255) : FColor::Yellow,
	               InOutY);
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("动作响应 | Has=%d Gen=%d Accepted=%d Reject=%s"),
	                               ActionNetwork.LastResponse.bHasResponse ? 1 : 0,
	                               ActionNetwork.LastResponse.ProcessedGeneration.Value,
	                               ActionNetwork.LastResponse.bAccepted ? 1 : 0,
	                               *GetEnumValueName(ActionNetwork.LastResponse.RejectReason)),
	               ActionNetwork.LastResponse.bHasResponse && !ActionNetwork.LastResponse.bAccepted
	                   ? FColor::Yellow
	                   : FColor(120, 210, 255),
	               InOutY);

	const USkeletalMeshComponent* MeshComponent = Character.GetMesh();
	UAnimInstance* AnimInstance = IsValid(MeshComponent) ? MeshComponent->GetAnimInstance() : nullptr;
	UAnimMontage* ActiveMontage = IsValid(AnimInstance) ? AnimInstance->GetCurrentActiveMontage() : nullptr;
	const float MontagePosition =
	    IsValid(AnimInstance) && IsValid(ActiveMontage) ? AnimInstance->Montage_GetPosition(ActiveMontage) : 0.f;
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("远端动画 | Gen=%d Active=%d StartCount=%d Montage=%s Position=%.3f"),
	                               ActionNetwork.PresentationGeneration.Value,
	                               ActionNetwork.bPresentationActive ? 1 : 0,
	                               ActionNetwork.PresentationStartCount,
	                               *GetNameSafe(ActiveMontage),
	                               MontagePosition),
	               ActionNetwork.bPresentationActive ? FColor(255, 120, 220) : FColor(180, 180, 180),
	               InOutY);

	const FWuwaGrappleNetworkRuntimeSnapshot& Grapple = ActionNetwork.Grapple;
	DrawCanvasLine(
	    Canvas,
	    Font,
	    FString::Printf(TEXT("钩锁网络 | PredGen=%d AuthGen=%d Scale=%.3f/%.3f Correction=%.2f Reject=%s Lateral=%s"),
	                    Grapple.PredictedGeneration.Value,
	                    Grapple.AuthorityGeneration.Value,
	                    Grapple.PredictedTrajectoryScale,
	                    Grapple.AuthorityTrajectoryScale,
	                    Grapple.LastCorrectionDistance,
	                    *GetEnumValueName(Grapple.LastQueryRejectReason),
	                    *GetEnumValueName(Grapple.LateralInputSource)),
	    Grapple.bCorrectionRequired ? FColor::Yellow : FColor(80, 255, 120),
	    InOutY);

	const UWuwaActionCoordinatorComponent* Coordinator = Character.GetActionCoordinatorComponent();
	if (IsValid(Coordinator))
	{
		const FWuwaActionRuntimeSnapshot Action = Coordinator->GetRuntimeSnapshot();
		DrawCanvasLine(Canvas,
		               Font,
		               FString::Printf(TEXT("独占状态 | Gen=%d Owns=%d Queue=%d"),
		                               Action.ActiveNetworkGeneration.Value,
		                               Action.bOwnsExclusiveStateTag ? 1 : 0,
		                               Action.QueueCount),
		               Action.bOwnsExclusiveStateTag ? FColor(255, 120, 220) : FColor(180, 180, 180),
		               InOutY);
	}
}

void UWuwaDebugVisualizationComponent::DrawAbilitySystemSummary(UCanvas& Canvas,
                                                                UFont& Font,
                                                                const AWuwaCharacter& Character,
                                                                float& InOutY) const
{
	const AWuwaPlayerState* PlayerState = Character.GetPlayerState<AWuwaPlayerState>();
	const UWuwaAbilitySystemComponent* AbilitySystemComponent =
	    IsValid(PlayerState) ? PlayerState->GetWuwaAbilitySystemComponent() : nullptr;
	const UWuwaPawnAbilityInitComponent* InitComponent = Character.GetPawnAbilityInitComponent();
	if (!IsValid(AbilitySystemComponent) || !IsValid(InitComponent))
	{
		DrawCanvasLine(Canvas, Font, TEXT("GAS | PlayerState、ASC 或 InitComponent 无效"), FColor::Red, InOutY);
		return;
	}

	const FWuwaAbilitySystemRuntimeSnapshot ASC = AbilitySystemComponent->GetRuntimeSnapshot();
	const FWuwaPawnAbilityInitRuntimeSnapshot Init = InitComponent->GetRuntimeSnapshot();
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("GAS | Owner=%s Avatar=%s Init=%s Replication=%s"),
	                               *ASC.OwnerActorName,
	                               *ASC.AvatarActorName,
	                               *GetEnumValueName(Init.InitState),
	                               *ASC.ReplicationModeName),
	               FColor(120, 210, 255),
	               InOutY);
	DrawCanvasLine(
	    Canvas,
	    Font,
	    FString::Printf(TEXT("Ability | Granted=%d Active=%d Tags=%s"),
	                    ASC.GrantedAbilityCount,
	                    ASC.ActiveAbilityCount,
	                    ASC.ActiveAbilityTags.IsEmpty() ? TEXT("无") : *ASC.ActiveAbilityTags.ToStringSimple()),
	    FColor(170, 140, 255),
	    InOutY);
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("Effect/Init | ActiveEffects=%d InitFailure=%s GrantGeneration=%d"),
	                               ASC.ActiveGameplayEffectCount,
	                               *GetEnumValueName(Init.LastFailureReason),
	                               Init.AbilitySetGrantGeneration),
	               Init.LastFailureReason == EWuwaPawnAbilityInitFailureReason::None ? FColor(120, 210, 255)
	                                                                                 : FColor::Yellow,
	               InOutY);

	const UWuwaActionAbilityInteropComponent* InteropComponent = Character.GetActionAbilityInteropComponent();
	if (!IsValid(InteropComponent))
	{
		DrawCanvasLine(Canvas, Font, TEXT("Interop | 组件无效"), FColor::Red, InOutY);
		return;
	}

	const FWuwaActionAbilityInteropRuntimeSnapshot Interop = InteropComponent->GetRuntimeSnapshot();
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("Interop | Ready=%d InitReady=%d Attack=%d Mirror=%d Dead=%d Mirror=%d"),
	                               Interop.bInitialized ? 1 : 0,
	                               Init.bActionAbilityInteropReady ? 1 : 0,
	                               Interop.AttackingTagCount,
	                               Interop.bAttackingMirrored ? 1 : 0,
	                               Interop.DeadTagCount,
	                               Interop.bDeadMirrored ? 1 : 0),
	               Interop.bInitialized && Init.bActionAbilityInteropReady ? FColor(80, 255, 120) : FColor::Yellow,
	               InOutY);
}

void UWuwaDebugVisualizationComponent::DrawAbilityInputSummary(UCanvas& Canvas,
                                                               UFont& Font,
                                                               const AWuwaCharacter& Character,
                                                               float& InOutY) const
{
	const UWuwaAbilityInputRouterComponent* InputRouter = Character.GetAbilityInputRouterComponent();
	if (!IsValid(InputRouter))
	{
		DrawCanvasLine(Canvas, Font, TEXT("Ability 输入 | Router 无效"), FColor::Red, InOutY);
		return;
	}

	const FWuwaAbilityInputRouterRuntimeSnapshot Input = InputRouter->GetRuntimeSnapshot();
	DrawCanvasLine(
	    Canvas,
	    Font,
	    FString::Printf(TEXT("Ability 输入 | Tag=%s Sequence=%d Spec=%s Result=%s Failure=%s"),
	                    Input.LastAcceptedInputTag.IsValid() ? *Input.LastAcceptedInputTag.ToString() : TEXT("无"),
	                    Input.LastAcceptedSequence,
	                    *Input.LastActivationSpecHandle.ToString(),
	                    Input.bLastActivationSucceeded ? TEXT("成功") : TEXT("未成功"),
	                    *GetEnumValueName(Input.LastActivationFailure)),
	    Input.LastActivationFailure == EWuwaAbilityActivationDebugFailure::None ? FColor(120, 255, 160)
	                                                                            : FColor::Yellow,
	    InOutY);
}

void UWuwaDebugVisualizationComponent::DrawHealthSummary(UCanvas& Canvas,
                                                         UFont& Font,
                                                         const AWuwaCharacter& Character,
                                                         float& InOutY) const
{
	const AWuwaPlayerState* PlayerState = Character.GetPlayerState<AWuwaPlayerState>();
	const UWuwaAbilitySystemComponent* AbilitySystemComponent =
	    IsValid(PlayerState) ? PlayerState->GetWuwaAbilitySystemComponent() : nullptr;
	const UWuwaHealthComponent* HealthComponent = Character.GetHealthComponent();
	const UWuwaPoiseComponent* PoiseComponent = Character.GetPoiseComponent();
	if (!IsValid(AbilitySystemComponent) || !IsValid(HealthComponent))
	{
		DrawCanvasLine(Canvas, Font, TEXT("属性/死亡 | ASC 或 HealthComponent 无效"), FColor::Red, InOutY);
		return;
	}

	const FWuwaAbilitySystemRuntimeSnapshot ASC = AbilitySystemComponent->GetRuntimeSnapshot();
	const FWuwaHealthRuntimeSnapshot Health = HealthComponent->GetRuntimeSnapshot();
	const FString IncomingDamage =
	    Character.HasAuthority() ? FString::Printf(TEXT("%.1f"), ASC.IncomingDamage) : TEXT("仅 Authority 可见");
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("属性 | Health=%.1f/%.1f Stamina=%.1f/%.1f AP=%.1f Defense=%.1f Incoming=%s"),
	                               ASC.Health,
	                               ASC.MaxHealth,
	                               ASC.Stamina,
	                               ASC.MaxStamina,
	                               ASC.AttackPower,
	                               ASC.Defense,
	                               *IncomingDamage),
	               FColor(255, 190, 80),
	               InOutY);
	DrawCanvasLine(
	    Canvas,
	    Font,
	    FString::Printf(TEXT("死亡 | State=%s DeadTag=%d Sequence=%d Effect=%d Previous=%.1f LastDamage=%.1f"),
	                    *GetEnumValueName(Health.DeathState),
	                    Health.DeadTagCount,
	                    Health.DeathSequence,
	                    Health.bDeadEffectActive ? 1 : 0,
	                    Health.PreviousHealth,
	                    Health.LastDamage),
	    Health.DeathState == EWuwaDeathState::NotDead ? FColor(120, 255, 160) : FColor::Red,
	    InOutY);
	const UWuwaWorldHealthBarComponent* WorldHealthBarComponent = Character.GetWorldHealthBarComponent();
	const FWuwaWorldHealthBarRuntimeSnapshot WorldHealthBar = IsValid(WorldHealthBarComponent)
	                                                              ? WorldHealthBarComponent->GetRuntimeSnapshot()
	                                                              : FWuwaWorldHealthBarRuntimeSnapshot();
	DrawCanvasLine(Canvas,
	               Font,
	               IsValid(WorldHealthBarComponent)
	                   ? FString::Printf(TEXT("头顶血条 | Source=%d Widget=%d ScreenSubmitted=%d Visible=%d "
	                                          "Health=%.1f/%.1f Timer=%d Duration=%.1f LocalHidden=%d"),
	                                     WorldHealthBar.bSourceFound ? 1 : 0,
	                                     WorldHealthBar.bWidgetCreated ? 1 : 0,
	                                     WorldHealthBar.bScreenPresentationSubmitted ? 1 : 0,
	                                     WorldHealthBar.bVisible ? 1 : 0,
	                                     WorldHealthBar.LastCurrentHealth,
	                                     WorldHealthBar.LastMaxHealth,
	                                     WorldHealthBar.bHideTimerActive ? 1 : 0,
	                                     WorldHealthBar.VisibleDuration,
	                                     WorldHealthBar.bHiddenForLocalOwner ? 1 : 0)
	                   : TEXT("头顶血条 | WorldHealthBarComponent 无效"),
	               IsValid(WorldHealthBarComponent) && WorldHealthBar.bSourceFound ? FColor(120, 255, 160)
	                                                                               : FColor::Red,
	               InOutY);
	const FWuwaPoiseRuntimeSnapshot Poise =
	    IsValid(PoiseComponent) ? PoiseComponent->GetRuntimeSnapshot() : FWuwaPoiseRuntimeSnapshot();
	DrawCanvasLine(
	    Canvas,
	    Font,
	    IsValid(PoiseComponent)
	        ? FString::Printf(
	              TEXT("韧性 | Poise=%.1f/%.1f Incoming=%.1f Previous=%.1f LastDamage=%.1f Pending=%d Sequence=%d"),
	              Poise.Poise,
	              Poise.MaxPoise,
	              Poise.IncomingPoiseDamage,
	              Poise.PreviousPoise,
	              Poise.LastPoiseDamage,
	              Poise.bPendingBreak ? 1 : 0,
	              Poise.BreakSequence)
	        : TEXT("韧性 | PoiseComponent 无效"),
	    IsValid(PoiseComponent) && Poise.bInitialized ? FColor(255, 190, 80) : FColor::Red,
	    InOutY);
}

void UWuwaDebugVisualizationComponent::DrawCombatSummary(UCanvas& Canvas,
                                                         UFont& Font,
                                                         const AWuwaCharacter& Character,
                                                         float& InOutY) const
{
	const UWuwaWeaponComponent* WeaponComponent = Character.GetWeaponComponent();
	const FWuwaWeaponRuntimeSnapshot Weapon =
	    IsValid(WeaponComponent) ? WeaponComponent->GetRuntimeSnapshot() : FWuwaWeaponRuntimeSnapshot();
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("武器 | Definition=%s Attach=%s Placeholder=%d"),
	                               *Weapon.WeaponDefinitionName,
	                               *Weapon.CharacterAttachSocket.ToString(),
	                               Weapon.bIsPlaceholder ? 1 : 0),
	               Weapon.bInitialized ? FColor(255, 210, 90) : FColor::Red,
	               InOutY);
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("剑刃 | Base=%d %s Tip=%d %s"),
	                               Weapon.bTraceBaseSocketValid ? 1 : 0,
	                               *Weapon.TraceBaseWorldPosition.ToCompactString(),
	                               Weapon.bTraceTipSocketValid ? 1 : 0,
	                               *Weapon.TraceTipWorldPosition.ToCompactString()),
	               Weapon.bTraceBaseSocketValid && Weapon.bTraceTipSocketValid ? FColor(255, 210, 90) : FColor::Red,
	               InOutY);

	const AWuwaPlayerState* PlayerState = Character.GetPlayerState<AWuwaPlayerState>();
	const UWuwaAbilitySystemComponent* AbilitySystemComponent =
	    IsValid(PlayerState) ? PlayerState->GetWuwaAbilitySystemComponent() : nullptr;
	const UWuwaGameplayAbility_MeleeAttack* MeleeAbility = nullptr;
	const UWuwaGameplayAbility_Stagger* StaggerAbility = nullptr;
	if (IsValid(AbilitySystemComponent))
	{
		for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
		{
			if (!IsValid(MeleeAbility))
			{
				MeleeAbility = Cast<UWuwaGameplayAbility_MeleeAttack>(Spec.GetPrimaryInstance());
				if (!IsValid(MeleeAbility))
				{
					MeleeAbility = Cast<UWuwaGameplayAbility_MeleeAttack>(Spec.Ability);
				}
			}
			if (!IsValid(StaggerAbility))
			{
				StaggerAbility = Cast<UWuwaGameplayAbility_Stagger>(Spec.GetPrimaryInstance());
				if (!IsValid(StaggerAbility))
				{
					StaggerAbility = Cast<UWuwaGameplayAbility_Stagger>(Spec.Ability);
				}
			}
			if (IsValid(MeleeAbility) && IsValid(StaggerAbility))
			{
				break;
			}
		}
	}

	const UWuwaPawnAbilityInitComponent* InitComponent = Character.GetPawnAbilityInitComponent();
	const UWuwaCombatProfile* CombatProfile = IsValid(InitComponent) ? InitComponent->GetCombatProfile() : nullptr;
	const UWuwaMeleeAttackDefinition* AttackDefinition =
	    IsValid(CombatProfile) ? CombatProfile->GetDefaultAttackDefinition() : nullptr;
	const FWuwaMeleeAttackRuntimeSnapshot Attack =
	    IsValid(MeleeAbility) ? MeleeAbility->GetRuntimeSnapshot() : FWuwaMeleeAttackRuntimeSnapshot();
	const FWuwaStaggerRuntimeSnapshot Stagger =
	    IsValid(StaggerAbility) ? StaggerAbility->GetRuntimeSnapshot() : FWuwaStaggerRuntimeSnapshot();
	const UWuwaActionAbilityInteropComponent* InteropComponent = Character.GetActionAbilityInteropComponent();
	const FWuwaActionAbilityInteropRuntimeSnapshot Interop =
	    IsValid(InteropComponent) ? InteropComponent->GetRuntimeSnapshot() : FWuwaActionAbilityInteropRuntimeSnapshot();
	const UWuwaCombatExecutionComponent* CombatExecution = Character.GetCombatExecutionComponent();
	const FWuwaCombatExecutionRuntimeSnapshot Execution =
	    IsValid(CombatExecution) ? CombatExecution->GetRuntimeSnapshot() : FWuwaCombatExecutionRuntimeSnapshot();
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("轻攻击 | Tag=%s Spec=%s Prediction=%s"),
	                               Attack.AttackTag.IsValid() ? *Attack.AttackTag.ToString() : TEXT("无"),
	                               *Attack.AbilitySpecHandle,
	                               *Attack.PredictionKeySummary),
	               IsValid(MeleeAbility) ? FColor(255, 170, 80) : FColor::Red,
	               InOutY);
	DrawCanvasLine(
	    Canvas,
	    Font,
	    FString::Printf(TEXT("攻击表现 | Definition=%s Montage=%s Active=%d Window=%s"),
	                    *GetNameSafe(AttackDefinition),
	                    IsValid(AttackDefinition) ? *GetNameSafe(AttackDefinition->GetMontage()) : TEXT("None"),
	                    Attack.bActive ? 1 : 0,
	                    *GetEnumValueName(Attack.WindowState)),
	    IsValid(AttackDefinition) && IsValid(MeleeAbility) ? FColor(255, 170, 80) : FColor::Red,
	    InOutY);
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("硬直 | Active=%d End=%s Effect=%d Montage=%s"),
	                               Stagger.bActive ? 1 : 0,
	                               *GetEnumValueName(Stagger.LastEndReason),
	                               Stagger.bStateEffectActive ? 1 : 0,
	                               *GetNameSafe(Stagger.Montage)),
	               IsValid(StaggerAbility) ? FColor(255, 120, 120) : FColor::Yellow,
	               InOutY);
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("硬直互操作 | ASC=%d/%d Mirror=%d/%d Invalidate=%d Reset=%d"),
	                               Stagger.StateTagCount,
	                               Stagger.MoveBlockTagCount,
	                               Interop.bStaggeredMirrored ? 1 : 0,
	                               Interop.bMoveBlockMirrored ? 1 : 0,
	                               Interop.StaggerInvalidationCount,
	                               Stagger.PoiseResetCount),
	               Interop.bInitialized ? FColor(255, 120, 120) : FColor::Yellow,
	               InOutY);
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("攻击窗口 | Begin=%d End=%d LastEnd=%s Sweep=%d Damage=%d Rejected=%d"),
	                               Attack.HitWindowBeginCount,
	                               Attack.HitWindowEndCount,
	                               *GetEnumValueName(Attack.LastEndReason),
	                               Attack.SweepCount,
	                               Attack.DamageApplicationCount,
	                               Attack.RejectedDamageApplicationCount),
	               FColor(255, 170, 80),
	               InOutY);
	const auto ReadStepCount = [](const TArray<int32>& Values, const int32 StepIndex)
	{
		return Values.IsValidIndex(StepIndex) ? Values[StepIndex] : 0;
	};
	const auto ReadStepHandle = [](const TArray<FWuwaCombatWindowHandle>& Values, const int32 StepIndex)
	{
		return Values.IsValidIndex(StepIndex) ? Values[StepIndex].Value : 0u;
	};
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("连招 | Step=%d/3 Current=%s Next=%s Open=%d Buffer=%d/1"),
	                               Attack.CurrentStepIndex >= 0 ? Attack.CurrentStepIndex + 1 : 0,
	                               *Attack.CurrentSection.ToString(),
	                               *Attack.NextSection.ToString(),
	                               Attack.bComboWindowOpen ? 1 : 0,
	                               Attack.BufferedInputCount),
	               FColor(255, 170, 80),
	               InOutY);
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("连招输入 | Accepted=%d Rejected=%d Last=%s"),
	                               Attack.AcceptedComboInputCount,
	                               Attack.RejectedComboInputCount,
	                               *GetEnumValueName(Attack.LastComboInputRejectReason)),
	               FColor(255, 170, 80),
	               InOutY);
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("逐段窗口 | Begin=%d/%d/%d End=%d/%d/%d Handle=%u/%u/%u"),
	                               ReadStepCount(Attack.PerStepHitWindowBeginCounts, 0),
	                               ReadStepCount(Attack.PerStepHitWindowBeginCounts, 1),
	                               ReadStepCount(Attack.PerStepHitWindowBeginCounts, 2),
	                               ReadStepCount(Attack.PerStepHitWindowEndCounts, 0),
	                               ReadStepCount(Attack.PerStepHitWindowEndCounts, 1),
	                               ReadStepCount(Attack.PerStepHitWindowEndCounts, 2),
	                               ReadStepHandle(Attack.PerStepCombatWindowHandles, 0),
	                               ReadStepHandle(Attack.PerStepCombatWindowHandles, 1),
	                               ReadStepHandle(Attack.PerStepCombatWindowHandles, 2)),
	               FColor(255, 170, 80),
	               InOutY);
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("逐段命中 | Hit=%d/%d/%d Damage=%d/%d/%d"),
	                               ReadStepCount(Attack.PerStepHitFactCounts, 0),
	                               ReadStepCount(Attack.PerStepHitFactCounts, 1),
	                               ReadStepCount(Attack.PerStepHitFactCounts, 2),
	                               ReadStepCount(Attack.PerStepDamageApplicationCounts, 0),
	                               ReadStepCount(Attack.PerStepDamageApplicationCounts, 1),
	                               ReadStepCount(Attack.PerStepDamageApplicationCounts, 2)),
	               FColor(255, 170, 80),
	               InOutY);
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("命中执行 | Role=%s Handle=%u Active=%d Attack=%s Radius=%.1f Samples=%d"),
	                               *GetEnumValueName(Character.GetLocalRole()),
	                               Execution.ActiveWindowHandle.Value,
	                               Execution.bWindowActive ? 1 : 0,
	                               Execution.AttackTag.IsValid() ? *Execution.AttackTag.ToString() : TEXT("无"),
	                               Execution.TraceRadius,
	                               Execution.BladeSampleCount),
	               Execution.bInitialized ? FColor(100, 220, 255) : FColor::Red,
	               InOutY);
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("命中状态 | Sweep=%d Hit=%d/%d LastTarget=%s End=%s Failure=%s"),
	                               Execution.SweepCount,
	                               Execution.HitCount,
	                               Execution.MaxTargets,
	                               *GetActorDebugName(Execution.LastHitFact.TargetActor.Get()),
	                               *GetEnumValueName(Execution.LastEndReason),
	                               *GetEnumValueName(Execution.LastFailureReason)),
	               Execution.LastFailureReason == EWuwaCombatWindowFailureReason::None ? FColor(100, 220, 255)
	                                                                                   : FColor::Yellow,
	               InOutY);
	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("Mesh Tick | Original=%u Current=%u Elevated=%d"),
	                               Execution.OriginalMeshTickOption,
	                               Execution.CurrentMeshTickOption,
	                               Execution.bMeshTickOptionElevated ? 1 : 0),
	               Execution.LastFailureReason == EWuwaCombatWindowFailureReason::None ? FColor(100, 220, 255)
	                                                                                   : FColor::Yellow,
	               InOutY);
}

void UWuwaDebugVisualizationComponent::DrawCombatExecutionWorld(const AWuwaCharacter& Character) const
{
	const UWorld* World = Character.GetWorld();
	const UWuwaCombatExecutionComponent* CombatExecution = Character.GetCombatExecutionComponent();
	if (!IsValid(World) || !IsValid(CombatExecution))
	{
		return;
	}

	const FWuwaCombatExecutionRuntimeSnapshot Execution = CombatExecution->GetRuntimeSnapshot();
	if (!Execution.bWindowActive || Execution.PreviousBase.ContainsNaN() || Execution.PreviousTip.ContainsNaN() ||
	    Execution.CurrentBase.ContainsNaN() || Execution.CurrentTip.ContainsNaN())
	{
		return;
	}

	DrawDebugLine(World, Execution.PreviousBase, Execution.PreviousTip, FColor::Blue, false, -1.0f, 0, 2.0f);
	DrawDebugLine(World, Execution.CurrentBase, Execution.CurrentTip, FColor::Cyan, false, -1.0f, 0, 3.0f);
	DrawDebugLine(World, Execution.PreviousBase, Execution.CurrentBase, FColor::Green, false, -1.0f, 0, 1.5f);
	DrawDebugLine(World, Execution.PreviousTip, Execution.CurrentTip, FColor::Green, false, -1.0f, 0, 1.5f);
	DrawDebugSphere(World, Execution.CurrentBase, Execution.TraceRadius, 12, FColor::Cyan, false, -1.0f, 0, 1.5f);
	DrawDebugSphere(World, Execution.CurrentTip, Execution.TraceRadius, 12, FColor::Cyan, false, -1.0f, 0, 1.5f);
}

void UWuwaDebugVisualizationComponent::DrawMovementWorld(const AWuwaCharacter& Character) const
{
	const UWorld* World = Character.GetWorld();
	const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();

	if (!IsValid(World) || !IsValid(Capsule))
	{
		return;
	}

	const FVector Origin = Character.GetActorLocation() + FVector(0.0, 0.0, 20.0);

	DrawDebugCapsule(World,
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
		    World, Origin, Origin + FacingDirection * 160.0, 24.0f, FColor::Green, false, -1.0f, 0, 2.5f);
	}

	const FVector Velocity = Character.GetVelocity();

	if (!Velocity.IsNearlyZero() && !Velocity.ContainsNaN())
	{
		const float VelocityArrowLength = FMath::Clamp(Velocity.Size() * 0.15f, 60.0f, 300.0f);

		DrawDebugDirectionalArrow(World,
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

		DrawDebugDirectionalArrow(World,
		                          Origin,
		                          Origin + InputWorldDirection.GetSafeNormal2D() * InputArrowLength,
		                          24.0f,
		                          FColor::Cyan,
		                          false,
		                          -1.0f,
		                          0,
		                          2.5f);
	}

	const UWuwaTraversalActionIntentProviderComponent* GrappleQuery =
	    Character.FindComponentByClass<UWuwaTraversalActionIntentProviderComponent>();
	if (!IsValid(GrappleQuery) || !GrappleQuery->GetDebugSnapshot().bHasQuery)
	{
		return;
	}

	const FWuwaGrappleQueryDebugSnapshot& Grapple = GrappleQuery->GetDebugSnapshot();
	const FWuwaGrappleQueryResult& Query = Grapple.QueryResult;
	DrawDebugSphere(World, Query.ResolvedVisualAnchorLocation, 24.f, 12, FColor(255, 80, 255), false, -1.f, 0, 2.f);
	DrawDebugCapsule(World,
	                 Query.ResolvedReleaseLocation,
	                 28.f,
	                 16.f,
	                 FQuat::Identity,
	                 Query.bSucceeded ? FColor::Green : FColor::Red,
	                 false,
	                 -1.f,
	                 0,
	                 2.f);

	for (int32 PointIndex = 1; PointIndex < Grapple.PredictedTrajectoryPoints.Num(); ++PointIndex)
	{
		DrawDebugLine(World,
		              Grapple.PredictedTrajectoryPoints[PointIndex - 1],
		              Grapple.PredictedTrajectoryPoints[PointIndex],
		              FColor::Cyan,
		              false,
		              -1.f,
		              0,
		              2.f);
	}
}

void UWuwaDebugVisualizationComponent::DrawTargetingCanvas(UCanvas& Canvas,
                                                           UFont& Font,
                                                           APlayerController& PlayerController,
                                                           float& InOutY) const
{
	AWuwaCharacter* Character = ResolveCharacter(PlayerController);

	if (!IsValid(Character))
	{
		DrawCanvasLine(Canvas, Font, TEXT("锁定 | 未找到玩家角色"), FColor::Red, InOutY);
		return;
	}

	const UWuwaTargetingComponent* Targeting = Character->GetTargetingComponent();

	if (!IsValid(Targeting))
	{
		DrawCanvasLine(Canvas, Font, TEXT("锁定 | 组件无效"), FColor::Red, InOutY);
		return;
	}

	const FWuwaTargetContext Context = Targeting->GetTargetContext();
	const AActor* TargetActor = Context.TargetActor.Get();
	const bool bHasValidTarget = Context.HasValidTarget();
	const FColor ModeColor = GetTargetModeColor(Context.Mode);

	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("锁定 | 模式=%s 有效=%d 目标=%s 修订=%d"),
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
	        TEXT("目标点 | X=%.1f Y=%.1f Z=%.1f"), Context.TargetPoint.X, Context.TargetPoint.Y, Context.TargetPoint.Z),
	    ModeColor,
	    InOutY);

	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("评分 | 距离=%.1f 角度=%.1f 总分=%.3f 可见=%d"),
	                               Context.Score.Distance,
	                               Context.Score.ViewAngleDegrees,
	                               Context.Score.TotalScore,
	                               Context.Score.bVisible ? 1 : 0),
	               FColor(255, 170, 60),
	               InOutY);

	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("评分明细 | 距离=%.3f 视角=%.3f 保留=%.3f 视空间=(%.3f, %.3f)"),
	                               Context.Score.DistanceScore,
	                               Context.Score.ViewAlignmentScore,
	                               Context.Score.RetentionBonus,
	                               Context.Score.ViewSpaceHorizontal,
	                               Context.Score.ViewSpaceVertical),
	               FColor(255, 170, 60),
	               InOutY);

	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("最近失败 | %s"), *GetEnumValueName(Targeting->GetLastFailureReason())),
	               Targeting->GetLastFailureReason() == EWuwaTargetingFailureReason::None ? FColor(180, 180, 180)
	                                                                                      : FColor::Red,
	               InOutY);

	DrawCanvasLine(Canvas,
	               Font,
	               TEXT("世界图例 | 黄=软锁定目标球/目标框/连线 红=硬锁定目标球/目标框/连线"),
	               FColor::White,
	               InOutY);
}

void UWuwaDebugVisualizationComponent::DrawTargetingWorld(const AWuwaCharacter& Character) const
{
	const UWorld* World = Character.GetWorld();
	const UWuwaTargetingComponent* Targeting = Character.GetTargetingComponent();

	if (!IsValid(World) || !IsValid(Targeting))
	{
		return;
	}

	const FWuwaTargetContext Context = Targeting->GetTargetContext();

	if (!Context.HasValidTarget() || !IsFiniteVector(Context.TargetPoint))
	{
		return;
	}

	const FColor TargetColor = GetTargetModeColor(Context.Mode);
	const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	if (IsValid(Capsule) && IsFiniteVector(Capsule->GetComponentLocation()))
	{
		DrawDebugLine(World, Capsule->GetComponentLocation(), Context.TargetPoint, TargetColor, false, -1.f, 0, 1.5f);
	}

	DrawDebugSphere(World, Context.TargetPoint, 28.0f, 16, TargetColor, false, -1.0f, 0, 1.5f);

	const AActor* TargetActor = Context.TargetActor.Get();
	if (IsValid(TargetActor))
	{
		FVector BoundsOrigin = FVector::ZeroVector;
		FVector BoundsExtent = FVector::ZeroVector;
		TargetActor->GetActorBounds(false, BoundsOrigin, BoundsExtent, true);
		if (IsFiniteVector(BoundsOrigin) && IsFiniteVector(BoundsExtent))
		{
			DrawDebugBox(World, BoundsOrigin, BoundsExtent, TargetColor, false, -1.f, 0, 1.5f);
		}
	}
}

void UWuwaDebugVisualizationComponent::DrawCameraCanvas(UCanvas& Canvas,
                                                        UFont& Font,
                                                        APlayerController& PlayerController,
                                                        float& InOutY) const
{
	AWuwaCharacter* Character = ResolveCharacter(PlayerController);
	if (!IsValid(Character))
	{
		DrawCanvasLine(Canvas, Font, TEXT("镜头 | 未找到玩家角色"), FColor::Red, InOutY);
		return;
	}

	const UWuwaCameraModeComponent* CameraMode = Character->GetCameraModeComponent();
	const UWuwaSpringArmComponent* CameraBoom = Character->FindComponentByClass<UWuwaSpringArmComponent>();
	const UCameraComponent* FollowCamera = Character->FindComponentByClass<UCameraComponent>();
	if (!IsValid(CameraMode) || !IsValid(CameraBoom) || !IsValid(FollowCamera))
	{
		DrawCanvasLine(Canvas,
		               Font,
		               FString::Printf(TEXT("镜头 | 状态无效 模式组件=%d 镜头臂=%d 相机=%d"),
		                               IsValid(CameraMode) ? 1 : 0,
		                               IsValid(CameraBoom) ? 1 : 0,
		                               IsValid(FollowCamera) ? 1 : 0),
		               FColor::Red,
		               InOutY);
		return;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	PlayerController.GetPlayerViewPoint(ViewLocation, ViewRotation);
	const bool bValidView =
	    IsFiniteVector(ViewLocation) && IsFiniteRotator(ViewRotation) && FMath::IsFinite(FollowCamera->FieldOfView);
	const bool bValidRuntime = CameraMode->IsInitialized() && CameraMode->GetActiveModeConfig() != nullptr;
	const FGameplayTag ActiveModeTag = CameraMode->GetActiveModeTag();
	FVector CharacterAnchorLocation = FVector::ZeroVector;
	const bool bValidCharacterAnchor = ResolveCharacterMeshAnchor(*Character, CharacterAnchorLocation);
	const FVector SpringArmPivotLocation = CameraBoom->GetComponentLocation() + CameraBoom->TargetOffset;
	const FVector ActualCameraLocation = FollowCamera->GetComponentLocation();
	const float ActualArmLength = FVector::Distance(SpringArmPivotLocation, ActualCameraLocation);
	const FVector ViewForward = ViewRotation.Vector().GetSafeNormal();
	const float AnchorDepth = FVector::DotProduct(CharacterAnchorLocation - ViewLocation, ViewForward);
	const FVector FrameCenterLocation = ViewLocation + ViewForward * AnchorDepth;
	const float SphereSeparation = FVector::Distance(CharacterAnchorLocation, FrameCenterLocation);
	const bool bValidComposition = bValidView && bValidCharacterAnchor && IsFiniteVector(CharacterAnchorLocation) &&
	                               IsFiniteVector(ActualCameraLocation) && !ViewForward.IsNearlyZero() &&
	                               IsFiniteVector(FrameCenterLocation) && FMath::IsFinite(ActualArmLength) &&
	                               FMath::IsFinite(SphereSeparation) && AnchorDepth > KINDA_SMALL_NUMBER;

	DrawCanvasLine(
	    Canvas,
	    Font,
	    FString::Printf(TEXT("镜头 | 模式=%s FOV=%.1f 实际臂长=%.1f"),
	                    ActiveModeTag.IsValid() ? *GetDebugValueDisplayName(ActiveModeTag.ToString()) : TEXT("无效"),
	                    FollowCamera->FieldOfView,
	                    ActualArmLength),
	    bValidRuntime && bValidComposition ? FColor(80, 255, 120) : FColor::Red,
	    InOutY);

	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("角色锚点 | 骨骼=neck_01 位置=%s"), *CharacterAnchorLocation.ToCompactString()),
	               bValidComposition ? FColor(80, 200, 255) : FColor::Red,
	               InOutY);

	DrawCanvasLine(Canvas,
	               Font,
	               FString::Printf(TEXT("双球 | 画面中心=(%.0f, %.0f) 锚点间距=%.1f"),
	                               Canvas.SizeX * 0.5f,
	                               Canvas.SizeY * 0.5f,
	                               SphereSeparation),
	               bValidComposition ? FColor(255, 190, 40) : FColor::Red,
	               InOutY);

	const UWuwaTargetingComponent* Targeting = Character->GetTargetingComponent();
	const FWuwaTargetContext TargetContext = IsValid(Targeting) ? Targeting->GetTargetContext() : FWuwaTargetContext();
	if (!TargetContext.HasValidTarget() || !bValidView)
	{
		DrawCanvasLine(Canvas, Font, TEXT("锁定构图 | 无有效目标"), FColor(180, 180, 180), InOutY);
	}
	else
	{
		FVector2D TargetScreen = FVector2D::ZeroVector;
		const bool bProjected =
		    PlayerController.ProjectWorldLocationToScreen(TargetContext.TargetPoint, TargetScreen, false);
		const FVector2D ScreenCenter(Canvas.SizeX * 0.5f, Canvas.SizeY * 0.5f);
		const FVector2D NormalizedScreenOffset = bProjected && Canvas.SizeX > 0.f && Canvas.SizeY > 0.f
		                                             ? FVector2D((TargetScreen.X - ScreenCenter.X) / ScreenCenter.X,
		                                                         (TargetScreen.Y - ScreenCenter.Y) / ScreenCenter.Y)
		                                             : FVector2D::ZeroVector;

		DrawCanvasLine(Canvas,
		               Font,
		               FString::Printf(TEXT("锁定构图 | 目标屏幕偏移=(%.2f, %.2f)"),
		                               NormalizedScreenOffset.X,
		                               NormalizedScreenOffset.Y),
		               bProjected ? FColor(80, 255, 120) : FColor::Red,
		               InOutY);
	}

	DrawCanvasLine(Canvas, Font, TEXT("世界图例 | 天蓝=角色锚点 橙黄=画面中心"), FColor::White, InOutY);
}

void UWuwaDebugVisualizationComponent::DrawCameraCompositionSpheres(const AWuwaCharacter& Character,
                                                                    const APlayerController& PlayerController) const
{
	const UWorld* World = Character.GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const auto ReportInvalidComposition = [this, World](const TCHAR* Reason)
	{
		const double CurrentTime = World->GetTimeSeconds();
		if (CurrentTime < NextCameraCompositionWarningTime)
		{
			return;
		}

		NextCameraCompositionWarningTime = CurrentTime + 5.0;
		UE_LOG(LogWuwa, Warning, TEXT("镜头 Debug 双球跳过绘制：%s"), Reason);
	};

	const UCameraComponent* FollowCamera = Character.FindComponentByClass<UCameraComponent>();
	if (!IsValid(FollowCamera))
	{
		ReportInvalidComposition(TEXT("相机组件无效"));
		return;
	}

	FVector CharacterAnchorLocation = FVector::ZeroVector;
	if (!ResolveCharacterMeshAnchor(Character, CharacterAnchorLocation))
	{
		ReportInvalidComposition(TEXT("角色模型 neck_01 锚点无效"));
		return;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	PlayerController.GetPlayerViewPoint(ViewLocation, ViewRotation);

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PlayerController.GetViewportSize(ViewportWidth, ViewportHeight);

	const FVector ViewForward = ViewRotation.Vector().GetSafeNormal();
	const float AnchorDepth = FVector::DotProduct(CharacterAnchorLocation - ViewLocation, ViewForward);
	const FVector FrameCenterLocation = ViewLocation + ViewForward * AnchorDepth;
	float CharacterWorldRadius = 0.f;
	float FrameCenterWorldRadius = 0.f;
	constexpr float ReferenceHorizontalFieldOfView = 90.f;
	constexpr float ReferenceViewDepth = 400.f;
	if (!IsFiniteVector(ViewLocation) || !IsFiniteRotator(ViewRotation) || ViewForward.IsNearlyZero() ||
	    !IsFiniteVector(CharacterAnchorLocation) || !IsFiniteVector(FrameCenterLocation) ||
	    !CalculateScreenSpaceSphereRadius(
	        FollowCamera->FieldOfView, ViewportWidth, ViewportHeight, AnchorDepth, CharacterWorldRadius) ||
	    !CalculateScreenSpaceSphereRadius(
	        ReferenceHorizontalFieldOfView, ViewportWidth, ViewportHeight, ReferenceViewDepth, FrameCenterWorldRadius))
	{
		ReportInvalidComposition(TEXT("视点、视口、FOV 或角色锚点深度无效"));
		return;
	}

	DrawDebugSphere(
	    World, CharacterAnchorLocation, CharacterWorldRadius, 16, FColor(80, 200, 255), false, -1.f, 0, 1.5f);
	DrawDebugSphere(World, FrameCenterLocation, FrameCenterWorldRadius, 16, FColor(255, 190, 40), false, -1.f, 0, 1.5f);
}

bool UWuwaDebugVisualizationComponent::ResolveCharacterMeshAnchor(const AWuwaCharacter& Character,
                                                                  FVector& OutAnchorLocation)
{
	OutAnchorLocation = FVector::ZeroVector;
	static const FName CharacterMeshAnchorName(TEXT("neck_01"));
	const USkeletalMeshComponent* CharacterMesh = Character.GetMesh();
	if (!IsValid(CharacterMesh) || (CharacterMesh->GetBoneIndex(CharacterMeshAnchorName) == INDEX_NONE &&
	                                !CharacterMesh->DoesSocketExist(CharacterMeshAnchorName)))
	{
		return false;
	}

	OutAnchorLocation = CharacterMesh->GetSocketLocation(CharacterMeshAnchorName);
	return IsFiniteVector(OutAnchorLocation);
}

bool UWuwaDebugVisualizationComponent::CalculateScreenSpaceSphereRadius(const float HorizontalFieldOfView,
                                                                        const int32 ViewportWidth,
                                                                        const int32 ViewportHeight,
                                                                        const float ViewDepth,
                                                                        float& OutWorldRadius)
{
	OutWorldRadius = 0.f;
	if (!FMath::IsFinite(HorizontalFieldOfView) || HorizontalFieldOfView <= 5.f || HorizontalFieldOfView >= 170.f ||
	    ViewportWidth <= 0 || ViewportHeight <= 0 || !FMath::IsFinite(ViewDepth) || ViewDepth <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	constexpr float ScreenRadiusFraction = 0.06f;
	const float AspectRatio = static_cast<float>(ViewportWidth) / static_cast<float>(ViewportHeight);
	const float HorizontalTangent = FMath::Tan(FMath::DegreesToRadians(HorizontalFieldOfView * 0.5f));
	const float VerticalTangent = HorizontalTangent / AspectRatio;
	OutWorldRadius = ViewDepth * VerticalTangent * ScreenRadiusFraction * 2.f;
	return FMath::IsFinite(OutWorldRadius) && OutWorldRadius > KINDA_SMALL_NUMBER;
}

AWuwaCharacter* UWuwaDebugVisualizationComponent::ResolveCharacter(const APlayerController& PlayerController) const
{
	return Cast<AWuwaCharacter>(PlayerController.GetPawn());
}

void UWuwaDebugVisualizationComponent::DrawCanvasLine(
    UCanvas& Canvas, UFont& Font, const FString& Text, const FColor& Color, float& InOutY)
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

bool UWuwaDebugVisualizationComponent::IncludesCamera(const EWuwaDebugVisualizationMode Mode)
{
	return Mode == EWuwaDebugVisualizationMode::Camera || Mode == EWuwaDebugVisualizationMode::All;
}

bool UWuwaDebugVisualizationComponent::RequiresWorldTick(const EWuwaDebugVisualizationMode Mode)
{
	return IncludesMovement(Mode) || IncludesTargeting(Mode) || IncludesCamera(Mode);
}

const TCHAR* UWuwaDebugVisualizationComponent::GetModeDisplayName(const EWuwaDebugVisualizationMode Mode)
{
	switch (Mode)
	{
		case EWuwaDebugVisualizationMode::Movement:
			return TEXT("移动");

		case EWuwaDebugVisualizationMode::Targeting:
			return TEXT("锁定");

		case EWuwaDebugVisualizationMode::Camera:
			return TEXT("镜头");

		case EWuwaDebugVisualizationMode::All:
			return TEXT("全部");

		case EWuwaDebugVisualizationMode::Disabled:
		default:
			return TEXT("关闭");
	}
}
