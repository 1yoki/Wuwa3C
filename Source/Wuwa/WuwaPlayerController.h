// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Input/WuwaInputTypes.h"
#include "WuwaPlayerController.generated.h"

class AWuwaCharacter;
class SWuwaLanRoomMenu;
class UWuwaDebugVisualizationComponent;
class UWuwaInputConfig;
struct FInputActionValue;

/** 将 Enhanced Input 边沿聚合为每帧唯一输入消息的 PlayerController */
UCLASS(Abstract)
class WUWA_API AWuwaPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AWuwaPlayerController();

protected:
	/** 本地演示视口的只读 Debug 绘制拥有者 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug")
	TObjectPtr<UWuwaDebugVisualizationComponent> DebugVisualizationComponent;

	/** 当前 Enhanced Input 资产配置 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UWuwaInputConfig> InputConfig;

	//~ Begin APlayerController Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostProcessInput(float DeltaTime, bool bGamePaused) override;
	virtual void SetupInputComponent() override;
	//~ End APlayerController Interface

private:
	/** 当前本地玩家持有的原生局域网房间菜单 */
	TSharedPtr<SWuwaLanRoomMenu> LanRoomMenu;

	/** 当前帧 Enhanced Input 聚合状态 */
	FWuwaInputIntent InputIntent;

	/** 当前 Controller 会话的下一个输入边沿序号 */
	int32 NextInputCommandSequence = 1;

	/** 持续移动输入 */
	void Input_Move(const FInputActionValue& Value);

	/** 移动输入结束 */
	void Input_MoveCompleted(const FInputActionValue& Value);

	/** 持续观察输入 */
	void Input_Look(const FInputActionValue& Value);

	/** 普通 Jump 按下边沿 */
	void Input_JumpPressed();

	/** 普通 Jump 释放边沿 */
	void Input_JumpReleased();

	/** Sprint Action 按下边沿 */
	void Input_SprintPressed();

	/** Attack Action 按下边沿 */
	void Input_AttackPressed();

	/** Attack Action 释放边沿 */
	void Input_AttackReleased();

	/** 记录本帧 Grapple 按下边沿 */
	void Input_GrapplePressed();

	/** Lock Target 按下边沿 */
	void Input_LockTargetPressed();

	/** Switch Target 方向边沿 */
	void Input_SwitchTarget(const FInputActionValue& Value);

	/** 切换本地只读调试叠层 */
	void CycleDebugVisualizationMode();

	/** 构建并提交本帧唯一输入消息 */
	void SubmitInputFrame(float DeltaTime, bool bGamePaused);

	/**
     * 构建一条带统一消息头的输入边沿
     * @param InputTag 输入语义
     * @param Trigger 按下或释放边沿
     * @param Direction 边沿发生时的二维方向
     * @return 不可变输入命令
     */
	FWuwaInputCommand
	BuildInputCommand(const FGameplayTag& InputTag, EWuwaInputCommandTrigger Trigger, const FVector2D& Direction);

	/** @return 当前 World 时间 */
	double GetInputCommandTime() const;

	/** @return 当前 Controller 是否应显示局域网房间菜单 */
	bool ShouldShowLanRoomMenu() const;

	/** 创建并聚焦局域网房间菜单 */
	void ShowLanRoomMenu();

	/** 从游戏视口移除局域网房间菜单 */
	void RemoveLanRoomMenu();

	/** 恢复本地玩家的游戏输入与视口焦点 */
	void RestoreGameInputMode();
};
