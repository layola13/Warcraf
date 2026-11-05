# S8 — 音频 Miles（Mss32）

- 状态：已验证 2026-11-06
- 证据：`~/workspace/warcraf_ghidra/audio_dump.txt`（32 个音频字符串定位 + 11 个函数反编译）、`audio_dump2.txt`（110 个 AIL_* 导入符号 × Game 调用者全表 + 39 个函数反编译）、`audio_dump3.txt`（30 个函数反编译）
- 上承：S2 主循环（引擎初始化经 `FUN_6f0280e0` 进入本阶段）、S4 JASS2（`FUN_6f1e9a50` 注册表里的声音 native 在此实现）、S5 Gx 渲染（听者位置/朝向与相机同源，见 §8）、S6 UI（`FUN_6f0280e0` 是 UI 初始化入口，同时调本阶段初始化）、S7 Storm 文件层（Miles 的文件回调直接调 Storm 序号，见 §5）

## 1. 总述

音频子系统是 **Game 侧声音管理器 + RAD Miles Sound System 6.1d** 两层结构：

- **Game 侧**：`CSoundWar3` / `CSoundManager`（残留源码路径 `War3\Source\UI\CSoundManager.cpp`、`CSoundManagerI.cpp`、`W32\OsSnd.cpp`），负责声音对象的创建/抢占/延迟播放、3D 听者同步、音量组与距离衰减、音乐与 MIDI 序列管理。日志写 `SoundManagerLog.txt`。
- **Miles 侧**：`Mss32.dll`（Miles Sound System v6.1d，1991–2002 RAD Game Tools，ImageBase `0x21100000`，只依赖 KERNEL32/USER32/WINMM），导出 319 个 `AIL_*` 函数；**Game.dll 静态导入其中 110 个**，另有 2 个导入后无任何 Game 调用者（死导入，见 §9）。
- **三条播放管线**：2D 采样（WAV/MP3/ASI 解压）、3D 采样（带位置/朝向/锥角/遮挡）、流（stream，音乐长文件）与 MIDI 序列（XMIDI 驱动 + DLS 音色库）——四类句柄分属三个 16 条记录的句柄池（见 §4）。
- **文件来源**：Miles 不直接读磁盘；`AIL_set_file_callbacks` 注册的四个回调全部转调 Storm 序号（S7 已验证：267 打开、269 读、271 seek、253 释放），即所有音频数据都走 MPQ 归档读取。
- **内存**：Miles 的 malloc/free 被替换为 Game 自有的 10 档尺寸分级分配器（`W32\OsSnd.cpp`），带 `AUMilesMemoryBlock` 标记。

## 2. 初始化与关闭

调用链（均已反编译验证）：

```
FUN_6f0280e0（UI 初始化，S6）/ FUN_6f298040
  └─ FUN_6f339df0  CSoundManager 初始化（1087 字节）
       ├─ FUN_6f144b50（142 字节，驱动打开包装）
       │    └─ FUN_6f1423c0  数字驱动打开（284 字节）
       │         ├─ GetFileAttributesA 校验 redist 目录
       │         ├─ _AIL_mem_use_malloc(FUN_6f143290) / _AIL_mem_use_free(FUN_6f143310)
       │         ├─ _AIL_set_redist_directory（日志 "Setting MP3 redist directory to %s"）
       │         ├─ _AIL_startup；旧驱动先 _AIL_close_digital_driver
       │         ├─ _AIL_open_digital_driver(速率表 UNK_6f961ac8[DAT_6fb66e80], 0x10, 2, (flags&0x10)!=0)
       │         │    失败 → "Error, cannot open Miles digital sound driver!"，DAT_6fbda510（数字驱动句柄）= 0
       │         ├─ _AIL_set_file_callbacks(FUN_6f1438e0 打开, LAB_6f1414d0 关闭, FUN_6f143fe0 seek, FUN_6f143b20 读取)
       │         ├─ FUN_6f142570  MIDI/DLS 打开（见下）
       │         └─ FUN_6f16ec30  预分配 16 个 2D 采样句柄（见 §4）
       ├─ FUN_6f145350  保存配置（DAT_6fb66f24 / DAT_6fbda524，驱动未打开时）
       ├─ FUN_6f32b0c0  3D 提供者选择（见下）
       ├─ FUN_6f145820  _AIL_set_3D_room_type（仅当 3D 提供者已开且 DAT_6fbda950 bit0 清零）
       ├─ FUN_6f32fb20  声音标签/主声音表等子表初始化（11 个子调用）
       ├─ 11 次 FUN_6f179ce0  回调注册（Ordinal_580 + FUN_6f179d50；含 FUN_6f320920 等，见 §3）
       ├─ FUN_6f339d90  8 个音量组初始化为 1.0f（DAT_6fbe6250 起，每组 0x14 字节）
       └─ DAT_6fbe61c8 = 0.1f（FUN_6f057d50 换算后），DAT_6fbe61c4 = 1
```

`FUN_6f142570`（418 字节，XMIDI + DLS 打开）：先关旧 XMIDI 驱动，再 `_AIL_open_XMIDI_driver((flags&8)!=0)`，失败 → `"Error, cannot open MIDI driver: \"%s\""`（`_AIL_last_error`）；成功后 `_AIL_DLS_open` 按固定顺序尝试硬件 DLS 驱动：`S3BASE.DLL`（日志 "Using SonicVibe DLS driver"）→ `VORT_DLS.DLL`（"Using Vortex DLS driver"）→ 失败则用 XMIDI 驱动 + 数字驱动做软件 DLS，仍失败 → `"Error, cannot open software MIDI driver: \"%s\""`。DLS 句柄存 `DAT_6fbda51c`，XMIDI 句柄存 `DAT_6fbda518`。

`FUN_6f32b0c0`（12 字节壳，实际循环在 `FUN_6f32b0cc`，164 字节）：`FUN_6f144840` 调 `_AIL_enumerate_3D_providers` 枚举提供者填表（`DAT_6fb694b8` 起，每条目 0x14=20 字节），按配置优先级倒序逐个试 `FUN_6f145610(0x3cd013a9, (flags&2)!=0)`，成功则记 `DAT_6fb694ac` 为当前提供者序号并调 `FUN_6f145160`（距离因子钳制写入 `_DAT_6fb66f28`）。**未验证**：常量 `0x3cd013a9` 作为提供者句柄参数的确切含义（枚举表条目里明明有句柄字段，但调用时传的是常量，见 §9）。

`FUN_6f145610`（259 字节，3D 提供者 + 听者打开）：先关旧提供者/听者；`_AIL_open_3D_provider(param_1)` 返回非 0 → `"Cannot open primary provider! (\"%s\")"` 并关闭返回 false；成功后 `DAT_6fbda508 = param_1`（3D 提供者句柄），`"Successfully set provider"`；`_AIL_open_3D_listener` 得 `DAT_6fbda95c`（听者句柄），`_AIL_set_3D_distance_factor(DAT_6fbda508, param_2)`；最后 `FUN_6f16edc0` 预分配 16 个 3D 采样句柄。

关闭（`FUN_6f145a60`，687 字节，调用者 `FUN_6f33d380`）：依次销毁四条播放链表上的全部声音对象（虚表 +4 析构）、释放两个 DLS 对象链表、调 `FUN_6f144350`（174 字节：`FUN_6f16ea50` 释放全部三类句柄池 → 关听者/提供者/DLS/XMIDI/数字驱动，六个全局句柄清零）→ `_AIL_shutdown()` → 释放通道表 `DAT_6fbda538`、清 `DAT_6fbda958`（声音总开关）等全局状态。

## 3. 播放主流程

### 3.1 创建

- `FUN_6f339670`（864 字节，`HLOOPINGSOUND`，0x1d4=468 字节对象）：读标签/主声音表（`FUN_6f3262e0` / `AUMASTERSOUNDENTRY`），填文件名（`+0x34` 起 0x104 字节）、声道/音量/优先级等字段，最后调 `FUN_6f1445d0` 创建底层声音。
- `FUN_6f339550`（279 字节，`HLOOPINGMIDISOUND`，0x14c=332 字节对象）：同上，MIDI 专用。
- `FUN_6f1445d0`（297 字节，`SndCreate`）：声道号 `param_2` 超过通道表长度 `DAT_6fbda534` 返回 0；按通道表 `DAT_6fbda538`（每条目 0x14 字节）钳制标志位与音量上下限；`FUN_6f04d150(0,"HSOUND",…)` 取句柄表对象并 `FUN_6f13fb50` 分配；调 `FUN_6f16f0d0` 填对象字段（文件名拷到 `+0x34`，`+0x178`=声道，`+0x13c`=标志，`+0x140`=优先级，`+0x144`=音量，`+0x148`=用户名，`+0x1b0`=目标播放速率，`+0x14c`=类型）；若采样率 > 0x31fff 且未置 `+0x13c` bit 0x1000，置 `+0x174`=1（流标志，见 §6）。

### 3.2 播放入口与分发

- `FUN_6f145020`（287 字节，8 个调用者）：播放总入口。`DAT_6fbda958`（声音总开关）为 0 直接返回；若全局声音启用（`DAT_6fb66f30` 且 `DAT_6fb66f20` bit 2），调 `FUN_6f171ba0` 真播放；否则按对象标志决定：无标志 → `"Sound disabled, ignored play request for sound %s."`；有延迟标志 → 记 `+0x144`=音量并 `FUN_6f1703c0` 挂入延迟队列，日志 `"Sound disabled, delayed play request for sound %s."`。
- `FUN_6f144e30`（494 字节，调用者 `FUN_6f32da10` 与 `FUN_6f320920`）：带 3D 参数的播放入口（文件名/声道/标志/音量/位置等 14 个参数），同样先过总开关与通道钳制，再 `FUN_6f16f0d0` 填对象 → `FUN_6f171ba0`；失败时按 `param_13` 逐个回写位置数组（`FUN_6f173870`）；成功后对象 `+0x13c` 置 bit 0x2000000。声音全禁时日志 `"All sounds disabled, ignoring play request for sound %s"`。
- `FUN_6f171ba0`（231 字节，播放分发器）：先 `FUN_6f171380` 做重复/抢占检查（返回 3 则拒绝）；再按对象类型分发：`+0x174 != 0` → 流 `FUN_6f171c90`；`+0x16c == 3` → MIDI 序列 `FUN_6f171a70`；`+0x14c == 1` → 2D 采样 `FUN_6f171750`；`+0x14c == 2` → 3D 采样 `FUN_6f1718f0`；随后 `FUN_6f1706f0` 登记为“正在播放”（见 §3.4）。状态 `+0x1ac`：2=延迟/排队，3=正在播放，5=停止（由各分支返回值与调用方共同维护，**完整状态机未验证**）。
- `FUN_6f32da10`（1126 字节，6 个调用者）：高层播放入口（带主声音/距离截断判定）。先查 `"master sounds disabled, aborting"`；读声音标签对象（`FUN_6f3262e0`）；若设置了距离截断（`+0x40` bit 0 且 `DAT_6fbe5d78==5` 除外）且听者到声源距离平方大于 `+0x50` 的截断值，日志 `"sound cutoff at %g, current distance at %g, aborting"` 并放弃；否则调 `FUN_6f144e30`。**推断**：这是游戏内“按标签播音效”（含 3D 位置声）的统一入口；调用方 `FUN_6f33bc20/6f33b2e0/6f33bcb0/6f33bc60/6f33b890/6f33b190` 各自的触发语义未逐一验证。

### 3.3 抢占与上限

`FUN_6f171380`（962 字节，重复名/抢占仲裁，返回 0=可播、3=拒绝）：

- 同名检查：遍历全局播放链表 `DAT_6fb66f54`，按 `+0x34` 文件名比对（`Ordinal_509` 即 `strnicmp` 类比较）；若对象 `+0x13c` 置 bit 0x400（重复名检查）且无 bit 0x800（允许抢占同名）→ `"Sound %s unplayable, duplicate username %d exists!"`（用户名 `+0x148`）；有 bit 0x800 则 `FUN_6f171230` 停掉全部同名旧声，日志 `"Duplicate username %d found, requested sound %s has preempted other sounds"`。
- 同文件检查（`FUN_6f1748c0` 查 `AUSOUNDIDHASH` 哈希表）：按 `+0x13c` 的 bit 0x40/0x80 决定是否可抢占低优先级同文件声，否则 `"Sound %s unplayable, no dupe check flag or no lower-priority sound found"`；可抢占则日志 `"Sound %s playing after other duplicate sound file preempted"`。
- 声道容量：`DAT_6fbe3780` 的声道链表计数达到通道表上限时，按 bit 0x8（可抢占声道内最老声）或 bit 0x4（可抢占声道内任意低优先级声）处理，否则 `"Sound %s unplayable, channel full and can't preempt!"`；同文件数超过 4（`FUN_6f16dcd0() > 3`）时无抢占标志 → `"Sound %s unplayable, filename count reached %d and sound has no preempt flags"`。
- 系统总量：正在播放数 `DAT_6fbe3804` 达到 **0x18=24** 时，无系统抢占标志（`+0x13c` bit 0x10/0x20）→ `"Sound %s unplayable, already %d sounds playing in system"`；有则抢占当前最低优先级声（`FUN_6f16db40` 找最低优先级）并日志 `"%d sounds in system, requested sound %s playing after preempting existing sound"`；抢不到 → `"Sound %s unplayable, already %d sounds playing in system and can't preempt other sounds"`。

### 3.4 播放登记与停止

- `FUN_6f1706f0`（227 字节，标记开始播放）：`+0x1ac`=3，`DAT_6fbe3804` 加 1，`+0x17c` 记 `GetTickCount()`，对象同时挂入两条全局链表（`PTR_LOOP_6fb66f80` 播放链表与 `PTR_LOOP_6fb66f50` 同名/哈希链表）。
- `FUN_6f1703c0`（271 字节，标记延迟/停播）：若 `+0x1ac` 为 3 则 `DAT_6fbe3804` 减 1；`+0x1ac`=2；对象从三条链表摘除后挂入延迟链表 `PTR_LOOP_6fb66f5c`。
- `FUN_6f1712d0`（163 字节，停止）：按句柄类型先把音量置 0 再停：2D `_AIL_stop_sample`、3D `_AIL_stop_3D_sample`、流 `_AIL_pause_stream(…,1)`、序列 `_AIL_stop_sequence`；随后 `FUN_6f1705e0`（271 字节，清理播放登记，**本体未反编译**）。
- `FUN_6f173f00`（141 字节，对象 teardown）：清各回调（`FUN_6f172b10`）、`FUN_6f1707e0`、序列 `_AIL_end_sequence`、流 `_AIL_pause_stream(…,1)` + `_AIL_close_stream`、DLS 资源 `FUN_6f169720`；四类句柄槽清零。
- `FUN_6f16fea0`（140 字节，状态查询）：`+0x1ac` 为 3 时按类型调 `_AIL_sample_status` / `_AIL_3D_sample_status` / `_AIL_stream_status` / `_AIL_sequence_status`，返回 4 或 0x10 视为已播完。
- `FUN_6f145e30`（257 字节）与 `FUN_6f1462c0`（257 字节，3 个调用者）：周期性扫表函数——前者遍历播放链表处理 EOS/重播延迟声，后者按参数 0/1 分别处理“全部停播”（无延迟标志的声直接 teardown）或“重播延迟队列”（逐个调 `FUN_6f171ba0`）。**未验证**：两者被谁调用（Ghidra 里 `FUN_6f145e30` 显示 0 个调用者，疑似经虚表/回调指针调用；`FUN_6f1462c0` 的 3 个调用者 `FUN_6f144700/6f145880/6f1447f0` 本体未反编译）。

## 4. 数据结构

### 4.1 声音对象（`HLOOPINGSOUND` 0x1d4 字节；MIDI 版 `HLOOPINGMIDISOUND` 0x14c 字节）

由各函数读写交叉验证（字段名是推断，偏移是证据）：

| 偏移 | 内容（证据） |
|---|---|
| `+0x30` | DLS 数据指针（`FUN_6f1735f0` 写入；`FUN_6f171750` 传给 `_AIL_set_named_sample_file`） |
| `+0x34` | 文件名，0x104=260 字节（`FUN_6f16f0d0` 拷入；抢占比较键） |
| `+0x138` | DLS 已加载标志（`FUN_6f1735f0` 置 1） |
| `+0x13c` | 标志位（见 §3.3；另 bit 0=延迟播放相关、bit 0x1000=跳过流判定、bit 0x10000=声音禁用时仍可播、bit 0x2000000=已创建） |
| `+0x140` | 优先级（抢占比较） |
| `+0x144` | 音量（`FUN_6f145a20` 写入；`FUN_6f173cb0` 读取） |
| `+0x148` | 用户名（duplicate username 键） |
| `+0x14c` | 类型：1=2D 采样，2=3D 采样 |
| `+0x158` | 序列索引（-1 表示未分配，`FUN_6f171a70`） |
| `+0x15c` | 3D 采样句柄（H3DSAMPLE） |
| `+0x160` | 2D 采样句柄（HSAMPLE） |
| `+0x164` | 流句柄（HSTREAM）；`+0x174` 流标志 |
| `+0x168` | 序列句柄（HSEQUENCE）；`+0x16c` 类型 3=序列 |
| `+0x170` | DLS 相关资源非零时 `FUN_6f173f00` 调 `FUN_6f169720` 释放 |
| `+0x178` | 声道号 |
| `+0x17c` | 开始播放的 GetTickCount（最老声抢占比较键） |
| `+0x1a8` | 播放前置 0xffffffff |
| `+0x1ac` | 状态：2=延迟/排队，3=正在播放，5=停止（见 §3.2） |
| `+0x1b0` / `+0x1b4` | 目标 / 当前播放速率（`FUN_6f173940` 比较后同步） |
| `+0x1b8` | 3D 参数块（`FUN_6f16cbe0` 读取：标志字 + 距离 + 位置 + 速度 + 锥角 + 遮挡/衰减，见 §7） |
| `+0x1bc` | 距离截断值（`FUN_6f173cb0`、`FUN_6f32da10` 用） |
| `+0x1c4/0x1c8/0x1cc` | 声源 3D 位置（`FUN_6f173cb0` 读） |
| `+0x264` | 循环次数（传给各 `_AIL_set_*_loop_count`） |
| `+0x270` | 非零时跳过 `FUN_6f16caa0` 的关联对象创建（**含义未验证**） |

### 4.2 句柄池（三种，各 16 条记录，每条 0x10 字节）

三个分配函数结构完全相同（循环最多 16 次，失败即停；记录挂入对应 `PTR_LOOP` 链表）：

| 池 | 分配函数 | 句柄 | 计数/基址全局量 |
|---|---|---|---|
| 3D 采样 | `FUN_6f16edc0`（`_AIL_allocate_3D_sample_handle`） | `DAT_6fbda508` 提供者 | `DAT_6fbe374c` / `DAT_6fbe3750` |
| 2D 采样 | `FUN_6f16ec30`（`_AIL_allocate_sample_handle`） | `DAT_6fbda510` 数字驱动 | `DAT_6fbe375c` / `DAT_6fbe3760` |
| MIDI 序列 | `FUN_6f16f240`（`_AIL_allocate_sequence_handle`） | `DAT_6fbda518` XMIDI 驱动 | `DAT_6fbe376c` / `DAT_6fbe3770` |

释放集中在 `FUN_6f16ea50`（353 字节）：三池逐条 `_AIL_end_*` + `_AIL_release_*_handle`，计数与基址清零。单条释放另有 `FUN_6f1727b0`（2D：清 user_data、注销 EOS 回调、`_AIL_end_sample`）与 `FUN_6f172930`（序列）、`FUN_6f172870`（3D）等小包装。

### 4.3 全局句柄与开关

| 全局量 | 内容 |
|---|---|
| `DAT_6fbda510` | 数字驱动句柄 HDIGDRIVER（0=未打开） |
| `DAT_6fbda518` | XMIDI 驱动句柄 |
| `DAT_6fbda51c` | DLS 句柄 |
| `DAT_6fbda508` | 3D 提供者句柄 HPROVIDER |
| `DAT_6fbda95c` | 3D 听者句柄 H3DLISTENER |
| `DAT_6fbda958` | 声音总开关（`FUN_6f144b50` 置 1；`FUN_6f145a60` 清零） |
| `DAT_6fbe3804` | 当前播放声音数（上限 24） |
| `DAT_6fbe3800` | 当前 CSoundWar3 实例指针（thunk `FUN_6f173ea0` 设置；为空时用默认虚表 `PTR_vftable_6fb685b8`） |
| `DAT_6fbda538` / `DAT_6fbda534` | 通道表基址 / 通道数（每条目 0x14 字节：`+0`、`+4`、`+8` 为音量上下限类字段，`+0x10` bit0 为标志——**各字段精确含义未验证**） |
| `DAT_6fb66f20` | 全局标志字节：bit1=EAX 效果启用（`FUN_6f16cbe0` 用）、bit2=声音启用（`FUN_6f145020` 用） |
| `DAT_6fbda938/940/944/94c` | 听者缓存位置/朝向（`FUN_6f1453a0/6f1454a0` 用变化检测） |
| `DAT_6fbda974` | 听者脏标志：bit0=位置已变，bit1=朝向已变 |

### 4.4 CSoundWar3 虚表

`PTR_vftable_6fb685b8`。已验证的槽位用法：`+0x4` 为对象析构/释放类虚函数（`FUN_6f1445d0`、`FUN_6f144e30`、`FUN_6f145a60` 在播放入队失败或关闭时调 `( **(code **)(*obj + 4))()`）；`+0xc` 为带级别的日志虚函数（全部 `"Sound %s …"` 日志都经此槽，`FUN_6f16df70` 返回当前实例指针供各处取日志对象）。**完整虚表方法表未验证**（其余槽位未逐一确认）。

## 5. 文件加载与格式分发

- `FUN_6f16a600`（410 字节，`_AIL_file_type` 分发）：返回值 1/2=WAV（`_AIL_WAV_info` 读头信息，置 `+0x18` bit 0x9；类型 2 且带压缩标志时 `_AIL_decompress_ADPCM` 经 `FUN_6f169040`）；5=MIDI（`_AIL_MIDI_to_XMI` 转 XMI，内存由 `FUN_6f16acf0` 接管）；6/9=直接可用；7=`_AIL_find_DLS`；8/10=`_AIL_extract_DLS`；0xd=ASI 压缩（`FUN_6f168fb0`：`_AIL_decompress_ASI`，用 `DAT_6f961fe8` 的 ASI 表，成功后再 `_AIL_WAV_info`）；0/3/4/0xb/0xc/0xe=不支持（落到 `FUN_6f1691d0` 释放后返回 0）。类型码数值本身是 Miles 的枚举，**未逐一对照 Miles 头文件验证**。
- `FUN_6f141aa0`（115 字节，信息查询分发）：WAV 类（1/2/0xd）→ `FUN_6f141d10`；MIDI 类（5/6/7/8）→ `FUN_6f141b50`。
- `FUN_6f141d10`（323 字节，采样信息）：`_AIL_init_sample` + `_AIL_set_named_sample_file`（类型 0xd 用 `DAT_6f961fe8` 的 ASI 名字表，其余用 `DAT_6f961fe0` 的 WAV 名字表；`Ordinal_572` 返回的指针非空时优先用它）+ `_AIL_WAV_info` 填 9 个 dword 的信息结构（`FUN_6f141c40` 解析 MP3 帧头：比特率/采样率档位查表 `DAT_6f961ad0`、`DAT_6f961cd0`），最后 `_AIL_sample_ms_position` 取毫秒位置。
- `FUN_6f141b50`（231 字节，序列信息）：MIDI 先 `_AIL_MIDI_to_XMI` 再 `_AIL_init_sequence`，`_AIL_sequence_ms_position` 取位置。
- `FUN_6f144a00`（304 字节，文件加载 + 缓存）：先 `FUN_6f1439b0` 查缓存（命中直接拷 9 个 dword 的信息结构）；未命中则 Storm 序号 267 打开文件、269 读入 `Ordinal_401` 分配的内存、253 释放句柄，`FUN_6f141aa0` 填信息后 `FUN_6f1435b0` 登记缓存。
- `FUN_6f1735f0`（164 字节，DLS 音色加载）：`_AIL_DLS_load_memory` 失败且软件 DLS 可用时先 `_AIL_DLS_compact` 再重试；仍失败 → `"Error, cannot load DLSSound %s into memory! (last Miles error = %d)"`。
- MP3 播放：`FUN_6f171750` 经 `_AIL_set_named_sample_file` 用 `PTR_s_UNUSED_6f963b50` 表按 `+0x16c` 索引取流名字；采样率 > 22050（0x5622）且配置标志 `DAT_6fbda528` bit1 置位时记 `"Warning, sound \"%s\" is more than 22050Hz!"`（3D 分支 `FUN_6f1718f0` 同样）。

## 6. 四类播放后端

| 后端 | 启动函数 | 关键 Miles 调用 | 备注 |
|---|---|---|---|
| 2D 采样 | `FUN_6f171750`（396 字节） | `_AIL_set_named_sample_file`、`_AIL_set_sample_loop_count`、`_AIL_set_sample_playback_rate`、`_AIL_set_sample_user_data`、`_AIL_register_EOS_callback(FUN_6f172ee0)`、`_AIL_start_sample` / `_AIL_resume_sample` | 播放前 `FUN_6f173cb0` 统一设音量；`FUN_6f1708f0` 返回 0/2/3 分别对应正常/返回 1/重播（`FUN_6f1704d0`） |
| 3D 采样 | `FUN_6f1718f0`（364 字节） | `_AIL_set_3D_sample_info`、`_AIL_set_3D_sample_loop_count`、`FUN_6f16cbe0`（§7）、`_AIL_set_3D_user_data`、`_AIL_register_3D_EOS_callback(FUN_6f172f20)`、`_AIL_resume_3D_sample` | 3D 参数块在启动前由 `FUN_6f16cbe0` 一次性下发 |
| 流（音乐长文件） | `FUN_6f171c90`（191 字节） | `FUN_6f16f390`：`_AIL_open_stream(DAT_6fbda510, 文件名+0x34, 0x60000)`（缓冲 393216 字节）→ `_AIL_set_stream_loop_count`、`_AIL_set_stream_playback_rate`、`_AIL_register_stream_callback(FUN_6f172fa0)`、`_AIL_pause_stream(…,0)` 开始 | 位置查询用 `_AIL_stream_ms_position`（`FUN_6f16df90`） |
| MIDI 序列 | `FUN_6f171a70`（277 字节） | `_AIL_init_sequence`、`_AIL_set_sequence_loop_count`、`_AIL_set_sequence_user_data`、`_AIL_register_sequence_callback(FUN_6f172f60)`、`_AIL_start_sequence` / `_AIL_resume_sequence` | 要求 XMIDI 驱动与 DLS 句柄均已打开；序列句柄来自 `FUN_6f16dda0/6f16dd80` 分配 |

统一音量：`FUN_6f173cb0`（492 字节，7 个调用者）。先 `FUN_6f7712e0` 取基础音量（**该函数 385 字节，本体未反编译，推断为音量组 × 主音量换算**）；若启用距离衰减（`FUN_6f173eb0` 返回非零或 `param_3` 非零），按听者与声源距离平方与截断值 `+0x1bc` 比较，超出则 `sqrt` + `pow` 衰减；最后按类型调 `_AIL_set_sequence_volume` / `_AIL_set_stream_volume` / `_AIL_set_3D_sample_volume` / `_AIL_set_sample_volume`。播放速率：`FUN_6f173940`（314 字节，`FUN_6f145840` 的唯一被调者），速率钳在下限 `_DAT_6f956678`，三种后端分别调对应的 get/set playback_rate，`+0x1b0` 与 `+0x1b4` 相等时跳过。位置查询：`FUN_6f16df90`（346 字节），流/采样用 ms_position 差值，3D 用 `_AIL_3D_sample_length` − `_AIL_3D_sample_offset` 再经 `FUN_6f7712e0` 换算；`+0x13c` bit0 置位时返回 0x7fffffff（**含义未验证**）。

音乐入口：`FUN_6f33b770`（283 字节，调用者 `FUN_6f33b670`）——按 `param_2`（2，或 0 且 `DAT_6fbe5f7c==1`）决定用标志 1 还是 0x10000，经 `FUN_6f1445d0` 创建声音后 `FUN_6f145020` 播放，文件名存 `DAT_6fbe5c70`，句柄存 `DAT_6fbe5b5c`；`param_4` 非零时置 `DAT_6fb69494=1`、`DAT_6fbe61d4=param_4`、`DAT_6fbe61cc = 0.05f`（0x3d4ccccd）。**推断**：这是 `PlayMusic` / `PlayMusicEx` 的实现入口，但与 JASS native 的精确对应关系未逐一验证（见 §9）。

## 7. 3D 音频与听者

- 听者更新：`FUN_6f1454a0`（180 字节，位置）与 `FUN_6f1453a0`（242 字节，朝向）都带变化检测——仅当缓存值（`DAT_6fbda938/940/944/94c`）与新值不同且脏标志（`DAT_6fbda974` bit0/1）条件满足时才调 `_AIL_set_3D_position` / `_AIL_set_3D_orientation`（后者先 `FUN_6f140f80` 做坐标换算，498 字节，**本体未反编译**）。调用方均为 `FUN_6f8785a0`（125 字节，**本体未反编译，推断为相机/听者同步函数**）；位置更新另有调用方 `FUN_6f2e2790`、`FUN_6f2a78c0`。
- 单声 3D 参数下发：`FUN_6f16cbe0`（678 字节，10 个 AIL 导入，`FUN_6f1718f0` 的唯一调用者），按 `+0x1b8` 参数块的标志字逐项下发：bit3 锥角（`_AIL_set_3D_sample_cone`）、bit0 距离（`_AIL_set_3D_sample_distances`）、bit4 朝向（`_AIL_set_3D_orientation`）、bit1 位置（`_AIL_set_3D_position`）、bit2 速度（向量归一化：长度 `sqrt` 后乘 `DAT_6f950d8c` 缩放，再 `_AIL_set_3D_velocity`）、bit6 遮挡（`_AIL_set_3D_sample_obstruction`）、bit7 衰减（`_AIL_set_3D_sample_occlusion`，上限 `DAT_6f9641bc`）、bit5 效果级（EAX 启用时 `param_2[0xf] * _DAT_6fb66f2c`，否则 0）；最后处理 EAX2 采样标志（bit8 时读 `_AIL_3D_sample_attribute("EAX2 sample flags")` 再 `_AIL_set_3D_sample_preference` 置/清 bit1）。
- 提供者偏好：`FUN_6f145720`（215 字节），按 `DAT_6f9619b0` 的偏好表（每项 0x14 字节：类型/上下限）把 `EAX effect volume` 等偏好值钳制后调 `_AIL_set_3D_provider_preference`；索引 0 的偏好直接写 `_DAT_6fb66f2c`（效果总音量）。房间类型：`FUN_6f145820`（27 字节）调 `_AIL_set_3D_room_type`。**未验证**：偏好表的完整条目语义与 `EAX effect volume` 之外的偏好名字符串。
- XMIDI 主音量：`FUN_6f145370`（42 字节），0–127 钳制后 `_AIL_set_XMIDI_master_volume`。

## 8. 与其它阶段的关系

- **S0/S1（帧同步/网络）**：声音播放本身不同步——`FUN_6f145020` 的延迟队列与抢占是本地行为；但 JASS 声音 native（`StartSound` 等）由触发器在回合内调用，调用时序随帧同步走（S4 的 native 注册表是入口，见 §9）。
- **S2（主循环）**：初始化经 `FUN_6f0280e0`（UI 初始化）进入 `FUN_6f339df0`；关闭经 `FUN_6f33d380` → `FUN_6f145a60`；周期扫表 `FUN_6f145e30/6f1462c0` 的调用方未验证（见 §9）。
- **S4（JASS2）**：声音 native 在 `FUN_6f1e9a50` 注册（S4 已验证注册表结构），本阶段提供实现侧。native 名与签名（字符串表 0x6f96e0c4 起）：`NewSoundEnvironment`、`CreateSound(SBBBIIS)Hsound;`、`CreateSoundFilenameWithLabel`、`CreateSoundFromLabel(SBBBII)Hsound;`、`CreateMIDISound(SII)Hsound;`、`SetStackedSound`、`ClearStackedSound`、`SetStackedSoundRect`、`ClearStackedSoundRect`、`SetSoundParamsFromLabel(Hsound;S)V`、`SetSoundChannel`、`SetSoundVolume`、`SetSoundPitch`、`SetSoundPlayPosition`、`SetSoundDistances(Hsound;RR)V`、`SetSoundDistanceCutoff`、`SetSoundConeAngles(Hsound;RRI)V`、`SetSoundConeOrientation(Hsound;RRR)V`、`SetSoundPosition`、`SetSoundVelocity`、`AttachSoundToUnit(Hsound;Hunit;)V`、`StartSound`、`StopSound(Hsound;BB)V`、`KillSoundWhenDone`、`SetMapMusic`、`ClearMapMusic`、`PlayMusic`、`PlayMusicEx`、`StopMusic`、`ResumeMusic`、`PlayThematicMusic`、`PlayThematicMusicEx`、`EndThematicMusic`、`SetMusicVolume`，以及 `TriggerWaitForSound`、`SaveSoundHandle` / `LoadSoundHandle`。各 native 到 Game 函数的映射**未逐一验证**（只验证了注册表字符串与 `FUN_6f33b770` 等个别入口）。
- **S5（Gx 渲染）**：听者位置/朝向与相机同源（`FUN_6f8785a0` 同时驱动两者，**本体未验证**）；3D 声音的距离截断与渲染的视锥裁剪是两套独立的距离判定。
- **S6（UI）**：`UI\SoundInfo\MiscData.txt` 字符串存在（0x6f951304），但**加载它的函数未定位**（字符串无函数内引用）；配置键 `musicvolume`（0x6f950864）、`nosoundwarn`（0x6f9508c8）同样只见字符串、未见读取函数。
- **S7（Storm/MPQ）**：Miles 文件回调（`FUN_6f1438e0/6f143fe0/6f143b20` + `LAB_6f1414d0`）分别对应 Storm 序号 267（打开）、269（读）、271（seek）、253（释放）；`FUN_6f144a00` 的缓存加载同样走 267/269/253。`Ordinal_401/403` 是 Storm 侧的 new/delete 包装（S7 已验证）。
- **S9（录像）**：声音播放不进录像流（本轮未发现录像相关的声音记录；待 S9 验证）。

## 9. 关键函数地址表

### 9.1 初始化 / 关闭

| 地址 | 大小 | 作用 |
|---|---|---|
| `FUN_6f339df0` | 1087 | CSoundManager 初始化（日志、配置、驱动、3D、回调注册、音量组） |
| `FUN_6f144b50` | 142 | 驱动打开包装（调 `FUN_6f1423c0`，失败时再试 `FUN_6f142720`） |
| `FUN_6f1423c0` | 284 | 数字驱动打开：redist 目录、AIL_startup、open_digital_driver、文件回调 |
| `FUN_6f142570` | 418 | XMIDI + DLS 打开（S3BASE → VORT_DLS → 软件 DLS） |
| `FUN_6f16ec30` / `FUN_6f16f240` / `FUN_6f16edc0` | 各 332 | 三类句柄池预分配（2D 采样 / MIDI 序列 / 3D 采样，各 16 条） |
| `FUN_6f32b0c0` | 12（+`FUN_6f32b0cc` 164） | 3D 提供者枚举与选择 |
| `FUN_6f144840` | 72 | `_AIL_enumerate_3D_providers` 枚举包装 |
| `FUN_6f145610` | 259 | 3D 提供者 + 听者打开、距离因子 |
| `FUN_6f145a60` | 687 | 声音子系统关闭（调 `FUN_6f144350` + `_AIL_shutdown`） |
| `FUN_6f144350` | 174 | 关闭全部六个 Miles 句柄 |
| `FUN_6f16ea50` | 353 | 释放三类句柄池全部句柄 |
| `FUN_6f339d90` | 59 | 8 个音量组初始化为 1.0f |
| `FUN_6f32fb20` | 133 | 声音标签/主声音表等子表初始化 |

### 9.2 创建 / 播放 / 停止

| 地址 | 大小 | 作用 |
|---|---|---|
| `FUN_6f339670` | 864 | 创建 HLOOPINGSOUND（0x1d4 字节） |
| `FUN_6f339550` | 279 | 创建 HLOOPINGMIDISOUND（0x14c 字节） |
| `FUN_6f1445d0` | 297 | SndCreate：通道钳制 + `FUN_6f16f0d0` 填对象 |
| `FUN_6f16f0d0` | 194 | 声音对象字段填充 |
| `FUN_6f145020` | 287 | 播放总入口（总开关、延迟队列） |
| `FUN_6f144e30` | 494 | 带 3D 参数的播放入口 |
| `FUN_6f32da10` | 1126 | 高层播放入口（主声音、距离截断） |
| `FUN_6f171ba0` | 231 | 播放分发器（流/序列/2D/3D） |
| `FUN_6f171380` | 962 | 重复名/抢占仲裁（返回 3=拒绝） |
| `FUN_6f171230` | 147 | 停掉全部同名旧声 |
| `FUN_6f1748c0` | 94 | 查 AUSOUNDIDHASH（同文件是否在播） |
| `FUN_6f16db40` | 60 | 找当前最低优先级播放声 |
| `FUN_6f1706f0` | 227 | 标记开始播放（入链表、计数+1） |
| `FUN_6f1703c0` | 271 | 标记延迟/停播（出链表、入延迟队列） |
| `FUN_6f1712d0` | 163 | 停止（音量归零后按类型停） |
| `FUN_6f173f00` | 141 | 对象 teardown（关流/序列、清回调） |
| `FUN_6f16fea0` | 140 | 播放状态查询（status 4/0x10=播完） |
| `FUN_6f145e30` / `FUN_6f1462c0` | 各 257 | 周期扫表（EOS/重播/停播），调用方未验证 |
| `FUN_6f33b770` | 283 | 音乐播放入口（推断，见 §6） |

### 9.3 四类后端

| 地址 | 大小 | 作用 |
|---|---|---|
| `FUN_6f171750` | 396 | 2D 采样启动 |
| `FUN_6f1718f0` | 364 | 3D 采样启动 |
| `FUN_6f171c90` | 191 | 流启动（`FUN_6f16f390` 开流） |
| `FUN_6f16f390` | 69 | `_AIL_open_stream(…, 0x60000)` |
| `FUN_6f171a70` | 277 | MIDI 序列启动 |
| `FUN_6f173cb0` | 492 | 统一音量（含距离衰减） |
| `FUN_6f173940` | 314 | 统一播放速率（音调） |
| `FUN_6f16df90` | 346 | 统一位置查询 |
| `FUN_6f173ad0` | 85 | 统一 ms 位置设置（流/采样） |

### 9.4 3D 与听者

| 地址 | 大小 | 作用 |
|---|---|---|
| `FUN_6f1453a0` | 242 | 听者朝向更新（变化检测 + `_AIL_set_3D_orientation`） |
| `FUN_6f1454a0` | 180 | 听者位置更新（变化检测 + `_AIL_set_3D_position`） |
| `FUN_6f16cbe0` | 678 | 单声 3D 参数下发（锥角/距离/朝向/位置/速度/遮挡/EAX2） |
| `FUN_6f145720` | 215 | 3D 提供者偏好（EAX effect volume 等） |
| `FUN_6f145820` | 27 | `_AIL_set_3D_room_type` |
| `FUN_6f145370` | 42 | XMIDI 主音量（0–127 钳制） |
| `FUN_6f145160` | 74 | 距离因子钳制（写入 `_DAT_6fb66f28`） |

### 9.5 文件 / 格式 / 内存

| 地址 | 大小 | 作用 |
|---|---|---|
| `FUN_6f16a600` | 410 | `_AIL_file_type` 分发（WAV/MIDI/DLS/ASI） |
| `FUN_6f141aa0` | 115 | 信息查询分发（采样/序列） |
| `FUN_6f141d10` | 323 | 采样信息（WAV_info / MP3 帧头解析） |
| `FUN_6f141b50` | 231 | 序列信息（MIDI_to_XMI / init_sequence） |
| `FUN_6f168fb0` | 133 | ASI 解压（`_AIL_decompress_ASI`） |
| `FUN_6f169040` | 120 | ADPCM 解压（`_AIL_decompress_ADPCM`） |
| `FUN_6f144a00` | 304 | 文件加载 + 信息缓存 |
| `FUN_6f1735f0` | 164 | DLS 音色加载（失败 compact 后重试） |
| `FUN_6f1438e0` / `FUN_6f143fe0` / `FUN_6f143b20` | 51/83/33 | Miles 文件回调：打开/ seek / 读取（Storm 267/271/269） |
| `FUN_6f143290` / `FUN_6f143310` | 118/84 | Miles 内存钩子：10 档分级分配/释放（`W32\OsSnd.cpp`） |
| `FUN_6f16df70` | 17 | 取当前 CSoundWar3 实例指针 |
| `FUN_6f145a20` | 49 | 设对象音量并调 `FUN_6f173cb0` |
| `FUN_6f145840` | 33 | 设对象播放速率并调 `FUN_6f173940` |

### 9.6 导入表（Game.dll → mss32.dll，110 个）

按调用方归类（`audio_dump2.txt` 全表；括号内为 Game 调用者数）：

- 驱动/全局：`_AIL_startup`（1）、`_AIL_shutdown`（1）、`_AIL_open_digital_driver`（1）、`_AIL_close_digital_driver`（2）、`_AIL_set_redist_directory`（1）、`_AIL_set_file_callbacks`（1）、`_AIL_mem_use_malloc`/`_AIL_mem_use_free`（各 1）、`_AIL_last_error`（3）。
- 2D 采样：`_AIL_allocate_sample_handle`（1）、`_AIL_init_sample`（2）、`_AIL_set_named_sample_file`（2）、`_AIL_start/resume/stop/end_sample`（各 1–2）、`_AIL_set_sample_volume/pan/loop_count/playback_rate/ms_position/user_data`、`_AIL_sample_status/ms_position/position/playback_rate/user_data`、`_AIL_WAV_info`（4）、`_AIL_decompress_ASI/ADPCM`（各 1）。
- 3D：`_AIL_enumerate_3D_providers`（1）、`_AIL_open/close_3D_provider`（1/2）、`_AIL_open/close_3D_listener`（1/2）、`_AIL_set_3D_position/orientation/distance_factor/room_type/speaker_type/provider_preference`、`_AIL_allocate/release_3D_sample_handle`、`_AIL_set_3D_sample_volume/playback_rate/loop_count/cone/distances/effects_level/occlusion/obstruction/velocity/preference/info`、`_AIL_3D_sample_status/length/offset/playback_rate/attribute/user_data`、`_AIL_start/resume/stop/end_3D_sample`、`_AIL_register_3D_EOS_callback`。
- 流：`_AIL_open/close_stream`、`_AIL_pause_stream`（3）、`_AIL_set_stream_volume/pan/loop_count/playback_rate/ms_position/user_data`、`_AIL_stream_status/ms_position/position/playback_rate/user_data`、`_AIL_register_stream_callback`。
- MIDI/序列/DLS：`_AIL_open/close_XMIDI_driver`、`_AIL_init_sequence`、`_AIL_start/resume/stop/end_sequence`、`_AIL_set_sequence_volume/loop_count/user_data`、`_AIL_sequence_status/ms_position/user_data`、`_AIL_register_sequence_callback`、`_AIL_set_XMIDI_master_volume`、`_AIL_DLS_open/close/unload/compact/load_memory`、`_AIL_MIDI_to_XMI`、`_AIL_find_DLS`、`_AIL_extract_DLS`、`_AIL_file_type`（2）。
- **死导入（0 个 Game 调用者）**：`_AIL_digital_CPU_percent@4`、`_AIL_set_3D_speaker_type@8`——导入表里有，但 Game 代码从未调用（推断为 Miles 模板导入残留或预留接口）。

## 10. 未验证点

1. `FUN_6f32b0c0` 传给 `FUN_6f145610` 的常量 `0x3cd013a9` 的含义；3D 提供者枚举表 `DAT_6fb694b8` 的 20 字节条目完整布局。
2. 周期扫表 `FUN_6f145e30`（Ghidra 显示 0 个调用者）与 `FUN_6f1462c0` 的调用方——是否由主循环/定时器驱动、调用频率均未验证。
3. 听者同步函数 `FUN_6f8785a0`（`FUN_6f1453a0/6f1454a0` 的调用方）本体未反编译；与相机（S5）的具体同步关系未验证。
4. `UI\SoundInfo\MiscData.txt`、`musicvolume`、`nosoundwarn` 三个字符串的读取函数未定位（字符串存在，无函数内引用）。
5. 各 JASS 声音 native 到 Game 实现函数的映射未逐一验证（只验证了注册表字符串、`FUN_6f33b770` 等个别入口与 `FUN_6f1e9a50` 注册表结构）；`FUN_6f33b770` 与 `PlayMusic/PlayMusicEx` 的对应关系是推断。
6. 通道表 `DAT_6fbda538` 每条目 `+0/+4/+8/+0x10` 各字段的精确含义（只验证了钳制逻辑）；音量组 8 个分组的具体语义（`DAT_6fbe6250` 表）；`FUN_6f7712e0`（385 字节，音量换算核心）本体未反编译。
7. 声音对象 `+0x270` 字段、`+0x13c` 中 bit 0x1000/0x10000/0x2000000 以外未逐项确认的标志位、`+0x1ac` 状态 2/3/5 之外的取值、`FUN_6f16df90` 里 `+0x13c` bit0 置位时返回 0x7fffffff 的含义。
8. `_AIL_file_type` 返回码 0–0xd 与 Miles 枚举的逐一对照；MP3 名字表 `PTR_s_UNUSED_6f963b50` 的具体内容；`LAB_6f1414d0`（Miles 文件关闭回调）本体未反编译（推断为 Storm 序号 253 包装）。
9. `FUN_6f1705e0`（停止清理）、`FUN_6f1704d0`（重播分支）、`FUN_6f1708f0`（播放前状态检查）、`FUN_6f140f80`（3D 坐标换算）、`FUN_6f169720`（DLS 资源释放）等辅助函数本体未反编译。
10. 录像（S9）是否记录声音事件未验证；Battle.net 大厅音乐（S10）是否复用 `FUN_6f33b770` 音乐入口未验证。
