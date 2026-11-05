# War3 1.27a 帧同步（Lockstep）架构还原

- 目标（用户 2026-11-05）：仅研究引擎内核，分析学习它是如何做到帧同步的
- 证据：Game.dll（1.27.0.52240）经 Ghidra 12.2 完整分析（97,839 个函数，21 分钟），关键函数已反编译
- 图解版：`~/workspace/warcraf_framesync_architecture.html`（Mermaid 图）
- Ghidra 工程：`~/workspace/warcraf_ghidra/war3`；反编译原文 `sync_decomp.txt`；函数与调用边 `sync_dump2.txt`
- 工具链：Ghidra 12.2（需 JDK 25+；JDK 21 被拒后改装 Temurin 25.0.5）+ JDK 25，位于 `~/workspace/tools/`；headless 堆上限 `-Xmx2G`（launch.properties 追加独立 VMARGS 行，注意不要拼进 `--sun-misc-unsafe-memory-access` 的值里）
- 脚本教训：Ghidra 12.2 headless 不启用 PyGhidra，`.py` 脚本不可用；用 `.java` 脚本（Ghidra 现场编译），`-postScript` 要带扩展名；DLL 无符号，函数名全是 FUN_xxx，定位靠字符串 xref + RTTI 类型名

## 一句话结论

确定性锁步：所有客户端不各自推进模拟，而是把输入打包成“回合（Turn）”里的命令发给所有人；每回合只有当所有玩家该回合的命令块（TurnsSyncChunk）都到齐，各端才用相同输入 + 相同随机种子推进同一回合；模拟后各子系统算校验和并交换比对，不一致触发可信 desync 事件。

## 回合数据模型（RTTI）

- `Net::ClientTurn`：单个玩家一个回合的命令集合
- `Net::ClientTurnsSync`：单个玩家的回合同步状态
- `Net::TurnsSyncChunk`：线上传输的回合数据块
- `CTurnsSyncStore`：按回合号组装收到的块，等齐才放行模拟
- `Net::NetRouter` / `Net::NetClient`：收发、主机交接、延迟设置

## 命令是网络对象

`CNetCommandBase` 族：AllyPing、ArrowKey、技能命令（CAbility*Command），以及作弊码（CNetCommandCheatBase + 20 多个子类：God/Gold/Lumber/FastBuild/ShowMap/Victory/Defeat…）。命令先同步、后执行，本地输入不立刻生效。

## 事件泵与大分发器（已反编译）

- `FUN_6f30ade0`（954 字节）：网络事件泵，遍历事件链表（头 +0xf40）；对事件负载算滚动校验（逐 dword `acc = rotl(acc ^ word, 3)`，尾字节同样处理，再按字节 ×0x7a2d 折叠进 +0x227c 累加器）；逐个调用前处理 `FUN_6f307980` → 大分发器 → 后处理 `FUN_6f307880`
- `FUN_6f3098d0`（5074 字节，73 个被调函数）：按事件类型 0x01–0x31 的大 switch，生成 CNetEvent*（Connect/Disconnect/GameList/GameCreate/GameJoin/PlayerJoin/Leave/Ready/GameSetup/GameStart/GameReady/Trigger…）；case 0x1e/0x1f 回合同步数据出入（FUN_6f310c80），0x20（FUN_6f30b1a0），0x22/0x23 应用回合 + 校验和步骤（FUN_6f3048c0/6f304680），0x21 SetTurnsLatency，0x26–0x2a RouterHandoff（主机交接），0x2b–0x2d DistFile（地图分发），0x30 TrustedDesync / 0x31 TrustedResult（仅游戏进行中）；GameStart 时写 TempReplay.w3g，每条网络命令可写 NET_COMMAND_SAVE
- `FUN_6f3079f0`（59 字节）：打印 `syncNetEventChecksum = %08x`（网络事件流校验和总值）
- `FUN_6f231cd0`（43 字节）：打印 `Last fog checksum`（战争迷雾校验和）
- `FUN_6f283540`：引用 `Router is running in lockstep mode`（路由器锁步模式）

## 校验和三层

1. 七子系统：`%scheat / %scombat / %sgame / %snet / %srand / %stempest / %sunit checksum %08x`
2. 网络事件流：`syncNetEventChecksum`（滚动校验，见上）
3. 战争迷雾：`Last fog checksum`

## 确定性来源

同一输入（命令随回合同步）+ 同一随机数（rand checksum 单独校验，确定性种子序列）+ 同一初始状态（DistFile 地图分发）+ 同一节奏（回合是推进单位，SetTurnsLatency 只调等待）+ 可验证（三层校验，跑偏以 TrustedDesync 事件定性上报）。

## 未验证 / 待深挖

回合的 wall-clock 节奏（每秒几回合）未确认；七子系统校验和的具体哈希函数与 CTurnsSyncStore 的等齐超时/丢包重传逻辑未展开；FUN_6f3098d0 的 0x15/0x1b 预处理分支只读了主干。
