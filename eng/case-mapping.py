"""Generate src/core/CaseMappingTable.hpp from Python's unicodedata (Issue #781).

The Vim case operators (`~` `g~` `gu` `gU` and VISUAL `~` `u` `U`) change one code point into one
code point. The table is the simple (one to one) part of the Unicode case mapping as Python's
`str.upper` / `str.lower` give it. Python returns the full (SpecialCasing) mapping, so a code point
whose full mapping is longer than one code point drops out; `SIMPLE_OVERRIDES` puts back the simple
mapping that UnicodeData.txt gives those code points (28 of them, Issue #990: `İ` -> `i` and the
Greek small letters with ypogegrammeni -> their title case capitals), which is what Vim's
`utf_toupper` / `utf_tolower` use. `ß` has no simple uppercase and keeps itself here; the one to many
`gU` of `ß` -> `SS` is written beside the operator (src/core/VimStepOperators.cpp `cased_text`).
Locale rules (Turkish `i`) and title case are not used.

The mapping is folded into ranges (first, last, step, delta): every `step`-th code point from
`first` to `last` maps to itself plus `delta`. Vim's own tables are not copied; the data is the
Unicode Character Database that ships with the pinned Python (eng/tool-versions.json), and its
version is written into the header. CNF-010 compares the stored file with `case_mapping_header()`
byte for byte (eng/conformance.py), so this file is the one place the C++ table's form is written.

  python eng/case-mapping.py --write   rewrite src/core/CaseMappingTable.hpp
  python eng/case-mapping.py --check   exit 1 when the stored file differs
"""

from __future__ import annotations

import argparse
import sys
import unicodedata
from pathlib import Path

TARGET = "src/core/CaseMappingTable.hpp"
BANNER = "// 生成物。手で編集しない。python eng/case-mapping.py --write"
LAST_CODE_POINT = 0x10FFFF
STEPS = (1, 2)

# The simple case mapping of UnicodeData.txt (Unicode 15.0.0, fields 12 and 13; Unicode License v3)
# for the code points whose SpecialCasing full mapping Python returns instead (Issue #990).
# Vim 9.1's toupper() / tolower() agree with the generated table plus these 28 on U+0080..U+10FFFF.
SIMPLE_OVERRIDES: dict[str, dict[int, int]] = {
    "upper": {
        **{0x1F80 + offset: 0x1F88 + offset for offset in range(8)},
        **{0x1F90 + offset: 0x1F98 + offset for offset in range(8)},
        **{0x1FA0 + offset: 0x1FA8 + offset for offset in range(8)},
        0x1FB3: 0x1FBC,
        0x1FC3: 0x1FCC,
        0x1FF3: 0x1FFC,
    },
    "lower": {0x0130: 0x0069},
}


def simple_mapping(convert, overrides: dict[int, int]) -> dict[int, int]:
    """Code point -> code point for the one-to-one part of `convert` (str.upper or str.lower),
    with `overrides` filling the code points whose full mapping is longer than one code point."""
    for value in overrides:
        if len(convert(chr(value))) < 2:
            raise ValueError(f"U+{value:04X}: the override is not for a one to many full mapping")
    mapping = dict(overrides)
    for value in range(LAST_CODE_POINT + 1):
        if 0xD800 <= value <= 0xDFFF:
            continue
        mapped = convert(chr(value))
        if len(mapped) == 1 and ord(mapped) != value:
            mapping[value] = ord(mapped)
    return mapping


def folded_ranges(mapping: dict[int, int]) -> list[tuple[int, int, int, int]]:
    """Greedy runs of code points spaced by 1 or 2 that share one delta, in ascending order."""
    keys = sorted(mapping)
    ranges = []
    index = 0
    while index < len(keys):
        first = keys[index]
        delta = mapping[first] - first
        step = 1
        last = first
        if index + 1 < len(keys):
            gap = keys[index + 1] - first
            if gap in STEPS and mapping[keys[index + 1]] - keys[index + 1] == delta:
                step = gap
        while (index + 1 < len(keys) and keys[index + 1] == last + step
               and mapping[keys[index + 1]] - keys[index + 1] == delta):
            index += 1
            last = keys[index]
        ranges.append((first, last, step, delta))
        index += 1
    return ranges


def expanded(ranges: list[tuple[int, int, int, int]]) -> dict[int, int]:
    return {value: value + delta
            for first, last, step, delta in ranges
            for value in range(first, last + 1, step)}


def table_rows(name: str, ranges: list[tuple[int, int, int, int]]) -> str:
    rows = "\n".join(f"    {{U'\\x{first:X}', U'\\x{last:X}', {step}, {delta}}},"
                     for first, last, step, delta in ranges)
    return (f"inline constexpr std::array<CaseMappingRange, {len(ranges)}> {name}{{{{\n"
            f"{rows}\n"
            "}};\n")


def case_mapping_header() -> str:
    """The whole of src/core/CaseMappingTable.hpp. Needs no Vim."""
    tables = []
    for name, convert, overrides in (("case_to_upper", str.upper, SIMPLE_OVERRIDES["upper"]),
                                     ("case_to_lower", str.lower, SIMPLE_OVERRIDES["lower"])):
        mapping = simple_mapping(convert, overrides)
        ranges = folded_ranges(mapping)
        if expanded(ranges) != mapping:
            raise ValueError(f"{name}: the folded ranges do not reproduce the mapping")
        tables.append(table_rows(name, ranges))
    return ("// clang-format off\n"
            f"{BANNER}\n"
            f"// Unicode {unicodedata.unidata_version}（Python {sys.version_info.major}.{sys.version_info.minor} の unicodedata）\n"
            f"// 1 対多の字は UnicodeData.txt の簡単な写像で {sum(map(len, SIMPLE_OVERRIDES.values()))} 字を上書き（Issue #990）\n"
            "#pragma once\n"
            "\n"
            '#include "CaseMappingRange.hpp"\n'
            "\n"
            "#include <array>\n"
            "\n"
            "namespace nenenib::core\n"
            "{\n"
            f"{tables[0]}"
            "\n"
            f"{tables[1]}"
            "} // namespace nenenib::core\n"
            "// clang-format on\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true", help=f"rewrite {TARGET}")
    mode.add_argument("--check", action="store_true", help=f"exit 1 when {TARGET} differs")
    arguments = parser.parse_args()
    target = Path(__file__).resolve().parents[1] / TARGET
    content = case_mapping_header().encode("utf-8")
    if arguments.write:
        target.write_bytes(content)
        return 0
    if target.is_file() and target.read_bytes() == content:
        return 0
    print(f"{TARGET} differs from the generated table; run python eng/case-mapping.py --write",
          file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
