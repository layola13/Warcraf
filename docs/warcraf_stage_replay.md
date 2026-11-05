# S9 — 录像与回放

- 状态：已验证 2026-11-06（补做：00:22 定时轮次未执行，由本轮修复完成）
- 证据：`~/workspace/warcraf_ghidra/replay_dump.txt`（13 个录像相关函数 + 前 6 个反编译）、`strings` RTTI/名字符串统计
- 上承：S0 帧同步、S1 网络层（事件 0x1b 写 TempReplay.w3g、NET_COMMAND_SAVE、`CNetData::SetReplayState`）、S2 主循环、S6 UI

## 1. 总述

录像就是**确定性回放的输入日志**：对局进行中，网络事件按回合号写入临时文件 `TempReplay.w3g`；对局结束时可另存为正式录像。回放时不再跑网络层，而是把文件里的命令流按回合重放进同一台确定性模拟器——只要模拟是确定性的（S0），重放结果就与原局一致。录像文件本身只存输入（玩家命令 + 回合结构），不存模拟状态。

## 2. 录制路径

- 对局内录制：事件泵在分发器里，`GameStart`（事件 0x1b）开始写 `TempReplay.w3g`（S1 已验证）；录像开启标志在 `CNetData +0x1b44`，每条网络命令同时写一条 `NET_COMMAND_SAVE` 记录（`FUN_6f2fd0d0` 按回合记日志）。
- 录制状态机：`CNetData::SetRecordState`（`FUN_6f3128d0`，状态在 `+0xbe4`）；回放状态机：`CNetData::SetReplayState`（`FUN_6f312930`，状态在 `+0x614`，0→1→2→3→4 带合法性校验，非法转移记日志）。
- 自动保存：`AUTOSAVE_REPLAY_INFO` 字符串指向的函数 `FUN_6f2902c0`（2506 字节）——自动保存录像信息；`LastReplay`（2 处引用，`FUN_6f2dd210` 173 字节）管理最近一次录像。
- 文件名：`FUN_6f30b530`（151 字节）按 `"%s_%s_replay"`（两个名字字段，来自 `+0x22c8` 与 `+0x2348`）+ 目录 + `.w3g` 后缀拼出路径，再调 `FUN_6f3122c0`（触发保存，推断）。另有 `"%s\%s_%s.w3g"` 与 `"%s%s%s%s.w3g"` 两种格式串。
- 保存对话框：`FUN_6f2beba0`（397 字节，0 个静态调用者——UI 回调注册）初始化保存面板，绑定 `SaveReplaySaveButton` / `SaveReplayDeleteButton` / `SaveReplayCancelButton`。

## 3. 回放路径：加载器 `FUN_6f2da7f0`（547 字节，17 个被调函数）

1. 拼路径 `Replay\LastReplay.w3g`（Storm 503）。
2. `FUN_6f30eab0()` 检查文件；不存在 → `FUN_6f18cb00()`（报错 `Couldn't load replay file!`）。
3. 存在 → `FUN_6f30e360()` / `FUN_6f283400()` 读头部；取 0x104 字节字符串进 `+0x254`（地图名，推断），再由 `FUN_6f2c1280` / `FUN_6f2c19b0` / `FUN_6f2c16c0` 读出三项数值进 `+0x358/+0x35c/+0x360`。
4. **版本校验**：`FUN_6f30e860()` 的结果必须非零，且两个字段必须等于 `0x1b`（27）与 `0x17ab`（6059）——否则走 `FUN_6f2b2480(…, 0x1b, 0x17ab)` 报错路径并记 Storm 403 日志；通过则 `FUN_6f2edc20()` + `FUN_6f1bdb90()` 开始回放。
5. 失败分支统一报 `Couldn't get replay data!`。

版本常量 `0x1b` / `0x17ab` 的具体字段含义（哪个是格式版本、哪个是地图/引擎版本）**未验证**。

## 4. 数据结构与命令类型

- `TReplayUnit` + `TSExplicitList<TReplayUnit>` + `TSHashTable<TReplayUnit, HASHKEY_DWORD>`：回放单位表（按 DWORD 键哈希的单位记录）。
- 网络命令族（RTTI）：`CNetCommandReplayBase`（基类）、`CNetCommandReplayDataCompressed` / `CNetCommandReplayDataUncompressed`（录像数据块的两种形态——压缩与未压缩）、`CNetCommandReplayDataDone`、`CNetCommandReplayViewSpeedIncrement` / `CNetCommandReplayViewSpeedDecrement`（回放速度调节）、`CNetCommandSave` / `CNetCommandSaveDone`、`CNetEventSaveComplete`。
- 保存/加载类：`CBaseSaveLoad`（基类）、`CSave`、`CSaveGame`（源码头 `Game/CSaveGame.h`，构建机路径泄露）、`CGameSaveEventData`、`CGameSaveSplashDialog`。
- UI 类：`CReplayPanel`（源码 `UI/CReplayPanel.cpp`）、`CViewReplayScreen`、`CQuickReplayDialog` / `CQuickReplayConfirmDialog`、`CReplayButton`、`CSaveReplayPanel`、`CLoadSavedGameScreen`、`CEscMenuSaveGamePanel`。
- JASS 侧：native 名 `DoNotSaveReplay`（在注册表 `FUN_6f1e9a50` 内）——脚本可声明本局不保存录像。

## 5. 与其它阶段的关系

- S0 帧同步：录像成立的前提——回放不依赖网络，只重放命令流；确定性保证重放与原局一致。
- S1 网络层：录制就是事件流的落盘；`NET_COMMAND_SAVE` 与 `TempReplay.w3g` 都在网络层。
- S4 JASS2：录像命令由脚本触发的 native 调用产生；`DoNotSaveReplay` 是脚本侧的开关。
- S6 UI：保存/查看/快进（速度调节命令）都在 UI 层。

## 6. 关键函数地址表

| 地址 | 函数 | 作用 |
|---|---|---|
| 6f2da7f0 | FUN_6f2da7f0 | 录像加载器（547 字节，版本校验 0x1b / 0x17ab） |
| 6f30b530 | FUN_6f30b530 | 录像文件名拼接（`%s_%s_replay` + `.w3g`） |
| 6f2beba0 | FUN_6f2beba0 | 保存录像对话框初始化（绑定三个按钮） |
| 6f2902c0 | FUN_6f2902c0 | 自动保存录像信息（2506 字节，AUTOSAVE_REPLAY_INFO） |
| 6f2dd210 | FUN_6f2dd210 | LastReplay 相关（173 字节） |
| 6f28b570 | FUN_6f28b570 | 录像相关大函数（1952 字节，.w3g，未深究） |
| 6f2c4c30 | FUN_6f2c4c30 | 录像相关（520 字节，.w3g，未深究） |
| 6f312930 | FUN_6f312930 | CNetData::SetReplayState（127 字节，S1 已验证） |
| 6f3128d0 | FUN_6f3128d0 | CNetData::SetRecordState（95 字节，S1 已验证） |
| 6f3098d0 | FUN_6f3098d0 | 大分发器（事件 0x1b 写 TempReplay.w3g） |

## 7. 未验证点

1. 版本常量 `0x1b` / `0x17ab` 各自的字段含义。
2. 录像文件格式细节（头部结构、命令块编码、压缩算法）。
3. `FUN_6f28b570`（1952 字节）与 `FUN_6f2c4c30`（520 字节）的具体职责（疑似写入器与读取器）。
4. 回放速度调节命令的实际生效机制。
5. `TReplayUnit` 记录的字段布局。
