# S6 — UI / Frame 系统

- 状态：已验证 2026-11-05
- 证据：`~/workspace/warcraf_ghidra/ui_dump.txt`（34 个 Frame/UI 字符串定位 + 14 个函数反编译）、`ui_dump2.txt`（字符串/ RTTI 引用表 + 20 个函数反编译）、`ui_dump3.txt`（20 个函数反编译）、`ui_dump4.txt`（10 个函数反编译）
- 上承：S0 帧同步、S1 网络层、S2 主循环（GameMain → 引擎初始化 → 本阶段 `FUN_6f0280e0`）、S3 模拟核心、S4 JASS2（`FUN_6f1e9a50` native 注册表在 UI 初始化内被调用）、S5 Gx 渲染（Frame 的绘制最终走 `CGxDevice` 图元批量）

## 1. 总述

UI 是 **Frame 树 + FrameDef 模板 + 布局（Layout）+ 渲染批量** 四层结构：

- **Frame 树**：所有 UI 元素都是 `CFrame` 派生对象，挂在一棵以 `CScreenFrame`（全屏根）为顶的树上；`CFrame` 派生自 `CLayer`（层，带 z 顺序与父子关系），`CSpriteFrame` / `CControl` 是中间层（见 §2）。
- **FrameDef 模板**：`.fdf` 文本定义的控件模板（`UI\FrameDef\FrameDef.toc` 索引 + 各 `.fdf` 文件），启动时由 `FUN_6f0280e0` 载入并建成名字→对象的哈希表；`CFrame` 构造时带一个 FrameDef 参数，模板里的属性（纹理、文字、锚点）据此填充。
- **布局**：每个 Frame 有一组锚点（`CFramePoint`，绝对/相对两种），锚点变动时把 Frame 登记进一个全局“待布局”链表（`FUN_6f0bd630`），由 `CLayoutFrame` 的 `FRAMENODE` 链表做依赖排序后重算矩形。
- **渲染**：Frame 的绘制是虚函数分发（0 直接调用者是常态）；轻量 UI（`CSimpleFrame` / `CSimpleTop`）走独立的渲染批量（`RENDERBATCHENTRY` 数组），与重型 Frame 树并行存在。
- **事件/输入**：按钮等控件是 `CButtonFrame`；消息行是 `CMessageFrame`；聊天输入是 `CEditBox` 复合控件。帧事件名（如 `OnClick`）在 1.27a 的二进制里几乎不以字符串形式出现——**推断**：事件处理函数名写在 `.fdf` 模板里，由 JASS 触发器按名字绑定（S4 的 native 注册表是另一条线），本轮未在引擎侧定位到名字分发表（见 §9）。

源码目录（残留路径字符串为准）：`engine\source\frame\`（`CFrame.cpp`、`CBackdropFrame.cpp`、`CTextFrame.cpp`、`CButtonFrame.cpp`、`CModelFrame.cpp`、`CSpriteFrame.cpp`、`CMessageFrame.cpp`、`CHighlightFrame.cpp`、`CTextButtonFrame.cpp`、`CSimpleFrame.cpp`、`CSimpleMessageFrame.cpp`、`CScreenFrame.cpp`、`CLayoutFrame.cpp`、`CFramePoint.cpp`、`FrameDef.cpp`、`CChatDisplay.cpp`、`CEditBox.cpp`、`CHighlightFrame.h`、`CSimpleMessageFrame.h`），以及 `war3\source\Glue\`（`CTimerTextFrame.h`、`BackdropFrameHandlers.cpp`）和 `war3\source\ui\`。

## 2. 类层次（由构造函数链验证）

```
CLayer                  FUN_6f097050（395 字节）
 └─ CFrame              FUN_6f0a0b80（355 字节，56 个调用者）
     ├─ CSpriteFrame   FUN_6f0a2c20（225 字节）
     │   └─ CControl   FUN_6f0bddc0（259 字节，21 个调用者）
     │       ├─ CTextFrame      FUN_6f0a9000（356 字节，0x240 字节对象）
     │       ├─ CButtonFrame    FUN_6f0aaac0（116 字节，0x1e4 字节对象）
     │       ├─ CEditBox        FUN_6f0ae290（1480 字节，复合控件）
     │       └─ CChatDisplay    FUN_6f0b4350（516 字节，0x224 字节对象）
     ├─ CMessageFrame  FUN_6f0a71b0（359 字节）
     ├─ CScrollFrame   FUN_6f0a3c20（121 字节，0x180 字节对象）
     ├─ CFloatingFrame FUN_6f0c98a0（180 字节，0x174 字节对象）
     ├─ CCursorFrame   FUN_6f0c9630（134 字节，0x1b0 字节对象）
     └─ CBackdropFrame （渲染函数 FUN_6f0a4cd0，5417 字节；构造器未单独定位）
CScreenFrame            FUN_6f0ca0f0 / FUN_6f0ca020（子 Frame 懒创建，见 §5）
CSimpleFrame / CSimpleTop（轻量分支，析构 FUN_6f0b8f30 / FUN_6f0b6d30）
CModelFrame             （析构 FUN_6f0a3ee0，模型指针在 +0x170）
```

继承的证据是构造函数的调用链：每个派生构造先调基类构造再写自己的虚表，且**每个对象写两次虚表**（`[0]` 与 `[0x2d]`，即对象头与 +0xb4 处各一个虚表指针——CFrame 在 +0xb4 处另有一个子对象/第二虚表，见 §3）。析构时同样先把虚表重置回本类再调基类析构（`CFrame` 析构 `FUN_6f0a0f00`、 `CMessageFrame` 析构 `FUN_6f0a75e0`、`CScreenFrame` 析构 `FUN_6f0c9d20`、`CSimpleFrame` 析构 `FUN_6f0b8f30`、`CSimpleTop` 析构 `FUN_6f0b6d30`）。

### 2.1 `CLayer` 构造 `FUN_6f097050`

- 设 `CLayer::vftable`；两个 `TSLinkedNode` 哨兵（`+6` 与 `+0x1e`，自指 + 取反自指——Tempest 链表的标准哨兵写法）。
- `[9] = param_3`（层名/标识，**未验证**具体含义）；`[0x2a] = param_4`；`+0x2b` 字节 = `0xff`。
- `[0x11] = [0x1a] = this`（自引用槽）。
- 若 `param_2`（父层）`+0xb0` 的低 2 位非零，`[0x2c]` 的相应位被置位（`(uVar2|6)*2` 的位运算，**具体位含义未验证**）。
- 调 `FUN_6f09d2b0(param_2, 1, 1)`（挂到父层，见 §2.2）与 `FUN_6f096e00()`（构造一个 `TSHashTable<SHORTCUTREGNODE, HASHKEY_NONE>`——**快捷键注册表**，初始 12 桶，`[1] = 0xc`，`[9] = 0xffffffff`）。
- 另：`CLayer` 内含 `TSHashTable<SHORTCUTREGNODE>` 说明每个层都可注册键盘快捷键（**推断**，未验证快捷键的分发路径）。

### 2.2 `CLayer::SetParent` `FUN_6f09d2b0`

- `+0x20` 是带引用计数的父层指针（旧父 `ref--`、为 0 时调虚表 `[0]` 删除；新父 `ref++`）。
- 新父非空时调 `FUN_6f09ca50(*(parent+0xac), 0)` 与 `FUN_6f098390(this)`（**推断**为可见性/层级重算），并把父层 `+0xb0` 的低 2 位同步进本层 `+0xb0`（bit 1），把 `+0xc` 的 bit 6（`0x40`）按父层 `+0xc` 的 bit 3 与 `FUN_6f098780(8)` 的结果决定置位或清零。
- 父层为空时调 `FUN_6f0984e0()` 并置全局 `DAT_6fbb9e18 = 0`。

### 2.3 `CFrame` 构造 `FUN_6f0a0b80`（56 个调用者）

- 先调 `FUN_6f097050(param_2, param_3, param_4)`（`CLayer` 构造，参数直传）。
- 设 `CFrame::vftable`（`[0]` 与 `[0x2d]`）；`[0x47..0x59]` 清零；两个 `TSLinkedNode` 哨兵（`+0x52`、`+0x55`）。
- 若 `param_1[8] != 0`（父 Frame 的某个字段），调 `FUN_6f0a26b0(*(param_1[8] + 0x114))`（见 §6 的更新传播）。

### 2.4 `CFrame` 析构 `FUN_6f0a0f00`（334 字节，221 个调用者）

- 虚表重置回 `CFrame::vftable`（两处）；释放名字字符串（`+0x58`，`"CFrame.cpp", 0x10d`）；从两个 `TSLinkedNode` 链表摘除（`+0x55` / `+0x52`，带 `0x154` / `0x148` 的偏移算术——Tempest 侵入式链表的标准摘除）；释放引用计数对象 `+0x48`（`ref--`，为 0 调虚表 `[0]` 删除）。
- 调用者众多是因为它是所有 Frame 析构链的终点（`CMessageFrame` / `CScreenFrame` / `CSimpleTop` 析构都以它收尾）。

### 2.5 `CSpriteFrame` / `CControl` 构造

- `FUN_6f0a2c20`：调 `CFrame` 构造；设 `CSpriteFrame::vftable`；`[0x5a..0x5f]` 清零；调 `FUN_6f0ca900(0)`；`[0x67] = 1.0f`（`0x3f800000`）、`[0x6a] = 0.25f`（`0x3f000000`）、`[0x6b] = 1`（**推断**为精灵的默认缩放/透明度类字段，未逐一验证）。
- `FUN_6f0bddc0`：调 `CSpriteFrame` 构造；设 `CControl::vftable`；`[0x6c..0x78]` 清零，`[0x78] = 1`；调 `FUN_6f0be2d0(1)`、`FUN_6f0be300(0)`、`FUN_6f0a1290(2)`（三个 22–46 字节小函数，**身份未验证**，疑似控件状态初始化）。

## 3. `CFrame` 对象布局（偏移以 dword 计，除非注明）

| 偏移 | 内容（证据） |
|---|---|
| +0x00 | 虚表指针（`CFrame::vftable` 等） |
| +0x03 / +0x04 | 标志位（`CMessageFrame` 构造里 `\|= 2`；`CScrollFrame` 构造里 `\|= 8`；`CCursorFrame` / `CFloatingFrame` 构造里 `\|= 2`） |
| +0x08 | 父 Frame 相关指针（构造时若非空触发 `FUN_6f0a26b0`） |
| +0x0c / +0x10 | 标志字段（`FUN_6f09d2b0` 读写） |
| +0x2d | 第二虚表指针（与 +0x00 相同值；`+0xb4` 处的子对象头） |
| +0x47–0x4f | 清零区 |
| +0x48 | 引用计数对象指针（析构时 `ref--`） |
| +0x50 | 非零时析构调 `FUN_6f04f1a0` |
| +0x52 / +0x55 | 两个 `TSLinkedNode` 侵入式链表节点（+0x154 / +0x148 偏移算术） |
| +0x58 | 名字字符串指针（析构释放，`"CFrame.cpp", 0x10d`） |
| +0x5a–0x6c | 派生类字段区（`CMessageFrame` 的 5 条消息链表哨兵在 +0x5f–0x6c；`CControl` 的清零区在 +0x6c–0x78） |
| +0x6c（字节） | `CBackdropFrame` 的九宫格标志位（bit 0x10 / 0x20 等，见 §4） |
| +0xb0 | 层标志（`CLayer::SetParent` 同步位） |
| +0xb4 | 子对象区起点（第二虚表所在对象的字段起点；`FUN_6f0bd750` 的锚点数组即从此处附近开始，见 §7） |

对象大小（`operator new` 参数为准）：`CTextFrame` 0x240、`CButtonFrame` 0x1e4、`CScrollFrame` 0x180、`CFloatingFrame` 0x174、`CCursorFrame` 0x1b0、`CChatDisplay` 0x224、`CHighlightFrame` 0x2a4、`CEditBox` 内嵌 `CScrollFrame` 0x180。`FrameDef` 对象两种大小：0x180（`FUN_6f08e950` → `FUN_6f0a3c20`，即 `CScrollFrame` 尺寸——**注意**：该分配实际是 `CScrollFrame`，不是 FrameDef 本体）与 0x224（`FUN_6f08f0b0` → `FUN_6f0b4350`，即 `CChatDisplay`）。

## 4. `CBackdropFrame`：九宫格渲染

- 渲染/更新函数 `FUN_6f0a4cd0`（5417 字节，0 调用者——**经虚表调用**；字符串证据：邻近静态数据区含 `"CBackdropFrame::OnUpdateModels()"` 与 `"CBackdropFrame"`，**函数名本身是推断**，只能确定它是 `CBackdropFrame` 的大号更新/渲染函数）。
- 流程：`param_1[0x76] = 0`；调 `FUN_6f0a4900` 取纹理槽表（见下）；调虚表 `+0x10` 取矩形（4 个 float：`local_108` 等）；按 `+0x6e` / `+0x6f`（边框宽高，0 时用 `+0x6e` 兜底）与 `DAT_6f95a590`（异或掩码常量，疑似浮点混淆/定点转换）算 UV；调 `FUN_6f12a400()`（取当前纹理/材质，**未验证**）再调 `FUN_6f12a6a0(4 顶点, …)` 提交一个四边形图元；随后按 `+0x6c` 字节的标志位逐个处理边角/边条切片：bit `0x10` → 第二片（UV 0.5–0.625 段）、bit `0x20` → 第三片……每片先调 `FUN_6f0a65e0` 算顶点再调 `FUN_6f12a6a0` 提交。
- `FUN_6f0a4900`（417 字节，仅被 `FUN_6f0a4cd0` 调用）：按 `+0x1b0` 字节的 8 个标志位（0x10 / 0x20 / 0x40 / 0x80 / 1 / 2 / 4 / 8）从 `+0x178` / `+0x180`（纹理指针数组）与 `+0x194` / `+0x19c`（坐标数组）里挑出最多 9 个槽位，返回槽数——**九宫格（3×3）纹理选择器**。`+0x17c` 非零时交换取槽方向（`bVar3` 分支，**推断**为翻转/镜像标志）。
- `FUN_6f0a65e0`（179 字节）：按 UV 矩形与纹理尺寸算 4 个顶点的 12 个 float（位置 + UV），调 `FUN_6f197840` / `FUN_6f0e3860` / `FUN_6f0d1270`（Gx 侧辅助，**未验证**具体身份）。
- `CHighlightFrame` 的全局高亮缓冲：`FUN_6f0a6f00`（418 字节，0 调用者，经虚表调用）：`param_1==0` 时按 `w*h*4` 分配全局 `DAT_6fbb9eb0` 并按行列填色（边框加粗逻辑：首尾行/列复制 `param_5` 的颜色）；`param_1==1` 时返回行宽与缓冲指针；`param_1==2` 时释放。**推断**：鼠标悬停高亮的纹理生成器（名字与 `CHighlightFrame.cpp` 的行号 0x1f/0x24 相符）。

## 5. `CScreenFrame`：全屏根与子 Frame 懒创建

- `FUN_6f0ca0f0`（222 字节，9 个调用者）：按索引 `param_2` 懒创建子 Frame：槽位 `+0x170 + param_2*4` 为空且 `param_3 != 0` 时，`new` 0x174 字节 → `FUN_6f0c98a0`（`CFloatingFrame` 构造，类型参数取自常量表 `DAT_6f95d420[param_2]`）；引用计数 `+4`，旧对象 `ref--`、为 0 时虚表 `[0]` 删除；随后调 `FUN_6f0bd750(this+0xb4, 1)`（锚点初始化）。
- 常量表 `DAT_6f95d420` 的前 9 个 dword（文件内直接读取）：`0x251c`（9500）、`0x2422`（9250）、`0x2ee`（750）、`0x1f4`（500）、`0`、`0`、`0x3f800000`（1.0f）、`0x3f800000`（1.0f）、`0x7f800000`（+inf）——**含义未验证**（可能是 FrameDef ID 或尺寸常量，不是字符串指针）。
- `FUN_6f0ca020`（195 字节，26 个调用者）：同一模式的单槽版本——槽位 `+0x16c`，`new` 0x1b0 字节 → `FUN_6f0c9630`（`CCursorFrame` 构造，`FUN_6f0a2c20(parent, 0, 10000)`——10000 **推断**为光标层的优先级/层级值）。调用者遍布游戏 UI 代码（`6f2bd430`、`6f1c1f20`、`6f1cabf0`、`6f28db50` 等 26 处），是游戏内取光标/浮动 Frame 的常用入口。
- `FUN_6f0c9d20`（`CScreenFrame` 析构）：销毁 `+0x5c` 起的 4 元数组（`eh_vector_destructor_iterator`，步长 4），释放引用计数对象 `+0x5b` / `+0x5a`，再调 `CFrame` 析构。

## 6. 更新传播与布局失效

- `FUN_6f0a26b0`（`CFrame` 的时间驱动更新，4 个调用者）：参数为 float（**推断**为帧时间 dt）；若 `|param_2 - *(this+0x60)|`（与 `DAT_6f95a890` 掩码后的绝对值）`>= DAT_6f95a884`（阈值常量），调 `FUN_6f0bd7e0(param_2)`，再遍历 `+(-0x98)` 处的子链表，对每个子对象调其 `+0xb4` 子对象的虚表 `+0x14`——**逐子 Frame 的虚函数更新调用**（`+0x14` 是哪个虚方法未验证，**推断**为 `OnUpdate` 类方法）。
- `FUN_6f0bd630`（布局登记，调用者众多，含大量 `6f28xxxx` / `6f2axxxx` / `6f3bxxxx` 的游戏 UI 函数）：把 Frame 挂进以 `PTR_LOOP_6fb6641c` 为哨兵的全局双向链表（节点地址 `DAT_6fb66418 + this`，即每 Frame 对象内嵌一个链表节点）；若 `param_2 != 0` 且虚表 `+4` 返回非零则直接返回（**推断**：已在链表中则跳过，避免重复登记）。这是“布局已脏，待重算”的登记函数（**推断**，函数名未验证）。
- `FUN_6f0bd520`（`CLayoutFrame::AddFrameNode`，117 字节？实际 109 字节的 `FUN_6f0bd750` 调它；`FUN_6f0bd520` 本体约 200+ 字节）：沿 `+0x40` / `+0x3c` 的 `FRAMENODE` 链表找 `param_2`（Frame 指针）；找不到就 `new` 一个 `CLayoutFrame::FRAMENODE`（RTTI 名 `?AUFRAMENODE@CLayoutFrame@@`），链首插入，`[3] = param_2`、`[4] = param_3`（标志）；找到则把 `param_3` 或进已有节点的 `+0x10`。
- `FUN_6f0bcca0`（锚点清理）：遍历 Frame 的锚点数组（个数 `+4`，数组 `+8`），对每个非空锚点调虚表 `+0xc`，非零时调 `FUN_6f0bdd20`，再调 `FUN_6f0ca5b0` 删除锚点并置空。

## 7. 锚点（`CFramePoint`）

- 两种 RTTI：`CFramePointAbsolute`（`?AVCFramePointAbsolute@@`，`6fb7b478`）与 `CFramePointRelative`（`?AVCFramePointRelative@@`，`6fb7b49c`）；基类 `CFramePoint`（`?AVCFramePoint@@`，`6fb7c438`）。
- `FUN_6f0ca670`（`CFramePointRelative` 工厂，3 个调用者）：`new`（`FUN_6f04d150`，RTTI 名 `CFramePointRelative`），设虚表，存 4 个参数（`[1..4]`：**推断**为相对目标 Frame、锚点枚举、x/y 偏移）。
- `FUN_6f0bd750`（锚点初始化，109 字节）：调 `FUN_6f0bcca0` 清旧锚点；在 `param_1+8` 与 `param_1+0x28` 各建一个 `CFramePointRelative(0,0)`；调 `FUN_6f0bd520(param_1, 0xf)`（登记布局节点，标志 0xf）；`param_3 != 0` 时调 `FUN_6f0bd630(0)`（登记待布局）。
- `FUN_6f0bd8a0`（`SetFramePoint`，117 字节，调用者众多）：替换槽位 `param_2` 的锚点——旧锚点非空时先调虚表 `+0xc` 检查、再 `FUN_6f0bdd20` 与 `FUN_6f0ca5b0` 删除；新建 `CFramePointRelative(param_5, param_6)` 存入 `param_1 + 8 + param_2*4`；再调 `FUN_6f0bd520(param_1, 0xf)` 与（`param_7 != 0` 时）`FUN_6f0bd630(0)`。
- `FUN_6f0ca5b0`（锚点删除析构，82 字节）：调虚表 `+0xc`（**推断**：判断是绝对还是相对锚点），两路都先调虚表 `[0](0)` 再调 `FUN_6f04d1f0(this, "CFramePoint.cpp", 0x28 或 0x3f)`（带行号的 `operator delete`）。

## 8. 文本、消息与聊天控件

### 8.1 `CTextFrame::SetText` `FUN_6f0aa130`（222 字节，148 个调用者）

- `param_2 == 0` → 清空（调 `FUN_6f0a94b0(1)`）；否则 `strlen(param_2)`：若缓冲 `+0x1e8` 为空或容量 `+0x1e4` 不足 → 重分配（`Ordinal_507`，`"CTextFrame.cpp", 0x221`）；否则原地拷贝（`Ordinal_501`，长度 `+1` 含结尾零）。
- 随后置脏标志 `+0x22c = +0x230 = +0x224 = 1`；调 `FUN_6f0a9490(0x100)`（**推断**：字体/纹理就绪检查，0x100 为标志位）；若未就绪或缓冲为空则直接返回，否则继续走字体纹理更新（后半段未完整提取，见未验证点）。
- `FUN_6f0a94b0`（166 字节）：按 `FUN_6f0a9490(0x100)` 的结果释放 `+0x1e8` 与 `+0x1ec`（字体纹理/字形缓存，`"CTextFrame.cpp", 0x158/0x15b/0x164`）。
- `FUN_6f0a9440`（79 字节）：分配 `strlen+1` 的缓冲并以 `0x2a`（`*`）填满——**密码/掩码文本的占位实现**（**推断**：输入框密码掩码用，函数名未验证）。
- `CTextFrame` 构造 `FUN_6f0a9000`：调 `CControl` 构造；设虚表；`[0x79..0x7b] = 0`；分配 8 字节块存 `+0x7c`；`[0x7d..0x7f] = -1`、`[0x80] = 0xff000000`（不透明黑，**推断**为默认文字颜色）、`[0x84] = 0x3c75c28f`（float 0.01）、`[0x85] = 0xbc75c28f`（float -0.01）、`[0x89] = [0x8b] = [0x8c] = 1`；调 `FUN_6f0be300(0x80)`（**未验证**，疑似默认字号 128？或字体句柄初始化）。

### 8.2 `CMessageFrame`（消息/聊天行）

- 构造 `FUN_6f0a71b0`：调 `FUN_6f0a0b80`（`CFrame` 构造）；设 `CMessageFrame::vftable`；分配两个 8 字节块存 `+0x5a` / `+0x5b`；初始化 5 个 `TSLinkedNode` 哨兵（`+0x5f`、`+0x62`、`+0x65`、`+0x68`、`+0x6b`，哨兵偏移值分别为 0x18 / 0x20 / 0x28 / 0x10 / 0x10）；`[0x5c] = 0x3ba3d70a`（float 0.0028…，**未验证**）；`[3] | [4] | 2`。
- `AddMessage` `FUN_6f0a7a80`（621 字节，2 个调用者）：`new` 0x38 字节消息节点（RTTI `__AUTEXTMSG__`，`6fb7d108`）；文本 `Ordinal_507` 复制（`"CMessageFrame.cpp", 0x142`）；`[1] = param_4`（颜色，**推断**）、`[2] = *param_5`（**未验证**）、`[4] = param_3`（float，持续时间）；`[5]` 置 bit 1（`| 2`，清 bit 0）；挂进两条链表（`+0x178` / `+0x184`，即构造时建的哨兵链表）；`param_6 != 0` 时按已有行的 bit 1 找插入位置（`FUN_6f0a80b0`）；`param_3 > 0` 时再 `new` 0x18 字节定时器节点（`__AUTIMERDATA__`，`6fb7d120`），挂进 `+0x1a8` / `+0x1ac` 链表（`[1] = 消息节点`、`[2] = param_3` 持续时间、`[3] = 0`）。
- 析构 `FUN_6f0a75e0`：释放 `[0x5a]` / `[0x5b]`；逐个摘除 5 条链表（`+0x5f`、`+0x62`、`+0x65`、`+0x68`、`+0x6b`）；调 `CFrame` 析构。
- 清理辅助 `FUN_6f0a77c0`：释放 `[0]` 的字符串，摘除 4 条链表（`+6/+8/+10/+12` 对）。

### 8.3 `CEditBox`（聊天输入框，复合控件）`FUN_6f0ae290`（1480 字节）

- 设 `CEditBox::vftable`；`[0x7c] = 0xffffff00`、`[0x81] = [0x83] = 1`、`[0x90] = 0x3f000000`（0.5f）。
- 内嵌一个 `CScrollFrame`（`[0x94]`，0x180 字节，`FUN_6f0a3c20`）与两个 `CTextFrame`（`[0x95]` / `[0x96]`，`FUN_6f0a9560` = `new CTextFrame`，0x240 字节）及一个 `CHighlightFrame`（`[0x99]`，0x2a4 字节，`FUN_6f0a66a0`），再一个 `CTextFrame`（`[0x97]`）；各子控件都 `|= 2` 标志位（`[0xc] | [0x10] | 2`）。
- 调 `FUN_6f0a93e0(0x3008)`、`FUN_6f0aa2b0(2)` / `(1)`、`FUN_6f0a6e70(2)`、`FUN_6f0a6e40(0x80c00000)`（**未验证**：疑似输入框的光标/选区/滚动参数），两次 `FUN_6f0bd8a0` 设锚点（槽位 1 与 7）。
- 另：`CEditBox` 的 RTTI 字符串在二进制里**不存在** `CEditBoxFrame` 这个名字——类名就是 `CEditBox`（`CEditBox.cpp`）。

### 8.4 `CChatDisplay` `FUN_6f0b4350`（516 字节）

- `new` 0x224 字节对象；内嵌一个 `CEditBox`（`[0x82]`，0x274 字节，`FUN_6f0ae290`）与一个 0x210 字节对象（`[0x81]`，`FUN_6f0ab630`，**未验证**身份）；`FUN_6f0b0130(0x400b0064, this, 1)` 与 `FUN_6f0ab340(0x40100064, this)`（**推断**：按 FrameDef 数字 ID 绑定模板/控件，0x400b0064 与 0x40100064 的含义未验证）；`[0x83] = [0x85] = 0x3c23d70a`（float 0.01）；对 `[0x81]` 的子对象三次调虚表 `+0x108`（**推断**：刷新/重布局调用）。

### 8.5 `CButtonFrame` 构造 `FUN_6f0aaac0`

- 调 `CControl` 构造；设 `CButtonFrame::vftable`；调 `FUN_6f056af0(0x40060064, 0x40060064, this)`（**推断**：注册按钮的两种状态 FrameDef ID，两个参数相同值 0x40060064，含义未验证）。

### 8.6 `CScrollFrame` 构造 `FUN_6f0a3c20`

- 调 `CFrame` 构造；设 `CScrollFrame::vftable`；`[0x5c] = [0x5d] = DAT_6f66378 ^ DAT_6f95a590`、`[0x5e] = [0x5f] = DAT_6f66378`（异或掩码常量，**未验证**）；`[3] | [4] | 8`。

## 9. FrameDef 模板系统

- **哈希表**：`FUN_6f06b030(name)` 查找、`FUN_6f0693f0(...)` 插入（S5 已验证的 `TSHashTable` 模式）；`FUN_6f090d60`：查不到就新建节点（`FUN_6f0693f0`），随后把 `param_2`（模板类型名字符串）`Ordinal_507` 复制存 `[7]`（`"FrameDef.cpp", 0x1b7`）；`FUN_6f090b00(name, buf, len)`：查 FrameDef，按 `[0x1c]` 的字符串字段 `Ordinal_501` 拷进调用方缓冲（**推断**：按名字取 FrameDef 的某个文本属性，如默认贴图路径）。
- **加载入口**：`FUN_6f090b70`（被 `FUN_6f0280e0` 调用）：调 `FUN_6f068300(0)` 两次、`FUN_6f067fa0(0)`、`FUN_6f066590(&DAT_6fbb9cfc, vftable_ptr)`（**推断**：初始化 FrameDef 注册表；各子函数本体未反编译）。
- **启动序列** `FUN_6f0280e0`（948 字节，UI/引擎初始化总入口，见 §10）内引用了 `"UI\FrameDef\FrameDef.toc"`（`6f0282bc` 处以 DATA 引用加载该字符串地址），并打开 `"FramedefErrors.log"`（`Ordinal_542` fopen / `Ordinal_541` fclose）记录模板错误。
- **模板文件**：`UI\FrameDef\FrameDef.toc`（索引）、`UI\war3skins.txt`（皮肤）、`UI\MiscData.txt` 与 `UI\MiscUI.txt`（杂项 UI 数据，`.data` 段 `6fb592fc` 处有指向 `UI\MiscData.txt` 的指针，**推断**为启动时加载的配置表指针）。
- **注册表**：`.data` 段 `6fb69810` 处有一张表：`[0] = "ConsoleUI"`、`[1] = 0x6f347d10`（函数指针，**推断**为 ConsoleUI 的工厂/构造）、`[2] = 0`、`[3] = "UpperButtonBarFrame"`（**推断**：父 Frame 名）——即“名字 → 工厂 → 父名”的 FrameDef 注册表项结构（**结构本身是推断**，仅指针与字符串位置已验证）。`6f97b994…` 处另有一张指向多个 `"GlueScreen…"` 字符串的指针表（**推断**：主菜单 Glue 屏的定义表）。
- **错误处理**：`FUN_6f6ca6b0`（316 字节）：读配置键 `"ErrorStringDefault"` 与 `"FrameDefDefault"`（`Ordinal_508`），任一为空则拼错误串（`FUN_6f6cae10` / `FUN_6f7020d0` / `FUN_6f090b00`）并调 `Ordinal_578`（**推断**：弹框显示）；`FUN_6f0222a0`（33 字节）：调 `FUN_6f090d50()`（→ `FUN_6f068300(0)`）后按 `param_1+8` / `param_1+4` 的标志决定是否调 `FUN_6f6d81c0()`（该函数内调 `FUN_6f18b240()` 检查与 `FUN_6f18af20(&LAB_6f6e8230, 0)`，**LAB_6f6e8230 指向 .text 段内地址而非字符串，含义未验证**）。

## 10. 启动链与每帧关系

```
GameMain 6f0297d0（S2）
  └─ … → FUN_6f0280e0（948 字节，UI/引擎初始化总入口）
       ├─ FUN_6f76e640 / thunk_FUN_6f04f210（引擎基础初始化）
       ├─ 语言：FUN_6f18ae80 / FUN_6f18b1d0 / FUN_6f18aff0("LANGID") → Ordinal_272
       ├─ FUN_6f04efb0() == 0 → new 0x14（"Engine.cpp", 0x1e1）→ FUN_6f0265c0 / FUN_6f04f010
       ├─ FUN_6f02a560 / FUN_6f02a260("Color") / FUN_6f02a2f0 ×3 / FUN_6f02a380 → FUN_6f0e3390（清屏色，S5 的 Gx 部分）
       ├─ Ordinal_542("FramedefErrors.log") 打开模板错误日志
       ├─ FUN_6f090b70()（FrameDef 注册表初始化，§9）
       ├─ FUN_6f1e9a50()（S4：JASS native 注册表）
       ├─ FUN_6f02fb10 / FUN_6f12a7c0 / FUN_6f32a2b0 / FUN_6f339df0 / FUN_6f339d80（==3 → FUN_6f33afa0）
       ├─ FUN_6f02a420(0x104 缓冲) → 失败则 Ordinal_465(0x57)；成功则：
       │    FUN_6f153e10 / FUN_6f175da0 ×2
       │    FUN_6f090b00 ×3（查 FrameDef 默认名，成功则 FUN_6f175af0）
       │    FUN_6f30e540 / FUN_6f09a430 / FUN_6f09c6e0 / FUN_6f18cdf0 / FUN_6f18ce30 / FUN_6f18cdd0
       │    FUN_6f704400 / FUN_6f026df0 / FUN_6f029710
       │    DAT_6fbb80d4 = FUN_6f0572e0()
       └─ Ordinal_541 关闭 FramedefErrors.log；FUN_6f04f2b0
```

- 每帧更新：`CFrame::Update` 类调用经 `FUN_6f0a26b0` 的时间阈值分支向子 Frame 传播（虚表 `+0x14`），但**引擎主循环（S2 的 `FUN_6f05f710`）到 UI 更新的完整调用链未验证**（未定位到主循环直接调用 UI 更新的边）。
- 渲染：Frame 的绘制经虚表分发到 `CBackdropFrame::OnUpdateModels` 疑似函数等，最终调 `FUN_6f12a6a0`（图元提交）进 S5 的 `CGxDevice` 批量；`CSimpleTop` / `CSimpleFrame` 的轻量路径用 `RENDERBATCHENTRY` 数组（0x18 字节/条，`FUN_6f0b6d30` 析构时释放）。

## 11. 关键函数地址表

| 地址 | 函数 | 作用（证据） |
|---|---|---|
| 6f0280e0 | FUN_6f0280e0 | UI/引擎初始化总入口：语言、清屏色、FramedefErrors.log、FrameDef 注册、JASS native 注册、FrameDef 默认名加载 |
| 6f090b70 | FUN_6f090b70 | FrameDef 注册表初始化（调 FUN_6f068300 / FUN_6f067fa0 / FUN_6f066590） |
| 6f090d60 | FUN_6f090d60 | FrameDef 哈希表取/建节点，`[7]` = 模板类型名字符串 |
| 6f090b00 | FUN_6f090b00 | 按名字取 FrameDef 的 `[0x1c]` 文本字段并拷贝 |
| 6f097050 | FUN_6f097050 | `CLayer` 构造：虚表、两 TSLinkedNode 哨兵、快捷键哈希表、挂父层 |
| 6f09d2b0 | FUN_6f09d2b0 | `CLayer::SetParent`：引用计数换父、标志位同步 |
| 6f096e00 | FUN_6f096e00 | `TSHashTable<SHORTCUTREGNODE>` 构造（快捷键注册表，12 桶） |
| 6f0a0b80 | FUN_6f0a0b80 | `CFrame` 构造：调 `CLayer` 构造、设双虚表、两链表哨兵 |
| 6f0a0f00 | FUN_6f0a0f00 | `CFrame` 析构：虚表复位、释名字、摘链表、释 `+0x48` 引用计数对象 |
| 6f0a2c20 | FUN_6f0a2c20 | `CSpriteFrame` 构造：`[0x67]=1.0f`、`[0x6a]=0.25f`、`[0x6b]=1` |
| 6f0bddc0 | FUN_6f0bddc0 | `CControl` 构造：设虚表、`[0x6c..0x78]` 清零、`[0x78]=1` |
| 6f0a9000 | FUN_6f0a9000 | `CTextFrame` 构造：0x240 字节；默认色 `0xff000000`，`[0x7d..0x7f]=-1` |
| 6f0aa130 | FUN_6f0aa130 | `CTextFrame::SetText`：148 个调用者；缓冲重分配/原地拷贝；置脏标志 |
| 6f0a94b0 | FUN_6f0a94b0 | `CTextFrame` 字体纹理释放（`+0x1e8` / `+0x1ec`） |
| 6f0a9440 | FUN_6f0a9440 | 分配 `strlen+1` 缓冲并以 `'*'` 填满（推断：密码掩码） |
| 6f0aaac0 | FUN_6f0aaac0 | `CButtonFrame` 构造：注册 FrameDef ID `0x40060064` 两次 |
| 6f0a71b0 | FUN_6f0a71b0 | `CMessageFrame` 构造：两 8 字节块、5 条消息链表哨兵 |
| 6f0a7a80 | FUN_6f0a7a80 | `CMessageFrame::AddMessage`：0x38 字节消息节点 + 0x18 字节定时器节点 |
| 6f0a75e0 | FUN_6f0a75e0 | `CMessageFrame` 析构：释两数组、摘 5 条链表 |
| 6f0a4cd0 | FUN_6f0a4cd0 | `CBackdropFrame` 九宫格渲染/更新（推断名 OnUpdateModels）：`FUN_6f12a6a0` 提交图元 |
| 6f0a4900 | FUN_6f0a4900 | 九宫格纹理槽选择器（标志位 `+0x1b0` → 最多 9 槽） |
| 6f0a65e0 | FUN_6f0a65e0 | 九宫格顶点 UV 计算（4 顶点 × 12 float） |
| 6f0a6f00 | FUN_6f0a6f00 | `CHighlightFrame` 高亮缓冲：分配/取指针/释放（全局 `DAT_6fbb9eb0`） |
| 6f0ae290 | FUN_6f0ae290 | `CEditBox` 构造：内嵌 `CScrollFrame` + 2×`CTextFrame` + `CHighlightFrame` |
| 6f0b4350 | FUN_6f0b4350 | `CChatDisplay` 构造：内嵌 `CEditBox`（0x274）与 0x210 字节对象 |
| 6f0a3c20 | FUN_6f0a3c20 | `CScrollFrame` 构造：`[0x5c..0x5f]` 异或掩码常量，标志 `\|= 8` |
| 6f0c98a0 | FUN_6f0c98a0 | `CFloatingFrame` 构造：设虚表、一链表哨兵、锚点初始化 |
| 6f0c9630 | FUN_6f0c9630 | `CCursorFrame` 构造：`FUN_6f0a2c20(parent, 0, 10000)` |
| 6f0ca0f0 | FUN_6f0ca0f0 | `CScreenFrame` 按索引懒创建子 Frame（常量表 `DAT_6f95d420`） |
| 6f0ca020 | FUN_6f0ca020 | `CScreenFrame` 取光标 Frame（槽 `+0x16c`，26 个调用者） |
| 6f0c9d20 | FUN_6f0c9d20 | `CScreenFrame` 析构：销毁 `+0x5c` 的 4 元数组 |
| 6f0b8f30 | FUN_6f0b8f30 | `CSimpleFrame` 析构：89 个调用者；释子链表与 5 元数组 |
| 6f0b6d30 | FUN_6f0b6d30 | `CSimpleTop` 析构：释 `RENDERBATCHENTRY` 数组与两数组 |
| 6f0bd750 | FUN_6f0bd750 | 锚点初始化：两 `CFramePointRelative(0,0)` + 布局登记 |
| 6f0bd8a0 | FUN_6f0bd8a0 | `SetFramePoint`：替换槽位锚点 + 布局登记 |
| 6f0ca670 | FUN_6f0ca670 | `CFramePointRelative` 工厂（存 4 参数） |
| 6f0ca5b0 | FUN_6f0ca5b0 | `CFramePoint` 删除析构（虚表 `+0xc` 分流绝对/相对） |
| 6f0bcca0 | FUN_6f0bcca0 | 清空 Frame 的锚点数组 |
| 6f0bd520 | FUN_6f0bd520 | `CLayoutFrame::AddFrameNode`：`FRAMENODE` 链表登记/或标志 |
| 6f0bd630 | FUN_6f0bd630 | 布局失效登记：挂进全局链表（哨兵 `PTR_LOOP_6fb6641c`） |
| 6f0a26b0 | FUN_6f0a26b0 | `CFrame` 时间驱动更新：阈值 `DAT_6f95a884`，向子 Frame 调虚表 `+0x14` |
| 6f0a1380 | FUN_6f0a1380 | `CFrame::SetName`（推断）：名字存 `+0x160`，`param_3` 存 `+0x164` |
| 6f0b93a0 | FUN_6f0b93a0 | `CSimpleFrame` 的同名 setter：名字存 `+0x7c`，`param_3` 存 `+0x80` |
| 6f0a3ee0 | FUN_6f0a3ee0 | `CModelFrame` 析构类函数：`+0x168 > 1` 时释模型（`"__PAUHMODEL__"`） |
| 6f6ca6b0 | FUN_6f6ca6b0 | 错误弹框：读 `"ErrorStringDefault"` / `"FrameDefDefault"`，调 `Ordinal_578` |
| 6f0222a0 | FUN_6f0222a0 | FrameDef 相关条件初始化（调 `FUN_6f090d50` 与 `FUN_6f6d81c0`） |
| 6f6d81c0 | FUN_6f6d81c0 | FrameDef 文件检查/加载（调 `FUN_6f18b240` / `FUN_6f18af20`） |

## 12. 数据结构 / 关键全局量

- **FrameDef 注册表**：哈希表（`FUN_6f06b030` 查 / `FUN_6f0693f0` 插）；节点 `[7]` = 模板类型名字符串，`[0x1c]` = 某文本字段；注册表项（`.data` `6fb69810`）：`"ConsoleUI"` → 工厂 `6f347d10` → 父 `"UpperButtonBarFrame"`（结构为推断）。
- **模板文件**：`UI\FrameDef\FrameDef.toc`（`6f951c54`）、`UI\war3skins.txt`（`6f951c73`）、`UI\MiscData.txt`（`6f951ee4`，指针在 `.data` `6fb592fc`）、`UI\MiscUI.txt`（`6f951ef4`）；错误日志 `FramedefErrors.log`。
- **全局链表哨兵**：`PTR_LOOP_6fb6641c`（待布局 Frame 链表头，`DAT_6fb66418 + this` 为节点地址）；`DAT_6fbb9eb0`（`CHighlightFrame` 全局高亮像素缓冲）；`DAT_6fbb9e18`（`CLayer::SetParent` 置 0 的标志，含义未验证）。
- **常量表**：`DAT_6f95d420`（`CScreenFrame` 子 Frame 类型参数：9500 / 9250 / 750 / 500 / 0 / 0 / 1.0f / 1.0f / +inf，含义未验证）；`DAT_6f95a590`（异或掩码常量，`CBackdropFrame` UV 与 `CScrollFrame` 字段共用）；`DAT_6f95a884` / `DAT_6f95a890`（`CFrame` 更新时间阈值与掩码）。
- **RTTI 类型**：`CFrame`、`CFrameDefStatus`、`CFramePoint`、`CFramePointAbsolute`、`CFramePointRelative`、`CLayoutFrame`、`CFrameResizeEvent`、`CBackdropFrame`、`CTextFrame`、`CButtonFrame`、`CTextButtonFrame`、`CModelFrame`、`CSpriteFrame`、`CScrollFrame`、`CMessageFrame`、`CSimpleMessageFrame`、`CHighlightFrame`、`CSimpleFrame`、`CSimpleGlueFrame`、`CCursorFrame`、`CFloatingFrame`、`CScreenFrame`、`CTimerTextFrame`、`CBuildFrame`、`CFrame::FOGNODE`、`CFrame::LIGHTNODE`、`CLayoutFrame::FRAMENODE`、`JassFrame`（`6fbb5d18`，`.data` 段，**用途未验证**）。
- **虚表偏移（已验证的调用点）**：`+0x00` 删除器（析构链）、`+0x04`（`FUN_6f0bd630` 的在链表检查）、`+0x0c`（`CFramePoint` 类型检查 / 锚点有效性）、`+0x10`（`CBackdropFrame` 取矩形）、`+0x14`（`FUN_6f6ca6b0` 的错误显示虚调用；`FUN_6f0a26b0` 的子 Frame 更新）、`+0x108`（`CChatDisplay` 子对象刷新，**推断**为重布局）。

## 13. 与其它阶段的关系

- **S2 主循环**：`FUN_6f0280e0` 是 UI 子系统的唯一初始化入口（在 GameMain 的启动序列内）；每帧 UI 更新的调用链（主循环 → `FUN_6f0a26b0`）未验证。
- **S4 JASS2**：`FUN_6f0280e0` 内调 `FUN_6f1e9a50`（native 注册），说明 UI 初始化与脚本系统初始化在同一启动序列里；Frame 事件（按钮点击等）**推断**经 `.fdf` 模板里的处理函数名绑定到 JASS 触发器，但引擎侧的分发表未定位。
- **S5 Gx 渲染**：`CBackdropFrame` 的图元经 `FUN_6f12a6a0` 提交进 `CGxDevice` 批量；`CSimpleTop` 的 `RENDERBATCHENTRY` 是另一条批量路径；字体纹理图集（S5 的 `FontTexture`，`FUN_6f0280e0` 的被调函数之一）是 `CTextFrame` 的底座。
- **S7 MPQ**：`FrameDef.toc` 与各 `.fdf`、`.blp` 皮肤都经 Storm 文件层读出；本轮只确认了文件名与加载入口，未深挖 MPQ 侧。
- **S3 模拟**：游戏内 UI（命令面板、英雄栏、小地图）是 `CScreenFrame` 的子 Frame（`FUN_6f0ca020` 的 26 个调用者在 `6f1cxxxx` / `6f2bxxxx` / `6f36xxxx` 的游戏逻辑代码里）；UI 只显示模拟状态、不直接改模拟（点击命令经 S0 的回合命令包走网络同步，**该链路未验证**）。
- **S9 录像 / S10 Battle.net**：Glue 屏（主菜单、战网大厅）共用同一套 Frame/FrameDef 机制（`GlueScreen` 字符串表、`BackdropFrameHandlers.cpp`）；录像回放与战网界面的具体 Frame 树未深挖。

## 14. 未验证点

1. 引擎主循环（S2 的 `FUN_6f05f710`）到 `CFrame` 每帧更新（`FUN_6f0a26b0`）的完整调用链；`FUN_6f0a26b0` 的时间阈值 `DAT_6f95a884` 的具体数值与单位。
2. `CFrame` 虚表各槽位的完整方法表（只验证了 `+0x00/+0x04/+0x0c/+0x10/+0x14/+0x108` 六个调用点）；`CFrame` 在 `+0xb4` 处的第二虚表对象身份。
3. Frame 事件分发：按钮点击/鼠标悬停如何从 Windows 消息（S2 消息泵 `FUN_6f1532b0`）到达 `CButtonFrame`；`.fdf` 里 `OnClick` 等处理函数名与 JASS 触发器的绑定方式；`BackdropFrameHandlers.cpp` 的处理函数表。
4. `DAT_6f95d420` 常量表（9500/9250/750/500/…）的含义；`CChatDisplay` 的 `0x400b0064` / `0x40100064` 与 `CButtonFrame` 的 `0x40060064` 的 FrameDef ID 含义。
5. `CTextFrame::SetText` 后半段（字体纹理更新）与 `FUN_6f0a9490(0x100)`、`FUN_6f0be300(0x80)` 的确切身份；`CTextFrame` 默认字段 `[0x84]=0.01f` / `[0x85]=-0.01f` 的含义。
6. `FUN_6f0a4cd0` 是否就是 `CBackdropFrame::OnUpdateModels`（字符串在邻近数据区，函数名是推断）；九宫格标志位 `+0x6c` 各 bit 与 `+0x6d/0x6e/0x6f/0x71/0x72…0x75` 字段的完整语义。
7. `JassFrame`（`.data` `6fbb5d18`）的用途；`FUN_6f6d81c0` / `LAB_6f6e8230` 的确切行为；`FUN_6f090b70` 内 `FUN_6f066590` / `FUN_6f067fa0` / `FUN_6f068300` 的本体。
8. `CModelFrame` 的模型加载与渲染（只验证了析构里的模型释放）；`CTextButtonFrame` / `CSimpleGlueFrame` / `CTimerTextFrame` / `CBuildFrame` 各类的构造与行为未深挖。
9. `CSimpleFrame` / `CSimpleTop` 轻量分支与重型 `CFrame` 树的分工边界（何时用 Simple、何时用重型）；`RENDERBATCHENTRY` 的提交路径。
10. `CLayer` 快捷键哈希表（`SHORTCUTREGNODE`）的分发路径；`DAT_6fbb9e18` 的含义。
