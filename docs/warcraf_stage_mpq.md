# S7 — Storm 文件层 MPQ（SFile）

- 状态：已验证 2026-11-05
- 证据：`~/workspace/warcraf_ghidra/mpq_storm_dump.txt`（SFile 簇 124 个函数调用图 + 字符串）、`mpq_storm_decomp.txt`（11 个 SFile 具名函数反编译）、`mpq_storm_decomp2.txt`（16 个归档/读取核心函数反编译）、`mpq_game_dump.txt`（26 个 MPQ 序号的 IAT 映射 + 31 条 MPQ 字符串）、`mpq_game_dump2.txt`（26 个 thunk 的真实调用者）、`mpq_game_decomp2.txt`（8 个 Game 侧挂载/补丁函数反编译）、`storm_exports.txt`（Storm.dll 814 个导出全表）、`game_storm_iat.txt`（Game.dll 的 113 个 Storm 导入 IAT 槽位）、`game_imports.txt`（Game.dll 导入表全解析）
- 上承：S0 帧同步、S1 网络层（Storm 导入表与 548/501/403 三序号已验证）、S2 主循环（`FUN_6f0280e0` 启动序列内调序号 272）、S6 UI（`FrameDef.toc`、`.fdf`、`.blp` 均经本层读出）

## 1. 总述

War3 的文件层是 **Game.dll → Storm.dll（SFile）→ Win32 文件 API** 三层：

```
Game.dll  游戏逻辑（挂载归档、读文件、预取、签名校验）
    │  26 个 MPQ 相关 Storm 序号导入（全是按序号，无一具名）
    ▼
Storm.dll  SFile 类 + 序号导出包装器
    │  SFile 对象（5 种类型分发）、ARCHIVEREC 归档记录、全局优先级链表
    │  MPQ 解析：头扫描、哈希表/块表解密、扇区读取、异步预取、签名验证
    ▼
Win32：CreateFileA / ReadFile / SetFilePointer / CreateFileMappingA / advapi32 CryptoAPI
```

关键事实（本轮新验证）：

- **Game.dll 一个具名 SFile 方法都不导入**：Storm 54 个具名导出里 19 个是 `SFile::` 方法（序号 153–195），Game 只导入了其中**零个**；Game 的全部 MPQ 调用都走 **26 个无名序号导出**（151、251–253、263–272、275、279–281、288–289、291、293–295、300、302），这些序号函数是 `SFile::` 方法的薄包装或归档级操作。
- **补丁链是优先级链表**：启动时按顺序挂载 `War3.mpq`、`War3X.mpq`、`War3XLocal.mpq`、`War3Patch.mpq`，再按 `(teen)` 文件存在与否挂载 `t.mpq` / `tx.mpq`；低/中画质档（`War3_low/med.mpq`、`War3X_low/med.mpq`、`War3Patch_low/med.mpq`）不在启动挂载里，而是在状态机函数 `FUN_6f2ce350` 里按画质档挂载（见 §8）。
- **MPQ 头可不在文件开头**：归档打开时以 0x200 步长扫描文件找 `MPQ\x1a` 魔数（见 §5），即归档前可带任意前缀。
- **签名验证是真 CryptoAPI**：序号 251 用 `advapi32.dll` 的 `CryptAcquireContextA`（容器名 `Blizzard_Storm`）+ Storm 自身资源里的 `BLIZZARDKEY`（#256）验证归档内 `(signature)` 文件（见 §7）。
- **文件对象分 5 型**：`SFile::Open` 的实现 `FUN_150251b0` 按解析结果把文件分成 0=普通文件、1=压缩流（`.bz`，zlib inflate）、2=归档内文件、3=指定归档内文件、4=资源/内存文件；`Read` / `SetFilePointer` / `GetFileSize` 都是按 `+0x00` 类型号 switch 分发（见 §4）。

源码目录（残留路径字符串为准）：`D:\BuildServer\3\work-git\warcraft3-repository\Storm\`（`SFile.cpp`、`SFile2.cpp`、`h\stpl.h`）；Storm 内部用带文件名/行号的分配器 `FUN_1502b830` / 释放器 `FUN_1502be40`（如 `"SFile2.cpp", 0x307`）。

## 2. Storm.dll 导出表全貌

- Storm.dll：334 KB，32 位 PE，基址 `0x15000000`，Ghidra 分析 1105 个函数。
- 导出表：**814 个导出，序号基数 101**（即序号 101–914），具名仅 54 个；具名部分除 `SErrSetBlizzardErrorFunction` 等系统函数外，全是 `SFile::` / `SFile` 相关与同步原语（`SCritSect`、`CSRWLock`、`SEvent`、`SSyncObject`、`SInterlocked*`、`SThread`）。
- Game.dll 导入 Storm 共 **113 个**（112 个按序号 + 1 个具名 `SErrSetBlizzardErrorFunction`），其中 MPQ 相关 26 个（§9 表）。

### 2.1 具名 `SFile::` 导出（19 个，序号 153–195，签名取自修饰名）

| 序号 | Storm 地址 | 修饰名（签名） |
|---|---|---|
| 153 | 0x150240d0 | `SFile::Close(SFile*) -> ULONG` |
| 155 | 0x15024150 | `SFile::CreateOverlapped(SOVERLAPPED*) -> void` |
| 156 | 0x15024220 | `SFile::DestroyOverlapped(SOVERLAPPED*) -> void` |
| 157 | 0x150244b0 | `SFile::EnableHash(BOOL) -> void`（18 字节，无被调函数，本体未反编译） |
| 162 | 0x150244d0 | `SFile::FileExists(const char*) -> int` |
| 163 | 0x150246f0 | `SFile::GetActualFileName(SFile*, char*, ULONG) -> int` |
| 164 | 0x15024730 | `SFile::GetBasePath(char*, ULONG) -> int` |
| 165 | 0x15024750 | `SFile::GetFileSize(SFile*, ULONG*) -> ULONG` |
| 170 | 0x15024c70 | `SFile::Load(SArchive*, const char*, void**, ULONG*, ULONG, ULONG, SOVERLAPPED*) -> ULONG` |
| 171 | 0x15024d30 | `SFile::LoadFile(const char*, void**, ULONG*, ULONG, SOVERLAPPED*) -> ULONG` |
| 172 | 0x15025170 | `SFile::Open(const char*, SFile**) -> ULONG`（55 字节薄壳 → `FUN_150251b0`） |
| 173 | 0x15025840 | `SFile::PollOverlapped(SOVERLAPPED*) -> int` |
| 174 | 0x150259d0 | `SFile::Read(SFile*, void*, ULONG, ULONG*, SOVERLAPPED*, _TASYNCPARAMBLOCK*) -> ULONG` |
| 176 | 0x15025d70 | `SFile::ResetOverlapped(SOVERLAPPED*) -> void` |
| 190 | 0x15025d90 | `SFile::SetBasePath(const char*) -> int` |
| 191 | 0x15025ef0 | `SFile::SetFilePointer(SFile*, LONG, LONG*, ULONG) -> ULONG` |
| 192 | 0x15026120 | `SFile::Unload(void*) -> int`（即释放 Storm 分配的内存） |
| 195 | 0x15026140 | `SFile::WaitOverlapped(SOVERLAPPED*) -> void` |

注：经典 Storm MPQ API（`SFileOpenArchive` / `SFileOpenFileEx` 等自由函数名）**在本 Storm.dll 的导出表里不存在**——对应的功能由 §9 的无名序号导出提供。`SFile::Load` 的第一个参数类型是 `SArchive*`，说明 Storm 内部另有 `SArchive` 类（与 `SFile::ARCHIVEREC` 结构体并存，见 §6）。

## 3. MPQ 格式（由 `FUN_15020af0` 反编译验证）

归档头解析在 `FUN_15020af0`（943 字节，序号 251/266/293 的共同下层）：

- **魔数**：头首 dword 必须为 `0x1a51504d`（即 `MPQ\x1a`），且头大小字段（`+4`）须 `> 0x1f`；头共复制 0x20 字节。
- **扇区大小**：`0x200 << (头字节 0x0e & 0x1f)`，即 512 字节 × 2^shift（经典 MPQ 的 sector size shift 在头偏移 0x0E 处）。
- **表偏移/条数**（头内 dword 偏移）：哈希表偏移 `+0x10`、块表偏移 `+0x14`、哈希表条数 `+0x18`、块表条数 `+0x1c`。
- **哈希表**：每条 16 字节（分配与读取均为 `条数 << 4`），读出后调 `FUN_1501f500` 用种子 `"(hash table)"` 解密（种子经 `FUN_1501fa60("(hash table)", 3)` 算出——`FUN_1501fa60` 是哈希函数，**本体未反编译**）。
- **块表**：磁盘上每条 16 字节（读取 `条数 << 4`），内存里展开成 **0x2c（44）字节/条**的结构体（分配 `条数 * 0x2c`，`FUN_1501e7a0` 处理）；随后逐条把文件偏移字段加上归档基址偏移（`param_1[0x4c]`），即块表存的是相对偏移。
- **归档基址**：`FUN_15020af0` 的扫描循环从 `param_1[0x4c] = 0` 起，以 0x200 步长、每次读 0x1000 字节找魔数；若文件剩余不足 0x20 字节仍未找到则失败（错误码 `0x8510006c`）。即 MPQ 头可以不在文件偏移 0 处。
- **表内文件查找**：`FUN_1501f850` 是哈希表查找（被序号 267/268/288/289 与 `FUN_15024540` 共用，**本体未反编译**）；块表条目的标志位参与判断，如 `FUN_15022d20` 里 `(块条目[3] & 0x20000) != 0` 时哈希值要与 `*(local_114 + 0x130)` 异或调整、`(块条目[3] & 0xff00) != 0` 时分配扇区偏移表（具体标志含义**未验证**）。

## 4. `SFile` 对象：五态分发

`SFile::Open` 的实现 `FUN_150251b0`（1656 字节）先调路径解析 `FUN_15024540`（见 §5.2），按解析结果 `local_33c`（0–4）建一个 0x60 字节的 `SFile` 对象（`FUN_150239a0` 构造，类型号写 `+0x00`），再按类型打开：

| 类型号 | 来源 | 打开方式（`FUN_150251b0` 内） |
|---|---|---|
| 0 | 普通磁盘文件 | `fopen(path, "rb")`，`FILE*` 存 `+0x04`，文件名存 `+0x0c` |
| 1 | 压缩流（`.bz` 后缀） | `fopen` 后读 0x18 字节头；若头 dword `local_334 == 0x30305a42`（`"BZ0"`），按头字段填 `+0x14/+0x44/+0x48/+0x4c/+0x50`，`+0x54 = 1`，分配 0x1000 字节缓冲（`+0x18`）与 0x38 字节 zlib 流（`+0x1c`），调 `FUN_15003950(stream, "1.1.3", 0x38)`（zlib inflate 初始化，版本号字符串 `"1.1.3"` 为证）；`.bz` 后缀在存 `+0x0c` 前被截掉 |
| 2 | 归档内文件（全局链表查到） | 调 `FUN_15022820`（序号 266）打开归档，再调 `FUN_15022d20`（序号 268）按文件名取文件记录，存 `+0x3c`；归档记录指针存 `+0x08` |
| 3 | 指定归档内文件 | 调 `FUN_15022d20(archive, name, flags, &file_record)`，文件记录存 `+0x3c`（`puVar3 + 0xf`，即 `+0x3c`） |
| 4 | 资源/内存文件 | 调 `FUN_1503e7c0(name, 0)` 取资源对象存 `+0x40`；若 flags 有 `0x10000`，还调 `FUN_1503e7c0` / `FUN_1503e8b0` 读 0x10 字节头进 `+0x44` |

`SFile` 对象已验证字段（偏移以字节计）：

| 偏移 | 内容 |
|---|---|
| +0x00 | 类型号（0–4，见上表） |
| +0x04 | `FILE*`（类型 0/1） |
| +0x08 | 归档记录指针（类型 2，`SFile::Close` 里经它调序号 252 释放） |
| +0x0c | 文件名字符串（`FUN_1503a810` 分配的拷贝） |
| +0x10 | 原始请求名（类型 1） |
| +0x14/+0x44/+0x48/+0x4c/+0x50 | `.bz` 头字段（类型 1） |
| +0x18 | 0x1000 字节解压缓冲（类型 1） |
| +0x1c | zlib 流对象 0x38 字节（类型 1） |
| +0x20 | inflate 已消费位置（类型 1） |
| +0x24 | `SCritSect`（`Read` / `SetFilePointer` 进出都加锁） |
| +0x3c | 归档内文件记录（类型 2/3） |
| +0x40 | 资源对象（类型 4） |
| +0x54 | 标志（类型 1 置 1；类型 2/3 在 flags 有 `0x10000` 且 `FUN_15023c40` 返回 0 时置 1） |
| +0x58 | overlapped 标志（`Load` 在 `SOVERLAPPED != NULL` 时置 1） |

### 4.1 `SFile::Read`（0x150259d0，323 字节）

- `param_3 == 0`：直接成功（overlapped 时置事件，`*param_4 = 0`）。
- `SOVERLAPPED != NULL`：走异步分支 `FUN_150258e0` 后立即返回 1。
- 同步分支先 `SCritSect::Enter(+0x24)`，按类型号 switch：
  - 0：`fread`，返回 `读到字节数 > 0`；
  - 1：`FUN_150243f0`（解压读取，185 字节，调 `fread` + `FUN_15003970`）；
  - 2/3：`FUN_15023260`（序号 291，扇区读取，见 §5.3）；
  - 4：`FUN_1503e8b0`（内存/资源读取）。

### 4.2 `SFile::SetFilePointer`（0x15025ef0，393 字节）

- 类型 0：`fseek` / `ftell`（标准 FILE 语义，`param_4` = 0/1/2 对应 SEEK_SET/CUR/END）；
- 类型 1：向前 seek 时反复调 `FUN_150243f0` 读弃 0x1000 字节块；向后 seek（`param_2 < +0x20`）时重置 inflate 流（`FUN_150037a0` + `FUN_15003950(..., "1.1.3", 0x38)`，`fseek(FILE, 0x18, 0)` 跳过 `.bz` 头）；
- 类型 2/3：调序号 271（`FUN_150237b0`，230 字节）；
- 类型 4：调 `FUN_1503eab0`。

### 4.3 `SFile::Load` / `LoadFile`（0x15024c70 / 0x15024d30）

`Load(archive, name, &buf, &size, zero_pad, flags, overlapped)` = `FUN_150251b0` 打开 + `GetFileSize` + 按 `flags + 文件大小` 分配 + `Read` 全读 + 失败则 `Close`、成功则可选 `memset` 零填充 `zero_pad` 字节 + 无 overlapped 时 `Close`。`LoadFile` 是 `Load(NULL, ...)` 的薄壳，即“在全局归档链里找”。全局搜索标志取自 `DAT_1504c46c`：bit0 → 1、bit1 → 2、为 0 → 1（该位组合的具体含义**未验证**）。

### 4.4 `SFile::Close`（0x150240d0，127 字节）

依次：`fclose(+0x04)`；调序号 253 释放 `+0x3c` 的文件记录；释放 `+0x40` 资源对象（`FUN_1503e740`）；若 `+0x08`（归档指针）非零，调序号 252 释放其归档记录并 `delete` 该指针；最后 `FUN_15023bc0` + `delete` 自身。返回恒 1。

### 4.5 `SFile::GetFileSize`（0x15024750，198 字节）

按类型号：0 → `_fileno` + `fstat64i32`；1 → 直接返回（`FUN_1503eee8` 只是栈 cookie 检查）；2/3 → 序号 265（`FUN_150225e0`，调 Win32 `GetFileSize`）；4 → `FUN_1503e7b0`。

### 4.6 `SFile::FileExists` / `SetBasePath` / `Unload`

- `FileExists`（0x150244d0）：调路径解析 `FUN_15024540`，返回解析是否成功（即名字在全局文件索引 `DAT_1504c470` 里或在磁盘/归档里存在）。
- `SetBasePath`（0x15025d90）：把路径拷进全局 `DAT_1504c4b8`（0x104 字节），末尾无 `\` 则补一个，再调序号 270（`FUN_150236c0`，179 字节，**本体未反编译**，推断为按新基路径重建归档/文件索引）。
- `Unload`（0x15026120）：`FUN_1502be40(ptr, "SFile2.cpp", 0x434)`，即释放一块 Storm 分配的内存。

## 5. 归档打开与读取核心

### 5.1 归档打开：序号 266 → `FUN_15022820`（616 字节）→ `FUN_15020af0`

`FUN_15022820(path, ?, flags, &ARCHIVEREC*)`：

1. 全局临界区 `DAT_1504c2fc` 加锁；若 `DAT_1504c458 == 0` 则调 `FUN_1501fae0(0x100001)`（**推断**为 Storm 文件子系统一次性初始化）。
2. 路径规范化：`GetFileAttributesA`；若是目录（bit 4），且路径不是绝对路径（无 `:\` 与 `\\`），调 `FUN_1501f2f0` 拼基路径。
3. `FUN_1501f0c0(path)` 判断是否为已挂载归档内的虚拟路径（返回值影响后续分支，**本体未反编译**）。
4. `CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0)`；分配 **0x25c（604）字节的 `SFile::ARCHIVEREC`**（RTTI 名 `".?AUARCHIVEREC@SFile@Storm@@"`，0x1504b5bc）：`+0x08` 起存 0x104 字节路径，`+0x10c` 存文件句柄，`+0x110` 存标志，`+0x118` 存优先级（`param_2`），`+0x124` 存扇区大小、`+0x128` 扇区缓冲、`+0x130` 归档基址、`+0x134` 文件大小、`+0x138` 头指针（0x20 字节）、`+0x13c` 块表、`+0x140` 哈希表、`+0x144`/`+0x148` 引用计数（见 §5.4）、`+0x258`（+600）另一引用计数（序号 151 操作）。
5. 调 `FUN_15020af0(ARCHIVEREC, flags, ...)` 解析头与表（§3），成功后把记录按优先级插入全局链表（见 §5.4），失败则 `FUN_1501e480` 清理并 `delete`。

### 5.2 路径解析：`FUN_15024540`（431 字节）

`SFile::Open` / `FileExists` 的第一步（`FUN_15024540(name, out_path, 0x104, flags, &kind)`）：

1. 绝对路径（以 `\\` 开头或第 2 字符为 `:`）或全局 `DAT_1504c469 != 0` 时走 `FUN_15024e60`（**本体未反编译**）。
2. 否则若全局索引未建（`DAT_1504c468 == 0`），调 `FUN_15023f70` 建索引（见 §5.5）。
3. 在全局文件索引 `DAT_1504c470` 里查：先按 `\` 后的尾名查（`FUN_15025860`），查不到再按全名查；查到则 `out_path = 索引项[6]` 的字符串、`kind = 索引项[7]`（0/1/2/3，对应 §4 的类型号）。
4. 查不到则拼基路径（`DAT_1504c4b8` + `\` + 名字）再查一次；仍查不到则调序号 289（`FUN_150222e0`，归档内查找）确认，成功则 `out_path = 名字`、`kind = 3`；失败返回 0。

### 5.3 扇区读取：序号 291 → `FUN_15023260`（1028 字节）

归档内文件的同步/异步读取核心：

- 入口先处理 `param_3 == 0`（空读直接成功）与已缓存扇区（`+0x138` 缓冲非空且 `+0x13c` 未读完时直接 `memcpy`，更新 `+0x120` 位置与 `+0x13c` 已读量）。
- 未缓存时按扇区大小（`DAT_150494a8`，即 `0x200 << shift`）把请求切成多片，每片调 `FUN_150203c0`（343 字节，异步读任务：`CreateEventA` / `CreateThread` / `ReadFile` / `SetFilePointer`，带 `GetTickCount` 超时与 `FUN_150203c0` 内的 `SetEvent` 完成通知）；同步调用时 `WaitForSingleObject(event, INFINITE)` 等完成；异步（`param_5 != NULL`）时直接返回，由 `FUN_150208f0` / `FUN_15020830` 处理已缓存扇区的快路径（`+0x110 == -1` 时走 `FUN_15020110` 解压扇区）。
- 读完后若文件记录 `+0x14c != 0` 且状态为 4，调 `FUN_1501c790(..., 0x85100083, ...)`（**推断**为块校验失败上报）。
- 全程持全局临界区 `DAT_1504c2fc`，异步等待期间临时释放（`LeaveCriticalSection` → `WaitForSingleObject` → `EnterCriticalSection`）。

其余读取辅助：序号 279 → `FUN_15022660`（78 字节）→ `FUN_150226b0`（313 字节）=“打开文件 + 取大小 + `FUN_15023260` 全读 + 可选零填充”的整文件加载（Game 最常用的读文件入口，20 个调用者）；序号 269 → `FUN_15023240`（31 字节）是 `FUN_15023260` 的薄包装。

### 5.4 归档/文件记录的引用计数与全局链表

- 全局临界区：`DAT_1504c2fc`（几乎所有 SFile/序号函数入口都 `EnterCriticalSection` / `LeaveCriticalSection` 它）。
- 全局归档链表哨兵：`PTR_LOOP_150494b8`；`FUN_15020af0` 在解析成功后按优先级 `ARCHIVEREC+0x118` 降序插入（`while (cur[0x46] > new[0x46]) cur = cur[1]`，即高优先级在前，先查到）。
- 归档释放：序号 252 → `FUN_15021540`（195 字节）：从归档链表摘除；`+0x148`（dword [0x52]）引用计数减 1，为 0 时 `FUN_1501e480` 清理 + `delete`；若记录的 `+0x14c`（[0x53]）非零则先调序号 253 释放其关联文件记录。
- 文件释放：序号 253 → `FUN_15021610`（132 字节）：从文件链表哨兵 `PTR_LOOP_150494d0` 摘除；`+0x144`（dword [0x51]）引用计数减 1，为 0 时 `FUN_1501e600` 清理 + `delete`。
- 序号 151 → `FUN_150221e0`（58 字节）：在全局临界区内对 `ARCHIVEREC + 600`（0x258）做 `param_2 == 0 ? +1 : -1`——即归档的另一引用计数增/减（**推断**为归档句柄的 addref/release，函数名未验证；Game 唯一调用者 `FUN_6f020d90`，118 字节，未反编译）。
- 序号 280 → `FUN_150238e0`（50 字节）：`FUN_1502be40(ptr, "SFile.cpp", 0x1404)`，即释放一块 Storm 分配的内存（与 `SFile::Unload` 同机制，行号不同）；Game 有 32 个调用者，是最常用的 Storm 序号——**推断**为“释放 Storm 返回的缓冲”的通用 free 口。

## 6. 数据结构

### 6.1 `ARCHIVEREC`（`SFile::ARCHIVEREC`，0x25c = 604 字节）

`FUN_15022820` / `FUN_15020af0` / `FUN_15022d20` 共同验证的字段（偏移以字节计，`[n]` 为 dword 序号）：

| 偏移 | 内容 |
|---|---|
| +0x00 / +0x04 | 全局归档链表前后指针（`TSLinkedNode` 式侵入链表） |
| +0x08 | 归档路径（0x104 字节） |
| +0x10c（[0x43]） | Win32 文件句柄（`CreateFileA` 返回） |
| +0x110（[0x44]） | 标志（`FUN_15022820` 里按 `iVar3` 与 `param_3` 的位算出） |
| +0x114（[0x45]） | 未验证（`FUN_15020af0` 末尾按它决定是否起后台线程） |
| +0x118（[0x46]） | 优先级（挂载时传入，链表按它降序） |
| +0x124（[0x49]） | 扇区大小（`0x200 << shift`） |
| +0x128（[0x4a]） | 扇区缓冲指针 |
| +0x130（[0x4c]） | 归档基址偏移（头扫描得到的魔数位置） |
| +0x134（[0x4d]） | 文件总大小（`GetFileSize`） |
| +0x138（[0x4e]） | 头拷贝指针（0x20 字节） |
| +0x13c（[0x4f]） | 块表指针（内存展开后 0x2c 字节/条） |
| +0x140（[0x50]） | 哈希表指针（16 字节/条） |
| +0x144（[0x51]） | 引用计数（序号 253 侧用，文件记录共用此布局时） |
| +0x148（[0x52]） | 引用计数（序号 252 侧用） |
| +0x14c（[0x53]） | 标志（非零时序号 252 先释放关联文件记录） |
| +0x258（+600） | 引用计数（序号 151 操作） |

注：归档记录与归档内文件记录都用 `FUN_15020600` 从同一个池（`DAT_150494cc`）分配 0x25c 字节，但字段布局不同（文件记录见 §6.2）；`+0x144/+0x148` 两个计数槽分别由序号 253/252 操作，**两者分工未完全验证**。

### 6.2 归档内文件记录（`FUN_15022d20` 建，0x25c 字节）

`FUN_15022d20`（841 字节，序号 268）按 `FUN_1501f850` 的查找结果建记录：`+0x08` 起存 0x104 字节请求名；`+0x110`（[0x44]）= -1；`+0x114`（[0x45]）= 所属归档指针；`+0x118`（[0x46]）= 块表条目指针；`+0x11c`（[0x47]）= 文件哈希；`+0x128`（[0x4a]）= 扇区数（`(块大小-1+扇区大小)/扇区大小`）；`+0x12c`（[0x4b]）= 扇区偏移表（块条目标志 `& 0xff00` 时分配）；`+0x138`（[0x4e]）= 0x1000 字节缓冲；`+0x144`（[0x51]）= 1；`+0x14c`（[0x53]）= `DAT_1505771c`（块条目 [6] 非零时，否则 0）；`+0x150`（[0x54]）= 1；所属归档的 `+0x148`（[0x52]，即 `local_114 + 0x148`）加 1。若 `param_3 & 4`，则不建记录、直接返回底层文件句柄（并 `SetFilePointer` 到块偏移）——即“裸句柄”模式。

### 6.3 全局量

| 地址 | 内容 |
|---|---|
| `DAT_1504c2fc` | 全局临界区（SFile 子系统大锁） |
| `DAT_1504c46c` | 全局搜索标志（bit0/bit1，`Open`/`LoadFile`/`FileExists` 共用；为 0 时当 1） |
| `DAT_1504c468` | 全局文件索引已建标志（0 → 调 `FUN_15023f70` 建） |
| `DAT_1504c469` | 未验证（`FUN_15024540` 里非零时走绝对路径分支） |
| `DAT_1504c470` | 全局文件索引（`TSHashTable` 式，`FUN_15025860` 查 / `FUN_15024e00` 插；表项 `[6]`=路径串、`[7]`=类型号） |
| `DAT_1504c4b8` | 基路径（0x104 字节，`SetBasePath` 写） |
| `DAT_1504c7d0` | 未验证（`FUN_15023f70` 里与基路径比较的路径） |
| `DAT_150494b8` / `DAT_150494bc` | 全局归档链表哨兵 / 表头 |
| `DAT_150494d0` | 全局文件链表哨兵 |
| `DAT_150494cc` | 记录分配池头（`FUN_15020600`） |
| `DAT_1504c458` | Storm 文件子系统初始化标志（0 → `FUN_1501fae0(0x100001)`） |
| `DAT_1504c440` / `DAT_1504c434` | 后台线程句柄/事件（`FUN_15020af0` 在特定标志下 `CreateThread(FUN_1501e980)`，**线程用途未验证**） |

## 7. 签名验证：序号 251 → `FUN_15020ea0`（1195 字节）

Game 经序号 251 校验归档/地图签名（调用者：`FUN_6f156110`、`FUN_6f155f60`、`FUN_6f155ab0`，均在 `6f155xxx–6f156xxx` 段，**各自场景未验证**）：

1. 先调 `FUN_15021350(param_1, param_2, 0, 0, 0, 0)`——返回非零则直接返回 1（**该快路径的含义未验证**，推断为“无签名/已验证过则跳过”）。
2. `FUN_15020940(archive, "(signature)", 0, '\0')` 在归档里找 `(signature)` 文件；找不到则错误码 `0x4dc`、返回 0。
3. `LoadLibraryA("advapi32.dll")`，逐个 `GetProcAddress` 取 9 个 CryptoAPI 函数存入 0x24 字节函数表：`CryptAcquireContextA` / `CryptCreateHash` / `CryptDestroyHash` / `CryptDestroyKey` / `CryptHashData` / `CryptImportKey` / `CryptReleaseContext` / `CryptSignHashA` / `CryptVerifySignatureA`。
4. `CryptAcquireContextA(&hProv, "Blizzard_Storm", "Microsoft Base Cryptographic Provider v1.0", PROV_RSA_FULL=1, 0)`，失败则重试 `CRYPT_NEWKEYSET=8`。
5. `CreateFileMappingA` + `MapViewOfFile` 把归档文件映射进内存；取 `(signature)` 块表条目（`piVar6`），要求 `(条目[3] & 0x1ff00) == 0` 且 `条目[2] > 8` 且 `*条目 != 0`，否则 `*param_2 = 2`。
6. `FUN_1503c110()` 取模块句柄（序号 302），`FindResourceA(hMod, "BLIZZARDKEY", "#256")` → `LoadResource` → `LockResource`，`CryptImportKey` 导入公钥；`CryptCreateHash(hProv, CALG_SHA1=0x8003, ...)` 后对映射区（归档基址起、`*(条目)+条目[2]` 长度内，分段哈希）`CryptHashData`，最后 `CryptVerifySignatureA(hHash, 签名, 签名长-8, hKey, NULL, 0)`；成功置 `local_24 = 1`（`*param_2 = _DAT_15043674`，**该值未验证**），失败置 `*param_2 = 2`。
7. 清理：`CryptDestroyHash` / `CryptDestroyKey` / `UnmapViewOfFile` / `CloseHandle` / `CryptReleaseContext`（`CryptAcquireContextA` 再调一次带 `CRYPT_DELETEKEYSET=0x10` 的清理，**推断**为删容器）/`FreeLibrary`；最终返回 `local_24`（1=签名有效，0=无效/无签名）。

注：签名校验只验证“归档内容 + 签名块”的哈希，不涉及游戏逻辑；`FUN_15021350` 的快路径与 `_DAT_15043674` 的值未验证。

## 8. Game.dll 的调用面

### 8.1 调用方式：两簇 6 字节跳转 thunk

Game 不直接 `call [IAT]`，而是经两簇 thunk（每簇一个 6 字节 `jmp dword ptr [IAT槽]`）：`0x6f1205xx` 簇与 `0x6f8190xx` 簇。Ghidra 里这些 thunk 被命名为 `Ordinal_<序号>`。下表按 Game 内真实调用者数量排序（调用者函数名见 `mpq_game_dump2.txt`）。

### 8.2 26 个 MPQ 序号的身份与调用者

| 序号 | Storm 地址 | 身份（证据等级） | Game 调用者数 | 主要调用者 |
|---|---|---|---|---|
| 151 | 0x150221e0 | 归档引用计数 ±1（已验证，`+600` 槽） | 1 | `FUN_6f020d90` |
| 251 | 0x15020ea0 | 签名验证（已验证，§7） | 3 | `FUN_6f156110`、`FUN_6f155f60`、`FUN_6f155ab0`（1189 字节） |
| 252 | 0x15021540 | 归档释放/引用计数（已验证） | 15 | `FUN_6f020cd0`、`FUN_6f155ab0`、`FUN_6f6f1060`、`FUN_6f2f17d0` 等 |
| 253 | 0x15021610 | 文件句柄释放/引用计数（已验证） | 13 | `FUN_6f020e20`、`FUN_6f702450`、`FUN_6f155ab0`、`FUN_6f144a00` 等 |
| 263 | 0x15022220 | 未验证（20 字节，无被调函数） | 1 | `FUN_6f021080`（30 字节） |
| 264 | 0x150224e0 | 推断：由文件句柄取所属归档（`FUN_6f702450` 上下文：开文件→取归档→取归档路径→释放） | 2 | `FUN_6f702450`、`FUN_6f0210a0` |
| 265 | 0x150225e0 | 归档内文件大小（已验证，调 Win32 `GetFileSize`） | 9 | `FUN_6f169620`、`FUN_6f155ab0`、`FUN_6f144a00` 等 |
| 266 | 0x15022820 | 打开归档文件（已验证，§5.1） | 10 | `FUN_6f020e20`（启动挂载）、`FUN_6f155ab0`、`FUN_6f1c9bf0` 等 |
| 267 | 0x15022a90 | 打开归档内文件（已验证本体 `FUN_15022d20`；序号名是推断） | 9 | `FUN_6f020e20`（`(teen)` 检查）、`FUN_6f702450`、`FUN_6f144a00` 等 |
| 268 | 0x15022d20 | 打开归档内文件底层（已验证，§6.2；与 267 共用实现，参数不同） | 5 | `FUN_6f84f4c0`、`FUN_6f0210a0`、`FUN_6f155ab0` 等 |
| 269 | 0x15023240 | 扇区读取薄包装（已验证 → `FUN_15023260`） | 11 | `FUN_6f169620`、`FUN_6f055410`、`FUN_6f144a00` 等 |
| 270 | 0x150236c0 | 未验证本体（179 字节；被 `SetBasePath` 与启动挂载调用，推断为归档/索引重建） | 1 | `FUN_6f020e20` |
| 271 | 0x150237b0 | 归档内文件 seek（已验证，调 Win32 `SetFilePointer`） | 3 | `FUN_6f055410`、`FUN_6f055340`、`FUN_6f143fe0` |
| 272 | 0x150238c0 | 未验证（16 字节） | 1 | `FUN_6f0280e0`（S6 的 UI/引擎初始化总入口） |
| 275 | 0x15022410 | 推断：取归档文件路径（`FUN_6f702450` 上下文：`"    Archive: %s"` 打印） | 4 | `FUN_6f702450`、`FUN_6f76ed90`、`FUN_6f76ecd0` |
| 279 | 0x15022660 | 整文件加载（已验证 → `FUN_150226b0`，§5.3） | 20 | `FUN_6f048e20`、`FUN_6f6d7e70`、`FUN_6f3940e0`、`FUN_6f02f680`、`FUN_6f2debf0` 等 |
| 280 | 0x150238e0 | 释放 Storm 分配内存（已验证机制；语义是推断） | 32 | `FUN_6f048e20`、`FUN_6f02f680`、`FUN_6f6d7e70`、`FUN_6f30d140` 等 |
| 281 | 0x150227f0 | 未验证（修饰名 `_OOBECompleteWnfQueryCallback@28`，本体调 `FUN_150226b0`） | 5 | `FUN_6f1899f0`、`FUN_6f76df00`、`FUN_6f702d60` 等 |
| 288 | 0x15022250 | 归档内查找（已验证 → `FUN_1501f850`，全局搜索） | 22 | `FUN_6f169620`、`FUN_6f76e960`、`FUN_6f6ef530`、`FUN_6f66ba00` 等 |
| 289 | 0x150222e0 | 归档内查找（已验证 → `FUN_1501f850`，指定归档） | 13 | `FUN_6f029960`（版本检查）、`FUN_6f2d8790`/`FUN_6f2d8a60` 等（225 字节三胞胎） |
| 291 | 0x15023260 | 扇区读取核心（已验证，§5.3） | 1 | `FUN_6f6f0e90` |
| 293 | 0x15022ae0 | 打开归档变体（已验证，调 266+`FUN_15020af0`；带优先级参数） | 7 | `FUN_6f020e20`（`t.mpq`/`tx.mpq`）、`FUN_6f76ee40`（`%c.mpq`）等 |
| 294 | 0x15022640 | 未验证（7 字节） | 1 | `FUN_6f156110` |
| 295 | 0x15023690 | 未验证（23 字节） | 2 | `FUN_6f205050`、`FUN_6f200d30` |
| 300 | 0x15023070 | 未验证（390 字节） | 1 | `FUN_6f020f70` |
| 302 | 0x1503c110 | 取 Storm 自身模块句柄（已验证用法；Game 侧包装未反编译） | 1 | `FUN_6f153fb0`（15 字节） |

### 8.3 启动挂载：`FUN_6f020e20`（322 字节）

1. 读注册表 `Ordinal_423("Warcraft III", "Allow Local Files", 0, &flag)`（**推断**为 `SRegGetValue`）；若本地文件模式开且 `FUN_6f0475b0()` 返回非空路径，调序号 270（该路径）。
2. 按 `.data` 表 `0x6fb59000`（文件内直接读取，见下）挂载：`War3.mpq`（优先级 0）、`War3X.mpq`（优先级 1），各调序号 266（`266(name, priority, 2, &DAT_6fbb7fe0 + i)`，flags=2）。
3. 再挂 `War3XLocal.mpq`（优先级 2，表项 [3]；表项 [2] 为 NULL 跳过）。
4. 挂 `War3Patch.mpq`（优先级 8，flags=6，`&DAT_6fbb7ff0`）。
5. 序号 267 查 `"(teen)"` 文件：存在时挂 `t.mpq`（优先级 0x11=17）、`tx.mpq`（优先级 0x12=18），各调序号 293（`293(0, name, priority, 6, &DAT_6fbb8008 + i)`，flags=6）。
6. 调 `FUN_6f023e00(6, ...)` / `FUN_6f023ed0`（**未验证**，推断为挂载完成后的收尾/索引整理）与 `FUN_6f021140()`，返回 1；任一 266 失败且不是第 0 项时调 `FUN_6f0277c0()` 返回 0。

归档名表 `0x6fb59000`（`.data` 段直接读取，已验证）：

| 索引 | 值 | 索引 | 值 |
|---|---|---|---|
| [0] | `War3.mpq` | [6] | `War3_med.mpq` |
| [1] | `War3X.mpq` | [7] | `War3X_med.mpq` |
| [2] | NULL | [8] | NULL |
| [3] | `War3XLocal.mpq` | [9] | NULL |
| [4] | `War3_low.mpq` | [10] | `t.mpq` |
| [5] | `War3X_low.mpq` | [11] | `tx.mpq` |

### 8.4 画质档挂载与补丁选择：`FUN_6f2ce350`（508 字节）

状态机分支（`*(param_1+0x5f8)+0x16c` 的值）：

- 状态 1：调 `FUN_6f30d990()` 与 `FUN_6f790430()`（**未验证**）。
- 状态 2：`FUN_6f153f60()` 后按 `DAT_6fb592f0`（0 → `"Warcraft III.exe"`，1 → `"Frozen Throne.exe"`）拼路径；调 `FUN_6f30d990(exe路径, "War3.mpq", "War3Patch.mpq", "War3_med.mpq", "War3Patch_med.mpq", "War3_low.mpq", "War3Patch_low.mpq")`（六档名一次传入，**该函数本体未反编译**，推断为按画质挂载低/中档 MPQ）；再调 `FUN_6f794870(...)` 返回 5 / 0x26 / 0x27，分别对应 `FUN_6f090b00(0x200)` 或 `FUN_6f057820()`（**各返回值含义未验证**）。
- 状态 3：按 `*(param_1+0x5f8)+0x170` 的 0x33 / 0x35 / 其他分支调 `FUN_6f090b00(0x200)`（**含义未验证**）。
- 收尾：`FUN_6f30d6f0()`、两个虚表调用（`+0xd0` / `+0xd8`，**未验证**）、`FUN_6f2b46c0()`、`*(param_1+0x5d8) = 0`；状态 != 1 时 `FUN_6f29e8f0(0,0,0,4,1)`。

### 8.5 归档版本检查：`FUN_6f029960`（276 字节）

对表 `0x6fb592d0` 的 `War3.mpq` / `War3X.mpq` 逐个调序号 289（`289(0, name, 2, cookie)`）；成功时调 `FUN_6f0545c0` 解析出版本值，与表 `0x6fb592d8` 的期望值（`0xe, 0xf, 0x10, 0x12, 0x13`）逐项比较，匹配则置 `DAT_6fbb80e0[i] = (表A[i] <= 某阈值)`。**比较的字段与阈值语义未验证**（`FUN_6f0545c0` 与序号 289 返回值的确切结构未反编译）；表 `0x6fb592c8` 的前两项为 1、2（**含义未验证**）。

### 8.6 其余 Game 侧 MPQ 相关函数

- `FUN_6f702450`（260 字节）：“File Location for '%s'” 调试打印：序号 267 打开文件 → 序号 264 取所属归档 → 序号 275 取归档路径 → 打印 `"    Archive: %s"` → 序号 253 释放（264/275 的身份由此上下文**推断**）。
- `FUN_6f76ee40`（155 字节）：拼 `"%c.mpq"` 文件名，调序号 293（`293(0, name, 6, 4, &DAT_6fbee128)`）挂载（**场景未验证**，推断为按盘符/驱动器挂载的 MPQ）。
- `FUN_6f2a2010`（84 字节）：拼 `"%sMovies\%s.mpq"` 路径（**用途未验证**，推断为过场动画 MPQ 路径构造）。
- `FUN_6f144c20`（432 字节）：引用字符串 `"HDLSFILE"`（0x6f96207c）；内调 `FUN_6f143930` / `FUN_6f1434f0` / `FUN_6f169840`，并把对象挂进两条全局链表（哨兵 `PTR_LOOP_6fb66f8c` / `PTR_LOOP_6fb66f98`）。**该函数确切身份未验证**（推断与地图/存档文件打开有关；`"HDLSFILE"` 只是字符串证据）。
- `FUN_6f7bd750`（301 字节，0 调用者）：错误码 → 字符串表（见 §8.7）。
- 预取：RTTI `PrefetchListFile`（`6fb7b1ac`）、`TSExplicitList<PrefetchListFile>`、`TSHashTable<PrefetchListFile, HASHKEY_STRI_P>`；引用函数 `FUN_6f049d40`、`FUN_6f04b1f0`、`FUN_6f04bd50`、`FUN_6f04aee0`、`FUN_6f04c3d0`、`FUN_6f047e40`。**预取列表的填充与消费流程本轮未反编译**，只确认了类型与哈希表存在。
- 字符串 `"Disabling of map archive file deletion"`（0x6fa54894，无函数内引用）与 `"No Archive"` / `"    Archive: %s"`（`FUN_6f702450`）——地图归档删除相关的开关字符串，**上下文未验证**。

### 8.7 归档损坏错误码表：`FUN_6f7bd750`（Game 侧）

错误码 0–0x1d → 字符串（0 调用者，**推断**为调试/日志用的错误名表；产生这些码的校验函数本轮未定位）：

| 码 | 字符串 | 码 | 字符串 |
|---|---|---|---|
| 0 | `CORRUPT_NONE` | 0x10 | `CORRUPT_HASHENTRYBLOCK` |
| 1 | `CORRUPT_ARCHIVETOOBIG` | 0x11 | `CORRUPT_READFILEHEADERSIZE` |
| 2 | `CORRUPT_ARCHIVETOOSMALL` | 0x12 | `CORRUPT_READFILEHEADER` |
| 3 | `CORRUPT_READHEADER` | 0x13 | `CORRUPT_READFILEDATA` |
| 4 | `CORRUPT_HEADERSIZE` | 0x14 | `CORRUPT_FILEHEADERSIZE` |
| 5 | `CORRUPT_SIGNATURE` | 0x15 | `CORRUPT_FILEHEADERCRC` |
| 6 | `CORRUPT_HEADERCRC` | 0x16 | `CORRUPT_FILETOOBIG` |
| 7 | `CORRUPT_HEADERHASH` | 0x17 | `CORRUPT_BLOCKTABLEFULL` |
| 8 | `CORRUPT_HEADERBLOCK` | 0x18 | `CORRUPT_HASHTABLEFULL` |
| 9 | `CORRUPT_ARCHIVESIZE` | 0x19 | `CORRUPT_WRITEFILEHEADER` |
| 0xa | `CORRUPT_HASHOFFSET` | 0x1a | `CORRUPT_WRITEFILEDATA` |
| 0xb | `CORRUPT_HASHENTRIES` | 0x1b | `CORRUPT_WRITEHASHTABLE` |
| 0xc | `CORRUPT_HASHCRC` | 0x1c | `CORRUPT_WRITEBLOCKTABLE` |
| 0xd | `CORRUPT_BLOCKOFFSET` | 0x1d | `CORRUPT_WRITEARCHIVEHEADER` |
| 0xe | `CORRUPT_BLOCKENTRIES` | — | — |
| 0xf | `CORRUPT_BLOCKCRC` | — | — |

注：表里有 WRITE*（写归档）类错误码，说明 Storm/Game 存在写 MPQ 的路径；**本轮未定位任何写归档函数**（具名导出与 26 个序号里均无写操作），见 §12。

## 9. 关键函数地址表

### 9.1 Storm.dll（基址 0x15000000）

| 地址 | 函数 | 作用（证据） |
|---|---|---|
| 15020af0 | FUN_15020af0 | 归档头+表解析核心：魔数扫描、扇区大小、哈希/块表解密、优先级链表插入 |
| 15020ea0 | FUN_15020ea0（序号 251） | 签名验证：CryptoAPI + BLIZZARDKEY 资源 + `(signature)` 文件 |
| 15021540 | FUN_15021540（序号 252） | 归档释放（链表摘除 + 引用计数） |
| 15021610 | FUN_15021610（序号 253） | 文件句柄释放（链表摘除 + 引用计数） |
| 150221e0 | FUN_150221e0（序号 151） | 归档引用计数 `+600` 槽 ±1 |
| 15022250 | FUN_15022250（序号 288） | 归档内查找（全局搜索 → `FUN_1501f850`） |
| 150222e0 | FUN_150222e0（序号 289） | 归档内查找（指定归档 → `FUN_1501f850`） |
| 15022660 | FUN_15022660（序号 279） | 整文件加载（→ `FUN_150226b0`） |
| 150226b0 | FUN_150226b0 | 整文件加载实现：打开 + 取大小 + 扇区读 + 零填充 |
| 15022820 | FUN_15022820（序号 266） | 打开归档文件（建 ARCHIVEREC → `FUN_15020af0`） |
| 15022a90 | FUN_15022a90（序号 267） | 打开归档内文件（→ `FUN_15022d20`） |
| 15022d20 | FUN_15022d20（序号 268） | 打开归档内文件底层（建文件记录 / 裸句柄模式） |
| 15023240 | FUN_15023240（序号 269） | 扇区读取薄包装 |
| 15023260 | FUN_15023260（序号 291） | 扇区读取核心（同步/异步、缓存、切分） |
| 150236c0 | FUN_150236c0（序号 270） | 未验证本体（被 `SetBasePath` 调用） |
| 150238e0 | FUN_150238e0（序号 280） | 释放 Storm 分配内存（32 个 Game 调用者） |
| 15023ca0 | FUN_15023ca0 | 目录递归建索引：`.bz`→类型 1、`.MPQ`→类型 2、其他→类型 0；子目录递归 |
| 15023f70 | FUN_15023f70 | 建全局文件索引（基路径 + 附加路径 `DAT_1504c6c8`） |
| 15024540 | FUN_15024540 | 路径解析（索引查 → 基路径拼 → 归档查找） |
| 150251b0 | FUN_150251b0 | `SFile::Open` 实现（五态分发） |
| 15025170 | `SFile::Open`（序号 172） | `Open` 薄壳 |
| 150259d0 | `SFile::Read`（序号 174） | 五态读取分发 |
| 15025ef0 | `SFile::SetFilePointer`（序号 191） | 五态定位分发 |
| 15024750 | `SFile::GetFileSize`（序号 165） | 五态取大小分发 |
| 15024c70 | `SFile::Load`（序号 170） | 打开 + 全读 + 零填充 |
| 15024d30 | `SFile::LoadFile`（序号 171） | `Load(NULL, ...)` 薄壳 |
| 150240d0 | `SFile::Close`（序号 153） | 关闭（`fclose` + 释放记录 + `delete`） |
| 150244d0 | `SFile::FileExists`（序号 162） | 路径解析是否成功 |
| 15025d90 | `SFile::SetBasePath`（序号 190） | 写全局基路径 + 调序号 270 |
| 15026120 | `SFile::Unload`（序号 192） | 释放 Storm 分配内存 |
| 1503c110 | FUN_1503c110（序号 302） | 取 Storm 自身模块句柄 |
| 150203c0 | FUN_150203c0 | 异步读任务（线程 + 事件 + 超时） |
| 15020830 | FUN_15020830 | 读表辅助（`SetFilePointer` + `ReadFile`） |
| 150208f0 | FUN_150208f0 | 已缓存扇区快路径（→ `FUN_15020830`） |
| 15020110 | FUN_15020110 | 扇区解压（685 字节，`FUN_15023260` 调用） |
| 150243f0 | FUN_150243f0 | `.bz` 流解压读取 |
| 15020690 | FUN_15020690 | 块表后处理（`FUN_15020af0` 调用，407 字节） |
| 15020600 | FUN_15020600 | 记录池分配（`DAT_150494cc`） |

### 9.2 Game.dll（基址 0x6f000000）

| 地址 | 函数 | 作用（证据） |
|---|---|---|
| 6f020e20 | FUN_6f020e20 | 启动挂载：本地文件模式 + War3/War3X/War3XLocal/War3Patch + `(teen)`→t/tx.mpq |
| 6f029960 | FUN_6f029960 | War3/War3X 归档版本检查（序号 289 + 期望值表） |
| 6f2ce350 | FUN_6f2ce350 | 状态机：画质档 MPQ 挂载（六档名）与补丁选择 |
| 6f702450 | FUN_6f702450 | “File Location” 调试打印（267→264→275→253） |
| 6f76ee40 | FUN_6f76ee40 | `"%c.mpq"` 挂载（序号 293） |
| 6f2a2010 | FUN_6f2a2010 | `"%sMovies\%s.mpq"` 路径构造 |
| 6f7bd750 | FUN_6f7bd750 | CORRUPT_* 错误码 → 字符串表（30 码） |
| 6f144c20 | FUN_6f144c20 | 引用 `"HDLSFILE"`（身份未验证） |
| 6f020d90 | FUN_6f020d90 | 序号 151 唯一调用者（未反编译） |
| 6f1205xx / 6f8190xx | `Ordinal_NNN` thunk | 6 字节 `jmp [IAT]` 跳转桩（两簇） |

## 10. 与其它阶段的关系

- **S1 网络层**：同一 Storm.dll、同一种“序号导入”机制；本轮把 S1 遗留的“251–302 段序号身份未验证”补上了 MPQ 部分（251/252/253/265/266/267/268/269/271/279/280/288/289/291/293 已验证，263/270/272/275/294/295/300/302 仍待验证；401+ 段仍属 S1 的 SNet/日志范畴）。
- **S2 主循环**：`FUN_6f0280e0`（UI/引擎初始化总入口）直接调用序号 272（16 字节，身份未验证）；启动挂载 `FUN_6f020e20` 在 GameMain 启动序列内（调用者链未完整追到 GameMain，见 §12）。
- **S6 UI**：`FrameDef.toc`、`.fdf`、`.blp` 皮肤、`UI\MiscData.txt` / `UI\MiscUI.txt` 均经本层读出；`FUN_6f029960` 的表 B 里直接出现了 `Units\MiscData.txt`、`UI\MiscData.txt`、`UI\MiscUI.txt`、`UI\SoundInfo\MiscData.txt` 四个路径字符串（0x6fb592d8+8 起）。
- **S3 模拟 / S4 JASS2**：地图文件（`.w3x`）本身就是一个 MPQ 归档（`"maps\campaign\War3XRegularCreditsIce.w3x"` 等字符串经本层打开，`FUN_6f1c7fe0` 引用）；`war3map.j` 的加载走同一 `SFile::Open`/`Load` 路径（**该链路未逐段验证**）。
- **S9 录像**：录像文件读写（`TempReplay.w3g` 等）若走 Storm，将复用本层的序号 279/280/288 族；S9 轮需确认录像是否经 SFile 还是直接 Win32 文件 API。
- **S10 Battle.net**：下载的补丁/地图文件落盘后由本层挂载（`War3Patch.mpq` 的挂载点已验证）；补丁下载本身不在本层。
- **S11 随机数**：无直接关系；但注意 `FUN_15020af0` 的哈希种子 `"(hash table)"` / `"(block table)"` 与 `FUN_1501fa60` 哈希函数是 MPQ 自身的哈希，不是游戏 RNG。

## 11. 机制总述（流程图）

```
启动（FUN_6f020e20）
  注册表 Allow Local Files？ → 是：序号 270(本地路径)
  序号 266 打开 War3.mpq(优先级 0) → War3X.mpq(1) → War3XLocal.mpq(2)
  序号 266 打开 War3Patch.mpq(优先级 8，flags 6)
  序号 267 查 "(teen)" 存在？ → 是：序号 293 打开 t.mpq(17)、tx.mpq(18)
  （画质档：FUN_6f2ce350 状态 2 时挂 War3_low/med.mpq 等六档）

打开归档（序号 266 → FUN_15022820 → FUN_15020af0）
  CreateFileA → 建 ARCHIVEREC(0x25c) → 扫描找 MPQ\x1a 魔数（0x200 步长）
  → 读头（扇区大小、表偏移/条数）→ 读哈希表/块表并解密（种子 "(hash table)"/"(block table)"）
  → 块表展开（16→44 字节/条）、偏移重基址 → 按优先级插入全局链表

读文件（Game 调序号 279 / SFile::Load / SFile::Open+Read）
  FUN_15024540 路径解析（全局索引 DAT_1504c470 → 基路径 → 归档查找）
  → FUN_150251b0 按类型建 SFile 对象（0 普通 / 1 .bz 解压 / 2·3 归档内 / 4 资源）
  → SFile::Read 按类型分发 → 归档内走 FUN_15023260 扇区读（缓存 + 切分 + 异步）
  → 释放：序号 253（文件）/ 252（归档）/ 280（缓冲）

校验（序号 251 → FUN_15020ea0）
  找 "(signature)" → CryptoAPI（Blizzard_Storm 容器）+ BLIZZARDKEY 资源验签
```

## 12. 未验证点

1. 序号 263（0x15022220，20 字节）、272（0x150238c0，16 字节）、294（0x15022640，7 字节）、295（0x15023690，23 字节）、300（0x15023070，390 字节）的本体——均只有 1–2 个 Game 调用者，未反编译。
2. 序号 264（取所属归档）与 275（取归档路径）的身份是**推断**（仅凭 `FUN_6f702450` 的调用上下文，未反编译本体 `FUN_150224e0` / `FUN_15022410`）。
3. 序号 270（`FUN_150236c0`，179 字节）本体未反编译；“基路径变更后重建索引”只是推断。
4. 序号 281 的修饰名是 `_OOBECompleteWnfQueryCallback@28`（Windows 回调风格），本体只确认调 `FUN_150226b0`；它在 Game 里的 5 个调用者场景未验证。
5. 序号 151 的函数名未验证（只确认是 `ARCHIVEREC+600` 引用计数的 ±1）；唯一 Game 调用者 `FUN_6f020d90` 未反编译。
6. 哈希函数 `FUN_1501fa60`、查找 `FUN_1501f850`、解密 `FUN_1501f500`、块表处理 `FUN_1501e7a0` / `FUN_15020690`、扇区解压 `FUN_15020110`、索引构建 `FUN_15023ca0` 的递归细节（除已验证的 `.bz`/`.MPQ` 分支外）均未逐行反编译；MPQ 哈希算法的具体常数未验证。
7. 写 MPQ 路径完全未定位：`CORRUPT_WRITE*` 错误码证明存在写操作，但具名导出与 26 个序号里均无写函数；写函数可能在 Game 未导入的序号段，或在 Game.dll 内部用 Win32 API 直接写。S9（录像）与 S10（补丁）轮需留意。
8. `FUN_6f020e20` 到 GameMain（`6f0297d0`）的完整调用链未验证；`FUN_6f30d990` / `FUN_6f794870`（画质档挂载）与 `FUN_6f023e00` / `FUN_6f023ed0`（挂载收尾）本体未反编译；`FUN_6f029960` 的版本比较字段语义未验证。
9. `PrefetchListFile` 预取系统的填充/消费流程未反编译；`FUN_6f020d90`、`FUN_6f153fb0`（序号 302 包装）、`FUN_6f144c20`（`"HDLSFILE"`）的身份未验证。
10. `SFile::EnableHash`（序号 157，18 字节）本体未反编译；`DAT_1504c469` / `DAT_1504c7d0` / `DAT_1504c6c8` / `ARCHIVEREC+0x114` 等全局量的确切含义未验证；`FUN_15020af0` 末尾起的后台线程（`FUN_1501e980`）用途未验证。
11. 块表内存条目 0x2c 字节的完整字段布局未验证（只确认步长 0x2c 与偏移字段的重基址）；哈希表条目 16 字节的字段（哈希 A/B、语言、平台、块索引）未逐字段验证。
12. `(teen)` / `t.mpq` / `tx.mpq` 的具体内容与挂载条件（除“`(teen)` 文件存在”外）未验证；`"%c.mpq"`（`FUN_6f76ee40`）的调用场景未验证。
