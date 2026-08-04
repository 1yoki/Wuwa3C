#include "Core/WuwaGameplayTags.h"

namespace WuwaGameplayTags
{
    // 定义 Input 命令标签
    UE_DEFINE_GAMEPLAY_TAG(Input_Jump, "Input.Jump");
    UE_DEFINE_GAMEPLAY_TAG(Input_Sprint, "Input.Sprint");
    UE_DEFINE_GAMEPLAY_TAG(Input_Dodge, "Input.Dodge");
    UE_DEFINE_GAMEPLAY_TAG(Input_Attack, "Input.Attack");
    UE_DEFINE_GAMEPLAY_TAG(Input_Grapple, "Input.Grapple");
    UE_DEFINE_GAMEPLAY_TAG(Input_LockTarget, "Input.LockTarget");
    UE_DEFINE_GAMEPLAY_TAG(Input_SwitchTarget, "Input.SwitchTarget");
    UE_DEFINE_GAMEPLAY_TAG(Block_Input_Move, "Block.Input.Move");
    
    // 定义 Action 类标签
    UE_DEFINE_GAMEPLAY_TAG(Action_Movement_Dash_Forward, "Action.Movement.Dash.Forward");
    UE_DEFINE_GAMEPLAY_TAG(Action_Movement_Backstep, "Action.Movement.Backstep");
    UE_DEFINE_GAMEPLAY_TAG(Action_Movement_DoubleJump_Directional, "Action.Movement.DoubleJump.Directional");
    UE_DEFINE_GAMEPLAY_TAG(Action_Movement_DoubleJump_Backflip, "Action.Movement.DoubleJump.Backflip");
    
    // 定义状态标签
    UE_DEFINE_GAMEPLAY_TAG(State_Action_Dash, "State.Action.Dash");
    UE_DEFINE_GAMEPLAY_TAG(State_Locomotion_Sprinting, "State.Locomotion.Sprinting");
    UE_DEFINE_GAMEPLAY_TAG(State_Targeting_HardLocked, "State.Targeting.HardLocked");

    // 定义摄像机模式标签
    UE_DEFINE_GAMEPLAY_TAG(Camera_Exploration, "Camera.Exploration");
    UE_DEFINE_GAMEPLAY_TAG(Camera_LockOn, "Camera.LockOn");
}
