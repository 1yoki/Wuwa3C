#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "GameplayTagContainer.h"
#include "WuwaMovementActionTypes.generated.h"

/** Movement Action 使用的主位移驱动 */
UENUM(BlueprintType)
enum class EWuwaMovementActionDriver : uint8
{
	/** 使用自定义空中跳跃原语 */
	AirJump,

	/** 使用 Root Motion Source 推动 Capsule */
	RootMotionSource
};

/** Movement Action 活动期间的朝向策略 */
UENUM(BlueprintType)
enum class EWuwaActionFacingPolicy : uint8
{
	/** 对齐输入边沿解析出的世界方向 */
	FaceContextDirection,

	/** 保持 Action 开始时朝向 */
	PreserveStartingFacing,

	/** 不取得朝向覆盖 */
	UseLocomotion
};

/** 空中跳跃原语的可复用变体 */
UENUM(BlueprintType)
enum class EWuwaAirJumpVariant : uint8
{
	/** 使用 Context 世界方向 */
	Directional,

	/** 使用 Context 中的背向方向 */
	Backflip
};

/** Movement Action 完成后的可选出口策略 */
UENUM(BlueprintType)
enum class EWuwaMovementActionExitPolicy : uint8
{
	/** 不执行额外出口行为 */
	None,

	/** 标签释放后根据最新 MoveIntent 尝试进入 SprintRun */
	TryEnterSprintRun
};

/** Movement Capability 对 Action Event 的可复用响应 */
UENUM(BlueprintType)
enum class EWuwaMovementActionEventResponse : uint8
{
	/** 打开 MoveIntent 取消窗口 */
	OpenMoveCancelWindow,

	/** 请求正常完成并记录出口策略 */
	RequestCompletedExit
};

/** Root Motion Source 位移的只读配置 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaRootMotionSourceConfig
{
	GENERATED_BODY()

	/** Capsule 水平移动距离 */
	UPROPERTY(EditAnywhere,
	          BlueprintReadOnly,
	          Category = "Root Motion Source",
	          meta = (ClampMin = "0.01", Units = "cm"))
	float Distance = 500.f;

	/** 到达目标位置所需时间 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Root Motion Source", meta = (ClampMin = "0.01", Units = "s"))
	float Duration = 0.35f;

	/** 标准化时间到标准化位移进度的可选映射 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Root Motion Source")
	TObjectPtr<UCurveFloat> TimeMappingCurve = nullptr;

	/** @return 数值是否可以安全用于 RMS */
	bool IsRuntimeValid() const
	{
		return FMath::IsFinite(Distance) && Distance > 0.f && FMath::IsFinite(Duration) && Duration > 0.f;
	}
};

/** Action Event 到 Movement 响应策略的配置项 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaMovementActionEventBinding
{
	GENERATED_BODY()

	/** Animation 或 Gameplay 发布的 Action Event */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement Action", meta = (Categories = "Action.Event"))
	FGameplayTag EventTag;

	/** 命中事件后执行的 Movement 响应 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement Action")
	EWuwaMovementActionEventResponse Response = EWuwaMovementActionEventResponse::OpenMoveCancelWindow;
};
