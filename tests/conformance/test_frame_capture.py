"""PNG の書き出しと読み戻し、前後の画素比較、鍵の記法（Issue #131）。display は要らない。

窓を撮る側（window_driver.capture と verify-window.py --capture）は対話の desktop が要るので
CI から外したまま（QLT-013）。ここで回すのは、撮った後の画素をファイルへ書く・読む・比べる
部分と、鍵の列を読む部分だけで、製品は build も起動もしない。
"""

import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "eng"))

from window_driver import parse_keys, read_png, VK_ESCAPE, write_png  # noqa: E402

spec = importlib.util.spec_from_file_location("compare_frames", ROOT / "eng/compare-frames.py")
compare_frames = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compare_frames)

WIDTH, HEIGHT = 4, 3
GROUND = (0x30, 0x0A, 0x24)


def frame(changes=None, width=WIDTH, height=HEIGHT) -> bytes:
    """Top-down BGRA filled with GROUND, with {(x, y): (r, g, b)} painted over it."""
    pixels = bytearray()
    for y in range(height):
        for x in range(width):
            red, green, blue = (changes or {}).get((x, y), GROUND)
            pixels += bytes((blue, green, red, 0xFF))
    return bytes(pixels)


def compare(*arguments):
    return subprocess.run([sys.executable, str(ROOT / "eng/compare-frames.py"), *arguments],
                          capture_output=True, text=True, encoding="utf-8",
                          env=dict(os.environ, PYTHONUTF8="1"))


class PngRoundTrip(unittest.TestCase):
    def test_known_pixels_come_back_as_rgb(self):
        known = {(0, 0): (0xFF, 0x00, 0x00), (3, 0): (0x00, 0xFF, 0x00), (1, 2): (0x12, 0x34, 0x56)}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "frame.png"
            write_png(path, WIDTH, HEIGHT, frame(known))
            width, height, rgb = read_png(path)
        self.assertEqual((width, height), (WIDTH, HEIGHT))
        for y in range(HEIGHT):
            for x in range(WIDTH):
                start = (y * WIDTH + x) * 3
                self.assertEqual(tuple(rgb[start:start + 3]), known.get((x, y), GROUND), (x, y))

    def test_a_file_that_is_not_png_is_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "frame.bmp"
            path.write_bytes(b"BM" + bytes(64))
            with self.assertRaises(ValueError):
                read_png(path)


class CompareFrames(unittest.TestCase):
    def pair(self, directory, changes, after_size=(WIDTH, HEIGHT)):
        before, after = Path(directory) / "before.png", Path(directory) / "after.png"
        write_png(before, WIDTH, HEIGHT, frame())
        write_png(after, *after_size, frame(changes, *after_size))
        return str(before), str(after)

    def test_a_difference_inside_the_box_passes(self):
        with tempfile.TemporaryDirectory() as directory:
            result = compare(*self.pair(directory, {(1, 1): (1, 2, 3), (2, 2): (4, 5, 6)}),
                             "--inside", "1,1,3,2")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["differentPixels"], 2)
        self.assertEqual(report["bounds"], {"x": 1, "y": 1, "w": 2, "h": 2})
        self.assertTrue(report["inside"])

    def test_no_difference_has_no_bounds_and_is_inside(self):
        with tempfile.TemporaryDirectory() as directory:
            result = compare(*self.pair(directory, {}), "--inside", "0,0,1,1")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        report = json.loads(result.stdout)
        self.assertIsNone(report["bounds"])
        self.assertTrue(report["inside"])

    def test_a_difference_reaching_outside_the_box_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            result = compare(*self.pair(directory, {(1, 1): (1, 2, 3), (0, 0): (9, 9, 9)}),
                             "--inside", "1,1,3,2")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["bounds"], {"x": 0, "y": 0, "w": 2, "h": 2})
        self.assertFalse(report["inside"])

    def regions_file(self, directory):
        path = Path(directory) / "frames.json"
        # 4x3 を縦に 3 つへ分ける: title は行 0、body は行 1、status は行 2 の右の 3 画素だけ。
        path.write_text(json.dumps({"regions": {
            "title": {"x": 0, "y": 0, "w": 4, "h": 1},
            "body": {"x": 0, "y": 1, "w": 4, "h": 1},
            "status": {"x": 1, "y": 2, "w": 3, "h": 1}}}), encoding="utf-8")
        return str(path)

    def test_differences_only_inside_the_regions_pass(self):
        with tempfile.TemporaryDirectory() as directory:
            result = compare(*self.pair(directory, {(2, 0): (1, 2, 3), (0, 1): (4, 5, 6),
                                                    (3, 1): (4, 5, 6), (1, 2): (7, 8, 9)}),
                             "--regions", self.regions_file(directory))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        report = json.loads(result.stdout)
        self.assertTrue(report["inside"])
        self.assertEqual(report["outside"], {"differentPixels": 0, "bounds": None})
        self.assertEqual(report["regions"]["title"],
                         {"differentPixels": 1, "bounds": {"x": 2, "y": 0, "w": 1, "h": 1}})
        self.assertEqual(report["regions"]["body"],
                         {"differentPixels": 2, "bounds": {"x": 0, "y": 1, "w": 4, "h": 1}})
        self.assertEqual(report["regions"]["status"]["differentPixels"], 1)

    def test_one_pixel_outside_the_regions_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            result = compare(*self.pair(directory, {(0, 1): (4, 5, 6), (0, 2): (9, 9, 9)}),
                             "--regions", self.regions_file(directory), "--inside", "0,0,4,3")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        report = json.loads(result.stdout)
        self.assertFalse(report["inside"])
        self.assertEqual(report["outside"],
                         {"differentPixels": 1, "bounds": {"x": 0, "y": 2, "w": 1, "h": 1}})
        self.assertEqual(report["regions"]["body"]["differentPixels"], 1)

    def test_an_expected_region_that_changed_passes(self):
        with tempfile.TemporaryDirectory() as directory:
            result = compare(*self.pair(directory, {(2, 0): (1, 2, 3), (0, 1): (4, 5, 6)}),
                             "--regions", self.regions_file(directory), "--expect", "body,title")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(json.loads(result.stdout)["expected"], {"body": True, "title": True})

    def test_an_expected_region_without_difference_fails(self):
        # 鍵が届かず同じ画が返った回。差分 0 は --expect の下では成功ではない（訂正 2）。
        with tempfile.TemporaryDirectory() as directory:
            result = compare(*self.pair(directory, {(2, 0): (1, 2, 3)}),
                             "--regions", self.regions_file(directory), "--expect", "body,title")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        report = json.loads(result.stdout)
        self.assertTrue(report["inside"])
        self.assertEqual(report["expected"], {"body": False, "title": True})

    def test_frames_of_different_sizes_are_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            result = compare(*self.pair(directory, {}, after_size=(WIDTH + 1, HEIGHT)))
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn("size", json.loads(result.stdout)["error"])


class FramesChanged(unittest.TestCase):
    def test_a_changed_pixel_is_a_change(self):
        self.assertTrue(compare_frames.frames_changed(frame(), frame({(1, 1): (1, 2, 3)})))

    def test_the_same_capture_is_no_change(self):
        # verify-window.py --keys はこのとき "changed": false を書いて終了 1 にする。
        self.assertFalse(compare_frames.frames_changed(frame(), frame()))


class KeyNotation(unittest.TestCase):
    def test_the_fixture_notation_is_read(self):
        self.assertEqual(parse_keys("ihello<Esc>"), [("text", "ihello"), ("key", VK_ESCAPE)])

    def test_an_unknown_name_is_refused(self):
        with self.assertRaises(ValueError):
            parse_keys("i<Foo>")


if __name__ == "__main__":
    unittest.main()
