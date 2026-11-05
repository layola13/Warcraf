# Warcraf 仓库引擎分析

- 仓库：`https://github.com/layola13/Warcraf`，本地 `~/workspace/Warcraf`
- HEAD：`97b55c0d630ae19f50013b3a70693ab0d6a94937`，唯一提交 "first"（2026-11-05，sonygod）
- 内容：只有 6 个 32 位 Windows DLL，共 21 MB，无源码、无 README、无可执行文件

## 版本定性

- **Warcraft III 1.27.0.52240（f7ba2a3）**，即 1.27a 时代构建
- PE 时间戳 2016-02-12，版权 2002–2016，MSVC 2013 运行时（MSVCR120）
- 32 位 x86，GUI 子系统，Game.dll 基址 `6f000000`
- PDB 路径泄露内部源码树：`D:\BuildServer\3\work-git\warcraft3-repository\`

## 模块分工

| DLL | 大小 | 角色 |
|---|---|---|
| Game.dll | 13.2 MB | 游戏本体：模拟、JASS 脚本虚拟机、UI、渲染抽象层 |
| Storm.dll | 334 KB | Blizzard Storm 库：MPQ 档案、网络、线程/同步原语 |
| Mss32.dll | 348 KB | Miles Sound System：3D 音频（319 个 `AIL_*` 导出） |
| ijl15.dll | 373 KB | Intel JPEG Library：图片解码 |
| msvcp120.dll / msvcr120.dll | 445 KB / 961 KB | MSVC 2013 C++ 运行时 |

## Game.dll 内部结构（来自 RTTI / PDB 路径 / 字符串）

- **唯一导出：`GameMain`** —— war3.exe（不在仓库里）就是调它启动游戏
- **War3/Source/**：`Game`（模拟：CGroup、CSaveGame、JassWar3.cpp、寻路 CPathingFootprint）、`UI`（CSkinManager、CChatDialog、血条/背包/英雄栏等）、`Glue`（菜单/战网界面，44 处引用）、`WorldEdit`（与编辑器共享代码）
- **Engine/Source/**：`Gx`（渲染抽象：CGxDeviceD3d / CGxDeviceOpenGl 双后端，CGxuLight、渲染批 RenderBatch）、`Tempest`（容器/分配器模板）、`Ipse`、`FDFile`（文件层）
- **engine/source/jass2/**：JASS2 脚本虚拟机 —— CJassFunc、JassThreadLocal、JassArray、JassHandle 哈希表、`TriggerExecute`、`scripts\common.j`
- **导入**：Storm.dll、mss32.dll、ijl15.dll、OPENGL32.dll、WININET.dll、WS2_32.dll（战网联网）、WINMM、COMCTL32 等

## Storm.dll（814 个导出，序号基数 101）

- `SFile` 族：Open/Read/Load/Close、overlapped 异步读写 —— MPQ 档案系统
- 线程与同步：SCreateThread、SCritSect、SRWLock、SSyncObject、SEvent
- `SErrSetBlizzardErrorFunction` 等错误处理

## 架构全景

```
war3.exe（不在仓库）──► Game.dll:GameMain
    ├── 模拟层：单位/触发器/存档/寻路
    ├── JASS2 VM：common.j 原生函数、触发器执行
    ├── UI / Glue：游戏内界面 + 菜单
    ├── Gx 渲染抽象：D3D8 / OpenGL 双后端
    ├── Storm.dll：MPQ 读档 + Battle.net 联网
    ├── Mss32.dll：3D 音频
    └── ijl15.dll：JPEG 解码
```

## 局限

- 仓库里只有二进制，没有源码，"分析引擎"只能做到二进制层面（导出表、导入表、RTTI、字符串、PDB 路径）
- Game.dll 只导出一个函数，内部全部是静态链接逻辑；要做函数级逆向需要 IDA/Ghidra 反汇编
- 这是一份 1.27a 的 DLL 转储，不是现代重制版（Reforged）引擎
