# TOS PC Tools — Linux / Windows

`pc-app` 是唯一维护的桌面客户端，包含 TOS Helper（Tkinter GUI + CLI）、
TVID 视频转换、SBL FASTBOOT 和 TOS Debug Bridge。以 Linux 当前源码及未提交的
HID 修复为基线，保留 Windows 原生支持。许可证为 GPLv2 或更新版本，见 [LICENSE](LICENSE)。

| 工具 | 当前版本 | 用途 |
|---|---|---|
| TOS Helper | 1.0.2 | HID 诊断，VID 0x0483 / PID 0x5750 |
| tvidtool | 0.1.0 | 视频转 TVID，大端 RGB565，支持帧区间 |
| sbltool | 1.0.2 | FASTBOOT CDC，PID 0x5751 |
| tdb | 1.0.1 | REC 调试桥、Shell、文件传输，PID 0x5756 |

## 快速开始

在 `pc-app` 目录下，Linux：

```bash
bash scripts/pc.sh setup
bash scripts/pc.sh run
bash scripts/pc.sh build
bash scripts/pc.sh install
bash scripts/pc.sh clean
```

Windows PowerShell：

```powershell
./scripts/pc.cmd setup
./scripts/pc.cmd run
./scripts/pc.cmd build
./scripts/pc.cmd install
./scripts/pc.cmd clean
```

统一管理脚本是 `scripts/manage.py`。默认自动查找 Conda，并在既有 `tos` 环境运行。
`setup` 创建或更新环境，Python 3.11 / Tk 由 `environment.yml` 定义；
所有 pip 依赖仅定义在 `requirement.txt`，`pyreadline3` 仅用于 Windows。
只有 `setup` 安装或更新依赖，普通运行和构建不会自动安装。

已有 venv、手动环境和 CI 可使用当前 Python：

```bash
python scripts/manage.py setup --current-env
python scripts/manage.py run --current-env
python scripts/manage.py build --current-env
```

Shell/CMD 包装入口也接受 `--current-env`。可用 `TOS_PYTHON` 指定启动 Python 的完整路径。
不支持的系统会明确报错；Linux 只能构建 Linux 程序，Windows 只能构建 Windows 程序。

## 常用操作

| 操作 | 说明 |
|---|---|
| `setup` | 显式安装依赖 / 更新 tos Conda 环境 |
| `run` | 默认启动 TOS Helper，`--tool` 选择其他工具 |
| `build` | 原生构建四个工具，通过版本检查后替换 dist |
| `install` | 安装到独立用户目录，创建应用菜单 / 开始菜单快捷方式 |
| `clean` | 删除 build 和 Python 缓存，保留 dist 和安装 |
| `clean --all` | 额外删除 dist，仍保留安装和用户配置 |
| `test` | 运行无需设备的 unittest 回归检查 |
| `doctor` | 显示宿主平台、Python、依赖、版本、产物和安装位置 |

参数通过 `--` 传给工具，支持空格和中文路径；下例中的 `python scripts/manage.py` 也可替换为
Linux 的 `bash scripts/pc.sh` 或 Windows 的 `./scripts/pc.cmd`：

```bash
python scripts/manage.py run --tool tvidtool -- input.mp4 -o test.tvid --fps 60 --size 240x240 --start-frame 0 --end-frame 1000
python scripts/manage.py run --tool sbltool -- --help
python scripts/manage.py run --tool sbltool -- examples
python scripts/manage.py run --tool tdb -- --help
python scripts/manage.py run -- --smoke-test
```

`--smoke-test` 会创建真实 Tk 窗口、加载图标和主题并自动退出，不枚举或连接 USB。
正常 GUI 启动保留原有自动连接行为。Linux USB CDC 权限不足时会提示 dialout/重新登录；
Windows 提示检查 COM 占用与设备管理器，公共模块不会调用另一平台的专属 API。

打包后可使用：

```text
dist/
├── TOS Helper[.exe]
├── assets/                       # 安装启动项所需外部图标
└── platform-tools/
    ├── tvidtool[.exe]
    ├── sbltool[.exe]
    └── tdb[.exe]
```

TVID 输出复制到设备 SD 卡的 `0:/storage/test.tvid`，进入 `Demo → TVID Test` 播放。
SBL 支持 `devices`、`getvar all`、`partitions`、`flash`、`erase`、`reboot` 和 `oem`。
TDB 支持 `devices`、`connect`、`disconnect`、`shell`、`push`、`pull`。
使用 `-s <STM32 UID>` 选择设备，勿依赖变化的 COM/tty 端口。
`oem unlock` 会清空设备用户数据，按工具提示操作。

## 构建与安装边界

所有工具由一份 [packaging/tos.spec](packaging/tos.spec) 打包，版本唯一来源是各工具的
`metadata.json`；Windows 文件属性由其动态生成，HID 图标、DPI manifest 和命令行补全一并打包。
冻结程序内含元数据和图标。两平台均保留控制台 CLI 能力；Windows GUI 快捷方式最小化控制台。

构建先写入临时目录，四个程序通过 `--version` 检查后才替换 `dist`；失败保留上一版。
安装从已有 `dist` 复制，不自动构建：

- Linux：`${XDG_DATA_HOME:-~/.local/share}/tos-pc`，应用菜单 `applications/tos-helper.desktop`。
- Windows：`%LOCALAPPDATA%\Programs\TOS PC Tools`，开始菜单 `TOS Helper.lnk`。
- `install --desktop` 同时创建桌面快捷方式；已有本应用桌面快捷方式会更新。
- `install --install-dir <绝对路径>` 可指定独立目录，禁止安装进源码或其父目录。
- 不覆盖未标记为本应用安装的已有目录；启动项只更新本应用，不改变其他应用。
- 安装内含四个程序和图标，不依赖源码、Conda、build 或 dist。CLI 可从安装目录的
  `platform-tools` 直接运行；如需全局命令，请自行将该目录加入 PATH。

TDB 配置仍使用 `~/.tdb_device.json`，字段保持原样。迁移、安装和清理不会改写它。
VS Code 保留 `quzhen.tasks-button` 推荐，Setup/Play/Build/Install/Clean/Test/Doctor
按钮均有 `label`、`icon.id` 和提示，Build 仍为默认构建任务。

## 迁移与验证

新仓库保留 Linux 提交历史，当前本地分支为 `pc-app`；Windows 提交保存在本地
`refs/remotes/legacy/win-pc`，原有 `linux-pc/`、`win-pc/` 仓库及未提交修改保留，停止维护。
远端地址仍为 `https://github.com/typheye/tos`，尚未创建远端 pc-app 分支。

验证结果与范围见 [迁移记录](docs/migration.md)，开发细节见
[开发指南](docs/developer-guide.md) 和 [技术手册](docs/technical-docs.md)。
[GitHub Actions](.github/workflows/pc-build.yml) 配置 Linux / Windows 原生测试、构建和安装验证；
本地 Windows 原生构建、DPI/任务栏/快捷方式验证待 Windows 环境执行。
