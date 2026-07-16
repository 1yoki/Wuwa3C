// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "Movement/WuwaMovementTypes.h"
#include "Input/WuwaInputTypes.h"

#include "WuwaCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UWuwaInputBufferComponent;

class UWuwaMovementProfile;
class UWuwaCharacterMovementComponent;
class UWuwaStateTagComponent;
class UWuwaActionRouterComponent;

struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AWuwaCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent *CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent *FollowCamera;

	// 角色所有活动 Gameplay Tags 的统一聚合组件。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaStateTagComponent> StateTagComponent;

	// 角色离散输入命令的唯一队列拥有者。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaInputBufferComponent> InputBufferComponent;

	// 角色动作准入和独占生命周期的唯一拥有者
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaActionRouterComponent> ActionRouterComponent;

	// 当前角色使用的移动配置。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaMovementProfile> MovementProfile;

public:
	/** Constructor */
	AWuwaCharacter(const FObjectInitializer &ObjectInitializer);

public:
	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoJumpEnd();

	// 根据当前移动状态请求空中 Sprint。
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual bool DoSprintPressed();

public:
	/** 获取角色统一状态标签组件。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|State")
	UWuwaStateTagComponent *GetStateTagComponent() const
	{
		return StateTagComponent;
	}

	UFUNCTION(BlueprintPure, Category = "Wuwa|Action")
	UWuwaActionRouterComponent *GetActionRouterComponent() const
	{
		return ActionRouterComponent;
	}

	// 转发持续移动意图。
	void SetLocomotionIntent(const FVector2D &MoveIntent);

	// 将离散输入命令提交给角色输入缓存。
	bool SubmitInputCommand(const FWuwaInputCommand &Command);

	// 查看当前队首有效命令，但不消费。
	bool PeekInputCommand(FWuwaInputCommand &OutCommand);

	// 只消费 Sequence 匹配的队首命令。
	bool ConsumeInputCommand(uint32 Sequence, FWuwaInputCommand &OutCommand);

	// 清除指定输入标签的全部缓存命令。
	int32 ClearInputCommandsByTag(const FGameplayTag &InputTag);

	// 返回清理过期项后的命令副本。
	TArray<FWuwaInputCommand> GetBufferedInputCommands();

private:
	// 所有缓存查询使用同一个 World 时间基准。
	double GetInputCommandTime() const;

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent *GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent *GetFollowCamera() const { return FollowCamera; }

protected:
	virtual void BeginPlay() override;

public:
	// 返回自定义移动组件，仅供 Gameplay C++ 使用。
	UWuwaCharacterMovementComponent *GetWuwaMovementComponent() const;

	// 返回动画和 Debug 使用的只读状态。
	UFUNCTION(BlueprintPure, Category = "Wuwa|Movement")
	FWuwaLocomotionSnapshot GetLocomotionSnapshot() const;
};
