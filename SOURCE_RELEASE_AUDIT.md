# Wuwa 源码发布审计

> 审计日期：2026-09-04（Asia/Shanghai）
>
> 审计对象：批准执行时的当前工作树复制品；原工程保持原样

## 1. 基线与输出

| 项目 | 结果 |
| --- | --- |
| 基准提交 | `33a6d189cbf26c08443ae8cff1b5a859cdcc4df9` |
| 快照时间 | `2026-09-04 16:56:53 +08:00` |
| 输入文件 | 283 |
| 输入总大小 | 2,653,000 bytes |
| 输入清单 SHA-256 | `23040e3dae92ccd52ab5010cbaeec09b6702aa97f8933ce30574430f9666a7cd` |
| 快照时工作树状态项 | 1,247 |
| 发布版模块文件 | 177 |
| 发布版 `.h/.cpp` | 176 |
| 删除输入文件 | 100 |
| 新增 Gameplay 文件 | 0 |
| 最终发布区文件 | 189（包含本审计文件与本地 `.ubtignore`） |

输入清单摘要的计算方式：按相对路径排序，对每个输入文件记录 SHA-256，再以 LF 拼接全部“哈希 + 路径”行并计算一次 SHA-256。执行结束后重新计算原工程摘要，结果保持一致。

原工程保护检查：Git 状态中 `Source/_release` 之外仍为 1,247 项，与快照基线相同；根项目、原 `Source/Wuwa` 与三个输入配置的聚合摘要未变化。

## 2. 发布根新增文件

- `.ubtignore`：阻止父工程的 UBT 扫描暂存副本；导出独立仓库前删除
- `.clang-format`：UE 风格 Allman、4 列 Tab、120 列、左侧指针/引用、禁用 Include 排序和注释重排
- `.editorconfig`：UTF-8、CRLF、末尾换行与基础缩进规则
- `.gitignore`：排除 Content、插件、缓存、构建产物、Demo 压缩包及本地 `.ubtignore`
- `README.md`：说明源码、Content 包和 Demo 的组合方式
- `SOURCE_RELEASE_AUDIT.md`：本文件

以下输入以白名单复制：`Wuwa.uproject`、三个 Runtime Config、两个 Target 文件以及 `Source/Wuwa/**`。未复制 Content、Plugins、Binaries、Intermediate、Saved、DerivedDataCache、Artifacts、Tools、Docs 或本机配置。

## 3. 删除清单

### 3.1 自动化测试（82 个）

- `Source/Wuwa/Test/AbilitySystem/WuwaAbilityEffectAssetTests.cpp`
- `Source/Wuwa/Test/AbilitySystem/WuwaAbilityInputPipelineTests.cpp`
- `Source/Wuwa/Test/AbilitySystem/WuwaAbilityInputRouterTests.cpp`
- `Source/Wuwa/Test/AbilitySystem/WuwaAbilityResourceRuleTests.cpp`
- `Source/Wuwa/Test/AbilitySystem/WuwaAbilitySetTests.cpp`
- `Source/Wuwa/Test/AbilitySystem/WuwaAbilitySystemFoundationTests.cpp`
- `Source/Wuwa/Test/AbilitySystem/WuwaAbilitySystemTestDoubles.cpp`
- `Source/Wuwa/Test/AbilitySystem/WuwaAbilitySystemTestDoubles.h`
- `Source/Wuwa/Test/AbilitySystem/WuwaActionAbilityInteropTests.cpp`
- `Source/Wuwa/Test/AbilitySystem/WuwaAttributeSetTests.cpp`
- `Source/Wuwa/Test/AbilitySystem/WuwaMeleeComboInputTests.cpp`
- `Source/Wuwa/Test/AbilitySystem/WuwaPawnAbilityInitTests.cpp`
- `Source/Wuwa/Test/AbilitySystem/WuwaRespawnTests.cpp`
- `Source/Wuwa/Test/AbilitySystem/WuwaStaggerAbilityTests.cpp`
- `Source/Wuwa/Test/Actions/WuwaActionCoordinatorTests.cpp`
- `Source/Wuwa/Test/Actions/WuwaActionIntentProviderTests.cpp`
- `Source/Wuwa/Test/Actions/WuwaActionQueueTests.cpp`
- `Source/Wuwa/Test/Actions/WuwaActionResolverTests.cpp`
- `Source/Wuwa/Test/Actions/WuwaActionTestDoubles.cpp`
- `Source/Wuwa/Test/Actions/WuwaActionTestDoubles.h`
- `Source/Wuwa/Test/Actions/WuwaCharacterMessageDispatcherTests.cpp`
- `Source/Wuwa/Test/Animation/WuwaActionAnimationCapabilityTests.cpp`
- `Source/Wuwa/Test/Camera/WuwaCameraFeedbackStackTests.cpp`
- `Source/Wuwa/Test/Combat/WuwaCombatAssetTests.cpp`
- `Source/Wuwa/Test/Combat/WuwaCombatDataValidationTests.cpp`
- `Source/Wuwa/Test/Combat/WuwaCombatDeliveryTests.cpp`
- `Source/Wuwa/Test/Combat/WuwaCombatExecutionTests.cpp`
- `Source/Wuwa/Test/Combat/WuwaCombatPhase2IntegrationTests.cpp`
- `Source/Wuwa/Test/Combat/WuwaCombatTestDoubles.cpp`
- `Source/Wuwa/Test/Combat/WuwaCombatTestDoubles.h`
- `Source/Wuwa/Test/Combat/WuwaDamageDummyActorTests.cpp`
- `Source/Wuwa/Test/Combat/WuwaDamageExecutionTests.cpp`
- `Source/Wuwa/Test/Combat/WuwaHealthChangedFactTests.cpp`
- `Source/Wuwa/Test/Combat/WuwaHealthDeathTests.cpp`
- `Source/Wuwa/Test/Combat/WuwaMeleeAttackAbilityTests.cpp`
- `Source/Wuwa/Test/Combat/WuwaMeleeAttackAssetTests.cpp`
- `Source/Wuwa/Test/Combat/WuwaMeleeAttackDataTests.cpp`
- `Source/Wuwa/Test/Combat/WuwaMeleeAttackRootMotionPolicyTests.cpp`
- `Source/Wuwa/Test/Combat/WuwaMeleeComboDamageTests.cpp`
- `Source/Wuwa/Test/Combat/WuwaMeleeComboTestUtils.h`
- `Source/Wuwa/Test/Combat/WuwaMeleeComboWindowTests.cpp`
- `Source/Wuwa/Test/Combat/WuwaPoiseAttributeTests.cpp`
- `Source/Wuwa/Test/Combat/WuwaPoiseBreakTests.cpp`
- `Source/Wuwa/Test/Combat/WuwaWeaponComponentTests.cpp`
- `Source/Wuwa/Test/Diagnostics/WuwaActionEvaluationGmaTests.cpp`
- `Source/Wuwa/Test/Diagnostics/WuwaActionRequestGmaTests.cpp`
- `Source/Wuwa/Test/Diagnostics/WuwaGrappleGmaPIETests.cpp`
- `Source/Wuwa/Test/Diagnostics/WuwaLockOnGmaTests.cpp`
- `Source/Wuwa/Test/Diagnostics/WuwaLockOnRuntimeABPIETests.cpp`
- `Source/Wuwa/Test/Movement/WuwaGrappleMovementTests.cpp`
- `Source/Wuwa/Test/Movement/WuwaGrappleMovementTests.h`
- `Source/Wuwa/Test/Movement/WuwaNetworkLocomotionTests.cpp`
- `Source/Wuwa/Test/Network/WuwaCharacterNetworkMoveTests.cpp`
- `Source/Wuwa/Test/Network/WuwaCrossDomainActionTests.cpp`
- `Source/Wuwa/Test/Network/WuwaGrappleNetworkTests.cpp`
- `Source/Wuwa/Test/Network/WuwaLanSessionContractTests.cpp`
- `Source/Wuwa/Test/Network/WuwaLegacyActionControlNetworkTests.cpp`
- `Source/Wuwa/Test/Network/WuwaLegacyActionNetworkTests.cpp`
- `Source/Wuwa/Test/Network/WuwaLegacyActionPresentationTests.cpp`
- `Source/Wuwa/Test/Network/WuwaLocomotionAnimationNetworkTests.cpp`
- `Source/Wuwa/Test/Network/WuwaMeleeComboNetworkTests.cpp`
- `Source/Wuwa/Test/Network/WuwaNetworkBaselineTests.cpp`
- `Source/Wuwa/Test/Network/WuwaNetworkJumpTests.cpp`
- `Source/Wuwa/Test/Network/WuwaPhase15DeliveryTests.cpp`
- `Source/Wuwa/Test/Network/WuwaPhase2ActiveAbilityInputTests.cpp`
- `Source/Wuwa/Test/Network/WuwaPhase2NetworkTests.cpp`
- `Source/Wuwa/Test/Network/WuwaPoiseNetworkTests.cpp`
- `Source/Wuwa/Test/Network/WuwaRootMotionSourceRouteTests.cpp`
- `Source/Wuwa/Test/Network/WuwaStaggerLegacyActionTests.cpp`
- `Source/Wuwa/Test/Network/WuwaStaggerNetworkTests.cpp`
- `Source/Wuwa/Test/Network/WuwaTargetingNetworkTests.cpp`
- `Source/Wuwa/Test/Network/WuwaWorldHealthBarNetworkTests.cpp`
- `Source/Wuwa/Test/Traversal/WuwaGrappleCapabilityTests.cpp`
- `Source/Wuwa/Test/Traversal/WuwaGrappleQueryTests.cpp`
- `Source/Wuwa/Test/Traversal/WuwaGrappleTrajectoryTests.cpp`
- `Source/Wuwa/Test/Traversal/WuwaTraversalCompositionTests.cpp`
- `Source/Wuwa/Test/UI/WuwaPhase2HealthBarIntegrationTests.cpp`
- `Source/Wuwa/Test/UI/WuwaWorldHealthBarTests.cpp`
- `Source/Wuwa/Test/WuwaCameraFramingRulesTests.cpp`
- `Source/Wuwa/Test/WuwaCameraModeStackTests.cpp`
- `Source/Wuwa/Test/WuwaLandingTests.cpp`
- `Source/Wuwa/Test/WuwaTargetingRulesTests.cpp`

### 3.2 内存诊断探针（8 个）

- `Source/Wuwa/Diagnostics/GameMemCases/WuwaActionEvaluationGmaProbe.cpp`
- `Source/Wuwa/Diagnostics/GameMemCases/WuwaActionEvaluationGmaProbe.h`
- `Source/Wuwa/Diagnostics/GameMemCases/WuwaActionRequestGmaProbe.cpp`
- `Source/Wuwa/Diagnostics/GameMemCases/WuwaActionRequestGmaProbe.h`
- `Source/Wuwa/Diagnostics/GameMemCases/WuwaGrappleGmaProbe.cpp`
- `Source/Wuwa/Diagnostics/GameMemCases/WuwaGrappleGmaProbe.h`
- `Source/Wuwa/Diagnostics/GameMemCases/WuwaLockOnGmaProbe.cpp`
- `Source/Wuwa/Diagnostics/GameMemCases/WuwaLockOnGmaProbe.h`

### 3.3 一次性 Editor Library（6 个）

- `Source/Wuwa/Editor/Combat/WuwaCombatAssetAuthoringLibrary.cpp`
- `Source/Wuwa/Editor/Combat/WuwaCombatAssetAuthoringLibrary.h`
- `Source/Wuwa/Editor/WuwaGrappleAnimationContinuityEditorLibrary.cpp`
- `Source/Wuwa/Editor/WuwaGrappleAnimationContinuityEditorLibrary.h`
- `Source/Wuwa/Editor/WuwaLocomotionAnimationStateMigrationEditorLibrary.cpp`
- `Source/Wuwa/Editor/WuwaLocomotionAnimationStateMigrationEditorLibrary.h`

### 3.4 合并后删除的过碎实现（4 个）

- `Source/Wuwa/AbilitySystem/WuwaAbilitySystemLog.cpp`
- `Source/Wuwa/Actions/Resolution/WuwaActionRuleIntentProviderComponent.cpp`
- `Source/Wuwa/Actions/Runtime/WuwaActionCoordinatorAdmission.cpp`
- `Source/Wuwa/Combat/WuwaCombatLog.cpp`

未删除任何批准清单之外的文件。

## 4. 构建与依赖净化

- `Wuwa.uproject` 删除内存诊断插件声明
- `Wuwa.Build.cs` 删除两个内存诊断模块、仅供测试报告使用的 Json、一次性 Editor Library 使用的 BlueprintGraph/Kismet/UnrealEd，以及六个不存在的 `Variant_Combat/**` Include Path
- `Wuwa.Target.cs` 删除两个 Automation 强制开关
- `WuwaEditor.Target.cs` 保留
- `PublicIncludePaths` 仅保留有效的 `Wuwa`
- 删除空的 `Source/Wuwa/build`、Diagnostics、Editor 与 Test 目录

## 5. Gameplay 结构调整

- Action Coordinator：将 Admission、Prepare、Commit 与回滚实现并回 `WuwaActionCoordinatorComponent.cpp`，删除诊断专用 `TryStartIntentImpl` 包装层；阅读顺序为初始化/绑定 → Queue → Admission → Prepare/Commit → Finalize/Cleanup → Snapshot/EndPlay
- Rule Driven Intent Provider：将派生 Provider 的唯一构造函数合并到 `WuwaRuleDrivenIntentProviderComponent.cpp`
- Module Log Categories：将 `LogWuwaAbility` 与 `LogWuwaCombat` 定义合并到 `Wuwa.cpp`，类别头和调用语义不变
- Targeting：删除双缓冲实验和测试计时分支，仅保留函数内临时数组的单一候选收集、评分与切换路径
- Traversal：删除 Resolve/Query/Commit/Phys/Finalize 上的诊断 Scope、Token 和采样，不改变 Query → Frozen Payload → Capability → Movement 链路
- Action Network：共享 Payload、Authority、Generation 与 Kind 校验继续位于服务端入口；私有 Legacy/Grapple 分支删除对同一 Payload/Kind 的重复校验
- Movement：删除只为临时网络基线日志创建的延迟 Timer，保留 `GetNetworkBaselineSnapshot()` 及其 Debug 消费者
- Melee、Stagger、Health、Poise、Combat Execution、Pawn Ability Init 和 Character 装配：删除常态追踪、重复拒绝日志及其纯诊断局部量；状态机、Fact、Effect、委托和清理边界不变
- 保留 Movement/Traversal、Movement/Network、Actions/Network、Camera/Feedback、Network/LAN、UI/LAN、Animation Notifies、Melee Ability 和 Debug Visualization 的独立职责边界
- 未新增模块、Manager、Facade、Service、Adapter、兼容层、公共 API、反射字段、持久状态或网络协议

## 6. 日志审计

| 级别 | 基线 | 发布版 | 减少 |
| --- | ---: | ---: | ---: |
| Error | 212 | 181 | 31 |
| Warning | 120 | 96 | 24 |
| Log | 62 | 0 | 62 |
| Display | 12 | 0 | 12 |
| Verbose | 39 | 0 | 39 |
| VeryVerbose | 1 | 0 | 1 |
| 合计 | 446 | 277 | 169（37.89%） |

基线包含随后删除的三个 Editor Library，其日志贡献为 Error 27、Warning 6、Display 9。其余运行时文件的日志变化如下：

| 文件 | 基线 | 发布版 | 减少 |
| --- | ---: | ---: | ---: |
| `AbilitySystem/Abilities/WuwaGameplayAbility_MeleeAttack.cpp` | 44 | 15 | 29 |
| `AbilitySystem/Abilities/WuwaGameplayAbility_Stagger.cpp` | 22 | 17 | 5 |
| `AbilitySystem/Data/WuwaAbilitySet.cpp` | 10 | 8 | 2 |
| `AbilitySystem/Input/WuwaAbilityInputRouterComponent.cpp` | 4 | 2 | 2 |
| `AbilitySystem/Interop/WuwaActionAbilityInteropComponent.cpp` | 18 | 16 | 2 |
| `AbilitySystem/Runtime/WuwaPawnAbilityInitComponent.cpp` | 7 | 2 | 5 |
| `Actions/Network/WuwaActionNetworkComponent.cpp` | 25 | 24 | 1 |
| `Actions/Runtime/WuwaActionCoordinatorAdmission.cpp` | 2 | 0 | 2 |
| `Animation/Notifies/WuwaAnimNotifyState_AbilityInterruptWindow.cpp` | 2 | 1 | 1 |
| `Combat/Actors/WuwaDamageDummyActor.cpp` | 16 | 14 | 2 |
| `Combat/Animation/WuwaAnimNotifyState_CombatHitWindow.cpp` | 2 | 1 | 1 |
| `Combat/Animation/WuwaAnimNotifyState_ComboWindow.cpp` | 2 | 1 | 1 |
| `Combat/Attributes/WuwaCombatSet.cpp` | 3 | 1 | 2 |
| `Combat/Attributes/WuwaHealthSet.cpp` | 4 | 1 | 3 |
| `Combat/Attributes/WuwaPoiseSet.cpp` | 4 | 1 | 3 |
| `Combat/Attributes/WuwaResourceSet.cpp` | 3 | 1 | 2 |
| `Combat/Effects/WuwaDamageExecution.cpp` | 6 | 5 | 1 |
| `Combat/Health/WuwaHealthComponent.cpp` | 8 | 5 | 3 |
| `Combat/Poise/WuwaPoiseComponent.cpp` | 6 | 3 | 3 |
| `Combat/Presentation/WuwaGameplayCue_SwordHit.cpp` | 2 | 1 | 1 |
| `Combat/Runtime/WuwaCombatExecutionComponent.cpp` | 10 | 6 | 4 |
| `Combat/Runtime/WuwaWeaponComponent.cpp` | 3 | 2 | 1 |
| `Core/WuwaStateTagComponent.cpp` | 6 | 4 | 2 |
| `Movement/Actions/WuwaMovementActionCapabilityComponent.cpp` | 12 | 10 | 2 |
| `Movement/Actions/WuwaMovementActionResources.cpp` | 14 | 10 | 4 |
| `Movement/Traversal/WuwaCharacterMovementComponent_Grapple.cpp` | 3 | 1 | 2 |
| `Movement/WuwaCharacterMovementComponent.cpp` | 44 | 37 | 7 |
| `Network/LAN/WuwaLanSessionSubsystem.cpp` | 23 | 9 | 14 |
| `Targeting/WuwaTargetingComponent.cpp` | 21 | 13 | 8 |
| `Traversal/Resolution/WuwaTraversalActionIntentProviderComponent.cpp` | 2 | 0 | 2 |
| `WuwaCharacter.cpp` | 14 | 9 | 5 |
| `WuwaGameMode.cpp` | 4 | 2 | 2 |
| `WuwaPlayerController.cpp` | 6 | 4 | 2 |
| `WuwaPlayerState.cpp` | 2 | 1 | 1 |

保留的 277 条日志均为 Error/Warning，覆盖核心装配、资产配置、RPC/网络载荷、Authority/Generation、状态所有权、委托、Gameplay Effect、Root Motion 与不可恢复清理失败。

## 7. 静态验收

| 门禁 | 结果 |
| --- | --- |
| 原工程保护 | 通过：输入清单摘要和发布区外 Git 状态项计数均未变化 |
| UBT 隔离 | 通过：`.ubtignore` 存在 |
| 删除边界 | 通过：100 个获批删除，0 个额外删除 |
| 新增 Gameplay 文件 | 通过：0 |
| 内存诊断代码耦合 | 通过：Source、Target、Build、uproject 和 Config 零命中 |
| Automation/Test 代码 | 通过：Test 目录及测试宏、类型、替身零命中 |
| Editor 工具 | 通过：三个类型和 Build.cs 的四个依赖零命中 |
| 项目内 Include | 通过：499 条可解析，缺失 0，大小写错误 0 |
| 条件编译 | 通过：176 个 C++ 文件的 `#if/#endif` 不平衡为 0 |
| 项目描述符 | 通过：`Wuwa.uproject` JSON 可解析 |
| 反射类型 | 通过：216 个，差异 0 |
| UFUNCTION | 通过：64 个，差异 0 |
| UPROPERTY | 通过：690 个，差异 0 |
| Gameplay Tag 声明 | 通过：60 个，差异 0 |
| 既有 C++ 注释正文 | 通过：发布版 2,944 条均能在原源码找到完全相同正文，新增/改写 0 |
| clang-format | 通过：Visual Studio clang-format 19.1.5，176 个文件 `--dry-run --Werror` 失败 0 |
| Target 编译 | 通过：UE 5.7 `WuwaEditor Win64 Development`，UBT 结果 `Succeeded` |
| 发布纯度 | 通过：无 uasset、umap、DLL、PDB、EXE、缓存、测试源码或删除插件文件 |

审计文档为说明删除历史，会出现被删除功能的名称；“零命中”门禁的扫描范围是可构建项目内容，即 `Source/**`、`Wuwa.uproject` 与 `Config/**`。

## 8. 编译验收

用户于 2026-09-04 单独授权编译。为避免在源码暂存区生成构建产物，将发布内容复制到系统临时目录，并仅在临时副本中移除 `.ubtignore`；编译前逐文件 SHA-256 校验为 188/188 一致，缺失、额外与内容差异均为 0。

| 项目 | 结果 |
| --- | --- |
| 引擎与工具链 | Unreal Engine 5.7；Visual Studio 2022 14.44.35227；Windows SDK 10.0.22621.0 |
| Target | `WuwaEditor Win64 Development` |
| UHT | 通过：处理完成，生成 174 个文件 |
| C++ 编译与链接 | 通过：最终字节版本全量完成 86/86 个 action，`UnrealEditor-Wuwa.dll` 链接成功，UBT `Result: Succeeded`，总耗时 134.04 秒 |
| 编译修复 | 清除 `WuwaActionCoordinatorComponent.cpp` 合并时残留的单个 `+` 语法字符；未改变函数、反射、网络协议或 Gameplay 架构 |
| 非阻断警告 | 3 条 UE 5.7 C4996：`FGameplayAbilitySpec::ActivationInfo` 1 条，网络更新频率字段直接访问 2 条 |
| 隔离与清理 | 构建产物只位于系统临时副本；验收后删除，不写入发布区或原工程 |

未启动 Unreal Editor，未运行 Automation、PIE、Gameplay Smoke、Cook 或 Package。

## 9. 未执行项

根据审批边界，本次仍未执行：

- Unreal Editor、Automation、PIE 或 Gameplay Smoke Test
- Cook、Package 或 Shipping 构建
- Content/Demo 压缩与 SHA-256 文件生成
- Git commit、tag、GitHub 仓库创建、Release 或资产上传

## 10. 后续最小验证矩阵

`WuwaEditor Win64 Development` 编译已经完成；获得相应授权后建议继续依次执行：

1. 打开默认地图，确认项目描述符、配置、Content 软路径和资产校验正常。
2. 单机验证移动、Sprint、普通跳跃/二段跳、Soft/Hard Target、目标切换、Grapple、Melee Combo/Hit Window、伤害、Poise/Stagger、死亡/重生和血条。
3. 两实例 Listen Server 验证 Legacy Action、Grapple、Targeting、Melee、Poise、死亡/重生及移动校正。
4. 编译目标发布配置并执行 Cook/Package；在干净机器完成 Demo 启动与基础 Gameplay Smoke。

Content 包应以项目实际资产依赖审计结果为白名单，只包含匹配源码版本所需的 `Content/**`；排除 DerivedDataCache、Intermediate、Saved、日志、源码和本机配置。若现有 Shipping Demo 的引擎版本、Content 哈希和源码标签完全一致且通过 Smoke，可复用；否则应重新 Package。

导出独立 GitHub 仓库时删除 `.ubtignore`，再补齐版本号、许可证、仓库地址和 Release 下载链接。当前四项均未提供，不应在发布前猜测。
