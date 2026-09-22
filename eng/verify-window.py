"""Interactive Windows verification of the window slices, separate from the unit tests (QLT-013).

Starts build/NeNeNib.exe with an isolated environment, finds its window by class name, records the
geometry and DPI, asks the window procedure where the caption and the close button are, reads the
composed pixels back from the screen device context, drives the mode toggle and the editing keys
with posted messages, and closes the window with WM_CLOSE. It never moves the real pointer; the
IME, save, and Vim viewport modifier checks use foreground-acquired real keyboard input. Requires
an unlocked interactive Windows session with DWM running;
eng/check.ps1 and CI do not call it, because a gate must not need a display (QLT-013).

Posted messages cannot carry modifier state: GetKeyState only follows keys that really went through
the raw input queue, so Ctrl+A / C / X / V / Z / Y cannot be driven from here without real input.
Those live in the unit tests (Issue #7); the Vim viewport Ctrl-d/u/f/b checks use SendInput after
foreground acquisition. The remaining editing slice drives WM_CHAR, Enter, Backspace, Esc, PgUp,
PgDn and a posted click in the body, which need no modifier.

Issue #22 adds the Vim slice: the toggle enters NORMAL, `ihello<Esc>` types into the empty buffer,
`0x` removes the first character, `yyp` (Issue #43) puts the line below itself, and `uuu` puts the
buffer back. Every one of those keys arrives as WM_CHAR or as Esc, so the whole check runs on
posted messages too.

Issue #24 adds the first-paint record: ADR 0013 shows the window before the device is created,
so the client area is the Mica backdrop alone for about 160 ms. This script starts one more editor
with --measure, samples the centre of the client area every 10 ms until the body background
appears, and records the pixels it saw in between; a black or a white one is the ADR's rejection
condition, so it is an assertion here rather than a note.

Issue #28 adds the IME slice (ADR 0014): the open status of the editor's input context is
readable from outside through its default IME window, so "Vim NORMAL turns the IME off and INSERT
brings it back" is checked with posted messages alone. The composed text itself needs keystrokes
the IME can see, so that part generates real input after taking the foreground and records what it
found rather than failing when the session has no Japanese IME or refuses the foreground. The
machine is left with the IME open status it was found in.

Issue #11 adds the file slice: two temporary files (UTF-8 CRLF and Shift_JIS LF) are written under
out/window-verification, opened with the startup argument, and the drawn rows, the window title
and the two status items are read back. Ctrl+S is the one chord this script does try to drive with
SendInput, after bringing the window to the foreground; when the session refuses the foreground the
run records that instead of failing (ADR 0010). The unsaved confirmation on WM_CLOSE is answered
with IDNO, because the script types into the buffer before it closes the window. A startup argument
that names no file must say so in one line and then carry on with an empty 無題 buffer.

Issue #31 adds the title bar's own ground (D16): the band is the opaque title_bar token and the
active tab is the body ground, so both are read back as exact colours instead of as "the band
differs from the body behind the tint".
Python standard library and ctypes only.
"""

from __future__ import annotations

import argparse
import ctypes as c
from ctypes import wintypes as w
import json
import os
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import time
import winreg

# 窓の駆動（起動・検出・PostMessageW・確認ダイアログ・終了）は 1 本しかない（ADR 0011 の決定 7）。
from window_driver import (acknowledge_dialog, api, ask_hit, await_dialog, become_dpi_aware,
                           capture as capture_client, capture_png, click, close, covered_by,
                           dismiss_dialog, gdi, GWL_STYLE, HTCAPTION, HTCLOSE, HWND_TOPMOST, IDNO,
                           parse_keys, press, press_chord, raise_window, rectangle,
                           send_key_sequence, send_keys, start, stop, SWP_NOMOVE_NOSIZE_SHOW,
                           user, VK_BACK, VK_CONTROL, VK_ESCAPE, VK_NEXT, VK_NIHONGO, VK_PRIOR,
                           VK_RETURN, VK_S, VK_SPACE, window_title, WINDOW_CLASS, write_png,
                           write_text, WS_CAPTION, WS_POPUP, WS_THICKFRAME, WS_VISIBLE)

imm = c.WinDLL("imm32", use_last_error=True)

api(gdi, "GetPixel", w.DWORD, w.HDC, c.c_int, c.c_int)
# IME の開閉は、その窓の既定 IME 窓に WM_IME_CONTROL を送れば外から読める（ADR 0014 の決定 5）。
api(user, "GetKeyboardLayout", c.c_ssize_t, w.DWORD)
api(imm, "ImmGetDefaultIMEWnd", w.HWND, w.HWND)

CLR_INVALID = 0xFFFFFFFF
# ADR 0013: 窓は device の生成より前に見える。その間のクライアント領域は DWM の Mica の面で、
# 黒か白が見えたら決定そのものが却下になるので、起動直後の画素の列をここに記録する。
FIRST_PAINT_SAMPLES = 150
FIRST_PAINT_INTERVAL = 0.01
# 「1 段の差」。最初のフレームの本文背景とこれ以上離れた面が見えていたら数字を報告に残す。
FIRST_PAINT_STEP = 32
BLACK = [0, 0, 0]
WHITE = [255, 255, 255]
# Mica（DWMWA_SYSTEMBACKDROP_TYPE）は Windows 11 22H2 以降でだけ掛かる（ADR 0008）。
MICA_BUILD = 22621
# src/core/BuiltinTheme.hpp の正本と同じ値。ここが食い違ったら、どちらかが間違っている。
PALETTE = {"light": (0xF4, 0xF5, 0xF7), "dark": (0x30, 0x0A, 0x24)}
# タブの帯は不透明の title_bar で塗り、アクティブなタブは本文の地と同じ tab_active（D16）。
TITLE_BAR = {"light": (0xE1, 0xE4, 0xE9), "dark": (0x1E, 0x05, 0x16)}
TAB_ACTIVE = {"light": (0xF4, 0xF5, 0xF7), "dark": (0x30, 0x0A, 0x24)}
ACCENT = (0xE9, 0x54, 0x20)
TOGGLE = {"light": (0xDF, 0xE3, 0xE8), "dark": (0x4A, 0x1E, 0x3D)}
CURRENT_LINE = {"light": (0xE6, 0xE8, 0xEC), "dark": (0x3E, 0x1A, 0x32)}
STATUS_BAND = {"light": (0xE9, 0xEB, 0xEF), "dark": (0x26, 0x07, 0x1D)}
# src/core/TitleBarLayout.cpp / StatusBarLayout.cpp の DIP。同じ整数丸めで物理画素へ直す。
TITLE_BAR_DIPS = 40
CAPTION_BUTTON_DIPS = 46
# タブの左上と高さ（src/core/TitleBarLayout.cpp）と、題名までの左余白
# （src/ui/win32/Direct2DRenderer.cpp の tab_padding_left_dips）。面だけを読む点を出すのに要る。
TAB_LEFT_DIPS = 8
TAB_TOP_DIPS = 8
TAB_HEIGHT_DIPS = 32
TAB_PADDING_LEFT_DIPS = 14
STATUS_BAR_DIPS = 28
STATUS_PADDING_DIPS = 12
TOGGLE_PADDING_DIPS = 2
SEGMENT_WIDTH_DIPS = 44
SEGMENT_HEIGHT_DIPS = 20
STATUS_ITEM_GAP_DIPS = 16
# 文字コードの項目は「UTF-8 BOM」「Shift_JIS」が入る 72 DIP（ADR 0010 の決定 14）。
STATUS_ITEM_WIDTH_DIPS = (96, 72, 36)
# トグルの幅と、その右のモード名（通常 / NORMAL / INSERT）の幅。core::status_bar_layout と同じ。
TOGGLE_WIDTH_DIPS = TOGGLE_PADDING_DIPS * 3 + SEGMENT_WIDTH_DIPS * 2
MODE_LABEL_WIDTH_DIPS = 72
# src/core/BodyLayout.cpp の DIP。行の帯とキャレットの位置はここから同じ整数丸めで出す。
BODY_TOP_DIPS = 12
BODY_DEFAULT_POINTS = 13.5
BODY_GUTTER_DIPS = 56
TYPED_LINES = 200
PAGE_KEYS = 30
# IME（ADR 0014）。ime トークンは src/core/BuiltinTheme.hpp の正本と同じ値。
IME_TOKEN = {"light": (0x5E, 0x27, 0x50), "dark": (0xD7, 0xC4, 0xE5)}
WM_IME_CONTROL = 0x0283
IMC_GETOPENSTATUS = 0x0005
IMC_SETOPENSTATUS = 0x0006
JAPANESE_LANGUAGE = 0x0411
# IME が鍵を食べて変換文字列を作るまで待つ。押した鍵は 7 つで、描画は WM_PAINT に畳まれる。
IME_SETTLE_SECONDS = 1.5


def expected_appearance() -> str:
    """The same AppsUseLightTheme value that src/adapters/win32 reads."""
    key = r"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize"
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, key) as handle:
            value, kind = winreg.QueryValueEx(handle, "AppsUseLightTheme")
    except OSError:
        return "dark"
    if kind != winreg.REG_DWORD:
        return "dark"
    return "dark" if value == 0 else "light"


def capture(window, width: int, height: int) -> bytes:
    """window_driver.capture (the one read-back path), checked against the size the caller measured."""
    captured_width, captured_height, pixels = capture_client(window)
    assert (captured_width, captured_height) == (width, height), "the client area changed size"
    return pixels


def snapshot(frames: Path | None, window, name: str) -> None:
    """--capture: save the client area after a section as <dir>/<name>.png (Issue #131)."""
    if frames is not None:
        capture_png(window, frames / f"{name}.png")


def pixel(pixels: bytes, width: int, x: int, y: int) -> list[int]:
    start_index = (y * width + x) * 4
    blue, green, red = pixels[start_index:start_index + 3]
    return [red, green, blue]


def write_bitmap(path: Path, pixels: bytes, width: int, height: int) -> None:
    rows = [pixels[y * width * 4:(y + 1) * width * 4] for y in range(height)]
    body = b"".join(reversed(rows))
    header = struct.pack("<2sIHHI", b"BM", 14 + 40 + len(body), 0, 0, 14 + 40)
    info = struct.pack("<IiiHHIIiiII", 40, width, height, 1, 32, 0, len(body), 0, 0, 0, 0)
    path.write_bytes(header + info + body)


def to_pixels(dips: int, dpi: int) -> int:
    """The same integer rounding as core::to_pixels (src/core/DevicePixels.hpp)."""
    return (dips * dpi + 48) // 96


def toggle_points(width: int, height: int, dpi: int) -> dict:
    """The centres of the two toggle halves, mirroring core::status_bar_layout."""
    band = to_pixels(STATUS_BAR_DIPS, dpi)
    padding = to_pixels(TOGGLE_PADDING_DIPS, dpi)
    segment = to_pixels(SEGMENT_WIDTH_DIPS, dpi)
    segment_height = to_pixels(SEGMENT_HEIGHT_DIPS, dpi)
    band_top = max(height - band, 0)
    box_height = segment_height + padding * 2
    box_top = band_top + ((height - band_top) - box_height) // 2
    left = to_pixels(STATUS_PADDING_DIPS, dpi)
    ordinary_left = left + padding
    vim_left = ordinary_left + segment + padding
    middle = box_top + padding + segment_height // 2
    return {
        "ordinary": [ordinary_left + segment // 2, middle],
        "vim": [vim_left + segment // 2, middle],
        "vimGround": [vim_left + to_pixels(5, dpi), middle],
        "ordinaryGround": [ordinary_left + to_pixels(5, dpi), middle],
    }


def active_tab_point(dpi: int) -> list:
    """A point inside the active tab's fill, mirroring core::tab_rect and draw_tab.

    It sits half way into the title's left padding and at half the tab's height, so it is clear of
    the rounded corner, of the glyphs and of the accent underline along the bottom.
    """
    padding = to_pixels(TAB_PADDING_LEFT_DIPS, dpi)
    return [to_pixels(TAB_LEFT_DIPS, dpi) + padding // 2,
            to_pixels(TAB_TOP_DIPS, dpi) + to_pixels(TAB_HEIGHT_DIPS, dpi) // 2]


def write_documents(output: Path) -> dict:
    """Two files the editor must open from the command line: UTF-8 CRLF and Shift_JIS LF."""
    folder = output / "documents"
    folder.mkdir(parents=True, exist_ok=True)
    utf8 = folder / "utf8-crlf.txt"
    utf8.write_bytes("一行目\r\n二行目\r\n三行目".encode("utf-8"))
    shift_jis = folder / "sjis-lf.txt"
    shift_jis.write_bytes("日本語\n二行目".encode("cp932"))
    return {"utf8": utf8, "shiftJis": shift_jis}


def measure_document(window, appearance: str, rows: int, output: Path, name: str) -> dict:
    """Rows drawn, the window title, and the two status items of an opened file."""
    client = rectangle(window, user.GetClientRect)
    width, height = client[2] - client[0], client[3] - client[1]
    dpi = user.GetDpiForWindow(window)
    body = body_points(width, height, dpi)
    grounds = [list(PALETTE[appearance]), list(CURRENT_LINE[appearance]), list(ACCENT)]
    band = [list(STATUS_BAND[appearance])]
    pixels = capture(window, width, height)
    write_bitmap(output / f"file-slice-{name}.bmp", pixels, width, height)
    return {
        "title": window_title(window),
        "rowInk": [ink(pixels, width, content_box(body, index, dpi), grounds)
                   for index in range(rows + 1)],
        "gutterInk": [ink(pixels, width, gutter_box(body, index, dpi), grounds)
                      for index in range(rows + 1)],
        "encodingInk": ink(pixels, width, status_item_box(width, height, dpi, 1), band),
        "endingInk": ink(pixels, width, status_item_box(width, height, dpi, 2), band),
        "capture": f"file-slice-{name}.bmp",
    }


def try_saving(window, path: Path) -> dict:
    """Ctrl+S with SendInput; a session that refuses the foreground is recorded, not failed."""
    before = path.read_bytes()
    if not press_chord(window, VK_CONTROL, VK_S):
        return {"driven": False,
                "note": "the window could not take the foreground; Ctrl+S stays in the unit tests"}
    time.sleep(1.2)
    after = path.read_bytes()
    saved = {"driven": True, "bytesChanged": after != before, "title": window_title(window),
             "bytesBefore": len(before), "bytesAfter": len(after)}
    assert saved["bytesChanged"], "Ctrl+S did not change the file on disk"
    assert not saved["title"].startswith("● "), f"the unsaved mark survived the save: {saved}"
    return saved


def verify_document(executable: Path, environment: dict, appearance: str, output: Path,
                    plan: dict) -> dict:
    """Open one file with the startup argument and read the result back from the screen."""
    path = plan["path"]
    process, window, _ = start(executable, environment, [str(path)])
    try:
        assert user.SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE_NOSIZE_SHOW)
        time.sleep(0.6)
        measured = measure_document(window, appearance, plan["rows"], output, path.stem)
        measured["file"] = path.name
        assert measured["title"] == f"{path.name} - NeNe Nib", measured["title"]
        for index in range(plan["rows"]):
            assert measured["rowInk"][index] > 0, f"row {index + 1} was not drawn: {measured}"
            assert measured["gutterInk"][index] > 0, f"line number {index + 1} is missing"
        assert measured["gutterInk"][plan["rows"]] == 0, "one line too many was drawn"
        assert measured["encodingInk"] > 0, "the encoding item is empty"
        assert measured["endingInk"] > 0, "the line ending item is empty"
        write_text(window, "X")
        time.sleep(0.6)
        measured["titleAfterTyping"] = window_title(window)
        assert measured["titleAfterTyping"] == f"● {path.name} - NeNe Nib", measured
        if plan["save"]:
            measured["save"] = try_saving(window, path)
        close(window)
        measured["confirmationDismissed"] = dismiss_dialog(process, IDNO)
        measured["exitCode"] = process.wait(timeout=5)
        assert measured["exitCode"] == 0, measured["exitCode"]
    finally:
        stop(process)
    return measured


def verify_missing_document(executable: Path, environment: dict, output: Path) -> dict:
    """A startup argument that is not there is reported in one line, and the editor carries on."""
    missing = output / "documents" / "not-there.txt"
    missing.unlink(missing_ok=True)
    process, window, _ = start(executable, environment, [str(missing)])
    reported = False
    try:
        reported = acknowledge_dialog(process)
        assert reported, "a missing startup argument was opened in silence"
        time.sleep(0.5)
        result = {"reported": reported, "title": window_title(window)}
        assert result["title"] == "無題 - NeNe Nib", result["title"]
        close(window)
        # 本文は空のままなので、閉じるときに未保存の確認は出ない。
        result["confirmationAsked"] = bool(await_dialog(process, 1.5))
        assert not result["confirmationAsked"], "an untouched buffer asked about saving"
        result["exitCode"] = process.wait(timeout=5)
        assert result["exitCode"] == 0, result["exitCode"]
    finally:
        stop(process)
    return result


def screen_pixel(window, x: int, y: int) -> list[int]:
    """One composed pixel at a client point, read without capturing the whole client area."""
    point = w.POINT(x, y)
    assert user.ClientToScreen(window, c.byref(point))
    screen = user.GetDC(None)
    try:
        value = int(gdi.GetPixel(screen, point.x, point.y))
    finally:
        user.ReleaseDC(None, screen)
    assert value != CLR_INVALID, "the screen device context has no pixel at that point"
    return [value & 0xFF, (value >> 8) & 0xFF, (value >> 16) & 0xFF]


def sample_first_paint(window, centre: tuple, background: list) -> list:
    """The centre pixel every 10 ms from the moment the window is visible until the body shows."""
    started = time.monotonic()
    samples = []
    for _ in range(FIRST_PAINT_SAMPLES):
        samples.append({"atMs": round((time.monotonic() - started) * 1000.0, 1),
                        "pixel": screen_pixel(window, centre[0], centre[1]),
                        "ours": covered_by(window) is None})
        if samples[-1]["pixel"] == background and samples[-1]["ours"]:
            break
        time.sleep(FIRST_PAINT_INTERVAL)
    return samples


def judge_first_paint(samples: list, background: list) -> dict:
    """Everything seen before the body background arrived, judged by ADR 0013's condition."""
    arrived = [index for index, entry in enumerate(samples)
               if entry["pixel"] == background and entry["ours"]]
    # `ours` が偽の列だけが返ったときは、別の窓が中央を覆っている（その面を測っても意味が無い）。
    assert arrived, (f"the body background {background} never reached the centre of the window;"
                     f" is another window covering it? last samples: {samples[-3:]}")
    before = [entry for entry in samples[:arrived[0]] if entry["ours"]]
    distances = [max(abs(value - ground) for value, ground in zip(entry["pixel"], background))
                 for entry in before]
    return {"samples": samples, "backgroundAtSample": arrived[0],
            "beforeFirstFrame": before,
            "blackSeen": [entry for entry in before if entry["pixel"] == BLACK],
            "whiteSeen": [entry for entry in before if entry["pixel"] == WHITE],
            "maxChannelDistance": max(distances, default=0),
            "withinOneStep": all(distance <= FIRST_PAINT_STEP for distance in distances)}


def verify_first_paint(executable: Path, environment: dict, appearance: str, output: Path) -> dict:
    """ADR 0013: between window_shown and the first frame only the Mica backdrop is on screen."""
    marks = output / "first-paint-marks.json"
    marks.unlink(missing_ok=True)
    background = list(PALETTE[appearance])
    process, window, _ = start(executable, environment, ["--measure", str(marks)])
    try:
        client = rectangle(window, user.GetClientRect)
        width, height = client[2] - client[0], client[3] - client[1]
        centre = (width // 2, height // 2)
        result = judge_first_paint(sample_first_paint(window, centre, background), background)
        pixels = capture(window, width, height)
        write_bitmap(output / "first-paint.bmp", pixels, width, height)
        result["centrePixelAfterFirstFrame"] = pixel(pixels, width, centre[0], centre[1])
        close(window)
        # 本文は打っていないので未保存の確認は出ない。
        result["exitCode"] = process.wait(timeout=5)
        assert result["exitCode"] == 0, result["exitCode"]
    finally:
        stop(process)
    measured = json.loads(marks.read_text(encoding="utf-8"))
    # 最初の読みだけを見る（frame_presented は描くたびに打たれる。eng/measure-speed.py と同じ立場）。
    readings: dict = {}
    for entry in measured["marks"]:
        readings.setdefault(entry["milestone"], int(entry["qpcMicroseconds"]))
    result["processCreationToWindowShownMs"] = round(
        float(measured["processCreationToOriginMs"]) + readings["window_shown"] / 1000.0, 3)
    result["windowShownToFirstFrameMs"] = round(
        (readings["frame_presented"] - readings["window_shown"]) / 1000.0, 3)
    result["expectedBackground"] = background
    result["capture"] = "first-paint.bmp"
    # ADR 0013 の却下の条件。ここが落ちたら、直す先はコードではなく ADR そのもの。
    assert not result["blackSeen"], f"black before the first frame: {result['blackSeen']}"
    assert not result["whiteSeen"], f"white before the first frame: {result['whiteSeen']}"
    return result


def ime_window(window) -> int:
    """The default IME window of the editor; its WM_IME_CONTROL answers for that input context."""
    return int(imm.ImmGetDefaultIMEWnd(window) or 0)


def ime_open(ime: int) -> int:
    return int(user.SendMessageW(ime, WM_IME_CONTROL, IMC_GETOPENSTATUS, 0))


def set_ime_open(ime: int, on: bool) -> None:
    user.SendMessageW(ime, WM_IME_CONTROL, IMC_SETOPENSTATUS, 1 if on else 0)


def colour_count(pixels: bytes, width: int, box: tuple, colour: tuple) -> int:
    """How many pixels inside a box are exactly one palette token (the underlines are opaque)."""
    return sum(1 for value in box_pixels(pixels, width, box) if value == list(colour))


def drive_ime_modes(window, ime: int, width: int, height: int, dpi: int) -> dict:
    """ADR 0014 decision 5: NORMAL turns the IME off, INSERT and the ordinary mode bring it back.

    The open status of the editor's own input context is readable from outside, so this needs no
    keyboard at all: the toggle and `i` and Esc all arrive as posted messages.
    """
    toggle = toggle_points(width, height, dpi)
    set_ime_open(ime, True)
    result = {"openAfterTurningItOn": ime_open(ime)}
    write_text(window, "a")
    time.sleep(0.5)
    result["openInOrdinaryMode"] = ime_open(ime)
    click(window, toggle["vim"][0], toggle["vim"][1])
    time.sleep(0.5)
    result["openInVimNormal"] = ime_open(ime)
    write_text(window, "i")
    time.sleep(0.5)
    result["openInVimInsert"] = ime_open(ime)
    press(window, VK_ESCAPE)
    time.sleep(0.5)
    result["openBackInVimNormal"] = ime_open(ime)
    click(window, toggle["ordinary"][0], toggle["ordinary"][1])
    time.sleep(0.5)
    result["openBackInOrdinaryMode"] = ime_open(ime)
    press(window, VK_BACK)
    time.sleep(0.3)
    return result


def drive_composition(window, ime: int, ground: dict, output: Path) -> dict:
    """Try a real composition: にほんご, Space to convert, Enter to commit (ADR 0014).

    Posted messages cannot reach the IME, so this is the one place that generates real keystrokes
    after taking the foreground. When the session refuses the foreground, or when the IME does not
    compose (another input mode, another IME), the run records that instead of failing; the manual
    check is written down in docs/quality/gate-proofs.md 5-h.
    """
    width, height, dpi, body = ground["size"]
    grounds = [ground["background"], ground["current"], list(ACCENT)]
    box = content_box(body, 0, dpi)
    set_ime_open(ime, True)
    if not send_keys(window, VK_NIHONGO):
        return {"foreground": False, "reason": "the session refused the foreground"}
    time.sleep(IME_SETTLE_SECONDS)
    composing = capture(window, width, height)
    write_bitmap(output / "ime-slice.bmp", composing, width, height)
    result = {
        "foreground": True,
        "inkWhileComposing": ink(composing, width, box, grounds),
        "imeUnderlinePixels": colour_count(composing, width, box, ground["ime"]),
        "accentPixelsWhileComposing": accent_count(composing, width, box),
        "capture": "ime-slice.bmp",
    }
    send_keys(window, [VK_SPACE])
    time.sleep(IME_SETTLE_SECONDS)
    converted = capture(window, width, height)
    write_bitmap(output / "ime-slice-converted.bmp", converted, width, height)
    result["inkAfterSpace"] = ink(converted, width, box, grounds)
    result["accentPixelsAfterSpace"] = accent_count(converted, width, box)
    result["captureAfterSpace"] = "ime-slice-converted.bmp"
    send_keys(window, [VK_RETURN])
    time.sleep(IME_SETTLE_SECONDS)
    committed = capture(window, width, height)
    write_bitmap(output / "ime-slice-committed.bmp", committed, width, height)
    result["inkAfterEnter"] = ink(committed, width, box, grounds)
    result["imeUnderlinePixelsAfterEnter"] = colour_count(committed, width, box, ground["ime"])
    result["captureAfterEnter"] = "ime-slice-committed.bmp"
    return result


def judge_composition(composed: dict) -> None:
    """Assert only what the IME actually produced; a session that never composed is recorded."""
    if not composed.get("foreground") or composed["inkWhileComposing"] == 0:
        composed["checked"] = "the IME did not compose in this session; see gate-proofs 5-h"
        return
    # 変換中は本文の行に ime の下線が出る（採用案 D15・決定 7）。
    assert composed["imeUnderlinePixels"] > 0, \
        f"no ime-coloured underline under the composed text: {composed}"
    # Space に対する IME の答えは一定しない: 変換に入って注目文節が accent になることも、
    # すでに出ている予測候補をそのまま確定することもある。前者だったときだけ注目文節を測る。
    converted = composed["accentPixelsAfterSpace"] > composed["accentPixelsWhileComposing"]
    composed["spaceOpenedATargetClause"] = converted
    # 確定すると本文に字が入り、下線は消える（本文は TextBuffer の側にある・決定 2）。
    assert composed["inkAfterEnter"] > 0, f"the committed text is not in the body: {composed}"
    assert composed["imeUnderlinePixelsAfterEnter"] == 0, \
        f"the underline survived the commit: {composed}"
    composed["checked"] = ("composed, converted and committed with real keystrokes" if converted
                           else "composed and committed with real keystrokes; Space took the "
                                "predicted candidate instead of opening a target clause")


def verify_ime(executable: Path, environment: dict, appearance: str, output: Path,
               points: float = BODY_DEFAULT_POINTS) -> dict:
    """Issue #28 / ADR 0014. A fresh editor, because this one drives the real keyboard."""
    thread_layout = user.GetKeyboardLayout(0)
    process, window, _ = start(executable, environment)
    try:
        assert user.SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE_NOSIZE_SHOW)
        time.sleep(0.4)
        client = rectangle(window, user.GetClientRect)
        width, height = client[2] - client[0], client[3] - client[1]
        dpi = user.GetDpiForWindow(window)
        thread = user.GetWindowThreadProcessId(window, None)
        language = user.GetKeyboardLayout(thread) & 0xFFFF
        ime = ime_window(window)
        result = {"keyboardLanguage": hex(language), "defaultImeWindow": bool(ime),
                  "verifierKeyboardLanguage": hex(thread_layout & 0xFFFF)}
        if not ime or language != JAPANESE_LANGUAGE:
            result["checked"] = "no Japanese IME on this machine; see gate-proofs 5-h"
            close(window)
            result["exitCode"] = process.wait(timeout=5)
            return result
        was_open = ime_open(ime)
        result["openStatusAsFound"] = was_open
        ground = {
            "size": (width, height, dpi, body_points(width, height, dpi, points)),
            "background": list(PALETTE[appearance]),
            "current": list(CURRENT_LINE[appearance]),
            "ime": IME_TOKEN[appearance],
        }
        try:
            result["modes"] = drive_ime_modes(window, ime, width, height, dpi)
            result["composition"] = drive_composition(window, ime, ground, output)
        finally:
            # この機械の IME は見つけたとおりに返す（ADR 0014 の実機の節の約束）。
            set_ime_open(ime, bool(was_open))
            result["openStatusRestored"] = ime_open(ime)
        modes = result["modes"]
        assert modes["openAfterTurningItOn"] == 1, "the IME could not be turned on from outside"
        assert modes["openInOrdinaryMode"] == 1, "the ordinary mode must not touch the IME"
        assert modes["openInVimNormal"] == 0, "Vim NORMAL must turn the IME off (decision 5)"
        assert modes["openInVimInsert"] == 1, "INSERT must bring the recorded status back"
        assert modes["openBackInVimNormal"] == 0, "Esc must turn the IME off again"
        assert modes["openBackInOrdinaryMode"] == 1, "leaving Vim must restore the status"
        assert result["openStatusRestored"] == was_open, "the machine kept the IME it was found in"
        judge_composition(result["composition"])
        close(window)
        # 本文を打ち替えたので未保存の確認が出る。破棄して閉じる。
        result["closeConfirmationDismissed"] = dismiss_dialog(process, IDNO)
        result["exitCode"] = process.wait(timeout=5)
        assert result["exitCode"] == 0, result["exitCode"]
    finally:
        stop(process)
    return result


def verify_documents(executable: Path, environment: dict, appearance: str, output: Path) -> dict:
    documents = write_documents(output)
    utf8 = verify_document(executable, environment, appearance, output,
                           {"path": documents["utf8"], "rows": 3, "save": True})
    shift_jis = verify_document(executable, environment, appearance, output,
                                {"path": documents["shiftJis"], "rows": 2, "save": False})
    # 同じ幅の枠に違う語が入る。UTF-8 と Shift_JIS、CRLF と LF が同じ画なら何かが固定されている。
    assert utf8["encodingInk"] != shift_jis["encodingInk"], "the encoding item never changed"
    assert utf8["endingInk"] != shift_jis["endingInk"], "the line ending item never changed"
    missing = verify_missing_document(executable, environment, output)
    return {"utf8": utf8, "shiftJis": shift_jis, "missing": missing}


def body_points(width: int, height: int, dpi: int, points: float = BODY_DEFAULT_POINTS) -> dict:
    """The body band, mirroring core::body_layout (src/core/BodyLayout.cpp)."""
    title = to_pixels(TITLE_BAR_DIPS, dpi)
    band_bottom = max(height - to_pixels(STATUS_BAR_DIPS, dpi), title)
    first = title + to_pixels(BODY_TOP_DIPS, dpi)
    line_height = max(to_pixels(int(points * 96 / 72 * 1.6 + 0.5), dpi), 1)
    return {
        "firstRowTop": first,
        "lineHeight": line_height,
        "gutter": min(to_pixels(int(BODY_GUTTER_DIPS * points / BODY_DEFAULT_POINTS + 0.5), dpi), width),
        "bandBottom": band_bottom,
        "visibleLines": max((band_bottom - first) // line_height, 0),
    }


def row_middle(body: dict, index: int) -> int:
    return body["firstRowTop"] + body["lineHeight"] * index + body["lineHeight"] // 2


def caret_pixel(pixels: bytes, width: int, body: dict, index: int) -> list[int]:
    """The leftmost pixel of the caret bar when the caret sits in column one."""
    return pixel(pixels, width, body["gutter"], row_middle(body, index))


def row_ground(pixels: bytes, width: int, body: dict, index: int) -> list[int]:
    """A pixel far to the right of any glyph, so it shows the ground of the row."""
    return pixel(pixels, width, width - 4, row_middle(body, index))


def ink(pixels: bytes, width: int, box: tuple, grounds: list) -> int:
    """How many pixels inside a box are neither ground nor caret; that is drawn ink."""
    left, top, right, bottom = box
    return sum(1 for y in range(top, bottom) for x in range(left, right)
               if pixel(pixels, width, x, y) not in grounds)


def gutter_box(body: dict, index: int, dpi: int) -> tuple:
    top = body["firstRowTop"] + body["lineHeight"] * index
    return (0, top, body["gutter"] - to_pixels(8, dpi), top + body["lineHeight"])


def content_box(body: dict, index: int, dpi: int) -> tuple:
    top = body["firstRowTop"] + body["lineHeight"] * index
    left = body["gutter"] + to_pixels(4, dpi)
    return (left, top, left + to_pixels(60, dpi), top + body["lineHeight"])


def mode_label_box(width: int, height: int, dpi: int) -> tuple:
    """The box that holds 通常 / NORMAL / INSERT, mirroring core::status_bar_layout."""
    band_top = max(height - to_pixels(STATUS_BAR_DIPS, dpi), 0)
    left = to_pixels(STATUS_PADDING_DIPS + TOGGLE_WIDTH_DIPS + STATUS_ITEM_GAP_DIPS, dpi)
    return (left, band_top, left + to_pixels(MODE_LABEL_WIDTH_DIPS, dpi), height)


def box_pixels(pixels: bytes, width: int, box: tuple) -> list:
    """Every pixel inside a box, so two captures of the same box can be compared exactly."""
    left, top, right, bottom = box
    return [pixel(pixels, width, x, y) for y in range(top, bottom) for x in range(left, right)]


def accent_count(pixels: bytes, width: int, box: tuple) -> int:
    """How many pixels in a box are the accent; a block caret fills far more of a cell than a bar.

    A single pixel cannot tell the two shapes apart once a glyph sits under the caret, because the
    glyph is painted on top of the block in the background colour (Issue #22).
    """
    return sum(1 for value in box_pixels(pixels, width, box) if value == list(ACCENT))


def first_cell_box(body: dict, index: int, dpi: int) -> tuple:
    """The cell the caret covers in column one of a row."""
    top = body["firstRowTop"] + body["lineHeight"] * index
    return (body["gutter"], top, body["gutter"] + to_pixels(10, dpi), top + body["lineHeight"])


def status_item_box(width: int, height: int, dpi: int, index: int) -> tuple:
    """The first status item (行 X, 桁 Y), mirroring core::status_bar_layout."""
    band_top = max(height - to_pixels(STATUS_BAR_DIPS, dpi), 0)
    right = width - to_pixels(STATUS_PADDING_DIPS, dpi)
    boxes = [None, None, None]
    for step in reversed(range(len(STATUS_ITEM_WIDTH_DIPS))):
        item = to_pixels(STATUS_ITEM_WIDTH_DIPS[step], dpi)
        boxes[step] = (right - item, band_top, right, height)
        right -= item + to_pixels(STATUS_ITEM_GAP_DIPS, dpi)
    return boxes[index]


def verify(window, appearance: str, output: Path) -> dict:
    assert user.SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE_NOSIZE_SHOW)
    time.sleep(0.4)
    window_bounds = rectangle(window, user.GetWindowRect)
    client = rectangle(window, user.GetClientRect)
    width, height = client[2] - client[0], client[3] - client[1]
    dpi = user.GetDpiForWindow(window)
    style = user.GetWindowLongPtrW(window, GWL_STYLE)
    title_bar = to_pixels(TITLE_BAR_DIPS, dpi)
    button = to_pixels(CAPTION_BUTTON_DIPS, dpi)
    toggle = toggle_points(width, height, dpi)
    tab = active_tab_point(dpi)
    pixels = capture(window, width, height)
    write_bitmap(output / "look-slice.bmp", pixels, width, height)
    expected = list(PALETTE[appearance])
    centre = pixel(pixels, width, width // 2, height // 2)
    # 帯の地は「タブでも ＋ でも窓の操作でもない場所」。下の hitTest の HTCAPTION がそれを示す。
    title_pixel = pixel(pixels, width, width // 2, title_bar // 2)
    tab_pixel = pixel(pixels, width, tab[0], tab[1])
    resting_vim = pixel(pixels, width, toggle["vimGround"][0], toggle["vimGround"][1])
    resting_ordinary = pixel(pixels, width, toggle["ordinaryGround"][0], toggle["ordinaryGround"][1])
    mica_expected = sys.getwindowsversion().build >= MICA_BUILD
    expected_band = list(TITLE_BAR[appearance])
    expected_tab = list(TAB_ACTIVE[appearance])
    hits = {"close": ask_hit(window, width - button // 2, title_bar // 2),
            "caption": ask_hit(window, width // 2, title_bar // 2)}
    assert centre == expected, f"centre pixel {centre} is not the {appearance} background {expected}"
    assert not (style & WS_POPUP), "the window must not be a WS_POPUP any more"
    assert style & WS_THICKFRAME, "WS_THICKFRAME must stay for Snap and the resize border"
    assert hits["close"] == HTCLOSE, f"the close button answered {hits['close']}"
    assert hits["caption"] == HTCAPTION, f"the empty title bar answered {hits['caption']}"
    assert resting_ordinary == list(ACCENT), f"the 通常 half starts on the accent: {resting_ordinary}"
    assert resting_vim == list(TOGGLE[appearance]), f"the resting Vim half is {resting_vim}"
    # D16: 帯は Mica の有無に関わらず不透明の title_bar、アクティブなタブは本文の地。
    assert title_pixel == expected_band, (f"the title bar band {title_pixel} is not the"
                                          f" {appearance} title_bar {expected_band}")
    assert tab_pixel == expected_tab, (f"the active tab {tab_pixel} is not the {appearance}"
                                       f" tab_active {expected_tab}")
    assert expected_tab == expected, "the active tab must carry the body ground (D16)"
    click(window, toggle["vim"][0], toggle["vim"][1])
    time.sleep(0.5)
    switched = capture(window, width, height)
    write_bitmap(output / "look-slice-vim.bmp", switched, width, height)
    vim_pixel = pixel(switched, width, toggle["vimGround"][0], toggle["vimGround"][1])
    ordinary_after = pixel(switched, width, toggle["ordinaryGround"][0],
                           toggle["ordinaryGround"][1])
    assert vim_pixel == list(ACCENT), f"the Vim half {vim_pixel} did not become the accent"
    assert ordinary_after == list(TOGGLE[appearance]), f"the 通常 half is still {ordinary_after}"
    # 意図は「どちらを選んだか」なので、同じ側をもう一度押しても状態は変わらない（ADR 0008 の決定 4）。
    click(window, toggle["vim"][0], toggle["vim"][1])
    time.sleep(0.5)
    repeated = capture(window, width, height)
    vim_repeat = pixel(repeated, width, toggle["vimGround"][0], toggle["vimGround"][1])
    assert vim_repeat == vim_pixel, f"selecting Vim twice changed the toggle to {vim_repeat}"
    return {
        "windowClass": WINDOW_CLASS,
        "windowRect": window_bounds,
        "clientRect": client,
        "clientSize": [width, height],
        "dpi": dpi,
        "expectedClientSize": [640 * dpi // 96, 360 * dpi // 96],
        "framelessOverlapped": bool(style & WS_THICKFRAME) and not bool(style & WS_POPUP),
        "osDrawnCaption": bool(style & WS_CAPTION),
        "visible": bool(style & WS_VISIBLE),
        "appearance": appearance,
        "expectedBackground": expected,
        "centrePixel": centre,
        "titleBarPixel": title_pixel,
        "expectedTitleBar": expected_band,
        "titleBarDiffersFromBody": title_pixel != centre,
        "tabPoint": tab,
        "activeTabPixel": tab_pixel,
        "expectedActiveTab": expected_tab,
        "micaExpected": mica_expected,
        "windowsBuild": sys.getwindowsversion().build,
        "hitTest": hits,
        "togglePoints": toggle,
        "toggleOrdinaryPixelBefore": resting_ordinary,
        "toggleVimPixelBefore": resting_vim,
        "toggleVimPixelAfterClick": vim_pixel,
        "toggleOrdinaryPixelAfterClick": ordinary_after,
        "toggleVimPixelAfterSecondClick": vim_repeat,
        "expectedAccent": list(ACCENT),
        "capture": "look-slice.bmp",
        "captureAfterToggle": "look-slice-vim.bmp",
    }


def verify_typing(window, ground: dict, output: Path) -> dict:
    """WM_CHAR then Enter: the caret and the current line move to the second row."""
    width, height, dpi, body = ground["size"]
    grounds = [ground["background"], ground["current"], list(ACCENT)]
    before = capture(window, width, height)
    status_before = ink(before, width, status_item_box(width, height, dpi, 0),
                        [ground["status"]])
    resting_caret = caret_pixel(before, width, body, 0)
    write_text(window, "abc")
    press(window, VK_RETURN)
    time.sleep(0.5)
    after = capture(window, width, height)
    write_bitmap(output / "editing-slice.bmp", after, width, height)
    typed = {
        "restingCaretPixel": resting_caret,
        "caretPixelRowTwo": caret_pixel(after, width, body, 1),
        "rowOneGround": row_ground(after, width, body, 0),
        "rowTwoGround": row_ground(after, width, body, 1),
        "rowOneInk": ink(after, width, content_box(body, 0, dpi), grounds),
        "gutterInkRowTwo": ink(after, width, gutter_box(body, 1, dpi), grounds),
        "statusInkBefore": status_before,
        "statusInkAfter": ink(after, width, status_item_box(width, height, dpi, 0),
                              [ground["status"]]),
        "capture": "editing-slice.bmp",
    }
    assert resting_caret == list(ACCENT), f"the resting caret is not the accent: {resting_caret}"
    assert typed["caretPixelRowTwo"] == list(ACCENT), "Enter did not put the caret on row two"
    assert typed["rowTwoGround"] == ground["current"], "the current line did not move to row two"
    assert typed["rowOneGround"] == ground["background"], "row one is still the current line"
    assert typed["rowOneInk"] > 0, "the typed characters were not drawn on row one"
    assert typed["gutterInkRowTwo"] > 0, "the line number 2 was not drawn"
    assert typed["statusInkAfter"] != status_before, "行 X, 桁 Y did not change"
    return typed


def verify_block_caret(window, ground: dict) -> dict:
    """The Vim toggle turns the bar caret into a block one character wide (採用案 第 1 節)."""
    width, height, dpi, body = ground["size"]
    toggle = toggle_points(width, height, dpi)
    inside_block = body["gutter"] + to_pixels(5, dpi)
    middle = row_middle(body, 1)
    bar = pixel(capture(window, width, height), width, inside_block, middle)
    click(window, toggle["vim"][0], toggle["vim"][1])
    time.sleep(0.5)
    block = pixel(capture(window, width, height), width, inside_block, middle)
    click(window, toggle["ordinary"][0], toggle["ordinary"][1])
    time.sleep(0.5)
    restored = pixel(capture(window, width, height), width, inside_block, middle)
    shapes = {"pixelBesideBarCaret": bar, "pixelInsideBlockCaret": block,
              "pixelAfterReturningToOrdinary": restored}
    assert bar != list(ACCENT), f"the ordinary caret is wider than a bar: {bar}"
    assert block == list(ACCENT), f"the Vim caret is not a block: {block}"
    assert restored == bar, "the caret did not go back to a bar"
    return shapes


def verify_escape(window, process, ground: dict) -> dict:
    """Esc drops a selection; it must not close the window any more (Issue #7)."""
    width, height, dpi, body = ground["size"]
    press(window, VK_ESCAPE)
    time.sleep(0.5)
    still_open = bool(user.IsWindow(window)) and process.poll() is None
    assert still_open, "Esc closed the window"
    after = capture(window, width, height)
    escaped = {"windowStillOpen": still_open,
               "caretPixelRowTwo": caret_pixel(after, width, body, 1)}
    assert escaped["caretPixelRowTwo"] == list(ACCENT), "Esc moved the caret"
    return escaped


def verify_click_caret(window, ground: dict) -> dict:
    """A posted click in the body puts the caret on the clicked line (no real pointer)."""
    width, height, dpi, body = ground["size"]
    left = body["gutter"]
    click(window, left, row_middle(body, 0))
    time.sleep(0.5)
    on_first = capture(window, width, height)
    # 行番号の欄のクリックは行頭に寄せる。2 行目は空なので桁 1 のキャレットがそのまま出る。
    click(window, to_pixels(6, dpi), row_middle(body, 1))
    time.sleep(0.5)
    on_second = capture(window, width, height)
    # 最後の行より下のクリックは最も近い（最後の）行へ寄せる。
    click(window, left, body["bandBottom"] - to_pixels(4, dpi))
    time.sleep(0.5)
    below = capture(window, width, height)
    placed = {
        "caretRowOneAfterClickingRowOne": caret_pixel(on_first, width, body, 0),
        "caretRowTwoAfterClickingRowOne": caret_pixel(on_first, width, body, 1),
        "caretRowTwoAfterClickingGutter": caret_pixel(on_second, width, body, 1),
        "caretRowOneAfterClickingGutter": caret_pixel(on_second, width, body, 0),
        "caretRowTwoAfterClickingBelow": caret_pixel(below, width, body, 1),
    }
    assert placed["caretRowOneAfterClickingRowOne"] == list(ACCENT), "the click missed row one"
    assert placed["caretRowTwoAfterClickingRowOne"] != list(ACCENT), "the caret stayed on row two"
    assert placed["caretRowTwoAfterClickingGutter"] == list(ACCENT), "the gutter click missed"
    assert placed["caretRowOneAfterClickingGutter"] != list(ACCENT), "the caret stayed on row one"
    assert placed["caretRowTwoAfterClickingBelow"] == list(ACCENT), "a click below the last line"
    return placed


def verify_backspace(window, ground: dict) -> dict:
    """Two Backspaces put the buffer back on one line."""
    width, height, dpi, body = ground["size"]
    grounds = [ground["background"], ground["current"], list(ACCENT)]
    press(window, VK_BACK, 2)
    time.sleep(0.5)
    after = capture(window, width, height)
    erased = {
        "rowOneGround": row_ground(after, width, body, 0),
        "rowTwoGround": row_ground(after, width, body, 1),
        "gutterInkRowTwo": ink(after, width, gutter_box(body, 1, dpi), grounds),
        "rowOneInk": ink(after, width, content_box(body, 0, dpi), grounds),
    }
    assert erased["rowOneGround"] == ground["current"], "the caret did not return to row one"
    assert erased["rowTwoGround"] == ground["background"], "row two is still painted"
    assert erased["gutterInkRowTwo"] == 0, "the line number 2 is still drawn"
    assert erased["rowOneInk"] > 0, "Backspace removed more than the two code points"
    return erased


def await_ink(window, size: tuple, box: tuple, grounds: list, wanted: bool,
              seconds: float = 8.0) -> tuple[bytes, int]:
    """Capture until a box has ink (or has none), or until the deadline says it never will.

    Drawing is coalesced into one WM_PAINT after the posted messages drain (ADR 0011 decision 6),
    so the picture arrives when the queue is empty rather than after a fixed sleep. Waiting for the
    expected state and then asserting it is the same check with a deadline instead of a guess.
    """
    width, height = size
    deadline = time.monotonic() + seconds
    while True:
        pixels = capture(window, width, height)
        amount = ink(pixels, width, box, grounds)
        if (amount > 0) == wanted or time.monotonic() > deadline:
            return pixels, amount
        time.sleep(0.1)


def await_accent(window, size: tuple, box: tuple, wanted: bool,
                 seconds: float = 8.0) -> tuple[bytes, int]:
    """Capture until a Vim block caret appears in a cell, or the deadline expires."""
    width, height = size
    deadline = time.monotonic() + seconds
    while True:
        pixels = capture(window, width, height)
        amount = accent_count(pixels, width, box)
        if (amount > 0) == wanted or time.monotonic() > deadline:
            return pixels, amount
        time.sleep(0.1)


def verify_scrolling(window, ground: dict, output: Path) -> dict:
    """200 Enters, then PgUp back to the top and PgDn away from it again."""
    width, height, dpi, body = ground["size"]
    grounds = [ground["background"], ground["current"], list(ACCENT)]
    size, box = (width, height), content_box(body, 0, dpi)
    press(window, VK_RETURN, TYPED_LINES)
    bottom, bottom_ink = await_ink(window, size, box, grounds, False)
    write_bitmap(output / "editing-slice-scrolled.bmp", bottom, width, height)
    press(window, VK_PRIOR, PAGE_KEYS)
    _, top_ink = await_ink(window, size, box, grounds, True)
    press(window, VK_NEXT)
    _, paged_ink = await_ink(window, size, box, grounds, False)
    scrolled = {
        "typedLines": TYPED_LINES,
        "visibleLines": body["visibleLines"],
        "rowOneInkAtBottom": bottom_ink,
        "rowOneInkAtTop": top_ink,
        "rowOneInkAfterPageDown": paged_ink,
        "capture": "editing-slice-scrolled.bmp",
    }
    assert scrolled["rowOneInkAtBottom"] == 0, "the first line is still visible after 200 Enters"
    assert scrolled["rowOneInkAtTop"] > 0, "PgUp did not bring the first line back"
    assert scrolled["rowOneInkAfterPageDown"] == 0, "PgDn did not scroll away from the first line"
    return scrolled


def verify_vim(window, ground: dict, output: Path) -> dict:
    """Issue #22: the toggle enters NORMAL, ihello<Esc> types hello, 0x leaves ello.

    Issue #43 adds the second slice: yyp yanks the line and puts it below, so the body grows a
    second row. Issue #53 adds the third: V names VISUAL LINE on the status bar and d takes that
    row away again. The buffer is empty when this runs and four undos put it back (the insert, the
    x, the put and the Vd are one unit each), so the editing checks that follow still start from an
    empty 無題 buffer. Vim needs no modifier for any of these keys.
    """
    width, height, dpi, body = ground["size"]
    grounds = [ground["background"], ground["current"], list(ACCENT)]
    toggle = toggle_points(width, height, dpi)
    label = mode_label_box(width, height, dpi)
    cell = first_cell_box(body, 0, dpi)
    size, content = (width, height), content_box(body, 0, dpi)
    click(window, toggle["vim"][0], toggle["vim"][1])
    time.sleep(0.5)
    normal = capture(window, width, height)
    write_text(window, "i")
    time.sleep(0.5)
    inserting = capture(window, width, height)
    write_text(window, "hello")
    typed, typed_ink = await_ink(window, size, content, grounds, True)
    write_bitmap(output / "vim-slice.bmp", typed, width, height)
    press(window, VK_ESCAPE)
    time.sleep(0.5)
    write_text(window, "0")
    time.sleep(0.5)
    back = capture(window, width, height)
    write_text(window, "x")
    time.sleep(0.5)
    shortened = capture(window, width, height)
    # Issue #43: yy puts the line in the unnamed register and p opens a second row below it.
    write_text(window, "yyp")
    second_row = content_box(body, 1, dpi)
    put, put_ink = await_ink(window, size, second_row, grounds, True)
    write_bitmap(output / "vim-put.bmp", put, width, height)
    # Issue #53: V は行を選んで VISUAL LINE を名乗り、d はその行をまるごと消す（本文が 1 行に戻る）。
    write_text(window, "V")
    time.sleep(0.5)
    visual = capture(window, width, height)
    write_text(window, "d")
    time.sleep(0.5)
    unput = capture(window, width, height)
    write_text(window, "uuuu")
    time.sleep(0.5)
    # ブロックのキャレットは本文の枠に掛かるので、字形が消えたことは通常モードに戻してから測る。
    click(window, toggle["ordinary"][0], toggle["ordinary"][1])
    ordinary, cleared_ink = await_ink(window, size, content, grounds, False)
    result = {
        "inkAfterTyping": typed_ink,
        "inkAfterRemoving": ink(shortened, width, content, grounds),
        "inkAfterUndoing": cleared_ink,
        "insertChangedTheModeLabel":
            box_pixels(inserting, width, label) != box_pixels(normal, width, label),
        "escapeBroughtTheNormalLabelBack":
            box_pixels(back, width, label) == box_pixels(normal, width, label),
        "ordinaryLabelDiffers":
            box_pixels(ordinary, width, label) != box_pixels(normal, width, label),
        "accentInTheInsertCell": accent_count(inserting, width, cell),
        "accentInTheNormalCell": accent_count(back, width, cell),
        "inkInTheSecondRowAfterPutting": put_ink,
        "inkInTheSecondRowBeforePutting": ink(shortened, width, second_row, grounds),
        "visualLineChangedTheModeLabel":
            box_pixels(visual, width, label) != box_pixels(normal, width, label),
        "inkInTheSecondRowAfterVisualDelete": ink(unput, width, second_row, grounds),
        "capture": "vim-slice.bmp",
        "capturePut": "vim-put.bmp",
    }
    assert result["inkAfterTyping"] > 0, "ihello did not draw anything"
    assert result["insertChangedTheModeLabel"], "the status bar did not say INSERT"
    assert result["escapeBroughtTheNormalLabelBack"], "Esc did not bring NORMAL back"
    assert result["ordinaryLabelDiffers"], "the ordinary label looks like NORMAL"
    assert result["accentInTheInsertCell"] > 0, "the INSERT caret was not drawn"
    assert result["accentInTheNormalCell"] > 2 * result["accentInTheInsertCell"], \
        "the NORMAL caret is not wider than the INSERT bar"
    assert 0 < result["inkAfterRemoving"] < result["inkAfterTyping"], "0x did not remove one glyph"
    assert result["inkInTheSecondRowBeforePutting"] == 0, "the buffer had a second row already"
    assert result["inkInTheSecondRowAfterPutting"] > 0, "yyp did not put a second line"
    assert result["visualLineChangedTheModeLabel"], "the status bar did not say VISUAL LINE"
    assert result["inkInTheSecondRowAfterVisualDelete"] == 0, "Vd did not take the line away"
    assert result["inkAfterUndoing"] == 0, f"four undos did not empty the line: {result}"
    return result


def verify_vim_viewport(window, ground: dict, output: Path) -> dict:
    """Issue #58: check Vim viewport motions through the real window input path."""
    width, height, dpi, body = ground["size"]
    grounds = [ground["background"], ground["current"], list(ACCENT)]
    size = (width, height)
    first_row = content_box(body, 0, dpi)
    first_row_probe = (body["gutter"] + to_pixels(16, dpi), first_row[1], first_row[2], first_row[3])
    marker = "VIEWPORT-TOP"
    blank_lines = max(body["visibleLines"] * 3, 3) + 1
    click(window, toggle_points(width, height, dpi)["vim"][0],
          toggle_points(width, height, dpi)["vim"][1])
    time.sleep(0.3)
    write_text(window, "i" + marker)
    press(window, VK_RETURN, blank_lines)
    write_text(window, "VIEWPORT-BOTTOM")
    press(window, VK_ESCAPE)

    def return_to_top() -> int:
        write_text(window, "999k0")
        _, ink_at_top = await_ink(window, size, first_row_probe, grounds, True)
        top_cell = first_cell_box(body, 0, dpi)
        shifted_cell = (top_cell[0] + to_pixels(3, dpi), top_cell[1], top_cell[2], top_cell[3])
        _, top_accent = await_accent(window, size, shifted_cell, True)
        assert top_accent > 0, "999k0 did not settle on a NORMAL block caret"
        return ink_at_top

    top_ink = return_to_top()

    def check_page_pair() -> dict:
        return_to_top()
        press(window, VK_NEXT)
        down_pixels, down = await_ink(window, size, first_row_probe, grounds, False)
        write_bitmap(output / "vim-viewport-page-down.bmp", down_pixels, width, height)
        press(window, VK_PRIOR)
        up_pixels, up = await_ink(window, size, first_row_probe, grounds, True)
        write_bitmap(output / "vim-viewport-page-up.bmp", up_pixels, width, height)
        return {"pageDownFirstRowInk": down, "pageUpFirstRowInk": up}

    def check_ctrl_pair(key_down: int, key_up: int, name: str) -> dict:
        return_to_top()
        moved = press_chord(window, VK_CONTROL, key_down)
        if not moved:
            return {"status": "unconfirmed", "reason": "foreground acquisition failed"}
        down_pixels, down = await_ink(window, size, first_row_probe, grounds, False)
        write_bitmap(output / f"vim-viewport-{name}-down.bmp", down_pixels, width, height)
        returned = press_chord(window, VK_CONTROL, key_up)
        if not returned:
            return {"status": "unconfirmed", "reason": "foreground acquisition failed after movement",
                    "movedFirstRowInk": down}
        up_pixels, up = await_ink(window, size, first_row_probe, grounds, True)
        write_bitmap(output / f"vim-viewport-{name}-up.bmp", up_pixels, width, height)
        return {"status": "confirmed", "movedFirstRowInk": down, "returnedFirstRowInk": up}

    return_to_top()
    write_text(window, "M")
    expected_middle = max(body["visibleLines"] - 1, 0) // 2
    middle_cell = first_cell_box(body, expected_middle, dpi)
    middle, m_accent = await_accent(window, size, middle_cell, True)
    write_bitmap(output / "vim-viewport-m.bmp", middle, width, height)
    write_text(window, "L")
    expected_lower = max(body["visibleLines"] - 1, 0)
    lower_cell = first_cell_box(body, expected_lower, dpi)
    lower, l_accent = await_accent(window, size, lower_cell, True)
    write_bitmap(output / "vim-viewport-l.bmp", lower, width, height)
    write_text(window, "H")
    h_cell = first_cell_box(body, 0, dpi)
    h, h_accent = await_accent(window, size, h_cell, True)
    write_bitmap(output / "vim-viewport-h.bmp", h, width, height)
    result = {
        "marker": marker,
        "visibleLines": body["visibleLines"],
        "topInk": top_ink,
        "hAccent": h_accent,
        "mAccent": m_accent,
        "lAccent": l_accent,
        "page": check_page_pair(),
        "ctrlD_U": check_ctrl_pair(ord("D"), ord("U"), "ctrl-d-u"),
        "ctrlF_B": check_ctrl_pair(ord("F"), ord("B"), "ctrl-f-b"),
    }
    (output / "vim-viewport-results.json").write_text(json.dumps(result, indent=2) + "\n",
                                                         encoding="utf-8")
    assert result["topInk"] > 0, "the viewport marker was not drawn at the top"
    assert result["hAccent"] > 0, "H did not place the caret on the first visible row"
    assert result["mAccent"] > 0, "M did not place the caret on the middle visible row"
    assert result["lAccent"] > 0, "L did not place the caret on the last visible row"
    assert result["page"]["pageDownFirstRowInk"] == 0, "PgDn left the top marker visible"
    assert result["page"]["pageUpFirstRowInk"] > 0, "PgUp did not return to the top marker"
    for name in ("ctrlD_U", "ctrlF_B"):
        if result[name]["status"] == "confirmed":
            assert result[name]["movedFirstRowInk"] == 0, f"{name} did not move from the top"
            assert result[name]["returnedFirstRowInk"] > 0, f"{name} did not return to the top"
    write_text(window, "u")
    _, empty_ink = await_ink(window, size, first_row_probe, grounds, False)
    result["emptyAfterUndoInk"] = empty_ink
    assert result["emptyAfterUndoInk"] == 0, "undo did not return the viewport body to empty"
    click(window, toggle_points(width, height, dpi)["ordinary"][0],
          toggle_points(width, height, dpi)["ordinary"][1])
    time.sleep(0.3)
    return result


def verify_editing(window, process, appearance: str, output: Path,
                   frames: Path | None = None) -> dict:
    """Drive the editing keys with posted messages and read the result back as pixels."""
    client = rectangle(window, user.GetClientRect)
    width, height = client[2] - client[0], client[3] - client[1]
    dpi = user.GetDpiForWindow(window)
    # 直前の検査は Vim モードで終わる。キャレットをバーに戻してから編集を測る（採用案 第 1 節）。
    toggle = toggle_points(width, height, dpi)
    click(window, toggle["ordinary"][0], toggle["ordinary"][1])
    time.sleep(0.5)
    ground = {
        "size": (width, height, dpi, body_points(width, height, dpi)),
        "background": list(PALETTE[appearance]),
        "current": list(CURRENT_LINE[appearance]),
        "status": list(STATUS_BAND[appearance]),
    }
    result = {"body": body_points(width, height, dpi)}
    # Vim の鍵は本文が空のうちに測り、u で空へ戻してから通常モードの編集を測る（Issue #22）。
    result["vim"] = verify_vim(window, ground, output)
    snapshot(frames, window, "vim")
    result["vimViewport"] = verify_vim_viewport(window, ground, output)
    snapshot(frames, window, "vimViewport")
    result["typing"] = verify_typing(window, ground, output)
    snapshot(frames, window, "typing")
    result["caretShapes"] = verify_block_caret(window, ground)
    snapshot(frames, window, "caretShapes")
    result["escape"] = verify_escape(window, process, ground)
    snapshot(frames, window, "escape")
    result["clickedCaret"] = verify_click_caret(window, ground)
    snapshot(frames, window, "clickedCaret")
    result["backspace"] = verify_backspace(window, ground)
    snapshot(frames, window, "backspace")
    result["scrolling"] = verify_scrolling(window, ground, output)
    snapshot(frames, window, "scrolling")
    # Ctrl の組み合わせは PostMessageW では作れない（GetKeyState は実キーだけを見る）。
    result["modifierKeysCovered"] = (
        "Vim viewport Ctrl-d/u/f/b uses press_chord; other modifier shortcuts are covered by "
        "unit tests (posted messages carry no modifier state)"
    )
    return result


def await_new_frame(window, before: bytes, size: tuple, seconds: float = 8.0) -> bytes:
    """Capture until the picture has changed and holds still for one more capture (await_ink's way).

    Drawing is one WM_PAINT after the posted messages drain, so a changed picture that repeats is
    the settled frame. Keys that change nothing hand back the unchanged picture at the deadline.
    """
    width, height = size
    deadline = time.monotonic() + seconds
    previous = before
    while True:
        time.sleep(0.1)
        pixels = capture(window, width, height)
        if (pixels != before and pixels == previous) or time.monotonic() > deadline:
            return pixels
        previous = pixels


def drive_keys(executable: Path, environment: dict, frames: Path, keys: str) -> dict:
    """--keys: before.png, the keys through the posted-message path, after.png, frames.json."""
    steps = parse_keys(keys)
    process, window, _ = start(executable, environment)
    try:
        raise_window(window)
        time.sleep(0.4)
        client = rectangle(window, user.GetClientRect)
        width, height = client[2] - client[0], client[3] - client[1]
        dpi = user.GetDpiForWindow(window)
        body = body_points(width, height, dpi)
        top = to_pixels(TITLE_BAR_DIPS, dpi)
        before = capture(window, width, height)
        write_png(frames / "before.png", width, height, before)
        send_key_sequence(window, keys)
        after = await_new_frame(window, before, (width, height))
        write_png(frames / "after.png", width, height, after)
        band_top = max(height - to_pixels(STATUS_BAR_DIPS, dpi), 0)  # core::status_bar_layout と同じ
        body_box = {"x": 0, "y": top, "w": width, "h": body["bandBottom"] - top}
        # 期待してよい差分の領域（Issue #131 の訂正）。物理画素・クライアント座標で、重ならない。
        regions = {"title": {"x": 0, "y": 0, "w": width, "h": top}, "body": body_box,
                   "status": {"x": 0, "y": band_top, "w": width, "h": height - band_top}}
        record = {"before": "before.png", "after": "after.png", "keys": keys, "steps": len(steps),
                  "body": body_box, "regions": regions,
                  "dpi": dpi, "size": {"w": width, "h": height}}
        (frames / "frames.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
        close(window)
        # 鍵が本文を変えていれば未保存の確認が出る。変えていなければ何も出ずにそのまま閉じる。
        dismiss_dialog(process, IDNO)
        process.wait(timeout=5)
        return record
    finally:
        stop(process)


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, default=root / "build/NeNeNib.exe")
    parser.add_argument("--capture", type=Path, default=None,
                        help="save a PNG of the client area after each section into this directory")
    parser.add_argument("--keys", default=None,
                        help="with --capture: skip the sections, save before.png, send these keys "
                             "(fixture notation, e.g. ihello<Esc>), save after.png and frames.json")
    arguments = parser.parse_args()
    if arguments.keys is not None and arguments.capture is None:
        parser.error("--keys needs --capture <dir>")
    frames = arguments.capture.resolve() if arguments.capture is not None else None
    if frames is not None:
        frames.mkdir(parents=True, exist_ok=True)
    output = root / "out/window-verification"
    output.mkdir(parents=True, exist_ok=True)
    executable = output / "NeNeNib.exe"
    shutil.copy2(arguments.executable, executable)
    isolated = Path(tempfile.mkdtemp(prefix="profile-", dir=output)).resolve()
    assert isolated.is_relative_to(output.resolve())
    environment = dict(os.environ, LOCALAPPDATA=str(isolated), APPDATA=str(isolated))
    become_dpi_aware()
    if arguments.keys is not None:
        print(json.dumps(drive_keys(executable, environment, frames, arguments.keys), indent=2))
        return
    process, window, first_rect = start(executable, environment)
    try:
        appearance = expected_appearance()
        result = verify(window, appearance, output)
        snapshot(frames, window, "look")
        result["editing"] = verify_editing(window, process, appearance, output, frames)
        result["firstSeenWindowRect"] = first_rect
        result["shownAfterPlacement"] = first_rect[:2] != [0, 0]
        assert result["shownAfterPlacement"], f"the window was shown at {first_rect[:2]}"
        close(window)
        # この検査は本文を打ち替えたあとなので、閉じるときは未保存の確認が出る（ADR 0010 の決定 10）。
        result["closeConfirmationDismissed"] = dismiss_dialog(process, IDNO)
        assert result["closeConfirmationDismissed"], "WM_CLOSE did not ask about the unsaved body"
        result["closeExitCode"] = process.wait(timeout=5)
        assert result["closeExitCode"] == 0, result["closeExitCode"]
    finally:
        stop(process)
    result["documents"] = verify_documents(executable, environment, appearance, output)
    # ADR 0014: IME の開閉は外から読め、変換そのものは本物の鍵が要るので別の 1 回の起動で測る。
    result["ime"] = verify_ime(executable, environment, appearance, output)
    # ADR 0013 の却下の条件は、窓が見えてから最初のフレームまでの面。別の 1 回の起動で記録する。
    result["firstPaint"] = verify_first_paint(executable, environment, appearance, output)
    (output / "look-slice-results.json").write_text(json.dumps(result, indent=2) + "\n",
                                                    encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
