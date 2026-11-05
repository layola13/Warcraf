# S1 — 网络层完整分析

- 状态：已验证 2026-11-05（首轮，立即执行）
- 证据：`~/workspace/warcraf_ghidra/net_dump.txt`（导入表、Net 函数簇、关键函数调用者、校验和格式串指针搜索）、`net_decomp.txt`（13 个回合管线函数反编译）、`storm_dump.txt`（Storm.dll 1105 个函数）、`sync_decomp.txt`（事件泵与大分发器）、PE 导出/导入表解析（Python struct 直读）
- 上承：S0 帧同步核心（`~/workspace/warcraf_framesync_analysis.md`）

## 1. 总述：分层

```
Game.dll  Net 层（CNetData / NetRouter / NetClient / 事件系统 / TurnsSync）
    │  113 个 Storm 导入（112 个按序号 + 1 个具名）
    ▼
Storm.dll  传输与系统层（SNet 序号导出 / SFile MPQ / 同步原语 / 字符串与日志）
    │  WS2_32.dll（Winsock，26 个导入，按序号）/ WINMM timeGetTime
    ▼
操作系统网络栈
```

关键事实：Game.dll 与 Storm.dll 之间**几乎全是序号导入**（112/113），只有 `SErrSetBlizzardErrorFunction` 一个具名导入；Storm.dll 814 个导出里只有 54 个具名，且全是 `SFile`（MPQ）方法——SNet 的函数名在导出表里不存在，只能靠序号 + 反编译识别。

## 2. 传输层：Storm.dll

- Storm.dll：334 KB，32 位 PE，Ghidra 分析 1105 个函数，基址 0x15000000。
- 导出表：814 个导出，序号基数 101，具名 54 个；具名部分全是 `SFile`（见下）。
- 具名 `SFile` 导出（MPQ 文件层，S7 会深挖，这里只列网络层相关的调用面）：`SFile::Open/Close/Read/Load/LoadFile/GetFileSize/FileExists/GetBasePath/SetBasePath/SetFilePointer/PollOverlapped/WaitOverlapped/CreateOverlapped/DestroyOverlapped/ResetOverlapped/EnableHash`（序号 151–195 段）。
- Storm 内部具名函数（无导出名，供内部调用）：文件 I/O（`Open/Close/Read/Load`）、同步原语（`SCritSect`、`CSRWLock`、`SEvent`、`SSyncObject`、`SInterlocked*`）、线程（`SCreateThread`）、错误处理（`SErrSetBlizzardErrorFunction`）。
- Game.dll 导入的 112 个 Storm 序号（已验证的序号→Storm 虚拟地址对照，基址 0x15000000）：

| 序号段 | Storm 地址段 | 备注 |
|---|---|---|
| 151 | 0x150221e0 | 单个 |
| 251–253 | 0x15020ea0 / 0x15021540 / 0x15021610 | 密集簇（0x20e00–0x23800 段共 251–302） |
| 263–272、275、279–281、288–289、291、293–295、300 | 0x15022220–0x150238e0 | 同上簇 |
| 302 | 0x1503c110 | 单个（落在 0x3c000 段） |
| 401、403、405–406 | 0x1502b830–0x1502bf10 | 簇 |
| 421–426、428 | 0x15036130–0x15036700 | 簇 |
| 460–463、465、469–472、474、476、479、482 | 0x1501c790–0x1502bd10 | 跨两簇 |
| 496 | 0x1502c980 | 单个 |
| 501–504、506–510 | 0x1503a640–0x1503b890 | 字符串函数簇 |
| 524–525、534、537 | 0x150390e0–0x15039480 | 簇 |
| 541–545、548、553 | 0x150298e0–0x1502a330 | 簇（含日志口 548） |
| 551–552 | 0x15016120 / 0x15016310 | 小函数 |
| 563、565、567–568 | 0x1501c550–0x1501d7f0 | 簇 |
| 569–572、574–581、584、589–590、595–597 | 0x1503a5e0–0x1503af00 | 字符串簇（569 与 571 同地址 0x1503a5e0，570 与 572 同地址 0x1503a610——同一函数的两个序号入口） |
| 601–638 | 0x1500df20–0x1500e630 | 约 0x60 间隔的一串小函数（疑似小包装器/跳转表） |

已验证身份的三个序号（按调用点反编译确认，其余序号的函数身份**未验证**）：

- **548**：printf 风格日志输出口。调用形如 `Ordinal_548(fmt, ...)`，所有 checksum 打印（`syncNetEventChecksum = %08x`、`Last fog checksum`、`Net:\n`）都走它。
- **501**：有界字符串拷贝。调用形如 `Ordinal_501(dst, src, 0x20/0x10/0x36)`，路由器建立函数用它拷贝名字、密码等字段。
- **403**：条件日志/事件上报。`FUN_6f304680` 中 `if (*(p+0x70)) Ordinal_403(v, "%E…"串, -2, 0)`。

其它导入：`WS2_32.dll` 26 个（按序号，Winsock）；`WINMM.dll` 仅 `timeGetTime` 一个——回合定时很可能以它为时钟（推断，未验证）；`mss32.dll` 110 个 `AIL_*`（音频，S8）；`OPENGL32.dll` 50 个（渲染，S5）。

## 3. 事件系统：泵 + 双分发器

### 3.1 事件对象结构（反编译确认）

每个网络事件是一个链表节点 + 负载：类型字节 `+0x14`，标志字节 `+0x15`，负载指针 `+0x8`，负载长度 `+0xc`。类型 `> 0x3f` 会被钳到 `0x32`（大分发器入口）；`+0x610` 是本玩家槽位，`+0x1b40` 是当前处理中的事件指针，`+0x2274`/`+0x1c68` 是上下文标志。

### 3.2 事件泵 `FUN_6f30ade0`（954 字节）

- 两个调用者：`FUN_6f30ff00`、`FUN_6f310f20`（两个泵驱动点，推断：游戏循环与大厅/菜单各一处，未验证）。
- 遍历事件链表（头 `+0xf40`，next 在 `+0xf3c/+4`）。
- 分发前对每个事件负载算滚动校验并累加到 `+0x227c`：逐 dword `acc = rotl(acc ^ word, 3)`，尾字节逐字节处理，再按字节 ×`0x7a2d` 折叠。
- 类型 `0x1f` 的事件参与该校验；类型 `0x17` 走 TurnsSync 存储路径（三次 `FUN_6f0fb2c0`）；类型 `0x21`/`0x2f` 先 `FUN_6f02d2d0(负载, 长度)` 再调 `FUN_6f83e540/670`。
- 每个事件依次经过：前处理 `FUN_6f307980` → 大分发器 `FUN_6f3098d0` → 后处理 `FUN_6f307880`；类型 `≥ 0x40` 的事件改走扩展分发器 `FUN_6f307b50`。
- 前处理（38 字节）：空负载直接返回；否则 `FUN_6f312dc0`（引用计数 +1，推断）。
- 后处理（62 字节）：清当前事件指针，`FUN_6f3039f0()`，再 `FUN_6f78a5c0(事件, 0x18, "…CNetEvent_CNetData…", -2)`（释放 CNetEvent，带 RTTI 名）。

### 3.3 大分发器 `FUN_6f3098d0`（5074 字节，73 个被调函数）

按类型 `0x01–0x31` 的大 switch，生成对应的 `CNetEvent*` 对象（每个事件类型对应一个约 213 字节的构造函数，见 §3.5）：

| 类型 | 事件 |
|---|---|
| 0x01 / 0x02 | CNetEventConnect / Disconnect |
| 0x03–0x08 | GameListStart / Stop / Error / Add / Update / Delete |
| 0x09–0x0d | TeamGameListStart / Stop / Add / Update / Delete |
| 0x0e–0x0f | AnonGameFind / AnonGameJoin |
| 0x10 / 0x11 | GameCreate / GameAd |
| 0x12 / 0x13 | TeamGameAd / TeamInfo |
| 0x14 / 0x15 | GameFind / GameJoin |
| 0x16–0x18 | PlayerJoin / PlayerLeave / PlayerReady |
| 0x19 / 0x1a | GameSetup / GameClose |
| 0x1b | GameStart（兼写 TempReplay.w3g） |
| 0x1c / 0x1d | GameReady / PlayerUpdate |
| 0x1e / 0x1f | 回合同步数据出入 → `FUN_6f310c80`（§4） |
| 0x20 | 回合包解析 → `FUN_6f30b1a0`（§4） |
| 0x21 | CNetEventSetTurnsLatency（回合延迟设置） |
| 0x22 / 0x23 | 回合应用 + 校验和步骤（`FUN_6f3048c0` / `FUN_6f304680`） |
| 0x24 / 0x25 | GameSuspend / PlayerResume |
| 0x26–0x28 | RouterHandoffSearching / Syncing / Done |
| 0x29 / 0x2a | RouterUnresponsive / RouterResponsive |
| 0x2b–0x2d | DistFileStart / Progress / Complete（地图分发） |
| 0x2e | OfficialPlayers |
| 0x2f | CNetEventTrigger |
| 0x30 / 0x31 | CNetEventTrustedDesync / CNetEventTrustedResult（仅游戏进行中且 `+0xabc != 0` 时触发） |

分发器还维护逐玩家状态表（步长 `0x304`，`+0x278` 为每玩家状态计数器），并在录像开启时（`+0x1b44`）写 `NET_COMMAND_SAVE` 记录。

### 3.4 扩展分发器 `FUN_6f307b50`（2625 字节，56 个被调函数）

类型 `≥ 0x40` 的事件（减去 `0x40` 后查表），全部生成 `CBattleNetEvent*` 对象：Connect、Disconnect、Logon、Create、Change、UpgradeUpdate、DownloadUpgradeFile、CheckAd、QueryAdUrl、NewsStart/Stop/Add、FriendsStart/Stop/Add/Remove/Position/Update、ReadUserData、Profile、ClanProfile、CheckDataFile、EnterChat、ChannelList、ChatEvent、MessageBox、TeamGetPotentials、AnonProfile（+Clan）、ClanGetPotentials、ClanCreate、ClanFounder、ClanDisband、ClanDesignate、ClanAdded、ClanRemoved、ClanMemberAdd/Remove/Invite/Rank、ClanInfoGet、ClanListStart/Stop/Remove/Update、ClanChangeRank、TournamentInfo、ScheduledGame、MatchmakingInfo、IconList。特殊 case `0x2b`（类型 `0x6b`）：`FUN_6f2ffe50(0x400900e2)` 后走 `FUN_6f30ada0` 处理数据负载。

结论：网络事件分两个空间——`0x01–0x31` 是游戏内同步事件，`0x40+` 是 Battle.net 客户端事件（聊天、好友、战队、天梯、匹配），共用同一条泵与对象模型。

### 3.5 事件构造器函数群

`6f2f35d0–6f2f5400` 一带聚集了大量约 213 字节、4–5 个被调函数、仅 1 个调用者的函数——与两分发器的 `CNetEvent*` / `CBattleNetEvent*` 构造调用一一对应（每个约 213 字节的函数就是一个事件类的构造函数）。另有一个 7 字节、250 个调用者的 `FUN_6f2f20e0`（极小共享桩函数，疑似取值/空返回桩，未深究）。

## 4. 回合同步管线

### 4.1 TurnsSync 出入 `FUN_6f310c80`（435 字节）

参数：玩家槽、数据、标志。逐玩家字节会计：

- 玩家槽为 0 时全局计数 `+0x1c68` 加一。
- `FUN_6f3086a0(数据, 槽)` 得出本次字节数；为 0 则标志清零。
- 每玩家表（步长 `0x304`）`+0x284 += 字节数`；全局剩余 `+0x225c -= 字节数`。
- 标志置位时：带宽滑动平均 `+0x2258 = +0x2258*7 + 字节数 >> 3`（7/8 旧值 + 1/8 新值）；每玩家倒计时 `+0x2e0` 递减，归零时调 `FUN_6f30b460(槽, 0)`（玩家就绪/超时处理，未验证具体语义）。
- 本玩家槽 + 标志置位时：依次调 `FUN_6f3023c0()`、`FUN_6f304920()`、`FUN_6f840bd0()`、`FUN_6f3041c0()`（本方回合数据到齐后的推进动作，未验证各函数身份）。
- 模式判断：`+0x2288` 是 4 字符模式标记，`0x4c4f4f50`（"LOOP"）为回环/本地局，`0x4e4f4e45`（"NONE"）为无网络；本地玩家且满足条件时调 `FUN_6f3130c0`（推进回合，推断）。

### 4.2 回合包解析 `FUN_6f30b1a0`（654 字节，事件 0x20）

从数据存储读子记录流，按子类型字节分发：`0x10–0x14` 与 `0x20` 各对应一个构造调用（`FUN_6f2fc4b0/6f2fc7d0/6f2fc590/6f2fc710/6f2fc650/6f2fc3d0`）。即事件 0x20 是一个“回合数据包”，内含多条子记录。

### 4.3 发送与命令日志

- `FUN_6f2feb10`（338 字节）：建 `CDataStoreCache<1460>`，调 `FUN_6f311750(事件类型, &store)` 发送事件；大分发器以 `0x22` 为参数调它——即回合/事件发送口。
- `FUN_6f2fd0d0`（59 字节）：记日志 `War3Net: (INFO): turn %u: %s from %u`，再调 `FUN_6f30ada0` 处理该命令——每条网络命令都按回合号 + 内容 + 来源玩家记一条日志（即 `NET_COMMAND_SAVE` 路径）。
- `FUN_6f30e2b0`（21 字节）：返回魔数 `0x57415233`（"WAR3"，混乱之治）或 `0x57335850`（"W3XP"，冰封王座，由全局 `DAT_6fb592f0` 决定）——录像/协议头的游戏类型标记。

### 4.4 状态机：录制与回放

- `FUN_6f3128d0`（95 字节）：`CNetData::SetRecordState`，在 `+0xbe4` 上做录制状态转移，非法转移记日志。
- `FUN_6f312930`（127 字节）：`CNetData::SetReplayState`，在 `+0x614` 上做回放状态转移（0→1→2→3→4，带合法性校验，非法转移记日志 `CNetData::SetReplayState (from %s to %s)`）。

## 5. 路由与连接

- `FUN_6f283540`（612 字节）：路由器建立。按标志位组装连接参数（`0x200` 复用既有连接；`0x10`/`0x400000` 置位时参数 6 固定为 `0xc`；`0x80000000` 由 param_14 控制）；用 Storm 501 有界拷贝名字（0x20）、密码（0x10）等字段；引用字符串 `Router is running in lockstep mode`。调用者：`FUN_6f2c3160`、`FUN_6f283320`、`FUN_6f281330`（三处建连点，未验证各自场景）。
- `CNetEventSetTurnsLatency`（事件 0x21）：回合延迟可调，用来吸收网络抖动。
- `RouterHandoff*`（0x26–0x2a）：主机交接事件；`RouterUnresponsive/Responsive`（0x29/0x2a）：对端无响应检测。
- `DistFile*`（0x2b–0x2d）：地图文件分发，保证开局状态一致。

## 6. 校验和：三层 + 回调表机制

- 七子系统校验和：`%scheat / %scombat / %sgame / %snet / %srand / %stempest / %sunit checksum %08x`（地址 0x6f987de8–0x6f987e6b，相邻存放）。
- 网络事件流校验和：`syncNetEventChecksum = %08x`（`FUN_6f3079f0`，59 字节，经虚表取总值打印）。
- 战争迷雾校验和：`Last fog checksum was %d`（`FUN_6f231cd0`，43 字节）。
- **关键结构发现**：七子系统格式串在全二进制里没有任何指针命中，`FUN_6f3079f0` 与 `FUN_6f231cd0` 在 Ghidra 静态调用图里也没有任何调用者——说明校验和输出函数是通过回调表/虚表注册调用的，静态调用图到不了它们。这是“回调注册、非直接调用”的设计，不是分析遗漏。
- 网络事件滚动校验算法（`FUN_6f30ade0` 内联）：逐 dword `acc = rotl(acc ^ word, 3)`，尾字节逐字节处理，再按字节 ×`0x7a2d` 折叠进累加器。

## 7. 关键函数地址表

| 地址 | 函数 | 作用 |
|---|---|---|
| 6f30ade0 | FUN_6f30ade0 | 事件泵（954 字节），调用者 FUN_6f30ff00 / FUN_6f310f20 |
| 6f3098d0 | FUN_6f3098d0 | 游戏内事件大分发器（5074 字节，0x01–0x31） |
| 6f307b50 | FUN_6f307b50 | Battle.net 事件分发器（2625 字节，类型 ≥0x40） |
| 6f307980 / 6f307880 | — | 事件前处理（引用计数）/ 后处理（释放 CNetEvent） |
| 6f310c80 | FUN_6f310c80 | TurnsSync 出入：逐玩家字节会计、带宽滑动平均、倒计时 |
| 6f30b1a0 | FUN_6f30b1a0 | 事件 0x20 回合包解析（子记录 0x10–0x14/0x20） |
| 6f2feb10 | FUN_6f2feb10 | 事件发送口（经 FUN_6f311750） |
| 6f2fd0d0 | FUN_6f2fd0d0 | 按回合记网络命令日志 + 分发命令 |
| 6f30e2b0 | FUN_6f30e2b0 | 返回 WAR3 / W3XP 魔数 |
| 6f3128d0 / 6f312930 | — | CNetData::SetRecordState / SetReplayState 状态机 |
| 6f283540 | FUN_6f283540 | 路由器建立（锁步模式） |
| 6f3079f0 / 6f231cd0 | — | syncNetEventChecksum / 迷雾校验和打印（回调表调用） |
| 6f3048c0 / 6f304680 | — | 13 字节转发桩 / Storm 403 条件日志 |
| 1502a330 | Storm ord 548 | printf 风格日志口 |
| 1503a6c0 | Storm ord 501 | 有界字符串拷贝 |
| 1502be40 | Storm ord 403 | 条件日志/事件上报 |

## 8. 未验证点（留给后续轮次或专项）

1. 112 个 Storm 序号导入里，除 548/501/403 外的函数身份（需逐个反编译 Storm 对应地址；重点是 251–253、263–272、401–428、460–482、524–553 段，疑似 SNet 收发/建连/枚举）。
2. 七子系统校验和各自的哈希函数（回调表里的实现）。
3. 回合节奏数值（每秒几回合）与 `SetTurnsLatency` 的具体换算；泵驱动点 `FUN_6f30ff00` / `FUN_6f310f20` 的调用链与 `timeGetTime` 的关系。
4. 丢包重传与“等齐”超时逻辑（`FUN_6f30b460` 与 `+0x2e0` 倒计时的完整语义）。
5. 事件 0x22/0x23 的完整语义（回合应用的两个步骤到底各做了什么）。
6. `FUN_6f2f20e0`（7 字节、250 个调用者）的身份。
7. `CNetData` 对象全貌（`+0x610` 玩家槽、`+0xbe4` 录制态、`+0x614` 回放态、`+0x2288` 模式标记等字段的完整布局）。
