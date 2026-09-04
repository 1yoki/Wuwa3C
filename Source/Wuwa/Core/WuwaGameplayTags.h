#pragma once

#include "NativeGameplayTags.h"

// 项目原生 Gameplay Tags 的集中声明。

namespace WuwaGameplayTags
{
// 离散输入定义标签
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Jump);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Sprint);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Dodge);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Attack);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Grapple);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_LockTarget);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_SwitchTarget);

// Action 产生标签
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Movement_Dash_Forward);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Movement_Backstep);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Movement_DoubleJump_Directional);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Movement_DoubleJump_Backflip);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Traversal_Grapple);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Traversal_Grapple_Free);

UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Capability_Movement);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Capability_Animation);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Capability_Traversal_Grapple);

UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Event_Animation_Completed);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Event_Animation_Interrupted);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Event_Movement_Landed);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Event_Movement_LeftGround);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Event_Input_MoveChanged);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Event_Window_MoveCancel_Begin);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Event_Movement_SprintHandoff);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Event_Traversal_Grapple_PullStarted);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Event_Traversal_Grapple_Released);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Event_Traversal_Grapple_Blocked);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Event_Traversal_Grapple_TimedOut);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Event_Traversal_Grapple_Landed);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Event_Traversal_Grapple_Visual_Attach);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Event_Traversal_Grapple_Visual_Detach);

// 角色状态标签
UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Action_Dash);

/** 任意 Legacy 独占动作活动状态标签 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Action_ExclusiveActive);

UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Locomotion_Sprinting);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Targeting_HardLocked);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Traversal_Grappling);

/** 战斗 Ability 分类标签 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat);

/** 攻击 Ability 分类标签 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_Attack);

/** 受击反应 Ability 分类标签 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_Reaction);

/** 硬直受击反应 Ability 标签 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_Reaction_Stagger);

/** 轻攻击 Ability 标签 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_Attack_Light);

/** 近战攻击活动状态标签 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat_Attacking);

/** 命中窗口开始事件 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_HitWindow_Begin);

/** 命中窗口结束事件 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_HitWindow_End);

/** 连招输入窗口开始事件 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_ComboWindow_Begin);

/** 连招输入窗口结束事件 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_ComboWindow_End);

/** 挥剑 GameplayCue 标签 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Combat_Sword_Swing);

/** 命中 GameplayCue 标签 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Combat_Sword_Hit);

/** 伤害基础值 SetByCaller 标签 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage_Base);

/** 韧性伤害 SetByCaller 标签 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage_Poise);

/** 韧性重置 SetByCaller 标签 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Poise_Reset);

/** 权威破韧 Gameplay Event 标签 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Poise_Broken);

/** 角色死亡状态标签 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat_Dead);

/** 角色处于硬直状态 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat_Staggered);

/** 占位训练剑标签 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Sword_Training);

// 摄像机模式标签
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Camera_Exploration);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Camera_LockOn);

/** 轻攻击冷却标签 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Combat_Attack_Light);

// 持有该标签时，禁止 Move Intent 驱动角色位移。
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Block_Input_Move);

// ability可打断窗口
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Ability_InterruptWindow_Begin);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Ability_InterruptWindow_End);
}
