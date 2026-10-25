# DesktopTodo

轻量 Windows 桌面待办。当前应用采用 C++20、Win32 和 SQLite；Native SDK
验证工程保留在 `prototype/` 和 `window-probe/`，不参与产品运行。

当前另有使用 **nim_duilib + Skia** 的原生组件库候选版，复用现有业务和存储模块，
支持任务拖拽过渡动画与锁定后的只读滚动。菜单、设置和管理窗口仍沿用原生实现。
详见 [组件库前端说明](nim-ui/README.md)。

```powershell
# 还需 Visual Studio 2022 C++ Build Tools、Windows SDK 和 7-Zip
./scripts/bootstrap-app.ps1
./scripts/bootstrap-nim-ui.ps1
npm run build:nim-ui
npm run test:nim-ui
```

组件库版入口：`dist/nim-ui/DesktopTodo-Nim.exe`，移动时请同时保留 `resources/`。
两个入口默认各自在程序旁保存数据；升级或对比时，需通过存储设置连接原数据目录，
否则会看到一份新的空数据库。已有数据不会自动被另一入口覆盖。
仓库仅包含源码与测试资料，不包含个人数据库、备份、依赖缓存或编译产物。

See [validation results](docs/native-sdk-validation.md) for observed failures,
reproduction commands and untested capabilities.
See [desktop integration](docs/desktop-integration.md) for desktop attachment,
mode transitions and actual cross-process mouse-input results.

## 构建与检查

需要 Windows x64、Zig 0.16.0、Node.js 和 Python 3。

```powershell
./scripts/bootstrap-app.ps1
npm ci --prefix web-comparison --no-audit --no-fund
npm run check
npm run test:app
npm run build:app
python scripts/test-app-integration.py
./scripts/measure-app.ps1
```

产物为 `dist/DesktopTodo.exe`，直接运行。无需安装 Native SDK 或浏览器运行时。
默认在程序同目录创建 `todo.sqlite`，使用 SQLite 即时保存；`backups/` 保存备份。
旧版 `%LOCALAPPDATA%/DesktopTodo` 数据会在首次启动时自动导入，旧库保留为安全副本。
请将便携版放在可写目录。
移动可执行文件后，如启用了开机启动，应在管理窗口重新关闭、开启该选项。

## 使用

- 托盘图标打开便签管理，可新建、删除便签，设置开机启动和启动时锁定。
- 便签顶部菜单设置标题、背景颜色、桌面/置顶/普通窗口模式。
- 底部输入框按 Enter 添加待办；双击标题或任务文字编辑。
- 点击状态圆点切换完成状态；任务右键菜单可选未完成、进行中、已完成，
  以及图标、文字颜色和删除。顶部筛选菜单显示对应分类。
- 拖动任务左侧把手排序，也可拖到另一便签；拖动标题移动便签，右下角调整大小。
- 桌面模式右上角使用同一个锁形按钮，点击切换锁定 / 解锁，位置不跳动。
  锁定时正文穿透鼠标；原有四角位置和边距设置仍保留，对两种状态共同生效。
- Ctrl+Z / Ctrl+Y 撤销、重做；管理窗口可导出与恢复备份。
- 普通窗口关闭时隐藏便签，管理窗口可重新显示；托盘菜单退出程序。
- 管理窗口的「存储设置」可更改目录并自动迁移、打开数据目录、整理数据库。
  目标已有数据库时不会覆盖；程序目录内的位置以相对路径保存，设置文件为 `DesktopTodo.ini`。
- 清空当前数据库需要连续三次确认；不新增备份，不删除已有备份，也不能撤销。
  手动导出备份独立使用；自动备份在每日首次启动时执行，保留最近 7 份。
- 当前仍是便携版，没有安装器或卸载器。退出后可删除程序文件夹；若选择了外部数据目录，
  该目录不会随程序文件夹自动删除。删除前建议关闭开机启动。

## 模块与验证

`app/src/core` 管理领域规则与事务控制；`storage` 实现 SQLite 存储；
`platform` 封装 Windows 窗口与启动注册；`ui` 使用原生控件和 GDI 绘制。
领域、存储、窗口策略和启动适配器均可无头测试，真实 Explorer 行为另行验证。

Lefthook 在提交和推送前执行文本文件少于 600 行门禁和原生测试。
`npm run test:native` 还包含历史 SDK 实验，需要额外运行
`./scripts/bootstrap.ps1` 安装其实验依赖。

当前已通过 55 项原生无头测试、55 项通用程序集成检查，以及 21 项存储专项检查，详见
[集成结果](docs/app-integration-results.json)。多屏高 DPI、Explorer 重启和
“显示桌面”交互仍需进一步实机验证，尚不能视为完成全部验收。

[本机资源测量](docs/app-performance-results.json)：10 个便签、1000 条待办，
工作集约 35.54 MiB、私有内存约 2.58 MiB；10 秒静置样本未测得 CPU 时间增长。
结果受操作系统和运行环境影响。

存储专项可用 `python scripts/test-storage-management.py` 重现，结果见
[存储检查](docs/storage-management-results.json)。输入框的焦点边框、光标、中文输入和提交
已通过实际鼠标键盘检查。

独立的 [Svelte / 系统 WebView2 对比实验](web-comparison/README.md) 使用 Flowbite Svelte、
Lucide 和现成拖拽组件，复用 C++ 待办核心。它不替换原生版；普通窗口资源对比见
[测量报告](docs/web-comparison.md)。

另有真正使用 Native SDK 0.11.0 的 [Todo 小样](sdk-comparison/README.md)，
采用 SDK 内置组件与 Geist 主题；[单次粗测结果](docs/sdk-comparison.md)独立记录，
没有替换 C++ 主程序。
