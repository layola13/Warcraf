# S3 — 模拟核心：单位 / 战斗 / 技能

- 状态：已验证 2026-11-05
- 证据：`~/workspace/warcraf_ghidra/sim_dump.txt`（回合命令执行器 `FUN_6f2fxxxx` 一族反编译、RTTI 字符串定位）、`sim_dump2.txt`（`FUN_6f30ba60`/`FUN_6f30b940`/`FUN_6f30fbb0`、CDataStore 读写原语、JASS 签名字符串 xref）、`sim_dump3.txt`（RTTI xref 修正版、JASS native 注册表 `FUN_6f1e9a50`、CUnit 复活路径 `FUN_6f678380`）、`sim_dump4.txt`（伤害数据结构 `WidgetDamageData`、AoE 伤害 `FUN_6f6b19e0`、RTTI 动态转换辅助）、`sim_dump5.txt`（单体伤害 `FUN_6f6af530`、伤害列表 `FUN_6f6af6c0`、区域查询 `FUN_6f633a40`、单位重生/放置 `FUN_6f677ef0`、命令提交入口 `FUN_6f30b8c0`/`FUN_6f306fd0`）；`strings` 统计（CAbility* 492 类、CBuff* 226 类、CUnit* 51 类）
- 上承：S0 帧同步、S1 网络层、S2 主循环（本阶段的命令对象由 S2 的 `FUN_6f30bac0` 分发进入，伤害/技能执行发生在 25 ms 固定步进内）

## 1. 总述：命令 → 命令对象 → 队列 → 模拟执行

```
回合应用 FUN_6f30bac0（S2）
    │  按命令号 0x10–0x31 分发
    ▼
FUN_6f2fxxxx 一族（每个 185 字节，结构完全相同）
    │  ① 建一个栈上 CDataStore（vftable 指向 CDataStore）
    │  ② 调一个小的 FUN_6f83exxx 读取器，把回合包数据读进 CDataStore
    │  ③ 调 FUN_6f30ba60(session, &store, param_3)
    ▼
FUN_6f30ba60（90 字节）
    │  调 CDataStore 虚函数 (*vftable + 0x28)(&v1, &v2, &v3) 读出三个值
    ▼
FUN_6f30b940(cmd, v1, v2, v3, flag)
    │  FUN_6f30fbb0 建命令对象（24 字节，见 §2）
    │  命令号 < 0x40：FUN_6f83ff00（释放/丢弃，CDataStore.h:0x10a）
    │  命令号 0x40–0xfe：FUN_6f791c50（同文件断言/日志，身份未验证）
    │  否则：FUN_6f30b920 → FUN_6f30ce40 → FUN_6f30ceb0 把对象挂进双向链表队列
    │  命令号 0x1e/0x1f：会话 +0x225c += param_5 & 0xffff（与 S2 的速度/回合计数同一偏移）
    ▼
本地命令提交（不经过回合包）
    FUN_6f30b8c0（调用者 FUN_6f30e540）、FUN_6f306fd0（调用者 FUN_6f30d640）
    两者都先检查会话 +0x2288 != 'NONE'（0x4e4f4e45），再调 FUN_6f30b940；
    FUN_6f306fd0 把命令字节 + 0x40（'@'），即本地下发的命令号落在 ≥0x40 的区间
```

关键事实：S2 报告的“185 字节一族命令执行函数”实际上不是执行函数，而是**命令解包薄壳**——真正的执行发生在命令对象入队之后；本轮没有找到队列的消费（drain）函数，只确认了入队链（`FUN_6f30ceb0` 的唯一调用者是 `FUN_6f30ce40`，其唯一调用者是 `FUN_6f30b920`）。各命令号对应的具体语义（移动/攻击/施法）仍未验证。

## 2. 命令对象与 CDataStore

**命令对象**（`FUN_6f30c4a0` 分配 24 字节 = `0x18`，`FUN_6f30fbb0` 填充）：

| 偏移 | 含义（反编译确认） |
|---|---|
| `+0x00`–`+0x14` | 前 6 个 dword 清零，`+0x14`（第 6 个 dword 的低字节）处后写 `0xff`（`*(u16*)(obj+5)=0xff`，即 `+0x14=0xff`、`+0x15=0` 初始） |
| `+0x14`（byte） | 命令号（`FUN_6f30fbb0` 写 `param_1`） |
| `+0x15`（byte） | 标志（`FUN_6f30fbb0` 写 `param_5`；`FUN_6f30ce40` 中 `== 2` 时触发链表查找） |
| `+0x08` | 值 1（`FUN_6f30fbb0` 写 `param_2`） |
| `+0x0c` | 值 2（`FUN_6f30fbb0` 写 `param_3`） |
| `+0x10` | 值 3（`FUN_6f30fbb0` 写 `param_4`） |

**CDataStore 读写原语**（`engine/source/base/CDataStore.h`，源文件路径字符串为证）：

| 函数 | 大小 | 作用 |
|---|---|---|
| `FUN_6f04e1d0` | 81 | 写一个 dword：越界时调虚函数 `(*vftable+0xc)` 扩容，再 `*(base - begin + pos) = val`，`pos += 4` |
| `FUN_6f04dfa0` | 80 | 写一个 byte，同上，`pos += 1` |
| `FUN_6f83f8e0` | 118 | CDataStore 缓冲区分配/重分配（`Ordinal_401/405`，文件 `CDataStore.h:0xf9/0x11b`） |
| `FUN_6f83ff00` / `FUN_6f791c50` | 57 / 23 | 释放/断言（`CDataStore.h:0x10a`，`Ordinal_403` 日志） |

**每命令读取器**（`FUN_6f83exxx`，由各 `FUN_6f2fxxxx` 薄壳调用，差异即命令格式差异）：

| 读取器 | 大小 | 已验证内容 |
|---|---|---|
| `FUN_6f83ee60` / `FUN_6f83f010` / `FUN_6f83efd0` | 14 | 只读一个 dword（调 `FUN_6f04e1d0`）——对应命令 0x1a / 0x1b / 0x1c |
| `FUN_6f83f060` | 40 | 读 1 byte + 调 `FUN_6f83f530` + 读 `+0x1c` 处的 dword——对应命令 0x16 |
| `FUN_6f83f090` | 50 | 读 1 dword + 1 byte + 2 个 dword——对应命令 0x17 |
| `FUN_6f83f380` | 91 | 读 1 dword + 1 byte + `+5` 起的变长数据 + `+0x1b` 处的计数个元素——对应命令 0x23（含变长列表，推断为批量命令） |

命令号与读取器的对应关系来自 S2 的分发表（`FUN_6f30bac0` 内 `0x10→FUN_6f2fdbd0` 等）与各薄壳调用的读取器交叉核对；命令号本身的语义（哪个是移动、哪个是攻击）**未验证**。

**命令队列**（`FUN_6f30ce40` + `FUN_6f30ceb0`，160 字节，无直接被调函数）：纯双向链表拼接操作——`FUN_6f30ceb0(list, node, mode, anchor)`：`mode != 1` 时把 node 插到 anchor 之后（`*(anchor+4)=node` 等指针改写），`mode == 1` 时插到 anchor 之前；`FUN_6f30ce40` 先在链表里找 `+0x15 == 2` 的节点作锚点，再调 `FUN_6f30ceb0`，最后把 `(session+0xc + (cmd + flag*0xff)*4)` 处的计数器 +1（每命令号一个计数槽，推断为已入队命令统计）。队列的消费函数**未验证**。

## 3. 伤害系统：WidgetDamageData 与两条伤害路径

**数据结构 `WidgetDamageData`**（RTTI `?AUWidgetDamageData@@`，`0x6fbb1e7c`）：

- 每条 12 字节（3 个 dword）：`[0]` 目标 widget 指针、`[1]` float 伤害量、`[2]` 类别（0/1/2，见 `FUN_6f6af6c0` 的三个计数器 `+5/+6/+7`）。
- 向量容器：`FUN_6f6b81c0`（153 字节）是按 12 字节步长扩容/拷贝的 vector 生长函数（`Ordinal_405/401`，类型名 `s___AUWidgetDamageData___`）；`FUN_6f6aeb00`（23 字节）释放。调用者：`FUN_6f6af6c0`、`FUN_6f6b75f0`、`FUN_6f6b8500`。

**AoE 伤害 `FUN_6f6b19e0`**（1099 字节，`__fastcall`，14 个参数）：

1. 攻击者（`param_9`）与目标/施法者（`param_14`）对象做引用计数 +1（`+1` 字段，虚函数 `+0x1c` 检查，`FUN_6f02fbf0` 确认存活）。
2. 取最多 4 组（`param_4` 钳到 4）(float, dword) 对（`param_3`，每组 8 字节），算最大半径 `local_d0`（初值 `DAT_6fbb81d4`）。
3. 若 `param_8 & 0x7e0 == 0`：调 `FUN_6f661060`（2957 字节，区域枚举，见 §5）；否则调 `FUN_6f633a40`（812 字节，widget 区域查询，见 §5）。
4. 对结果列表（`local_b8`，12 字节/条）逐条：先调回调过滤器 `param_11`（非 0 时必须返回真），再调 `FUN_6f6af530` 施加伤害。
5. 收尾：调 `FUN_6f662780` / `FUN_6f6510a0`（位置/范围收尾，身份未验证），释放 `local_b8`，引用计数 -1（为 0 时调虚析构 `(**obj)()`）。

**单体伤害/治疗 `FUN_6f6af530`**（378 字节，唯一调用者 `FUN_6f6b19e0`）：

- 若 `*param_2 >= DAT_6fbb81d4`（阈值浮点，推断为 0）：走伤害分支——调 `FUN_6f685690`（目标/攻击者/类型参数，`param_3+0x50/0x54/0x58/0x5c`），再调目标虚函数 `(*param_1 + 0x120)`（伤害应用，虚函数号 0x120）。
- 否则走治疗分支：先判断攻击者是否为虚函数 `+0x1c` 返回的“当前单位”（`FUN_6f667470` 返回值比较），再按 `param_4`（治疗量）是否为 0 / `param_1[0x56]`（推断为某状态计数）是否 < 1 分流；非零治疗且带“EtherealHealBonus”（字符串为证，`FUN_6f02a4b0("EtherealHealBonus",0)`）时先对伤害量取反（`^ 0x80000000`）再调 `FUN_6f0527e0` 换算，最后调 `FUN_6f662d60` 施加；否则调目标虚函数 `(*param_1 + 0x128)`（治疗应用，虚函数号 0x128）。
- 收尾：把当前攻击者（`FUN_6f667a80()`）按虚函数 `+0xec` 得到的索引写进 `*(param_3 + 0x6c + idx*4)`（推断为“最后伤害来源”记录表）。

伤害/治疗共用一条函数、以阈值分流、虚函数 0x120/0x128 分别为伤害/治疗入口，是本轮反编译确认的结构；`DAT_6fbb81d4` 的具体数值、虚函数 0x120/0x128 的目标函数身份**未验证**。

**伤害列表构建 `FUN_6f6af6c0`**（798 字节）：

- 先算一组候选伤害值里的最大值（`param_2+0xe` 起的 float 数组，长度 `param_2[0x12]`），分支条件是 `param_2[0x18]` 与 `FUN_6f66eb20` 的结果（身份未验证）。
- 按 `param_2[0x14]` 的值（`0x10` / `0x20` / `0x4000000` / `0x400000` 四个哨兵）与若干状态查询（`FUN_6f02a380`、`FUN_6f695270/2a0/2b0/300`）决定是否跳过；再按 `param_1[0x56]` / `[0x57]` 计数是否 > 0 决定返回。
- 追加一条 12 字节 `WidgetDamageData`：`[0]=param_1`（目标）、`[1]=最大值 float`、`[2]=类别`——类别由 `param_2[0x19]` 与 `FUN_6f1c4fe0` / `FUN_6f1ccfd0` 的查询结果决定（0、1 或 2），三个类别分别有计数器 `param_2+5/+6/+7`。
- 类别 0/1/2 的具体含义（命中/闪避/暴击之类）**未验证**，只确认三分类结构存在。

## 4. 单位：CUnit / CWidget / CAgent 类族

RTTI 证据（`strings` 统计，类型名以 `.?AV` / `.?AU` 开头）：

| 类族 | 不同 RTTI 名数量 | 代表 |
|---|---|---|
| `CAbility*` | 492 | `CAbilityAttack`、`CAbilityAura*`、`CAbilityDeathCoil`、`CAbilityFrostArmor`、`CAbilityCleavingAttack`、`CAbilityDamageBonusBase` 等 |
| `CBuff*` | 226 | `CBuffPoisonDamage`、`CBuffPoisonDamageStackDoT`、`CBuffTornadoDamageAoe` 等 |
| `CUnit*` | 51 | `CUnit`、`CUnitAI`、`CUnitPool`、`CUnitEventReg`、`CUnitDamagedEventData`、`CUnitDeathEventData` 等 |
| `COrder*` | 6 | 订单类数量很少（订单主要以命令号 + CDataStore 表达，见 §2） |

RTTI 引用点（修正后的 VA，`'.?AV…'` 起始地址，数据 xref）：

| 类型 | 引用函数 | 函数性质 |
|---|---|---|
| `CUnit`（`0x6fbadc24`） | `FUN_6f665640`、`FUN_6f663810`、`FUN_6f656210` | `FUN_6f656210`（23 字节）是 type_info 静态注册桩（调 `FUN_6f04d0f0` + `FUN_6f668b00`）；另两处未反编译 |
| `CAbility`（`0x6fb8ee00`） | `FUN_6f3f14b0`、`FUN_6f3f3850`、`FUN_6f3e3880` | `FUN_6f3f14b0`（104 字节）是 `dynamic_cast` 辅助（`FUN_6f04d150(0, "CAbility", …)`，成功时调 `FUN_6f3cac90`）；`FUN_6f3e3880` 是 type_info 注册桩 |
| `CBuff`（`0x6fb8ef18`） | `FUN_6f511480`、`FUN_6f510c10`、`FUN_6f510200` | `FUN_6f511480`（97 字节）是 `dynamic_cast` 辅助（成功时调 `FUN_6f3ccd30`） |
| `CWidget`（`0x6fbad934`） | `FUN_6f634a10`、`FUN_6f634860`、`FUN_6f633380` | 未反编译（`FUN_6f633380` 在区域查询簇内，推断与 widget 查询相关） |
| `CUnitAI`（`0x6fbb7dc8`） | `FUN_6f8a30d0`、`FUN_6f8a3060`、`FUN_6f8a2da0` | `FUN_6f8a30d0`（97 字节）是 `dynamic_cast` 辅助（成功时调 `FUN_6f8a29f0`） |
| `CAgent`（`0x6fb79ea5`） | 无 xref | 只有类型名字符串，无已确认的引用点 |

**英雄复活 `FUN_6f678380`**（438 字节，`__thiscall`，含 `"CUnit::ReviveHero"` 字符串引用 `0x6f6784ba`）：

- 门槛：`param_1[0xc] >> 0x18` 落在 `'A'`–`'Z'`（即单位类型 id 首字符为大写字母，推断为英雄单位 id 形如 `'Hpal'` 等四字符码的首字节检查），且 `param_1[0x17]` 的 `0x40000000` 位为 0、`0x100` 位为 1。
- 流程：清两个状态槽（`FUN_6f3f82a0/8240` + `FUN_6f02f230`），取虚函数 `+0xec` 的返回值（推断为单位 id/等级），检查资源（`FUN_6f662960` + `FUN_6f247d20` + `FUN_6f029ec0` 比较），调 `FUN_6f677ef0`（复活/放置本体，见下），`param_3 != 0` 时再调 `FUN_6f3d9b00`（542 字节，英雄技能相关，身份未验证）与 `FUN_6f35a010(0,1,0)`。
- 函数本体身份是“英雄复活”是**推断**（字符串引用 + 门槛检查 + `FUN_6f677ef0` 调用链）；字符串本身出现在函数内数据引用处，未确认是日志标签还是断言。

**单位（重）放置 `FUN_6f677ef0`**（598 字节，`__thiscall`，7 个调用者）：

- 若虚函数 `+0xec` 返回值与 `param_2` 不同，调 `FUN_6f67b5b0(param_2, 1, param_6)`（1298 字节，疑似单位创建/替换大函数，未反编译）。
- 按 `param_5` 的位：`& 2` → `FUN_6f23ddd0(obj+0xc, 1)`；`& 4` → `FUN_6f245300(obj+0xc, 1)`；`& 0x100` → 置 `+0x19 |= 0x300`；`& 0x400` → 更新 `FUN_6f039860()` 计时与 `+0x248` 最大值（推断为复活计时记录）。
- 置/清标志：`+0x19 |= 0x12`，`+0x17 &= ~0x100`，`+0x17 &= ~0x2c00`，`+0x17 &= ~4`；调放置虚函数 `+0x80`、`+0xa0(1)`、`+0x8c(4, 0x200, 1)`；位置钳制：`FUN_6f0528b0` 取坐标，绝对值与 `DAT_6fb59398` / `DAT_6fb593ec` 比较后钳制，再调虚函数 `+0x130` / `+0x124` 写入。
- 调用者除复活路径 `FUN_6f678380` 外还有 `FUN_6f3dac10`、`FUN_6f3e4a20`、`FUN_6f3f0360`、`FUN_6f4ab820`、`FUN_6f4c8fa0`、`FUN_6f4f1140`——因此它是通用的单位放置/重生辅助，不只服务复活（推断）。

**CAbility 特效取 art**（`FUN_6f3f7980`，69 字节，含 `"CAbility::GetEffectArt"` 字符串引用）：从全局表（`FUN_6f322c30` / `FUN_6f3fac30` 取到的对象 `+0xd4` 为数量、`+0xd8` 为指针数组）按 `param_1` 取下标（越界钳到 `count-1`）返回一个 dword——即技能特效 art 是集中式索引表（推断）。`FUN_6f3fac30` 与 `FUN_6f322c30` 的身份未验证。

## 5. 区域查询：空间枚举是伤害与技能的公共底座

**`FUN_6f661060`**（2957 字节，本轮未反编译）：调用者约 130 处，遍布 `0x6f3exxxx`（技能簇）、`0x6f4exxxx`（技能/buff 簇）、`0x6f5exxxx`、`0x6f8axxxx`（AI 簇）、`FUN_6f6b19e0`（AoE 伤害）、`FUN_6f6510a0` 等。结合它在 AoE 伤害路径中接收位置/半径参数、并被大量技能函数调用，判定为**区域单位枚举**（推断：函数名与参数未验证，只确认调用模式与调用者分布）。

**`FUN_6f633a40`**（812 字节，CWidget 区域查询构造器）：

- 解析一段查询描述（opcode 表，`param_2` 指向的 dword 序列，`switch (op - 0x20)`）：
  - `0x20/0x21`：置过滤位 `puVar4[3]`；
  - `0x22/0x24`：取 `local_10`（推断为 x/y 或半径）；
  - `0x23/0x25`：同上另加 `local_c = 1`；
  - `0x26`：`puVar4[4] = …`；`0x27`：`puVar4[2] = …`（推断为所有者/类型过滤）；
  - `0x28`：按 `local_8` 与 `FUN_6f6a4440` / `FUN_6f8a7270` / `FUN_6f667470` / `FUN_6f63a8d0` / `FUN_6f6372b0` 返回值的比较，给 `uVar10` 置不同的掩码位（`0xfb0000` / `0xef0000` / `0xfd0000` / `0xf70000` / `0xf10000`）——推断为单位类别掩码；
  - `0x29/0x2a`：置位 `0x200/0x400` 并记录附加参数（`puVar4[8..0xb]`）。
- 按过滤位挂接过滤器链（`LAB_6f6351d0/2b0/2c0/2d0/2f0/1e0`，`FUN_6f6354a0` 分配节点），最后按 `bVar2/bVar3` 分派：`FUN_6f03ca30`（半径查询）或 `FUN_6f02f3c0`（矩形查询），否则 `FUN_6f02f230`（直接执行）。
- 查询描述 opcode 的具体含义、掩码位的类别对应关系**未验证**。

## 6. JASS native 注册表：脚本侧的模拟接口

`FUN_6f1e9a50`（23,346 字节，仅 2 个被调函数）是 JASS native 函数签名注册表：反复调 `FUN_6f7e3710(签名)` 注册数百个 native 的类型签名。类型编码：`H`=句柄、`I`=整数、`R`=实数、`B`=布尔、`S`=字符串、`C`=代码、`V`=无返回值。已确认与模拟直接相关的签名（字符串为证）：

| 签名 | 对应 native（按 common.j 惯例，推断） |
|---|---|
| `(Hunit;Hwidget;RBBHattacktype;Hdamagetype;Hweapontype;)B` | `UnitDamageTarget`（目标伤害） |
| `(Hunit;RRRRRBBHattacktype;Hdamagetype;Hweapontype;)B` | `UnitDamagePoint`（点伤害） |
| `(Hunit;Hunitstate;)R`、`(Hunit;Hunitstate;R)V` | `GetUnitState` / `SetUnitState` |
| `(Hunit;I)I`、`(Hunit;I)V`、`(Hunit;IB)V` 等 | 单位等级/技能等级读写（`DecUnitAbilityLevel` / `IncUnitAbilityLevel` 字符串同表出现） |
| `(Hunit;S)B`、`(Hunit;SRR)B`、`(Hunit;SHlocation;)B` 等 | `IssueOrder` 系列（点/目标/即时命令） |
| `(Hunit;Hitem;)B`、`(Hunit;I)Hitem;` 等 | 物品栏操作 |
| `(Hunit;Hplayer;B)V`、`(Hunit;Hplayercolor;)V` | 所有权/颜色 |
| `(Htrigger;Hunit;Hunitevent;)Hevent;` 等 | 单位事件注册（`CUnitDamagedEventData` / `CUnitDeathEventData` 等 RTTI 与之对应） |

另有一张表 `FUN_6f6f5d00`，同样引用 `DecUnitAbilityLevel` / `IncUnitAbilityLevel` / 伤害签名字符串，推断为第二张 native 表（Blizzard.j，推断，未反编译）。native 名字符串与签名在注册表内的配对方式、以及签名到实现函数的映射**未验证**（注册函数 `FUN_6f7e3710` 只收签名参数，名字在另一张表里）。

## 7. 关键函数地址表

| 地址 | 大小 | 角色 |
|---|---|---|
| `0x6f30bac0` | 1530 | 回合应用（S2 已验证；本阶段确认其分发的薄壳结构） |
| `0x6f2fdbd0` 等 14 个 | 各 185 | 回合命令解包薄壳（命令 0x10/0x16/0x17/0x19/0x1a/0x1b/0x1c/0x20/0x21/0x22/0x23/0x2f/0x30/0x31） |
| `0x6f30ba60` | 90 | CDataStore 虚读 + 调 `FUN_6f30b940` |
| `0x6f30b940` | 280 | 命令对象构建 + 入队分派（0x1e/0x1f 时更新 `+0x225c`） |
| `0x6f30fbb0` | 94 | 命令对象分配填充（24 字节，字段见 §2） |
| `0x6f30c4a0` | 112 | 命令对象内存分配（`FUN_6f78a4a0` 池分配，`0x18` 字节） |
| `0x6f30b920` | 19 | 入队包装 → `FUN_6f30ce40` |
| `0x6f30ce40` | 107 | 链表锚点查找 + 调 `FUN_6f30ceb0` + 命令计数 +1 |
| `0x6f30ceb0` | 160 | 双向链表拼接（命令队列插入） |
| `0x6f30b8c0` | 91 | 本地命令提交入口（调用者 `FUN_6f30e540`） |
| `0x6f306fd0` | 94 | 本地命令提交入口，命令号 +0x40（调用者 `FUN_6f30d640`） |
| `0x6f04e1d0` / `0x6f04dfa0` | 81 / 80 | CDataStore 写 dword / 写 byte |
| `0x6f83f8e0` | 118 | CDataStore 缓冲区分配/重分配 |
| `0x6f83ee60` / `0x6f83f010` / `0x6f83efd0` | 各 14 | 单 dword 命令读取器 |
| `0x6f83f060` / `0x6f83f090` / `0x6f83f380` | 40 / 50 / 91 | 多字段 / 变长列表命令读取器 |
| `0x6f6b19e0` | 1099 | AoE 伤害应用（枚举 → 过滤 → 单体伤害） |
| `0x6f6af530` | 378 | 单体伤害/治疗应用（虚函数 0x120 伤害 / 0x128 治疗） |
| `0x6f6af6c0` | 798 | 伤害列表构建（WidgetDamageData 追加，三分类 0/1/2） |
| `0x6f6b81c0` | 153 | WidgetDamageData 向量扩容（12 字节/条） |
| `0x6f6aeb00` | 23 | WidgetDamageData 向量释放 |
| `0x6f661060` | 2957 | 区域枚举（约 130 个调用者，未反编译） |
| `0x6f633a40` | 812 | CWidget 区域查询构造器（opcode 0x20–0x2b） |
| `0x6f678380` | 438 | 英雄复活路径（推断，含 `"CUnit::ReviveHero"` 字符串） |
| `0x6f677ef0` | 598 | 单位（重）放置/重生辅助（7 个调用者） |
| `0x6f3f7980` | 69 | CAbility 特效 art 索引表读取 |
| `0x6f3f14b0` / `0x6f511480` / `0x6f8a30d0` | 104 / 97 / 97 | `dynamic_cast` 辅助：CAbility / CBuff / CUnitAI |
| `0x6f656210` / `0x6f3e3880` | 23 / 23 | type_info 静态注册桩：CUnit / CAbility |
| `0x6f1e9a50` | 23346 | JASS native 签名注册表（common.j，推断） |
| `0x6f6f5d00` | 未测 | 第二张 JASS native 表（推断为 Blizzard.j，未反编译） |
| `0x6f30db70` | 760 | 比赛结果数据打包（`FUN_6f3123e0` 的游戏结束路径；非模拟主线，顺带确认） |

## 8. 数据结构 / 消息格式

**回合命令包 → 命令对象**：回合包内每条命令先由 `FUN_6f83exxx` 读取器按命令号读出字段（dword/byte/变长列表三种形态）进栈上 CDataStore，再由 `FUN_6f30ba60` 的虚函数 `+0x28` 读出三个值，`FUN_6f30b940` 把（命令号，值1，值2，值3，标志）装进 24 字节命令对象（字段见 §2），挂进会话的命令链表。命令号 < 0x40 的走网络回合路径、≥ 0x40 的走本地提交路径（`FUN_6f306fd0` 的 +0x40 偏移为证）——两条路径最终汇合于 `FUN_6f30b940`。

**伤害数据**：`WidgetDamageData` 12 字节/条（目标指针，float 量，类别 0/1/2）；AoE 伤害先枚举区域内 widget（`FUN_6f661060` / `FUN_6f633a40`），再逐条过回调过滤器、调 `FUN_6f6af530`；单体伤害以 `DAT_6fbb81d4` 为阈值分流伤害/治疗，分别走目标虚函数 `+0x120` / `+0x128`。

**类层次**（RTTI 证据）：`CAgent` → `CWidget` → `CUnit`（`CUnit` 派生 `CUnitAI` 相关类与大量事件数据类型 `CUnit*EventData`）；`CAbility` 492 个派生类、`CBuff` 226 个派生类——技能与 buff 是以“每技能一个类”的方式实现的（类名即技能名，如 `CAbilityDeathCoil`、`CBuffPoisonDamageStackDoT`）。继承链的虚表未直接验证（Ghidra 符号表里找不到 `CUnit::vftable` / `??_7CUnit@@6B@` 形式的符号名），只确认了 RTTI 名与 `dynamic_cast` 辅助的存在。

## 9. 与其它阶段的关系

- **S0 帧同步**：命令对象在 25 ms 步进内由 `FUN_6f30bac0` 从 TurnsSync 回合包解出；命令入队顺序即确定性执行顺序。七子系统校验和（S0）覆盖的模拟状态，主要就是本阶段的单位/技能状态。
- **S1 网络层**：回合包格式（`FUN_6f83exxx` 读取器）是 S1 回合包解析的下半段；`FUN_6f30b940` 中命令号 0x1e/0x1f 更新 `+0x225c` 与 S1 的回合游标相关。
- **S2 主循环**：帧步进 `FUN_6f30ff00` → `FUN_6f310f20` → `FUN_6f30bac0` 是命令进入模拟的唯一已确认路径；本地命令提交入口 `FUN_6f30b8c0` / `FUN_6f306fd0` 的调用者 `FUN_6f30e540` / `FUN_6f30d640` 尚未追踪到 UI/输入侧（留待 S6）。
- **S4 JASS2 虚拟机**：`FUN_6f1e9a50` 的 native 签名表是脚本调用模拟的接口层；`UnitDamageTarget` / `UnitDamagePoint` / `IssueOrder` 等 native 的实现函数尚未定位，留待 S4。
- **S5 渲染**：`CAbility::GetEffectArt`（`FUN_6f3f7980`）与伤害特效（`FUN_6f677ef0` 内的 `FUN_6f46f180` 等）是模拟 → 渲染的 art 引用点；特效实际挂接留待 S5。
- **S11 随机数与确定性**：伤害列表的三分类（0/1/2）与 `FUN_6f6af6c0` 的最大值选取是否涉 RNG 未验证；命令队列的消费顺序是确定性的关键，消费函数未找到，留待 S11 交叉验证。

## 10. 未验证点

1. 命令队列的消费（drain）函数未找到：只确认入队链（`FUN_6f30b940` → `FUN_6f30ce40` → `FUN_6f30ceb0`），谁在何时从链表取命令并执行成单位行为（移动/攻击/施法）未验证。
2. 各命令号（0x10–0x31）的具体语义未验证；命令号与 S0 事件号的对应关系未验证。
3. `FUN_6f661060`（2957 字节，约 130 个调用者）未反编译；区域枚举的参数与返回值未验证。
4. 伤害阈值 `DAT_6fbb81d4` 的数值、伤害/治疗虚函数 `+0x120` / `+0x128` 的目标函数、伤害三分类 0/1/2 的含义未验证。
5. `FUN_6f678380` 是否就是 `CUnit::ReviveHero` 本体未验证（只确认字符串引用与复活门槛逻辑）；`FUN_6f677ef0` 的 `param_5` 各标志位含义未完全验证。
6. `CUnit` / `CAbility` / `CBuff` 的虚表地址与继承链未验证（符号表无 `vftable` 命名符号）。
7. JASS native 签名到实现函数的映射未验证；`FUN_6f6f5d00`（第二张 native 表）未反编译。
8. `FUN_6f30b940` 中命令号 0x40–0xfe 分支的 `FUN_6f791c50`（23 字节，仅调 `Ordinal_403` 日志）与 < 0x40 分支的 `FUN_6f83ff00` 的确切语义未验证。
9. 技能施放流程（`CAbility` 派生类如何被触发、冷却/法力消耗、buff 挂接时序）未触及——本轮只确认了类族规模与特效 art 索引表；施法主流程留待与 S4 交叉。
10. 单位移动/寻路、攻击间隔/弹道（`CAbility::GetMissileArt/Speed` 字符串存在但未追踪）未验证。
