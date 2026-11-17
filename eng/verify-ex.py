"""Focused native Ex settings checks (#64); reuse the existing window/pixel driver.

The profile and document are isolated. This does not run the unrelated GUI suites.
An unlocked foreground-capable Windows desktop is required for modifier input.
"""

import argparse
import ctypes as c
import importlib.util
import json
import os
from pathlib import Path
import shutil
import tempfile
import time

from window_driver import (close, click, press, press_chord, rectangle, start, stop, user,
                           window_title, write_text, VK_CONTROL, take_foreground, INPUT,
                           key_input, KEYEVENTF_KEYUP, await_setting, await_text,
                           read_settings_bytes, settings_fields, settings_path, wait_until)

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("settings_verification", ROOT / "eng/verify-settings.py")
SETTINGS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SETTINGS)
VIEW = SETTINGS.VIEW


def fields(path):
    """The saved key=value lines; waits (bounded) for a file the product is replacing (#1784)."""
    return settings_fields(path)


def saved(path, key, expected):
    """The saved value of key once it is expected (bounded wait); the last value seen otherwise."""
    return await_setting(path, key, lambda value: value == expected)


def unchanged(path, old):
    """The file still holds old (snapshot's bytes, or is still absent when old is None). A file
    mid-replacement reads again, so a torn read cannot pass."""
    if old is None:
        return not path.exists()
    return read_settings_bytes(path) == old


def command(window, text):
    write_text(window, ":" + text)
    press(window, 0x0D)
    time.sleep(0.3)


def shot(window, output, name):
    _, _, width, height = rectangle(window, user.GetClientRect)
    pixels = VIEW.capture(window, width, height)
    VIEW.write_bitmap(output / f"{name}.bmp", pixels, width, height)
    return pixels, width, height


def still_shot(window, output, name, seconds=3.0):
    """shot(), taken again until two frames in a row are equal (bounded), so a frame that is still
    changing (a preview being applied) is not the one judged (#1784)."""
    last = [shot(window, output, name)]

    def settled():
        current = shot(window, output, name)
        equal = current[0] == last[0][0]
        last[0] = current
        return equal
    wait_until(settled, seconds)
    return last[0]


def shot_like(window, output, name, expected, seconds=3.0):
    """shot(), taken again until the frame equals expected (bounded); the last frame otherwise.
    For a check that the screen came back to an earlier picture: a frame taken before the editor
    has painted the closing is not the one to judge, and the caller's assert still fails if the
    picture never comes back (#1784)."""
    last = [shot(window, output, name)]

    def same():
        last[0] = shot(window, output, name)
        return last[0][0] == expected
    wait_until(same, seconds)
    return last[0]


def snapshot(path):
    """The settings bytes now, or None when the file does not exist (no wait for a file not there)."""
    return read_settings_bytes(path) if path.exists() else None


def silhouette(pixels, width, box):
    """Where the box differs from its own background (its most common colour). Which pixels are
    ink survives a colour change, so the right status items can be compared while a colour scheme
    candidate is previewed (ADR 0150 / D59), when every colour in the window is the candidate's."""
    cells = crop(pixels, width, box)
    colours = [cells[i:i + 4] for i in range(0, len(cells), 4)]
    background = max(set(colours), key=colours.count)
    return bytes(colour != background for colour in colours)


def crop(pixels, width, box):
    left, top, right, bottom = box
    return b"".join(pixels[(y * width + left) * 4:(y * width + right) * 4]
                    for y in range(top, bottom))


def verify_input(window, settings, output):
    before, width, height = shot(window, output, "ex-normal")
    dpi = user.GetDpiForWindow(window)
    scale = dpi / 96
    right_status = (width - round(248 * scale), height - round(28 * scale), width, height)
    body = VIEW.body_points(width, height, dpi)
    write_text(window, ":colorscheme ")
    time.sleep(0.3)
    typed, _, _ = still_shot(window, output, "ex-typed")
    assert crop(before, width, right_status) == crop(typed, width, right_status), \
        "Ex input changed the right status items"
    press(window, 0x09)
    completing, _, _ = still_shot(window, output, "ex-completion")
    assert silhouette(before, width, right_status) == silhouette(completing, width, right_status), \
        "the candidate list or its colour preview changed the right status items"
    assert VIEW.colour_count(completing, width,
                            (body["gutter"], body["firstRowTop"], width,
                             body["firstRowTop"] + body["lineHeight"]), VIEW.ACCENT) == 0, \
        "body caret is still visible during Ex"
    # The list is sorted by name (the catalog of #419), so Tab on an empty prefix takes whichever
    # theme sorts first. Tab on a prefix that only one theme has names the theme to expect.
    press(window, 0x1B)
    write_text(window, ":colorscheme ubuntu-a")
    press(window, 0x09)
    press(window, 0x0D)
    assert saved(settings, "colorscheme", "ubuntu-aubergine") == "ubuntu-aubergine", "Tab did not select a theme"
    write_text(window, ":xset fontsize=18x")
    for key in (0x24, 0x2E, 0x23, 0x08, 0x0D):  # Home, Delete, End, Backspace, Enter
        press(window, key)
    time.sleep(0.3)
    assert saved(settings, "font_size", "18") == "18", "command editing keys did not edit Ex"
    old = snapshot(settings)
    write_text(window, ":set fontsize=39")
    SETTINGS.chord(window, ord('Z'))  # Must not reach text history.
    press(window, 0x1B)
    time.sleep(0.2)
    assert unchanged(settings, old), "Esc executed a pending command"
    command(window, "set fontsize=90")
    assert unchanged(settings, old), "an invalid size changed settings"
    shot(window, output, "ex-error")
    write_text(window, ":")
    press(window, 0x08)
    time.sleep(0.2)
    assert "●" not in window_title(window), "command keys altered the document"
    command(window, "set guifont=Consolas:h21.5")
    assert saved(settings, "font_family", "Consolas") == "Consolas"
    assert saved(settings, "font_size", "21.5") == "21.5"
    SETTINGS.geometry(window, 21.5, "dark", output, "ex-font")
    command(window, "colorscheme neutral-light")
    SETTINGS.geometry(window, 21.5, "light", output, "ex-light")
    before, _, _ = shot(window, output, "ex-before-long")
    write_text(window, ":" + "x" * 256)
    time.sleep(0.35)
    long_line, _, _ = shot(window, output, "ex-long")
    assert crop(before, width, right_status) == crop(long_line, width, right_status), \
        "long command escaped its clipping rectangle"
    left_box = (round(12 * scale), height - round(28 * scale), right_status[0], height)
    assert VIEW.colour_count(long_line, width, left_box, VIEW.ACCENT) > 0, \
        "horizontal tracking hid the command caret"
    assert press_chord(window, VK_CONTROL, ord('C')), "Ctrl+C stimulus was unavailable"
    time.sleep(0.2)
    return {"dpi": dpi, "clientSize": [width, height], "completionAndEditing": True,
            "bodyAndStatusPreserved": True, "cancelAndInvalidValue": True,
            "themeAndFontApplied": True, "longInputClippedWithCaret": True}


def altgr(window, key):
    assert take_foreground(window), "Ctrl+Alt stimulus was unavailable"
    keys = (INPUT * 6)(key_input(VK_CONTROL, 0), key_input(0x12, 0), key_input(key, 0),
                       key_input(key, KEYEVENTF_KEYUP), key_input(0x12, KEYEVENTF_KEYUP),
                       key_input(VK_CONTROL, KEYEVENTF_KEYUP))
    assert user.SendInput(len(keys), c.byref(keys), c.sizeof(INPUT)) == len(keys)
    time.sleep(0.25)


def verify_guards(window, settings, output):
    write_text(window, "jl")  # Keep the caret away from the top left for the click regression.
    time.sleep(0.2)
    before, width, height = shot(window, output, "ex-guard-caret")
    dpi = user.GetDpiForWindow(window)
    scale = dpi / 96
    status = (width - round(248 * scale), height - round(28 * scale), width, height)
    write_text(window, ":colorscheme")
    # Inside the tab, left of its close button: the button sits near 180 DIP now, and a click
    # there closes the only tab and the window with it.
    click(window, round(100 * scale), round(20 * scale))
    time.sleep(0.2)
    after, _, _ = shot(window, output, "ex-guard-tab")
    assert crop(before, width, status) == crop(after, width, status), "tab click moved the body caret"
    command(window, "colorscheme")
    click(window, *VIEW.toggle_points(width, height, dpi)["ordinary"])
    write_text(window, ":colorscheme ")
    time.sleep(0.2)
    assert "●" not in window_title(window), "a hidden toggle changed the mode"
    active, _, _ = shot(window, output, "ex-guard-active")
    for key in (ord('C'), ord('V')):
        altgr(window, key)
        guarded, _, _ = shot(window, output, f"ex-guard-altgr-{key}")
        assert guarded == active, "Ctrl+Alt was interpreted as an Ex Ctrl shortcut"
    press(window, 0x1B)
    command(window, "set guifont=Consolas|q:h12")
    assert not settings.exists(), "unsupported pipe syntax saved settings"
    assert "●" not in window_title(window), "guarded command input altered the body"
    return {"dpi": dpi, "tabClickPreservesCaret": True, "messageClickDoesNotToggleMode": True,
            "ctrlAltDoesNotTriggerCommandShortcuts": True, "pipeRejectedBeforeSaving": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, default=ROOT / "build/NeNeNib.exe")
    parser.add_argument("--only", choices=("input", "input-guards"), default="input")
    args = parser.parse_args()
    output = ROOT / "out/ex-verification"
    output.mkdir(parents=True, exist_ok=True)
    executable = output / "NeNeNib.exe"
    shutil.copy2(args.executable, executable)
    profile = Path(tempfile.mkdtemp(prefix="profile-", dir=output)).resolve()
    environment = dict(os.environ, LOCALAPPDATA=str(profile), APPDATA=str(profile))
    settings = settings_path(environment)
    document = output / "ex-document.txt"
    original = "\ncommand input stays separate\n"
    document.write_text(original, encoding="utf-8", newline="\n")
    VIEW.become_dpi_aware()
    process, window, _ = start(executable, environment, [str(document)])
    try:
        SETTINGS.prepare(window)
        _, _, width, height = rectangle(window, user.GetClientRect)
        click(window, *VIEW.toggle_points(width, height, user.GetDpiForWindow(window))["vim"])
        time.sleep(0.2)
        verifier = verify_input if args.only == "input" else verify_guards
        result = verifier(window, settings, output)
        SETTINGS.chord(window, ord('S'))
        assert await_text(document, original) == original, "Ex leaked into the saved document"
        close(window)
        assert process.wait(timeout=5) == 0
    finally:
        stop(process)
    if args.only == "input":
        process, window, _ = start(executable, environment, [str(document)])
        try:
            SETTINGS.prepare(window)
            result["restart"] = SETTINGS.geometry(window, 21.5, "light", output, "ex-restart")
            result["saved"] = fields(settings)
            close(window)
            assert process.wait(timeout=5) == 0
        finally:
            stop(process)
    name = "ex-results.json" if args.only == "input" else "ex-guards-results.json"
    (output / name).write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
