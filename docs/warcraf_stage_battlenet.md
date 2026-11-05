# S10 — Battle.net / 大厅与匹配

- 状态：已验证 2026-11-06
- 证据：`~/workspace/warcraf_ghidra/bnet_dump.txt`（32 个 Battle.net 相关函数 + 前 8 个反编译）、`~/workspace/warcraf_ghidra/bnet_dump2.txt`（CONNSTATE 状态表、登录屏、广告管理器）、`~/workspace/warcraf_ghidra/net_decomp.txt`（Battle.net 事件分发器 `FUN_6f307b50` 全表）、`~/workspace/warcraf_ghidra/sync_decomp.txt`（游戏内大厅事件 0x01–0x31）
- 上承：S1 网络层（双分发器、事件泵、路由器）、S2 主循环、S6 UI（Frame 查找与按钮绑定模式）、S7 Storm 文件层（SNet 疑似传输层）

## 1. 总述

Battle.net 在 Game.dll 里分三层：

1. **连接/登录层（OsNet + BNCS 状态机）**：`OsNet` 命名空间提供 TCP 连接对象（`NETCONN` / `TCPLISTEN` / `NETCONNECT`），一套 13 个 `CONNSTATE_*` 状态描述从建连、鉴权（SRP 风格的 `ACCOUNTLOGON` / `ACCOUNTLOGONPROOF`）、升级检查到已登录的完整握手。网关域名不在 Game.dll 里，Storm.dll 里只有注册表路径 `Software\Battle.net\`——连接参数来自注册表/配置，传输本身走 Storm SNet（S1 遗留未验证点）。
2. **事件层（两张分发器）**：游戏内大厅事件（类型 0x01–0x31，走 `FUN_6f3098d0`）与 Battle.net 客户端事件（类型 0x41–0x80，走 `FUN_6f307b50`）共用同一条事件泵 `FUN_6f30ade0` 与同一套 `CNetEvent*` / `CBattleNetEvent*` 对象模型。大厅事件带回调函数指针（建房、加房、玩家进出等）；Battle.net 事件按类型构造对象，构造器本身完成解析与分发（S1 已验证的模式）。
3. **Glue 界面层（`Source\Glue\CBattleNet*`）**：主菜单、登录屏、建房屏、游戏列表、好友列表、战队列表、天梯/档案、锦标赛、新闻/广告，全部用 S6 的模式实现：`FUN_6f09ef40` 按名查 Frame，`FUN_6f2c3590` / `FUN_6f2c3460` 按名 + 数字 id 绑按钮。

## 2. 连接与登录状态机

### 2.1 CONNSTATE 状态表（0x6fa5c7f0 起连续存放）

| 序号（推断，按存放顺序） | 状态名 | 含义（按名字推断） |
|---|---|---|
| 0 | CONNSTATE_NONE | 未连接 |
| 1 | CONNSTATE_DISCONNECTING | 断开中 |
| 2 | CONNSTATE_CHAT_CONNECTING | 聊天服务器建连中 |
| 3 | CONNSTATE_AUTH_INFO | 鉴权信息交换 |
| 4 | CONNSTATE_AUTH_CHECK | 鉴权校验 |
| 5 | CONNSTATE_CONNECTED | 已连接（未登录） |
| 6 | CONNSTATE_UPGRADE_REQUIRED | 需要升级客户端 |
| 7 | CONNSTATE_AUTH_ACCOUNTLOGON | 账号登录请求 |
| 8 | CONNSTATE_AUTH_ACCOUNTLOGONPROOF | 登录证明（SRP 证明交换） |
| 9 | CONNSTATE_AUTH_ACCOUNTCHANGE | 改密请求 |
| 10 | CONNSTATE_AUTH_ACCOUNTCHANGEPROOF | 改密证明 |
| 11 | CONNSTATE_AUTH_ACCOUNTCREATE | 建号请求 |
| 12 | CONNSTATE_LOGGEDON | 已登录 |

另有 `Unknown CONNSTATE`（0x6fa5c948）。**未验证**：这 13 个字符串在 Ghidra 静态引用图里没有任何代码引用——它们应是通过指针表/计算寻址读取的（状态名查表打印），读取函数身份未验证；序号与状态的对应关系是按存放顺序的推断，未逐个核对。

### 2.2 OsNet 传输对象（RTTI 证据）

- `NETCONN@OsNet`、`NETSELSOCKPTR@OsNet`、`INPUT@LOOPCONN@OsNet` / `OUTPUT@OsNet`、`TCPACCEPT@OsNet`、`TCPHOSTADDRINFO@OsNet`、`TCPLISTEN@OsNet`、`NETCONNECT@OsNet`、`TSHashObjectChunk<NETSELSOCKPTR, HASHKEY_NONE>`——OsNet 内部用 select 式 socket 集合（`NETSELSOCKPTR` 哈希表）管理连接，另有回环连接 `LOOPCONN`（与 S1 的 `"LOOP"` 本地局模式标记呼应）。
- 日志：`OsNetLog.txt`、`OsNet: (INFO): %s` → `FUN_6f162ee0`（160 字节，9 个被调函数）。

### 2.3 登录屏 `FUN_6f2b9d60`（3132 字节，27 个被调函数，0 个静态调用者——Glue 回调注册）

绑定的按钮（名 + 数字 id）：`TOSButton`(0)、`NewAccountButton`(1)、`ChangePasswordButton`(3)、`ChangeEmailButton`(4)、`LogonButton`(5)、`AgreeButton`(8)、`DisagreeButton`(9)、`OKButton`(0x10)、`CancelButton`(0x11)。流程中调 `FUN_6f2c07c0()` 取 Battle.net 客户端对象（推断），`FUN_6f0b6500` 取两处文本（偏移 `+0x300` / `+0x2fc`，推断为服务条款文本），并用 `FUN_6f023e00` / `FUN_6f023e60(0x40, buf, 0x1000)` 读一块 4KB 数据（**未验证**：用途不明，疑似读注册表网关/账号配置）。

### 2.4 版本与升级

- `battlenet_V0` / `battlenet_V1`（0x6f951688 / 0x6f951698）：各 1 处出现，无代码引用——推断为版本化资源名前缀；主菜单 `FUN_6f2b7560` 里确实有 `Ordinal_578(buf, 0x100, "%s_V%d", "GlueSpriteLayerCenter", DAT_6fb592f0)` 的版本化拼串（`DAT_6fb592f0` 即 S1 的 WAR3/W3XP 标志，0=RoC、1=TFT，推断）。
- 事件 `UpgradeUpdate`（0x47）、`DownloadUpgradeFile`（0x48）、`CheckDataFile`（0x57）对应升级检查与文件下载；`CONNSTATE_UPGRADE_REQUIRED` 与之呼应。升级传输细节未验证。

## 3. Battle.net 事件分发器 `FUN_6f307b50`（2625 字节，56 个被调函数）

类型字节减 `0x40` 后查表（越界则走默认分支 0x41），每个 case 调一个约 213 字节的事件构造器（S1 已验证的模式）。全表如下（类型 → 事件名 → 构造器地址）：

| 类型 | 事件 | 构造器 |
|---|---|---|
| 0x41 | CBattleNetEventConnect | 6f2f4dd0 |
| 0x42 | CBattleNetEventDisconnect | 6f2f4fd0 |
| 0x44 | CBattleNetEventLogon | 6f2f5980 |
| 0x45 | CBattleNetEventCreate | 6f2f4ef0 |
| 0x46 | CBattleNetEventChange | 6f2f3830 |
| 0x47 | CBattleNetEventUpgradeUpdate | 6f2f65d0 |
| 0x48 | CBattleNetEventDownloadUpgradeFile | 6f2f50b0 |
| 0x49 | CBattleNetEventCheckAd | 6f2f3b30 |
| 0x4a | CBattleNetEventQueryAdUrl | 6f2f6080 |
| 0x4b | CBattleNetEventNewsStart | 6f2f5da0 |
| 0x4c | CBattleNetEventNewsStop | 6f2f5e80 |
| 0x4d | CBattleNetEventNewsAdd | 6f2f5c90 |
| 0x4e | CBattleNetEventFriendsStart | 6f2f55c0 |
| 0x4f | CBattleNetEventFriendsStop | 6f2f56a0 |
| 0x50 | CBattleNetEventFriendsAdd | 6f2f52e0 |
| 0x51 | CBattleNetEventFriendsRemove | 6f2f54e0 |
| 0x52 | CBattleNetEventFriendsPosition | 6f2f5400 |
| 0x53 | CBattleNetEventFriendsUpdate | 6f2f5780 |
| 0x54 | CBattleNetEventReadUserData | 6f2f6190 |
| 0x55 | CBattleNetEventProfile | 6f2f5f60 |
| 0x56 | CBattleNetEventClanProfile | 6f2f4c10 |
| 0x57 | CBattleNetEventCheckDataFile | 6f2f3c50 |
| 0x58 | CBattleNetEventEnterChat | 6f2f51c0 |
| 0x59 | CBattleNetEventChannelList | 6f2f3910 |
| 0x5a | CBattleNetEventChatEvent | 6f2f3a10 |
| 0x5b | CBattleNetEventMessageBox | 6f2f5b70 |
| 0x62 | CBattleNetEventTeamGetPotentials | 6f2f63e0 |
| 0x67 | CBattleNetEventAnonProfile | 6f2f35d0 |
| 0x68 | CBattleNetEventAnonProfileClan | 6f2f3710 |
| 0x6b | 特殊分支（无事件对象） | 见下 |
| 0x6c | CBattleNetEventClanGetPotentials | 6f2f42c0 |
| 0x6d | CBattleNetEventClanCreate | 6f2f3ef0 |
| 0x6e | CBattleNetEventClanFounder | 6f2f4190 |
| 0x6f | CBattleNetEventClanDisband | 6f2f40b0 |
| 0x70 | CBattleNetEventClanDesignate | 6f2f3fd0 |
| 0x71 | CBattleNetEventClanAdded | 6f2f3d30 |
| 0x72 | CBattleNetEventClanRemoved | 6f2f4cf0 |
| 0x73 | CBattleNetEventClanMemberAdd | 6f2f4880 |
| 0x74 | CBattleNetEventClanMemberRemove | 6f2f4b30 |
| 0x75 | CBattleNetEventClanMemberInvite | 6f2f4960 |
| 0x76 | CBattleNetEventClanMemberRank | 6f2f4a50 |
| 0x77 | CBattleNetEventClanInfoGet | 6f2f43b0 |
| 0x78 | CBattleNetEventClanListStart | 6f2f45a0 |
| 0x79 | CBattleNetEventClanListStop | 6f2f4680 |
| 0x7a | CBattleNetEventClanListRemove | 6f2f44c0 |
| 0x7b | CBattleNetEventClanListUpdate | 6f2f4760 |
| 0x7c | CBattleNetEventClanChangeRank | 6f2f3e10 |
| 0x7d | CBattleNetEventTournamentInfo | 6f2f64d0 |
| 0x7e | CBattleNetEventScheduledGame | 6f2f6280 |
| 0x7f | CBattleNetEventMatchmakingInfo | 6f2f5a90 |
| 0x80 | CBattleNetEventIconList | 6f2f5890 |

**无处理分支的类型**（分发器里没有对应 case）：0x43、0x5c–0x61、0x63–0x66、0x69–0x6a——这些类型到达时不构造事件对象（含义未验证，可能是预留/未使用）。

**特殊分支 0x6b**：先调 `FUN_6f2ffe50(0x400900e2)`，再走 `FUN_6f30ada0` 处理数据负载（S1 已验证）——这是唯一带数据负载直通的 Battle.net 事件分支，具体负载格式未验证。

## 4. 大厅：游戏列表、建房、加房

游戏内事件分发器 `FUN_6f3098d0` 里的相关事件（S0/S1 已验证，构造器地址见 `sync_decomp.txt`）：

| 类型 | 事件 | 构造器 | 回调（事件处理函数） |
|---|---|---|---|
| 0x0b | CNetEventTeamGameListAdd | 6f2f8790 | — |
| 0x0c | CNetEventTeamGameListUpdate | 6f2f8b40 | — |
| 0x0d | CNetEventTeamGameListDelete | 6f2f88a0 | — |
| 0x0e | CNetEventAnonGameFind | 6f2f66b0 | — |
| 0x0f | CNetEventAnonGameJoin | 6f2f6790 | — |
| 0x10 | CNetEventGameCreate | 6f2f6f50 | FUN_6f3107d0 |
| 0x11 | CNetEventGameAd | 6f2f6d50 | — |
| 0x12 | CNetEventTeamGameAd | 6f2f86b0 | — |
| 0x13 | CNetEventTeamInfo | 6f2f8c50 | — |
| 0x14 | CNetEventGameFind | 6f2f7080 | — |
| 0x15 | CNetEventGameJoin | 6f2f71a0 | FUN_6f310870 |
| 0x16 | CNetEventPlayerJoin | 6f2f7d00 | FUN_6f310ac0 |
| 0x17 | CNetEventPlayerLeave | 6f2f7df0 | FUN_6f310380 |
| 0x18 | CNetEventPlayerReady | 6f2f7ed0 | — |
| 0x19 | CNetEventGameSetup | 6f2f7930 | FUN_6f310940 |
| 0x1a | CNetEventGameClose | 6f2f6e70 | — |
| 0x1b | CNetEventGameStart | 6f2f7a40 | FUN_6f310990 |
| 0x1c | CNetEventGameReady | 6f2f7850 | FUN_6f310900 |
| 0x1d | CNetEventPlayerUpdate | 6f2f8090 | — |

其中 `GameCreate` / `GameJoin` / `PlayerJoin` / `PlayerLeave` / `GameSetup` / `GameStart` / `GameReady` / `SetTurnsLatency`（0x21，回调 `FUN_6f310b40`）在构造时传入了回调函数指针——即这些事件到达时由回调直接处理，而不是只生成对象。回调本体均未深究。

Glue 侧的大厅界面：

- **游戏列表屏** `FUN_6f2bdd90`（834 字节，18 个被调函数）：引用 `GameList` 字符串（0x6f985090 / 0x6f9850a0）；另有一组连续存放的 `GameList` 相关字符串（0x6f987319–0x6f98741d，推断为列表列名/格式串，未逐条核对）。
- **建房屏** `FUN_6f2b8da0`（989 字节，21 个被调函数，0 个静态调用者）：`MapInfoButton`(3)、`AdvancedOptionsButton`(2)、`CreateGameButton`(4)、`CancelButton`(5)；调 `FUN_6f2a16f0((DAT_6fb592f0 < 2 ? DAT_6fb592f0 : 0) + 0x46, 1, 0)`——按版本取标签（推断为游戏类型名）。
- **建房/加房屏** `FUN_6f2b92c0`（973 字节，19 个被调函数，0 个静态调用者）：同时引用 `CreateGame` 与 `JoinGame` 字符串——**本体未反编译**，只确认了入口与引用关系。

## 5. Battle.net 主菜单 `FUN_6f2b7560`（4146 字节，49 个被调函数，0 个静态调用者）

这是登录成功后的 Battle.net 主界面初始化函数，绑定的按钮（名 + 数字 id）：

| id | 按钮 | 功能（按名字） |
|---|---|---|
| 0 | StandardGameButton | 标准对战 |
| 1 | QuickStandardGameButton | 快速标准对战 |
| 2 | StandardTeamGameButton | 标准组队对战 |
| 3 | CustomGameButton | 自定义游戏 |
| 4 | TournamentButton | 锦标赛 |
| 7 | LadderButton | 天梯 |
| 8 | ProfileButton | 档案 |
| 10 | ChatChannelButton | 聊天频道 |
| 11（0xb） | EnterChatButton | 进入聊天 |
| 12（0xc） | QuitBattleNetButton | 退出 Battle.net |
| 13（0xd） | HelpButton | 帮助 |
| 14（0xe） | JoinChannelButton | 加入频道 |
| 15（0xf） | BackButton | 返回 |

其它要点：调 `FUN_6f2c07c0()` 取 Battle.net 客户端对象（与登录屏同一入口，推断）；按 `+0x3c8` 标志调 `FUN_6f2c1b40(1)` / `FUN_6f2c1b40(0)` 决定标准/组队按钮是否可用（推断为是否有可用游戏类型）；`+0x3a0` 标志在 0/1 间切换当前面板（`+0x1f4`/`+0x358` 两个面板指针，推断为聊天面板与游戏列表面板）；结尾调 `FUN_6f30eda0(param_1, 0)`（未验证）。

## 6. 好友与聊天

- 事件：`FriendsStart/Stop/Add/Remove/Position/Update`（0x4e–0x53）、`ReadUserData`（0x54）、`EnterChat`（0x58）、`ChannelList`（0x59）、`ChatEvent`（0x5a）。
- 好友列表项渲染 `FUN_6f29f090`（673 字节）与 `FUN_6f29ed80`（766 字节）：拼 `|Cfffed312` 颜色码 + 名字 + 状态文本；好友记录字段（**推断**，仅从反编译读数位置得出）：`+0x79` 状态字节（取 2–5 四档）、`+0x7a` 互为好友标志、`+0x80` 状态码、`+0x84` / `+0x90` 附加字段；时间格式化走 `FUN_6f2a2540(0x3e428f5c, …)`（0x3e428f5c 是 float 常量 0.19…，用途未验证）。
- 状态文本（`BNET_FRIEND_*`，104 个 `BNET_` 字符串中的一部分）：`BNET_FRIEND_OFFLINE` / `AFK` / `DND` / `ONLINE` / `MUTUAL_FRIEND` / `IN_CHANNEL`（`_FMT` 带频道名）/ `IN_PUBLIC_GAME` / `IN_PRIVATE_GAME` / `IN_PASSWORD_GAME`（各有 `_FMT` 变体）。
- UI 类（源码路径泄露）：`Glue/CBattleNetFriendsListBox.cpp`、`Glue/CBattleNetUserListBox.cpp`、`Glue/CBattleNetUserListBoxItem.cpp`、`Glue/CBattleNetStatusBox.cpp`、`Glue/CBattleNetIconSelectBox.cpp`；Frame 名 `BattleNetUserListName`、`BattleNetUserListLevel`、`BattleNetChatActionMenu`；图标 `.blp`：`friends-offline/online/chatroom/publicgame/privategame/mutual/nonmutual`、`chaticons/iconselection-border-*`。
- 聊天图标选择：`BNET_ICON_TIP1` → `FUN_6f29f350`（259 字节）。

## 7. 战队（Clan）

- 事件：`ClanProfile`（0x56）、`ClanGetPotentials`（0x6c）、`ClanCreate`（0x6d）、`ClanFounder`（0x6e）、`ClanDisband`（0x6f）、`ClanDesignate`（0x70）、`ClanAdded`（0x71）、`ClanRemoved`（0x72）、`ClanMemberAdd/Remove/Invite/Rank`（0x73–0x76）、`ClanInfoGet`（0x77）、`ClanListStart/Stop/Remove/Update`（0x78–0x7b）、`ClanChangeRank`（0x7c）；匿名档案 `AnonProfile` / `AnonProfileClan`（0x67/0x68）。
- 战队等级格式化：`CLAN_RANK_%d` → `FUN_6f291a90`（113 字节）与 `FUN_6f2c99e0`（304 字节，2 个被调函数引用，含两次格式化调用）。
- UI：`Glue/CBattleNetClanMateListBox.cpp`、`ClanMateScrollBar`；种族战队等级图标 `.blp`：`clan-orc-initiate/member/officer/leader`（仅兽族图标串被扫到，其余种族未逐一核对）。
- `clickedclan` 字符串（0x6f9516d8）仅 1 处出现，无代码引用——推断为 `.fdf` 界面动作名，未验证。

## 8. 天梯、档案与锦标赛

- 事件：`Profile`（0x55）、`TournamentInfo`（0x7d）、`ScheduledGame`（0x7e）、`MatchmakingInfo`（0x7f）、`IconList`（0x80）、`TeamGetPotentials`（0x62）。
- **天梯/档案屏** `FUN_6f28f170`（1342 字节，14 个被调函数，0 个静态调用者）：按名查找 `LadderPanel`、`LadderLabel`、`ProfileLadderButton`、`LadderRankValue`、`LadderLevelLabel` / `LadderLevelBar` / `LadderLevelMinusOneMarker` / `LadderLevelMarker` / `LadderPlusOneMarker`、`LadderWinsValue`、`LadderLossesValue`、`PROFILE_LADDER`、`ProfileTeamLadderButton`、`TeamLadderLevel*` 等 Frame，并用 Storm 序号 401（`Ordinal_401(1000, &DAT_6f952094, …)`，S1 已验证为配置读取类调用，推断此处读天梯配置）取值。UI 类：`Glue/CBattleNetProfileListBox.cpp`。
- 天梯网页链接（字符串证据）：`http://www.battle.net/war3/ladder/war3-ladder-info-clans.aspx`——战队天梯信息页由外部浏览器打开（推断）。
- **锦标赛屏** `FUN_6f29d8c0`（3280 字节，18 个被调函数，1 个调用者）：按 `param_2` 的锦标赛状态分发（case 0–7 及以上，数值含义未验证），用 `BNET_TOURNAMENT_*` 格式串拼标题/信息/时钟：`TITLE_UPCOMING` / `SPONSOR` / `SINFO1/2_UPCOMING` / `TITLE_ENTRY` / `TITLE_INFO_ENTRY` / `CLOCK_ENTRY` / `TITLE_PRELIM` / `TITLE_PRELIMEND` / `TITLE_NOFINALS` / `SINFO1_FINALS` / `TITLE_FINALLOST` / `TITLE_FINALWON` / `FINALSROUND` / `FINALWAITING` 等（33 处出现）；Frame 名 `TournamentPane`、`TournamentTitleValue`、`TournamentTitleInfoValue`、`TournamentSmallInfoValue1/2`、`TournamentLargeInfoValue`、`TournamentClockIcon`、`TournamentPlayGameButton`。
- 锦标赛 JASS 侧：native 名 `GetTournamentFinishSoonTimeRemaining`、`GetTournamentFinishNowRule`、`GetTournamentFinishNowPlayer`、`GetTournamentScore`（在 `FUN_6f1e9a50` 注册表内，S4 已验证）；事件数据类型 `CTournamentFinishSoonEventData` / `CTournamentFinishNowEventData`。
- **匹配**：`MatchmakingInfo`（0x7f，构造器 6f2f5a90）、`TeamGetPotentials`（0x62，构造器 6f2f63e0）、`ScheduledGame`（0x7e）；`Matchmaking` 字符串仅 2 处出现（0x6f987cd7、0x6fb8ba4b），均无代码引用。匹配算法、组队潜力计算、预定对局流程**未验证**。

## 9. 新闻与广告（CAdManager）

- 事件：`CheckAd`（0x49）、`QueryAdUrl`（0x4a）、`NewsStart/Stop/Add`（0x4b–0x4d）、`DownloadUpgradeFile`（0x48）。
- `FUN_6f2cf9b0`（463 字节，CAdManager 消息处理，`param_2 + 8` 为消息类型）：0=下载广告（调 `FUN_6f7917c0`，成功置 `+0x2c` 等待标志）；1=广告送达（校验 `+0x18` 会话 id 与 `+0x1c` 错误码，成功显示并调 Storm 401 读配置、`FUN_6f30d990`、`FUN_6f791720`）；2=广告 URL（调 Storm 507 存 URL，置 `+0x30`/`+0x34` 标志）。
- `FUN_6f2ef200`（157 字节）：URL 启动结果上报（`success`/`failure` + 是否上报 Battle.net），日志 `CAdManager: (INFO): Reported %s in launching an ad to Battle.net.`。
- 新闻框：`Glue/CBattleNetNewsBox.cpp`、`BattleNetNewsBox`、`BattleNetNewsBoxScrollBar`、`BNET_NEWS_NEW_ITEM` → `FUN_6f292ed0`（794 字节，新闻条目渲染，0 个静态调用者）与 `FUN_6f2d99f0`（236 字节）。
- 另有 `BattleNetWorkingSmall`（加载中小部件，`FUN_6f28f6b0` 引用，878 字节）与 `BattleNetStatusBox`（`FUN_6f2b7560` 引用）。

## 10. 关键函数地址表

| 地址 | 函数 | 作用 |
|---|---|---|
| 6f307b50 | FUN_6f307b50 | Battle.net 事件分发器（2625 字节，类型 0x41–0x80） |
| 6f3098d0 | FUN_6f3098d0 | 游戏内事件大分发器（大厅事件 0x01–0x31，S1 已验证） |
| 6f30ade0 | FUN_6f30ade0 | 事件泵（S0/S1 已验证） |
| 6f2b9d60 | FUN_6f2b9d60 | 登录屏初始化（3132 字节，TOS/建号/改密/改邮/登录按钮） |
| 6f2b7560 | FUN_6f2b7560 | Battle.net 主菜单初始化（4146 字节，15 个按钮） |
| 6f2b8da0 | FUN_6f2b8da0 | 建房屏初始化（989 字节） |
| 6f2b92c0 | FUN_6f2b92c0 | 建房/加房屏（973 字节，本体未反编译） |
| 6f2bdd90 | FUN_6f2bdd90 | 游戏列表屏（834 字节，本体未反编译） |
| 6f28f170 | FUN_6f28f170 | 天梯/档案屏初始化（1342 字节） |
| 6f29d8c0 | FUN_6f29d8c0 | 锦标赛屏状态机（3280 字节） |
| 6f29f090 / 6f29ed80 | FUN_6f29f090 / FUN_6f29ed80 | 好友列表项渲染（673 / 766 字节） |
| 6f291a90 / 6f2c99e0 | FUN_6f291a90 / FUN_6f2c99e0 | 战队等级格式化（113 / 304 字节） |
| 6f2cf9b0 | FUN_6f2cf9b0 | CAdManager 消息处理（463 字节） |
| 6f2ef200 | FUN_6f2ef200 | 广告 URL 启动上报（157 字节） |
| 6f292ed0 | FUN_6f292ed0 | 新闻条目渲染（794 字节） |
| 6f162ee0 | FUN_6f162ee0 | OsNet 日志口（160 字节） |
| 6f3107d0 / 6f310870 | — | GameCreate / GameJoin 事件回调（本体未验证） |
| 6f310ac0 / 6f310380 | — | PlayerJoin / PlayerLeave 事件回调（本体未验证） |
| 6f310940 / 6f310990 / 6f310900 / 6f310b40 | — | GameSetup / GameStart / GameReady / SetTurnsLatency 回调（本体未验证） |

## 11. 数据结构与常量

- `CONNSTATE_*` 状态名表：0x6fa5c7f0 起 13 项 + `Unknown CONNSTATE`（0x6fa5c948）。
- `battlenet_V0` / `battlenet_V1`：0x6f951688 / 0x6f951698（版本化资源名前缀，推断）。
- Storm.dll 注册表路径：`Software\Battle.net\`（网关配置来源，推断）。
- 天梯信息页 URL：`http://www.battle.net/war3/ladder/war3-ladder-info-clans.aspx`。
- 好友状态文本族 `BNET_FRIEND_*`（见 §6）；战队等级格式 `CLAN_RANK_%d`；锦标赛格式串族 `BNET_TOURNAMENT_*`（33 处）。
- 事件类型空间：游戏内 0x01–0x31、大厅/列表 0x0b–0x1d、Battle.net 0x41–0x80（0x43、0x5c–0x61、0x63–0x66、0x69–0x6a 无处理分支）。

## 12. 与其它阶段的关系

- S0 帧同步：大厅事件（GameStart 等）是进入锁步对局的入口；`GameStart` 回调 `FUN_6f310990` 是同步开始点（推断）。
- S1 网络层：本阶段的两个分发器与事件泵、路由器（`FUN_6f283540` 建连）都在 S1；Battle.net 事件与游戏内事件共用泵与对象模型。
- S2 主循环：Glue 界面运行在主循环的 UI 分支里；登录/主菜单是游戏启动后的第一个状态机分支（S2 状态机 `FUN_6f312740` 的 0–6 迁移表，推断）。
- S4 JASS2：锦标赛 native（`GetTournament*`）是脚本侧读取锦标赛状态的接口。
- S6 UI：全部 Glue 屏都复用 Frame 查找（`FUN_6f09ef40`）、按钮绑定（`FUN_6f2c3590` / `FUN_6f2c3460`）、事件注册（`FUN_6f0b0130` / `FUN_6f0b0170`）模式。
- S7 Storm 文件层：网关配置在注册表；BNCS 传输疑似走 Storm SNet 序号函数（S1 未验证点，本阶段仍未验证）；广告/升级文件下载走 Storm 文件接口（推断）。
- S9 录像：`GameStart`（0x1b）同时启动录像写入；大厅阶段决定录像是否开启。

## 13. 未验证点

1. CONNSTATE 状态名表的读取函数与状态机驱动函数（无静态引用）；各状态转移条件与 BNCS 握手包格式。
2. 网关主机名/端口、BNCS 协议细节（SRP 登录证明交换的具体实现）；Game.dll 内无网关域名字符串。
3. Storm SNet 序号函数中哪些承载 Battle.net 流量（S1 遗留：251–253、263–272、401–428、460–482、524–553 段）。
4. 大厅事件回调本体：`FUN_6f3107d0`（GameCreate）、`FUN_6f310870`（GameJoin）、`FUN_6f310ac0` / `FUN_6f310380`（PlayerJoin/Leave）、`FUN_6f310940` / `FUN_6f310990` / `FUN_6f310900`（GameSetup/Start/Ready）、`FUN_6f310b40`（SetTurnsLatency）。
5. `FUN_6f2b92c0`（建房/加房屏）与 `FUN_6f2bdd90`（游戏列表屏）本体。
6. 好友记录字段语义（`+0x79` 状态字节 2–5 各档含义、`+0x80` 状态码、`+0x84` / `+0x90`）；`FUN_6f2a2540` 的时间格式化常量 `0x3e428f5c`。
7. 锦标赛屏 `FUN_6f29d8c0` 的状态数值含义（case 0–7）；`FUN_6f2c2040` / `FUN_6f2c0ee0` 的身份。
8. 匹配算法：`MatchmakingInfo` / `TeamGetPotentials` / `ScheduledGame` 的内部流程；`AnonGameFind` / `AnonGameJoin` 的匿名匹配流程。
9. 登录屏 `FUN_6f023e60(0x40, buf, 0x1000)` 读取的 4KB 数据来源与内容；`FUN_6f2c07c0` 的身份；主菜单结尾 `FUN_6f30eda0` 的作用。
10. `clickedclan` / `clickedladder` / `battlenet_V0` / `battlenet_V1` 的确切用途（均无代码引用）。
11. 特殊分支 0x6b 的数据负载格式与 `FUN_6f2ffe50(0x400900e2)` 的含义。
