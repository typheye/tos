"""Migration regressions: no hardware access and no real user data mutations."""
from __future__ import annotations

import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "src"))
from tos_common import metadata, resource
from tos_common import platform_support
from tos_helper import app as helper
from tdb import app as tdb
from tvidtool import app as tvid

spec = importlib.util.spec_from_file_location("tos_manage", ROOT / "scripts" / "manage.py")
manage = importlib.util.module_from_spec(spec)
spec.loader.exec_module(manage)


class PlatformTests(unittest.TestCase):
    def test_windows_permission_hint_never_queries_unix_identity(self):
        with mock.patch.object(platform_support.sys, "platform", "win32"), \
             mock.patch.object(platform_support.getpass, "getuser", side_effect=AssertionError):
            hint = platform_support.usb_permission_hint("COM7")
        self.assertIn("Device Manager", hint)
        self.assertNotIn("dialout", hint)

    def test_linux_dpi_never_calls_windows_api(self):
        import ctypes
        with mock.patch.object(platform_support.sys, "platform", "linux"), \
             mock.patch.object(ctypes, "WinDLL", create=True, side_effect=AssertionError):
            platform_support.enable_windows_high_dpi()

    def test_permission_error_chain_and_cycle(self):
        outer = RuntimeError("wrapped")
        denied = OSError(1, "denied")
        outer.__cause__ = denied
        self.assertTrue(platform_support.permission_denied(outer))
        a = RuntimeError("a")
        b = RuntimeError("b")
        a.__cause__ = b
        b.__cause__ = a
        self.assertFalse(platform_support.permission_denied(a))

    @unittest.skipUnless(sys.platform == "linux", "Linux group diagnostics")
    def test_linux_group_change_requires_new_login(self):
        import grp
        group = mock.Mock(gr_mem=["test-user"], gr_gid=12345)
        with mock.patch.object(platform_support.getpass, "getuser", return_value="test-user"), \
             mock.patch.object(grp, "getgrnam", return_value=group), \
             mock.patch("os.getgroups", return_value=[]):
            self.assertIn("Log out", platform_support.usb_permission_hint("/dev/ttyACM0"))

    def test_unsupported_host_is_rejected(self):
        with mock.patch.object(manage.sys, "platform", "darwin"):
            with self.assertRaisesRegex(RuntimeError, "Unsupported"):
                manage.host()


class ApplicationTests(unittest.TestCase):
    def test_metadata_versions_and_direct_entrypoints(self):
        for tool in manage.TOOLS:
            with self.subTest(tool=tool):
                script = ROOT / "src" / tool / "app.py"
                result = subprocess.run([sys.executable, str(script), "--version"],
                                        capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn(metadata(tool)["version"], result.stdout)

    def test_frozen_resource_paths(self):
        with tempfile.TemporaryDirectory(prefix="TOS 空格 ") as tmp, \
             mock.patch.object(sys, "frozen", True, create=True), \
             mock.patch.object(sys, "_MEIPASS", tmp, create=True):
            base = Path(tmp)
            (base / "tos_helper").mkdir()
            (base / "tos_helper" / "metadata.json").write_text('{"version":"7.2.1"}')
            self.assertEqual(metadata("tos_helper")["version"], "7.2.1")
            self.assertEqual(helper.package_resource("assets", "icon.png"),
                             base / "tos_helper" / "assets" / "icon.png")
        self.assertTrue(resource("tos_helper", "assets", "typheye_rounded.png").is_file())

    def test_tdb_keeps_config_identity_and_reads_existing_data(self):
        self.assertEqual(tdb.CONFIG_PATH, Path.home() / ".tdb_device.json")
        with tempfile.TemporaryDirectory() as tmp:
            config = Path(tmp) / ".tdb_device.json"
            raw = '{"port":"COM9","baud":115200,"extra":"keep"}'
            config.write_text(raw)
            with mock.patch.object(tdb, "CONFIG_PATH", config):
                self.assertEqual(tdb.load_saved_port(), "COM9")
            self.assertEqual(config.read_text(), raw)

    def test_hid_report_format_and_windows_fallback_preserved(self):
        report = helper.make_vendor_report(b"test")
        self.assertEqual(len(report), 64)
        self.assertEqual(report[0], 0x10)
        self.assertEqual(helper.parse_vendor_report(report), b"test")
        entry = helper.HidEntry.from_dict({"path": b"bad", "vendor_id": 0x0483,
                                           "product_id": 0x5750, "serial_number": "uid"})
        broken = mock.Mock()
        broken.open_path.side_effect = OSError("bad path")
        working = mock.Mock()
        fake_hid = mock.Mock()
        fake_hid.device.side_effect = [broken, working]
        with mock.patch.object(helper, "hid", fake_hid):
            self.assertIs(helper.open_device(entry), working)
        broken.close.assert_called_once()
        working.open.assert_called_once_with(0x0483, 0x5750, "uid")

    def test_tvid_big_endian_rgb565_preserved(self):
        # OpenCV BGR input: red, green, blue -> big endian 565 for the LCD.
        self.assertEqual(tvid.bgr888_to_rgb565_be(bytes([0, 0, 255, 0, 255, 0, 255, 0, 0])),
                         b"\xf8\x00\x07\xe0\x00\x1f")

    @unittest.skipUnless(sys.platform == "win32" or os.environ.get("DISPLAY"), "display required")
    def test_real_gui_icon_and_theme_without_usb(self):
        # A real Tk window is created, but enumeration/opening would fail the test.
        import tkinter as tk
        from tkinter import ttk
        original = tk.Tk
        observed = []
        def create_root(*args, **kwargs):
            window = original(*args, **kwargs)
            window.after(50, lambda: observed.append((window.winfo_class(),
                bool(window._tos_app_icon), ttk.Style(window).theme_use())))
            return window
        with mock.patch.object(helper, "enumerate_devices", side_effect=AssertionError), \
             mock.patch.object(helper, "open_device", side_effect=AssertionError), \
             mock.patch.object(tk, "Tk", side_effect=create_root):
            self.assertEqual(helper.gui_main(smoke_test=True), 0)
        self.assertEqual(observed[0][0], "Toshelper")
        self.assertTrue(observed[0][1])
        self.assertTrue(observed[0][2])


class ManagementTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="TOS 测试 ")
        self.base = Path(self.temporary.name)
        self.root = self.base / "pc-app 空格"
        self.root.mkdir()
        for tool in manage.TOOLS:
            package = self.root / "src" / tool
            package.mkdir(parents=True)
            shutil.copy2(ROOT / "src" / tool / "metadata.json", package / "metadata.json")
        self.patch = mock.patch.object(manage, "ROOT", self.root)
        self.patch.start()

    def tearDown(self):
        self.patch.stop()
        self.temporary.cleanup()

    def test_forwarding_keeps_unicode_spaces_and_app_flags(self):
        with mock.patch.object(manage, "run") as run:
            manage.main(["run", "--current-env", "--tool", "tvidtool", "--",
                         "电影 空格.mp4", "-o", "输出 文件.tvid", "--current-env"])
        self.assertEqual(run.call_args.args[0], [sys.executable,
            str(self.root / "src/tvidtool/app.py"), "电影 空格.mp4", "-o",
            "输出 文件.tvid", "--current-env"])

    def test_conda_routing_and_guard(self):
        with mock.patch.dict(os.environ, {"CONDA_DEFAULT_ENV": "other"}), \
             mock.patch.object(manage, "find_conda", return_value="conda"), \
             mock.patch.object(manage, "run", return_value=mock.Mock(returncode=0)) as run:
            manage.main(["run", "--", "--version"])
        command = run.call_args.args[0]
        self.assertIn("--current-env", command)
        self.assertEqual(command[-3:], ["run", "--", "--version"])

    def test_windows_conda_batch_shim_uses_native_argv(self):
        base = self.base / "Conda 空格 & data"
        (base / "condabin").mkdir(parents=True)
        (base / "Scripts").mkdir()
        shim = base / "condabin/conda.bat"
        shim.touch()
        executable = base / "Scripts/conda.exe"
        executable.touch()
        with mock.patch.object(manage.sys, "platform", "win32"):
            self.assertEqual(manage.conda_command(str(shim), ["run", "中文 & path"]),
                             [str(executable), "run", "中文 & path"])

    def test_command_failure_retains_exit_code(self):
        with mock.patch.object(manage, "run", side_effect=subprocess.CalledProcessError(17, ["app"])):
            with self.assertRaises(subprocess.CalledProcessError) as raised:
                manage.main(["run", "--current-env"])
        self.assertEqual(raised.exception.returncode, 17)

    def test_real_wrapper_propagates_app_failure(self):
        if sys.platform == "win32":
            command = ["cmd.exe", "/d", "/c", str(ROOT / "scripts/pc.cmd")]
        else:
            command = ["bash", str(ROOT / "scripts/pc.sh")]
        result = subprocess.run(command + ["run", "--current-env", "--", "--invalid-tos-flag"],
                                capture_output=True, text=True, timeout=30,
                                env={**os.environ, "TOS_PYTHON": sys.executable})
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)

    def test_real_wrapper_from_unicode_checkout_path(self):
        checkout = self.base / "源码 with spaces"
        for name in ("src", "scripts"):
            shutil.copytree(ROOT / name, checkout / name,
                            ignore=shutil.ignore_patterns("__pycache__"))
        if sys.platform == "win32":
            command = ["cmd.exe", "/d", "/c", str(checkout / "scripts/pc.cmd")]
        else:
            command = ["bash", str(checkout / "scripts/pc.sh")]
        result = subprocess.run(command + ["run", "--current-env", "--tool", "tdb", "--", "--version"],
                                capture_output=True, text=True, timeout=30,
                                env={**os.environ, "TOS_PYTHON": sys.executable})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("tdb 1.0.1", result.stdout)

    def test_failed_build_preserves_old_dist(self):
        (self.root / "dist").mkdir()
        old = self.root / "dist" / "last-good"
        old.write_text("usable")
        with mock.patch.object(manage, "run", side_effect=[mock.Mock(),
                         subprocess.CalledProcessError(9, ["PyInstaller"])]):
            with self.assertRaises(subprocess.CalledProcessError):
                manage.build()
        self.assertEqual(old.read_text(), "usable")
        self.assertFalse((self.root / "dist.previous").exists())

    def test_incomplete_successful_build_preserves_old_dist(self):
        (self.root / "dist").mkdir()
        old = self.root / "dist" / "last-good"
        old.write_text("usable")
        def fake_run(command, **kwargs):
            if "PyInstaller" in command:
                output = Path(command[command.index("--distpath") + 1])
                output.mkdir()
            return mock.Mock(returncode=0)
        with mock.patch.object(manage, "run", side_effect=fake_run):
            with self.assertRaises(FileNotFoundError):
                manage.build()
        self.assertEqual(old.read_text(), "usable")

    def test_failed_publish_restores_previous_directory(self):
        destination = self.root / "dist"
        destination.mkdir()
        (destination / "old").write_text("good")
        candidate = self.root / "candidate"
        candidate.mkdir()
        rename = Path.rename
        def fail_candidate(path, target):
            if path == candidate:
                raise OSError("simulated publish failure")
            return rename(path, target)
        with mock.patch.object(Path, "rename", fail_candidate):
            with self.assertRaises(OSError):
                manage.replace_directory(candidate, destination)
        self.assertEqual((destination / "old").read_text(), "good")

    def test_clean_preserves_dist_install_and_config(self):
        (self.root / "build").mkdir()
        (self.root / "dist").mkdir()
        (self.root / "dist" / "good").write_text("artifact")
        installed = self.base / "installed"
        installed.mkdir()
        (installed / "app").write_text("installed")
        config = self.base / ".tdb_device.json"
        config.write_text("existing")
        manage.clean(False)
        self.assertFalse((self.root / "build").exists())
        self.assertTrue((self.root / "dist" / "good").exists())
        manage.clean(True)
        self.assertFalse((self.root / "dist").exists())
        self.assertEqual((installed / "app").read_text(), "installed")
        self.assertEqual(config.read_text(), "existing")

    @unittest.skipIf(sys.platform == "win32", "symlink creation may require Windows privileges")
    def test_clean_refuses_symlink_and_keeps_external_files(self):
        external = self.base / "external"
        external.mkdir()
        (external / "data").write_text("keep")
        (self.root / "build").symlink_to(external, target_is_directory=True)
        with self.assertRaisesRegex(RuntimeError, "symbolic link"):
            manage.clean(False)
        self.assertEqual((external / "data").read_text(), "keep")

    @unittest.skipIf(sys.platform == "win32", "symlink creation may require Windows privileges")
    def test_clean_refuses_nested_symlink(self):
        (self.root / "build").mkdir()
        external = self.base / "outside"
        external.mkdir()
        (self.root / "build" / "nested").symlink_to(external, target_is_directory=True)
        with self.assertRaisesRegex(RuntimeError, "symbolic link"):
            manage.clean(False)
        self.assertTrue(external.is_dir())

    def test_install_refuses_source_or_unmanaged_directory(self):
        for target in (self.root, self.root / "build", self.base):
            with self.subTest(target=target), self.assertRaises(RuntimeError):
                manage.validate_install_dir(target)
        existing = self.base / "user-data"
        existing.mkdir()
        (existing / "important").write_text("keep")
        with self.assertRaisesRegex(RuntimeError, "unmanaged"):
            manage.validate_install_dir(existing)
        self.assertEqual((existing / "important").read_text(), "keep")

    def test_install_paths_do_not_depend_on_source(self):
        target = self.base / "安装 files"
        text = manage.desktop_entry(target)
        self.assertIn('Exec="', text)
        self.assertIn(str(target / "assets"), text)
        self.assertIn("StartupWMClass=Toshelper", text)
        self.assertIn("Terminal=false", text)
        self.assertIn("StartupNotify=false", text)
        self.assertNotIn(str(self.root), text)

    def test_launcher_protects_other_apps_and_updates_tos(self):
        path = self.base / "tos-helper.desktop"
        path.write_text("[Desktop Entry]\nName=Other app\n")
        with self.assertRaises(RuntimeError):
            manage.write_launcher(path, manage.desktop_entry(self.base / "installed"))
        path.write_text("[Desktop Entry]\nName=TOS Helper\nExec=/old/linux-pc/app\n")
        manage.write_launcher(path, manage.desktop_entry(self.base / "installed"))
        self.assertNotIn("linux-pc", path.read_text())

    def test_vscode_buttons_have_icons_and_platform_commands(self):
        tasks = json.loads((ROOT / ".vscode/tasks.json").read_text())["tasks"]
        self.assertEqual({t["label"] for t in tasks},
                         {"Setup", "Play", "Build", "Install", "Clean", "Test", "Doctor"})
        for task in tasks:
            self.assertTrue(task["icon"]["id"])
            self.assertTrue(task["icon"]["tooltip"])
            self.assertTrue(task["windows"]["command"].endswith("pc.cmd"))
        self.assertTrue(next(t for t in tasks if t["label"] == "Build")["group"]["isDefault"])


if __name__ == "__main__":
    unittest.main()
