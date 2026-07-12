#include "Core/WuwaGameplayTags.h"

namespace WuwaGameplayTags
{
    // 注册原生标签；模块加载时会自动加入全局 Gameplay Tag 字典。
    UE_DEFINE_GAMEPLAY_TAG(State_Locomotion_Sprinting, "State.Locomotion.Sprinting");
}