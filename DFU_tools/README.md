# TOS 固件 DFU 升级工具

这是一个给客户使用的 Python Qt GUI 升级工具，用于把编译生成的 `tos.elf` 通过 STM32 USB DFU 写入设备。

## 推荐使用方式

1. 让设备进入 STM32 DFU 模式。
   - 常见方式：BOOT0 拉高后复位，或使用你们板子的 DFU 按键/跳线。
2. 用 USB 连接设备和电脑。
3. 运行 GUI：

   ```bash
   cd DFU_tools
   pip install -r requirements.txt
   python run_gui.py
   ```

4. 选择编译输出的 `tos.elf`。
5. 点击「检测 DFU」。
6. 点击「开始升级」。

## 刷写后端

GUI 目前支持两个后端。

### 1. dfu-util 后端

优点：轻量，适合将 `dfu-util.exe` 和 GUI 一起打包给客户。

工作流程：

```text
TOS tos.elf -> GUI 内置 ELF 转 BIN -> dfu-util 写入 0x08000000 -> leave/reboot
```

GUI 会把 `tos.elf` 转换成同目录下的 `tos.dfu.bin`，然后执行类似命令：

```bash
dfu-util -d 0483:df11 -a 0 -s 0x08000000:leave -D tos.dfu.bin
```

工具查找顺序：

1. `DFU_tools/config.json` 里的 `dfu_util_path`
2. `DFU_tools/bin/dfu-util.exe`
3. 系统 PATH

Windows 下如果 dfu-util 检测不到 `0483:df11`，通常是驱动问题。可用 Zadig 把 STM32 BOOTLOADER/DFU 设备驱动切换为 WinUSB。也可以改用下面的 STM32CubeProgrammer CLI 后端。

### 2. STM32CubeProgrammer CLI 后端

优点：ST 官方工具，Windows 驱动兼容性通常更省事。

要求：客户电脑安装 STM32CubeProgrammer。

GUI 会执行类似命令：

```bash
STM32_Programmer_CLI -c port=USB1 -w tos.elf -v -rst
```

工具查找顺序：

1. `DFU_tools/config.json` 里的 `stm32_cli_path`
2. 系统 PATH
3. Windows 默认安装路径

## 默认参数

`config.json` 默认值：

```json
{
  "flash_base": "0x08000000",
  "flash_size": "0x00100000",
  "dfu_vid_pid": "0483:df11",
  "dfu_alt": "0"
}
```

这些参数对应 STM32F407VG：内部 Flash 起始地址 `0x08000000`，Flash 大小 1MB，ROM DFU VID:PID 通常为 `0483:df11`。

## Windows 打包

在 Windows 上运行：

```bat
cd DFU_tools
package_windows.bat
```

生成路径：

```text
DFU_tools\dist\TOS_DFU_Upgrade\TOS_DFU_Upgrade.exe
```

如果使用 dfu-util 后端，请把 `dfu-util.exe` 放到：

```text
DFU_tools\dist\TOS_DFU_Upgrade\bin\dfu-util.exe
```

## 文件结构

```text
DFU_tools/
  run_gui.py                    GUI 入口
  config.json                   默认刷写参数
  requirements.txt              Python 依赖
  build_windows.bat             Windows 直接运行
  package_windows.bat           Windows 打包脚本
  bin/                          可放 dfu-util.exe
  tos_dfu_gui/
    app.py                      Qt 界面
    dfu.py                      DFU 后端/命令执行
    elf2bin.py                  内置 ELF -> BIN 转换
    qt_compat.py                PySide6/PyQt5 兼容层
```

## 注意事项

- 升级过程中不要断电或拔 USB。
- `dfu-util` 后端只写内部 Flash，不会自动处理 Option Bytes。
- 如果你的 Bootloader/应用需要特殊分区，不是从 `0x08000000` 启动，请修改 `config.json` 的 `flash_base`。
- 如果客户电脑不能安装驱动，建议优先使用 STM32CubeProgrammer CLI 后端。
