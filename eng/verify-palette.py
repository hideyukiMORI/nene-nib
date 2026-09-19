"""Focused native Ctrl+P checks (#66), reusing the Ex/window driver and isolated settings.

Requires an unlocked Windows desktop for Ctrl chords and IME open-state checks.
"""

import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import tempfile
import time

from window_driver import (close, click, press, rectangle, start, stop, user,
                           window_title, write_text, HWND_TOPMOST)

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("ex_verification", ROOT / "eng/verify-ex.py")
EX = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EX)
SETTINGS = EX.SETTINGS
VIEW = EX.VIEW


def open_palette(window, query=""):
    SETTINGS.chord(window, ord('P'))
    if query:
        write_text(window, query)
        time.sleep(0.2)


def execute(window, query):
    open_palette(window, query)
    press(window, 0x0D)
    time.sleep(0.25)


def verify_settings(window, settings, output):
    open_palette(window)
    pixels, width, height = EX.shot(window, output, "palette-open")
    dpi = user.GetDpiForWindow(window)
    scale = dpi / 96
    assert VIEW.colour_count(pixels, width, (0, round(96 * scale), width, height),
                            VIEW.ACCENT) > 0, "palette input caret was not drawn"
    press(window, 0x1B)
    execute(window, "ntrl-l")
    assert EX.fields(settings)["colorscheme"] == "neutral-light", "fuzzy theme selection failed"
    open_palette(window, "fz")
    press(window, 0x0D)
    time.sleep(0.2)
    EX.shot(window, output, "palette-fill")
    assert EX.fields(settings)["font_size"] == "13.5", "fill candidate executed prematurely"
    write_text(window, "18")
    # A candidate click must trigger the same settings/reflow path as Enter.
    click(window, width // 2, round((96 + 52 + 20) * scale))
    time.sleep(0.25)
    assert EX.fields(settings)["font_size"] == "18", "candidate click did not execute"
    SETTINGS.geometry(window, 18, "light", output, "palette-click-font")
    execute(window, "set guifont=Consolas:h21.5")
    assert EX.fields(settings)["font_family"] == "Consolas"
    SETTINGS.geometry(window, 21.5, "light", output, "palette-font")
    old = settings.read_bytes()
    execute(window, "set fontsize=90")
    assert settings.read_bytes() == old, "invalid literal was persisted"
    EX.shot(window, output, "palette-invalid")
    press(window, 0x1B)
    return {"dpi": dpi, "clientSize": [width, height], "themeFuzzyMatch": True,
            "fillThenValue": True, "clickExecutesAndReflows": True, "invalidRejected": True}


def verify_modes(window, output):
    _, _, width, height = rectangle(window, user.GetClientRect)
    dpi = user.GetDpiForWindow(window)
    ime = VIEW.ime_window(window)
    assert ime, "IME open-state stimulus unavailable"
    VIEW.set_ime_open(ime, True)
    assert VIEW.ime_open(ime) == 1, "IME could not be opened"
    open_palette(window)
    assert VIEW.ime_open(ime) == 0, "palette did not close ordinary-mode IME"
    press(window, 0x1B)
    time.sleep(0.25)
    assert VIEW.ime_open(ime) == 1, "palette did not restore ordinary-mode IME"
    VIEW.set_ime_open(ime, False)
    SETTINGS.chord(window, ord('A'))
    before, _, _ = EX.shot(window, output, "palette-ordinary-selection")
    open_palette(window, "no-result?")
    press(window, 0x0D)
    EX.shot(window, output, "palette-empty")
    click(window, 2, round(65 * dpi / 96))  # Outside panel; must consume the click.
    time.sleep(0.2)
    after, _, _ = EX.shot(window, output, "palette-selection-restored")
    assert before == after, "outside click changed body selection, caret or status"
    press(window, 0x1B)
    click(window, *VIEW.toggle_points(width, height, dpi)["vim"])
    time.sleep(0.2)
    for entry, name in (("", "normal"), ("i", "insert"), ("v", "visual"), ("V", "visual-line")):
        if entry:
            write_text(window, entry)
            time.sleep(0.2)
        if name == "insert":
            VIEW.set_ime_open(ime, True)
        before, _, _ = EX.shot(window, output, f"palette-before-{name}")
        open_palette(window, "fz")
        assert VIEW.ime_open(ime) == 0, f"IME remained open in {name} palette"
        press(window, 0x0D)  # Stage, then toggle closed; nothing is executed.
        SETTINGS.chord(window, ord('P'))
        after, _, _ = EX.shot(window, output, f"palette-after-{name}")
        assert before == after, f"palette changed original {name} state"
        if name == "insert":
            assert VIEW.ime_open(ime) == 1, "INSERT IME was not restored"
            VIEW.set_ime_open(ime, False)
        press(window, 0x1B)
        time.sleep(0.2)
    click(window, *VIEW.toggle_points(width, height, dpi)["ordinary"])
    time.sleep(0.2)
    return {"ordinarySelectionPreserved": True, "vimModesPreserved": True,
            "imeClosedAndRestored": True, "outsideClickConsumed": True}


def verify_navigation(window, settings, output):
    # Fit fewer rows than candidates; the last selected row must scroll into view.
    assert user.SetWindowPos(window, HWND_TOPMOST, 0, 0, 460, 360, 0x0002 | 0x0040)
    time.sleep(0.3)
    before, width, height = EX.shot(window, output, "palette-narrow-before")
    dpi = user.GetDpiForWindow(window)
    scale = dpi / 96
    old = settings.read_bytes()
    open_palette(window)
    EX.altgr(window, ord('P'))  # Ctrl+Alt+P must not toggle the palette.
    press(window, 0x26)  # Up wraps to the last candidate.
    time.sleep(0.2)
    EX.shot(window, output, "palette-narrow-last")
    SETTINGS.wheel(window, -120)  # Ctrl+wheel selects; it must not resize body text.
    press(window, 0x09)  # Tab selects the next candidate.
    press(window, 0x28)
    assert settings.read_bytes() == old, "palette wheel changed font size"
    # Query remains ':' through navigation. Unknown input leaves no executable candidate.
    write_text(window, "?")
    press(window, 0x0D)
    press(window, 0x1B)
    time.sleep(0.2)
    after, _, _ = EX.shot(window, output, "palette-narrow-after")
    assert before == after, "palette navigation scrolled or edited the body"
    open_palette(window, "x" * 255)
    time.sleep(0.2)
    pixels, _, _ = EX.shot(window, output, "palette-long")
    left = round(32 * scale)
    right = width - round(32 * scale)
    query = (left, round(96 * scale), right, round(148 * scale))
    assert VIEW.colour_count(pixels, width, query, VIEW.ACCENT) > 0, "long query hid its caret"
    assert EX.crop(before, width, (0, height - round(28 * scale), width, height)) == \
        EX.crop(pixels, width, (0, height - round(28 * scale), width, height)), "palette obscured status bar"
    press(window, 0x1B)
    # An actual body edit followed by palette cancellation must retain the body's undo.
    write_text(window, "X")
    open_palette(window, "drac")
    SETTINGS.chord(window, ord('Z'))  # Captured while the palette is active.
    press(window, 0x1B)
    assert "●" in window_title(window), "Ctrl+Z leaked to body history while palette was active"
    SETTINGS.chord(window, ord('Z'))
    assert "●" not in window_title(window), "palette cancellation broke body undo"
    return {"narrowWindow": [width, height], "navigationLeavesBodyAndSettings": True,
            "longQueryClippedWithCaret": True, "bodyUndoPreserved": True}


def verify_surface(window, settings, output):
    open_palette(window)
    time.sleep(0.2)
    pixels, width, height = EX.shot(window, output, "palette-count")
    dpi = user.GetDpiForWindow(window)
    scale = dpi / 96
    # The approved layout shows three rows at 800x450/120DPI. Footer contains count ink.
    bottom = round((96 + 52 + 3 * 40 + 34) * scale)
    box = (width - round(96 * scale), bottom - round(30 * scale),
           width - round(32 * scale), bottom - round(4 * scale))
    colours = {tuple(VIEW.pixel(pixels, width, x, y)) for y in range(box[1], box[3])
               for x in range(box[0], box[2])}
    assert len(colours) > 2, "candidate count footer was not drawn"
    press(window, 0x1B)
    execute(window, "colorscheme missing")
    assert not settings.exists(), "unknown theme saved settings"
    EX.shot(window, output, "palette-unknown-theme")
    # Ctrl+P now opens a fresh palette; its counter/query proves Enter closed the invalid one.
    open_palette(window)
    actual, _, _ = EX.shot(window, output, "palette-reopened")
    assert actual == pixels, "unknown theme did not close with an error and permit fresh input"
    press(window, 0x1B)
    return {"candidateCountVisible": True, "unknownThemeClosesWithoutSaving": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, default=ROOT / "build/NeNeNib.exe")
    parser.add_argument("--only", choices=("workflow", "surface"), default="workflow")
    args = parser.parse_args()
    output = ROOT / "out/palette-verification"
    output.mkdir(parents=True, exist_ok=True)
    executable = output / "NeNeNib.exe"
    shutil.copy2(args.executable, executable)
    profile = Path(tempfile.mkdtemp(prefix="profile-", dir=output)).resolve()
    environment = dict(os.environ, LOCALAPPDATA=str(profile), APPDATA=str(profile))
    settings = profile / "NeNeNib/settings.v1"
    document = output / "palette-document.txt"
    original = "\ncommand palette stays separate\n"
    document.write_text(original, encoding="utf-8", newline="\n")
    VIEW.become_dpi_aware()
    process, window, _ = start(executable, environment, [str(document)])
    try:
        SETTINGS.prepare(window)
        if args.only == "surface":
            result = verify_surface(window, settings, output)
        else:
            result = verify_settings(window, settings, output)
            result.update(verify_modes(window, output))
            result.update(verify_navigation(window, settings, output))
        SETTINGS.chord(window, ord('S'))
        assert document.read_text(encoding="utf-8") == original, "palette changed saved body"
        close(window)
        assert process.wait(timeout=5) == 0
    finally:
        stop(process)
    if args.only == "surface":
        (output / "palette-surface-results.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(result, indent=2))
        return
    process, window, _ = start(executable, environment, [str(document)])
    try:
        SETTINGS.prepare(window)
        result["restart"] = SETTINGS.geometry(window, 21.5, "light", output, "palette-restart")
        result["saved"] = EX.fields(settings)
        close(window)
        assert process.wait(timeout=5) == 0
    finally:
        stop(process)
    (output / "palette-results.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
