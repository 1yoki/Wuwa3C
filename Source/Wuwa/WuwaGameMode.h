// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Contracts/WuwaCombatFacts.h"
#include "GameFramework/GameModeBase.h"
#include "WuwaGameMode.generated.h"

class AController;
class APawn;
class UAbilitySystemComponent;

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class WUWA_API AWuwaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	/** 创建使用 Wuwa PlayerState 的游戏模式 */
	AWuwaGameMode();

	/**
	 * 接收 Authority Pawn 的类型化死亡事实并启动唯一重生计时器
	 *
	 * @param Fact	Authority 死亡事实
	 * @return 无
	 */
	void HandlePawnDeath(const FWuwaDeathFact& Fact);

protected:
	//~ Begin AActor Interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

private:
	/** 架构验证使用的固定重生延迟 */
	UPROPERTY(EditDefaultsOnly, Category = "Wuwa|Combat|Respawn", meta = (ClampMin = "0.0"))
	float RespawnDelaySeconds = 1.f;

	/** 每个 Controller 当前唯一重生计时器 */
	TMap<TWeakObjectPtr<AController>, FTimerHandle> PendingRespawnTimers;

	/**
	 * 清理期望旧 Pawn 并调用 RestartPlayer
	 *
	 * @param Controller		死亡 Pawn 的 Controller
	 * @param ExpectedDeadPawn	计时器创建时的旧 Pawn
	 */
	void ExecuteRespawn(TWeakObjectPtr<AController> Controller, TWeakObjectPtr<APawn> ExpectedDeadPawn);

	/** @return 从 ASC 移除的死亡效果数量 */
	static int32 RemoveDeadEffects(UAbilitySystemComponent* AbilitySystemComponent);
};
