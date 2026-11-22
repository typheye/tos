# 桌面端统一迁移记录

日期：2026-11-22。正式项目：`/home/typheye/Code/tos/pc-app`。

## 后续调整：移除 Ubuntu 启动项（2026-11-22）

按用户要求删除 Ubuntu 的 TOS Helper 应用菜单项，并移除 VS Code Install 按钮、
Linux `.desktop` 内容生成、写入、桌面路径查找和应用菜单注册逻辑。
Linux `install` 仅复制程序和资源，`--desktop` 仅用于 Windows。
CI 改为检查 Linux 安装后没有生成启动项。以下表格记录迁移当时的验证，
其中 Ubuntu 启动项状态已由本节更新。

## Git 与源码来源

工作区顶层不是 Git 仓库，两端原来是独立 checkout，远端均为
`https://github.com/typheye/tos`。没有发现本项目适用的 AGENTS.md 或 .code-workspace。

| 项目 | 迁移前分支 / 提交 | 处理 |
|---|---|---|
| linux-pc | linux-pc / a3b249ce4dc93ac2554bd321319d43ad7e070a89 | 本地克隆保留完整历史，复制当前工作区文件作为功能基线 |
| win-pc | win-pc / 1a48904e291d838ea0c4bbcd18f37828d65e6f34 | 历史保存在新仓库的 refs/remotes/legacy/win-pc；合入平台配置和依赖 |
| pc-app | 本地 pc-app 分支，以 Linux HEAD 为基线 | 迁移改动尚未提交，无上游分支 |

原 Linux 未提交的 README、依赖、Helper 源码、元数据和版本修复均纳入新项目。
原 Windows 的 Helper 源码与 Linux 当前源码相同，其未提交修复也因此保留。
两个旧目录没有移动、覆盖或删除：除 README 顶部添加“已迁移，停止维护”提示外，
逐文件 SHA256 比对确认原有文件内容保留，包括原未提交业务修改。

未提交、推送、发布版本，未修改远端分支或默认分支。后续正式提交/推送前请审阅
pc-app 工作区，以及 main 总说明的修改。新文件目前为正常未跟踪文件，不要执行 git clean。
`_guide/` 中当前操作指南已更新；2026-11-06 会话交接文件保留历史原貌。

## 核实的差异与合并

- 两端 TOS Helper / TVID 当前业务源码相同；Linux 的 SBL/TDB 多了权限错误链识别、
  dialout 登录组诊断和设备枚举时的权限提示，全部保留。
- Linux 权限函数原本会在 Windows 错误分支调用 os.getuid；现集中到平台模块，
  Windows 提供 COM 占用/驱动提示，Linux 才导入 grp、检查登录组。
- Windows 独有的 pyreadline3 依赖改为平台条件依赖，保留 TDB 交互补全。
- Windows ICO、DPI manifest、文件属性迁入统一 spec；Windows DPI API 声明原生参数类型，
  增加任务栏 AppUserModelID，保留 HID path → VID/PID/serial 打开回退。
- 使用 Tkinter。没有 Qt、QSettings 或应用 OpenGL 窗口，不引入额外适配依赖。
- 版本只从各工具 metadata.json 读取；删除新项目内各平台独立 spec 和手工版本资源，
  用 packaging/tos.spec 构建四个程序，冻结包包含元数据、图标和实际依赖。
- TDB 配置标识和结构不变：用户主目录下 `.tdb_device.json`。没有读取真实设备或改写用户配置。
- 迁移时 Setup/Play/Build/Install/Clean/Test/Doctor 任务均保留 label、icon.id、tooltip 和平台入口，
  保留默认 Build 与 quzhen.tasks-button 推荐，没有安装编辑器扩展。

## 本机验证结果

宿主：Linux x86_64；Conda 环境沿用 `tos`，Python 3.11.17。
显式执行 setup 完成环境更新：hidapi 0.15.0、pyserial 3.5、PyInstaller 6.22.3、
opencv-python 4.14.0.94；Linux 不安装 pyreadline3。

| 检查 | 结果与范围 |
|---|---|
| 回归检查 | 27 项通过，包含真实 Tk 窗口、图标、主题和实际窗口类 Toshelper |
| 环境与参数 | doctor、自动 Conda 路由、当前环境、中文/空格源码目录、参数转发及真实错误码通过 |
| 源码启动 | 四个 app.py 的版本入口、Helper 无设备 GUI 启动通过 |
| 原生构建 | 四个 Linux 单文件程序构建并通过 --version 检查 |
| 冻结资源 | 冻结 Helper 的 GUI / PNG 与元数据可加载，CLI 版本可读取 |
| 构建失败保护 | 真实缺依赖构建失败退出且旧 dist 全部文件哈希不变；中途失败、缺产物和发布回滚另有回归检查 |
| 独立安装 | `/home/typheye/.local/share/tos-pc`，四个程序及 PNG/ICO 已安装；重复安装成功 |
| 启动项 | `/home/typheye/.local/share/applications/tos-helper.desktop` 通过 desktop-file-validate，实际启动命令可运行无设备 GUI 检查 |
| 清理后运行 | 真实 clean 删除 build、保留 dist；安装版四个版本入口及 GUI 仍可运行 |
| TVID 实际转换 | 中文/空格视频与输出路径，帧区间、32×32、10 fps、RLE，源码/安装版均生成 2 帧，输出逐字节相同 |
| 原配置兼容 | 使用临时既有配置检查读取与字段保留；真实用户配置路径和键未变，无写入 |
| 原仓库保护 | 两端原文件哈希比对通过，仅 README 新增迁移提示 |
| 静态检查 | Shell 语法、Git diff --check、任务 JSON 通过 |

迁移时应用菜单使用安装目录内图标和程序，Terminal=false、StartupNotify=false、
StartupWMClass=Toshelper，与真实 Tk 窗口类一致。没有创建额外桌面快捷方式。
CLI 位于安装目录下的 platform-tools，未修改系统 PATH。

## 待 Windows 环境验证

已配置 `.github/workflows/pc-build.yml`，在 Linux / Windows 分别安装依赖、运行测试、
原生构建、独立安装、清理后启动，并上传 dist。工作流尚未推送或运行。

本机未执行 Windows 原生构建、GUI、高 DPI、任务栏归属、COM 权限、readline 补全和
开始菜单/桌面快捷方式验证。模拟平台分支的单元测试只说明调用隔离和参数构造正确，
不能替代上述原生验证。USB 通信、烧录、擦除和重启设备均未执行。

## 统一命令

在 pc-app 下：Linux 使用 `bash scripts/pc.sh <操作>`，Windows PowerShell 使用
`./scripts/pc.cmd <操作>`，操作为 setup/run/build/install/clean/test/doctor。
已有 Python 环境加 `--current-env`。`clean --all` 才额外删除 dist，仍不删除安装。
详见 [README](../README.md) 和 [开发指南](developer-guide.md)。
