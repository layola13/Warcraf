# S11 随机数与确定性保障

- 状态：已验证 2026-11-06
- 证据：Ghidra 12.2 工程 `~/workspace/warcraf_ghidra/war3`（Game.dll 1.27.0.52240）；脚本与原始反编译：`rng_dump.txt`、`rng_dump2.txt`、`rng_dump3.txt`、`rng_dump4.txt`、`rng_dump5.txt`、`rng_dump6.txt`、`rng_dump7.txt`、`rng_dump8.txt`、`rng_dump9.txt`（脚本 `DumpRng*.java`，均在 `~/workspace/warcraf_ghidra/scripts/`）
- 一句话结论：引擎不用 CRT `rand`，用自研查表随机数——8 字节状态 + 一张 64 个 dword 的固定随机表，四路计数器以 61/59/53/47 的素数周期推进；种子来自开局同步的游戏设置，`SetRandomSeed` 会做 45 轮预热；随机状态本身被折进每回合交换的 `rand checksum`；浮点运算走软件浮点（`__fadd/__fsub/__fmul`），不依赖硬件浮点语义——这几层合起来保证各客户端算出完全相同的随机序列。

## 1. 机制总述

确定性随机数 = 同一种子 + 同一算法 + 同一消费顺序 + 同一位精确运算：

1. **同一算法**：核心抽取 `FUN_6f199400`（178 字节，无被调函数），67 个调用者。状态是 8 字节（`state[0]` 累加器 + `state[1]` 打包的 4 路计数器），每抽一次查 4 次固定表、做旋转异或、累加器加一。
2. **同一种子**：开局时 `FUN_6f30d9c0` 从同步的游戏设置对象（全局指针 `DAT_6fbe40a4` 的 +0x1c/+0x40/+0x50 字段）给**三个**随机状态播种；`SetRandomSeed`（JASS native）走同一播种函数 `FUN_6f1ae7e0` + 45 轮预热 `FUN_6f675fe0`。
3. **同一消费顺序**：所有随机消费（JASS `GetRandomInt/GetRandomReal`、物品/野怪随机选择、战斗掷骰等）都发生在模拟回合内，按回合顺序执行，回合本身由 S0 的锁步同步（TurnsSync）定序。
4. **同一位精确运算**：`GetRandomReal` 的缩放用软件浮点（`FUN_6f0527e0` 乘、`FUN_6f0528b0` 减、`FUN_6f0529d0` 加，分别为 538/642/667 个调用者，全引擎通用），结果与机器浮点单元无关。
5. **可验证**：随机状态每回合被折进 `rand checksum`（`FUN_6f3130c0`，由 TurnsSync 出入函数 `FUN_6f310c80` 调用），随回合数据交换比对；跑偏触发 S0 的 TrustedDesync。

## 2. 核心随机数算法

### 2.1 状态结构（8 字节，`CRandData` 疑似同物，见未验证点）

```
struct RandState {       // 8 字节，指针由调用者以 __fastcall 的 ECX 传入
  uint32 acc;            // state[0]：累加器，也是每次抽取的返回值
  uint32 idx;            // state[1]：4 路计数器打包（每字节一路）
};
```

### 2.2 固定随机表 `DAT_6f964e48`（256 字节 = 64 个 dword，小端）

表内容（Ghidra 实测，前 4 个 dword：`0x9927148e 0x08c7aafd 0x1f3ee6d5 0xda55bbf6`；完整 256 字节见附录 A）。所有查表偏移恒为 4 的倍数，等效于 64 项 dword 表，索引为字节值 / 4。

同一张表也被校验和哈希函数族复用（`FUN_6f1b0040`、`FUN_6f1b0280`、`FUN_6f1b07b0`、`FUN_6f1b0c20`、`FUN_6f1b0d40`、`FUN_6f1b0e60`、`FUN_6f1b0ee0`、`FUN_6f1b13a0`、`FUN_6f1af940`、`FUN_6f1aff20` 等，均以 `(x>>16 & 0xfc)`、`(x>>10 & 0xfc)`、`(x>>4 & 0xfc)`、`(x&0x3f)*4` 四路取表项）。

### 2.3 抽取 `FUN_6f199400`（反编译，`rng_dump4.txt`）

```c
int __fastcall FUN_6f199400(int *state)
{
  uint idx = state[1];
  uint b0 = idx & 0xff, b1 = (idx >> 8) & 0xff,
       b2 = (idx >> 16) & 0xff, b3 = (idx >> 24) & 0xff;
  uint nb0 = b0 >= 28 ? b0 - 28 : b0 + 216;   // 步 -28，模 244
  uint nb1 = b1 >= 24 ? b1 - 24 : b1 + 212;   // 步 -24，模 236
  uint nb2 = b2 >= 12 ? b2 - 12 : b2 + 200;   // 步 -12，模 212
  uint nb3 = b3 >= 4  ? b3 - 4  : b3 + 184;   // 步 -4，模 188
  int mix = (rotl32(T[nb1], 3) ^ rotl32(T[nb2], 2)
             ^ T[nb0] ^ rotl32(T[nb3], 1));
  state[0] += mix;
  state[1] = (nb3 << 24) | (nb2 << 16) | (nb1 << 8) | nb0;
  return state[0];
}
```

其中 `T[i]` 是表 `DAT_6f964e48` 在字节偏移 `i` 处的非对齐 32 位读。注意：种子与步长、模数全是 4 的倍数，所以四路字节恒为 4 的倍数，实际查的是 64 项 dword 表的第 `b/4` 项。

### 2.4 周期（按反编译在 Python 中复现验证）

- 四路字节的有效取值数：b0：244/4 = **61**；b1：236/4 = **59**；b2：212/4 = **53**；b3：188/4 = **47**——全是素数。
- 单路周期即取值数（步长与模数互素，Python 复现确认 b0/b1/b2/b3 周期确为 61/59/53/47）。
- 四路组合（索引元组）重复周期 = lcm(61, 59, 53, 47) = **8,965,109 次抽取**。
- 累加器是该周期增量序列的滑动和，因此原始输出的完整周期是 8,965,109 的倍数（确切倍数取决于一个周期内增量和 S mod 2³²，未计算——见未验证点）。
- 复现验证：对种子 0、1、7、42、1000000、123456789、0xFFFFFFFF 各跑 200,000 次抽取，四路字节始终为 4 的倍数且分别落在 [0,240]、[0,232]、[0,208]、[0,184] 内，无越表读取（表长 256 字节，最大偏移 252+3）。

### 2.5 播种 `FUN_6f1ae7e0`（`__thiscall`，ECX=状态指针，栈上传 seed）

```c
void FUN_6f1ae7e0(uint *state, uint seed)
{
  state[0] = seed;
  state[1] = (seed % 59) * 0x400 | (seed % 61) * 4
           | (seed % 53) * 0x40000
           | (((seed / 47) * 0x11 + seed) * 0x4000000);
}
```

按字节拆开（推导，见 2.4 的周期对应关系）：

| 字节 | 种子公式 | 取值 | 对应周期 |
|---|---|---|---|
| b0 | `(seed % 61) * 4` | 0..240（步 4） | 61 |
| b1 | `(seed % 59) * 4` | 0..232（步 4） | 59 |
| b2 | `(seed % 53) * 4` | 0..208（步 4） | 53 |
| b3 | `(((seed/47)*17 + seed) & 0x3f) * 4` | 0..252（步 4） | 47（循环内 0..184） |

种子取模数 61/59/53/47 与四路循环长度一一对应，是有意设计。唯一例外是 b3：种子可取到 252，超出循环上限 184，但按步 -4 递减，最多 17 次抽取后落入循环内，且查表偏移始终 < 256，不越界。

### 2.6 `SetRandomSeed` 与 45 轮预热

JASS native `SetRandomSeed` → `FUN_6f1f64b0`（36 字节）：

1. `FUN_6f1ae7e0(seed)`——按 2.5 播种；
2. `FUN_6f199400()`——丢弃式抽一次；
3. `FUN_6f675fe0()`——**45 轮预热**（`local_10 = 0x2d` 循环）：每轮按 2.3 的同一查表/旋转混合推进累加器，再用 `FUN_6f1ae7e0` 把演化中的累加器重播为新种子（反编译中 fastcall 寄存器直通，状态指针传递细节未完全解析，但 45 轮循环与重播种结构是明确的）。

## 3. JASS 层 native 映射

注册函数 `FUN_6f7e3710`（ECX=实现，EDX=名字，栈上=类型签名）。两张 native 注册表（S4 的两表：`FUN_6f1e9a50`、`FUN_6f6f5d00`）都注册了以下名字；实现地址取自第一张表的反汇编（`rng_dump3.txt` 6f1eda29–6f1edb78 段）：

| JASS native | 实现 | 行为（已反编译） |
|---|---|---|
| `GetRandomInt` | `FUN_6f1e43e0`（55 字节） | `low==high` 返回 low；否则 `low + ((uint64)draw * (|high-low|+1) >> 32)`（64 位乘高位取整，无偏区间映射） |
| `GetRandomReal` | `FUN_6f1e4420`（165 字节） | 见 3.1 |
| `SetRandomSeed` | `FUN_6f1f64b0`（36 字节） | 见 2.6 |
| `ChooseRandomCreep` | `FUN_6f1db0a0`（26 字节） | 薄壳：`FUN_6f76e610(1,1,1)` + `FUN_6f655520(1,1,1)` |
| `ChooseRandomItem` | `FUN_6f1db0c0`（24 字节） | 薄壳：`FUN_6f639b20(0,1,0)` |
| `ChooseRandomItemEx` | `FUN_6f1db0e0`（22 字节） | 薄壳（同上族，未展开） |
| `PlaceRandomUnit` | `FUN_6f1f1d00` | 仅定位到注册项，未反编译 |
| `PlaceRandomItem` | `FUN_6f1f1cc0` | 仅定位到注册项，未反编译 |

### 3.1 `GetRandomReal`（`FUN_6f1e4420`）

- 先用软件浮点减法（`FUN_6f0528b0`，即 `__fsub`）得到区间量，取绝对值与常量 `DAT_6fb59468` = **2.0e-07** 比较：区间过小直接返回 low。
- 核心是 23 位尾数法：`(draw & 0x7fffff) | 0x3f800000` 构造 [1,2) 的 float，再减 1.0 得到 [0,1)（同款写法见内部助手 `FUN_6f199380` / `FUN_6f1993d0`，减 `DAT_6f950d8c` = 1.0f）。
- 再经软件浮点乘/加（`FUN_6f0527e0` `__fmul`、`FUN_6f0529d0` `__fadd`）缩放到 [low, high)。
- 反编译中 fastcall 寄存器直通，缩放链的具体操作数对应关系未完全解析（见未验证点）；区间分支与尾数法是明确的。

### 3.2 内部取随机数助手族（都被多处调用）

| 函数 | 大小 | 行为 |
|---|---|---|
| `FUN_6f199360` | 16 字节 | int 区间：`(draw * n) >> 32` |
| `FUN_6f199380` / `FUN_6f1993d0` | 35 / 40 字节 | float [0,1)：尾数法 − 1.0f |
| `FUN_6f19f490` | 80 字节 | float [-1,1)：按抽取值符号取 `±(尾数 float − 2.0f)`（`DAT_6f95ad98` = 2.0f） |
| `FUN_6f1b35f0` | 35 字节 | float (0,1]：`2.0f − 尾数 float` |
| `FUN_6f1b8600` | 40 字节 | float [0,x)：`(尾数 float − 1.0f) * x` |
| `FUN_6f675f30` | 43 字节 | int 区间：先调 TLS 槽位读取 `FUN_6f04efb0`，再 `draw`，再 `(draw * n) >> 32`；被 `ChooseRandomItem/Creep` 实现链调用（8 个调用者，含 `FUN_6f639b20`、`FUN_6f655520`） |

`FUN_6f199400` 共有 **67 个调用者**，覆盖 JASS native、物品/野怪随机选择、战斗与模拟代码（`FUN_6f6737c0`、`FUN_6f675f30` 等）、AI（`FUN_6f87a410`）、地图/放置（`FUN_6f26c800`、`FUN_6f324bd0`、`FUN_6f3327a0`、`FUN_6f33b540`、`FUN_6f19d9c0`）等。调用者按地址段归属子系统是推断，未逐一核对。

## 4. 种子来源：开局三状态播种

`FUN_6f30d9c0`（139 字节，位于同步区 0x6f30xxxx，无直接调用者——疑似经函数指针/回调分发）在开局（或重同步）时播三个随机状态，种子全部取自同步的游戏设置对象（全局指针 `DAT_6fbe40a4`）：

1. `FUN_6f1ae7e0(FUN_6f3071d0())`——种子 = 聚合函数返回值（见 4.1）；
2. `FUN_6f1ae7e0(*(DAT_6fbe40a4 + 0x40))`，若该字段为 -1 则改用 `+0x1c` 字段；
3. `FUN_6f1ae7e0(*(DAT_6fbe40a4 + 0x50))`。

每次播种后调一次 `FUN_6f3131a0`（把状态字节折进校验累加器，见 5.2）。三个状态各自对应哪个用途（同步模拟 / 其它），未从反编译中钉死——JASS native 与内部消费都以 ECX 直通传递状态指针，槽位映射在解释器分发处（见未验证点）。

### 4.1 聚合种子 `FUN_6f3071d0`（71 字节）

遍历某对象内的 (id, 指针) 对数组（数量在 +0x1c，数组在 +0xc，每项 8 字节）：对 `id == -2` 且指针非空的项，调其虚表 +0x3c 的方法，把返回值写进同一个输出槽（后写的覆盖先写的）。该值既用作状态 1 的种子，也进 rand checksum（5.1）。虚方法身份、各 -2 项是哪些对象，未验证。

### 4.2 字符串哈希种子 `FUN_6f1ae7a0`（48 字节）

```c
h = s[0];
for (i = 1; i < strlen(s); i++) h = s[i] + (h * 0x7a2d ^ h);
```

即逐字节 `h = b + (h*0x7a2d ^ h)`，常数 **0x7a2d** 与 S0 事件泵滚动校验、`FUN_6f3131a0` 的折叠是同一常数（引擎通用字节混合常数）。调用者 `FUN_6f1ae860` 用它给一个状态播种；被哈希的是哪个字符串（玩家名/地图名？）未验证。`Ordinal_506` 在此作取串长用。

### 4.3 另一聚合 `FUN_6f025d00`（46 字节）

遍历对象内指针数组（数量 +0xc，指针表 +0x10），对非空项调虚表首方法，把返回值按 `acc = rotl(acc ^ v, 3)` 折叠。被 `FUN_6f30d9c0`（播种）与 `FUN_6f3130c0`（rand checksum）共同使用。

## 5. rand checksum：随机状态如何被校验

### 5.1 计算 `FUN_6f3130c0`（179 字节，唯一调用者 `FUN_6f310c80`——S0 的 TurnsSync 出入函数）

```c
void FUN_6f3130c0(sync_obj *p)
{
  base = *(p + 0x1c70);
  if (*(p + 0x2f8) != 0) base = FUN_6f3071d0();      // 聚合值（4.1）
  s1 = *(setup + 0x40); if (s1 == -1) s1 = *(setup + 0x1c);
  s2 = *(setup + 0x50);
  FUN_6f3131a0();                                   // 把 RNG 状态字节折进累加器
  x = base ^ rotl32(FUN_6f025d00() ^ 0x17ab, 3);     // 0x17ab：与录像版本号同值（S9）
  x = rotl32(x, 3) ^ s1;
  x = rotl32(x, 3) ^ s2;
  *(p + 0x1c64) = rotl32(x, 6);                     // 存回同步对象 +0x1c64
}
```

（反编译为 fastcall 直通，`FUN_6f3131a0` 与 `FUN_6f025d00` 的具体入参指针在反编译中被折叠，链式结构与常量是明确的。）

### 5.2 状态折叠 `FUN_6f3131a0`（191 字节）

取一个 8 字节随机状态（指针直通），对其 8 个字节逐个做 `acc += b * 0x7a2d ^ b`，累加进校验累加器。`FUN_6f30d9c0` 在每播一个状态后都调它（共 4 次调用：状态 1 后 1 次、状态 2 后 1 次、状态 3 后 2 次），与 5.1 呼应：rand checksum 本质上是“随机状态字节 + 开局种子字段 + 聚合值”的 rotl 混合。

### 5.3 七子系统校验打印

S0 已确认七条格式串 `%scheat / %scombat / %sgame / %snet / %srand / %stempest / %sunit checksum %08x` 在 `.rdata` 中连续存放（0x6f987de8–0x6f987e7a，`%srand checksum` 在 0x6f987e41）。但本轮核查发现：**这 7 个地址在整个镜像里没有任何直接引用**——原始文件里也不存在值为这些地址的 imm32/dword。说明打印函数用计算指针（如基址 + 偏移）访问这片连续串，而非直接引用。打印函数本身未定位（见未验证点）。`rand checksum` 的值由 5.1 产生、存在同步对象 +0x1c64，是推断（与 5.1 的存储位置吻合，但未见打印调用链）。

## 6. 确定性的其它支柱

- **软件浮点**：`FUN_6f0527e0`（`__fmul`，538 个调用者）、`FUN_6f0528b0`（`__fsub`，642 个调用者）、`FUN_6f0529d0`（`__fadd`，667 个调用者）是纯整数实现的 IEEE float 加/减/乘（尾数乘、指数加、规格化辅助 `FUN_6f052b50`）。`GetRandomReal` 及全引擎浮点关键路径走它们，不依赖 x87/SSE 的硬件舍入差异。推断：这是跨机器位精确的刻意设计。
- **TLS 全局槽位**：`FUN_6f04efb0`（27 字节，340 个调用者）= `TlsGetValue` 取线程局部数组再按槽位取指针（根 `FUN_6f158a30`：`TlsGetValue`，失败调 `Ordinal_465(0x57)`）。游戏/RNG 状态放在线程局部槽位里，单线程模拟不存在多线程竞争随机的问题。
- **CRT rand 是死导入**：`msvcr120.dll` 的 `rand` / `srand` 在导入表里（`game_imports.txt` 约第 306/337 行），但 Ghidra 中零引用、零调用者——导入了但没人用。游戏随机数完全不走 CRT。
- **Storm.dll 没有 RNG**：Storm 串里与 random 相关的只有 `FLT_DENORMAL_OPERAND`（浮点异常名），无随机数实现。
- **没有定点数**：RNG 路径与校验路径未见定点类型；`GetRandomReal` 用 float32 尾数法。引擎用“同一份二进制 + 软件浮点 + 同步回合”保证位精确，而非定点数（全库无 `FixedPoint` 类名；仅有的 `Fixed` 是 UI 串 `Test Map - Fixed Random Seed`）。
- **测试地图固定种子**：串 `Test Map - Fixed Random Seed`（0x6fa558d0）存在，但无直接代码引用，其与 `SetRandomSeed` 的接线未追踪（见未验证点）。

## 7. 关键函数地址表

| 地址 | 名称/角色 | 大小 | 证据 |
|---|---|---|---|
| `FUN_6f199400` | 核心抽取（查表 RNG） | 178 B，67 调用者 | `rng_dump4.txt` |
| `DAT_6f964e48` | 64 dword 固定随机表 | 256 B | `rng_dump5.txt`（hex 见附录） |
| `FUN_6f1ae7e0` | 播种（state[0]=seed，state[1]=取模打包） | 119 B，18 调用者 | `rng_dump5.txt` |
| `FUN_6f675fe0` | SetRandomSeed 的 45 轮预热 | 227 B | `rng_dump5.txt` |
| `FUN_6f1e43e0` | JASS `GetRandomInt` 实现 | 55 B | `rng_dump3.txt` |
| `FUN_6f1e4420` | JASS `GetRandomReal` 实现 | 165 B | `rng_dump3.txt` |
| `FUN_6f1f64b0` | JASS `SetRandomSeed` 实现 | 36 B | `rng_dump4.txt` |
| `FUN_6f1db0a0` / `FUN_6f1db0c0` / `FUN_6f1db0e0` | `ChooseRandomCreep` / `ChooseRandomItem` / `ChooseRandomItemEx` 实现（薄壳） | 26 / 24 / 22 B | `rng_dump4.txt` |
| `FUN_6f1f1d00` / `FUN_6f1f1cc0` | `PlaceRandomUnit` / `PlaceRandomItem` 实现 | 仅注册项定位 | `rng_dump3.txt` |
| `FUN_6f199360` / `FUN_6f199380` / `FUN_6f1993d0` / `FUN_6f19f490` / `FUN_6f1b35f0` / `FUN_6f1b8600` / `FUN_6f675f30` | 内部 int/float 取随机助手 | 16–80 B | `rng_dump5/6.txt` |
| `FUN_6f30d9c0` | 开局三状态播种 | 139 B | `rng_dump6.txt` |
| `FUN_6f3071d0` | 聚合种子（虚表 +0x3c，id==-2 项） | 71 B | `rng_dump7.txt` |
| `FUN_6f1ae7a0` | 字符串哈希种子（0x7a2d） | 48 B | `rng_dump7.txt` |
| `FUN_6f025d00` | 对象列表聚合（虚表首方法，rotl3 折叠） | 46 B | `rng_dump9.txt` |
| `FUN_6f3130c0` | rand checksum 计算 | 179 B，调用者 `FUN_6f310c80` | `rng_dump8.txt` |
| `FUN_6f3131a0` | RNG 状态字节折叠（0x7a2d） | 191 B | `rng_dump7.txt` |
| `FUN_6f1b0040` 等 | 复用随机表的哈希函数族（校验和用） | 572 B（`FUN_6f1b0040`） | `rng_dump6.txt` |
| `FUN_6f0527e0` / `FUN_6f0528b0` / `FUN_6f0529d0` | 软件浮点乘/减/加 | 203 / 260 / 274 B | `rng_dump9.txt` |
| `FUN_6f04efb0` / `FUN_6f158a30` | TLS 全局槽位读取 | 27 / 35 B，340 调用者 | `rng_dump7/8.txt` |
| `FUN_6f1e9a50` / `FUN_6f6f5d00` | 两张 JASS native 注册表 | 23346 / 25844 B | `rng_dump.txt`、`rng_dump3.txt` |

## 8. 数据结构 / 常量

- **RNG 状态**：8 字节（`uint32 acc; uint32 idx`），指针经 ECX 直通。
- **随机表**：`DAT_6f964e48`，256 字节，64 个 dword（附录 A）。
- **游戏设置对象**：全局指针 `DAT_6fbe40a4`，种子字段 +0x1c、+0x40、+0x50（字段语义未命名，是推断：开局同步数据）。
- **rand checksum 存储**：同步对象 +0x1c64（`FUN_6f3130c0` 写入）；聚合基值 +0x1c70，开关 +0x2f8。
- **常量**：`0x7a2d`（字节混合：事件泵 / 字符串哈希 / 状态折叠三处共用）；`0x17ab`（rand checksum 异或常数；与录像版本 0x17ab 同值，S9）；`2.0e-07`（`GetRandomReal` 区间阈值，`DAT_6fb59468`）；`1.0f`（`DAT_6f950d8c`）、`2.0f`（`DAT_6f95ad98`）。
- **周期**：索引元组 8,965,109 次抽取（61×59×53×47）；Python 复现验证通过。

## 9. 与其它阶段的关系

- **S0 帧同步**：`FUN_6f310c80`（TurnsSync 出入）在交换回合数据时调 `FUN_6f3130c0` 算 rand checksum；事件泵滚动校验与本阶段的 `0x7a2d` 折叠同源；随机序列的消费顺序由回合定序保证。
- **S2 主循环**：回合是推进单位；`FUN_6f30d9c0` 播种发生在同步区（0x6f30xxxx），与回合应用 `FUN_6f30bac0` 同一地址段。
- **S3 模拟核心**：战斗掷骰、伤害浮动等消费 `FUN_6f199400`（调用者含 0x6f67xxxx 段的战斗代码）；命令执行与随机消费都在同一回合内完成。
- **S4 JASS2**：`GetRandomInt/GetRandomReal/SetRandomSeed/ChooseRandom*/PlaceRandom*` 是 JASS 脚本唯一的随机来源，全部经两张 native 注册表绑定到本阶段函数；解释器主循环 `FUN_6f7f9070` 分发时以 ECX 传递状态指针（细节未验证）。
- **S9 录像**：同一初始种子 + 同一命令流 ⇒ 回放重算出同一随机序列；`0x17ab` 既是录像版本又是 rand checksum 常数（仅观察到同值，未证实有意关联）。
- **S10 Battle.net/大厅**：开局种子字段来自 `DAT_6fbe40a4` 指向的游戏设置对象，该对象在建房/加房时由主机与客户端同步（字段语义未逐一验证）。

## 10. 未验证点

1. 三个 RNG 状态各自的用途与槽位映射（哪个是 JASS 同步随机、哪两个是其它用途）；JASS native 调用时 ECX 里状态指针的来源（在解释器分发 `FUN_6f7f9070` 处，未展开）。
2. 七子系统校验打印函数本体：`%srand checksum` 等 7 个格式串无直接代码引用，打印函数未定位；`rand checksum` 值存于同步对象 +0x1c64 是已验证的写入位置，但打印调用链未见。
3. `CRandData` RTTI：名字串 `?.AVCRandData@@` 在 0x6fbb0370，但镜像里找不到指向它的类型信息/虚表，也无任何 `CRandData` 修饰方法名——8 字节状态是否就是 `CRandData`、还是另有其物，未证实。
4. `FUN_6f3071d0` 聚合的 -2 项对象身份与虚表 +0x3c 方法语义；`FUN_6f1ae7a0` 被哈希的字符串是哪个；`FUN_6f025d00` 列表里的对象身份。
5. `GetRandomReal` 缩放链的具体操作数对应（软件浮点调用间寄存器直通，反编译折叠）；`PlaceRandomUnit` / `PlaceRandomItem` 实现本体未反编译；第二张 native 注册表（`FUN_6f6f5d00`）是否绑定同一批实现地址未核对。
6. 累加器完整输出周期（8,965,109 的倍数，取决于一周期增量和 S mod 2³²，未计算）；`FUN_6f675fe0` 预热循环中状态指针的传递细节。
7. `eoRandom val: %u`（0x6fa4cf26）与 `Test Map - Fixed Random Seed`（0x6fa558d0）两处字符串无直接代码引用，接线未追踪；`DAT_6fbb81d0`（-14528.0f）在 `GetRandomReal` 中的角色未解析。
8. `FUN_6f30d9c0` 无直接调用者，其触发点（开局/重连/重同步？）未确认。

## 附录 A：随机表 `DAT_6f964e48` 完整 256 字节（Ghidra 实测）

```
8e 14 27 99 fd aa c7 08 d5 e6 3e 1f f6 bb 55 da
75 a0 4a 6a e8 bd 97 ff de 9b bc 9f 81 8a a1 46
6e 0b e3 63 76 7a 6c 5d 88 d3 69 ca c3 47 b9 25
83 ab a2 3f a6 41 7c ba e5 ac 95 01 7e cf 09 c1
d9 62 70 71 8d db 05 02 24 87 ef 54 c6 d4 37 30
d0 1b cb 7b b8 e4 d8 ec 49 ce ad dc 13 a9 94 c4
8f 39 ae 0d 18 52 dd 0e 78 fa f5 85 58 d2 af 6d
a4 b2 53 3b 51 a5 50 be fc 2d f4 11 48 98 16 f1
86 df 3d 66 5e 44 2e 2f 36 07 6b 17 8b 29 4c b6
e2 89 5f e7 cd a7 21 e1 4d c9 65 ed fe ee 9c 23
33 7d b7 04 9e 9a 2a 40 b3 10 5b f3 82 77 1c 92
20 4e 1e 57 22 72 06 8c 67 2c 73 fb 59 c2 0a bf
79 5c f9 0c 28 1a 12 68 74 34 19 42 b1 c0 84 f8
38 f0 15 9d 60 f2 3a 6f b4 90 eb 91 1d 7f 35 61
5a 32 03 56 a3 c5 2b 93 80 0f 4b 43 f7 a8 e0 3c
96 d1 64 26 d7 45 cc 4f c8 b0 e9 b5 00 d6 31 ea
```

## 附录 B：周期复现（Python，按反编译逐字实现）

对种子 0、1、7、42、1000000、123456789、0xFFFFFFFF 各跑 200,000 次抽取：四路字节恒为 4 的倍数，分别落在 [0,240]、[0,232]、[0,208]、[0,184] 内；单路周期实测 61/59/53/47，与取模数 61/59/53/47 一致；索引元组周期 61×59×53×47 = 8,965,109。
