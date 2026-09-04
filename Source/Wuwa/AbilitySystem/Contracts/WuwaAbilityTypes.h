// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "WuwaAbilityTypes.generated.h"

/** Wuwa Ability 的输入激活策略 */
UENUM(BlueprintType)
enum class EWuwaAbilityActivationPolicy : uint8
{
	/** 收到对应 InputTag 的按下边沿时尝试激活 */
	OnInputTriggered
};

/** Ability 输入路由最近一次激活诊断结果 */
UENUM(BlueprintType)
enum class EWuwaAbilityActivationDebugFailure : uint8
{
	/** 没有激活失败 */
	None,

	/** 输入路由尚未绑定有效 ASC */
	NotInitialized,

	/** 死亡状态拒绝激活并清空输入 */
	DeadState,

	/** 硬直状态拒绝激活并清空输入 */
	StaggeredState,

	/** 输入对应的 AbilitySpec 已失效 */
	InvalidSpec,

	/** ASC 拒绝 TryActivateAbility */
	TryActivateAbilityRejected
};

UENUM(BlueprintType)
enum class EWuwaAbilityInterruptSource : uint8
{
	Move,
	Action,
	Ability
};

/** ASC 对 Debug 和交付测试暴露的只读运行快照 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaAbilitySystemRuntimeSnapshot
{
	GENERATED_BODY()

	/** ASC 是否已建立完整 ActorInfo */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Debug")
	bool bInitialized = false;

	/** ASC Owner 是否具有 Authority */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Debug")
	bool bAuthority = false;

	/** ASC OwnerActor 名称 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Debug")
	FString OwnerActorName;

	/** ASC AvatarActor 名称 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Debug")
	FString AvatarActorName;

	/** GameplayEffect 复制模式名称 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Debug")
	FString ReplicationModeName;

	/** 当前授予的 AbilitySpec 数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Debug")
	int32 GrantedAbilityCount = 0;

	/** 当前活动 AbilitySpec 数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Debug")
	int32 ActiveAbilityCount = 0;

	/** 当前活动 Ability 的资产标签并集 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Debug")
	FGameplayTagContainer ActiveAbilityTags;

	/** 当前 ActiveGameplayEffect 数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Debug")
	int32 ActiveGameplayEffectCount = 0;

	/** 当前生命值 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Debug")
	float Health = 0.f;

	/** 当前最大生命值 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Debug")
	float MaxHealth = 0.f;

	/** 当前耐力 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Debug")
	float Stamina = 0.f;

	/** 当前最大耐力 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Debug")
	float MaxStamina = 0.f;

	/** 当前攻击力 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Debug")
	float AttackPower = 0.f;

	/** 当前防御力 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Debug")
	float Defense = 0.f;

	/** Authority 当前待结算伤害 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Debug")
	float IncomingDamage = 0.f;

	/** 当前死亡标签数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Debug")
	int32 DeadTagCount = 0;
};

/** Ability 输入路由的只读运行快照 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaAbilityInputRouterRuntimeSnapshot
{
	GENERATED_BODY()

	/** 是否已绑定有效 ASC */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Input")
	bool bInitialized = false;

	/** 当前 ASC 名称 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Input")
	FString AbilitySystemComponentName;

	/** 最近接受的输入序号 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Input")
	int32 LastAcceptedSequence = 0;

	/** 最近接受的输入标签 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Input")
	FGameplayTag LastAcceptedInputTag;

	/** 最近尝试激活的 AbilitySpec Handle */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Input")
	FGameplayAbilitySpecHandle LastActivationSpecHandle;

	/** 最近转发活动输入的 AbilitySpec Handle */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Input")
	FGameplayAbilitySpecHandle LastForwardedInputSpecHandle;

	/** 最近一次激活尝试是否成功 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Input")
	bool bLastActivationSucceeded = false;

	/** 最近一次激活失败原因 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Input")
	EWuwaAbilityActivationDebugFailure LastActivationFailure = EWuwaAbilityActivationDebugFailure::None;

	/** 当前按下集合数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Input")
	int32 PressedSpecCount = 0;

	/** 当前释放集合数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Input")
	int32 ReleasedSpecCount = 0;

	/** 当前持续按住集合数量 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Input")
	int32 HeldSpecCount = 0;

	/** 累计激活尝试次数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Input")
	int32 ActivationAttemptCount = 0;

	/** 累计激活成功次数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Input")
	int32 ActivationSuccessCount = 0;

	/** 累计转发活动 Ability 输入的次数 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|AbilitySystem|Input")
	int32 ActiveInputForwardCount = 0;
};

USTRUCT(BlueprintType)
struct WUWA_API FWuwaAbilityInterruptPolicy
{
	GENERATED_BODY()

	/** Ability 活动期间是否阻止普通移动 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bBlockMoveWhileActive = false;

	/** 普通移动和移动是否可以在 InterruptWindow 中打断 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bAllowMoveInterrupt = false;

	/** Legacy Action 是否可以在 InterruptWindow 中打断 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bAllowActionInterrupt = false;

	/** Ability 活动期间是否阻止普通 Action 启动 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bBlockActionWhileActive = false;

	/** 哪些 Action 可以打断；为空表示全部 Action */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Action"))
	FGameplayTagContainer AllowedInterruptActionTags;

	/** WASD 超过这个值才认为玩家确实希望打断 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MoveInterruptThreshold = 0.1f;
};
