"""Drive the NeNe Nib window from outside: start it, find it, post to it, close it (ADR 0011).

There is one driver for every script that needs a running editor, so eng/verify-window.py (which
reads pixels back) and eng/measure-speed.py (which reads the milestone file back) share the same
start / find / PostMessageW / dismiss / close path instead of growing a second one (ARC-001 /
ARC-012). The one pixel read-back (capture: the composed client area from the screen device
context, and its PNG form for eng/compare-frames.py, Issue #131) lives here too, so there is one
way to take a picture; nothing here judges a result, that belongs to the caller.

The real pointer is never moved and no real keyboard input is generated, with one exception:
press_chord uses SendInput, because posted messages cannot carry modifier state (GetKeyState only
follows keys that went through the raw input queue). Taking the foreground is refused often enough
on a busy desktop that it is retried a few times before the caller is told it failed.

Python standard library and ctypes only.
"""

from __future__ import annotations

import ctypes as c
from ctypes import wintypes as w
from pathlib import Path
import struct
import subprocess
import time
import zlib

user = c.WinDLL("user32", use_last_error=True)
kernel = c.WinDLL("kernel32", use_last_error=True)
gdi = c.WinDLL("gdi32", use_last_error=True)


def api(dll, name, result, *arguments):
    function = getattr(dll, name)
    function.restype, function.argtypes = result, arguments
    return function


class KEYBDINPUT(c.Structure):
    _fields_ = [
        ("wVk", w.WORD), ("wScan", w.WORD), ("dwFlags", w.DWORD),
        ("dwTime", w.DWORD), ("dwExtraInfo", c.POINTER(w.ULONG)),
    ]


class INPUTPAYLOAD(c.Union):
    _fields_ = [("ki", KEYBDINPUT), ("padding", c.c_ubyte * 32)]


class INPUT(c.Structure):
    _fields_ = [("kind", w.DWORD), ("payload", INPUTPAYLOAD)]


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
api(user, "FindWindowExW", w.HWND, w.HWND, w.HWND, w.LPCWSTR, w.LPCWSTR)
api(user, "GetWindowThreadProcessId", w.DWORD, w.HWND, c.POINTER(w.DWORD))
api(user, "IsWindowVisible", w.BOOL, w.HWND)
api(user, "IsWindow", w.BOOL, w.HWND)
api(user, "GetWindowRect", w.BOOL, w.HWND, c.POINTER(w.RECT))
api(user, "GetClientRect", w.BOOL, w.HWND, c.POINTER(w.RECT))
# 窓の面が本当に見えているかを確かめる（別の窓が覆っていたら、測るのはその窓の面になる）。
api(user, "WindowFromPoint", w.HWND, w.POINT)
api(user, "GetWindowLongPtrW", c.c_ssize_t, w.HWND, c.c_int)
api(user, "GetDpiForWindow", w.UINT, w.HWND)
api(user, "GetDpiForSystem", w.UINT)
api(user, "ClientToScreen", w.BOOL, w.HWND, c.POINTER(w.POINT))
api(user, "SetWindowPos", w.BOOL, w.HWND, w.HWND, c.c_int, c.c_int, c.c_int, c.c_int, w.UINT)
api(user, "PostMessageW", w.BOOL, w.HWND, w.UINT, w.WPARAM, w.LPARAM)
api(user, "SendMessageW", w.LPARAM, w.HWND, w.UINT, w.WPARAM, w.LPARAM)
api(user, "GetWindowTextW", c.c_int, w.HWND, w.LPWSTR, c.c_int)
api(user, "SetForegroundWindow", w.BOOL, w.HWND)
api(user, "GetForegroundWindow", w.HWND)
api(user, "SendInput", w.UINT, w.UINT, c.c_void_p, c.c_int)
api(user, "AttachThreadInput", w.BOOL, w.DWORD, w.DWORD, w.BOOL)
api(user, "BringWindowToTop", w.BOOL, w.HWND)
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
api(kernel, "GetCurrentThreadId", w.DWORD)
api(kernel, "OpenThread", w.HANDLE, w.DWORD, w.BOOL, w.DWORD)
api(kernel, "SuspendThread", w.DWORD, w.HANDLE)
api(kernel, "ResumeThread", w.DWORD, w.HANDLE)
api(kernel, "CloseHandle", w.BOOL, w.HANDLE)

WINDOW_CLASS = "NeNeNib.Editor"
DIALOG_CLASS = "#32770"
WM_CLOSE = 0x0010
WM_COMMAND = 0x0111
IDNO = 7
WM_NCHITTEST = 0x0084
WM_KEYDOWN = 0x0100
WM_CHAR = 0x0102
WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
VK_RETURN = 0x0D
VK_BACK = 0x08
VK_TAB = 0x09
VK_V = 0x56
VK_ESCAPE = 0x1B
VK_PRIOR = 0x21
VK_NEXT = 0x22
VK_CONTROL = 0x11
VK_S = 0x53
VK_SPACE = 0x20
# ローマ字で「にほんご」を打つ鍵（ADR 0014 の実機の節）。仮想キーは英字の大文字の符号。
VK_NIHONGO = [0x4E, 0x49, 0x48, 0x4F, 0x4E, 0x47, 0x4F]
INPUT_KEYBOARD = 1
KEYEVENTF_KEYUP = 0x0002
SRCCOPY = 0x00CC0020
# PNG の署名（RFC 2083）。write_png / read_png が書いて読むのは 8-bit RGB・filter 0・非 interlace だけ。
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
PNG_RGB = 2
# 鍵の列の記法は tests/vim の fixture と同じ（<…> 以外の字は WM_CHAR）。読めない <…> は ValueError。
KEY_NAMES = {"<CR>": ("key", VK_RETURN), "<Esc>": ("key", VK_ESCAPE), "<BS>": ("key", VK_BACK),
             "<Tab>": ("key", VK_TAB), "<C-v>": ("chord", VK_V)}
HTCAPTION = 2
HTCLOSE = 20
HWND_TOPMOST = c.c_void_p(-1)
SWP_NOMOVE_NOSIZE_SHOW = 0x0003 | 0x0040
GWL_STYLE = -16
WS_POPUP = 0x80000000
WS_VISIBLE = 0x10000000
WS_THICKFRAME = 0x00040000
WS_CAPTION = 0x00C00000
DPI_AWARENESS_PER_MONITOR_V2 = c.c_void_p(-4)
THREAD_SUSPEND_RESUME = 0x0002
SUSPEND_FAILED = 0xFFFFFFFF
# 前景を取り損ねるのは珍しくない（他のプロセスが入力を握っている間は OS が断る）ので数回試す。
FOREGROUND_ATTEMPTS = 3


class WindowUnavailable(RuntimeError):
    """The editor did not put a window on this desktop; the caller decides whether that is fatal."""


def become_dpi_aware() -> None:
    """Per-monitor v2, so client sizes and DPI read back the way the editor sees them."""
    assert user.SetProcessDpiAwarenessContext(DPI_AWARENESS_PER_MONITOR_V2)


def rectangle(window, getter) -> list[int]:
    bounds = w.RECT()
    assert getter(window, c.byref(bounds))
    return [bounds.left, bounds.top, bounds.right, bounds.bottom]


def start(executable: Path, environment: dict, arguments: list[str] | None = None,
          seconds: float = 5.0) -> tuple[subprocess.Popen, int, list[int]]:
    """Return the process, its window, and the window rectangle as first seen."""
    process = subprocess.Popen([str(executable), *(arguments or [])], env=environment)
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        window = user.FindWindowW(WINDOW_CLASS, None)
        if window and user.IsWindowVisible(window):
            owner = w.DWORD()
            user.GetWindowThreadProcessId(window, c.byref(owner))
            if owner.value == process.pid:
                return process, window, rectangle(window, user.GetWindowRect)
        if process.poll() is not None:
            raise WindowUnavailable(f"NeNeNib exited before showing a window: {process.returncode}")
        time.sleep(0.05)
    stop(process)
    raise WindowUnavailable(f"NeNeNib did not create its window within {seconds} seconds")


def stop(process) -> None:
    """Kill a process that is still running; a run that ends here wrote no measurement file."""
    if process.poll() is None:
        process.terminate()
        process.wait(timeout=5)


def raise_window(window) -> None:
    """Put the window on top, so DXGI is not presenting into an occluded swap chain.

    An occluded window is throttled by the compositor, which shows up as whole vsyncs of jitter in
    anything that measures Present. Raising it is how eng/verify-window.py reads pixels back too.
    """
    assert user.SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE_NOSIZE_SHOW)


def close(window) -> None:
    """Ask the window to close the way the caption button does."""
    assert user.PostMessageW(window, WM_CLOSE, 0, 0)


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


def post_together(window, posting) -> bool:
    """Run posting() with the window's thread held, so every message lands before any is read.

    Posting one message at a time from Python is slower than a Release editor drains its queue, so
    without this a "burst" is read as it arrives and the measurement becomes the poster's speed
    rather than the editor's coalescing (ADR 0011 decision 6). Returns whether the hold took.
    """
    # 返り値がスレッド、出力引数がプロセス。取り違えると無関係なスレッドを止めることになる。
    thread = user.GetWindowThreadProcessId(window, None)
    handle = kernel.OpenThread(THREAD_SUSPEND_RESUME, False, thread) if thread else None
    if not handle:
        posting()
        return False
    held = kernel.SuspendThread(handle) != SUSPEND_FAILED
    try:
        posting()
    finally:
        if held:
            kernel.ResumeThread(handle)
        kernel.CloseHandle(handle)
    return held


def window_title(window) -> str:
    """The タスクバー title, which is "<tab title> - NeNe Nib" (ADR 0010 decision 13)."""
    buffer = c.create_unicode_buffer(512)
    user.GetWindowTextW(window, buffer, len(buffer))
    return buffer.value


def covered_by(window) -> str | None:
    """The title of the window that owns the centre of this one's client area, or None if it is ours.

    Both scripts that read something back through the screen need the same question answered: is
    the surface under the centre of the client area really this window? eng/verify-window.py reads
    pixels there, and eng/measure-speed.py measures a window the compositor must not be throttling
    (Issue #30). Nothing here judges; the caller decides what a covered window means for it.
    """
    client = rectangle(window, user.GetClientRect)
    point = w.POINT((client[2] - client[0]) // 2, (client[3] - client[1]) // 2)
    assert user.ClientToScreen(window, c.byref(point))
    other = int(user.WindowFromPoint(point))
    if other == int(window):
        return None
    return window_title(other) if other else "no window at that point"


def owned_dialog(pid: int) -> int:
    """The visible MessageBoxW of that process, if one is up."""
    child = None
    while True:
        child = user.FindWindowExW(None, child, DIALOG_CLASS, None)
        if not child:
            return 0
        owner = w.DWORD()
        user.GetWindowThreadProcessId(child, c.byref(owner))
        if owner.value == pid and user.IsWindowVisible(child):
            return child


def await_dialog(process, seconds: float = 2.5) -> int:
    """The MessageBox that process puts up within the window, or 0 if it puts none up."""
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        dialog = owned_dialog(process.pid)
        if dialog:
            return dialog
        if process.poll() is not None:
            return 0
        time.sleep(0.05)
    return 0


def dialog_closed(dialog, seconds: float = 3.0) -> bool:
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if not user.IsWindow(dialog) or not user.IsWindowVisible(dialog):
            return True
        time.sleep(0.05)
    return False


def dismiss_dialog(process, button: int, seconds: float = 2.5) -> bool:
    """Press one button of the unsaved confirmation; the real keyboard is never touched."""
    dialog = await_dialog(process, seconds)
    if not dialog:
        return False
    assert user.PostMessageW(dialog, WM_COMMAND, button, 0)
    assert dialog_closed(dialog), "the confirmation did not take the answer"
    return True


def acknowledge_dialog(process, seconds: float = 2.5) -> bool:
    """A MessageBox with one button has one answer, so closing it is that answer."""
    dialog = await_dialog(process, seconds)
    if not dialog:
        return False
    assert user.PostMessageW(dialog, WM_CLOSE, 0, 0)
    assert dialog_closed(dialog), "the reported reason could not be dismissed"
    return True


def key_input(key: int, flags: int) -> INPUT:
    record = INPUT()
    record.kind = INPUT_KEYBOARD
    record.payload.ki = KEYBDINPUT(key, 0, flags, 0, None)
    return record


def take_foreground(window, attempts: int = FOREGROUND_ATTEMPTS) -> bool:
    """SetForegroundWindow is refused unless we share the input queue of the current foreground."""
    for _ in range(attempts):
        if user.GetForegroundWindow() == window:
            return True
        foreground = user.GetForegroundWindow()
        theirs = user.GetWindowThreadProcessId(foreground, None) if foreground else 0
        ours = kernel.GetCurrentThreadId()
        attached = bool(theirs) and bool(user.AttachThreadInput(ours, theirs, True))
        user.BringWindowToTop(window)
        user.SetForegroundWindow(window)
        if attached:
            user.AttachThreadInput(ours, theirs, False)
        time.sleep(0.5)
    return user.GetForegroundWindow() == window


def send_keys(window, keys: list[int]) -> bool:
    """Real keystrokes through the raw input queue, so an IME can process them (ADR 0014).

    Posted messages never reach the IME: the window manager offers a key to ImmProcessKey only for
    input the queue really carried, so WM_IME_COMPOSITION cannot be provoked with PostMessageW.
    The pointer is never touched and the foreground is taken the same way press_chord takes it;
    when the session refuses the foreground this returns False and the caller records that.
    """
    if not take_foreground(window):
        return False
    records = (INPUT * (2 * len(keys)))()
    for index, key in enumerate(keys):
        records[2 * index] = key_input(key, 0)
        records[2 * index + 1] = key_input(key, KEYEVENTF_KEYUP)
    return user.SendInput(len(records), c.byref(records), c.sizeof(INPUT)) == len(records)


def press_chord(window, modifier: int, key: int) -> bool:
    """Ctrl+key through the raw input queue; posted messages cannot carry the modifier."""
    if not take_foreground(window):
        return False
    records = (INPUT * 4)(key_input(modifier, 0), key_input(key, 0),
                          key_input(key, KEYEVENTF_KEYUP),
                          key_input(modifier, KEYEVENTF_KEYUP))
    return user.SendInput(len(records), c.byref(records), c.sizeof(INPUT)) == len(records)


def capture(window) -> tuple[int, int, bytes]:
    """Read the composed client area back from the screen device context (top-down BGRA).

    It copies what the compositor shows, so another window over the editor ends up in the picture;
    callers raise the window first and keep the middle of the screen clear.
    """
    client = rectangle(window, user.GetClientRect)
    width, height = client[2] - client[0], client[3] - client[1]
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
        return width, height, bytes(pixels)
    finally:
        gdi.SelectObject(memory, previous)
        gdi.DeleteObject(bitmap)
        gdi.DeleteDC(memory)
        user.ReleaseDC(None, screen)


def png_chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def write_png(path: Path, width: int, height: int, bgra: bytes) -> None:
    """Top-down BGRA to an 8-bit RGB PNG with the standard library alone (zlib, no dependency)."""
    assert len(bgra) == width * height * 4, "the pixel buffer does not match the size"
    rows = []
    for y in range(height):
        row = bgra[y * width * 4:(y + 1) * width * 4]
        rgb = bytearray(width * 3)
        rgb[0::3], rgb[1::3], rgb[2::3] = row[2::4], row[1::4], row[0::4]
        rows.append(b"\x00" + bytes(rgb))
    header = struct.pack(">IIBBBBB", width, height, 8, PNG_RGB, 0, 0, 0)
    Path(path).write_bytes(PNG_SIGNATURE + png_chunk(b"IHDR", header)
                           + png_chunk(b"IDAT", zlib.compress(b"".join(rows), 6))
                           + png_chunk(b"IEND", b""))


def read_png(path: Path) -> tuple[int, int, bytes]:
    """Read back only the shape write_png writes (8-bit RGB, filter 0, no interlace) as RGB bytes."""
    data = Path(path).read_bytes()
    if not data.startswith(PNG_SIGNATURE):
        raise ValueError(f"{path}: not a PNG")
    offset, header, compressed = len(PNG_SIGNATURE), None, []
    while offset + 12 <= len(data):
        (length,) = struct.unpack(">I", data[offset:offset + 4])
        kind = data[offset + 4:offset + 8]
        body = data[offset + 8:offset + 8 + length]
        (checksum,) = struct.unpack(">I", data[offset + 8 + length:offset + 12 + length])
        if zlib.crc32(kind + body) != checksum:
            raise ValueError(f"{path}: the {kind!r} chunk fails its CRC")
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        if kind == b"IDAT":
            compressed.append(body)
        if kind == b"IEND":
            break
        offset += 12 + length
    if header is None:
        raise ValueError(f"{path}: no IHDR")
    width, height, depth, colour, method, filtering, interlace = header
    if (depth, colour, method, filtering, interlace) != (8, PNG_RGB, 0, 0, 0):
        raise ValueError(f"{path}: only 8-bit RGB without interlace is read here")
    raw = zlib.decompress(b"".join(compressed))
    stride = width * 3 + 1
    if len(raw) != stride * height:
        raise ValueError(f"{path}: the image data does not match {width}x{height}")
    rows = []
    for y in range(height):
        row = raw[y * stride:(y + 1) * stride]
        if row[0] != 0:
            raise ValueError(f"{path}: only filter 0 is read here")
        rows.append(row[1:])
    return width, height, b"".join(rows)


def capture_png(window, path: Path) -> tuple[int, int]:
    """capture() written as a PNG; returns the size that was saved."""
    width, height, pixels = capture(window)
    write_png(path, width, height, pixels)
    return width, height


def parse_keys(text: str) -> list[tuple[str, object]]:
    """The fixture notation (ihello<Esc>) as ("text", str) / ("key", vk) / ("chord", vk) steps."""
    steps: list[tuple[str, object]] = []
    index = 0
    while index < len(text):
        if text[index] == "<":
            end = text.find(">", index)
            name = text[index:end + 1] if end >= 0 else text[index:]
            if name not in KEY_NAMES:
                raise ValueError(f"unknown key notation {name!r}; known: {', '.join(KEY_NAMES)}")
            steps.append(KEY_NAMES[name])
            index += len(name)
            continue
        if steps and steps[-1][0] == "text":
            steps[-1] = ("text", str(steps[-1][1]) + text[index])
        else:
            steps.append(("text", text[index]))
        index += 1
    return steps


def send_key_sequence(window, text: str) -> None:
    """Post the parsed steps; only <C-v> needs the foreground, because it carries Ctrl."""
    for kind, value in parse_keys(text):
        if kind == "text":
            write_text(window, str(value))
        if kind == "key":
            press(window, int(value))
        if kind == "chord":
            assert press_chord(window, VK_CONTROL, int(value)), "the foreground was refused for Ctrl"
