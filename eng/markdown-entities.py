"""Generate src/core/MarkdownEntityTable.hpp from Python's html.entities (Issue #1840).

The Markdown preview resolves the named character references md4c hands it (MD_TEXT_ENTITY) with
one table: every HTML5 named reference whose name ends with `;` (`html.entities.html5`), with the
`;` dropped, in byte order of the name so that src/core/MarkdownEntity.cpp can binary search it.
The legacy names without `;` (`&amp` `&copy`) are not in the table: md4c only hands over spellings
that end with `;`. A name maps to one or two code points (`&NotEqualTilde;` is U+2242 U+0338).
The data is the WHATWG list that ships with the pinned Python (eng/tool-versions.json). CNF-010
compares the stored file with `markdown_entities_header()` byte for byte (eng/conformance.py), so
this file is the one place the C++ table's form is written.

  python eng/markdown-entities.py --write   rewrite src/core/MarkdownEntityTable.hpp
  python eng/markdown-entities.py --check   exit 1 when the stored file differs
"""

from __future__ import annotations

import argparse
import html.entities
import sys
from pathlib import Path

TARGET = "src/core/MarkdownEntityTable.hpp"
BANNER = "// 生成物。手で編集しない。python eng/markdown-entities.py --write"


def named_entities() -> list[tuple[str, str]]:
    """(name without `;`, value) for every HTML5 name that ends with `;`, in byte order."""
    pairs = [(name[:-1], value) for name, value in html.entities.html5.items() if name.endswith(";")]
    for name, value in pairs:
        if not name.isascii() or not name.isalnum():
            raise ValueError(f"{name}: a name is not ASCII letters and digits")
        if not 1 <= len(value) <= 2:
            raise ValueError(f"{name}: the value is not one or two code points")
    return sorted(pairs, key=lambda pair: pair[0].encode("ascii"))


def value_literal(value: str) -> str:
    return 'U"' + "".join(f"\\U{ord(character):08X}" for character in value) + '"'


def markdown_entities_header() -> str:
    """The whole of src/core/MarkdownEntityTable.hpp."""
    pairs = named_entities()
    rows = "\n".join(f'    {{"{name}", {value_literal(value)}}},' for name, value in pairs)
    return ("// clang-format off\n"
            f"{BANNER}\n"
            "// HTML5 の名前つきの実体参照（Python の html.entities.html5 の末尾が ; の名前・; を除いた"
            "名前のバイト順）\n"
            "#pragma once\n"
            "\n"
            "#include <array>\n"
            "#include <string_view>\n"
            "#include <utility>\n"
            "\n"
            "namespace nenenib::core\n"
            "{\n"
            "inline constexpr std::array<std::pair<std::string_view, std::u32string_view>, "
            f"{len(pairs)}> markdown_named_entities{{{{\n"
            f"{rows}\n"
            "}};\n"
            "} // namespace nenenib::core\n"
            "// clang-format on\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true", help=f"rewrite {TARGET}")
    mode.add_argument("--check", action="store_true", help=f"exit 1 when {TARGET} differs")
    arguments = parser.parse_args()
    target = Path(__file__).resolve().parents[1] / TARGET
    content = markdown_entities_header().encode("utf-8")
    if arguments.write:
        target.write_bytes(content)
        return 0
    if target.is_file() and target.read_bytes() == content:
        return 0
    print(f"{TARGET} differs from the generated table; run python eng/markdown-entities.py --write",
          file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
