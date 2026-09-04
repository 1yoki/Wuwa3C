#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Messaging/WuwaMessageTypes.h"
#include "WuwaInputTypes.generated.h"

/** 输入边沿对应的触发阶段 */
UENUM(BlueprintType)
enum class EWuwaInputCommandTrigger : uint8
{
	/** 按键或动作开始 */
	Pressed,

	/** 按键或动作结束 */
	Released
};

/*
 * 一次离散输入边沿的不可变快照。
 * Move/Look 等持续状态仍保留在 FWuwaInputIntent 中，不进入普通 FIFO。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaInputCommand
{
	GENERATED_BODY()

	/** 消息顺序与来源 */
	UPROPERTY(BlueprintReadOnly, Category = "Input Command")
	FWuwaMessageHeader Header;

	UPROPERTY(BlueprintReadOnly, Category = "Input Command")
	FGameplayTag InputTag;

	/** 本命令来自按下还是释放边沿 */
	UPROPERTY(BlueprintReadOnly, Category = "Input Command")
	EWuwaInputCommandTrigger Trigger = EWuwaInputCommandTrigger::Pressed;

	UPROPERTY(BlueprintReadOnly, Category = "Input Command")
	double PressedAt = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Input Command", meta = (ClampMin = "0.01", Units = "s"))
	float ValidDuration = 0.10f;

	// 按键边沿发生时的 WASD 快照；入队后不再随持续输入变化。
	UPROPERTY(BlueprintReadOnly, Category = "Input Command")
	FVector2D Direction = FVector2D::ZeroVector;

	bool IsValid() const
	{
		return Header.IsValid() && InputTag.IsValid() && PressedAt >= 0.0 && ValidDuration > 0.f;
	}

	double GetExpireAt() const
	{
		return PressedAt + static_cast<double>(ValidDuration);
	}

	float GetRemainingTime(const double CurrentTime) const
	{
		return static_cast<float>(FMath::Max(0.f, static_cast<float>(GetExpireAt() - CurrentTime)));
	}
};

/** PlayerController 每帧向 Character 提交的完整输入快照 */
USTRUCT()
struct WUWA_API FWuwaInputFrame
{
	GENERATED_BODY()

	/** 当前帧编号 */
	UPROPERTY()
	int64 FrameNumber = 0;

	/** 当前帧 World 时间 */
	UPROPERTY()
	double CapturedAt = 0.0;

	/** 本帧输入处理间隔 */
	UPROPERTY()
	float DeltaTime = 0.f;

	/** 本帧是否处于游戏暂停状态 */
	UPROPERTY()
	bool bGamePaused = false;

	/** 最新连续移动输入 */
	UPROPERTY()
	FVector2D MoveIntent = FVector2D::ZeroVector;

	/** 本帧连续观察输入 */
	UPROPERTY()
	FVector2D LookIntent = FVector2D::ZeroVector;

	/** 本帧全部离散边沿命令 */
	UPROPERTY()
	TArray<FWuwaInputCommand> Commands;

	/** @return 输入帧是否包含有效时间基准 */
	bool IsValid() const
	{
		return FrameNumber > 0 && CapturedAt >= 0.0;
	}
};

USTRUCT(BlueprintType)
struct WUWA_API FWuwaInputIntent
{
	GENERATED_BODY()

	FVector2D MoveIntent = FVector2D::ZeroVector;
	FVector2D LookIntent = FVector2D::ZeroVector;

	bool bJumpPressed = false;
	bool bJumpReleased = false;
	bool bSprintPressed = false;

	/** 本帧是否收到 Attack 按下边沿 */
	bool bAttackPressed = false;

	/** 本帧是否收到 Attack 释放边沿 */
	bool bAttackReleased = false;

	/** 本帧是否收到 Grapple 按下边沿 */
	bool bGrapplePressedThisFrame = false;

	bool bLockTargetPressed = false;

	float SwitchTargetAxis = 0.0f;

	void ResetTransientInputs()
	{
		LookIntent = FVector2D::ZeroVector;

		bJumpPressed = false;
		bJumpReleased = false;
		bSprintPressed = false;
		bAttackPressed = false;
		bAttackReleased = false;
		bGrapplePressedThisFrame = false;
		bLockTargetPressed = false;
		SwitchTargetAxis = 0.0f;
	}
};
