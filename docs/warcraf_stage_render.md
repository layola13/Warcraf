# S5 — Gx 渲染抽象（D3D/OpenGL 双后端）

- 状态：已验证 2026-11-05
- 证据：`~/workspace/warcraf_ghidra/render_dump.txt`（30 个 Gx 相关字符串定位 + 12 个函数反编译）、`render_dump2.txt`（D3D 加载器、OpenGL 能力探测、设备构造/析构、场景回读等 18 个函数反编译 + RTTI 引用）、`render_dump3.txt`（设备工厂、后端选择器、D3D/OpenGL 构造函数、上下文创建、CreateDevice 等 9 个函数反编译）；导入表（`objdump -p Game.dll`）
- 上承：S0 帧同步、S1 网络层、S2 主循环（GameMain → FUN_6f027fb0 → 本阶段图形初始化 FUN_6f027e20）、S3 模拟核心、S4 JASS2

## 1. 总述

渲染层是经典的 **Gx 抽象 + 双后端**结构：`CGxDevice` 为抽象基类（虚表 `CGxDevice::vftable`），`CGxDeviceD3d`（Direct3D 9）与 `CGxDeviceOpenGl`（OpenGL 1.x 固定管线）为两个实现类，共享同一套上层接口（纹理、图元批量、场景提交、回读）。后端在启动时由注册表键 `Gfx OpenGL` 决定（0 = OpenGL，1 = D3D），设备以单例形式存在（全局 `DAT_6fbc5420`）。

两个后端在**链接方式上不对称**，这是本阶段最值得记录的事实：

- **OpenGL 是静态链接**：`OPENGL32.dll` 在导入表里，数十个 `gl*` / `wgl*` 函数直接导入（`glTexImage2D`、`glDrawElements`、`wglCreateContext`、`wglMakeCurrent`、`wglGetProcAddress` 等）。
- **D3D9 是动态加载**：导入表里**没有** `d3d9.dll`；`FUN_6f0ec530` 用 `LoadLibraryA("d3d9.dll")` + `GetProcAddress("Direct3DCreate9")` 再以 SDK 版本 `0x20`（32）创建 `IDirect3D9`。D3D 不可用时该函数返回 0，调用方随即走失败路径。

上层（模型/地形/UI，见 S3/S6）只跟 `CGxDevice` 虚接口打交道；两个后端各自实现纹理上传（`CGxOglTexture` / `CGxD3dPrim` 等）、图元批量（`CGxOglPrim` / `CGxD3dPrim`）、场景提交与回读（`CGxOglScene` / `CGxD3dScene`）。源码路径字符串残留在二进制里，目录结构可直接还原：`engine\source\gx\`（`CGxDevice\CGxDevice.cpp`）、`CGxDeviceD3d\`（`CGxDeviceD3d.cpp`、`CGxD3dPrim.cpp`、`CGxD3dScene.cpp`）、`CGxDeviceOpenGL\`（`CGxDeviceOpenGl.cpp`、`CGxOglPrim.cpp`、`CGxOglScene.cpp`、`CGxOglTexture.cpp`）、`engine\source\gxu\`（`GxuFont.cpp`、`GxuLight.cpp`、`IGxuFont.h`）。

## 2. 启动链：GameMain → 图形初始化 → 设备工厂

```
GameMain 6f0297d0
  └─ FUN_6f027fb0（启动序列，292 字节；失败则调 FUN_6f0277c0 清理后返回 0）
       ├─ Ordinal_565("Warcraft III (build 52240)")、Ordinal_460(0xcc10) 等 Storm 初始化
       ├─ FUN_6f027e20（图形初始化，393 字节）── 本阶段入口
       │    ├─ 读注册表（Ordinal_423，键 "Warcraft III"）：
       │    │    "Gfx Fullscreen" → local_8（默认 1）、"Gfx SwTNL" → local_c、
       │    │    "Gfx OpenGL" → local_c（0 = OpenGL，1 = D3D）
       │    ├─ FUN_6f02c060()（664 字节，窗口/显示模式相关前置工作，细节未深挖）
       │    ├─ FUN_6f0278e0(&local_c)：读 5 个注册表值（索引 0/1/2/4/3）
       │    │    → FUN_6f027960（234 字节，显示模式选择；调后端选择器 FUN_6f0e3190）
       │    └─ FUN_6f0e2f70(local_c, …)（设备工厂，见 §3）
       │         成功 → DAT_6fbb80d8 = 设备单例；FUN_6f0e3440(0, 0x3a03126f)；返回 1
       │         失败 → FUN_6f0277c0() 清理，返回 0
       └─ FUN_6f0272a0() 等后续启动步骤（S2 已覆盖主循环部分）
```

- 后端选择器 `FUN_6f0e3190`（106 字节，被 `FUN_6f027960` 调用）：参数 0 → OpenGL 路径 `FUN_6f0e7b70`；参数 1 → D3D 路径 `FUN_6f0e8400`（显示模式枚举）；其它值 → 标记无效（`DAT_6fb665c8 = 2`，返回空）。当前后端记录在 `DAT_6fb665c8`，当前显示模式序号在 `DAT_6fb665c4`。
- `FUN_6f027e20` 里反复调用的 `FUN_6f0475a0`（7 字节小函数）身份未验证（疑似注册表/配置读取辅助），不展开。

## 3. 设备工厂与单例 `FUN_6f0e2f70`（215 字节）

```c
// param_1: 0 = OpenGL, 1 = D3D；param_3: 显示模式序号（-1 表示无）
DAT_6fb665c4 = param_3; DAT_6fb665c8 = param_1;
if (param_1 == 0) DAT_6fbc5420 = FUN_6f0e8900();   // new CGxDeviceOpenGl（0xbfc 字节）
else if (param_1 == 1) DAT_6fbc5420 = FUN_6f0e85b0(); // new CGxDeviceD3d（0x1ef8 字节）
iVar1 = (**(code **)(*DAT_6fbc5420 + 0xc))(param_2, iVar1, param_4, param_5, param_6); // 虚表 +0xc：Init
if (iVar1 != 0) {
    Ordinal_469(FUN_6f0e3c10);      // 注册退出清理回调
    FUN_6f0e3790(0xff000000);        // 默认清屏色：不透明黑
    DAT_6fbc5438 = param_4; DAT_6fbc5434 = param_2;
    FUN_6f0e2e90();
    return DAT_6fbc5420;
}
// 失败：虚表 +8（Release，引用计数 1）后返回 0
```

- 两个 `new` 包装器都经 `Ordinal_401`（带源码路径的 `operator new`）分配后调构造函数：D3D 构造 `FUN_6f0e80f0`（461 字节）、OpenGL 构造 `FUN_6f0e8630`（368 字节），二者都先调基类构造 `FUN_6f0e3ee0`（1357 字节）再设各自虚表（`CGxDeviceD3d::vftable` / `CGxDeviceOpenGl::vftable`）。
- D3D 构造初始化了 256 项的状态缓存表（`param_1[0x1d3]` 起，每项步长 3 个 dword，初值 -1）与 128 项的第二张表（`param_1[0x4d7]` 起，步长 4，初值 -1）——推断为渲染状态/纹理阶段缓存；OpenGL 构造则把 `0x2e1–0x2fb` 一段状态数组清零，并置 `param_1[0x15d] = 1`（与 D3D 的 0 相对，疑似“使用硬件 T&L / 顶点数组”类标志位，**未验证**具体含义）。
- 基类析构 `FUN_6f0e47a0`（240 字节）：设回 `CGxDevice::vftable`，释放成员——`CGxTex` 数组两处、`CGxStateBom`、`CGxAppRenderState`、`EGxRenderState` 数组、`CGxPushedRenderState` 数组等。OpenGL 析构 `FUN_6f0e87a0`（300 字节）先释放 GL 专有成员（`C2Vector`/`C3Vector`/`CImVector` 数组等，Tempest 数学库类型）再调基类析构。

## 4. D3D9 后端

### 4.1 动态加载 `FUN_6f0ec530`（97 字节，3 个调用者）

```c
*param_1 = 0; *param_2 = 0;
hModule = LoadLibraryA("d3d9.dll");
if (hModule) {
    pF = GetProcAddress(hModule, "Direct3DCreate9");
    if (pF) { iVar2 = (*pF)(0x20); *param_2 = iVar2; if (iVar2) return 1; }
}
FUN_6f0ec7d0();  // 失败清理（46 字节）
return 0;
```

三个调用者：`FUN_6f0e8400`（模式枚举）、`FUN_6f0ec240`（能力查询）、`FUN_6f0e8350`（当前显示模式查询）。

### 4.2 显示模式枚举 `FUN_6f0e8400`（431 字节）

- 创建 `IDirect3D9` 后，对 `DAT_6f95d930` 处的 3 个像素格式逐一调用虚表 `+0x1c`（`EnumAdapterModes`），过滤条件：格式码为 `0x16`/`0x17`/`0x18` 且宽度 `> 0x27f`（639），即只收宽度 ≥ 640 的模式；每条记录 0x18 字节（宽、高、刷新率、格式、色深：格式 `0x16` → 32 位，`0x17` → 16 位、`0x18` → 15 位，**格式码与 D3DFMT 的对应为推断，未逐一核对**）；结果数组按需扩容（`FUN_6f0e3b00`）。
- `FUN_6f0e8350`（171 字节）用虚表 `+0x14`（`GetAdapterDisplayMode`）取当前桌面分辨率，失败时宽高均为 `0xffff`；`FUN_6f0e2e40`（43 字节）是它的 thin wrapper，仅当 `param_1 == 1`（D3D 后端）时才查询。

### 4.3 能力查询 `FUN_6f0ec240`（441 字节）

- 虚表 `+0x38`（`GetDeviceCaps`）取 1064 字节的 `D3DCAPS9`；虚表 `+0x20`（`GetAdapterIdentifier`）取适配器标识；虚表 `+0x14` 取当前显示模式。
- 能力标志位存入设备对象 `+0xb0…+0xd0`（多 bit 位段：`+0x5a4 & 0x1000000`、`+0x594 & 0x20000`、`+0x618` 的 bit 4/6 等），厂商/设备 ID 存入 `+0x6b8`；随后调 `FUN_6f0ec400` 建设备。

### 4.4 建设备 `FUN_6f0ec400`（197 字节）

- 虚表 `+0x40`（`IDirect3D9::CreateDevice`，参数含窗口句柄 `+0x574`、行为标志 `param_2`、呈现参数块）；成功后虚表 `+0x28` 再调一次（参数含 `0x15`，**具体是哪个 COM 方法未验证**，疑似交换链/附加设备配置）。
- 成功后 `SetFocus(窗口)`，并用 `SetWindowLongA(hwnd, GWL_WNDPROC, 0x6f0ec940)` **接管窗口过程**（原过程存 `DAT_6fbc5e98`）——即 D3D 后端的窗口消息先过 Gx 的钩子再进主消息泵（S2 的 `FUN_6f1532b0`）。

### 4.5 图元批量 `CGxD3dPrim`：`FUN_6f0ee6c0` / `FUN_6f0ee810`

- 构造（332 字节，`CGxDeviceD3d\CGxD3dPrim.cpp:0x70`）：调 `FUN_6f0edd10` 创建 D3D 顶点/索引缓冲（COM 指针存对象 `[0xb]`/`[0xc]`），再按 `iVar1 * 元素大小` 与 `iVar2 * 2` 分配系统内存备份（`[3]`、`[7]`）；失败时对两个 COM 指针调虚表 `+8`（`Release`）并析构自身。
- 析构（120 字节，`0xcc–0xcd` 行）：释放两块系统内存与两个 COM 缓冲。

### 4.6 场景回读 `FUN_6f0ed080`（615 字节，`CGxD3dScene.cpp`）

- 对设备 `+0x584` 处的 surface 调虚表 `+0x48`（`LockRect`）与 `+0x30`/`+0x34` 取描述/行距，随后按像素格式把 16 位源像素逐行展开成 32 位 RGBA（alpha 恒 `0xff`）：
  - 格式 `0x15`/`0x16`：直接 memcpy 后逐像素置 alpha；
  - 格式 `0x17`：`R = (u<<3)&0xf8`、`G = (u>>3)&0xfc`、`B = (u>>8)&0xf8`（5-6-5）；
  - 格式 `0x18`/`0x19`：`R = (u>>7)&0xf8`、`G = (u>>2)&0xf8`、`B = u<<3`（5-5-5）；
  - 其它格式：整块 `memset 0xff`。
- 结束后虚表 `+0x38`（`UnlockRect`）、`+8`（`Release`），再调 `FUN_6f0ecde0` 与 `FUN_6f771125`。与 OpenGL 版（§5.3）同构——这是**截屏/小地图快照类回读**的后端实现（具体调用方未验证，见 §8）。

## 5. OpenGL 后端

### 5.1 上下文创建 `FUN_6f0e78b0`（89 字节，3 个调用者）

```c
HDC hdc = GetDC(*(HWND *)(param_1 + 0x578));   // 设备对象 +0x578 存窗口句柄
*(HDC *)(param_1 + 0x57c) = hdc;
HGLRC rc = FUN_6f0ebe20();                      // 封装 wglCreateContext（像素格式选择在内）
*(HGLRC *)(param_1 + 0x580) = rc;
wglMakeCurrent(hdc, rc);
FUN_6f0ebb40();   // 能力探测（§5.2）
FUN_6f0ea500();   // 1321 字节，GL 状态初始化（未深挖）
```

### 5.2 能力探测 `FUN_6f0ebb40`（607 字节）

- `glGetString(0x1f02 /* GL_VERSION */)` → `sscanf("%d.%d")` → 主/次版本号打包 `major<<16|minor` 存 `DAT_6fbc5df4`。
- `FUN_6f0ebda0`（97 字节）：在 `glGetString(0x1f03 /* GL_EXTENSIONS */)` 的空格分隔 token 列表里做前缀匹配（`Ordinal_509` = `strncmp` 封装），即扩展存在性检查。
- `GL_EXT_compiled_vertex_array` 存在 → `wglGetProcAddress("glLockArraysEXT"/"glUnlockArraysEXT")` 存 `DAT_6fbc5dfc/6fbc5e00`。
- `GL_ARB_multitexture` 存在 → `glGetIntegerv(0x84e2 /* GL_MAX_TEXTURE_UNITS */, &DAT_6fbc5e04)`，并用 `wglGetProcAddress` 取回全部 `glMultiTexCoord{1,2,3,4}{d,dv,f,fv,i,iv,s,sv}ARB`、`glActiveTextureARB`、`glClientActiveTextureARB` 函数指针，存入 `DAT_6fbc5e08…6fbc5e8c` 的指针表；不存在时 `DAT_6fbc5e04 = 1`（单纹理单元回退）。
- 另有两处扩展检查结果存 `DAT_6fbc5e90` / `DAT_6fbc5e94`，**对应哪个扩展未验证**（`DAT_6fbc5e94` 在纹理上传里控制 wrap 模式取值，见 §5.4）。

### 5.3 场景提交与回读 `FUN_6f0f0cc0`（443 字节，`CGxOglScene.cpp`）

- 若设备 `+0x504` 的 bit `0x20` 未置 → `glFinish()`；否则 `wglSwapLayerBuffers(HDC +0x57c, 1)`（交换前台层）。
- 若 `param_2 != 0` 再 `glFinish()` 一次（同步提交）。
- 若回读标志 `+0x560` 置位：`glReadBuffer(0x405 /* GL_BACK */)` + `glReadPixels(0, 0, w, h, 0x80e1 /* GL_BGRA */, 0x1401 /* GL_UNSIGNED_BYTE */, buf)`，逐像素 alpha 置 `0xff`，再上下翻转行序（OpenGL 原点在左下角）。与 D3D 版 §4.6 同构。

### 5.4 纹理上传 `FUN_6f0eb600`（939 字节，`CGxOglTexture.cpp`）

- 首次上传：`glGenTextures(1, &tex[0xc])`；`glBindTexture(0xde1 /* GL_TEXTURE_2D */, id)`。
- 过滤：`glTexParameteri(0x2800 /* MAG */, 表 DAT_6f95fa30[(flags&3)])`、`glTexParameteri(0x2801 /* MIN */, 表 DAT_6f95fa20[(flags&3)])`——过滤模式由纹理标志低 2 位查表决定。
- 环绕：若 `DAT_6fbc5e94 == 0`（无某扩展），wrap 由标志位算出 `0x2802/0x2803` 参数（`(flags&4|0xa400)>>2` 等，含 `GL_CLAMP` 家族值）；否则用 `0x812f /* GL_CLAMP_TO_EDGE */` 或 `0x2901 /* GL_REPEAT */`。
- mipmap 链：按纹理宽高算层数（`log2(max(w,h))`），每层经 `FUN_6f0eb9b0`（253 字节，像素格式转换）写入 staging 缓冲（设备 `+0xbf4`，按需扩容到 `w*h*4`），再 `glTexImage2D(…, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, …)`；已存在纹理则走 `glTexSubImage2D` 做矩形更新（坐标由 `tex[1..4] >> level` 计算）。
- 纹理标志位 `0x10` 控制是否生成完整 mipmap 链，`0x20` 控制像素步长（`local_18>>2` vs `>>1`）；**标志位的完整语义未验证**。

### 5.5 图元批量 `FUN_6f0f1910`（288 字节，`CGxOglPrim.cpp`）

- 顶点 staging 缓冲（设备 `+0xbcc`，容量 `+0xbd0`）按 `FUN_6f0e38f0()`（疑似顶点 stride）`* param_2` 扩容；索引缓冲（`+0xbd4`/`+0xbd8`/`+0xbdc`）由 `FUN_6f0de110(param_4)` 分配；返回两块缓冲指针给调用方填充。这是 OpenGL 版的“锁缓冲—填数据—绘制”批量路径。

## 6. 基类对象槽分配 `FUN_6f0e6980`（303 字节，`CGxDevice\CGxDevice.cpp:0x49d`）

- `CGxDevice` 的通用对象注册器（被 `FUN_6f0ebab0` / `FUN_6f0f09b0` 两个 69 字节 thin wrapper 调用，推断为两后端各自的纹理/资源创建入口）：
  - 先调 `FUN_6f0e45d0(…)` 创建资源对象（0x38 字节描述块）；
  - 若设备 `+0x80`（空闲槽栈顶）为 0：计数 `+0x540` 加 1，数组 `+0x544` 按需扩容（扩容步长 `FUN_6f0e4c40`，对齐到 `uVar3` 的倍数），对象指针写入槽位，对象 `+0x34` 存槽下标；
  - 否则从 `+0x84` 的空闲栈弹出一个旧槽位复用。
- 即设备对象用**槽位数组 + 空闲栈**管理纹理/图元等子资源，槽下标即资源句柄的一部分（与 S4 的 `JassHandle` 句柄思路同构，**对应关系未验证**）。

## 7. 字体与视频（Gx 之上的配套模块）

- **字体**：`GxuFont` 相关函数簇（`FUN_6f0d8090` 668 字节、`FUN_6f0da630` 373 字节、`FUN_6f0dd110` 839 字节、`FUN_6f0e2b10` 393 字节等），源码 `GxuFont.cpp` / `GxuFontUtil.cpp` / `IGxuFont.h`；字符串证据有 `"FontTexture"`（`FUN_6f0280e0`，948 字节，46 个被调函数，0 个调用者——疑似字体纹理图集生成）与 `"IGxuFontGlyph.cpp"`。字体以纹理图集（glyph bitmap 打包成一张纹理）方式渲染；**字形光栅化与图集打包细节未深挖**。
- **视频**：`GxVideo` / `GxMovie`（`engine\source\gx\gxmovie/gxvideo.h`、`GxVideo.cpp`），`FUN_6f0e8fb0` 1169 字节、14 个被调函数、1 个调用者，含 512 字节栈缓冲与浮点时间参数——为过场动画视频播放入口（**细节未深挖**，仅确认存在）。
- **光照**：`GxuLight.cpp` 相关（`FUN_6f0cc450` 210 字节等），RTTI 有 `CAnimLightObj` / `CLightList` / `LIGHTNODE`；**未深挖**。
- **模型/纹理缓存**：RTTI 有 `CGxMesh` / `CGxD3dMesh` / `CGxMat` / `CModelHash` / `CTextureHash` / `CTextureCacheRow` / `CModelNameHash` / `CTerrainTexture` 等；`.mdx` 字符串引用函数 `FUN_6f726570`（412 字节）与 `FUN_6f81aa60`（107 字节），`.blp` 引用函数数十个（含 `FUN_6f6f0260` 984 字节、`FUN_6f6fe6d0` 377 字节、27 个调用者）。模型（MDX）加载与 BLP 纹理解码的完整流程**未深挖**，留待 S6（UI 纹理）/ S12 交叉验证时补。

## 8. 关键函数地址表

| 地址 | 函数 | 作用（证据） |
|---|---|---|
| 6f027e20 | FUN_6f027e20 | 图形初始化：读注册表 Gfx Fullscreen/SwTNL/OpenGL，调模式选择与设备工厂；GameMain→6f027fb0→此函数 |
| 6f0278e0 | FUN_6f0278e0 | 读 5 个注册表值 → FUN_6f027960（显示模式选择） |
| 6f027fb0 | FUN_6f027fb0 | 启动序列（GameMain 直接调用者），含图形初始化调用 |
| 6f0e3190 | FUN_6f0e3190 | 后端选择器：0→OpenGL（FUN_6f0e7b70），1→D3D（FUN_6f0e8400），其它→无效 |
| 6f0e2f70 | FUN_6f0e2f70 | 设备工厂：0→new CGxDeviceOpenGl，1→new CGxDeviceD3d；虚表+0xc Init；单例 DAT_6fbc5420 |
| 6f0e8900 | FUN_6f0e8900 | new CGxDeviceOpenGl（分配 0xbfc 字节 → 构造 FUN_6f0e8630） |
| 6f0e85b0 | FUN_6f0e85b0 | new CGxDeviceD3d（分配 0x1ef8 字节 → 构造 FUN_6f0e80f0） |
| 6f0e8630 | FUN_6f0e8630 | CGxDeviceOpenGl 构造：设虚表，清零状态数组 |
| 6f0e80f0 | FUN_6f0e80f0 | CGxDeviceD3d 构造：设虚表，初始化 256 项 + 128 项状态缓存（初值 -1） |
| 6f0e3ee0 | FUN_6f0e3ee0 | CGxDevice 基类构造（1357 字节，两后端构造共用） |
| 6f0e47a0 | FUN_6f0e47a0 | CGxDevice 析构：释放 CGxTex/CGxStateBom/CGxAppRenderState/渲染状态数组 |
| 6f0e87a0 | FUN_6f0e87a0 | CGxDeviceOpenGl 析构：释放 GL 专有成员 → 基类析构 |
| 6f0ec530 | FUN_6f0ec530 | D3D9 动态加载：LoadLibraryA("d3d9.dll") + Direct3DCreate9(0x20) |
| 6f0e8400 | FUN_6f0e8400 | D3D 显示模式枚举（EnumAdapterModes，宽 > 639，格式 0x16/0x17/0x18） |
| 6f0ec240 | FUN_6f0ec240 | D3D 能力查询：GetDeviceCaps/GetAdapterIdentifier，标志存 +0xb0…+0xd0 |
| 6f0e8350 | FUN_6f0e8350 | 取当前桌面分辨率（GetAdapterDisplayMode），失败返回 0xffff |
| 6f0ec400 | FUN_6f0ec400 | D3D CreateDevice（虚表+0x40），接管窗口过程（0x6f0ec940） |
| 6f0ee6c0 | FUN_6f0ee6c0 | CGxD3dPrim 构造：创建 D3D 顶点/索引缓冲 |
| 6f0ee810 | FUN_6f0ee810 | CGxD3dPrim 析构：释放两缓冲 |
| 6f0ed080 | FUN_6f0ed080 | D3D 场景回读：LockRect + 16 位→32 位像素展开（CGxD3dScene.cpp） |
| 6f0e78b0 | FUN_6f0e78b0 | OpenGL 上下文创建：GetDC → wglCreateContext → wglMakeCurrent → 能力探测 |
| 6f0ebb40 | FUN_6f0ebb40 | OpenGL 能力探测：GL 版本解析 + 扩展检查 + ARB multitexture 函数指针表 |
| 6f0ebda0 | FUN_6f0ebda0 | GL 扩展存在性检查（GL_EXTENSIONS token 扫描） |
| 6f0f0cc0 | FUN_6f0f0cc0 | OpenGL 场景提交与回读：wglSwapLayerBuffers/glFinish + glReadPixels（CGxOglScene.cpp） |
| 6f0eb600 | FUN_6f0eb600 | OpenGL 纹理上传：glTexImage2D/SubImage2D + mipmap 链（CGxOglTexture.cpp） |
| 6f0f1910 | FUN_6f0f1910 | OpenGL 图元批量缓冲分配（CGxOglPrim.cpp） |
| 6f0e6980 | FUN_6f0e6980 | CGxDevice 对象槽分配器（槽数组 + 空闲栈，CGxDevice.cpp:0x49d） |
| 6f0d8090 | FUN_6f0d8090 | GxuFont 字体相关（668 字节，GxuFontUtil.cpp） |
| 6f0e8fb0 | FUN_6f0e8fb0 | GxMovie 视频播放相关（1169 字节，GxVideo.cpp） |

## 9. 数据结构 / 关键全局量

- **设备单例**：`DAT_6fbc5420`（`CGxDevice*`，工厂写入）；`DAT_6fbb80d8`（图形初始化保存的同一指针）；当前后端 `DAT_6fb665c8`（0=OpenGL，1=D3D，2=无效），当前模式序号 `DAT_6fb665c4`。
- **CGxDeviceD3d 对象大小**：0x1ef8 字节；**CGxDeviceOpenGl 对象大小**：0xbfc 字节（`operator new` 参数为准）。
- **D3D 模式表**：`DAT_6f95d930` 起 3 个像素格式；每条模式记录 0x18 字节；数组扩容步长逻辑见 `FUN_6f0e8400`。
- **GL 能力全局量**：`DAT_6fbc5df4` = 打包 GL 版本（major<<16\|minor）；`DAT_6fbc5df8` = compiled_vertex_array 标志；`DAT_6fbc5e04` = 最大纹理单元数；`DAT_6fbc5e08…6fbc5e8c` = ARB multitexture 函数指针表；`DAT_6fbc5dfc/6fbc5e00` = glLock/UnlockArraysEXT 指针；`DAT_6fbc5e90/6fbc5e94` = 未识别的扩展标志。
- **回读缓冲**：设备 `+0x564`（宽）、`+0x568`（高）、`+0x56c`（像素缓冲指针）；D3D 侧 surface 在 `+0x584`，窗口句柄 `+0x574`，HDC `+0x57c`，HGLRC 在 OpenGL 侧 `+0x580`。
- **纹理对象**：OpenGL 侧纹理 ID 在对象 `[0xc]`；标志在 `[8]`（低 2 位=过滤查表键，bit 2/3=环绕，bit 4=mipmap，bit 5=像素步长）；宽/高在 `[5]`/`[6]`。
- **RTTI 类型**：`CGxDevice`、`CGxDeviceD3d`、`CGxDeviceOpenGl`、`CGxMesh`、`CGxD3dMesh`、`CGxMat`、`CGxStateBom`、`CGxAppRenderState`、`CGxPushedRenderState`、`CGxVertexPNT0`（P=位置、N=法线、T=纹理坐标、0=无第二纹理坐标的顶点格式）、`CModelHash`、`CTextureHash`、`CTextureCacheRow`、`CAnimCameraObj`、`CAnimLightObj`、`CLightList`、`CRibbonVertex`、`CModelFrameHashNode` 等。

## 10. 与其它阶段的关系

- **S2 主循环**：GameMain → `FUN_6f027fb0` → `FUN_6f027e20` 是图形子系统的唯一初始化入口；每帧渲染的调用方未在本轮定位（场景提交函数 `FUN_6f0f0cc0` / `FUN_6f0ed080` 在 Ghidra 里 0 个直接调用者，推断经 `CGxDevice` 虚表分发，见 §11）。
- **S0 帧同步 / S3 模拟**：渲染只消费模拟状态、不产生模拟输入——渲染快慢不影响锁步（S0 的 25 ms 固定步长与渲染帧率解耦，**解耦机制本身未验证**，见 §11）；回读（截屏/快照）不进入校验和。
- **S6 UI**：UI 帧（`CBackdropFrame` 等）最终经 `CGxDevice` 的纹理与图元批量绘制；字体纹理图集（`FontTexture`）是 UI 文本渲染的底座。
- **S7 MPQ**：`.mdx` 模型与 `.blp` 纹理都经 MPQ 读出后再由本阶段的加载器（`FUN_6f726570` 等）解析上传；本轮只确认了引用关系，未深挖加载流程。
- **S4 JASS2**：脚本可通过 native 函数调渲染相关能力（如 `SetCameraBounds` 类 native 经触发器改相机），但相机/视图矩阵的 Gx 侧实现未在本轮覆盖。
- **S8 音频 / S9 录像**：`GxVideo`（过场视频）与音频播放的同步关系未验证；录像回放是否复用同一场景提交路径未验证。

## 11. 未验证点

1. 每帧渲染的完整调用链：引擎循环 `FUN_6f05f710`（S2）到 `CGxDevice` 虚表场景提交之间经过哪些函数（场景提交函数 0 直接调用者，须经虚表调用边追踪）。
2. `FUN_6f0ec240` 里能力标志位（`+0xb0…+0xd0`）各位的具体含义，以及 D3D 格式码 `0x16/0x17/0x18` 与 `D3DFMT_*` 的逐一对应。
3. `DAT_6fbc5e90` / `DAT_6fbc5e94` 对应的 GL 扩展身份；`FUN_6f0ea500`（1321 字节 GL 状态初始化）的具体内容。
4. `FUN_6f0e7b70`（OpenGL 模式枚举，`FUN_6f0e3190` 的 OpenGL 分支）与 `FUN_6f027960`（显示模式选择对话框逻辑）的本体。
5. `FUN_6f0ec400` 里虚表 `+0x28` 的第二次 COM 调用的具体方法身份。
6. 纹理标志位（`[8]` 各 bit）与 `FUN_6f0eb9b0` 像素格式转换的完整语义；BLP 解码与 MDX 加载的完整流程（`FUN_6f726570`、`FUN_6f6f0260` 等未逐个反编译）。
7. `GxuFont` 字形光栅化与图集打包、`GxVideo` 视频解码、`GxuLight` 光照计算的细节。
8. 渲染帧率与 S0 固定 25 ms 模拟步长的解耦机制（是否掉帧跳过渲染、插值与否）。
9. 回读路径（`FUN_6f0ed080` / `FUN_6f0f0cc0`）的实际调用方（截屏、小地图、还是过场），以及 `FUN_6f0e2e90`（工厂成功后调用）的身份。
10. `FUN_6f0475a0`（`FUN_6f027e20` 内反复调用的 7 字节函数）与 `FUN_6f02c060`（664 字节窗口/模式前置工作）的身份。
