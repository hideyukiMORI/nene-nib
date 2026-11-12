"""eng/render-app-icon.py の検査を、commit 済みの生成物と 1 画素だけ変えた写しで回す（Issue #1580）。

Chrome は起動しない。描画の代わりに、commit 済みの PNG と ICO の画素を大きさごとに返す替え玉を
compare へ渡す。写しは作業木の out/ の下に作る（C ドライブの Temp に書かない）。
"""

import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("render_app_icon", ROOT / "eng/render-app-icon.py")
ICON = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ICON)

MANIFEST = json.loads((ROOT / "eng/app-icon.json").read_text(encoding="utf-8"))


def committed_renders() -> dict[int, tuple]:
    """Size -> pixels from the committed files, standing in for Chrome."""
    images = {}
    for side, stored in ICON.icon_entries((ROOT / MANIFEST["icon"]).read_bytes()):
        images[side] = ICON.icon_frame_pixels(stored)
    for path, size in ICON.png_outputs(MANIFEST).items():
        if size not in images:
            images[size] = ICON.decode_png((ROOT / path).read_bytes())
    return images


class AppIconTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.renders = committed_renders()

    def setUp(self):
        directory = ROOT / "out/app-icon-tool-tests"
        directory.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=directory)
        self.addCleanup(self.temporary.cleanup)
        self.copy = Path(self.temporary.name)
        files = [ICON.MANIFEST, MANIFEST["source"], MANIFEST["icon"], *ICON.png_outputs(MANIFEST)]
        for name in files:
            (self.copy / name).parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, self.copy / name)

    def render(self, size):
        return self.renders[size]

    def test_committed_files_match_the_manifest(self):
        self.assertEqual(ICON.verify(ROOT), [])

    def test_the_icon_holds_the_sizes_and_the_256_frame_is_png(self):
        entries = ICON.icon_entries((ROOT / MANIFEST["icon"]).read_bytes())
        self.assertEqual([side for side, _ in entries], [16, 20, 24, 32, 40, 48, 64, 256])
        self.assertEqual([stored.startswith(ICON.PNG_SIGNATURE) for _, stored in entries],
                         [False] * 7 + [True])

    def test_the_msix_logos_are_the_25_names_of_nene_loupe_and_the_listing_logo(self):
        logos = ICON.msix_logos(MANIFEST)
        self.assertEqual(len(logos), 25)
        self.assertEqual(logos["Square150x150Logo.scale-125.png"], 188)
        self.assertEqual(logos["StoreLogo.scale-125.png"], 63)
        self.assertEqual(sorted(p.name for p in (ROOT / MANIFEST["msixDirectory"]).glob("*")), sorted(logos))
        self.assertEqual(ICON.png_outputs(MANIFEST)[MANIFEST["listingLogo"]], 300)

    def test_compare_passes_when_the_pixels_are_the_render(self):
        self.assertEqual(ICON.compare(self.copy, self.render), [])
        self.assertEqual(ICON.verify(self.copy), [])

    def test_one_changed_pixel_in_a_logo_fails_both_checks(self):
        name = f"{MANIFEST['msixDirectory']}/Square44x44Logo.scale-100.png"
        width, height, rgba = ICON.decode_png((self.copy / name).read_bytes())
        changed = bytearray(rgba)
        changed[(22 * width + 22) * 4] ^= 1
        (self.copy / name).write_bytes(ICON.encode_png((width, height, bytes(changed))))
        self.assertEqual(ICON.compare(self.copy, self.render),
                         [f"{name}: pixels differ from the render of {MANIFEST['source']}"])
        self.assertEqual(ICON.verify(self.copy), [f"{name}: SHA-256 differs from {ICON.MANIFEST}"])

    def test_one_changed_pixel_in_an_icon_frame_fails(self):
        data = bytearray((self.copy / MANIFEST["icon"]).read_bytes())
        _, stored = ICON.icon_entries(bytes(data))[0]
        offset = bytes(data).find(stored)
        data[offset + 40 + (8 * 16 + 8) * 4] ^= 1
        (self.copy / MANIFEST["icon"]).write_bytes(bytes(data))
        self.assertEqual(ICON.compare(self.copy, self.render),
                         [f"{MANIFEST['icon']}: the 16 px frame differs from the render"])

    def test_a_stray_logo_and_a_missing_logo_are_reported(self):
        stray = self.copy / MANIFEST["msixDirectory"] / "Square44x44Logo.targetsize-20_altform-unplated.png"
        shutil.copyfile(self.copy / MANIFEST["listingLogo"], stray)
        (self.copy / MANIFEST["msixDirectory"] / "StoreLogo.scale-100.png").unlink()
        problems = ICON.verify(self.copy)
        self.assertIn(f"{MANIFEST['msixDirectory']}/{stray.name}: not a generated logo", problems)
        self.assertIn(f"{MANIFEST['msixDirectory']}/StoreLogo.scale-100.png: missing", problems)

    def test_png_and_dib_round_trip_and_the_encoding_is_deterministic(self):
        image = (3, 2, bytes(range(24)))
        encoded = ICON.encode_png(image)
        self.assertEqual(ICON.decode_png(encoded), image)
        self.assertEqual(ICON.encode_png(image), encoded)
        square = (2, 2, bytes([1, 2, 3, 0, 4, 5, 6, 255, 7, 8, 9, 128, 10, 11, 12, 255]))
        self.assertEqual(ICON.decode_dib(ICON.dib_frame(square)), square)


if __name__ == "__main__":
    unittest.main()
