"""Interactive Windows verification of the window slices, separate from the unit tests (QLT-013).

Starts build/NeNeNib.exe with an isolated environment, finds its window by class name, records the
geometry and DPI, asks the window procedure where the caption and the close button are, reads the
composed pixels back from the screen device context, drives the mode toggle and the editing keys
with posted messages, and closes the window with WM_CLOSE. It never moves the real pointer and
never generates keyboard input. Requires an unlocked interactive Windows session with DWM running;
eng/check.ps1 and CI do not call it, because a gate must not need a display (QLT-013).

Posted messages cannot carry modifier state: GetKeyState only follows keys that really went through
the raw input queue, so Ctrl+A / C / X / V / Z / Y cannot be driven from here without touching the
real keyboard. Those live in the unit tests (Issue #7); this script drives WM_CHAR, Enter,
Backspace, Esc, PgUp, PgDn and a posted click in the body, which need no modifier.
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
import subprocess
import sys
import tempfile
import time
import winreg

user = c.WinDLL("user32", use_last_error=True)
gdi = c.WinDLL("gdi32", use_last_error=True)


def api(dll, name, result, *arguments):
    function = getattr(dll, name)
    function.restype, function.argtypes = result, arguments
    return function


class BITMAPINFOHEADER(c.Structure):
    _fields_ = [
        ("biSize", w.DWORD), ("biWidth", w.LONG), ("biHeight", w.LONG),
        ("biPlanes", w.WORD), ("biBitCount", w.WORD), ("biCompression", w.DWORD),
        ("biSizeImage", w.DWORD), ("biXPelsPerMeter", w.LONG), ("biYPelsPerMeter", w.LONG),
        ("biClrUsed", w.DWORD), ("biClrImportant", w.DWORD),
    ]


class BITMAPINFO(c.Structure):
    _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", w.DWORD * 3)]


api(user, "SetProcessDpiAwarenessContext", w.BOOL, w.HANDLE)
api(user, "FindWindowW", w.HWND, w.LPCWSTR, w.LPCWSTR)
api(user, "GetWindowThreadProcessId", w.DWORD, w.HWND, c.POINTER(w.DWORD))
api(user, "IsWindowVisible", w.BOOL, w.HWND)
api(user, "IsWindow", w.BOOL, w.HWND)
api(user, "GetWindowRect", w.BOOL, w.HWND, c.POINTER(w.RECT))
api(user, "GetClientRect", w.BOOL, w.HWND, c.POINTER(w.RECT))
api(user, "GetWindowLongPtrW", c.c_ssize_t, w.HWND, c.c_int)
api(user, "GetDpiForWindow", w.UINT, w.HWND)
api(user, "ClientToScreen", w.BOOL, w.HWND, c.POINTER(w.POINT))
api(user, "SetWindowPos", w.BOOL, w.HWND, w.HWND, c.c_int, c.c_int, c.c_int, c.c_int, w.UINT)
api(user, "PostMessageW", w.BOOL, w.HWND, w.UINT, w.WPARAM, w.LPARAM)
api(user, "SendMessageW", w.LPARAM, w.HWND, w.UINT, w.WPARAM, w.LPARAM)
api(user, "GetDC", w.HDC, w.HWND)
api(user, "ReleaseDC", c.c_int, w.HWND, w.HDC)
api(gdi, "CreateCompatibleDC", w.HDC, w.HDC)
api(gdi, "CreateCompatibleBitmap", w.HBITMAP, w.HDC, c.c_int, c.c_int)
api(gdi, "SelectObject", w.HGDIOBJ, w.HDC, w.HGDIOBJ)
api(gdi, "DeleteObject", w.BOOL, w.HGDIOBJ)
api(gdi, "DeleteDC", w.BOOL, w.HDC)
api(gdi, "BitBlt", w.BOOL, w.HDC, c.c_int, c.c_int, c.c_int, c.c_int, w.HDC, c.c_int, c.c_int, w.DWORD)
api(gdi, "GetDIBits", c.c_int, w.HDC, w.HBITMAP, w.UINT, w.UINT, w.LPVOID, c.POINTER(BITMAPINFO), w.UINT)
api(gdi, "GdiFlush", w.BOOL)

WINDOW_CLASS = "NeNeNib.Editor"
WM_CLOSE = 0x0010
WM_NCHITTEST = 0x0084
WM_KEYDOWN = 0x0100
WM_CHAR = 0x0102
WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
VK_RETURN = 0x0D
VK_BACK = 0x08
VK_ESCAPE = 0x1B
VK_PRIOR = 0x21
VK_NEXT = 0x22
HTCAPTION = 2
HTCLOSE = 20
SRCCOPY = 0x00CC0020
HWND_TOPMOST = c.c_void_p(-1)
SWP_NOMOVE_NOSIZE_SHOW = 0x0003 | 0x0040
GWL_STYLE = -16
WS_POPUP = 0x80000000
WS_VISIBLE = 0x10000000
WS_THICKFRAME = 0x00040000
WS_CAPTION = 0x00C00000
# Mica（DWMWA_SYSTEMBACKDROP_TYPE）は Windows 11 22H2 以降でだけ掛かる（ADR 0008）。
MICA_BUILD = 22621
# src/core/BuiltinTheme.hpp の正本と同じ値。ここが食い違ったら、どちらかが間違っている。
PALETTE = {"light": (0xF4, 0xF5, 0xF7), "dark": (0x30, 0x0A, 0x24)}
ACCENT = (0xE9, 0x54, 0x20)
TOGGLE = {"light": (0xDF, 0xE3, 0xE8), "dark": (0x4A, 0x1E, 0x3D)}
CURRENT_LINE = {"light": (0xE6, 0xE8, 0xEC), "dark": (0x3E, 0x1A, 0x32)}
STATUS_BAND = {"light": (0xE9, 0xEB, 0xEF), "dark": (0x26, 0x07, 0x1D)}
# src/core/TitleBarLayout.cpp / StatusBarLayout.cpp の DIP。同じ整数丸めで物理画素へ直す。
TITLE_BAR_DIPS = 40
CAPTION_BUTTON_DIPS = 46
STATUS_BAR_DIPS = 28
STATUS_PADDING_DIPS = 12
TOGGLE_PADDING_DIPS = 2
SEGMENT_WIDTH_DIPS = 44
SEGMENT_HEIGHT_DIPS = 20
STATUS_ITEM_GAP_DIPS = 16
STATUS_ITEM_WIDTH_DIPS = (96, 44, 36)
# src/core/BodyLayout.cpp の DIP。行の帯とキャレットの位置はここから同じ整数丸めで出す。
BODY_TOP_DIPS = 12
BODY_LINE_HEIGHT_DIPS = 24
BODY_GUTTER_DIPS = 56
TYPED_LINES = 200
PAGE_KEYS = 30


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


def rectangle(window, getter) -> list[int]:
    bounds = w.RECT()
    assert getter(window, c.byref(bounds))
    return [bounds.left, bounds.top, bounds.right, bounds.bottom]


def start(executable: Path, environment: dict) -> tuple[subprocess.Popen, int, list[int]]:
    """Return the process, its window, and the window rectangle as first seen (within 0.2 s)."""
    process = subprocess.Popen([str(executable)], env=environment)
    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline:
        window = user.FindWindowW(WINDOW_CLASS, None)
        if window and user.IsWindowVisible(window):
            owner = w.DWORD()
            user.GetWindowThreadProcessId(window, c.byref(owner))
            if owner.value == process.pid:
                return process, window, rectangle(window, user.GetWindowRect)
        if process.poll() is not None:
            raise AssertionError(f"NeNeNib exited before showing a window: {process.returncode}")
        time.sleep(0.05)
    process.terminate()
    process.wait(timeout=5)
    raise AssertionError("NeNeNib did not create its window within 5 seconds")


def capture(window, width: int, height: int) -> bytes:
    """Read the composed client area back from the screen device context (top-down BGRA)."""
    origin = w.POINT(0, 0)
    assert user.ClientToScreen(window, c.byref(origin))
    screen = user.GetDC(None)
    memory = gdi.CreateCompatibleDC(screen)
    bitmap = gdi.CreateCompatibleBitmap(screen, width, height)
    previous = gdi.SelectObject(memory, bitmap)
    try:
        assert gdi.BitBlt(memory, 0, 0, width, height, screen, origin.x, origin.y, SRCCOPY)
        assert gdi.GdiFlush()
        info = BITMAPINFO()
        info.bmiHeader.biSize = c.sizeof(BITMAPINFOHEADER)
        info.bmiHeader.biWidth = width
        info.bmiHeader.biHeight = -height
        info.bmiHeader.biPlanes = 1
        info.bmiHeader.biBitCount = 32
        info.bmiHeader.biCompression = 0
        pixels = (c.c_ubyte * (width * height * 4))()
        assert gdi.GetDIBits(memory, bitmap, 0, height, pixels, c.byref(info), 0) == height
        return bytes(pixels)
    finally:
        gdi.SelectObject(memory, previous)
        gdi.DeleteObject(bitmap)
        gdi.DeleteDC(memory)
        user.ReleaseDC(None, screen)


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


def ask_hit(window, x: int, y: int) -> int:
    """Ask the window procedure what is at a client point, without moving the real pointer."""
    point = w.POINT(x, y)
    assert user.ClientToScreen(window, c.byref(point))
    return int(user.SendMessageW(window, WM_NCHITTEST, 0, (point.y << 16) | (point.x & 0xFFFF)))


def click(window, x: int, y: int) -> None:
    """Post a left click at a client point; the real pointer is never touched."""
    packed = (y << 16) | (x & 0xFFFF)
    assert user.PostMessageW(window, WM_LBUTTONDOWN, 1, packed)
    assert user.PostMessageW(window, WM_LBUTTONUP, 0, packed)


def press(window, key: int, times: int = 1) -> None:
    """Post WM_KEYDOWN; no modifier state is involved, so no real key is ever pressed."""
    for _ in range(times):
        assert user.PostMessageW(window, WM_KEYDOWN, key, 1)


def write_text(window, text: str) -> None:
    """Post WM_CHAR for each UTF-16 unit, the way TranslateMessage would."""
    for character in text:
        assert user.PostMessageW(window, WM_CHAR, ord(character), 1)


def body_points(width: int, height: int, dpi: int) -> dict:
    """The body band, mirroring core::body_layout (src/core/BodyLayout.cpp)."""
    title = to_pixels(TITLE_BAR_DIPS, dpi)
    band_bottom = max(height - to_pixels(STATUS_BAR_DIPS, dpi), title)
    first = title + to_pixels(BODY_TOP_DIPS, dpi)
    line_height = max(to_pixels(BODY_LINE_HEIGHT_DIPS, dpi), 1)
    return {
        "firstRowTop": first,
        "lineHeight": line_height,
        "gutter": to_pixels(BODY_GUTTER_DIPS, dpi),
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
    pixels = capture(window, width, height)
    write_bitmap(output / "look-slice.bmp", pixels, width, height)
    expected = list(PALETTE[appearance])
    centre = pixel(pixels, width, width // 2, height // 2)
    title_pixel = pixel(pixels, width, width // 2, title_bar // 2)
    resting_vim = pixel(pixels, width, toggle["vimGround"][0], toggle["vimGround"][1])
    resting_ordinary = pixel(pixels, width, toggle["ordinaryGround"][0], toggle["ordinaryGround"][1])
    mica_expected = sys.getwindowsversion().build >= MICA_BUILD
    hits = {"close": ask_hit(window, width - button // 2, title_bar // 2),
            "caption": ask_hit(window, width // 2, title_bar // 2)}
    assert centre == expected, f"centre pixel {centre} is not the {appearance} background {expected}"
    assert not (style & WS_POPUP), "the window must not be a WS_POPUP any more"
    assert style & WS_THICKFRAME, "WS_THICKFRAME must stay for Snap and the resize border"
    assert hits["close"] == HTCLOSE, f"the close button answered {hits['close']}"
    assert hits["caption"] == HTCAPTION, f"the empty title bar answered {hits['caption']}"
    assert resting_ordinary == list(ACCENT), f"the 通常 half starts on the accent: {resting_ordinary}"
    assert resting_vim == list(TOGGLE[appearance]), f"the resting Vim half is {resting_vim}"
    if mica_expected:
        assert title_pixel != centre, "the title bar is not showing a backdrop behind the tint"
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
        "titleBarDiffersFromBody": title_pixel != centre,
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


def verify_scrolling(window, ground: dict, output: Path) -> dict:
    """200 Enters, then PgUp back to the top and PgDn away from it again."""
    width, height, dpi, body = ground["size"]
    grounds = [ground["background"], ground["current"], list(ACCENT)]
    press(window, VK_RETURN, TYPED_LINES)
    time.sleep(6.0)
    bottom = capture(window, width, height)
    write_bitmap(output / "editing-slice-scrolled.bmp", bottom, width, height)
    press(window, VK_PRIOR, PAGE_KEYS)
    time.sleep(2.5)
    top = capture(window, width, height)
    press(window, VK_NEXT)
    time.sleep(0.5)
    paged = capture(window, width, height)
    scrolled = {
        "typedLines": TYPED_LINES,
        "visibleLines": body["visibleLines"],
        "rowOneInkAtBottom": ink(bottom, width, content_box(body, 0, dpi), grounds),
        "rowOneInkAtTop": ink(top, width, content_box(body, 0, dpi), grounds),
        "rowOneInkAfterPageDown": ink(paged, width, content_box(body, 0, dpi), grounds),
        "capture": "editing-slice-scrolled.bmp",
    }
    assert scrolled["rowOneInkAtBottom"] == 0, "the first line is still visible after 200 Enters"
    assert scrolled["rowOneInkAtTop"] > 0, "PgUp did not bring the first line back"
    assert scrolled["rowOneInkAfterPageDown"] == 0, "PgDn did not scroll away from the first line"
    return scrolled


def verify_editing(window, process, appearance: str, output: Path) -> dict:
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
    result["typing"] = verify_typing(window, ground, output)
    result["caretShapes"] = verify_block_caret(window, ground)
    result["escape"] = verify_escape(window, process, ground)
    result["clickedCaret"] = verify_click_caret(window, ground)
    result["backspace"] = verify_backspace(window, ground)
    result["scrolling"] = verify_scrolling(window, ground, output)
    # Ctrl の組み合わせは PostMessageW では作れない（GetKeyState は実キーだけを見る）。
    result["modifierKeysCovered"] = "unit tests (posted messages carry no modifier state)"
    return result


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, default=root / "build/NeNeNib.exe")
    arguments = parser.parse_args()
    output = root / "out/window-verification"
    output.mkdir(parents=True, exist_ok=True)
    executable = output / "NeNeNib.exe"
    shutil.copy2(arguments.executable, executable)
    isolated = Path(tempfile.mkdtemp(prefix="profile-", dir=output)).resolve()
    assert isolated.is_relative_to(output.resolve())
    environment = dict(os.environ, LOCALAPPDATA=str(isolated), APPDATA=str(isolated))
    assert user.SetProcessDpiAwarenessContext(c.c_void_p(-4))
    process, window, first_rect = start(executable, environment)
    try:
        appearance = expected_appearance()
        result = verify(window, appearance, output)
        result["editing"] = verify_editing(window, process, appearance, output)
        result["firstSeenWindowRect"] = first_rect
        result["shownAfterPlacement"] = first_rect[:2] != [0, 0]
        assert result["shownAfterPlacement"], f"the window was shown at {first_rect[:2]}"
        assert user.PostMessageW(window, WM_CLOSE, 0, 0)
        result["closeExitCode"] = process.wait(timeout=5)
        assert result["closeExitCode"] == 0, result["closeExitCode"]
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
    (output / "look-slice-results.json").write_text(json.dumps(result, indent=2) + "\n",
                                                    encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
