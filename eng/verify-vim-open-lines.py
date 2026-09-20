"""Issue #79 native o/O checks using the existing input, profile and capture paths."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import tempfile

from window_driver import VK_BACK, VK_ESCAPE, VK_RETURN, press, rectangle, user


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "out/issue79-native"
SPEC = importlib.util.spec_from_file_location(
    "character_search_verification", ROOT / "eng/verify-vim-character-search.py")
CHAR = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(CHAR)
VIEW = CHAR.VIEW


def capture_caret(window, name: str) -> dict:
    client = rectangle(window, user.GetClientRect)
    width, height = client[2] - client[0], client[3] - client[1]
    dpi = int(user.GetDpiForWindow(window))
    body = VIEW.body_points(width, height, dpi)
    box = (body["gutter"], body["firstRowTop"], width, body["bandBottom"])
    pixels, count = VIEW.await_accent(window, (width, height), box, True)
    assert count > 0, "the open-line caret must remain visible in the body"
    filename = f"{name}.bmp"
    VIEW.write_bitmap(OUTPUT / filename, pixels, width, height)
    return {"capture": filename, "accentPixels": count,
            "visibleLines": body["visibleLines"], "dpi": dpi}


def saved_bytes(window, document: Path, expected: str) -> str:
    CHAR.save(window, document)
    actual = document.read_bytes()
    assert actual == expected.encode("utf-8"), f"expected {expected!r}, saved {actual!r}"
    return actual.decode("utf-8")


def scenario(executable: Path, environment: dict[str, str], name: str, case: dict) -> dict:
    document = OUTPUT / f"{name}.txt"
    document.write_bytes(case["initial"].encode("utf-8"))
    process, window = CHAR.start_case(executable, environment, document)
    try:
        CHAR.enter_vim(window)
        CHAR.type_chars(window, case["open"])
        opened = saved_bytes(window, document, case["opened"])
        insert_capture = capture_caret(window, f"{name}-insert")
        for input_value in case["input"]:
            if isinstance(input_value, str):
                CHAR.type_chars(window, input_value)
            else:
                press(window, input_value)
                CHAR.pause()
        typed = saved_bytes(window, document, case["typed"])
        press(window, VK_ESCAPE)
        CHAR.pause()
        expanded = saved_bytes(window, document, case["expanded"])
        normal_capture = capture_caret(window, f"{name}-normal")
        # Saving closes the current undo unit (ADR 0010), including during INSERT.
        restored = []
        for expected in (case["typed"], case["opened"], case["initial"]):
            CHAR.type_chars(window, "u")
            restored.append(saved_bytes(window, document, expected))
        CHAR.finish(process, window)
        return {"keys": case["open"], "opened": opened, "typed": typed,
                "expanded": expanded, "undo": restored,
                "insert": insert_capture, "normal": normal_capture}
    finally:
        CHAR.stop(process)


def cases() -> dict:
    initial = "\r\n".join(f"line-{line:03d}" for line in range(1, 41))
    return {
        "belowEof": {"initial": initial, "open": "G3o", "opened": initial + "\r\n",
                     "input": ["あ", VK_RETURN, "😀"],
                     "typed": initial + "\r\nあ\r\n😀",
                     "expanded": initial + "\r\nあ\r\n😀" * 3},
        "aboveEmpty": {"initial": "", "open": "3O", "opened": "\r\n",
                       "input": [VK_BACK, "ab", VK_BACK, "日"],
                       "typed": "a日\r\n", "expanded": "a日\r\na日\r\na日\r\n"},
        "joinBefore": {"initial": "aa\nbb", "open": "j3O", "opened": "aa\n\nbb",
                       "input": [VK_BACK, "X"], "typed": "aaX\nbb",
                       "expanded": "aaXXX\nbb"},
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, default=ROOT / "build/NeNeNib.exe")
    parser.add_argument("--only", choices=tuple(cases()))
    args = parser.parse_args()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    executable = OUTPUT / "NeNeNib.exe"
    shutil.copy2(args.executable, executable)
    profile = Path(tempfile.mkdtemp(prefix="profile-", dir=OUTPUT)).resolve()
    assert profile.is_relative_to(OUTPUT.resolve())
    environment = dict(os.environ, LOCALAPPDATA=str(profile), APPDATA=str(profile))
    VIEW.become_dpi_aware()
    result = {"scope": "Issue #79 native Vim o/O", "selected": args.only or "all",
              "executableSha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
              "stimulus": "WM_CHAR UTF-16 / WM_KEYDOWN / SendInput Ctrl+S"}
    result_path = OUTPUT / f"vim-open-lines-{args.only or 'all'}-results.json"
    for name, case in cases().items():
        if args.only is None or args.only == name:
            result[name] = scenario(executable, environment, name, case)
            CHAR.write_result(result_path, result)
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
