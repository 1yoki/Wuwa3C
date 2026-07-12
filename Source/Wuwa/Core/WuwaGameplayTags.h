#pragma once

#include "NativeGameplayTags.h"

// 项目原生 Gameplay Tags 的集中声明。

namespace WuwaGameplayTags
{
    /*
     * 表示角色当前正在进行有效冲刺。
     * 只有满足接地、存在移动输入且未被其他状态阻止时，Movement Component 才能授予该标签。
     */
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Locomotion_Sprinting);
}