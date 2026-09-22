"""Compare two PNG frames from eng/verify-window.py --capture pixel by pixel (Issue #131).

Prints one JSON object: the size, the number of pixels that differ, and their bounding rectangle
{"x","y","w","h"} (null when nothing differs). With --inside x,y,w,h it also says whether that
rectangle lies inside the given one. With --regions <frames.json> (the "regions" that
verify-window.py --keys writes: title, body, status) it counts the differing pixels per region
and those in no region ("outside"), and "inside" is true exactly when "outside" is 0; --regions
wins over --inside when both are given. There is no threshold: a pixel either matches exactly or
it does not, and the design seat reads the PNGs themselves for everything else.

Exit codes: 2 when the two frames differ in size, 1 when --inside is given and the difference
reaches outside it (with --regions: when any differing pixel lies in no region), 0 otherwise.
Needs no display (it only reads files), but it is a tool for the interactive check, not a gate
(QLT-013). Python standard library only; the PNG reader is the one in
eng/window_driver.py, the same module that wrote the frames.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from window_driver import read_png  # noqa: E402  窓の画を書いた側と同じ 1 本の読み手


def rectangle_argument(text: str) -> dict:
    parts = text.split(",")
    if len(parts) != 4:
        raise argparse.ArgumentTypeError("expected x,y,w,h")
    x, y, width, height = (int(part) for part in parts)
    return {"x": x, "y": y, "w": width, "h": height}


def difference_bounds(width: int, height: int, before: bytes, after: bytes) -> tuple[int, dict | None]:
    """Count the differing RGB pixels and return their bounding rectangle."""
    stride = width * 3
    count, left, top, right, bottom = 0, width, height, -1, -1
    for y in range(height):
        row_before = before[y * stride:(y + 1) * stride]
        row_after = after[y * stride:(y + 1) * stride]
        if row_before == row_after:
            continue
        for x in range(width):
            if row_before[x * 3:x * 3 + 3] != row_after[x * 3:x * 3 + 3]:
                count += 1
                left, right = min(left, x), max(right, x)
        top, bottom = min(top, y), max(bottom, y)
    if count == 0:
        return 0, None
    return count, {"x": left, "y": top, "w": right - left + 1, "h": bottom - top + 1}


def region_differences(width: int, height: int, before: bytes, after: bytes,
                       regions: dict) -> tuple[dict, dict]:
    """Per region, and for the pixels in no region, the count and bounds of the differing pixels."""
    boxes = {name: [0, width, height, -1, -1] for name in regions}
    outside = [0, width, height, -1, -1]
    stride = width * 3
    for y in range(height):
        row_before = before[y * stride:(y + 1) * stride]
        row_after = after[y * stride:(y + 1) * stride]
        if row_before == row_after:
            continue
        for x in range(width):
            if row_before[x * 3:x * 3 + 3] == row_after[x * 3:x * 3 + 3]:
                continue
            owner = next((boxes[name] for name, box in regions.items()
                          if box["x"] <= x < box["x"] + box["w"]
                          and box["y"] <= y < box["y"] + box["h"]), outside)
            owner[0] += 1
            owner[1], owner[2] = min(owner[1], x), min(owner[2], y)
            owner[3], owner[4] = max(owner[3], x), max(owner[4], y)

    def summary(box: list) -> dict:
        count, left, top, right, bottom = box
        bounds = None if count == 0 else {"x": left, "y": top, "w": right - left + 1,
                                          "h": bottom - top + 1}
        return {"differentPixels": count, "bounds": bounds}

    return {name: summary(box) for name, box in boxes.items()}, summary(outside)


def contains(outer: dict, inner: dict | None) -> bool:
    if inner is None:
        return True
    return (inner["x"] >= outer["x"] and inner["y"] >= outer["y"]
            and inner["x"] + inner["w"] <= outer["x"] + outer["w"]
            and inner["y"] + inner["h"] <= outer["y"] + outer["h"])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("before", type=Path)
    parser.add_argument("after", type=Path)
    parser.add_argument("--inside", type=rectangle_argument, default=None, metavar="x,y,w,h")
    parser.add_argument("--regions", type=Path, default=None, metavar="frames.json",
                        help="the frames.json whose \"regions\" hold the expected areas")
    arguments = parser.parse_args()
    before_width, before_height, before = read_png(arguments.before)
    after_width, after_height, after = read_png(arguments.after)
    if (before_width, before_height) != (after_width, after_height):
        print(json.dumps({"error": "the frames differ in size",
                          "before": {"w": before_width, "h": before_height},
                          "after": {"w": after_width, "h": after_height}}))
        return 2
    count, bounds = difference_bounds(before_width, before_height, before, after)
    result = {"size": {"w": before_width, "h": before_height}, "differentPixels": count,
              "bounds": bounds}
    if arguments.regions is not None:
        regions = json.loads(arguments.regions.read_text(encoding="utf-8"))["regions"]
        per_region, outside = region_differences(before_width, before_height, before, after,
                                                 regions)
        result["regions"] = per_region
        result["outside"] = outside
        result["inside"] = outside["differentPixels"] == 0
    elif arguments.inside is not None:
        result["insideOf"] = arguments.inside
        result["inside"] = contains(arguments.inside, bounds)
    print(json.dumps(result))
    return 0 if result.get("inside", True) else 1


if __name__ == "__main__":
    raise SystemExit(main())
