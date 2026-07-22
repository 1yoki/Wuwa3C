# Day 5 执行计划：动作路由、落地语义与上下文 Sprint

> 本文档是 `项目计划.md` 中 Day 5 的独立执行版。后续编码按 Part 顺序进行；每完成一个 Part，先执行编译、自动化测试和对应手工验收，再进入下一 Part。

## 1. 当前检查点

| Part                                | 状态     | 验收结果                                                                    |
| ----------------------------------- | -------- | --------------------------------------------------------------------------- |
| Part 1 类型、标签与状态所有权       | ✅ 已完成 | Action/Input/State 类型、原生 Gameplay Tags、来源计数 Tag Handle 已落地     |
| Part 2 Input Command 与 FIFO Buffer | ✅ 已完成 | `Wuwa.Input.Buffer.FifoLifetime` 已通过                                     |
| Part 3 Action Router 生命周期       | ✅ 已完成 | `Wuwa.Action.Router.AdmissionLifecycle`、`Wuwa.Action.Router.Buffer` 已通过 |
| Part 4 Light/Heavy Landing 语义     | ✅ 已完成 | 完整 `WuwaEditor` 编译与 `Wuwa.Movement.Landing.Classification` 已通过      |
| Part 5 空中 Sprint 二段跳分流       | ✅ 已完成 | 完整编译与 PIE 手工矩阵验收通过；新增自动化按用户决定跳过                   |
| Part 6 地面前冲/后撤/Sprint Run     | ✅ 已完成 | Dash/Backstep、中段 Handoff、Sprint Run 与异常清理已通过 PIE 手工验收 |
| Part 7 动画、Debug 与总回归         | ⬜ 未开始 | Part 6 已完成，下一步进入 Part 7                                      |

真实 Gameplay Action Source 与 Movement Executor 已注册，空中 Sprint 和地面 Dash/Backstep 均已进入 Router 闭环；旧 `DoSprintPressed → RequestAirSprint` 兼容入口已移除。Part 6 已完成，下一步进入 Part 7 动画、Debug 与总回归。

## 2. Part 5 最终目标

完成以下可验证闭环：

```text
Sprint Pressed Command
→ FIFO Buffer
→ Gameplay Action Source 快照 MovementMode / WASD / ActorForward
→ Falling + Direction 非零：Directional Double Jump
→ Falling + Direction 为零：Backflip Double Jump
→ Router 准入并取得 Block.Input.Move
→ Movement Executor 设置一次权威速度并播放 Montage
→ Montage 播放期间 Character 忽略 WASD 位移
→ 正常 Montage End 通知 Router Completed
→ Router 释放 Block.Input.Move
→ 下一帧按当前真实 WASD 恢复空中控制
```

### 2.1 已冻结的行为规则

- 输入平台只支持 Windows 键盘与鼠标。
- `FWuwaInputCommand::Direction` 继续保存按键边沿时的 WASD 快照，但在 Part 5 只用于区分 Directional 与 Backflip。
- Direction 非零时，无论具体按下 W、A、S、D 或斜向组合，都沿触发瞬间的 `ActorForward` 执行 Directional Double Jump。
- Direction 为零时，沿触发瞬间的 `-ActorForward` 执行 Backflip，角色 Yaw 保持不变。
- Directional 与 Backflip 共用 `MaxJumpCount` 和 `bAirSprintConsumed`；任一路径成功后，同一滞空不能再次使用另一条路径。
- 两类二段跳都由 `CharacterMovement` 设置速度；Montage 必须是 In-Place 或不包含会与代码速度竞争的平移 Root Motion。
- 两个动作都在 Montage 播放期间授予 `Block.Input.Move`；鼠标 Look 不受影响。
- 正常释放边界是 Montage 完整结束。Cancelled、Interrupted、Failed、提前 Landed 和 OwnerDestroyed 必须立即释放，不能等待动画原定时长。
- Controller 保留真实 Move Intent，不因阻止标签而清零；这样玩家持续按键时能在动作结束后的下一帧恢复移动。
- 动画只提供时间和表现，不直接修改 Velocity、JumpCount、旋转、Gameplay Tag 或 Action Runtime。

### 2.2 保持不变的 Part 5 内容

- `BackflipZVelocity`、`BackflipBackwardSpeed` 和朝向覆盖配置仍进入 `UWuwaMovementProfile`，具有 C++ 安全默认值和资产校验。
- `EWuwaJumpType` 新增 `AirBackflip`；Directional 继续使用 `AirSprint`，避免破坏现有 AnimBP 分支。
- 成功路径统一清除 Jump Buffer、设置速度、增加 `JumpCount`、设置 `bAirSprintConsumed` 并递增 Jump Sequence。
- 后空翻仍需保存并恢复开始前的旋转策略，防止 `bOrientRotationToMovement` 把角色转向后移速度。
- 落地仍统一重置空中次数预算；Part 4 的 Landing Type/Source 语义不被 Part 5 改写。

## 3. 状态所有权与模块职责

| 数据/行为                               | 唯一权威                         | Part 5 职责                                               |
| --------------------------------------- | -------------------------------- | --------------------------------------------------------- |
| 输入边沿和 WASD 快照                    | PlayerController / Input Command | 生成不可变 Command，不判断动作合法性                      |
| Directional/Backflip 请求构建           | Gameplay Action Source           | 根据 MovementMode、Direction 和 ActorForward 生成 Context |
| 动作准入、当前动作、Granted Tag 句柄    | Action Router                    | 准入、取得/释放 `Block.Input.Move`、统一结束原因          |
| Capsule 速度、JumpCount、AirSprint 预算 | Movement Component               | 校验并提交一次二段跳物理事实                              |
| Montage、结束委托、后空翻旋转覆盖       | Movement Executor                | 启动和清理自己创建的运行资源                              |
| 持续移动是否写入 Movement               | Character 输入门面               | 标签存在时提交零意图并跳过 `AddMovementInput`             |
| 动画表现                                | AnimBP / Montage                 | 读取 JumpType/Sequence，报告 Montage 时间结束             |

Router 不设置速度、不播放 Montage；Movement Component 不消费 Input Buffer；AnimBP 不直接释放 `Block.Input.Move`。

## 4. Part 5 分段实现顺序

### Part 5A：冻结 Profile、Jump Type 与 Definition 配置

目标：先建立可编译的数据契约，不改变现有运行表现。

源码调整：

1. 在 `UWuwaMovementProfile` 增加：
   - `BackflipZVelocity`
   - `BackflipBackwardSpeed`
   - 仅在确有固定最短朝向需求时保留的 `BackflipFacingLockDuration`；正常输入阻止时长不使用该数值，而以 Montage End 为准。
2. 为三个数值提供 C++ 安全默认值、单位、Clamp 和 `IsDataValid` 校验。
3. 为 `EWuwaJumpType` 增加 `AirBackflip`，保持 `AirSprint` 枚举值与现有蓝图兼容。
4. 创建或配置：
   - `DA_Action_DoubleJump_Directional`
   - `DA_Action_DoubleJump_Backflip`
5. 两个 Definition 均配置：
   - 对应 `Action.Movement.DoubleJump.*` 标签
   - `MovementPolicy = CharacterMovement`
   - `GrantedTags` 包含 `Block.Input.Move`
   - 对应 Montage
   - 合法优先级、缓存和取消规则

停点验收：

- 完整 `WuwaEditor` 编译成功。
- 两个 Data Asset 通过数据验证。
- 运行表现尚未迁移，旧空中 Sprint 链仍可回归。

### Part 5B：接入真实 Gameplay Action Source

目标：让 Sprint Command 通过 Router 构建稳定的空中 Action Context，但先不执行速度和动画。

建议新增一个由 Character 装配的真实 Action Source（例如 `UWuwaCharacterActionSourceComponent`），实现 `IWuwaActionSource`。当前 Router 只保存一个 Action Source，因此该组件应作为玩家语义命令的统一请求入口，不能让 Movement/Combat 各自抢占 `SetActionSource`。

实现规则：

1. 只处理已配置的语义 Input Tag；未接入的 Attack/Grapple/Lock 等命令返回明确的不可构建结果，不播放动画。
2. 收到 `Input.Sprint` 时快照：
   - Command 中不可变的 `Direction`
   - 当前 `MovementMode`
   - `ActorForward.GetSafeNormal2D()`
   - Character/Movement 弱引用
   - 来源 Input Sequence
3. Falling 且 Direction 非零：
   - Action Tag/Definition 使用 Directional
   - `Context.WorldDirection = ActorForwardSnapshot`
4. Falling 且 Direction 为零：
   - Action Tag/Definition 使用 Backflip
   - `Context.WorldDirection = -ActorForwardSnapshot`
   - Context 同时保存开始时角色朝向，供旋转恢复验证
5. Part 5 与 Part 6 都不把二维 Direction 转为实际位移方向；Direction 只选择 Forward/Back 分支，真实方向统一来自触发瞬间的角色朝向快照。
6. Character 在 BeginPlay 中显式向 Router 设置 Source；初始化失败输出 Owner、依赖和 Definition 路径。

自动化建议：

- W/A/S/D 与斜向输入都选择 Directional。
- 不同具体 Direction 得到相同的 ActorForward WorldDirection。
- 零 Direction 选择 Backflip 和 `-ActorForward`。
- 相机 Yaw 改变但 Actor Yaw 不变时，Part 5 Context 方向不改变。
- 非 Falling 或无效 Definition 返回明确拒绝，不消费成已启动动作。

停点验收：

- Source/Router 测试通过。
- Debug 日志能看到 Input Direction、ActorForward Snapshot、Action Tag 和 Sequence。
- 尚未注册 Executor 时返回 `Rejected.NoExecutor`，不改 Velocity、JumpCount 或标签。

### Part 5C：Movement Component 二段跳权威 API

目标：把 Directional 与 Backflip 的物理事实统一收口到 Movement Component。

实现规则：

1. 增加清晰的 Movement API，分别执行 Directional Air Sprint 与 Backflip；或使用带明确 Jump Type/速度策略的窄请求结构，禁止 Executor 直接修改 `Velocity` 私有状态。
2. 两条路径统一校验：
   - Owner/Movement 数据有效
   - `IsFalling()`
   - `!bAirSprintConsumed`
   - `JumpCount < ConfiguredMaxJumpCount`
   - Context 世界方向有限且水平归一化后非零
3. Directional：水平速度沿 Context 的 ActorForward Snapshot，使用现有 `DoubleJumpForwardSpeed` 与 `DoubleJumpZVelocity`。
4. Backflip：水平速度沿 Context 的 `-ActorForward`，使用 `BackflipBackwardSpeed` 与 `BackflipZVelocity`。
5. 只有速度成功提交后才能原子更新：
   - `BufferedJumpExpireAt = -1`
   - `bAirSprintConsumed = true`
   - `++JumpCount`
   - `LastJumpType`
   - `++JumpSequence`
6. 失败路径不得部分消耗预算或改变 Jump Type。

停点验收：

- 两条成功路径只消费一次共享预算。
- 失败路径不改变速度、计数和 Sequence。
- Directional 水平速度始终与 ActorForward Snapshot 同向。
- Backflip 水平速度始终与 ActorForward Snapshot 反向。
- 普通 Jump、Coyote Time、Jump Buffer 和 Part 4 Landing 回归不受影响。

### Part 5D：Movement Executor、Montage 生命周期与移动输入阻止

目标：完成 Router → Executor → Movement → Montage End → Router Cleanup 闭环。

建议新增由 Character 装配并注册到 Router 的 Movement Executor（例如 `UWuwaMovementActionExecutorComponent`），后续 Part 6 继续复用它执行地面 Dash/Backstep。

Executor 实现规则：

1. `SupportsAction` 只接受自己明确支持的 Movement Action Definition。
2. `CanStartAction` 校验 Movement、Character、AnimInstance、Montage、MovementMode、共享预算和 Context；Montage 缺失不能以 Timer 静默代替正常路径。
3. `StartAction`：
   - 对 Backflip 保存并覆盖旋转策略
   - 调用 Movement Component 的权威 API
   - 成功后播放 Definition Montage
   - 持有本次 Action Tag、Montage/实例、结束委托和旋转覆盖运行态
4. 如果速度已提交但 Montage 启动失败，返回 false 前必须由统一 Failed 清理恢复 Executor 资源；预算是否回滚要在实现前冻结为原子策略，不能留下“动作拒绝但预算已消耗”的半完成状态。推荐先验证/播放 Montage 成功，再提交不可回滚的跳跃事实，或为 Movement 请求增加显式提交阶段。
5. 正常 Montage End：委托只调用 `Router->FinishCurrent(Completed)`。
6. Montage 被外部停止或替换：调用 `FinishCurrent(Interrupted)`。
7. `EndAction` 先解绑委托，再停止仍在播放的 Montage，最后恢复自己持有的旋转覆盖，避免停止动画时递归结束动作。
8. Executor 订阅真实 Landing Event；二段跳 Action 尚未结束却提前落地时调用 `FinishCurrent(Interrupted)`。
9. Owner EndPlay 由 Router 的 OwnerDestroyed 路径结束当前动作；Executor 不能留下委托或旋转覆盖。

Character 输入门面调整：

```text
SetLocomotionIntent(CurrentMoveIntent)
├─ Has Block.Input.Move
│  ├─ Movement.SetLocomotionIntent(Zero)
│  └─ 不调用 AddMovementInput
└─ 未阻止
   ├─ Movement.SetLocomotionIntent(CurrentMoveIntent)
   └─ 正常 AddMovementInput
```

- 不修改 `AWuwaPlayerController::InputIntent.MoveIntent`。
- 不阻止 Look、Jump Release 或其他未经 Definition 阻止的语义命令。
- Router 在调用 Executor `StartAction` 前取得 Granted Tag，因此启动当帧的 `SetLocomotionIntent` 就必须看到阻止状态。
- Router 结束时按来源句柄释放标签；玩家仍按着 WASD 时，下一帧恢复普通空中控制。

停点验收：

- 两种 Montage 播放期间连续改变 WASD 不改变 Capsule 的输入轨迹。
- Montage 播放期间鼠标 Look 正常。
- Montage 正常结束后，持续按键无需重新松开/按下即可恢复空中控制。
- Cancelled、Interrupted、Failed、提前 Landed 和 OwnerDestroyed 均不残留 `Block.Input.Move`、Montage Delegate 或旋转覆盖。
- 重复结束回调只进入一次 Router Finish。

### Part 5E：动画与资产接入

1. `AM_Jump_Second_FF` 作为 Directional Montage：
   - 确认没有与 CharacterMovement 速度竞争的平移 Root Motion
   - 动画朝向与角色正前方推进一致
   - Montage End 能稳定触发 Executor 委托
2. 创建/配置 `AM_Jump_Second_Backflip`：
   - 使用 In-Place 或不含水平 Root Motion 的动画
   - 后移由 CharacterMovement 速度产生
   - Montage End 能稳定触发 Executor 委托
3. AnimBP 只根据 `LastJumpType/JumpSequence` 区分表现，不读取 WASD 决定二段跳轨迹。
4. 若 State Machine 与 Montage 同时存在，必须明确 Slot 与状态机职责，禁止同一动画被两套图重复驱动。

停点验收：

- Directional 和 Backflip 分支可由 Jump Type 独立观察。
- 动画切换不重复触发 Jump Sequence。
- Montage Blend Out 完成后 Action、标签与输入状态一致。

### Part 5F：自动化、手工矩阵与交付

原计划的新增自动化覆盖如下；本轮按用户决定全部跳过，不再实现：

- Command Direction 分流。
- ActorForward/-ActorForward Context 快照。
- 共享预算只能成功消费一次。
- 失败路径不部分消耗预算。
- Router 正常结束/中断后释放 `Block.Input.Move` 来源句柄。

PIE 手工矩阵：

| 场景                           | 预期                                                                 |
| ------------------------------ | -------------------------------------------------------------------- |
| 空中按 W 后 Sprint             | 沿角色正前方二段跳，不沿相机方向                                     |
| 空中按 A/D/S/斜向后 Sprint     | 都沿角色正前方二段跳，具体按键只表示 Direction 非零                  |
| 空中不按 WASD 后 Sprint        | 沿角色后方真实后移并播放 Backflip，Yaw 保持不变                      |
| 二段跳 Montage 内反复切换 WASD | Capsule 不接受新的移动输入，鼠标 Look 正常                           |
| Montage 结束时仍按着 WASD      | 下一帧立即恢复普通空中控制                                           |
| Montage 中途被停止/替换        | Action 为 Interrupted，输入和旋转立即恢复                            |
| 靠近地面触发后提前落地         | Action 立即清理，不等待 Montage 原时长                               |
| 同一滞空连续尝试两类二段跳     | 第一次成功，后续因共享预算拒绝                                       |
| 二段跳后真实落地               | JumpCount 与 AirSprint 预算统一重置                                  |
| Owner 销毁/停止 PIE            | 无残留委托、Tag Handle 或旋转覆盖错误                                |
| 基础回归                       | Walk/Run、普通 Jump、Coyote、Jump Buffer、Light/Heavy Landing 均正常 |

## 5. Part 5 完成条件

只有同时满足以下条件才进入 Part 6：

- 完整 `WuwaEditor` 编译成功。
- Part 1～4 已有自动化测试继续通过。
- Part 5 新增规则测试按用户决定跳过；以完整 PIE 手工矩阵作为本 Part 验收依据。
- Directional/Backflip 的 ActorForward 方向规则在 PIE 中可重复验证。
- 两类二段跳的 WASD 阻止严格覆盖 Montage 播放期，正常结束后立即恢复。
- 所有异常退出路径释放 Action、Granted Tag、Montage Delegate 和旋转覆盖。
- 旧 `DoSprintPressed → RequestAirSprint` 兼容链只在 Router 闭环验证后移除，且没有双重消费同一个 Sprint Command。
- 文件变更、架构职责、输入到结果逻辑链和测试结果已回填到 `项目计划.md`。

验收记录（2026-07-21）：完整编译通过，Directional/Backflip 分流、角色朝向快照、Montage 期间 WASD 阻止、结束恢复、共享预算、异常清理与基础移动回归均已由用户在 PIE 中验收通过。`ABP_WuwaCharacter` 的 `DefaultSlot` 已开启 `Always Update Source Pose`，确保二段跳 Montage 覆盖期间底层 Main State 仍能进入 `Fall_Loop → Land`。Part 5 正式完成。

## 6. Part 6：地面前冲、后撤与 Sprint Run

### 6.1 最终目标

```text
Sprint Pressed Command
→ Gameplay Action Source 快照接地状态与 WASD Direction
→ Direction 非零：快照 ActorForward，构建 Ground Dash
→ Direction 为零：快照 -ActorForward，构建 Backstep
→ Router 准入并取得 State.Action.Dash / Block.Input.Move
→ Movement Executor 播放 In-Place Montage 并添加 Root Motion Source
→ Dash 到达 Montage Handoff Point 时重新读取真实 Move Intent
→ 有输入：提前完成 Dash、保留当前速度并进入短暂 Sprint Run
→ 无输入：继续 Dash 后半段恢复动画并按原边界结束
→ Sprint Run 的水平速度快速衰减到 RunSpeed 后自动回到普通 Run
```

Part 5 与 Part 6 统一采用角色朝向规则：Direction 只选择动作类型，不决定实际推进方向。

### 6.2 已冻结的行为与所有权

- 地面 Sprint 与空中 Sprint 继续使用同一份不可变 `FWuwaInputCommand::Direction`；MovementMode 决定空中或地面分支。
- 地面 Direction 非零时，`WorldDirection` 固定为触发瞬间的 `ActorForward`；W/A/S/D 与斜向组合都只表示“存在移动输入”。
- 地面 Direction 为零时，`WorldDirection` 固定为触发瞬间的 `-ActorForward`，并保持动作开始时的角色 Yaw。
- Root Motion Source 是地面 Dash/Backstep 的唯一 Capsule 位移权威；Montage 必须为 In-Place，不混用 Montage Root Motion、`LaunchCharacter` 或 `SetActorLocation`。
- Executor 创建并持有本次动作唯一的 Root Motion Source ID；Completed、Cancelled、Interrupted、Failed、离地和 OwnerDestroyed 都必须按 ID 移除。
- `Block.Input.Move` 只阻止 Character 写入持续移动，不清除 Controller 保存的真实 Move Intent。
- 只有 Forward Dash 到达资产标记的 Handoff Point 且该时刻仍存在真实 WASD Move Intent 时，才提前完成 Dash 并衔接 Sprint Run；不能等待完整 Montage End 再判断。
- Handoff Point 由 Dash Montage 的专用 Notify 提供表现时间，初始约在完整 1.3 秒动作的 0.7 秒处；Notify 只报告时间事件，Executor 仍负责校验 Action、接地、Move Intent 和资源归属。
- Sprint Run 是短暂的 Dash 出口减速阶段，不是“只要持续按键就无限保持”的高速跑步；继承 Dash 当前水平速度，速度快速降到 `RunSpeed` 后自动退出为普通 Run。
- Sprint Run 在 Move Intent 归零、离地、出现阻止标签、速度已降到 RunSpeed 或 EndPlay 时退出。Sprint 按键释放本身不作为单独退出条件。
- `TimeMappingCurve` 的 X 是标准化时间、Y 是标准化位移进度；空值表示线性，非空曲线必须从 `(0,0)` 单调到 `(1,1)`。

| 数据/行为 | 唯一权威 | Part 6 职责 |
| --------- | -------- | ----------- |
| WASD 与角色朝向快照 | PlayerController / Input Command / Action Source | Direction 选择分支，ActorForward 决定位移方向 |
| 动作准入、冲突、Granted Tag 句柄 | Action Router | 开始前检查，结束后统一释放 |
| Root Motion Source ID、Montage、旋转覆盖 | Movement Executor | 创建、持有并对称清理运行资源 |
| Capsule 碰撞移动 | CharacterMovement / Root Motion Source | 根据固定起点、终点、时长和曲线执行位移 |
| 当前真实 Move Intent 与 Sprint Run | Controller / Character / Movement Component | Dash Handoff Point 重新判断输入，并维护短暂减速状态 |
| 动画表现 | AnimBP / In-Place Montage | Montage Notify 只提供 Handoff 时间事件，不直接结束 Action 或写 Gameplay 状态 |

### 6.3 分段实现顺序

#### Part 6A：数据契约与资产

- ✅ **6A-1**：`FWuwaRootMotionSourceConfig` 已加入 Distance、Duration、TimeMappingCurve、bPreserveFacing；`UWuwaActionDefinition` 已接入条件显示与运行时/资产验证，完整编译通过。
- ✅ **6A-2**：Dash/Backstep 的标准化时间映射 Curve、In-Place Montage 和两份 Action Definition 已创建并通过用户资产验收；尚未注册地面运行分支。

#### Part 6B：Gameplay Action Source 地面分流

- ✅ **6B-1**：Source 已持有并验证两份地面 Definition，Character 蓝图完成资产注入；完整编译、初始化和 Part 5 二段跳回归通过。
- ✅ **6B-2**：接地且 Direction 非零时使用触发瞬间 `ActorForward`；Direction 为零时使用触发瞬间 `-ActorForward`。相机朝向和具体 WASD 键位都不参与实际位移方向。
- ✅ **6B-3**：Context 已保存 InputDirection、WorldDirection、FacingDirection、MovementMode、SourceObject 与 Sequence；地面请求以 `Rejected.NoExecutor` 到达 Router，未播放 Montage、授予标签或改变速度，Part 5 Falling 分流回归通过。

#### Part 6C：Movement Executor 的 Root Motion Source 启动

- ✅ **6C-1**：RMS 创建、唯一 ID 持有、按 ID 移除和 Runtime 占用判断已落地并编译通过；地面 Tag 尚未注册为已支持动作。
- ✅ **6C-2**：AirDoubleJump/GroundRMS 准入已拆分，接地事实、Direction 分支和 ActorForward/Backward 点积校验已编译通过；地面 Tag 尚未向 Router 暴露。
- ✅ **6C-3**：`Action.Movement.Dash.Forward` 与 `Action.Movement.Backstep` 已原子启用；固定终点、Duration、TimeMappingCurve、Montage 同步和 EndAction 按 ID 释放 RMS 的闭环已通过用户验收。Dash 的最终资产时长、前段位移曲线与中段 Handoff Point 在 6E 新需求中重新配置。

#### Part 6D：结束生命周期、离地与旋转覆盖

- ✅ **6D-1**：后空翻专用旋转覆盖已泛化为来源明确的 Facing Rotation Override；Backstep 根据 `bPreserveFacing` 取得同一资源，所有结束路径恢复开始前策略，完整编译通过。
- ✅ **6D-2**：Action 结束资源已收敛到统一幂等入口，固定顺序为：解绑 Montage 委托 → 停止 Montage → 移除 RMS ID → 恢复旋转 → 清空 Runtime → Router 释放 Granted Tags；完整编译通过。
- ✅ **6D-3A**：Movement Component 已广播真实 MovementMode 变化；地面动作从 Walking/NavWalking 进入 Falling 时，Executor 立即以 Interrupted 结束，不等待 Montage 或 RMS 原时长，完整编译通过。
- ✅ **6D-3B**：Action Definition 已接入通用冷却，Router 在 Executor 成功启动后记录按 ActionTag 隔离的截止时间；Cooldown Buffer 使用一次性 Timer 到期重试，完整编译通过且 Dash/Backstep 资产已配置。
- ✅ **6D-3C**：Character Action Source Definition 聚合校验已恢复；Ground→Falling 使用一次性 `MaintainLastRootMotionVelocity`，Dash/Backstep 冲出边缘保留水平惯性，动作中主动 Jump 保留向上速度；正常完成仍零速，完整 PIE 生命周期回归通过。

#### Part 6E：Forward Dash 衔接 Sprint Run

- ✅ **6E-1**：Character 已保存 Controller 每帧提交的真实 Move Intent 快照；`Block.Input.Move` 期间只阻止向 Movement 下传，完整编译通过。
- **旧 6E-2 方案废弃**：不再在完整 Montage End 的 `EndAction(Completed)` 中创建 Pending Handoff；源码中已经按旧指导加入的 `RequestSprintRunHandoff/ClearSprintRunHandoff` 与 EndAction 判断需要在修订步骤中移除或替换。
- ✅ **6E-2R**：旧 Pending Handoff 与完整 Montage End 判断已移除；Executor 订阅并过滤 Dash Montage 的专用 Handoff Notify，到点后仅当当前仍是 Forward Dash、接地、真实 Move Intent 非零时，才以成功衔接语义提前结束 Dash，并在释放 RMS 时保留当前水平速度；完整编译通过。
- ✅ **6E-3R**：Router 完成 Executor 清理、Granted Tags 释放及 Buffer 同步重试后，同一 Handoff 调用链进入短暂 Sprint Run；若已有其他 Buffered Action 启动，则取消衔接；完整编译通过。
- ✅ **6E-4R**：Sprint Run 继承 Dash 出口速度，以 `RunSpeed` 为收敛目标；Sprint Run 期间 `MaxWalkSpeed` 跟随当前衰减速度，避免 CharacterMovement 内置超速制动抢先压回 RunSpeed，专用 `SprintRunDeceleration` 成为唯一水平减速权威；Move Intent 归零、离地、受阻或速度降到 RunSpeed 时退出。完整编译与 PIE 出口减速验证通过。
- ✅ **6E-5R**：Dash Montage 已恢复完整动作段并添加中段 Handoff Notify；TimeMappingCurve 将减速变化点从 0.3 秒延后至 0.35 秒，避免第 8 Tick 的 Notify 与曲线降速同帧发生，Handoff 时已能保留有效出口速度并成功进入 Sprint Run。Backstep 不添加该 Notify。

#### Part 6F：PIE 手工矩阵与交付

- ✅ **6F-1**：Forward Dash 正向 Handoff 闭环已通过 PIE 手工验收，角色朝向位移、真实 WASD 重读、出口速度继承、Sprint Run 动态限速和平滑收敛均符合预期。
- ✅ **6F-2**：Handoff 无输入分支与 Backstep 分支已通过 PIE 手工验收，两者均不进入 Sprint Run。
- ✅ **6F-3**：离地、跳跃、Montage 中断、重复输入等异常退出，以及 Part 5 空中二段跳与基础移动回归均已由用户验收。
- ✅ **6F-4**：最终验收结果、已知非阻塞问题与架构文档已回填，Part 6 完成交付。

| 场景 | 预期 |
| ---- | ---- |
| 地面按 W/A/S/D/斜向后 Sprint | 都沿触发瞬间的角色正前方前冲 |
| 前冲期间旋转相机或改变 WASD | 已开始的 Root Motion Source 方向不改变；相机朝向不影响轨迹 |
| 地面完全不按 WASD 后 Sprint | 沿触发瞬间的角色后方后撤，Yaw 保持不变 |
| Dash/Backstep 中途离地或 Montage 中断 | Action 立即结束，RMS、标签、委托与旋转覆盖全部释放 |
| Forward Dash 到达 Handoff Point 且仍按 WASD | 提前停止 Dash 后半段，释放标签并直接进入短暂 Sprint Run |
| Forward Dash 到达 Handoff Point 时已松开 WASD | 不衔接，继续 Dash 后半段恢复动画并正常结束 |
| Backstep 完成后按住 WASD | 恢复普通移动，但不进入 Sprint Run |
| Sprint Run 保持 WASD | 继承出口速度并快速衰减，达到 RunSpeed 后自动回到普通 Run |
| Sprint Run 中松开 WASD、跳跃或受阻 | 立即退出 Sprint Run |
| 空中按 Sprint | 仍按 Part 5 规则执行 Directional/Backflip 二段跳 |

已知非阻塞问题：Dash/Backstep 从平台边缘离地时，低帧率下的水平飞出距离会比高帧率更远。原因是 Root Motion Source 按整帧 `DeltaTime` 准备 Override Velocity，离地时又使用 `MaintainLastRootMotionVelocity` 继承该帧平均速度；`MaxSimulationTimeStep` 只细分 CharacterMovement 碰撞步进，不会重新采样 RMS 曲线。当前差异在可接受范围内，由用户决定延后处理，不阻塞 Part 6 交付。

### 6.4 当前停点

Part 6A～6F 已完成并由用户完成 PIE 手工验收。地面 Dash/Backstep 已接入 Source、Router、Executor 和 Root Motion Source 统一闭环；Forward Dash 可在中段 Notify 根据真实 WASD 衔接 Sprint Run，并由 `SprintRunDeceleration` 独立平滑收敛至 `RunSpeed`。低帧率下从边缘离地飞行距离偏大已记录为延后的非阻塞小问题。下一停点为 Part 7 动画、Debug 与 Day 5 总回归。
