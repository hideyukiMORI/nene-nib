"""Select only the spawned process's visible windows without opening a real window (#448)."""

import ctypes
from pathlib import Path
import sys
import unittest
from unittest.mock import Mock, call, patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "eng"))
import window_driver as driver


def windows(entries):
    """Ordered (handle, pid, visible) observations from the native enumeration boundary."""
    native = Mock()
    native.FindWindowExW.side_effect = [entry[0] for entry in entries] + [0]
    observations = {handle: (pid, visible) for handle, pid, visible in entries}

    def owner(handle, pointer):
        ctypes.cast(pointer, ctypes.POINTER(driver.w.DWORD)).contents.value = observations[handle][0]
        return 1

    native.GetWindowThreadProcessId.side_effect = owner
    native.IsWindowVisible.side_effect = lambda handle: observations[handle][1]
    native.FindWindowW.return_value = entries[0][0] if entries else 0
    return native


class WindowOwner(unittest.TestCase):
    def test_skips_other_process_and_hidden_window(self):
        native = windows([(10, 100, True), (11, 200, False), (12, 200, True)])
        with patch.object(driver, "user", native):
            self.assertEqual(driver.owned_window(200, driver.WINDOW_CLASS), 12)
        self.assertEqual(native.FindWindowExW.call_args_list, [
            call(None, None, driver.WINDOW_CLASS, None),
            call(None, 10, driver.WINDOW_CLASS, None),
            call(None, 11, driver.WINDOW_CLASS, None),
        ])
        native.PostMessageW.assert_not_called()

    def test_absent_owned_visible_window_returns_zero(self):
        native = windows([(10, 100, True), (11, 200, False)])
        with patch.object(driver, "user", native):
            self.assertEqual(driver.owned_window(200, driver.WINDOW_CLASS), 0)
        self.assertEqual(native.FindWindowExW.call_count, 3)
        native.PostMessageW.assert_not_called()

    def test_empty_enumeration_does_not_query_invalid_handle(self):
        native = windows([])
        with patch.object(driver, "user", native):
            self.assertEqual(driver.owned_window(200, driver.WINDOW_CLASS), 0)
        native.GetWindowThreadProcessId.assert_not_called()
        native.IsWindowVisible.assert_not_called()

    def test_dialog_uses_the_same_owner_selection_with_dialog_class(self):
        with patch.object(driver, "owned_window", return_value=90) as owned:
            self.assertEqual(driver.owned_dialog(200), 90)
        owned.assert_called_once_with(200, driver.DIALOG_CLASS)

    def test_start_finds_spawned_process_behind_another_editor(self):
        native = windows([(10, 100, True), (11, 200, False), (12, 200, True)])
        process = Mock(pid=200)
        process.poll.return_value = None
        with (patch.object(driver, "user", native),
              patch.object(driver.subprocess, "Popen", return_value=process),
              patch.object(driver, "forget_session"),
              patch.object(driver, "rectangle", return_value=[1, 2, 3, 4]),
              patch.object(driver.time, "monotonic", side_effect=[10, 10, 16]),
              patch.object(driver.time, "sleep")):
            self.assertEqual(driver.start(Path("probe.exe"), {}), (process, 12, [1, 2, 3, 4]))
        native.FindWindowW.assert_not_called()
        native.PostMessageW.assert_not_called()
        process.terminate.assert_not_called()

    def test_start_timeout_stops_only_the_spawned_process(self):
        process = Mock(pid=200)
        process.poll.return_value = None
        with (patch.object(driver.subprocess, "Popen", return_value=process),
              patch.object(driver, "forget_session"),
              patch.object(driver, "owned_window", return_value=0) as owned,
              patch.object(driver.time, "monotonic", side_effect=[10, 10, 16]),
              patch.object(driver.time, "sleep"),
              patch.object(driver, "stop") as stop):
            with self.assertRaises(driver.WindowUnavailable):
                driver.start(Path("probe.exe"), {})
        owned.assert_called_once_with(200, driver.WINDOW_CLASS)
        stop.assert_called_once_with(process)


if __name__ == "__main__":
    unittest.main()
