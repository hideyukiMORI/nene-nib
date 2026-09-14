"""Interactive Windows verification of the window slices, separate from the unit tests (QLT-013).

Starts build/NeNeNib.exe with an isolated environment, finds its window by class name, records the
geometry and DPI, asks the window procedure where the caption and the close button are, reads the
composed pixels back from the screen device context, drives the mode toggle with posted messages,
and closes the window with WM_CLOSE. It never moves the real pointer and never generates keyboard
input. Requires an unlocked interactive Windows session with DWM running; eng/check.ps1 and CI do
not call it, because a gate must not need a display (QLT-013).
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
WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
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
# src/core/TitleBarLayout.cpp / StatusBarLayout.cpp の DIP。同じ整数丸めで物理画素へ直す。
TITLE_BAR_DIPS = 40
CAPTION_BUTTON_DIPS = 46
STATUS_BAR_DIPS = 28
STATUS_PADDING_DIPS = 12
TOGGLE_PADDING_DIPS = 2
SEGMENT_WIDTH_DIPS = 44
SEGMENT_HEIGHT_DIPS = 20


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
        result = verify(window, expected_appearance(), output)
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
