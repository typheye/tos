"""Small Qt compatibility layer.

The app prefers PySide6 because it is the official Qt for Python package.
A PyQt5 fallback is kept so the GUI can also run on older lab computers.
"""

try:  # PySide6 first
    from PySide6 import QtCore, QtGui, QtWidgets
    Signal = QtCore.Signal
    Slot = QtCore.Slot
    QT_API = "PySide6"
except ImportError:  # pragma: no cover - depends on local installation
    from PyQt5 import QtCore, QtGui, QtWidgets  # type: ignore
    Signal = QtCore.pyqtSignal
    Slot = QtCore.pyqtSlot
    QT_API = "PyQt5"


def qapp_exec(app):
    """Run QApplication for both PySide6 and PyQt5."""
    if hasattr(app, "exec"):
        return app.exec()
    return app.exec_()
