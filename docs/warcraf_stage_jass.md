# S4 — JASS2 虚拟机与脚本

- 状态：已验证 2026-11-05（补做：22:22 定时轮次未执行，由本轮修复完成）
- 证据：`~/workspace/warcraf_ghidra/jass_dump.txt`（18 个 JASS 相关函数 + 前 6 个反编译，含解释器核心与两张 native 注册表）、`strings` RTTI/名字符串统计
- 上承：S0 帧同步、S1 网络层、S2 主循环、S3 模拟核心

## 1. 总述

JASS2 是**栈式字节码虚拟机**：地图脚本（war3map.j）被编译成字节码，由解释器逐条取指、分发表执行；所有与引擎交互的能力都以 **native 函数** 的形式注册进一张大表（名字符串 + 类型签名 + 实现指针）；触发器系统（条件/动作/事件注册）是脚本与模拟之间的桥梁。脚本执行本身是确定性的——同一回合、同一输入，各端执行同一批触发器（与 S0 的锁步一致）。

## 2. 解释器核心 `FUN_6f7f9070`（6160 字节，56 个被调函数）

- 67 处引用含 "Jass" 的字符串/符号，是全 DLL 里 JASS 关联度最高的函数。
- 反编译确认是**取指—分发循环**：状态结构（param_1）里是指令指针（`param_1[5]`/`[6]`）、值栈（`param_1[6]`）、当前操作码（`+10` 处，初始 `0x149`）。
- 操作码分发表：`DAT_6fb6bc08` / `DAT_6fb6bf20` / `DAT_6fb6ba48` / `0x6fb6c4b0` 四张表按操作码索引；操作码范围检查 `< 0x9c`（156）与 `< 0x36`（54），即两级分发。
- 主 switch 按操作码分派（case 0/1/3/4/5 可见），栈顶值经 `param_1[8]` 流转。
- 铁证：函数内引用 `"Jass2/Nodes.h"`（`FUN_6f04d150(0, "Jass2/Nodes.h", 0)`，断言/致命错误路径）——这是虚拟机节点定义头文件的名字。
- 调用者只有 1 个（未深究身份，疑似脚本执行入口）。

## 3. Native 注册：两张巨表

| 函数 | 大小 | 证据 |
|---|---|---|
| `FUN_6f1e9a50` | 23,346 字节 | 1167 次调用 `FUN_6f7e3710`，每次带一个类型签名字符串 |
| `FUN_6f6f5d00` | 25,844 字节 | 同一套触发器/native 名字符串引用（11 处），与上表并列 |

- 注册 helper `FUN_6f7e3710`：每次调用传入一个 native 的名字（`&DAT_…` 字符串指针）与类型签名，形如 `"(RR)R"`、`"(RII)S"`、`"(Hhandle;)I"`、`"(SII)S"`、`"(SB)S"`——JASS 类型签名（R=real、I=integer、S=string、B=boolean、H=handle）。
- 1167 次注册 ≈ common.j + Blizzard.j 的全部 native 函数集（数量级吻合；精确对应关系未验证）。
- 两张表的分工（common.j native vs Blizzard.j native）**未验证**——`FUN_6f6f5d00` 的反编译样本里名字符串多以 `&DAT_…` 形式出现，未逐条对应。
- RTTI 佐证：`NAT IVETOKEN`（native 令牌）、`NativeFunc`（native 函数对象）、`JassHandle`、`JassString`、`JassFrame`（脚本栈帧）。

已确认存在于 native 名字符串表中的函数（节选，均为数据段字符串，可被脚本按名调用）：

- 触发器：`CreateTrigger`、`TriggerEvaluate`、`ExecuteFunc`、`TriggerSleepAction`、`GetTriggerUnit/Destructable/Player/Widget/EventId/EvalCount/ExecCount`、`GetTriggeringTrigger/Region/Trackable`、`TriggerRegisterPlayerEvent/ChatEvent/DeathEvent/DialogEvent/EnterRegion/LeaveRegion/GameEvent/GameStateEvent/PlayerStateEvent/PlayerAllianceChange/FilterUnitEvent`
- 单位与玩家：`CreateUnit/CreateUnitAtLoc/CreateUnitByName/CreateUnitPool`、`GetPlayerAlliance/Color/Controller/Handicap/Id/Name/Race/Score/Selectable/SlotState/StartLocation`
- 其它：`Condition`、`Filter`、`ForGroup`、`CreateTimer/TimerStart`、`CreateQuest`、`CreateLeaderboard`、`DisplayText/DisplayTextToPlayer` 等

## 4. 触发器系统

- RTTI：`TriggerActionNode`（动作链表节点）、`TriggerConditionNode`（条件链表节点）、`TriggerString@CTriggerStrings`（触发器字符串表）。
- 模型：触发器 = 事件注册表 + 条件链表 + 动作链表；`TriggerRegister*` 把事件挂到触发器上，事件发生时 `TriggerEvaluate` 按条件求值后执行动作；`ExecuteFunc` 按名执行函数；`TriggerSleepAction` 让触发器等待（与回合推进的交互未验证）。
- 触发器与 S3 模拟核心的关系：触发器动作最终调用 native 函数改模拟状态（建单位、改属性、发命令），而这些 native 调用发生在确定性模拟之内，因此各端一致（S0）。

## 5. 句柄与类型系统

- `JassHandle`：所有 JASS 句柄（单位、触发器、计时器、对话框等）的统一基类；`JassString`：脚本字符串；`JassFrame`：调用栈帧。
- `GameHashTableBooleanReg / HandleReg / IntegerReg / RealReg / StringReg`：游戏哈希表（按值类型分五个注册表），即 `SaveXxxHandle` / `LoadXxxHandle` 背后的存储。
- `ScriptData` / `ScriptError`：脚本数据与脚本错误对象；`Script2HandleReg`：脚本到句柄的注册表。

## 6. 脚本加载

- `common.j` 在二进制里有 2 处字符串引用（函数 `FUN_6f6fc280`、`FUN_6f8a0b00`、`FUN_6f1ef8b0`、`FUN_6f1efac0`、`FUN_6f1ef990` 相关）。
- `war3map.j`、`Blizzard.j`、`InitGlobals`：**0 处字符串命中**——地图脚本文件名与初始化函数名没有以明文形式嵌在 DLL 里，加载路径很可能经 MPQ 动态构造（S7）或由调用方传入，未验证。
- 其余 JASS 相关小函数（`6f7f3f20` 823 字节、`6f7f8610` 185 字节、`6f7e3200` 1229 字节、`6f7e4480` 516 字节等）未逐个反编译，身份未验证。

## 7. 关键函数地址表

| 地址 | 函数 | 作用 |
|---|---|---|
| 6f7f9070 | FUN_6f7f9070 | JASS2 字节码解释器主循环（6160 字节，67 处 Jass 引用，引用 Jass2/Nodes.h） |
| 6f1e9a50 | FUN_6f1e9a50 | Native 注册表 A（23,346 字节，1167 次注册调用） |
| 6f6f5d00 | FUN_6f6f5d00 | Native 注册表 B（25,844 字节） |
| 6f7e3710 | FUN_6f7e3710 | Native 注册 helper（名字符串 + 类型签名） |
| 6f7f3f20 | FUN_6f7f3f20 | JASS 相关（823 字节，未深究） |
| 6f6fc280 | FUN_6f6fc280 | common.j 相关（264 字节） |
| 6f7e3200 | FUN_6f7e3200 | JASS 相关（1229 字节） |
| 6f7f8610 | FUN_6f7f8610 | JASS 相关（185 字节） |

## 8. 未验证点

1. 两张 native 注册表的分工（common.j vs Blizzard.j）与逐条对应关系。
2. `war3map.j` 的加载路径（MPQ 内路径构造，S7 待验证）。
3. `TriggerSleepAction` 与回合推进的交互（等待是否跨回合、如何与锁步协调）。
4. 字节码格式本身（操作码表 `0x6fb6bc08` 等的具体语义）与编译器前端。
5. 解释器调用者（唯一调用者的身份）与脚本执行入口。
6. 其余 10 个 JASS 相关小函数的身份。
