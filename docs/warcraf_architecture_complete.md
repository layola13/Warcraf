# 魔兽争霸 III 1.27a 引擎完整架构 — S12 汇总与交叉验证

- 完成日期：2026-11-06
- 目标：Warcraft III 1.27.0.52240（Game.dll 13.2 MB、Storm.dll 334 KB、Mss32.dll、ijl15.dll、msvcp120/msvcr120.dll，32 位 PE，MSVC 2013）
- 证据底座：Ghidra 12.2 + JDK 25，工程 `~/workspace/warcraf_ghidra/war3`（97,839 个函数）、`storm`；所有地址均为 Game.dll 加载基址 `0x6f000000` 下的虚拟地址，Storm.dll 地址以 `0x150xxxxx` 计
- 方法：S0–S11 共 13 份阶段文档（约 2,666 行）全部以反编译证据为准；本轮只做汇总、交叉核对与总图，不新挖函数，不编数字、不编函数名
- 合规边界：仅用于学习引擎如何工作，不复制引擎代码与资产，不绕过 DRM/反作弊，不做游戏作弊

## 1. 一句话结论

War3 1.27a 是一台**确定性锁步模拟机**：GameMain 只是一个 155 字节的薄壳，真正的引擎是 `FUN_6f05f710` 的 `while(true)` 循环驱动的一台 25 ms 固定步进状态机；所有玩家输入先变成 `CNetCommand*` 对象、随回合（Turn）经 Storm SNet 同步，等所有人到齐后才由 `FUN_6f30bac0` 统一应用到模拟；模拟状态由七子系统校验和 + 事件流滚动校验 + 迷雾校验三层验证，跑偏即以 `TrustedDesync` 事件定性。渲染（Gx）、UI（Frame）、音频（Miles）、脚本（JASS2）、文件（MPQ）全部是这台模拟机外围的**服务子系统**：只消费模拟状态、不产生模拟输入（音频/UI/渲染的快慢不影响锁步）；唯一能改模拟的入口是回合命令包与 JASS native。

## 2. 完整架构总图（文字版）

```
                        ┌────────────────────────────────────────────────┐
                        │  GameMain 0x6f0297d0（唯一导出，155 字节薄壳） │
                        │  DllMain / 单实例检查 / 进程加固 / 回调注册    │
                        └──────────────────────┬─────────────────────────┘
                                               │ FUN_6f027fb0 启动序列
        ┌──────────────────┬───────────────────┼───────────────────┬──────────────────┐
        │ Storm/MPQ 挂载   │  UI 初始化        │  Gx 图形初始化    │  其余子系统      │
        │ FUN_6f020e20     │ FUN_6f0280e0      │ FUN_6f027e20      │  音频/网络/JASS │
        │ War3/War3X/补丁链│ FrameDef.toc      │ D3D9/OpenGL 工厂  │  （S7/S8/S4）    │
        └──────────────────┴───────────────────┼───────────────────┴──────────────────┘
                                               ▼
                        ┌────────────────────────────────────────────────┐
                        │  引擎循环 FUN_6f05f710（564 字节，while(true)）│
                        │  EvtSched 调度 + Win32 消息泵 FUN_6f1532b0    │
                        │  状态机 FUN_6f312740（0–6 迁移表）            │
                        └──────────────────────┬─────────────────────────┘
                                               │ 25 ms 固定步长（累加器钳 4000，看门狗 200 ms）
                                               ▼
                        ┌────────────────────────────────────────────────┐
                        │  帧步进 FUN_6f30ff00（1148 字节）              │
                        │   ├─ 事件泵 FUN_6f30ade0（954 字节）           │
                        │   │    ├─ 前处理 FUN_6f307980                 │
                        │   │    ├─ 大分发器 FUN_6f3098d0（0x01–0x31）   │
                        │   │    ├─ 扩展分发器 FUN_6f307b50（≥0x40，BN） │
                        │   │    └─ 后处理 FUN_6f307880                 │
                        │   └─ 回合应用 FUN_6f30bac0（命令 0x10–0x31）  │
                        └──────┬───────────────────────┬─────────────────┘
                               │ 命令对象入队          │ 回合包出入
                               ▼                       ▼
              ┌────────────────────────┐   ┌────────────────────────────┐
              │ 模拟核心（S3）          │   │ 网络层（S1）+ 帧同步（S0） │
              │ CUnit/CAbility/CBuff   │   │ TurnsSync 三件套           │
              │ 伤害 FUN_6f6af530      │   │ FUN_6f310c80 / 6f30b1a0     │
              │ AoE FUN_6f6b19e0       │   │ NetRouter 锁步模式         │
              │ 区域枚举 FUN_6f661060  │   │ CTurnsSyncStore 等齐放行   │
              └──────┬─────────────────┘   └──────────────┬─────────────┘
                     │ JASS native 注册表                │ 校验和交换比对
                     ▼                                   ▼
              ┌────────────────────────┐   ┌────────────────────────────┐
              │ JASS2 虚拟机（S4）      │   │ 七子系统校验和（S0/S11）    │
              │ 解释器 FUN_6f7f9070    │   │ cheat/combat/game/net/    │
              │ 注册表 6f1e9a50/6f6f5d00│   │ rand/tempest/unit + 雾     │
              │ 触发器 / 句柄 / 哈希表  │   │ rand checksum FUN_6f3130c0 │
              └────────────────────────┘   │ 不一致 → TrustedDesync     │
                                           └────────────────────────────┘
        外围服务（只读模拟状态，不改模拟）：
        Gx 渲染 S5 ── CGxDevice 双后端（D3D9/OpenGL）── UI Frame S6 ── CFrame 树
        音频 S8 ── Miles 6.1d / CSoundWar3 ── 文件 S7 ── Storm MPQ ── 录像 S9 ── .w3g
        Battle.net S10 ── CONNSTATE 13 态 / 大厅 / 好友 / 战队 / 天梯
        随机数 S11 ── 查表 RNG FUN_6f199400（周期 8,965,109）── 软件浮点
```

## 3. 各阶段一览（S0–S11 全部已验证）

| 阶段 | 主题 | 文档 | 核心已验证结论 |
|---|---|---|---|
| S0 | 帧同步核心 | `warcraf_framesync_analysis.md`、图 `warcraf_framesync_architecture.html` | 确定性锁步；TurnsSync 三件套；事件泵 `FUN_6f30ade0`；大分发器 `FUN_6f3098d0`；七子系统校验和；滚动校验 xor/rotl3、常数 `0x7a2d` |
| S1 | 网络层完整 | `warcraf_stage_network.md` | Storm 导入表（112 序号 + 1 具名）；Net 函数簇 1806 个；双分发器（0x01–0x31 游戏内 + ≥0x40 Battle.net）；TurnsSync 出入与回合包解析；Storm 序号→VA 对照；已验证 548/501/403 |
| S2 | 主循环与入口 | `warcraf_stage_mainloop.md` | GameMain `0x6f0297d0`；引擎循环 `FUN_6f05f710`；帧步进 `FUN_6f30ff00`（25 ms 固定步长，累加器钳 4000，看门狗 200 ms）；EvtSched；消息泵 `FUN_6f1532b0`；状态机 `FUN_6f312740`（0–6 迁移表）；回合应用 `FUN_6f30bac0`（命令 0x10–0x31） |
| S3 | 模拟核心 | `warcraf_stage_simulation.md` | 命令解包薄壳 `FUN_6f2fxxxx` → `FUN_6f30ba60` → `FUN_6f30b940`；24 字节命令对象与链表队列；CDataStore 读写原语；WidgetDamageData 12 字节/条；AoE 伤害 `FUN_6f6b19e0`；单体伤害/治疗 `FUN_6f6af530`（虚函数 0x120/0x128）；区域枚举 `FUN_6f661060`（~130 调用者）；CAbility 492 类 / CBuff 226 类 / CUnit 51 类；英雄复活 `FUN_6f678380`；JASS native 签名表 `FUN_6f1e9a50` |
| S4 | JASS2 虚拟机 | `warcraf_stage_jass.md` | 解释器主循环 `FUN_6f7f9070`；两张 native 注册表（1167 次注册，类型签名）；触发器系统；句柄与哈希表 |
| S5 | Gx 渲染抽象 | `warcraf_stage_render.md` | CGxDevice 双后端工厂 `FUN_6f0e2f70`（0=OpenGL，1=D3D）；D3D9 动态加载 `FUN_6f0ec530`（LoadLibrary d3d9.dll + Direct3DCreate9 0x20）；OpenGL 静态导入；GL 能力探测 `FUN_6f0ebb40`；纹理上传 `FUN_6f0eb600`；场景回读两后端同构 |
| S6 | UI / Frame 系统 | `warcraf_stage_ui.md` | CLayer→CFrame→CSpriteFrame→CControl 继承链；九宫格渲染；FrameDef 哈希表与注册表；锚点/布局失效链表；CTextFrame/CMessageFrame/CEditBox/CChatDisplay；CScreenFrame 懒创建子 Frame |
| S7 | Storm 文件层 MPQ | `warcraf_stage_mpq.md` | Storm 814 导出（54 具名，SFile 19 个）；Game 零具名导入、26 个无名序号；SFile 五态分发；MPQ 头/哈希/块表解密；优先级补丁链（War3/War3X/War3XLocal/War3Patch + teen→t/tx.mpq）；CryptoAPI 签名验证（BLIZZARDKEY）；CORRUPT_* 30 码 |
| S8 | 音频 Miles | `warcraf_stage_audio.md` | CSoundWar3/CSoundManager；Miles 6.1d；Game 导入 110 个 AIL_*（2 死导入）；驱动/XMIDI/DLS 打开链；三类 16 条句柄池；播放分发器 `FUN_6f171ba0`（流/序列/2D/3D）；抢占 `FUN_6f171380`（24 声上限）；3D 听者变化检测 + 单声参数下发 `FUN_6f16cbe0`（EAX2）；统一音量 `FUN_6f173cb0`（距离衰减 sqrt/pow）；文件回调走 Storm 267/269/271/253；22050Hz 警告 |
| S9 | 录像与回放 | `warcraf_stage_replay.md` | 录像加载器 `FUN_6f2da7f0`（版本校验 0x1b/0x17ab）；文件名拼接；保存对话框；自动保存；TReplayUnit 与压缩/未压缩命令族 |
| S10 | Battle.net | `warcraf_stage_battlenet.md` | CONNSTATE 13 态表（0x6fa5c7f0）；OsNet 传输对象；登录屏 `FUN_6f2b9d60`（9 按钮）；主菜单 `FUN_6f2b7560`（15 按钮）；BN 事件分发器全表 0x41–0x80（含 4 段无处理分支 + 特殊 0x6b）；大厅事件回调表；好友/战队/天梯/锦标赛/CAdManager |
| S11 | 随机数与确定性 | `warcraf_stage_rng.md` | 查表 RNG `FUN_6f199400`（8 字节状态 + 64 dword 固定表 `DAT_6f964e48`，四路计数器周期 61/59/53/47，元组周期 8,965,109，Python 复现验证）；播种 `FUN_6f1ae7e0`（取模 61/59/53/47）；SetRandomSeed 45 轮预热 `FUN_6f675fe0`；JASS native 映射（GetRandomInt `FUN_6f1e43e0`、GetRandomReal `FUN_6f1e4420`、SetRandomSeed `FUN_6f1f64b0` 等）；开局三状态播种 `FUN_6f30d9c0`；rand checksum `FUN_6f3130c0`（同步对象 +0x1c64，0x7a2d 折叠 + 0x17ab 常数）；软件浮点 `__fadd/__fsub/__fmul`；CRT rand/srand 死导入；TLS 槽位 |

## 4. 交叉验证：跨阶段共享地址核对

方法：提取 S0–S11 全部文档中出现的函数地址（共 **750 个不同的 `FUN_6fxxxxxx`**），对出现在 ≥2 个阶段文档里的地址逐一比对各阶段给出的角色描述。结果：**无冲突**——同一地址在不同阶段的角色描述一致，或为同一函数的不同调用点观察（身份本身在各阶段均标注未验证的除外，见 §4.2）。

### 4.1 核心骨架地址（多阶段一致，全部已验证）

| 地址 | 角色（各阶段一致） | 出现阶段 |
|---|---|---|
| `FUN_6f30ade0` | 网络事件泵（954 字节），调用者 `FUN_6f30ff00` / `FUN_6f310f20` | S0、S1、S2、S10 |
| `FUN_6f3098d0` | 网络事件大分发器（5074 字节，73 个被调函数，事件 0x01–0x31） | S0、S1、S2、S9、S10 |
| `FUN_6f307b50` | Battle.net 事件分发器（2625 字节，56 个被调函数，类型 0x41–0x80） | S1、S10 |
| `FUN_6f307980` / `FUN_6f307880` | 事件前处理 / 后处理 | S0、S1、S10 |
| `FUN_6f310c80` | TurnsSync 出入（435 字节，逐玩家字节会计、带宽滑动平均、倒计时） | S0、S1、S11 |
| `FUN_6f30b1a0` | 事件 0x20 回合包解析（654 字节） | S0、S1 |
| `FUN_6f30ff00` | 帧步进（1148 字节，25 ms 固定步长） | S1、S2 |
| `FUN_6f310f20` | `FUN_6f30ade0(1)` 薄壳：无事件则 `FUN_6f30bac0` 再泵一次 | S1、S2 |
| `FUN_6f30bac0` | 回合应用（命令 0x10–0x31 分发） | S2、S3 |
| `FUN_6f312740` | 游戏状态机 SetGameState（133 字节，状态字段 +0x270，0–6 迁移表） | S2、S10 |
| `FUN_6f3079f0` | 打印 syncNetEventChecksum | S0、S1 |
| `FUN_6f231cd0` | 打印战争迷雾校验和 | S0、S1 |
| `FUN_6f283540` | 路由器建立，引用 "Router is running in lockstep mode" | S0、S1 |
| `FUN_6f3130c0` | rand checksum 计算（179 字节，唯一调用者 `FUN_6f310c80`） | S1、S11 |
| `FUN_6f30d9c0` | 开局三状态播种 | S2、S11 |
| `FUN_6f3128d0` / `FUN_6f312930` | CNetData::SetRecordState（+0xbe4）/ SetReplayState（+0x614，0→1→2→3→4） | S1、S9 |
| `FUN_6f2fd0d0` | 按回合记网络命令日志 + 分发（NET_COMMAND_SAVE 路径） | S1、S9 |
| `FUN_6f30ada0` | 事件投递双虚调用（61 字节） | S1、S2、S10 |
| `FUN_6f1e9a50` | JASS native 注册表 A（23,346 字节，1167 次调用 `FUN_6f7e3710`） | S3、S4、S6、S8、S9 |
| `FUN_6f6f5d00` | JASS native 注册表 B（25,844 字节，推断为 Blizzard.j 表） | S3、S4、S11 |
| `FUN_6f7e3710` | Native 注册 helper（名字 + 类型签名） | S3、S4 |
| `FUN_6f0280e0` | UI/引擎初始化总入口（948 字节，46 个被调函数，0 个调用者） | S2、S6、S7、S8 |
| `FUN_6f027fb0` | 启动序列（292 字节，含图形初始化调用） | S2、S5 |
| `FUN_6f090b00` | 按名字取 FrameDef 的 [0x1c] 文本字段并拷贝 | S6、S7 |
| `FUN_6f144a00` | 文件加载 + 信息缓存（304 字节，走 Storm 267/269/253） | S7、S8 |
| `FUN_6f30d990` | 按画质挂载低/中档 MPQ（六档名一次传入，本体未反编译） | S7（两次出现一致） |

### 4.2 同一地址、多处观察但身份均未验证（非冲突，记为待定）

- `FUN_6f023e00`：S2 列为初始化链成员（身份未验证）；S7 称挂载收尾（推断）；S10 在登录屏里用它读一块 4KB 数据（用途未验证）。三处都是调用点观察，未给出互相矛盾的身份断言，函数本体仍待反编译。
- `FUN_6f02f230`：S3 复活流程里清状态槽；S2 未展开。无冲突。
- `FUN_6f05fb10`：S2 的虚调用派发点候选（推断）；S3 记为模拟步进入口候选（推断）。两者是同一推断的两面，未验证。

### 4.3 数字一致性核对

- 引擎常数 `0x7a2d`：S0 事件泵滚动校验、S11 `FUN_6f3130c0` 折叠、`FUN_6f3131a0` 折叠三处同源，已验证为引擎通用字节混合常数。
- 常数 `0x17ab`：S9 录像版本校验、S11 rand checksum 折叠常数两处出现，字段含义各自未验证——同值不同用途，不构成冲突。
- 25 ms 固定步长：S2（帧步进）与 S0/S11（回合节拍）一致；速度表 `DAT_6f987f14` 的 7 个表值未读（S2 未验证点 5）。
- CNetData 对象字段：`+0x610` 玩家槽、`+0x614` 回放态、`+0xbe4` 录制态、`+0x1c64` rand checksum、`+0x2288` 模式标记、`+0x227c` 事件校验累加器——各阶段引用的偏移互不重叠，无冲突；完整布局未验证（S1 未验证点 7）。

## 5. 子系统关系矩阵（谁调用谁、谁依赖谁）

| 关系 | 方向 | 证据 |
|---|---|---|
| GameMain → 启动序列 → 各子系统初始化 | S2 → S5/S6/S7/S8/S4 | `FUN_6f027fb0` 内依次初始化；`FUN_6f0280e0` 内调 JASS 注册 `FUN_6f1e9a50` 与 FrameDef 载入 |
| 引擎循环 → 帧步进 → 事件泵/回合应用 | S2 → S0/S1 | `FUN_6f05f710` →（最后一跳未验证，见 §7）→ `FUN_6f30ff00` → `FUN_6f310f20` → `FUN_6f30ade0` + `FUN_6f30bac0` |
| 回合应用 → 模拟核心 | S2/S0 → S3 | `FUN_6f30bac0` 分发 `FUN_6f2fxxxx` 解包薄壳 → `FUN_6f30ba60` → `FUN_6f30b940` 入队；消费函数未找到（S3 未验证点 1） |
| 模拟 → JASS2 | S3 → S4 | 模拟函数通过两张 native 注册表暴露给脚本；触发器事件由模拟产生 |
| JASS2 → 随机数 | S4 → S11 | GetRandomInt/Real/SetRandomSeed 经 native 注册表绑定到 `FUN_6f199400` 族；状态指针由解释器 ECX 直通 |
| 模拟 → 随机数 | S3 → S11 | 伤害三分类是否涉 RNG 未验证；命令队列消费顺序是确定性关键 |
| 网络 → 录像 | S1 → S9 | GameStart（事件 0x1b）开始写 TempReplay.w3g；每条命令可写 NET_COMMAND_SAVE（`FUN_6f2fd0d0`） |
| 网络 → Battle.net | S1 → S10 | 同一事件泵；类型 ≥0x40 改走扩展分发器 `FUN_6f307b50`；同一套 CNetEvent*/CBattleNetEvent* 对象模型 |
| 文件层 → 全部 | S7 → S2/S4/S5/S6/S8/S9/S10 | MPQ 挂载链是启动前提；地图脚本（war3map.j）加载路径未验证；FrameDef/.fdf/.blp、声音文件、升级/广告文件均经 Storm 序号读出 |
| 渲染 → UI | S5 → S6 | CBackdropFrame 图元经 `FUN_6f12a6a0` 提交进 CGxDevice 批量；字体纹理图集是 CTextFrame 的底座 |
| 渲染 → 音频 | S5 → S8 | 听者位置/朝向与相机同源（`FUN_6f8785a0` 同时驱动两者，本体未验证） |
| UI → 模拟（经网络） | S6 → S0/S1/S3 | 点击命令经回合命令包走网络同步，该链路未验证（S6 未验证点）；UI 只显示模拟状态、不直接改模拟 |
| 音频 → 文件层 | S8 → S7 | Miles 文件回调走 Storm 267/269/271/253；缓存加载 `FUN_6f144a00` 同源 |
| 主循环 → 音频/UI/渲染更新 | S2 → S8/S6/S5 | 音频周期扫表调用方未验证；UI 每帧更新调用链未验证；渲染每帧调用链未验证（均经虚表，见 §7） |

## 6. 关键数据结构与消息格式（汇总）

- **回合数据模型**：`Net::ClientTurn`（单玩家单回合命令集）→ `Net::ClientTurnsSync`（玩家回合同步状态）→ `Net::TurnsSyncChunk`（线上传输块）→ `CTurnsSyncStore`（按回合号组装，等齐放行）。
- **事件对象**：事件泵按类型字节 `+0x14`、负载 `+0x8`、长度 `+0xc` 遍历链表；游戏内事件 0x01–0x31（Connect/Disconnect/GameList/GameCreate/GameJoin/PlayerJoin/Leave/Ready/GameSetup/GameStart/GameReady/Trigger/回合 0x1e–0x23/RouterHandoff 0x26–0x2a/DistFile 0x2b–0x2d/TrustedDesync 0x30/TrustedResult 0x31）；Battle.net 事件 0x41–0x80（0x43、0x5c–0x61、0x63–0x66、0x69–0x6a 无处理分支；特殊 0x6b 带数据负载直通）。
- **命令对象**：24 字节 `CNetCommand*`，命令号 0x10–0x31（`FUN_6f30bac0` 分发表）与 0x40–0xfe 分支；入队为双向链表（`FUN_6f30ce40`/`FUN_6f30ceb0`），每命令号一个计数槽。
- **RNG 状态**：8 字节（4 个计数器 + 累加器），64 dword 固定表 `DAT_6f964e48`（256 字节已全文实测，见 S11 附录 A）；播种取模 61/59/53/47；元组周期 8,965,109。
- **校验和**：七子系统格式串 `%scheat/%scombat/%sgame/%snet/%srand/%stempest/%sunit checksum %08x`（0x6f987de8–0x6f987e7a 连续存放，无直接代码引用，打印函数未定位）；事件流滚动校验 `acc = rotl(acc ^ word, 3)` + `0x7a2d` 折叠；rand checksum 存同步对象 +0x1c64。
- **MPQ**：头/哈希表/块表；块表条目 0x2c 字节；哈希表条目 16 字节；优先级补丁链 War3 → War3X → War3XLocal → War3Patch；签名验证经 CryptoAPI + 资源 `BLIZZARDKEY`。
- **UI**：CLayer → CFrame → CSpriteFrame → CControl；FrameDef 名字→对象哈希表；布局失效链表（哨兵 `PTR_LOOP_6fb6641c`）；九宫格切片标志 `+0x6c`。
- **音频**：三类 16 条句柄池；24 声抢占上限；声音对象状态 `+0x1ac`（2=延迟/排队，3=正在播放，5=停止）。
- **录像**：版本常量 0x1b/0x17ab；TempReplay.w3g；TReplayUnit；压缩/未压缩命令族。

## 7. 仍未验证的残留点（按影响面归并，共 8 类）

以下为 S0–S11 各阶段“未验证点”的去重归并；每条都已在原阶段文档中标注，未编造。

1. **三条“最后一跳”调用链（虚表分发，最大残留）**：引擎循环 `FUN_6f05f710` → 帧步进 `FUN_6f30ff00`（0x6f30c8d0 附近未定义小包装）；主循环 → UI 每帧更新 `FUN_6f0a26b0`；主循环 → CGxDevice 每帧场景提交；音频周期扫表 `FUN_6f145e30`（0 个调用者）/`FUN_6f1462c0` 的调用方。共同原因：分发经虚表/回调指针，静态调用图断开。
2. **模拟执行端**：命令队列消费（drain）函数未找到；各命令号 0x10–0x31 具体语义；`FUN_6f661060`（2957 字节，~130 调用者）本体未反编译；伤害阈值 `DAT_6fbb81d4` 数值、伤害/治疗虚函数 +0x120/+0x128 目标、三分类 0/1/2 含义；技能施放主流程（冷却/法力/buff 时序）；单位移动/寻路、攻击间隔/弹道。
3. **JASS2**：两张 native 注册表分工与逐条映射；`war3map.j` 加载路径；TriggerSleepAction 与回合推进的交互；字节码操作码表语义；解释器唯一调用者身份。
4. **网络/同步数值**：回合节奏（每秒几回合）与 SetTurnsLatency 换算；丢包重传与“等齐”超时逻辑；事件 0x22/0x23 完整语义；七子系统校验和各自的哈希函数与打印函数本体；Storm 112 序号中除 548/501/403 外的身份；`FUN_6f2f20e0`（7 字节、250 个调用者）身份；CNetData 完整对象布局。
5. **渲染**：能力标志位语义；D3D 格式码对应；BLP 解码与 MDX 加载完整流程；字体光栅化与图集打包；GxVideo；光照；渲染帧率与 25 ms 步长的解耦机制；回读路径实际调用方。
6. **UI**：CFrame 虚表完整方法表；按钮点击从 Win32 消息泵到 CButtonFrame 的分发；`.fdf` 处理函数名与 JASS 触发器的绑定；`DAT_6f95d420` 常量表与 FrameDef 数字 ID（0x400b0064 / 0x40100064 / 0x40060064）含义；JassFrame 用途。
7. **文件/录像/Battle.net**：写 MPQ 路径完全未定位；序号 263/272/294/295/300 等本体；序号 264/275 身份为推断；录像文件格式细节与写入器身份（`FUN_6f28b570` / `FUN_6f2c4c30`）；CONNSTATE 驱动函数与 BNCS 握手包格式；网关域名（Game.dll 内无）；大厅事件回调本体；匹配算法。
8. **随机数**：三状态用途映射；七子系统校验打印函数本体；CRandData RTTI 无虚表；`FUN_6f3071d0` 聚合的 -2 项对象身份；GetRandomReal 缩放链操作数对应；累加器完整输出周期（8,965,109 的倍数）；`FUN_6f30d9c0` 触发点。

## 8. 本轮 S12 的工作记录

- 汇总依据：S0–S11 全部 13 份文档（`~/workspace/warcraf_stage_*.md` + `warcraf_framesync_analysis.md`，共 2,666 行），未新跑 Ghidra（计划要求：只做汇总与交叉验证）。
- 交叉核对：750 个不同 `FUN_6fxxxxxx` 地址；跨阶段共享地址逐一比对角色描述，**无冲突**；仅 `FUN_6f023e00` 等 3 处为多调用点观察、身份均未验证，记为待定而非冲突。
- 总图：更新 `~/workspace/warcraf_framesync_architecture.html`（标题改为完整架构，新增总图、子系统关系表、主函数表与残留点清单；原 S0 帧同步章节原样保留）。
- 计划状态：S0–S12 全部“已验证”，本计划完成；定时任务 `warcraf-architecture-deep-dive` 已停用；追踪项 goal_0ea7e03ce1af 已置 completed。
- 证据文件清单见 §9。

## 9. 全部文档与证据清单

| 文件 | 内容 |
|---|---|
| `~/workspace/warcraf_architecture_complete.md` | 本文件：S12 汇总与交叉验证 |
| `~/workspace/warcraf_framesync_architecture.html` | 完整架构总图（已更新，含 S0–S12） |
| `~/workspace/warcraf_framesync_analysis.md` | S0 帧同步核心 |
| `~/workspace/warcraf_stage_network.md` | S1 网络层 |
| `~/workspace/warcraf_stage_mainloop.md` | S2 主循环与入口 |
| `~/workspace/warcraf_stage_simulation.md` | S3 模拟核心 |
| `~/workspace/warcraf_stage_jass.md` | S4 JASS2 虚拟机 |
| `~/workspace/warcraf_stage_render.md` | S5 Gx 渲染 |
| `~/workspace/warcraf_stage_ui.md` | S6 UI / Frame 系统 |
| `~/workspace/warcraf_stage_mpq.md` | S7 Storm 文件层 MPQ |
| `~/workspace/warcraf_stage_audio.md` | S8 音频 Miles |
| `~/workspace/warcraf_stage_replay.md` | S9 录像与回放 |
| `~/workspace/warcraf_stage_battlenet.md` | S10 Battle.net |
| `~/workspace/warcraf_stage_rng.md` | S11 随机数与确定性 |
| `~/workspace/warcraf_architecture_plan.md` | 分阶段计划（S0–S12 全部已验证，已完成） |
| `~/workspace/warcraf_ghidra/` | Ghidra 工程 war3/storm 与各阶段反编译原文（sync/net/mainloop/bnet/rng_dump*.txt 等） |
