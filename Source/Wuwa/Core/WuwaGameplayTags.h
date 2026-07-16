#pragma once

#include "NativeGameplayTags.h"

// 项目原生 Gameplay Tags 的集中声明。

namespace WuwaGameplayTags
{
    // 离散输入命令标签。
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Jump);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Sprint);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Dodge);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Attack);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Grapple);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_LockTarget);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_SwitchTarget);

    // Day 5 移动动作标签。
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Movement_Dash_Forward);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Movement_Backstep);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Movement_DoubleJump_Directional);
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Movement_DoubleJump_Backflip);

    // 当前存在一个地面冲刺独占动作。
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Action_Dash);

    /*
     * 表示角色当前正在进行有效冲刺。
     * 只有满足接地、存在移动输入且未被其他状态阻止时，Movement Component 才能授予该标签。
     */
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Locomotion_Sprinting);

    // 活动来源持有该标签时，持续 Move Intent 不得驱动角色位移。
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Block_Input_Move);
}
