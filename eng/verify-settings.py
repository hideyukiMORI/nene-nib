"""Focused native font/settings checks (#60), using the existing window driver and pixel reader.

An isolated LOCALAPPDATA keeps real user settings untouched. No unrelated Vim/file/first-paint
suite is run. Real Ctrl keys require an unlocked foreground-capable Windows desktop (QLT-013).
"""

import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import tempfile
import time

from window_driver import (close, click, press, press_chord, rectangle, start, stop, user,
                           VK_CONTROL, VK_S, write_text, HWND_TOPMOST, SWP_NOMOVE_NOSIZE_SHOW)

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("window_verification", ROOT / "eng/verify-window.py")
VIEW = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VIEW)


def saved_points(settings: Path) -> float:
    fields = dict(line.split("=", 1) for line in settings.read_text(encoding="utf-8").splitlines())
    return float(fields["font_size"])


def chord(window, key: int) -> None:
    assert press_chord(window, VK_CONTROL, key), "foreground input unavailable; not a product pass"
    time.sleep(0.25)


def wheel(window, delta: int) -> None:
    assert user.PostMessageW(window, 0x020A, ((delta & 0xFFFF) << 16) | 0x0008, 0)
    time.sleep(0.2)


def geometry(window, points: float, appearance: str, output: Path, name: str) -> dict:
    client = rectangle(window, user.GetClientRect)
    width, height = client[2], client[3]
    dpi = user.GetDpiForWindow(window)
    body = VIEW.body_points(width, height, dpi, points)
    pixels = VIEW.capture(window, width, height)
    expected = list(VIEW.CURRENT_LINE[appearance])
    top = body["firstRowTop"]
    bottom = top + body["lineHeight"]
    assert all(VIEW.pixel(pixels, width, width - 4, y) == expected for y in range(top, bottom)), \
        f"the highlighted row does not match {points} pt geometry"
    assert VIEW.pixel(pixels, width, width - 4, bottom) == list(VIEW.PALETTE[appearance])
    assert VIEW.caret_pixel(pixels, width, body, 0) == list(VIEW.ACCENT), "caret/gutter mismatch"
    VIEW.write_bitmap(output / f"{name}.bmp", pixels, width, height)
    return {"points": points, "dpi": dpi, "clientSize": [width, height], **body}


def prepare(window) -> None:
    assert user.SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE_NOSIZE_SHOW)
    time.sleep(0.5)
    ime = VIEW.ime_window(window)
    if ime:
        VIEW.set_ime_open(ime, False)


def verify_keys(window, settings: Path, appearance: str, output: Path) -> dict:
    result = {"default": geometry(window, 13.5, appearance, output, "font-default")}
    assert not settings.exists(), "startup must not create settings"
    for key, points in ((0xBB, 14.5), (0xBD, 13.5), (0x6B, 14.5), (0x6D, 13.5)):
        chord(window, key)
        assert saved_points(settings) == points, (key, settings.read_text())
    wheel(window, 60)
    assert saved_points(settings) == 13.5, "partial wheel must not resize yet"
    wheel(window, 60)
    assert saved_points(settings) == 14.5, "two partial wheel deltas make one step"
    wheel(window, 120 * 40)
    assert saved_points(settings) == 40
    result["maximum"] = geometry(window, 40, appearance, output, "font-maximum")
    wheel(window, -120 * 40)
    assert saved_points(settings) == 8
    result["minimum"] = geometry(window, 8, appearance, output, "font-minimum")
    chord(window, 0x30)
    assert saved_points(settings) == 13.5
    size = result["default"]
    toggle = VIEW.toggle_points(*size["clientSize"], size["dpi"])
    click(window, *toggle["vim"])
    time.sleep(0.2)
    chord(window, 0x6B)
    assert saved_points(settings) == 14.5, "Vim supports the same font intent"
    chord(window, 0x60)
    assert saved_points(settings) == 13.5, "numpad zero resets in Vim"
    click(window, *toggle["ordinary"])
    wheel(window, 120 * 8)
    assert saved_points(settings) == 21.5
    result["enlarged"] = geometry(window, 21.5, appearance, output, "font-enlarged")
    return result


def verify_click(window, body: dict, document: Path) -> None:
    width, height = body["clientSize"]
    click(window, body["gutter"], VIEW.row_middle(body, 1))
    press(window, 0x23)  # End: measure the current font's ten monospace cells.
    time.sleep(0.25)
    pixels = VIEW.capture(window, width, height)
    y = VIEW.row_middle(body, 1)
    caret = next(x for x in range(body["gutter"], width)
                 if VIEW.pixel(pixels, width, x, y) == list(VIEW.ACCENT))
    assert caret > body["gutter"], "End did not move to the end of the text"
    column_four = body["gutter"] + round((caret - body["gutter"]) * 4 / 10)
    click(window, column_four, y)
    write_text(window, "!")
    chord(window, VK_S)
    assert document.read_text(encoding="utf-8") == "\nabcd!efghij\n三行目\n", \
        "resizing changed text, injected a shortcut character, or broke click hit testing"


def verify_font(executable: Path, environment: dict, output: Path) -> dict:
    settings = Path(environment["LOCALAPPDATA"]) / "NeNeNib/settings.v1"
    document = output / "font-document.txt"
    document.write_text("\nabcdefghij\n三行目\n", encoding="utf-8", newline="\n")
    appearance = VIEW.expected_appearance()
    process, window, _ = start(executable, environment, [str(document)])
    try:
        prepare(window)
        result = verify_keys(window, settings, appearance, output)
        verify_click(window, result["enlarged"], document)
        result["clickAndDocumentPreserved"] = True
        close(window)
        assert process.wait(timeout=5) == 0
    finally:
        stop(process)
    process, window, _ = start(executable, environment, [str(document)])
    try:
        prepare(window)
        result["restart"] = geometry(window, 21.5, appearance, output, "font-restarted")
        close(window)
        assert process.wait(timeout=5) == 0
    finally:
        stop(process)
    result["imeAtEnlargedSize"] = VIEW.verify_ime(executable, environment, appearance, output, 21.5)
    # A manually selected theme and family take the same startup settings path.
    settings.write_text("version=1\ncolorscheme=neutral-light\nfont_family=Consolas\nfont_size=18\n",
                        encoding="utf-8", newline="\n")
    process, window, _ = start(executable, environment)
    try:
        prepare(window)
        result["explicitThemeAndFont"] = geometry(window, 18, "light", output, "font-explicit")
        close(window)
        assert process.wait(timeout=5) == 0
    finally:
        stop(process)
    return result


def verify_unchanged_size(executable: Path, environment: dict, output: Path) -> dict:
    settings = Path(environment["LOCALAPPDATA"]) / "NeNeNib/settings.v1"
    settings.parent.mkdir()
    settings.write_text("version=1\ncolorscheme=system\nfont_family=Consolas\nfont_size=40\n",
                        encoding="utf-8", newline="\n")
    document = output / "font-scroll.txt"
    document.write_text("\n".join(f"line {n:03}" for n in range(80)), encoding="utf-8")
    process, window, _ = start(executable, environment, [str(document)])
    try:
        prepare(window)
        client = rectangle(window, user.GetClientRect)
        width, height = client[2], client[3]
        assert user.PostMessageW(window, 0x020A, (-120 & 0xFFFF) << 16, 0)
        time.sleep(0.3)
        before = VIEW.capture(window, width, height)
        chord(window, 0x6B)
        after = VIEW.capture(window, width, height)
        assert before == after, "a no-op font adjustment moved the scrolled viewport"
        assert saved_points(settings) == 40
        close(window)
        assert process.wait(timeout=5) == 0
        return {"unchangedSizePreservesScrolledViewport": True}
    finally:
        stop(process)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, default=ROOT / "build/NeNeNib.exe")
    parser.add_argument("--only", choices=("font", "unchanged-size"), default="font")
    args = parser.parse_args()
    output = ROOT / "out/settings-verification"
    output.mkdir(parents=True, exist_ok=True)
    executable = output / "NeNeNib.exe"
    shutil.copy2(args.executable, executable)
    isolated = Path(tempfile.mkdtemp(prefix="profile-", dir=output)).resolve()
    assert isolated.is_relative_to(output.resolve())
    environment = dict(os.environ, LOCALAPPDATA=str(isolated), APPDATA=str(isolated))
    VIEW.become_dpi_aware()
    verifier = verify_font if args.only == "font" else verify_unchanged_size
    result = verifier(executable, environment, output)
    name = "settings-results.json" if args.only == "font" else "unchanged-size-results.json"
    (output / name).write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
