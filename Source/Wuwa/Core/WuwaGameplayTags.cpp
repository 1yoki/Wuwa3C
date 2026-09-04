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
UE_DEFINE_GAMEPLAY_TAG(Action_Traversal_Grapple, "Action.Traversal.Grapple");
UE_DEFINE_GAMEPLAY_TAG(Action_Traversal_Grapple_Free, "Action.Traversal.Grapple.Free");

UE_DEFINE_GAMEPLAY_TAG(Action_Capability_Movement, "Action.Capability.Movement");
UE_DEFINE_GAMEPLAY_TAG(Action_Capability_Animation, "Action.Capability.Animation");
UE_DEFINE_GAMEPLAY_TAG(Action_Capability_Traversal_Grapple, "Action.Capability.Traversal.Grapple");

UE_DEFINE_GAMEPLAY_TAG(Action_Event_Animation_Completed, "Action.Event.Animation.Completed");
UE_DEFINE_GAMEPLAY_TAG(Action_Event_Animation_Interrupted, "Action.Event.Animation.Interrupted");
UE_DEFINE_GAMEPLAY_TAG(Action_Event_Movement_Landed, "Action.Event.Movement.Landed");
UE_DEFINE_GAMEPLAY_TAG(Action_Event_Movement_LeftGround, "Action.Event.Movement.LeftGround");
UE_DEFINE_GAMEPLAY_TAG(Action_Event_Input_MoveChanged, "Action.Event.Input.MoveChanged");
UE_DEFINE_GAMEPLAY_TAG(Action_Event_Window_MoveCancel_Begin, "Action.Event.Window.MoveCancel.Begin");
UE_DEFINE_GAMEPLAY_TAG(Action_Event_Movement_SprintHandoff, "Action.Event.Movement.SprintHandoff");
UE_DEFINE_GAMEPLAY_TAG(Action_Event_Traversal_Grapple_PullStarted, "Action.Event.Traversal.Grapple.PullStarted");
UE_DEFINE_GAMEPLAY_TAG(Action_Event_Traversal_Grapple_Released, "Action.Event.Traversal.Grapple.Released");
UE_DEFINE_GAMEPLAY_TAG(Action_Event_Traversal_Grapple_Blocked, "Action.Event.Traversal.Grapple.Blocked");
UE_DEFINE_GAMEPLAY_TAG(Action_Event_Traversal_Grapple_TimedOut, "Action.Event.Traversal.Grapple.TimedOut");
UE_DEFINE_GAMEPLAY_TAG(Action_Event_Traversal_Grapple_Landed, "Action.Event.Traversal.Grapple.Landed");
UE_DEFINE_GAMEPLAY_TAG(Action_Event_Traversal_Grapple_Visual_Attach, "Action.Event.Traversal.Grapple.Visual.Attach");
UE_DEFINE_GAMEPLAY_TAG(Action_Event_Traversal_Grapple_Visual_Detach, "Action.Event.Traversal.Grapple.Visual.Detach");

// 定义状态标签
UE_DEFINE_GAMEPLAY_TAG(State_Action_Dash, "State.Action.Dash");
UE_DEFINE_GAMEPLAY_TAG(State_Action_ExclusiveActive, "State.Action.ExclusiveActive");
UE_DEFINE_GAMEPLAY_TAG(State_Locomotion_Sprinting, "State.Locomotion.Sprinting");
UE_DEFINE_GAMEPLAY_TAG(State_Targeting_HardLocked, "State.Targeting.HardLocked");
UE_DEFINE_GAMEPLAY_TAG(State_Traversal_Grappling, "State.Traversal.Grappling");

UE_DEFINE_GAMEPLAY_TAG(Ability_Combat, "Ability.Combat");
UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_Attack, "Ability.Combat.Attack");
UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_Attack_Light, "Ability.Combat.Attack.Light");
UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_Reaction, "Ability.Combat.Reaction");
UE_DEFINE_GAMEPLAY_TAG(Ability_Combat_Reaction_Stagger, "Ability.Combat.Reaction.Stagger");
UE_DEFINE_GAMEPLAY_TAG(State_Combat_Attacking, "State.Combat.Attacking");
UE_DEFINE_GAMEPLAY_TAG(Event_Combat_HitWindow_Begin, "Event.Combat.HitWindow.Begin");
UE_DEFINE_GAMEPLAY_TAG(Event_Combat_HitWindow_End, "Event.Combat.HitWindow.End");
UE_DEFINE_GAMEPLAY_TAG(Event_Combat_ComboWindow_Begin, "Event.Combat.ComboWindow.Begin");
UE_DEFINE_GAMEPLAY_TAG(Event_Combat_ComboWindow_End, "Event.Combat.ComboWindow.End");
UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Combat_Sword_Swing, "GameplayCue.Combat.Sword.Swing");
UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Combat_Sword_Hit, "GameplayCue.Combat.Sword.Hit");
UE_DEFINE_GAMEPLAY_TAG(Data_Damage_Base, "Data.Damage.Base");
UE_DEFINE_GAMEPLAY_TAG(Data_Damage_Poise, "Data.Damage.Poise");
UE_DEFINE_GAMEPLAY_TAG(Data_Poise_Reset, "Data.Poise.Reset");
UE_DEFINE_GAMEPLAY_TAG(Event_Combat_Poise_Broken, "Event.Combat.Poise.Broken");
UE_DEFINE_GAMEPLAY_TAG(State_Combat_Dead, "State.Combat.Dead");
UE_DEFINE_GAMEPLAY_TAG(State_Combat_Staggered, "State.Combat.Staggered");
UE_DEFINE_GAMEPLAY_TAG(Weapon_Sword_Training, "Weapon.Sword.Training");

// 定义摄像机模式标签
UE_DEFINE_GAMEPLAY_TAG(Camera_Exploration, "Camera.Exploration");
UE_DEFINE_GAMEPLAY_TAG(Camera_LockOn, "Camera.LockOn");

UE_DEFINE_GAMEPLAY_TAG(Cooldown_Combat_Attack_Light, "Cooldown.Combat.Attack.Light");

// 定义打断标签
UE_DEFINE_GAMEPLAY_TAG(Event_Ability_InterruptWindow_Begin, "Event.Ability.InterruptWindow.Begin");
UE_DEFINE_GAMEPLAY_TAG(Event_Ability_InterruptWindow_End, "Event.Ability.InterruptWindow.End");
}
