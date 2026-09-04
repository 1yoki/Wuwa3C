# Wuwa 3C

> A third-person action 3C technical demo built with Unreal Engine 5.7 and C++.

Wuwa 是一个聚焦角色、镜头与操作（Character、Camera、Controls）的第三人称动作技术 Demo。项目通过自定义 Gameplay 框架串联移动、动作仲裁、目标锁定、镜头、自由钩锁、Gameplay Ability System（GAS）战斗和网络同步，用于展示可配置、可预测且职责清晰的动作玩法实现。

> 当前版本：`v0.1.0`（Source-only）。

## 目录

- [Wuwa 3C](#wuwa-3c)
  - [目录](#目录)
  - [核心特性](#核心特性)
    - [角色移动与动作](#角色移动与动作)
    - [目标锁定与镜头](#目标锁定与镜头)
    - [自由钩锁](#自由钩锁)
    - [GAS 战斗](#gas-战斗)
    - [联机与调试](#联机与调试)
  - [架构概览](#架构概览)
    - [1. 意图驱动的固定管线](#1-意图驱动的固定管线)
    - [2. 数据驱动的动作规则](#2-数据驱动的动作规则)
    - [3. 分域协作与表现解耦](#3-分域协作与表现解耦)
    - [4. 网络预测与服务端权威](#4-网络预测与服务端权威)
    - [源码目录](#源码目录)
  - [操作方式](#操作方式)
  - [获取与运行](#获取与运行)
    - [发布状态](#发布状态)
    - [从源码构建](#从源码构建)
  - [仓库内容](#仓库内容)
  - [发布文件](#发布文件)
  - [项目边界](#项目边界)
  - [许可证与资产](#许可证与资产)

## 核心特性

### 角色移动与动作

- 走、跑、冲刺、普通跳跃、土狼时间、跳跃缓存和二段跳
- Dash、Backstep 等由方向和环境规则解析的独占动作
- 自定义 `UCharacterMovementComponent`、Root Motion Source 与 Saved Move 网络预测
- 动作请求 FIFO、优先级、打断、Prepare/Commit/Rollback 和统一结束生命周期

### 目标锁定与镜头

- Soft Target、Hard Lock 和目标切换
- 基于距离、视角与可见性的候选评分
- Exploration/LockOn Camera Mode、目标构图与有边界的朝向修正
- SpringArm 碰撞恢复及动作驱动的短时 Camera Feedback

### 自由钩锁

- 无预设锚点的空间查询与分段 Sweep
- 输入时冻结查询上下文，提交前重新校验
- `MOVE_Custom` 钩锁物理、预测轨迹、出口速度和网络校正
- Montage、Niagara 绳索、Spline Mesh 与镜头反馈的独立表现层

### GAS 战斗

- Ability System Component 位于 `PlayerState`，跨角色重生保留玩家能力身份
- 数据驱动的 Ability Set、Attribute Set、Gameplay Effect 与 Gameplay Tag
- 本地预测的轻攻击和连段输入窗口
- 权威命中、伤害执行、生命、Poise、Stagger、死亡与重生闭环
- 武器 Trace、Gameplay Cue 和世界空间血条

### 联机与调试

- 服务器权威的动作、战斗与目标状态校验
- Character Movement 预测以及 Movement Action/Grapple 的请求与校正路径
- 基于 `OnlineSubsystemNull` 的局域网房间创建、发现和加入界面
- 非 Shipping 构建可用的 F10 只读运行时可视化

## 架构概览

Wuwa 的核心思路是把“玩家想做什么”与“系统如何执行”分开，把瞬时输入与持续运行状态分开。角色对象负责装配，具体状态仍由移动、目标、镜头、动作和战斗等领域各自维护。

### 1. 意图驱动的固定管线

输入不会直接调用某个动作实现，而是先转换为带语义的命令，再按固定阶段交给唯一处理者：

```text
输入采样
   ↓
只读角色快照
   ↓
规则解析
   ├── 即时行为：移动、目标切换
   ├── 独占动作：Dash、Backstep、Grapple 等
   └── GAS 输入：攻击与能力
          ↓
   领域执行与事实回传
          ↓
     统一结束和清理
```

每帧的 Gameplay 顺序保持为：

```text
事实收集 → 状态快照 → 意图解析 → 即时行为 → 独占动作 → GAS 输入 → 收尾
```

输入回调只记录命令，真正的状态变化发生在对应阶段。这样可以避免组件 Tick、蓝图绑定或回调先后顺序改变同一帧的裁决结果。

### 2. 数据驱动的动作规则

数据配置决定“什么条件下选择什么动作”，C++ 执行层负责“这个动作具体怎么运行”。

| 配置层   | 负责内容                                                           |
| -------- | ------------------------------------------------------------------ |
| 输入规则 | 根据输入语义、按下/释放、地面或空中状态以及方向条件选择动作        |
| 动作定义 | 配置状态要求、阻塞条件、优先级、取消关系、输入缓冲、冷却和结束事件 |
| 领域参数 | 配置移动手感、目标评分、镜头构图、钩锁轨迹、武器和攻击数据         |

规则遵循以下约束：

- 同一输入可以按方向和移动环境派生不同动作，但最终必须得到唯一结果；相同优先级的歧义配置会被拒绝。
- “选择哪个动作”和“能否打断当前动作”是两层独立裁决，避免输入规则和运行状态互相污染。
- 输入命中时会冻结方向、朝向、移动环境和必要的领域上下文，进入队列后不会因状态变化被重新解释成另一个动作。
- 暂时无法执行的请求可以在有效期内按 FIFO 等待；后续请求不会越过仍有效的队首。
- 动作替换除了满足优先级，还必须同时满足双方声明的取消关系。
- 配置资产在编辑器和运行时都会验证关键标签、数值、引用和规则歧义。

现有动画、移动或钩锁原语能够表达的新动作，通常只需增加配置和映射；只有出现新的执行机制时才扩展对应领域代码。配置负责选型和参数，执行器负责副作用，表现层不参与 Gameplay 权威裁决。

实际 Data Asset 实例位于 Content 中，不包含在 `v0.1.0` 源码仓库里。

### 3. 分域协作与表现解耦

| 领域       | 主要职责                                               |
| ---------- | ------------------------------------------------------ |
| 输入与调度 | 采集输入、建立帧序和固定处理阶段                       |
| 动作仲裁   | 管理独占动作、缓冲、打断、提交、回滚与统一结束         |
| 移动       | 拥有速度、移动模式、跳跃、Root Motion 和钩锁物理       |
| 目标系统   | 搜索、评分并维护唯一的 Soft/Hard Target 上下文         |
| 镜头       | 只读消费目标与移动状态，负责模式、构图、碰撞和短时反馈 |
| Traversal  | 负责钩锁查询、轨迹和动作上下文，不侵入通用动作仲裁     |
| GAS 与战斗 | 管理能力、属性、效果、武器命中、伤害、硬直和死亡       |
| 动画与表现 | 播放 Montage、Niagara、绳索和 UI，不反向拥有玩法状态   |

跨领域通信以语义命令、状态标签和只读事实为主。目标、移动或战斗状态只保留一份权威来源，其他系统通过读取上下文或订阅结果协作，避免维护互相漂移的状态副本。

### 4. 网络预测与服务端权威

- 拥有者客户端可立即预测移动和动作表现，服务端根据权威状态与世界查询重新裁决。
- 服务端拒绝或预测误差超出容差时，客户端回滚对应动作，位置误差交给 Character Movement 校正。
- 目标选择由服务端复核；武器命中、伤害、Poise、死亡等 Gameplay 结果由服务端确定。
- 远端角色只同步必要的动作状态和开始时间，再在本地重建动画、绳索等表现，不复制完整的本地输入队列。

### 源码目录

| 目录                        | 职责                                                       |
| --------------------------- | ---------------------------------------------------------- |
| `Source/Wuwa/Actions`       | 意图解析、队列、动作仲裁与网络入口                         |
| `Source/Wuwa/AbilitySystem` | GAS 初始化、输入路由、Ability、Effect 与动作互操作         |
| `Source/Wuwa/Movement`      | Locomotion、跳跃、Root Motion、Saved Move 与钩锁物理       |
| `Source/Wuwa/Traversal`     | 自由钩锁查询、轨迹、执行与表现                             |
| `Source/Wuwa/Targeting`     | 候选收集、评分、Soft/Hard Target 与目标切换                |
| `Source/Wuwa/Camera`        | Camera Mode、构图规则、SpringArm 和反馈栈                  |
| `Source/Wuwa/Combat`        | 武器、攻击窗口、伤害、属性、Poise、Stagger 与 Gameplay Cue |
| `Source/Wuwa/Core`          | Gameplay Tags、状态标签与公共基础约束                      |
| `Source/Wuwa/Messaging`     | 输入命令、Gameplay Fact 和固定阶段调度                     |
| `Source/Wuwa/Network`       | 局域网 Session                                             |
| `Source/Wuwa/UI`            | 局域网房间界面与世界空间血条UI                             |

## 操作方式

以下键位来自当前版本的 `IMC_Gameplay`。实际输入以与源码版本配套的 Content 包为准。

| 操作               | 键鼠输入                  |
| ------------------ | ------------------------- |
| 移动               | `W` / `A` / `S` / `D`     |
| 调整视角           | 移动鼠标                  |
| 跳跃 / 释放跳跃    | `Space`                   |
| 冲刺及方向派生动作 | 鼠标右键                  |
| 轻攻击 / 连段      | 鼠标左键                  |
| 自由钩锁           | `T`                       |
| 锁定 / 取消锁定    | 鼠标中键                  |
| 切换锁定目标       | 鼠标滚轮或 `Q` / `E`      |
| 切换调试可视化     | `F10`，仅非 Shipping 构建 |

## 获取与运行

### 发布状态

`v0.1.0` 仅发布源码，不提供公开 Content 包或 Windows Demo。当前仓库可以用于阅读、审查和编译 C++ 模块。


### 从源码构建

准备环境：

- Unreal Engine 5.7
- Visual Studio 2022，并安装“使用 C++ 的游戏开发”工作负载和 Windows SDK

步骤：

1. Clone 或下载本源码仓库。
2. 右键 `Wuwa.uproject`，选择 **Generate Visual Studio project files**。
3. 使用 Visual Studio 构建 `WuwaEditor`、`Win64`、`Development`。

也可以在 PowerShell 中直接调用 UBT：

```powershell
& "<UE_5.7>\Engine\Build\BatchFiles\Build.bat" `
    WuwaEditor Win64 Development `
    "-Project=<repo>\Wuwa.uproject" `
    -WaitMutex
```

`<UE_5.7>` 和 `<repo>` 分别替换为本机 Unreal Engine 5.7 与仓库根目录的绝对路径。

完整运行仍需要与源码 tag 完全一致的 Content。`v0.1.0` 未公开该资源包，因此打开项目时出现缺失地图或资产属于预期边界，不代表 C++ 编译失败。

## 仓库内容

| 路径                      | 内容                                           |
| ------------------------- | ---------------------------------------------- |
| `Source/`                 | `Wuwa` Runtime Module，以及 Game/Editor Target |
| `Config/`                 | 运行所需的 Engine、Game 与 Input 配置          |
| `Wuwa.uproject`           | Unreal 项目描述符                              |
| `LICENSE`                 | MIT License                                    |
| `THIRD_PARTY_NOTICES.md`  | Unreal Engine 与非源码资产边界说明             |
| `SOURCE_RELEASE_AUDIT.md` | 源码净化、删除边界、静态门禁和构建记录         |

本源码仓库不包含 `Content`、项目插件、自动化测试、缓存、构建产物或可执行文件。

## 发布文件

`v0.1.0` 是 Source-only 版本，不附带二进制 Release 资产。

## 项目边界

- 当前目标是展示第三人称动作 3C 与 Gameplay 架构，不覆盖完整关卡、剧情、AI、存档或商业化系统。
- LAN 使用 `OnlineSubsystemNull`，不包含互联网账号、平台大厅或线上匹配服务。
- 自动化测试、GameMemAdvisor 诊断代码和一次性 Editor Library 不属于公开源码发布范围。
- `v0.1.0` 不发布 Content 和可执行 Demo。

## 许可证与资产

本仓库中由项目作者提供的源码采用 [MIT License](LICENSE) 发布。

MIT License 不覆盖 Unreal Engine、Epic Games 内容或任何第三方资产。`v0.1.0` 未包含 Content、Engine 二进制或项目插件；详细边界见 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)。
