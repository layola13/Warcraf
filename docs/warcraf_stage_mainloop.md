# S2 — 主循环与入口 GameMain

- 状态：已验证 2026-11-05
- 证据：`~/workspace/warcraf_ghidra/mainloop_dump.txt`（GameMain/DllMain/初始化链反编译、字符串与 Win32 导入 xref）、`mainloop_dump2.txt`（帧步进 `FUN_6f30ff00`、回合应用 `FUN_6f30bac0`、消息泵 `FUN_6f1532b0`、计时层）、`mainloop_dump3.txt`（引擎线程循环 `FUN_6f05f710`、EvtSched 初始化、`FUN_6f30ff00` 调用点定位）、`mainloop_dump4.txt`（每帧子步骤、EvtSched 等待/注销）、`mainloop_dump5.txt`（事件队列触发 `FUN_6f05ae90`、线程创建包装）；PE 导出表直读（Python struct）
- 上承：S0 帧同步核心、S1 网络层（本阶段的帧步进函数直接消费 S1 的 TurnsSync 存储与事件泵）

## 1. 总述：两层循环

```
war3.exe / 启动器
    │  LoadLibrary Game.dll，调用唯一导出
    ▼
GameMain @ 0x6f0297d0 ──► 安全加固 FUN_6f02bee0（进程拒绝 ACE）
    │                   错误报告 FUN_6f027d40（ClassicErrorReporter）
    │                   初始化 FUN_6f027fb0（单实例检查 → Storm/配置/子系统 → EvtSched）
    ▼
FUN_6f05e710 ──► FUN_6f05f710(1)   "Engine %x" 线程循环（while(true)，引擎主循环）
    │   每帧：定时器队列 FUN_6f0603b0 → 消息泵 FUN_6f059b00/FUN_6f1532b0
    │         → 事件链表 FUN_6f05b080 → 帧差 FUN_6f05fc10 → 虚调用 (**obj)(1)（FUN_6f05fb10）
    │         → 等待/调度 FUN_6f05ee90
    ▼
FUN_6f30ff00  帧步进（__thiscall，CNetData 风格对象）：25 ms 固定步长消费时间累加器，
              每步调用 FUN_6f310f20 → 事件泵 FUN_6f30ade0 + 回合应用 FUN_6f30bac0，
              再投递 CNetEventGameTick / IdleTick / AsyncIdleTick
    │
    ▼
FUN_6f3098d0 大分发器（S0）/ FUN_6f30bac0 回合命令执行（命令 0x10–0x31）
```

关键事实：`GameMain` 本身只有 155 字节、13 个直接被调函数，是个薄壳；真正的启动在 `FUN_6f027fb0`，真正的循环在 `FUN_6f05f710`（564 字节，`while(true)`），真正的“游戏帧步进”在 `FUN_6f30ff00`（1148 字节，25 ms 固定步长）。三者之间由 EvtSched 事件调度器与虚函数调用衔接，而不是一条直观的 `for(;;) Step()` 链。

## 2. 入口：GameMain 与 DllMain

- Game.dll 导出表：**只有 1 个导出**，`GameMain`，RVA `0x297d0`，VA `0x6f0297d0`；ImageBase `0x6f000000`。DLL 入口（AddressOfEntryPoint）RVA `0x77190a`，反编译只是 `___security_init_cookie` + `___DllMainCRTStartup`，无游戏逻辑。
- `GameMain`（155 字节，栈校验 cookie `DAT_6fb6a774`）调用顺序（反编译确认）：
  1. `FUN_6f02bee0()` —— 进程安全加固：`LoadLibraryA("advapi32.dll")`，动态取 `AllocateAndInitializeSid` / `InitializeAcl` / `AddAccessDeniedAce` / `SetSecurityInfo`，给当前进程（`GetCurrentProcess()`）加一条拒绝 ACE（`0xf01ffffe`，SID 全 0 参数组）。用途推断：阻止外部进程打开游戏进程句柄（反调试/反注入类加固），未验证具体意图。
  2. `GetTickCount()`（经内部包装 `FUN_6f154d30`，见 §5）。
  3. `FUN_6f154940()`、`FUN_6f154630()`、`FUN_6f153f60()` —— 小初始化；其中 `FUN_6f154630` 是 **8 槽回调注册表**：全局 `DAT_6fbdab98`/`DAT_6fbdab9c` 各 8 项，存（key，回调）对；`FUN_6f153e30` 按 key 注销。注册了什么回调**未验证**。
  4. `Ordinal_503(buf, &DAT_6f951e28, 0x104)` + `Ordinal_585(buf)` —— S1 已验证 `Ordinal_503` 是有界字符串拷贝；源字符串 `DAT_6f951e28` 的内容**未验证**（疑似命令行/路径，推断）。
  5. `FUN_6f027d40()` —— 错误报告：`FUN_6f019ee0("Warcraft III","Warcraft III",0,"Windows",".ClassicErrorReporter")`，`SErrSetBlizzardErrorFunction(FUN_6f01a310)`。
  6. `iVar1 = FUN_6f027fb0()` —— 初始化（§3）。成功返回非 0 时：
     - `thunk_FUN_6f05e710()` → `FUN_6f05e710()` → `FUN_6f05f710(1)`，进入引擎循环（§4），循环返回后才继续；
     - `FUN_6f027660()` —— 关机清理（见下）；
     - `FUN_6f153e30()` —— 从 8 槽表注销。
  7. `FUN_6f771125()` 收尾，返回。
- `FUN_6f027660`（155 字节）是**关机**不是循环：依次调 `FUN_6f023e00`、`FUN_6f023c60`、`thunk_FUN_6f175c70`、`Ordinal_496(0,8)`、`FUN_6f02f210`、`FUN_6f1851e0`、`FUN_6f18aef0`、`FUN_6f17bc60`、`FUN_6f17f850`、`FUN_6f057220`、`FUN_6f04f090`、`FUN_6f020cd0`；若 `DAT_6fbb80cc != 0` 则 `Ordinal_403(..., "…AUCachedDisplayMode…", -2, 0)` 打日志；清零 `DAT_6fbb80c4/c8/cc/d8`；`FUN_6f0e3100()`；`FUN_6f154fb0()`（20 ms 等待，§5）；`Ordinal_470(FUN_6f029090)`（注销 init 时 `Ordinal_469` 注册的回调）。

## 3. 初始化链 FUN_6f027fb0

292 字节，31 个被调函数；失败任一环节 → `FUN_6f0277c0` 报错框 + 返回 0。已验证的调用顺序与含义：

| 步骤 | 函数 | 已验证内容 |
|---|---|---|
| 单实例检查 | `FUN_6f155890()` | `CreateEventA(NULL,1,0,name)`，若 `GetLastError()==0xb7`（已存在）返回真。事件名在本次反编译中未显现，**未验证**（同文件另有 `FUN_6f02b490` 打开并 `SetEvent` 名为 `"WARCRAFT_III_OK"` 的事件，疑似配套，推断） |
| 已在运行 | `FUN_6f02b490()` + `FUN_6f0277c0(param)` | 前者 `OpenEventA(2,1,"WARCRAFT_III_OK")` 并 `SetEvent`；后者读 `ui\startupstrings.txt`（`Ordinal_279` 打开、`Ordinal_504` 逐行取），取第 param 行做消息框（`FUN_6f14d670`/`FUN_6f14d980`），默认文本含 `"Warcraft III was unable to initialize…"`（字符串 `0x6f951dac`） |
| 版本日志 | `Ordinal_565("Warcraft III (build 52240)")`、`Ordinal_460(0xcc10)` | 构建号 52240 与 S0/S1 一致 |
| 回调注册 | `Ordinal_469(FUN_6f029090)`、`Ordinal_568(FUN_6f0291f0)` | Storm 回调注册（S1 序号簇），具体回调身份**未验证** |
| 时间初始化 | `FUN_6f155010()` | 见 §5：`"W32\\Time.cpp"` 0x273 行、`"Internal"/"CpuTicksPerSecond"` 配置读取、`Sleep(0)` |
| 后续子系统 | `FUN_6f155830`、`FUN_6f0475d0`、`FUN_6f027dd0`、`FUN_6f023e00`、`FUN_6f024bf0(0x50)`、`FUN_6f020e20`、`FUN_6f029470`、`FUN_6f027e20`、`thunk_FUN_6f175c80`、`FUN_6f0272a0` | 逐个把关，任一失败即走报错分支；各自身份**未验证**（只知调用顺序与失败语义） |
| 引擎子系统 | `FUN_6f04f0d0`、`FUN_6f052b40`、`FUN_6f048b20`、`FUN_6f048da0`、`FUN_6f049720`（×2）、`FUN_6f057390`、`FUN_6f057210(&LAB_6f027700,1,3)`、`thunk_FUN_6f059ba0`、`FUN_6f02fbe0`、`FUN_6f02b490`、`FUN_6f028b50` | 其中 `FUN_6f057390` 内调 `FUN_6f05e410`（EvtSched 初始化，§6）；其余身份**未验证** |

## 4. 引擎线程循环 FUN_6f05f710

564 字节，32 个被调函数；`FUN_6f05e710`（28 字节）是薄壳：`FUN_6f1587b0(); FUN_6f05f710(1); DAT_6fbb8978 = 0;`。`FUN_6f05f710` 的另一调用点在 `FUN_6f05e410`（EvtSched 初始化，`0x6f05e61b` 处），传入参数**未验证**。

循环结构（反编译确认，`while(true)`）：

1. 线程命名：`Ordinal_578(buf, 0x40, "Engine %x", GetCurrentThreadId())`；`FUN_6f159b00()` 把线程登记到 TLS 链表（`TlsAlloc`、`DuplicateHandle`、`"%s [%u]"`，源文件 `"W32\\OsCall.cpp"` 0xe5 行）——即这是**引擎线程**的登记点。
2. 每轮先 `FUN_6f155300()` 与全局 `DAT_6fbb8974` 比较，不等则 `FUN_6f155840()`（身份未验证）；`FUN_6f158940(0)` 返回 0 则跳出循环（退出条件，语义未验证）。
3. 取定时对象 `FUN_6f05de80()`（身份未验证）：若存在，算等待毫秒 `iVar2 = *(obj+0x3c) - GetTickCount()`（负数钳 0）。
4. 等待：若 `DAT_6fbb8964 == 0`，`FUN_6f05dee0()` 后 `FUN_6f158940(iVar2)`（带超时的等待，推断）；否则 `FUN_6f1648a0()`，`iVar2 = 0x102`（特殊值，含义未验证）。
5. 若定时对象存在（`iVar3 != 0`）：`FUN_6f04f000()` / `FUN_6f04f010()` / `FUN_6f159dd0()`，写 `*(obj+0x28) = GetTickCount()`；当 `iVar2 == 0x102` 时执行一帧：
   - `FUN_6f05fca0()` —— 置标志位 `+0x44 & 1`，并 `*(obj+0x40) = *(obj+0x28)`，触发 `FUN_6f05ae90(0)`；
   - `FUN_6f0603b0()` —— **定时器队列分发**：遍历 `+0x1f8/+0x1fc` 处的队列，按 `deadline - GetTickCount()` 判断到期，调用到期项的回调（`puVar1[6]` 或 `puVar1[8]`，带/不带两个参数），到期前 `FUN_6f05f290(0)` 出堆（§6）；
   - 若标志 `+0x44 & 2`：`FUN_6f059b00()` —— **Windows 消息泵**（§4.1）；
   - `FUN_6f05fd10()` —— 若 `+0x2c != 1` 则 `FUN_6f05ae90(0)`；
   - `FUN_6f05b080()` —— **事件链表处理**：遍历 `+0x1b8/+0x1bc` 链表，逐项 `FUN_6f05ae90(entry+4)`，最后 `FUN_6f05bfd0()`；
   - `FUN_6f05fc10()` —— **帧差计算**：`dt = (GetTickCount() - *(obj+0x40)) * _DAT_6f956678`（浮点秒数，系数全局 `0x6f956678`），若 `+0x44 & 2` 则再置位 `& 4`，调 `FUN_6f05ae90(&dt)`；
   - `FUN_6f05fcd0()` —— 若 `+0x2c != 1` 且 `+0x44 & 4`，清位并 `FUN_6f05ae90(0)`。
6. `FUN_6f1577e0()`；`iVar2 = *(obj+0x2c)`；`FUN_6f158440()`；若 `== 1`：`FUN_6f05dce0()`（EvtThread 记账，§6）+ `FUN_6f05fb10()`（§4.2，含虚调用帧回调）；否则按 `+0x40/+0x48/+0x4c` 等字段算等待预算 `uVar5`，`FUN_6f159d70()`，`FUN_6f04f000()`，`FUN_6f05ee90(uVar5 + DVar1, *(obj+0x54))`（EvtSched 等待/重排，§6）。
7. 退出循环后：`FUN_6f05dac0()`（EvtThread 注销，527 字节）+ `FUN_6f1595c0()`，返回。

`FUN_6f05ae90`（493 字节，20 处引用）是**事件队列触发器**：对链表做 unlink/relink（`FUN_6f05c230`、`Ordinal_563` 检查、`FUN_6f05e700`），逐个调用事件对象的虚函数 `(**(uVar2+8))()`，返回非 0 则继续。参数为 0 时只是 `Ordinal_465(0x57)`（空操作/断言，推断）。

### 4.1 Windows 消息泵 FUN_6f059b00 / FUN_6f1532b0

- `FUN_6f059b00`（147 字节）：`param_1==0` 则 `Ordinal_465(0x57)` 返回；否则循环 `FUN_6f1532b0(...)`，每取到一条消息调 `FUN_6f05a310(...)`（334 字节，逐消息处理，身份**未验证**），直至取空。
- `FUN_6f1532b0`（462 字节）：先处理合成输入：若 `DAT_6fbdab1c != 0`，把其中的坐标转为类型 4 的事件返回；否则若内部队列 `DAT_6fbdab7c != DAT_6fbdab78`，`FUN_6f1535a0(...)` 取一条返回；否则 `PeekMessageA` / `GetMessageA` / `TranslateMessage` / `DispatchMessageA` 标准 Win32 循环；无消息且 `DAT_6fb673ec==0` 且 `FUN_6f155870()!=0` 时 `FUN_6f1552e0()` + `FUN_6f155860()`（空闲处理，身份未验证）；`GetMessageA` 返回 0（WM_QUIT）时以类型 `0xe` 返回 1。
- 另有 `FUN_6f148960`（63 字节）：`PeekMessageA` 有消息则 `GetMessageA` + `FUN_6f14e350(&msg)`；调用者**未验证**（无函数内引用）。

### 4.2 帧回调虚调用 FUN_6f05fb10

251 字节。先 `FUN_6f05fca0()`（若标志 `+0x11 & 1` 为 0）；若 `+0x11 & 2` 且 `FUN_6f152ac0()==0` 则 `FUN_6f05e730()`；`FUN_6f05ae90(0)`；`param_1[0xb] = 2`；`FUN_6f05b080()`；再 `FUN_6f05ae90(0)`；最后 **`( **param_1)(1)` —— 对当前引擎对象做虚函数调用，参数 1**。这是引擎循环向游戏帧的派发点（推断）；虚表目标**未验证**。

## 5. 计时层（W32/Time.cpp）

| 函数 | 大小 | 已验证内容 |
|---|---|---|
| `FUN_6f154d30`（Ghidra 命名 `GetTickCount`） | 6 字节 | `GetTickCount` 的内部包装；被 GameMain、`FUN_6f30ff00`、`FUN_6f05f710`、`FUN_6f0603b0`、`FUN_6f05fc10` 等数十处调用 |
| `FUN_6f155010` | 146 字节 | 时间子系统初始化：`Ordinal_401(0x20,"W32\\Time.cpp",0x273,8,...)`（分配/注册），`Ordinal_421("Internal","CpuTicksPerSecond",0,&DAT_6fbdabd8,8,...)` 读配置，`Sleep(0)` |
| `FUN_6f154c30` | 223 字节 | 校准 `CpuTicksPerSecond`：`QueryPerformanceFrequency` + `Sleep(500/50)` 两次 `QueryPerformanceCounter`，`__allmul/__alldiv` 算换算系数，存 `DAT_6fbdabd8/dc` |
| `FUN_6f154d40` | 87 字节 | 懒初始化 QPC 路径：`DAT_6fbdabe8` 首次置 1；`DAT_6fbdabec==0` 时退回 `GetTickCount()` |
| `FUN_6f155130` | 87 字节 | 取时间戳：保线程优先级（`GetThreadPriority`/`SetThreadPriority`），`FUN_6f154d20()`（rdtsc 包装，3 字节）+ `GetTickCount()`，有标志时再 `QueryPerformanceCounter` |
| `FUN_6f154fb0` | 91 字节 | 20 ms（`0x14`）忙等：`GetTickCount()` 差值 `< 0x14` 且 `DAT_6fbdabf4 != 0` 时循环 `Sleep(0)`；被关机 `FUN_6f027660` 调用 |
| `timeGetTime`（WINMM 导入） | — | S1 已确认仅导入 1 个；本次 xref 只见 IAT 引用（`0x6f94e904`，无函数），**代码中实际调用点未验证**；循环计时实际用的是 `GetTickCount`，不是 `timeGetTime` |

## 6. EvtSched 事件调度器（0x6f05xxxx 簇）

源文件线索：`"EvtSched.cpp"`（`FUN_6f05e410` 内 `Ordinal_401(4,"EvtSched.cpp",0x311,0)`）、`"s___AUEvtThread___"`、`"s__PAVCOsThread___"`、`"EvtSched#%d"`。

| 函数 | 大小 | 已验证内容 |
|---|---|---|
| `FUN_6f05e410` | 740 字节 | 初始化：参数取 2 的幂向上取整为线程槽数（`DAT_6fbb8930`），分配 EvtThread 数组（每项 `0x18` 字节，`FUN_6f771ab4` 初始化，回调 `FUN_6f157040`/`FUN_6f1571e0`）；分配每线程指针表 `DAT_6fbb896c`；`DAT_6fbb8978 = FUN_6f05e790()`；逐个以 `"EvtSched#%d"` 为名经 `FUN_6f157530` 创建线程；调用者为 `FUN_6f057390`（init 链内） |
| `FUN_6f157530` | 45 字节 | `_beginthreadex(NULL,0,fn,arg,0,&tid)` 包装，返回句柄 |
| `FUN_6f05e790` | 436 字节 | 取/建一个 EvtThread 结构（`Ordinal_401(0x38,"s___AUEvtThread___",...)`），字段 `+0xd = 0x30`，挂到全局链表 `PTR_LOOP_6fb598ec`，引用计数 `+0xc` |
| `FUN_6f05f710` | 564 字节 | 引擎循环本体（§4）；另在 `FUN_6f05e410` 内被调（`0x6f05e61b`，参数未验证） |
| `FUN_6f05ee90` | 482 字节 | 调度等待/重排：比较 `+0x3c/+0x54/+0x58` 字段，跨线程比较剩余时间做优先级调整（`ppuVar3[4]/[5]/[6]/[7]` 的堆式重排），最后 `FUN_6f05dda0` |
| `FUN_6f05dce0` | 166 字节 | 线程记账：`DAT_6fbb896c[param_1*4]` 的 `+4/-=*(param_2+0x50)`，计数 `+6` 减 1，为 0 时清 `+5`，并遍历全局链表调整其它线程的 `+7` |
| `FUN_6f05dac0` | 527 字节 | 注销：引用计数 `+0xc` 减 1，为 0 时释放（`FUN_6f05f0b0`、`FUN_6f05d100`、`Ordinal_403(...,"s___AUEvtThread___",...)`），并对挂起项逐个 `FUN_6f05fb10()` |
| `FUN_6f05f290` | 287 字节 | 优先队列（堆）删除：按 `**(item+4)` 的虚比较函数做下沉重排 |
| `FUN_6f05f140` | 129 字节 | `COsThread*` 数组扩容（`Ordinal_405` 重分配，失败则 `Ordinal_401` 新分配并拷贝） |
| `FUN_6f159b00` | 341 字节 | 线程登记（TLS，见 §4） |

## 7. 帧步进 FUN_6f30ff00：25 ms 固定步长

1148 字节，`__thiscall`，操作的对象与 S1 的网络会话对象偏移一致（`+0x610` 本玩家槽位、`+0x614` 状态、`+0x278` 游戏状态、`+0x2250` 时间累加器、`+0x2258/+0x22b0` 速度、`+0x628/+0x62c` 回合序号），因此判定为同一会话对象（推断，偏移证据见下）。

执行顺序（反编译确认）：

1. `FUN_6f30ade0(0)` —— 事件泵（S0，954 字节）先跑一遍。
2. 若 `+0x614 == 2` 且 `*(+0x610 槽位 * 0x304 + 0x278 + this) > 3`：`FUN_6f312a40(uVar1)` —— **追赶**：从同步存储读最多 `0x3fd`（1021）个回合（`FUN_6f04d5d0` 读、`FUN_6f8444e0` 校验，返回 0 走 `FUN_6f2fef80`，非 0 走 `FUN_6f2fec70`），`+0xad4` 计数 +1，`+0xacc` 前进；读完走 `FUN_6f2fedf0`。限流条件 `*(this+0xad4) < 5`。
3. 若 `*(this+0x278) < 4`：直接返回 1（尚未进入游戏状态，无步进）。
4. `FUN_6f312490()` —— 若 `*(this+0x2288)` 为 `'POOL'`（`0x4c4f4f50`）或 `'NONE'`（`0x4e4f4e45`）则调 `FUN_6f310f50()`，否则走虚调用 `(**(this+0x1c78)+0x1c)()`；两者身份**未验证**。
5. 时间累加：`local_20 = param_2 - *(this+0x224c)`，`*(this+0x224c) = param_2`（`param_2` 为当前 tick）；若 `*(this+0x2248) != 0` 则清零并令 `local_20 = param_2`。速度因子：若 `+0x610 == 0`（本地/单机分支，推断），按 `+0x2258/+0x225c/+0x284/+0x2250` 算出查表下标（钳 0–6），`*(this+0x22b0) = DAT_6f987f14[uVar2]`（**速度表，表值未读**）；否则 `*(this+0x22b0) = 0`。`*(this+0x2250) += *(this+0x22b4) * iVar3 / *(this+0x22b8)`（按速度缩放），钳到 4000。
6. **固定步长循环**（`while (*(this+0x2250) > 0x18)`，即 24 ms 以上）：
   - 内层：当 `*(this+0x284) < 0x19`（25）时循环 `FUN_6f310f20()`，直至其返回 0 或 `+0x284 >= 0x19`；`FUN_6f310f20` = `FUN_6f30ade0(1)`，若无事件则 `FUN_6f30bac0()`（回合应用，§8），再 `FUN_6f30ade0(1)`。
   - 每消耗 25 ms：`*(this+0x284) -= 0x19`，`*(this+0x2250) -= 0x19`。
   - 若游戏状态 `+0x278` 为 4 或 5：构造 `CNetEventGameTick`（事件码 `0x40090092`，负载 float `DAT_6fbe56e0`），`FUN_6f2877b0`/`FUN_6f2877c0` 取倍数 `iVar4`（身份未验证），`*(this+0x1c6c)=1` 期间按 `iVar4 * 5` 循环投递（`FUN_6f30ada0`，每次 `+0x1c70 += 5`，`+0x22c0 += 5`，超 250 则减 250）；状态为 5 时另调 `FUN_6f312740(6)`（状态迁移，见 §9）。
   - 若状态为 6 且 `+0x610 == 1`：`*(this+0x2250) += 0x19`（把步长还回去，疑似暂停/等待，推断）。
   - 看门狗：若本轮 `GetTickCount()` 距进入时超过 200 ms，`*(this+0x2250) = 0`（丢弃积压，防螺旋死亡，推断）。
7. 收尾事件：若本轮执行了游戏 tick（`param_2 != 0`）：构造 `CNetEventIdleTick`（`0x40090093`），float 参数 `param_2 * _DAT_6f956678`，`FUN_6f30ada0` 投递，期间 `*(this+0x1c6c)=3`。若墙钟流逝 `local_20 != 0`：构造 `CNetEventAsyncIdleTick`（`0x40090094`），float 参数 `local_20 * _DAT_6f956678`，同样投递。
8. `FUN_6f30ada0`（61 字节）：对事件对象调两个虚函数 `(**(obj+4)+0x10)` 与 `(**(obj+0x10)+0x10)`，任一非 0 即返回真——即事件的“投递/处理”双虚调用（推断）。

### 帧步进的调用方（未完全验证）

- 直接代码引用只有一处：一小段未被 Ghidra 定义为函数的代码，约 `0x6f30c8d0` 附近，字节为 `… 50 8b ce e8 1d 36 00 00 5e c3`（`call FUN_6f30ff00; pop esi; ret`），即一个薄包装。Ghidra 中最近的已定义函数是 `FUN_6f30c800`（116 字节，实为向量扩容辅助，与帧步进无关），该调用不在其体内。该包装的准确起始地址与调用者**未验证**。
- `FUN_6f05fb10` 的虚调用 `(**param_1)(1)` 是候选派发点（推断），虚表目标**未验证**。
- 因此“引擎循环 → 帧步进”的最后一跳尚未坐实，只确认两端都存在且 `FUN_6f30ff00` 的调用约定与会话对象一致。

## 8. 回合应用 FUN_6f30bac0（帧步进内层）

1530 字节，37 个被调函数；调用者：大分发器 `FUN_6f3098d0`（`0x6f3099fa`）、`FUN_6f310f20`（`0x6f310f37`）、`FUN_6f310b70`（`0x6f310c4f`）。

- 仅当 `*(this+0x614)` 为 2 或 3 时执行；读写同步存储的回合游标 `+0x628`（当前）/`+0x62c`（已应用）与备份 `+0xacc`；若 `+0x58c != 0` 则 `+0x628 = +0xacc`。
- 循环 `FUN_6f04d3b0(&cmd)` 取命令字节，按命令分发（每种先调 `FUN_6f83exxx` 读参数，再调 `FUN_6f2fxxxx(cmd, buf, 0)` 执行，185 字节一族）：
  - `0x10` → `FUN_6f2fdbd0`；`0x16` → `FUN_6f2fdf90`；`0x17` → `FUN_6f2fe050`；`0x19` → `FUN_6f2fdd50`；`0x1a` → `FUN_6f2fdb10`；`0x1b` → `FUN_6f2fde10`；`0x1c` → `FUN_6f2fdc90`；`0x20` → `FUN_6f2fded0`；`0x21` → `FUN_6f2fe110`；`0x22` → `FUN_6f2fe410`；`0x23` → `FUN_6f2fe4d0`（失败时 `Ordinal_403(..., "%E…", -2, 0)`）；`0x2f` → `FUN_6f2fe1d0`；`0x30` → `FUN_6f2fe290`；`0x31` → `FUN_6f2fe350`；`0x1e/0x1f` 为批量路径（`FUN_6f04d420` 取长度，`FUN_6f83f8e0` + `FUN_6f04d5d0` 读数据，`FUN_6f30b940` 执行，`0x1f` 置结束标志）；其它 → `FUN_6f30e9c0(*(this+0x610))`（默认处理，身份未验证）。
  - 各命令执行函数的具体语义（移动/攻击/施法等）**未验证**，只确认分发表与调用形态；与 S0 的事件类型表（0x01–0x31）同属一族，命令号与事件号的对应关系**未验证**。
- 每轮末若 `*(this+0xbe4) == 1` 则 `FUN_6f78b670()`；游标回写 `+0x62c = +0xacc`。

## 9. 游戏状态机 FUN_6f312740

133 字节，`__thiscall FUN_6f312740(this, newState)`，状态字段 `+0x270`，取值 0–6。允许的迁移（反编译确认）：

| 新状态 | 允许的旧状态 |
|---|---|
| 0 | 任意（`param_2 == iVar1` 时直接通过） |
| 1、2 | 旧状态为 0 |
| 3 | 旧状态为 1 |
| 4 | 旧状态为 3 或 5 |
| 5 | 旧状态为 4 或 6 |
| 6 | 旧状态为 5 |

非法迁移：`FUN_6f30c390()` 两次取状态名字符串，`FUN_6f3129f0("CNetData::CNetSession::SetGameState (from %s to %s)", ...)` 打日志，返回 0。状态 0–6 各自的含义（菜单/大厅/加载/游戏中/暂停…）**未验证**，只确认迁移表。`FUN_6f30ff00` 中状态 5 时调 `FUN_6f312740(6)`，即 5→6 是合法迁移。

## 10. 关键函数地址表

| 地址 | 大小 | 角色 |
|---|---|---|
| `0x6f0297d0` | 155 | `GameMain`（唯一导出） |
| `0x6f77190a` | 35 | DLL 入口（CRT 启动，无游戏逻辑） |
| `0x6f02bee0` | 347 | 进程安全加固（advapi32 拒绝 ACE） |
| `0x6f027d40` | 47 | 错误报告注册（ClassicErrorReporter） |
| `0x6f027fb0` | 292 | 初始化链（单实例→Storm→子系统→EvtSched） |
| `0x6f155890` | 51 | 单实例检查（CreateEventA + 0xb7） |
| `0x6f02b490` | 38 | 发 `WARCRAFT_III_OK` 事件 |
| `0x6f0277c0` | 272 | 初始化失败报错框（`ui\startupstrings.txt`） |
| `0x6f027660` | 155 | 关机清理 |
| `0x6f05e710` | 28 | 引擎循环薄壳 → `FUN_6f05f710(1)` |
| `0x6f05f710` | 564 | **引擎线程循环 `while(true)`**（"Engine %x"） |
| `0x6f05e410` | 740 | EvtSched 初始化（`EvtSched#%d` 线程） |
| `0x6f157530` | 45 | `_beginthreadex` 包装 |
| `0x6f05e790` | 436 | EvtThread 结构取/建 |
| `0x6f05ee90` | 482 | EvtSched 等待/重排 |
| `0x6f05dce0` | 166 | EvtThread 记账 |
| `0x6f05dac0` | 527 | EvtThread 注销 |
| `0x6f05f290` | 287 | 优先队列（堆）删除 |
| `0x6f05ae90` | 493 | 事件队列触发（虚回调 `(**(uVar2+8))()`） |
| `0x6f0603b0` | 326 | 定时器队列分发（按 GetTickCount 到期） |
| `0x6f05b080` | 349 | 事件链表处理 |
| `0x6f05fb10` | 251 | 帧回调（含虚调用 `(**param_1)(1)`） |
| `0x6f05fc10` | 136 | 帧差 dt 计算（`× _DAT_6f956678`） |
| `0x6f059b00` | 147 | 消息泵驱动（循环取消息 → `FUN_6f05a310`） |
| `0x6f1532b0` | 462 | Win32 消息泵（Peek/Get/Translate/DispatchMessageA） |
| `0x6f148960` | 63 | 备用消息抽取（调用者未验证） |
| `0x6f30ff00` | 1148 | **帧步进：25 ms 固定步长** |
| `0x6f310f20` | 47 | 步进内层：泵 + 回合应用 |
| `0x6f30bac0` | 1530 | 回合命令应用（命令 0x10–0x31） |
| `0x6f312a40` | 445 | 回合追赶（最多 1021 回合） |
| `0x6f312740` | 133 | 游戏状态机 `SetGameState`（0–6） |
| `0x6f312490` | 47 | `POOL`/`NONE` 分支 → `FUN_6f310f50` 或虚调用 |
| `0x6f30ada0` | 61 | 事件投递双虚调用 |
| `0x6f154d30` | 6 | `GetTickCount` 内部包装 |
| `0x6f154fb0` | 91 | 20 ms 忙等（Sleep(0)） |
| `0x6f154c30` | 223 | `CpuTicksPerSecond` 校准（QPC） |
| `0x6f155130` | 87 | 时间戳（rdtsc + GetTickCount + QPC） |
| `0x6f155010` | 146 | 时间子系统初始化（`W32\Time.cpp`） |
| `0x6f154d40` | 87 | QPC 路径懒初始化 |
| `0x6f30ade0` | 954 | 事件泵（S0 已验证） |
| `0x6f3098d0` | 5074 | 大分发器（S0 已验证） |
| `0x6f30c8d0`（约） | ~20 | `FUN_6f30ff00` 薄包装（未定义函数，起始地址未验证） |

## 11. 数据结构 / 关键偏移

**会话对象**（`FUN_6f30ff00` / `FUN_6f30bac0` / `FUN_6f312740` 的 `this`，与 S1 网络对象一致，推断为同一对象）：

| 偏移 | 含义（反编译确认） |
|---|---|
| `+0x270` | 游戏状态 0–6（`SetGameState`） |
| `+0x278` | 游戏状态（帧步进门槛 `< 4` 返回；4/5 发 GameTick） |
| `+0x284` | 回合内毫秒余量（与 25 比较） |
| `+0x610` | 本玩家槽位 |
| `+0x614` | 网络模式（2/3 时应用回合） |
| `+0x628` / `+0x62c` | 回合游标：当前 / 已应用 |
| `+0xacc` | 回合游标备份 |
| `+0xad4` | 追赶限流计数（< 5） |
| `+0xbe4` | 标志（==1 时每轮调 `FUN_6f78b670`） |
| `+0x1c6c` | 事件投递阶段（1=GameTick，3=IdleTick） |
| `+0x1c70` | GameTick 累计（步进 5） |
| `+0x2248` / `+0x224c` | 时间重置标志 / 上一 tick |
| `+0x2250` | 时间累加器（毫秒，钳 4000） |
| `+0x2258` / `+0x225c` / `+0x22b0` / `+0x22b4` / `+0x22b8` | 速度计算输入 / 速度因子（查表 `DAT_6f987f14`，下标 0–6）/ 缩放分子分母 |
| `+0x22bc` | 标志（==0 时才累加时间） |
| `+0x22c0` | GameTick 计数（步进 5，超 250 回绕） |
| `+0x2288` | 魔数 `'POOL'` / `'NONE'` |

**引擎循环对象**（`FUN_6f05f710` 的定时对象）：`+0x28` 上次 tick、`+0x2c` 模式（==1 时走 `FUN_6f05dce0`+`FUN_6f05fb10`）、`+0x3c` 截止时间、`+0x40` 帧起始 tick、`+0x44` 标志位（bit1=已开始、bit2=消息泵开、bit4=帧差待发）、`+0x48/+0x4c` 等待预算、`+0x50/+0x54/+0x58` 调度参数、`+0x1f8/+0x1fc` 定时器队列、`+0x1b8/+0x1bc` 事件链表。对象身份**未验证**。

**帧事件**：`CNetEventGameTick` 码 `0x40090092`、`CNetEventIdleTick` 码 `0x40090093`、`CNetEventAsyncIdleTick` 码 `0x40090094`；负载含一个 float 秒数（`tick数 × _DAT_6f956678`）。

**时间常量**：固定步长 25 ms（`0x19`）；累加器上限 4000 ms；看门狗 200 ms；忙等 20 ms；速度表下标钳 0–6；追赶上限 1021 回合、每帧最多 5 次追赶调用。

## 12. 与其它阶段的关系

- **S0 帧同步**：帧步进直接调用 S0 的事件泵 `FUN_6f30ade0` 与大分发器调用的回合应用 `FUN_6f30bac0`；`FUN_6f30bac0` 的命令表（0x10–0x31）与 S0 的事件类型表同源，回合游标 `+0x628/+0x62c` 就是 S0 的 TurnsSync 游标。
- **S1 网络层**：`FUN_6f312a40` 的追赶读的是 S1 的同步存储；`Ordinal_469/568` 的 Storm 回调注册发生在 init 链；`timeGetTime` 的实际调用点未验证，循环计时以 `GetTickCount` 为主。
- **S3 模拟核心**：`FUN_6f30bac0` 分发的 `FUN_6f2fxxxx` 命令执行函数（185 字节一族）与 `FUN_6f05fb10` 的虚调用 `(**param_1)(1)` 很可能是模拟步进的入口，留待 S3 深挖。
- **S5 渲染**：本阶段未触及渲染调用；`FUN_6f05f710` 的每帧序列里没有已确认的渲染函数，渲染的挂接点**未验证**（可能在虚调用或 `FUN_6f05ae90` 触发的事件回调里）。
- **S6 UI**：消息泵 `FUN_6f1532b0` 取到的 Win32 消息经 `FUN_6f05a310` 处理，身份未验证，留待 S6。
- **S11 随机数与确定性**：25 ms 固定步长 + 看门狗丢弃积压 + 速度表，是确定性步进的节拍基础；`FUN_6f30bac0` 的命令应用顺序即确定性来源，留待 S11。

## 13. 未验证点

1. 引擎循环 → 帧步进的最后一跳：`FUN_6f30ff00` 的唯一代码调用方是 `0x6f30c8d0` 附近未定义的小包装，`FUN_6f05fb10` 的虚调用目标也未确认；两者关系未验证。
2. `FUN_6f05f710` 运行在哪条线程：GameMain 经 `FUN_6f05e710` 直接调用（可能主线程即引擎线程，推断），`FUN_6f05e410` 内另有一处调用（参数未验证）；`EvtSched#%d` 线程与 "Engine %x" 线程的关系未验证。
3. 初始化链中 `FUN_6f155830`、`FUN_6f0475d0`、`FUN_6f027dd0`、`FUN_6f023e00`、`FUN_6f024bf0`、`FUN_6f020e20`、`FUN_6f029470`、`FUN_6f027e20`、`FUN_6f0272a0`、`FUN_6f04f0d0` 等各自对应的子系统身份未验证。
4. `FUN_6f05a310`（逐消息处理）、`FUN_6f05de80`、`FUN_6f05dee0`、`FUN_6f158940`、`FUN_6f159dd0`、`FUN_6f05f290` 的堆比较函数等未反编译。
5. 速度表 `DAT_6f987f14` 的 7 个表值未读；`FUN_6f2877b0`/`FUN_6f2877c0`（GameTick 倍数）身份未验证。
6. `FUN_6f30bac0` 各命令执行函数（`FUN_6f2fxxxx` 一族）的具体语义未验证；命令号与 S0 事件号的对应关系未验证。
7. `FUN_6f312490` 的 `POOL`/`NONE` 分支与 `FUN_6f310f50`、虚调用目标未验证；`FUN_6f30ff00` 中 `+0x2288` 魔数的含义未验证。
8. GameMain 中 `DAT_6f951e28` 字符串内容、8 槽回调注册表（`FUN_6f154630`）注册的回调身份、单实例事件名未验证。
9. `timeGetTime`（WINMM）在代码中的实际调用点未验证；`FUN_6f148960` 的调用者未验证。
10. 游戏状态 0–6 各自的含义未验证，只确认了迁移表。
