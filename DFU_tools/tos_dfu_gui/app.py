"""Qt GUI for customer firmware upgrade over STM32 DFU."""

from __future__ import annotations

import sys
from pathlib import Path
from typing import Optional

from .dfu import (
    FlashError,
    ToolConfig,
    detect_with_dfu_util,
    detect_with_stm32_cli,
    find_default_firmware,
    flash_with_dfu_util,
    flash_with_stm32_cli,
    load_config,
    parse_hex_int,
)
from .qt_compat import QT_API, QtCore, QtGui, QtWidgets, Signal, qapp_exec


class Worker(QtCore.QThread):
    log = Signal(str)
    done = Signal(str)
    failed = Signal(str)

    def __init__(self, action: str, backend: str, firmware: Path, config: ToolConfig, parent=None):
        super().__init__(parent)
        self.action = action
        self.backend = backend
        self.firmware = firmware
        self.config = config

    def run(self):  # pragma: no cover - GUI thread behavior
        try:
            if self.action == "detect":
                if self.backend == "dfu-util":
                    detect_with_dfu_util(self.config, self.log.emit)
                else:
                    detect_with_stm32_cli(self.config, self.log.emit)
                self.done.emit("检测完成：已找到可用 DFU 设备或可用编程接口。")
                return

            if self.action == "flash":
                if self.backend == "dfu-util":
                    result = flash_with_dfu_util(self.firmware, self.config, self.log.emit)
                else:
                    result = flash_with_stm32_cli(self.firmware, self.config, self.log.emit)
                self.done.emit(f"{result.message} 用时 {result.elapsed_seconds:.1f}s")
                return

            raise FlashError(f"未知操作：{self.action}")
        except Exception as exc:
            self.failed.emit(str(exc))


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("TOS 固件 DFU 升级工具")
        self.resize(900, 620)
        self.worker: Optional[Worker] = None
        self.config = load_config()
        self._build_ui()
        self._load_defaults()

    def _build_ui(self):
        central = QtWidgets.QWidget(self)
        self.setCentralWidget(central)
        root = QtWidgets.QVBoxLayout(central)
        root.setContentsMargins(18, 18, 18, 18)
        root.setSpacing(12)

        title = QtWidgets.QLabel("TOS 固件 DFU 升级工具")
        font = title.font()
        font.setPointSize(18)
        font.setBold(True)
        title.setFont(font)
        root.addWidget(title)

        hint = QtWidgets.QLabel("步骤：让设备进入 STM32 DFU 模式 -> 连接 USB -> 选择 tos.elf -> 检测 DFU -> 开始升级。")
        hint.setWordWrap(True)
        root.addWidget(hint)

        form = QtWidgets.QFormLayout()
        form.setLabelAlignment(QtCore.Qt.AlignRight)

        fw_row = QtWidgets.QHBoxLayout()
        self.firmware_edit = QtWidgets.QLineEdit()
        self.firmware_edit.setPlaceholderText("选择编译输出的 tos.elf")
        self.browse_btn = QtWidgets.QPushButton("选择 ELF...")
        self.browse_btn.clicked.connect(self._browse_firmware)
        fw_row.addWidget(self.firmware_edit, 1)
        fw_row.addWidget(self.browse_btn)
        form.addRow("固件文件：", fw_row)

        self.backend_combo = QtWidgets.QComboBox()
        self.backend_combo.addItem("dfu-util（轻量，可随软件打包）", "dfu-util")
        self.backend_combo.addItem("STM32CubeProgrammer CLI（已安装 ST 工具时使用）", "stm32")
        form.addRow("刷写后端：", self.backend_combo)

        advanced = QtWidgets.QGroupBox("高级参数")
        advanced_layout = QtWidgets.QFormLayout(advanced)
        self.addr_edit = QtWidgets.QLineEdit(f"0x{self.config.flash_base:08X}")
        self.size_edit = QtWidgets.QLineEdit(hex(self.config.flash_size))
        self.vidpid_edit = QtWidgets.QLineEdit(self.config.dfu_vid_pid)
        self.alt_edit = QtWidgets.QLineEdit(self.config.dfu_alt)
        advanced_layout.addRow("Flash 起始地址：", self.addr_edit)
        advanced_layout.addRow("Flash 大小：", self.size_edit)
        advanced_layout.addRow("DFU VID:PID：", self.vidpid_edit)
        advanced_layout.addRow("DFU Alt：", self.alt_edit)
        form.addRow("", advanced)
        root.addLayout(form)

        btns = QtWidgets.QHBoxLayout()
        self.detect_btn = QtWidgets.QPushButton("检测 DFU")
        self.flash_btn = QtWidgets.QPushButton("开始升级")
        self.clear_btn = QtWidgets.QPushButton("清空日志")
        self.detect_btn.clicked.connect(self._detect)
        self.flash_btn.clicked.connect(self._flash)
        self.clear_btn.clicked.connect(lambda: self.log_text.clear())
        btns.addWidget(self.detect_btn)
        btns.addWidget(self.flash_btn)
        btns.addStretch(1)
        btns.addWidget(self.clear_btn)
        root.addLayout(btns)

        self.progress = QtWidgets.QProgressBar()
        self.progress.setRange(0, 1)
        self.progress.setValue(0)
        root.addWidget(self.progress)

        self.status_label = QtWidgets.QLabel("就绪")
        root.addWidget(self.status_label)

        self.log_text = QtWidgets.QPlainTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setLineWrapMode(QtWidgets.QPlainTextEdit.NoWrap)
        mono = QtGui.QFontDatabase.systemFont(QtGui.QFontDatabase.FixedFont)
        self.log_text.setFont(mono)
        root.addWidget(self.log_text, 1)

        footer = QtWidgets.QLabel(f"Qt API: {QT_API}    默认 STM32F407 Flash: 0x08000000 / 1MB")
        footer.setStyleSheet("color: #666;")
        root.addWidget(footer)

    def _load_defaults(self):
        default_fw = find_default_firmware()
        if default_fw:
            self.firmware_edit.setText(str(default_fw))
        self._append_log("提示：dfu-util 后端会先把 ELF 转成 .dfu.bin，再写入 0x08000000。")
        self._append_log("提示：Windows 下如果 dfu-util 找不到设备，通常需要把 DFU 设备驱动切到 WinUSB。")

    def _append_log(self, text: str):
        self.log_text.appendPlainText(text)
        bar = self.log_text.verticalScrollBar()
        bar.setValue(bar.maximum())

    def _browse_firmware(self):
        start = self.firmware_edit.text().strip() or str(Path.cwd())
        filename, _ = QtWidgets.QFileDialog.getOpenFileName(
            self,
            "选择 tos.elf",
            start,
            "ELF firmware (*.elf);;All files (*)",
        )
        if filename:
            self.firmware_edit.setText(filename)

    def _read_config_from_ui(self) -> ToolConfig:
        cfg = load_config()
        try:
            cfg.flash_base = parse_hex_int(self.addr_edit.text())
            cfg.flash_size = parse_hex_int(self.size_edit.text())
        except ValueError as exc:
            raise FlashError("Flash 地址/大小格式错误，请使用 0x08000000 这样的格式。") from exc
        cfg.dfu_vid_pid = self.vidpid_edit.text().strip() or "0483:df11"
        cfg.dfu_alt = self.alt_edit.text().strip() or "0"
        return cfg

    def _firmware_path(self) -> Path:
        path = Path(self.firmware_edit.text().strip()).expanduser()
        if not path.exists():
            raise FlashError("固件文件不存在，请选择编译生成的 tos.elf。")
        if path.suffix.lower() != ".elf":
            raise FlashError("当前工具面向 tos.elf；请选择 .elf 文件。")
        return path

    def _backend(self) -> str:
        return str(self.backend_combo.currentData())

    def _set_busy(self, busy: bool, message: str = ""):
        self.detect_btn.setEnabled(not busy)
        self.flash_btn.setEnabled(not busy)
        self.browse_btn.setEnabled(not busy)
        self.backend_combo.setEnabled(not busy)
        self.progress.setRange(0, 0 if busy else 1)
        if not busy:
            self.progress.setValue(0)
        if message:
            self.status_label.setText(message)

    def _start_worker(self, action: str):
        try:
            cfg = self._read_config_from_ui()
            firmware = self._firmware_path() if action == "flash" else Path(self.firmware_edit.text().strip() or ".")
        except Exception as exc:
            QtWidgets.QMessageBox.warning(self, "参数错误", str(exc))
            return

        backend = self._backend()
        self._set_busy(True, "正在执行，请勿断开设备...")
        self.worker = Worker(action, backend, firmware, cfg, self)
        self.worker.log.connect(self._append_log)
        self.worker.done.connect(self._on_done)
        self.worker.failed.connect(self._on_failed)
        self.worker.finished.connect(lambda: self._set_busy(False, "就绪"))
        self.worker.start()

    def _detect(self):
        self._append_log("\n=== 检测 DFU ===")
        self._start_worker("detect")

    def _flash(self):
        reply = QtWidgets.QMessageBox.question(
            self,
            "确认升级",
            "即将擦写设备内部 Flash。请确认设备处于 DFU 模式，并且升级过程中不要断电/拔 USB。\n\n继续吗？",
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
            QtWidgets.QMessageBox.No,
        )
        if reply != QtWidgets.QMessageBox.Yes:
            return
        self._append_log("\n=== 开始升级 ===")
        self._start_worker("flash")

    def _on_done(self, message: str):
        self._append_log("完成：" + message)
        self.status_label.setText(message)
        QtWidgets.QMessageBox.information(self, "完成", message)

    def _on_failed(self, message: str):
        self._append_log("失败：" + message)
        self.status_label.setText("失败")
        QtWidgets.QMessageBox.critical(self, "失败", message)


def main() -> int:
    app = QtWidgets.QApplication(sys.argv)
    app.setApplicationName("TOS DFU Upgrade Tool")
    app.setOrganizationName("honghan")
    window = MainWindow()
    window.show()
    return qapp_exec(app)


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
