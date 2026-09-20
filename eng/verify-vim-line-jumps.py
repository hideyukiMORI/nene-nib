"""Focused native checks for Issue #76 Vim line jumps and linewise operators.

The input, save, isolated-profile, and checkpoint paths come from the existing character-search
verification.  The view probes come from verify-window.py; this script adds no Win32 driver.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import tempfile

from window_driver import VK_ESCAPE, rectangle, user, press


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "out/issue76-native"
CHAR_SPEC = importlib.util.spec_from_file_location(
    "vim_character_search_verification", ROOT / "eng/verify-vim-character-search.py"
)
CHAR = importlib.util.module_from_spec(CHAR_SPEC)
assert CHAR_SPEC.loader is not None
CHAR_SPEC.loader.exec_module(CHAR)

pause = CHAR.pause
type_chars = CHAR.type_chars
enter_vim = CHAR.enter_vim
save = CHAR.save
finish = CHAR.finish
start_case = CHAR.start_case
write_result = CHAR.write_result
VIEW = CHAR.VIEW


def lines_document() -> str:
    return "\n".join(f"line-{number:03d}" for number in range(1, 81))


def replace_first_character(text: str, line_number: int) -> str:
    lines = text.split("\n")
    lines[line_number - 1] = lines[line_number - 1][1:]
    return "\n".join(lines)


def window_view(window) -> tuple[tuple[int, int], int, dict]:
    client = rectangle(window, user.GetClientRect)
    width, height = client[2] - client[0], client[3] - client[1]
    dpi = int(user.GetDpiForWindow(window))
    return (width, height), dpi, VIEW.body_points(width, height, dpi)


def probe_body_caret(window, size: tuple[int, int], body: dict, name: str) -> dict:
    box = (body["gutter"], body["firstRowTop"], size[0], body["bandBottom"])
    pixels, accent = VIEW.await_accent(window, size, box, True)
    assert accent > 0, "Vim block caret is not visible in the body"
    capture_name = f"jump-{name}.bmp"
    VIEW.write_bitmap(OUTPUT / capture_name, pixels, *size)
    return {"accentPixelsInBody": accent, "capture": capture_name}


def pending_and_escape(executable: Path, environment: dict[str, str]) -> dict:
    initial = lines_document()
    document = OUTPUT / "pending-escape.txt"
    document.write_text(initial, encoding="utf-8", newline="\n")
    process, window = start_case(executable, environment, document)
    try:
        enter_vim(window)
        dpi = int(user.GetDpiForWindow(window))
        type_chars(window, "g")
        assert document.read_bytes() == initial.encode("utf-8"), "pending g changed the file"
        assert "●" not in CHAR.window_title(window), "pending g marked the document dirty"
        press(window, VK_ESCAPE)
        pause()
        type_chars(window, "x")
        changed = save(window, document)
        expected = replace_first_character(initial, 1)
        assert changed == expected, f"Esc cancellation did not restore normal x: {changed!r}"
        type_chars(window, "u")
        restored = save(window, document)
        assert restored == initial, "undo did not restore the line-jump input document"
        finish(process, window)
        return {"initial": initial, "pendingKey": "g", "cancelledThenX": changed,
                "undo": restored, "dpi": dpi}
    finally:
        CHAR.stop(process)


def jump_case(executable: Path, environment: dict[str, str], name: str, keys: str,
              target_line: int) -> dict:
    initial = lines_document()
    document = OUTPUT / f"jump-{name}.txt"
    document.write_text(initial, encoding="utf-8", newline="\n")
    process, window = start_case(executable, environment, document)
    try:
        enter_vim(window)
        size, dpi, body = window_view(window)
        assert 80 > body["visibleLines"], "the line-jump document must exceed visible lines"
        type_chars(window, keys)
        caret = probe_body_caret(window, size, body, name)
        saved = save_after_x(window, document, replace_first_character(initial, target_line))
        type_chars(window, "u")
        restored = save(window, document)
        assert restored == initial
        finish(process, window)
        return {"keys": keys, "targetLine": target_line, "lineCount": 80,
                "visibleLines": body["visibleLines"], "caret": caret, "saved": saved,
                "undo": restored, "dpi": dpi}
    finally:
        CHAR.stop(process)


def save_after_x(window, document: Path, expected: str) -> str:
    type_chars(window, "x")
    actual = save(window, document)
    assert actual == expected, f"line jump saved {actual!r}, expected {expected!r}"
    return actual


def delete_to_end(executable: Path, environment: dict[str, str]) -> dict:
    initial = lines_document()
    document = OUTPUT / "delete-to-end.txt"
    document.write_text(initial, encoding="utf-8", newline="\r\n")
    process, window = start_case(executable, environment, document)
    try:
        enter_vim(window)
        dpi = int(user.GetDpiForWindow(window))
        type_chars(window, "12GdG")
        changed = save(window, document)
        expected = "\n".join(initial.split("\n")[:11])
        assert changed == expected, "12GdG did not leave the first 11 lines"
        assert document.read_bytes() == expected.replace("\n", "\r\n").encode("utf-8")
        type_chars(window, "u")
        restored = save(window, document)
        assert restored == initial
        assert document.read_bytes() == initial.replace("\n", "\r\n").encode("utf-8")
        finish(process, window)
        return {"keys": "12GdG", "changed": changed, "undo": restored,
                "crlfBytesPreserved": True, "dpi": dpi}
    finally:
        CHAR.stop(process)


def yank_to_start(executable: Path, environment: dict[str, str]) -> dict:
    initial = lines_document()
    document = OUTPUT / "yank-to-start.txt"
    document.write_text(initial, encoding="utf-8", newline="\n")
    process, window = start_case(executable, environment, document)
    try:
        enter_vim(window)
        dpi = int(user.GetDpiForWindow(window))
        type_chars(window, "3Gyggp")
        lines = initial.split("\n")
        expected = "\n".join([*lines[:1], *lines[:3], *lines[1:]])
        changed = save(window, document)
        assert changed == expected, "3Gyggp did not paste the first three lines after line one"
        type_chars(window, "u")
        restored = save(window, document)
        assert restored == initial
        finish(process, window)
        return {"keys": "3Gyggp", "changed": changed, "undo": restored, "dpi": dpi}
    finally:
        CHAR.stop(process)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, default=ROOT / "build/NeNeNib.exe")
    parser.add_argument("--only", choices=("pendingAndEscape", "G", "gg", "12G",
                                            "deleteToEnd", "yankToStart"))
    args = parser.parse_args()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    executable = OUTPUT / "NeNeNib.exe"
    shutil.copy2(args.executable, executable)
    profile = Path(tempfile.mkdtemp(prefix="profile-", dir=OUTPUT)).resolve()
    assert profile.is_relative_to(OUTPUT.resolve())
    environment = dict(os.environ, LOCALAPPDATA=str(profile), APPDATA=str(profile))
    VIEW.become_dpi_aware()
    scenarios = {
        "pendingAndEscape": lambda: pending_and_escape(executable, environment),
        "G": lambda: jump_case(executable, environment, "G", "G", 80),
        "gg": lambda: jump_case(executable, environment, "gg", "Ggg", 1),
        "12G": lambda: jump_case(executable, environment, "12G", "G12G", 12),
        "deleteToEnd": lambda: delete_to_end(executable, environment),
        "yankToStart": lambda: yank_to_start(executable, environment),
    }
    result = {"scope": "Issue #76 native Vim line jumps", "selected": args.only or "all",
              "executableSha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
              "stimulus": {"printable": "WM_CHAR UTF-16 units", "escape": "WM_KEYDOWN",
                            "save": "SendInput Ctrl+S"}}
    result_path = OUTPUT / (f"vim-line-jumps-{args.only}-results.json"
                            if args.only else "vim-line-jumps-all-results.json")
    for name, scenario in scenarios.items():
        if args.only is None or args.only == name:
            result[name] = scenario()
            write_result(result_path, result)
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
