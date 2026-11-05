# Warcraf 引擎完整架构分析 — 分阶段深挖计划

- 目标（用户 2026-11-05）：把魔兽争霸 III 1.27a 整个引擎架构完整分析出来，不只是网络部分；每 30 分钟深挖下一个未验证的阶段，直到完整。
- 追踪项：goal_0ea7e03ce1af（Warcraf 引擎完整架构分析），工作区 `~/workspace/goals/warcraf/`
- 定时任务：`warcraf-architecture-deep-dive`，每 30 分钟一轮，每轮只做一个阶段。
- 证据底座：`~/workspace/Warcraf/dll/`（Game.dll 13.2 MB、Storm.dll 334 KB、Mss32.dll、ijl15.dll、msvcp120/msvcr120.dll），Warcraft III 1.27.0.52240，32 位 PE，MSVC 2013。
- 合规边界：仅用于学习引擎如何工作，不复制引擎代码与资产，不绕过 DRM/反作弊，不做游戏作弊。文档中只写机制总结、函数地址与少量必要的反编译片段，不大段照抄反编译代码。

## 工具链（每轮必读）

- Ghidra 12.2：`~/workspace/tools/ghidra_12.2_PUBLIC/`，JDK 25：`~/workspace/tools/jdk-25.0.5+8/`（Ghidra 12.2 要求 JDK 25+，JDK 21 会被拒）。
- 堆上限 `-Xmx2G` 已写在 `support/launch.properties` 的独立 `VMARGS=-Xmx2G` 行，**不要改**，也不要把它拼进 `--sun-misc-unsafe-memory-access` 的值里（会起不来）。
- 已分析好的工程（不要重跑完整分析）：Game.dll → `~/workspace/warcraf_ghidra/war3`（97,839 个函数）；Storm.dll → `~/workspace/warcraf_ghidra/storm`。
- headless 没有 PyGhidra：脚本必须写 `.java`（Ghidra 现场编译），调用形如：
  `JAVA_HOME=~/workspace/tools/jdk-25.0.5+8 PATH=~/workspace/tools/jdk-25.0.5+8/bin:$PATH ./support/analyzeHeadless ~/workspace/warcraf_ghidra war3 -process Game.dll -noanalysis -scriptPath <脚本目录> -postScript "DumpX.java"`
  （工程目录是 `~/workspace/warcraf_ghidra`（内含 `war3.gpr`），工程名 `war3`——计划里旧写法 `~/workspace/warcraf_ghidra/war3 war3` 是错的，2026-11-05 S6 轮已纠正；`-postScript` 要带 `.java` 扩展名；脚本里用 `DecompInterface` 反编译指定函数。）
- DLL 已剥离符号，函数名全是 `FUN_6fxxxxxx`：定位靠字符串 xref + RTTI 类型名（`strings` 找 `?AV…@@` / `?AU…@@`），不要按名字猜函数。
- 本机内存约 8 GB：一次只跑一个 Ghidra 实例；长分析用后台 exec，不要反复轮询。

## 每轮工作流（严格执行）

1. 读本计划 + 该阶段已有的文档，选出第一个状态为“进行中”或“未验证”的阶段（按 S1→S12 顺序；S0 已完成）。
2. 只深挖这一个阶段，不启动下一个。
3. 用 Ghidra 脚本定位该阶段的关键函数/类/字符串，反编译核心函数，写出机制说明。所有结论必须有函数地址或字符串证据；推断要标明是推断；没验证的写“未验证”，不编数字、不编函数名。
4. 写出阶段文档 `~/workspace/warcraf_stage_<slug>.md`（中文，含：机制总述、关键函数地址表、数据结构/消息格式、与其它阶段的关系、未验证点）。
5. 更新本计划：该阶段状态改为“已验证”（+ 日期 + 文档路径）；若被阻塞（Ghidra 失败、证据不足），保持“未验证”并写清阻塞原因；同一阶段连续被阻塞 2 轮后，标记为“阻塞待用户”并在下一轮跳到再下一个阶段。
6. 在追踪项 goal_0ea7e03ce1af 上记一条 `tracking.create_entry`（标题=阶段名，一句话结果）。
7. 每轮保持静默，不向用户发消息。例外：S12 完成且全部阶段为“已验证”时——标记本计划完成，把追踪项置为 completed（说明原因），用 `cron.update` 把本任务 `enabled` 设为 false 停掉它，然后在主聊天里向用户汇报一次完整成果（总图 + 各阶段文档清单）。
8. 若某轮发现本计划已全部完成，什么都不做，安静结束。

## 阶段表

| 阶段 | 主题 | 状态 | 文档 | 备注 |
|---|---|---|---|---|
| S0 | 帧同步核心（确定性锁步） | 已验证 2026-11-05 | `~/workspace/warcraf_framesync_analysis.md`、图 `~/workspace/warcraf_framesync_architecture.html` | TurnsSync 三件套、事件泵 FUN_6f30ade0、大分发器 FUN_6f3098d0、七子系统校验和、滚动校验（xor/rotl3、0x7a2d） |
| S1 | 网络层完整 | 已验证 2026-11-05 | `~/workspace/warcraf_stage_network.md` | 导入表（Storm 112 序号 + 1 具名）、Net 函数簇 1806 个、双分发器（0x01–0x31 游戏内 + ≥0x40 Battle.net）、TurnsSync 出入与回合包解析、Storm 序号→VA 对照、已验证 548/501/403；未验证：其余序号身份、七子系统哈希、回合节奏数值、丢包重传 |
| S2 | 主循环与入口 GameMain | 已验证 2026-11-05 | `~/workspace/warcraf_stage_mainloop.md` | GameMain 0x6f0297d0（唯一导出，155 字节薄壳）；引擎循环 FUN_6f05f710（"Engine %x"，while(true)）；帧步进 FUN_6f30ff00（25 ms 固定步长，累加器钳 4000，看门狗 200 ms）；EvtSched（EvtSched#%d 线程，_beginthreadex）；消息泵 FUN_6f1532b0；状态机 FUN_6f312740（0–6 迁移表）；回合应用 FUN_6f30bac0（命令 0x10–0x31）；未验证：引擎循环→帧步进最后一跳（0x6f30c8d0 附近未定义小包装）、速度表 DAT_6f987f14 值、各命令执行函数语义 |
| S3 | 模拟核心：单位 / 战斗 / 技能 | 已验证 2026-11-05 | `~/workspace/warcraf_stage_simulation.md` | 回合命令解包薄壳 FUN_6f2fxxxx→FUN_6f30ba60→FUN_6f30b940、24 字节命令对象与链表队列、CDataStore 读写原语、WidgetDamageData 12 字节/条、AoE 伤害 FUN_6f6b19e0、单体伤害/治疗 FUN_6f6af530（虚函数 0x120/0x128）、区域枚举 FUN_6f661060（~130 调用者）、CAbility 492 类/CBuff 226 类/CUnit 51 类、英雄复活 FUN_6f678380、JASS native 签名表 FUN_6f1e9a50；未验证：命令队列消费函数、各命令号语义、FUN_6f661060 本体、伤害阈值与三分类含义、技能施放主流程 |
| S4 | JASS2 虚拟机与脚本 | 已验证 2026-11-05 | `~/workspace/warcraf_stage_jass.md` | 解释器主循环 FUN_6f7f9070（Jass2/Nodes.h）、两张 native 注册表（1167 次注册，类型签名）、触发器系统、句柄与哈希表；未验证：两表分工、war3map.j 加载、TriggerSleepAction 与回合交互 |
| S5 | Gx 渲染抽象（D3D/OpenGL） | 已验证 2026-11-05 | `~/workspace/warcraf_stage_render.md` | CGxDevice 双后端工厂 FUN_6f0e2f70（0=OpenGL，1=D3D）、D3D9 动态加载 FUN_6f0ec530（LoadLibrary d3d9.dll + Direct3DCreate9 0x20）、OpenGL 静态导入、GL 能力探测 FUN_6f0ebb40（版本解析 + ARB multitexture 指针表）、纹理上传 FUN_6f0eb600、场景回读两后端同构；未验证：每帧渲染虚表调用链、能力标志位语义、BLP/MDX 加载细节、字体/视频/光照细节 |
| S6 | UI / Frame 系统 | 已验证 2026-11-05 | `~/workspace/warcraf_stage_ui.md` | 切入：CBackdropFrame 等 Frame RTTI、UI 布局、事件分发 → 已验证：CLayer→CFrame→CSpriteFrame→CControl 继承链、九宫格渲染、FrameDef 哈希表与注册表、锚点/布局失效链表、CTextFrame/CMessageFrame/CEditBox/CChatDisplay、CScreenFrame 懒创建子 Frame；未验证：主循环→UI 更新链、虚表完整方法表、按钮点击分发与 .fdf 处理函数绑定、DAT_6f95d420 常量含义 |
| S7 | Storm 文件层 MPQ（SFile） | 已验证 2026-11-05 | `~/workspace/warcraf_stage_mpq.md` | Storm 814 导出（54 具名，SFile 19 个）；Game 零具名导入、26 个无名序号；SFile 五态分发、MPQ 头/哈希/块表解密、优先级补丁链（War3/War3X/War3XLocal/War3Patch + teen→t/tx.mpq）、CryptoAPI 签名验证（BLIZZARDKEY）、CORRUPT_* 30 码；未验证：263/272/294/295/300 本体、264/275 身份（推断）、写 MPQ 路径、PrefetchListFile 流程、哈希函数本体 |
| S8 | 音频 Miles（Mss32） | 已验证 2026-11-06 | `~/workspace/warcraf_stage_audio.md` | CSoundWar3/CSoundManager（CSoundManager.cpp）、Miles 6.1d、Game 导入 110 个 AIL_*（2 死导入）、驱动/XMIDI/DLS 打开链（S3BASE→VORT_DLS→软件）、三类 16 条句柄池、播放分发器 FUN_6f171ba0（流/序列/2D/3D）、抢占 FUN_6f171380（24 声上限、同名/同文件/声道/系统四级）、3D 听者变化检测 + 单声参数下发 FUN_6f16cbe0（EAX2）、统一音量 FUN_6f173cb0（距离衰减 sqrt/pow）、文件回调走 Storm 267/269/271/253、22050Hz 警告；未验证：0x3cd013a9 提供者常量、扫表函数调用方、听者同步 FUN_6f8785a0 本体、MiscData.txt/musicvolume 读取函数、JASS native 逐一映射、音量组语义 |
| S9 | 录像与回放 | 已验证 2026-11-06 | `~/workspace/warcraf_stage_replay.md` | 录像加载器 FUN_6f2da7f0（版本校验 0x1b/0x17ab）、文件名拼接、保存对话框、自动保存、TReplayUnit 与压缩/未压缩命令族；未验证：文件格式细节、写入器身份 |
| S10 | Battle.net / 大厅与匹配 | 已验证 2026-11-06 | `~/workspace/warcraf_stage_battlenet.md` | 切入：BattleNet 相关 RTTI/事件（GameList/GameCreate/GameJoin 等已在 S1 的事件表里出现，这里补大厅、频道、好友、战队、天梯逻辑）→ 已验证：CONNSTATE 13 态表（0x6fa5c7f0）、OsNet 传输对象、登录屏 FUN_6f2b9d60（9 按钮）、主菜单 FUN_6f2b7560（15 按钮）、BN 事件分发器全表 0x41–0x80（含 4 段无处理分支 + 特殊 0x6b）、大厅事件回调表、好友/战队/天梯/锦标赛/CAdManager；未验证：CONNSTATE 驱动函数、网关域名与 BNCS 握手、Storm SNet 承载序号、大厅回调本体、匹配算法 |
| S11 | 随机数与确定性保障 | 已验证 2026-11-06 | `~/workspace/warcraf_stage_rng.md` | 切入：rand checksum 背后的 RNG（种子、分发、同步点）、定点数使用、其它确定性手段 → 已验证：查表 RNG `FUN_6f199400`（8 字节状态 + 64 dword 固定表 `DAT_6f964e48`，四路计数器周期 61/59/53/47，元组周期 8,965,109，Python 复现验证）、播种 `FUN_6f1ae7e0`（取模 61/59/53/47）、`SetRandomSeed` 45 轮预热 `FUN_6f675fe0`、JASS native 映射（GetRandomInt `FUN_6f1e43e0`、GetRandomReal `FUN_6f1e4420`、SetRandomSeed `FUN_6f1f64b0` 等）、开局三状态播种 `FUN_6f30d9c0`、rand checksum `FUN_6f3130c0`（同步对象 +0x1c64，0x7a2d 折叠 + 0x17ab 常数）、软件浮点 `__fadd/__fsub/__fmul`、CRT rand/srand 死导入、TLS 槽位；未验证：三状态用途映射、七子系统校验打印函数本体、CRandData RTTI 无虚表、聚合种子对象身份、GetRandomReal 缩放链细节 |
| S12 | 完整架构总图与交叉验证 | 已验证 2026-11-06 | 汇总 `~/workspace/warcraf_architecture_complete.md`；总图 `~/workspace/warcraf_framesync_architecture.html`（已更新） | 750 个不同 FUN 地址交叉核对无冲突；残留未验证点归并为 8 类（见汇总文档 §7）；S0–S12 全部已验证，本计划完成 |

## 已验证的锚点（各轮可直接引用，不用重挖）

- 事件泵 `FUN_6f30ade0`（954 字节）、大分发器 `FUN_6f3098d0`（5074 字节，事件类型 0x01–0x31）、校验和打印 `FUN_6f3079f0`、迷雾校验和 `FUN_6f231cd0`、锁步模式日志 `FUN_6f283540`。
- RTTI：`Net::ClientTurn` / `Net::ClientTurnsSync` / `Net::TurnsSyncChunk` / `CTurnsSyncStore` / `CNetCommandBase` 族 / `CNetEventTrustedDesync` / `EVENT_ID_TURNSSYNC` / `EVENT_ID_TURNSSYNCMISMATCH`。
- 反编译原文：`~/workspace/warcraf_ghidra/sync_decomp.txt`；函数与调用边：`sync_dump2.txt`；网络层：`net_dump.txt`、`net_decomp.txt`。

## 完成记录

- 2026-11-06：S0–S12 全部已验证，本计划完成。汇总文档 `~/workspace/warcraf_architecture_complete.md`；总图 `~/workspace/warcraf_framesync_architecture.html` 已更新。定时任务 `warcraf-architecture-deep-dive` 已停用；追踪项 goal_0ea7e03ce1af 已置 completed。
