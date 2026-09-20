"""Focused native checks for Issue #72 Vim character search.

The editor is driven through the existing WM_CHAR/WM_KEYDOWN path.  A saved document is the
observable result, so this check does not add a second pixel or input driver.  The profile and
documents live under out/issue72-native and never touch the user's NeNe Nib settings.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import tempfile
import time

from window_driver import (VK_CONTROL, VK_ESCAPE, VK_S, click, close, press, press_chord,
                           rectangle, start, stop, user, window_title, write_text)


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "out/issue72-native"
VIEW_SPEC = importlib.util.spec_from_file_location("window_verification", ROOT / "eng/verify-window.py")
VIEW = importlib.util.module_from_spec(VIEW_SPEC)
assert VIEW_SPEC.loader is not None
VIEW_SPEC.loader.exec_module(VIEW)


def pause() -> None:
    time.sleep(0.25)


def type_chars(window, text: str) -> None:
    """Post UTF-16 code units as WM_CHAR, preserving the editor's normal route."""
    encoded = text.encode("utf-16-le")
    units = "".join(chr(int.from_bytes(encoded[index:index + 2], "little"))
                    for index in range(0, len(encoded), 2))
    write_text(window, units)
    pause()


def enter_vim(window) -> None:
    client = rectangle(window, user.GetClientRect)
    width, height = client[2] - client[0], client[3] - client[1]
    dpi = user.GetDpiForWindow(window)
    click(window, *VIEW.toggle_points(width, height, dpi)["vim"])
    pause()


def save(window, document: Path) -> str:
    assert press_chord(window, VK_CONTROL, VK_S), "foreground input unavailable; native check cannot save"
    pause()
    actual = document.read_text(encoding="utf-8")
    assert "●" not in window_title(window), "Ctrl+S did not clear the document's dirty marker"
    return actual


def finish(process, window) -> None:
    close(window)
    assert process.wait(timeout=5) == 0, "editor did not close cleanly"


def start_case(executable: Path, environment: dict[str, str], document: Path):
    return start(executable, environment, [str(document)])[:2]


def write_result(path: Path, result: dict) -> None:
    path.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def movement_case(executable: Path, environment: dict[str, str], name: str, initial: str,
                 keys: str, expected: str) -> dict:
    document = OUTPUT / f"{name}.txt"
    document.write_text(initial, encoding="utf-8", newline="\n")
    process, window = start_case(executable, environment, document)
    try:
        enter_vim(window)
        dpi = int(user.GetDpiForWindow(window))
        type_chars(window, keys)
        actual = save(window, document)
        assert actual == expected, f"{name}: expected {expected!r}, got {actual!r}"
        finish(process, window)
        return {"initial": initial, "keys": keys, "expected": expected, "saved": actual,
                "wmCharPath": True, "dpi": dpi}
    finally:
        stop(process)


def pending_and_escape_case(executable: Path, environment: dict[str, str]) -> dict:
    initial = "aXbXc"
    document = OUTPUT / "pending-escape.txt"
    document.write_text(initial, encoding="utf-8", newline="\n")
    process, window = start_case(executable, environment, document)
    try:
        enter_vim(window)
        dpi = int(user.GetDpiForWindow(window))
        type_chars(window, "f")
        pending_bytes = document.read_bytes()
        pending_title = window_title(window)
        assert pending_bytes == initial.encode("utf-8"), "pending f changed the file"
        assert "●" not in pending_title, "pending f marked the document dirty"
        press(window, VK_ESCAPE)
        pause()
        type_chars(window, "x")
        changed = save(window, document)
        assert changed == "XbXc", f"Esc cancellation did not restore normal x: {changed!r}"
        type_chars(window, "u")
        restored = save(window, document)
        assert restored == initial, f"Esc cancellation undo expected {initial!r}, got {restored!r}"
        finish(process, window)
        return {"initial": initial, "pendingKey": "f", "pendingBytesUnchanged": True,
                "escape": "WM_KEYDOWN", "cancelledThenX": changed, "undo": restored,
                "dpi": dpi}
    finally:
        stop(process)


def operator_case(executable: Path, environment: dict[str, str], name: str, initial: str,
                  keys: str, changed: str) -> dict:
    document = OUTPUT / f"{name}.txt"
    document.write_text(initial, encoding="utf-8", newline="\n")
    process, window = start_case(executable, environment, document)
    try:
        enter_vim(window)
        dpi = int(user.GetDpiForWindow(window))
        if name == "change":
            type_chars(window, "cfXQ")
            press(window, VK_ESCAPE)
            pause()
        else:
            type_chars(window, keys)
        saved = save(window, document)
        assert saved == changed, f"{name}: expected {changed!r}, got {saved!r}"
        type_chars(window, "u")
        restored = save(window, document)
        assert restored == initial, f"{name}: undo expected {initial!r}, got {restored!r}"
        finish(process, window)
        return {"initial": initial, "keys": keys, "changed": changed, "saved": saved,
                "undo": restored, "wmCharPath": True, "dpi": dpi}
    finally:
        stop(process)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, default=ROOT / "build/NeNeNib.exe")
    parser.add_argument("--only", choices=("pendingAndEscape", "ascii", "unicode",
                                            "unicodeSurrogatePair", "delete", "change", "yank"))
    args = parser.parse_args()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    executable = OUTPUT / "NeNeNib.exe"
    shutil.copy2(args.executable, executable)
    profile = Path(tempfile.mkdtemp(prefix="profile-", dir=OUTPUT)).resolve()
    assert profile.is_relative_to(OUTPUT.resolve())
    environment = dict(os.environ, LOCALAPPDATA=str(profile), APPDATA=str(profile))
    VIEW.become_dpi_aware()

    scenarios = {
        "pendingAndEscape": lambda: pending_and_escape_case(executable, environment),
        "ascii": lambda: movement_case(executable, environment, "ascii", "aXbXc", "fXx", "abXc"),
        "unicode": lambda: movement_case(executable, environment, "unicode", "aあbあc", "fあx", "abあc"),
        "unicodeSurrogatePair": lambda: movement_case(executable, environment, "unicode-surrogate",
                                                        "a😀b😀c", "f😀x", "ab😀c"),
        "delete": lambda: operator_case(executable, environment, "delete", "aXbXc", "dfX", "bXc"),
        "change": lambda: operator_case(executable, environment, "change", "aXbXc", "cfXQ<Esc>", "QbXc"),
        "yank": lambda: operator_case(executable, environment, "yank", "aXbXc", "yfXp", "aaXXbXc"),
    }
    result = {
        "scope": "Issue #72 native character search input path",
        "stimulus": {"printable": "WM_CHAR", "escape": "WM_KEYDOWN", "save": "SendInput Ctrl+S"},
        "selected": args.only or "all",
    }
    result_path = OUTPUT / (f"vim-character-search-{args.only}-results.json"
                            if args.only else "vim-character-search-all-results.json")
    for name, scenario in scenarios.items():
        if args.only is None or args.only == name:
            result[name] = scenario()
            write_result(result_path, result)
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
