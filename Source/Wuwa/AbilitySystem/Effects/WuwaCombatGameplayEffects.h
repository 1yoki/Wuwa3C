// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"
#include "WuwaCombatGameplayEffects.generated.h"

/** 初始化玩家基础战斗属性的瞬时效果 */
UCLASS()
class WUWA_API UWuwaGameplayEffect_InitPlayer : public UGameplayEffect
{
	GENERATED_BODY()

public:
	/** 配置六项基础战斗属性的初始值 */
	UWuwaGameplayEffect_InitPlayer();
};

/** 轻攻击耐力消耗效果 */
UCLASS()
class WUWA_API UWuwaGameplayEffect_Cost_Stamina_SwordLight : public UGameplayEffect
{
	GENERATED_BODY()

public:
	/** 配置轻攻击的瞬时耐力消耗 */
	UWuwaGameplayEffect_Cost_Stamina_SwordLight();
};

/** 轻攻击冷却效果 */
UCLASS()
class WUWA_API UWuwaGameplayEffect_Cooldown_SwordLight : public UGameplayEffect
{
	GENERATED_BODY()

public:
	/** 配置轻攻击冷却时长和冷却标签 */
	UWuwaGameplayEffect_Cooldown_SwordLight();
};

/** 通过伤害 Execution 写入目标 IncomingDamage 的瞬时效果 */
UCLASS()
class WUWA_API UWuwaGameplayEffect_Damage_Sword : public UGameplayEffect
{
	GENERATED_BODY()

public:
	/** 配置轻剑伤害 Execution */
	UWuwaGameplayEffect_Damage_Sword();
};

/** 通过可追踪无限效果授予角色死亡状态 */
UCLASS()
class WUWA_API UWuwaGameplayEffect_Dead : public UGameplayEffect
{
	GENERATED_BODY()

public:
	/** 配置无限时长与死亡状态标签 */
	UWuwaGameplayEffect_Dead();
};

/** 通过可追踪无限效果授予硬直与移动输入阻止状态 */
UCLASS()
class WUWA_API UWuwaGameplayEffect_State_Staggered : public UGameplayEffect
{
	GENERATED_BODY()

public:
	/** 配置无限时长与硬直控制标签 */
	UWuwaGameplayEffect_State_Staggered();
};

/** 通过 SetByCaller 将当前韧性重置为指定值的瞬时效果 */
UCLASS()
class WUWA_API UWuwaGameplayEffect_Reset_Poise : public UGameplayEffect
{
	GENERATED_BODY()

public:
	/** 配置韧性 Override Modifier */
	UWuwaGameplayEffect_Reset_Poise();
};
