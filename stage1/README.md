# 阶段一：网页 wasm 能玩 — 验收报告

结论：**通过**。在无 GPU 的专用机上，用无头 Chrome（SwiftShader 软件光栅化）
实际加载 OpenRealm 的 Warcraft III wasm 构建，导入用户自备绿色包的 MPQ 数据，
进入冰封王座中文主菜单，并进入战役第一章的真实对局画面（地形/单位/完整 HUD 同框）。

## 路线选型

- **主选（已跑通）**：corepunch/open-realm（OpenRealm）的 wasm 构建（社区所称
  wc3-wasm 路线）。C99 Quake 式引擎、MIT 许可、Warcraft III 为首要可玩目标，
  渲染走 OpenGL ES 3 → Emscripten WebGL2，与「无 GPU + SwiftShader」口径兼容。
- 备选（未启用）：Retera/WarsmashModEngine 的 TeaVM 网页构建（AGPL-3.0，产物
  为 JS/WebGL 程序而非 wasm 二进制）——主选已通过验收，按纪律不切换。
- 原版二进制转译/模拟路线：无成熟开源形态，未选。

## 验收环境

- 场地：Kaggle 会话专用机（4 核 / 31 GB RAM / 无 GPU），页面在专机本地以
  `python3 port/web/serve.py` 服务（127.0.0.1:8000，COOP/COEP 跨源隔离头齐全）。
  **最终验收走本地形态**；公网代理后备（proxy.cxu.lol / get.sh）**未触发**
  （WebGL 上下文创建成功、首帧渲染正常、资源加载无失败，不满足触发条件）。
- 无头浏览器：Google Chrome **157.0.8103.38**，启动旗标：
  `--headless=new --no-sandbox --use-angle=swiftshader --enable-unsafe-swiftshader
  --enable-webgl --ignore-gpu-blocklist --window-size=1280,960
  --autoplay-policy=no-user-gesture-required --mute-audio`
  （Ubuntu 的 chromium snap 占位包在容器内不可用，故装官方 Chrome .deb。）
- 引擎构建：`make web-warcraft3-engine`（emscripten），产物 `index.wasm`
  3,469,470 B；构建日志 BUILD_RC=0。
- 数据：绿色包主树四件 MPQ（war3.mpq / War3x.mpq / War3xLocal.mpq /
  War3Patch.mpq，共 920.0 MiB）经页面导入器写入浏览器 OPFS，页面回执
  “Stored 4 game file(s) locally”，Launch mode = The Frozen Throne。
  素材原件未上公网、未入仓；入仓的只有本报告、截图证据与驱动脚本。

## 实测帧率（SwiftShader 纯 CPU 软件渲染，无 GPU）

| 场景 | FPS | Drawcalls |
|---|---|---|
| 主菜单（3D 场景） | 8（首轮）/ 4–5（驱动轮） | 137–168 |
| 战役选择/章节列表 | 4–5 | 155–188 |
| 对局内（第一章可玩段） | 2–4 | 251–368 |

慢但可玩：菜单导航、战役进入、单位/HUD 渲染、任务系统均实际工作。

## 证据截图（evidence/，均为无头浏览器全页截图，未裁剪）

1. `01-main-menu.png` — 冰封王座中文主菜单（3D 主菜单场景 + 全部菜单按钮）。
2. `02-campaign-chapters.png` — 单人模式 → 战役 → 暗夜精灵战役「恐怖之潮」章节列表。
3. `03-ingame-cinematic-terrain.png` — 第一章开场过场中的游戏世界（地形/单位/UI 边框，窗口化画布）。
4. `04-ingame-playable.png` — **对局画面（主证据）**：全屏，森林地形、月亮井、城门、
   7 个暗夜精灵单位与完整 HUD 同框；英雄玛维（等级 2 守望者，HP 600/600、MP 255/255）
   被选中，任务文本「主要任务：恶魔的行踪 — 玛维必须生存」在屏。
5. `05-ingame-hud-quests.png` — 对局 HUD 与任务文本帧（该帧镜头位于未探索区域，视口为黑场，
   作为 HUD/任务系统的补充证据，不单独作通过依据）。

## 进入对局的路径（如实登记，含失败路）

- 菜单导航（CDP 持久驱动 `harness/drive.js`）：主菜单 → 单人模式 → 战役 → 战役/章节
  选择均可达；点击须用「移动 → 按下保持 1.2–1.6 s → 抬起」的保持式点击，
  瞬时点击在 2–6 FPS 下会被引擎漏检；键盘热键在驱动会话中未生效。
- **失败路 1（自定义游戏）**：单人 → 自定义游戏与局域网 → 创建游戏两路的地图列表
  均为空。根因（OPFS 实查 + 导入器源码）：导入器对散装文件只按裸文件名存入安装代
  根部，引擎的地图枚举只认 `Maps/…` 路径与 MPQ 内地图；散装 w3x 再导入两次均未
  提交新安装代。此路未通，改走战役路（战役地图在 War3x.mpq 内）。
- **失败路 2（章节点击）**：章节列表播放钮/标题行点击后，引擎未开始加载（60 s 无变化）。
  源码核对：章节点击在引擎内最终排队控制台命令
  `map "Maps\FrozenThrone\Campaign\NightElfX01.w3x"`（single_player.c
  `SinglePlayer_LaunchMission`），点击未生效的原因未在 UI 层定位。
- **成功路（引擎命令行直达）**：`harness/startmap*.js` 在页面内拦截
  `window.openrealmStartEngine`，给引擎启动参数追加引擎自带的 Quake 式命令
  `+map Maps\FrozenThrone\Campaign\NightElfX01.w3x`。控制台实证：
  `CL_SendBegin: sending begin world="Maps\FrozenThrone\Campaign\NightElfX01.w3x"`、
  `G_ClientBegin: … name="Player"`、世界占用呈现后菜单命令被拒。
  其后用页面自带的 Esc 钮跳过开场过场、双击英雄头像把镜头拉到英雄队伍，
  得到 `04-ingame-playable.png`。这属于 CDP 输入注入推进对局的加分项实证。

## 驱动脚本（harness/）

- `drive.js` — 持久交互驱动（cmd.json 协议：click/key/shot/quit），菜单导航用。
- `startmap.js` — +map 直达启动 + 每 30 s 截图 + 全量控制台日志。
- `startmap2.js` — 同上 + 全屏 Play + 定时 Esc 跳过场。
- `startmap3.js` — 同上 + 英雄头像双击/小地图点击移动镜头（04 号证据出自此轮）。

复现：专机上 `python3 port/web/serve.py` 起服务后，用 Node 22 + puppeteer-core
运行相应脚本即可（Chrome 旗标见脚本内）。
