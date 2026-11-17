"""The bounded waits the window tools use for the settings file the product replaces (#1784)."""

from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "eng"))
import window_driver as driver


class Clock:
    """A fake monotonic clock that sleeping advances, so no test waits for real time."""

    def __init__(self):
        self.now = 0.0

    def monotonic(self):
        return self.now

    def sleep(self, seconds):
        self.now += seconds


class WaitUntil(unittest.TestCase):
    def test_returns_as_soon_as_the_state_holds(self):
        clock, asked = Clock(), []
        with patch.object(driver.time, "monotonic", clock.monotonic), \
                patch.object(driver.time, "sleep", clock.sleep):
            value = driver.wait_until(lambda: asked.append(1) or len(asked) == 3, 5.0, 0.1)
        self.assertTrue(value)
        self.assertEqual(len(asked), 3)
        self.assertLess(clock.now, 0.5)

    def test_gives_up_after_the_limit_with_the_last_value(self):
        clock = Clock()
        with patch.object(driver.time, "monotonic", clock.monotonic), \
                patch.object(driver.time, "sleep", clock.sleep):
            value = driver.wait_until(lambda: None, 1.0, 0.25)
        self.assertIsNone(value)
        self.assertGreaterEqual(clock.now, 1.0)
        self.assertLess(clock.now, 1.5)

    def test_zero_seconds_asks_once(self):
        asked = []
        driver.wait_until(lambda: asked.append(1), 0.0)
        self.assertEqual(len(asked), 1)


class SettingsReads(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "NeNeNib" / driver.SETTINGS_FILE

    def test_path_comes_from_one_name_under_the_profile(self):
        environment = {"LOCALAPPDATA": self.directory.name}
        self.assertEqual(driver.settings_path(environment), self.path)

    def test_missing_file_is_none_after_the_limit_and_not_an_error(self):
        clock = Clock()
        with patch.object(driver.time, "monotonic", clock.monotonic), \
                patch.object(driver.time, "sleep", clock.sleep):
            self.assertIsNone(driver.read_settings_bytes(self.path, 0.5))
            self.assertEqual(driver.settings_fields(self.path, 0.5), {})

    def test_file_that_appears_while_waiting_is_read(self):
        self.path.parent.mkdir()
        clock = Clock()

        def sleep(seconds):
            clock.sleep(seconds)
            self.path.write_text("version=4\nfont_size=14.5\n", encoding="utf-8")
        with patch.object(driver.time, "monotonic", clock.monotonic), \
                patch.object(driver.time, "sleep", sleep):
            self.assertEqual(driver.settings_fields(self.path, 2.0)["font_size"], "14.5")

    def test_await_setting_waits_for_the_value_and_reports_the_last_one_seen(self):
        self.path.parent.mkdir()
        self.path.write_text("font_size=13.5\n", encoding="utf-8")
        clock, writes = Clock(), iter(["font_size=14.5\n"])

        def sleep(seconds):
            clock.sleep(seconds)
            following = next(writes, None)
            if following:
                self.path.write_text(following, encoding="utf-8")
        with patch.object(driver.time, "monotonic", clock.monotonic), \
                patch.object(driver.time, "sleep", sleep):
            self.assertEqual(driver.await_setting(self.path, "font_size", lambda v: v == "14.5"), "14.5")
            self.assertEqual(driver.await_setting(self.path, "font_size", lambda v: v == "40", 0.3), "14.5")

    def test_a_file_replaced_under_the_reader_is_retried(self):
        self.path.parent.mkdir()
        self.path.write_bytes(b"font_size=13.5\n")
        real = Path.read_bytes
        refusals = iter([PermissionError("in use"), FileNotFoundError("gone")])

        def read_bytes(path):
            refusal = next(refusals, None)
            if refusal:
                raise refusal
            return real(path)
        clock = Clock()
        with patch.object(driver.time, "monotonic", clock.monotonic), \
                patch.object(driver.time, "sleep", clock.sleep), \
                patch.object(Path, "read_bytes", read_bytes):
            self.assertEqual(driver.read_settings_bytes(self.path), b"font_size=13.5\n")


if __name__ == "__main__":
    unittest.main()
