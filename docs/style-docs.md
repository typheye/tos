# 代码风格

> 适用于 `src/` 下所有 Python 代码。

---

## 1. 文件头模板

每个 `.py` 文件以模板开头（仿下位机 Doxygen 风格）：

```python
#!/usr/bin/env python3
"""
 ******************************************************************************
 * @file    filename.py
 * @author  Typheye
 * @brief   One-line description ending with period.
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 ******************************************************************************
 """
```

规则：
- `#!/usr/bin/env python3` shebang 必须第一行
- `@brief`：实现文件用 `XXX implementation.`，包文件用 `XXX package.`
- 结尾带句号
- `@author` 固定为 `Typheye`

---

## 2. 命名规范

| 条目 | 风格 | 示例 |
|------|------|------|
| 文件名/包名 | `snake_case` | `tos_helper/`, `sbltool/` |
| 类名 | `PascalCase` | `HidDevice`, `SblProtocol` |
| 函数/方法 | `snake_case()` | `list_devices()`, `send_command()` |
| 模块级常量 | `UPPER_SNAKE_CASE` | `DEFAULT_VID = 0x0483` |
| 私有函数/方法 | `_snake_case()` | `_probe_port()` |
| 私有属性 | `_snake_case` | `self._port`, `self._connected` |
| 类型变量 | `PascalCase` | `DeviceInfo`, `PartitionTable` |

---

## 3. 缩进与格式

- **缩进**：4 空格，不用 tab
- **行长**：≤ 100 字符（PEP 8 推荐 79，PC 端工具放宽到 100）
- **空行**：函数定义之间 2 空行，类方法之间 1 空行
- **引号**：优先双引号 `"text"`（与 JSON 一致）

```python
def list_devices(vid: int = 0x0483, pid: int = 0x5751) -> list[DeviceInfo]:
    """Enumerate TOS devices by VID/PID on serial ports."""
    result: list[DeviceInfo] = []
    for port_name in serial_port_names():
        info = _probe_port(port_name, vid, pid)
        if info:
            result.append(info)
    return result
```

---

## 4. 类型注解

所有公共函数和方法的参数和返回值必须标注类型。建议使用 `from __future__ import annotations` 以避免运行时开销。

```python
from __future__ import annotations

def flash_partition(device: str, partition: str,
                    image_path: str, timeout: int = 30) -> bool:
    ...
```

---

## 5. 文档字符串

### 模块 Docstring

文件头部使用 `"""<描述>."""` 单行或简单段落。

### 函数 Docstring

PEP 257 风格，必要时加参数描述：

```python
def send_report(data: bytes) -> bool:
    """Send a HID Vendor OUT report.

    Args:
        data: 64-byte report payload.

    Returns:
        True if the report was queued successfully.
    """
```

---

## 6. 导入顺序

按以下分组，每组之间空一行：

1. Python 标准库
2. 第三方库
3. 本地模块

```python
import argparse
import ctypes
import sys

import serial

from .protocol import SblProtocol
```

---

## 7. 错误处理

- 使用异常传播，不静默吞异常
- CLI 工具用 `argparse` 解析参数，错误时 `sys.exit(1)`
- 串口通信超时统一使用 `timeout` 参数，不硬编码长等待

```python
def read_response(timeout: float = 5.0) -> bytes:
    try:
        return _read(timeout)
    except serial.SerialTimeoutException:
        raise TimeoutError("Device did not respond")
```

---

## 8. 构建与发布

- 版本号在 `metadata.json` 中定义，构建脚本自动读取
- `version_info.txt` 仅用于 Windows 可执行文件属性
- `build.ps1` 是唯一官方构建入口
