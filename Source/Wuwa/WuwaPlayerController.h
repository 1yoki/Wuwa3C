// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Input/WuwaInputTypes.h"
#include "WuwaPlayerController.generated.h"

class UInputMappingContext;
class UWuwaInputConfig;
struct FInputActionValue;
class UUserWidget;

class AWuwaCharacter;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class AWuwaPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UWuwaInputConfig> InputConfig;

	FWuwaInputIntent InputIntent;

	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;

	void Input_Move(const FInputActionValue &Value);
	void Input_MoveCompleted(const FInputActionValue &Value);
	void Input_Look(const FInputActionValue &Value);

	void Input_JumpPressed();
	void Input_JumpReleased();

	void Input_SprintPressed();

	void Input_DodgePressed();
	void Input_AttackPressed();
	void Input_GrapplePressed();
	void Input_LockTargetPressed();
	void Input_SwitchTarget(const FInputActionValue &Value);

	void ProcessInputIntent();

	// 使用本帧输入数据构建不可变命令
	FWuwaInputCommand BuildInputCommand(const FGameplayTag &InputTag, const FVector2D &Direction);

	// 将命令提交给当前控制角色
	bool SubmitSemanticInputCommand(AWuwaCharacter &ControlledCharacter, const FGameplayTag &InputTag, const FVector2D &Direction);

	// 将本帧输入转换为结构化命令
	void SubmitTransientInputCommands(AWuwaCharacter &ControlledCharacter);

	// 返回 Command 使用的统一 World 时间
	double GetInputCommandTime() const;

	// 每个Controller会话使用单调递增序号
	uint32 NextInputCommandSequence = 1;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category = "Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;
};
