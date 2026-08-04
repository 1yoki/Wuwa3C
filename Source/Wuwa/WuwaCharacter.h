// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "Movement/WuwaMovementTypes.h"
#include "Input/WuwaInputTypes.h"

#include "WuwaCharacter.generated.h"

class UWuwaCameraModeComponent;
class UWuwaCameraProfile;
class UWuwaTargetingComponent;
class UWuwaTargetingProfile;

class USpringArmComponent;
class UWuwaSpringArmComponent;
class UCameraComponent;
class UInputAction;
class UWuwaInputBufferComponent;

class UWuwaMovementProfile;
class UWuwaCharacterMovementComponent;
class UWuwaMovementActionExecutorComponent;
class UWuwaStateTagComponent;
class UWuwaActionRouterComponent;
class UWuwaCharacterActionSourceComponent;

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
	TObjectPtr<UWuwaSpringArmComponent> CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent *FollowCamera;

	// 所有活动 Gameplay Tags 的统一管理组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaStateTagComponent> StateTagComponent;

	// 离散输入命令的队列拥有者
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaInputBufferComponent> InputBufferComponent;

	// 把语义 Input Command 转换为角色 Action Request 的组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaCharacterActionSourceComponent> ActionSourceComponent;
	
	// Action Request 准入、仲裁、生命周期管理的组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaActionRouterComponent> ActionRouterComponent;

	// 持有移动类 Action 运行资源，并执行获准的 Action 的组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaMovementActionExecutorComponent> MovementActionExecutorComponent;
	
	// 当前角色目标候选、软锁与硬锁状态的管理组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaTargetingComponent> TargetingComponent;

	// Camera Mode 选择并消费 Gameplay 事实的组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaCameraModeComponent> CameraModeComponent;

	// 当前角色使用的移动参数配置
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaMovementProfile> MovementProfile;

	// 当前角色使用的目标查询与评分配置
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaTargetingProfile> TargetingProfile;

	// 当前角色使用的 Camera 参数配置
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaCameraProfile> CameraProfile;

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

public:
	/** 获取当前角色拥有的组件 */
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

	UFUNCTION(BlueprintPure, Category = "Wuwa|Targeting")
	UWuwaTargetingComponent *GetTargetingComponent() const
	{
		return TargetingComponent;
	}

	UFUNCTION(BlueprintPure, Category = "Wuwa|Camera")
	UWuwaCameraModeComponent *GetCameraModeComponent() const
	{
		return CameraModeComponent;
	}
	
	// 返回自定义移动组件，仅供 Gameplay C++ 使用。
	UWuwaCharacterMovementComponent *GetWuwaMovementComponent() const;

	// 查询当前 Router 是否正在执行任一二段跳动作
	bool IsAirDoubleJumpActionActive() const;

	// 转发持续移动意图。
	void SetLocomotionIntent(const FVector2D &MoveIntent);

	// 返回 Controller 最近一帧提交的真实移动意图，不受 Block.Input.Move 清零影响
	const FVector2D &GetCurrentMoveIntent() const
	{
		return CurrentMoveIntent;
	}

	// 将离散输入命令提交给角色输入缓存。
	bool SubmitInputCommand(const FWuwaInputCommand &Command);

	// 返回清理过期项后的命令副本 Debug
	TArray<FWuwaInputCommand> GetBufferedInputCommands();

private:
	// GetBufferedInputCommands() 查询使用 World 时间基准。
	double GetInputCommandTime() const;

	// 查询持续移动输入是否被当前动作阻断。
	bool IsMoveInputBlocked() const;

	// Controller 每帧提交的真实 WASD 快照；动作输入阻止不能修改该值
	UPROPERTY(Transient)
	FVector2D CurrentMoveIntent = FVector2D::ZeroVector;

public:
	
	// 返回动画和 Debug 使用的当前角色运动状态快照。
	UFUNCTION(BlueprintPure, Category = "Wuwa|Movement")
	FWuwaLocomotionSnapshot GetLocomotionSnapshot() const;
	
	virtual void BeginPlay() override;
};
