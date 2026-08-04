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

    // 角色状态标签
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Action_Dash);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Locomotion_Sprinting);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Targeting_HardLocked);

    // 摄像机模式标签
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Camera_Exploration);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Camera_LockOn);

    // 持有该标签时，禁止 Move Intent 驱动角色位移。
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Block_Input_Move);
}
