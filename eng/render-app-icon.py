"""Derive the Windows icon and the MSIX logos from the one accepted SVG (Issue #1580 / ADR 0185).

The SVG (`docs/design/app-icon.svg`, D87 / D88) is the only drawing. Everything else is rendered from
it by headless Chrome, the same renderer and settings as the design rounds and NeNe Loupe's
`eng/render-app-icon.py` (transparent background, device scale 1, only the root svg width/height
changed per size). The rendered pixels are re-encoded here with the standard library alone, so the
bytes of a committed PNG or ICO depend only on its pixels, and the checks need no Pillow.

Modes:
  --chrome C --work D           render and write the generated files and their digests
  --chrome C --work D --check   render again and compare the pixels with the committed files
  --verify                      without Chrome: file set, sizes, ICO directory and digests
  --verify-exe EXE              the exe's icon group holds the committed ICO's frames

The manifest `eng/app-icon.json` owns the paths, the sizes, the renderer version and the digests.
"""

import argparse
import hashlib
import json
import struct
import subprocess
import sys
import zlib
from collections.abc import Callable
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = "eng/app-icon.json"
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"

# One image: (width, height, RGBA bytes row by row from the top).
Pixels = tuple[int, int, bytes]


# --- PNG --------------------------------------------------------------------------------------


def _paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def _unfilter(kind: int, line: bytearray, previous: bytes, step: int) -> None:
    for i in range(len(line)):
        left = line[i - step] if i >= step else 0
        up = previous[i]
        corner = previous[i - step] if i >= step else 0
        if kind == 1:
            line[i] = (line[i] + left) & 0xFF
        elif kind == 2:
            line[i] = (line[i] + up) & 0xFF
        elif kind == 3:
            line[i] = (line[i] + ((left + up) >> 1)) & 0xFF
        elif kind == 4:
            line[i] = (line[i] + _paeth(left, up, corner)) & 0xFF
        elif kind != 0:
            raise ValueError(f"PNG filter {kind} is not defined")


def decode_png(data: bytes) -> Pixels:
    """8-bit RGBA or RGB, not interlaced: what Chrome writes and what encode_png writes."""
    if not data.startswith(PNG_SIGNATURE):
        raise ValueError("not a PNG")
    position, header, compressed = len(PNG_SIGNATURE), None, bytearray()
    while position < len(data):
        length, kind = struct.unpack(">I4s", data[position:position + 8])
        body = data[position + 8:position + 8 + length]
        position += 12 + length
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            compressed.extend(body)
        elif kind == b"IEND":
            break
    if header is None:
        raise ValueError("PNG has no IHDR")
    width, height, depth, color, _, _, interlace = header
    if depth != 8 or color not in (2, 6) or interlace != 0:
        raise ValueError(f"unsupported PNG depth {depth} color {color} interlace {interlace}")
    channels = 4 if color == 6 else 3
    stride = width * channels
    raw = zlib.decompress(bytes(compressed))
    previous, rows = bytes(stride), bytearray()
    for y in range(height):
        start = y * (stride + 1)
        line = bytearray(raw[start + 1:start + 1 + stride])
        _unfilter(raw[start], line, previous, channels)
        rows.extend(line)
        previous = bytes(line)
    if channels == 3:
        rgba = bytearray()
        for i in range(0, len(rows), 3):
            rgba.extend(rows[i:i + 3])
            rgba.append(255)
        rows = rgba
    return width, height, bytes(rows)


def _filtered(kind: int, line: bytes, previous: bytes) -> bytes:
    out = bytearray(len(line))
    for i, value in enumerate(line):
        left = line[i - 4] if i >= 4 else 0
        up = previous[i]
        corner = previous[i - 4] if i >= 4 else 0
        predictor = (0, left, up, (left + up) >> 1, _paeth(left, up, corner))[kind]
        out[i] = (value - predictor) & 0xFF
    return bytes(out)


def _chunk(kind: bytes, body: bytes) -> bytes:
    return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))


def encode_png(image: Pixels) -> bytes:
    """Deterministic RGBA PNG: per row the filter with the least sum of magnitudes, zlib level 9."""
    width, height, rgba = image
    stride, previous, raw = width * 4, bytes(width * 4), bytearray()
    for y in range(height):
        line = rgba[y * stride:(y + 1) * stride]
        candidates = [_filtered(kind, line, previous) for kind in range(5)]
        costs = [sum(v if v < 128 else 256 - v for v in c) for c in candidates]
        kind = costs.index(min(costs))
        raw.append(kind)
        raw.extend(candidates[kind])
        previous = line
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (PNG_SIGNATURE + _chunk(b"IHDR", header) + _chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + _chunk(b"IEND", b""))


# --- ICO --------------------------------------------------------------------------------------


def dib_frame(image: Pixels) -> bytes:
    """32-bit BGRA bottom-up DIB with the AND mask set where alpha is 0 (as NeNe Loupe)."""
    width, height, rgba = image
    pixels = bytearray()
    for y in range(height - 1, -1, -1):
        for x in range(width):
            r, g, b, a = rgba[(y * width + x) * 4:(y * width + x) * 4 + 4]
            pixels.extend((b, g, r, a))
    mask_stride = ((width + 31) // 32) * 4
    mask = bytearray()
    for y in range(height - 1, -1, -1):
        row = bytearray(mask_stride)
        for x in range(width):
            if rgba[(y * width + x) * 4 + 3] == 0:
                row[x // 8] |= 0x80 >> (x % 8)
        mask.extend(row)
    header = struct.pack("<IiiHHIIiiII", 40, width, height * 2, 1, 32, 0, len(pixels), 0, 0, 0, 0)
    return header + bytes(pixels) + bytes(mask)


def decode_dib(frame: bytes) -> Pixels:
    size, width, double_height, _, bits = struct.unpack("<IiiHH", frame[:16])
    if size != 40 or bits != 32 or double_height != width * 2:
        raise ValueError("unsupported ICO frame")
    height, rgba = width, bytearray(width * width * 4)
    for row in range(height):
        y = height - 1 - row
        for x in range(width):
            b, g, r, a = frame[40 + (row * width + x) * 4:40 + (row * width + x) * 4 + 4]
            rgba[(y * width + x) * 4:(y * width + x) * 4 + 4] = bytes((r, g, b, a))
    return width, height, bytes(rgba)


def make_icon(frames: list[Pixels]) -> bytes:
    """Frames in ascending size; 256 is stored as PNG, the others as DIB with an AND mask."""
    encoded = [encode_png(f) if f[0] == 256 else dib_frame(f) for f in frames]
    offset, entries = 6 + 16 * len(frames), []
    for frame, data in zip(frames, encoded, strict=True):
        side = 0 if frame[0] == 256 else frame[0]
        entries.append(struct.pack("<BBBBHHII", side, side, 0, 0, 1, 32, len(data), offset))
        offset += len(data)
    return struct.pack("<HHH", 0, 1, len(frames)) + b"".join(entries) + b"".join(encoded)


def icon_entries(data: bytes) -> list[tuple[int, bytes]]:
    """(size, stored bytes) for every image in an ICO file."""
    reserved, kind, count = struct.unpack("<HHH", data[:6])
    if reserved != 0 or kind != 1:
        raise ValueError("not an ICO file")
    entries = []
    for i in range(count):
        side, _, _, _, _, bits, length, offset = struct.unpack("<BBBBHHII", data[6 + 16 * i:22 + 16 * i])
        if bits != 32:
            raise ValueError("ICO frame is not 32-bit")
        entries.append((side or 256, data[offset:offset + length]))
    return entries


def icon_frame_pixels(stored: bytes) -> Pixels:
    return decode_png(stored) if stored.startswith(PNG_SIGNATURE) else decode_dib(stored)


# --- manifest and the expected file set -------------------------------------------------------


def load_manifest(root: Path) -> dict:
    return json.loads((root / MANIFEST).read_text(encoding="utf-8"))


def msix_logos(manifest: dict) -> dict[str, int]:
    logos = {}
    for name, base in manifest["bases"].items():
        for scale in manifest["scales"]:
            logos[f"{name}.scale-{scale}.png"] = (base * scale + 50) // 100
    for size in manifest["targetSizes"]:
        for form in ("unplated", "lightunplated"):
            logos[f"Square44x44Logo.targetsize-{size}_altform-{form}.png"] = size
    return logos


def png_outputs(manifest: dict) -> dict[str, int]:
    """Repository path -> pixel size of every generated PNG."""
    outputs = {f"{manifest['msixDirectory']}/{name}": size for name, size in msix_logos(manifest).items()}
    outputs[manifest["listingLogo"]] = manifest["listingSize"]
    return outputs


def needed_sizes(manifest: dict) -> list[int]:
    return sorted(set(manifest["iconSizes"]) | set(png_outputs(manifest).values()))


# --- rendering --------------------------------------------------------------------------------


def chrome_version(chrome: Path) -> str:
    command = f"(Get-Item -LiteralPath '{chrome}').VersionInfo.ProductVersion"
    result = subprocess.run(["pwsh", "-NoProfile", "-Command", command], check=True,
                            capture_output=True, text=True)
    return result.stdout.strip()


def chrome_renderer(chrome: Path, source: Path, work: Path) -> Callable[[int], Pixels]:
    svg = source.read_text(encoding="utf-8")
    dimensions = 'width="256" height="256"'
    start = svg.find("<svg")
    end = svg.find(">", start)
    if start < 0 or end < 0 or svg[start:end].count(dimensions) != 1:
        raise ValueError("the accepted SVG must have one 256 by 256 root dimension")

    def render(size: int) -> Pixels:
        sized = svg[:start] + svg[start:end].replace(dimensions, f'width="{size}" height="{size}"') + svg[end:]
        work.mkdir(parents=True, exist_ok=True)
        sized_path = work / f"app-icon-{size}.svg"
        image = work / f"app-icon-{size}.png"
        sized_path.write_text(sized, encoding="utf-8")
        subprocess.run(
            [str(chrome), "--headless=new", "--disable-gpu", "--no-first-run",
             "--no-default-browser-check", f"--user-data-dir={work / 'chrome-profile'}",
             "--default-background-color=00000000", "--force-device-scale-factor=1",
             f"--window-size={size},{size}", f"--screenshot={image}", sized_path.as_uri()],
            check=True, capture_output=True, text=True)
        pixels = decode_png(image.read_bytes())
        if pixels[:2] != (size, size):
            raise ValueError(f"Chrome rendered {pixels[:2]} for {size}")
        alphas = pixels[2][3::4]
        if min(alphas) >= 255 or max(alphas) != 255:
            raise ValueError(f"the {size} px render lacks transparent corners or an opaque tile")
        return pixels

    return render


def generated_files(manifest: dict, render: Callable[[int], Pixels]) -> dict[str, bytes]:
    images = {size: render(size) for size in needed_sizes(manifest)}
    files = {manifest["icon"]: make_icon([images[s] for s in sorted(manifest["iconSizes"])])}
    for path, size in png_outputs(manifest).items():
        files[path] = encode_png(images[size])
    return files


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def write(root: Path, manifest: dict, render: Callable[[int], Pixels], version: str) -> None:
    files = generated_files(manifest, render)
    msix = root / manifest["msixDirectory"]
    msix.mkdir(parents=True, exist_ok=True)
    for stale in msix.glob("*.png"):
        stale.unlink()
    for path, data in files.items():
        (root / path).parent.mkdir(parents=True, exist_ok=True)
        (root / path).write_bytes(data)
    manifest["renderer"]["version"] = version
    manifest["digests"] = {manifest["source"]: sha256((root / manifest["source"]).read_bytes())}
    manifest["digests"].update({path: sha256(data) for path, data in sorted(files.items())})
    (root / MANIFEST).write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8",
                                 newline="\n")


# --- checks -----------------------------------------------------------------------------------


def verify(root: Path) -> list[str]:
    """Without Chrome: the committed files are the manifest's set, sizes and digests."""
    manifest = load_manifest(root)
    problems = []
    expected = {manifest["source"], manifest["icon"], *png_outputs(manifest)}
    if set(manifest.get("digests", {})) != expected:
        problems.append("the recorded digests do not name exactly the source and the generated files")
    on_disk = {f"{manifest['msixDirectory']}/{p.name}" for p in (root / manifest["msixDirectory"]).glob("*")}
    for extra in sorted(on_disk - set(png_outputs(manifest))):
        problems.append(f"{extra}: not a generated logo")
    for path in sorted(expected):
        file = root / path
        if not file.is_file():
            problems.append(f"{path}: missing")
            continue
        data = file.read_bytes()
        if manifest.get("digests", {}).get(path) != sha256(data):
            problems.append(f"{path}: SHA-256 differs from {MANIFEST}")
    for path, size in png_outputs(manifest).items():
        if (root / path).is_file() and decode_png((root / path).read_bytes())[:2] != (size, size):
            problems.append(f"{path}: not {size} by {size}")
    if (root / manifest["icon"]).is_file():
        entries = icon_entries((root / manifest["icon"]).read_bytes())
        if [side for side, _ in entries] != sorted(manifest["iconSizes"]):
            problems.append(f"{manifest['icon']}: sizes {[s for s, _ in entries]} are not {sorted(manifest['iconSizes'])}")
        for side, stored in entries:
            if (side == 256) != stored.startswith(PNG_SIGNATURE):
                problems.append(f"{manifest['icon']}: the {side} px frame has the wrong encoding")
            elif icon_frame_pixels(stored)[:2] != (side, side):
                problems.append(f"{manifest['icon']}: the {side} px frame is not {side} by {side}")
    return problems


def compare(root: Path, render: Callable[[int], Pixels]) -> list[str]:
    """Render again and compare pixels with every committed PNG and ICO frame."""
    manifest = load_manifest(root)
    images = {size: render(size) for size in needed_sizes(manifest)}
    problems = []
    for path, size in png_outputs(manifest).items():
        file = root / path
        if not file.is_file():
            problems.append(f"{path}: missing")
        elif decode_png(file.read_bytes()) != images[size]:
            problems.append(f"{path}: pixels differ from the render of {manifest['source']}")
    icon = root / manifest["icon"]
    if not icon.is_file():
        return problems + [f"{manifest['icon']}: missing"]
    for side, stored in icon_entries(icon.read_bytes()):
        if side not in images or icon_frame_pixels(stored) != images[side]:
            problems.append(f"{manifest['icon']}: the {side} px frame differs from the render")
    return problems


# --- the exe ----------------------------------------------------------------------------------


def _resource_directory(data: bytes, base: int, offset: int) -> list[tuple[int, int, bool]]:
    named, numbered = struct.unpack("<HH", data[base + offset + 12:base + offset + 16])
    entries = []
    for i in range(named + numbered):
        name, target = struct.unpack("<II", data[base + offset + 16 + 8 * i:base + offset + 24 + 8 * i])
        entries.append((name, target & 0x7FFFFFFF, bool(target & 0x80000000)))
    return entries


def exe_icon_resources(data: bytes) -> dict[int, dict[int, bytes]]:
    """{resource type: {id: bytes}} for RT_ICON (3) and RT_GROUP_ICON (14), first language each."""
    pe = struct.unpack("<I", data[0x3C:0x40])[0]
    sections, optional_size = struct.unpack("<H12xH", data[pe + 6:pe + 22])
    table = pe + 24 + optional_size
    rsrc = None
    for i in range(sections):
        name, virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack(
            "<8sIIII", data[table + 40 * i:table + 40 * i + 24])
        if name.rstrip(b"\0") == b".rsrc":
            rsrc = (virtual_address, raw_pointer)
    if rsrc is None:
        return {}
    virtual_address, base = rsrc
    found: dict[int, dict[int, bytes]] = {}
    for kind, kind_offset, _ in _resource_directory(data, base, 0):
        if kind not in (3, 14):
            continue
        for identifier, id_offset, _ in _resource_directory(data, base, kind_offset):
            _, leaf, _ = _resource_directory(data, base, id_offset)[0]
            rva, size = struct.unpack("<II", data[base + leaf:base + leaf + 8])
            found.setdefault(kind, {})[identifier] = data[base + rva - virtual_address:base + rva - virtual_address + size]
    return found


def verify_exe(root: Path, exe: Path, group: int) -> list[str]:
    manifest = load_manifest(root)
    resources = exe_icon_resources(exe.read_bytes())
    groups = resources.get(14, {})
    if group not in groups:
        return [f"{exe}: no RT_GROUP_ICON {group} (found {sorted(groups)})"]
    directory = groups[group]
    count = struct.unpack("<H", directory[4:6])[0]
    committed = icon_entries((root / manifest["icon"]).read_bytes())
    problems = []
    if count != len(committed):
        problems.append(f"{exe}: icon group {group} holds {count} images, the ICO {len(committed)}")
    for i in range(min(count, len(committed))):
        side, _, _, _, _, _, _, identifier = struct.unpack("<BBBBHHIH", directory[6 + 14 * i:20 + 14 * i])
        stored = resources.get(3, {}).get(identifier)
        if (side or 256) != committed[i][0] or stored != committed[i][1]:
            problems.append(f"{exe}: icon {identifier} is not the ICO's {committed[i][0]} px frame")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--chrome", type=Path)
    parser.add_argument("--work", type=Path, help="an empty-or-new directory for the sized SVGs and renders")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--verify-exe", type=Path)
    parser.add_argument("--group", type=int, default=101, help="the RT_GROUP_ICON id (AppIconResource.hpp)")
    arguments = parser.parse_args()
    if arguments.verify or arguments.verify_exe:
        problems = verify(ROOT)
        if arguments.verify_exe:
            problems += verify_exe(ROOT, arguments.verify_exe, arguments.group)
    else:
        if arguments.chrome is None or arguments.work is None or not arguments.chrome.is_file():
            parser.error("--chrome (an existing chrome.exe) and --work are required to render")
        manifest = load_manifest(ROOT)
        version = chrome_version(arguments.chrome)
        render = chrome_renderer(arguments.chrome, ROOT / manifest["source"], arguments.work.resolve())
        if not arguments.check:
            write(ROOT, manifest, render, version)
            print(f"Rendered {manifest['icon']} and {len(png_outputs(manifest))} PNGs with Chrome {version}.")
            return 0
        if version != manifest["renderer"]["version"]:
            print(f"note: Chrome {version}, the files were made with {manifest['renderer']['version']}")
        problems = compare(ROOT, render)
    for problem in problems:
        print(problem, file=sys.stderr)
    print("app icon: " + ("ok" if not problems else f"{len(problems)} problem(s)"))
    return 0 if not problems else 1


if __name__ == "__main__":
    raise SystemExit(main())
