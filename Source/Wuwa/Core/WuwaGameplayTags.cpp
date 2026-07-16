#include "Core/WuwaGameplayTags.h"

namespace WuwaGameplayTags
{
    UE_DEFINE_GAMEPLAY_TAG(Input_Jump, "Input.Jump");
    UE_DEFINE_GAMEPLAY_TAG(Input_Sprint, "Input.Sprint");
    UE_DEFINE_GAMEPLAY_TAG(Input_Dodge, "Input.Dodge");
    UE_DEFINE_GAMEPLAY_TAG(Input_Attack, "Input.Attack");
    UE_DEFINE_GAMEPLAY_TAG(Input_Grapple, "Input.Grapple");
    UE_DEFINE_GAMEPLAY_TAG(Input_LockTarget, "Input.LockTarget");
    UE_DEFINE_GAMEPLAY_TAG(Input_SwitchTarget, "Input.SwitchTarget");

    UE_DEFINE_GAMEPLAY_TAG(Action_Movement_Dash_Forward, "Action.Movement.Dash.Forward");
    UE_DEFINE_GAMEPLAY_TAG(Action_Movement_Backstep, "Action.Movement.Backstep");
    UE_DEFINE_GAMEPLAY_TAG(Action_Movement_DoubleJump_Directional, "Action.Movement.DoubleJump.Directional");
    UE_DEFINE_GAMEPLAY_TAG(Action_Movement_DoubleJump_Backflip, "Action.Movement.DoubleJump.Backflip");

    UE_DEFINE_GAMEPLAY_TAG(State_Action_Dash, "State.Action.Dash");

    // 注册原生标签；模块加载时会自动加入全局 Gameplay Tag 字典。
    UE_DEFINE_GAMEPLAY_TAG(State_Locomotion_Sprinting, "State.Locomotion.Sprinting");

    UE_DEFINE_GAMEPLAY_TAG(Block_Input_Move, "Block.Input.Move");
}
