# 开发指南

正式源码在 `src/{tos_helper,tvidtool,sbltool,tdb}/app.py`。
`src/tos_common` 管理平台适配、资源查找和元数据。入口兼容直接运行 app.py，亦可通过
`scripts/manage.py run --tool <工具> -- <参数>` 运行。保留 `tos` 环境名和现有 TDB 配置路径。

## 修改与验证

1. 首次 `bash scripts/pc.sh setup`（Windows 使用 `./scripts/pc.cmd setup`）。
2. 业务修改只做一份；平台判断集中使用 `sys.platform`，路径使用 pathlib。
3. 使用 `test` 回归；有显示服务时会创建真实 Tk 窗口，USB 调用被禁止。
4. `run -- --smoke-test` 验证 GUI 和图标，自动退出且不接触设备。
5. `build` 构建宿主产物；`install` 安装；`clean` 后检查安装版。
6. 更改版本只编辑相应 `metadata.json`。禁止恢复手工版本资源副本。

Linux 无显示服务时使用 `xvfb-run -a`；Windows GUI 验证必须在 Windows 上进行。
测试包括配置兼容、HID 回退、RGB565 字节序、参数转发、退出码、Conda 路由、资源路径、
失败保留旧 dist、发布失败回滚、清理保护、安装边界和 VS Code 任务按钮。
模拟 Windows 的单元测试只验证分支行为，不代表 Windows 实机验收。

## 构建与安装实现

`packaging/tos.spec` 为四个工具执行 Analysis/EXE，所有产物均为单文件程序。
Windows 按 metadata 动态构造 VSVersionInfo，Helper 附带 ICO 和 DPI manifest。
Helper 的 PNG/ICO、各工具 metadata.json 均进入冻结包；外部图标同时进入 dist/assets。
PyInstaller 单文件运行时使用临时解包目录，资源通过 tos_common.resource 查找。

构建临时目录在 `build/native-*`，完整产物通过版本检查后重命名替换 dist。
发布重命名失败恢复旧目录；异常中断残留 `dist.previous` 时应先人工检查再处理。
不允许默认删除旧 dist。安装同样先复制、校验后替换，安装标记为 `.tos-pc-install.json`。

安装启动项使用安装内资源。Linux 窗口类为 `Toshelper`，与 StartupWMClass 一致；
Windows DPI API 显式声明指针参数和返回类型，并设置 `Typheye.TOSHelper` 任务栏标识。
Windows 快捷方式通过 PowerShell / WScript 创建，配置用 JSON 传递以避免路径作为脚本执行。

`clean` 预检删除范围，拒绝符号链接，不清理用户配置和安装。`--all` 仅增加 dist。
原有 scripts/build.sh 和 clean.sh 为 Linux 兼容包装，不包含独立构建逻辑。

## CI 与发布

工作流假定本仓库位于 pc-app 分支根目录，分别在 ubuntu-latest / windows-latest
使用 Python 3.11 + `--current-env` 执行测试、构建、安装、清理和启动检查，上传 dist。
如未来合入单仓库子目录，应相应调整 checkout 后的工作目录。

此迁移没有提交、推送、创建远端分支或发布版本，也未连接设备。
