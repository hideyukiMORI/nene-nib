"""The probe's literal copy of the Ex candidates follows the product tables (#1182).

tests/performance/ExCompletionProbe.cpp keeps its own literal list of the fixed Ex candidates, and
nine probe workloads (ex-completion-* / completion-prefix-* / palette-choice-*) compare the product
against it. eng/probe-smoke.ps1 runs only under eng/check.ps1 -Full, so a candidate added to the
product table silently broke those workloads (#1182). This check reads the product tables and the
probe's list as text and fails at once when they differ, so the PR that changes a candidate table
also rewrites the probe's list.
"""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
EX_RESULT = ROOT / "src/core/ExResult.cpp"
EX_RESULT_HEADER = ROOT / "src/core/ExResult.hpp"
EX_MAP = ROOT / "src/core/ExMapCommand.cpp"
PROBE = ROOT / "tests/performance/ExCompletionProbe.cpp"

STRING = re.compile(r'"([^"]*)"')


def read(path):
    return path.read_text(encoding="utf-8")


def block(text, opening):
    """Return the text between the brace that ends `opening` and its matching close."""
    start = text.index(opening) + len(opening)
    depth = 1
    for index in range(start, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[start:index]
    raise AssertionError(f"unclosed block after {opening!r}")


def named_strings(*texts):
    names = {}
    for text in texts:
        for match in re.finditer(r'constexpr std::string_view (\w+) = "([^"]*)";', text):
            names[match.group(1)] = match.group(2)
    return names


def items(body, names):
    """Split a flat initializer list of string literals and named string constants."""
    values = []
    for item in (part.strip() for part in body.split(",")):
        if not item:
            continue
        literal = STRING.fullmatch(item)
        if literal is not None:
            values.append(literal.group(1))
        elif item in names:
            values.append(names[item])
        else:
            raise AssertionError(f"unrecognised candidate item {item!r}")
    return values


def rows(body):
    return [row.group(1) for row in re.finditer(r"\{([^{}]*)\}", body)]


def palette_candidates(text):
    return [STRING.search(row).group(1) for row in rows(block(text, "palette_names{{"))]


def document_candidates(text):
    candidates = []
    for row in rows(block(text, "document_names{{")):
        fields = [field.strip() for field in row.split(",")]
        name = STRING.fullmatch(fields[0]).group(1)
        candidates.append(name)
        # append_document_candidates: `!` only when the forced verb differs from the plain one.
        if fields[3] != "std::nullopt" and fields[3] != fields[2]:
            candidates.append(name + "!")
    return candidates


def product_candidates():
    result = read(EX_RESULT)
    names = named_strings(result, read(EX_RESULT_HEADER))
    fixed = items(block(result, "fixed_candidates{"), names)
    mapped = items(block(read(EX_MAP), "candidates{"), {})
    return fixed + palette_candidates(result) + mapped + document_candidates(result)


def probe_candidates():
    return items(block(read(PROBE), "fixed_candidates = std::to_array<std::string_view>({"), {})


def function_body(text, signature):
    """Return the body of the function whose declaration starts with `signature` (brace matched)."""
    start = text.index(signature)
    opening = text.index("{", start)
    return block(text[opening:], "{")


class ProbeCandidateTests(unittest.TestCase):
    maxDiff = None

    def test_matching_candidates_keeps_the_order_the_probe_copies(self):
        body = function_body(read(EX_RESULT), "matching_candidates(const ThemeCatalog &themes,")
        marks = ["fixed_candidates)", "palette_names)", "map_command_candidates())",
                 "append_document_candidates(", "append_theme_candidates("]
        positions = [body.find(mark) for mark in marks]
        self.assertNotIn(-1, positions, "matching_candidates no longer reads one of the tables")
        self.assertEqual(positions, sorted(positions),
                         "matching_candidates changed the order of the candidate tables; "
                         "rewrite tests/performance/ExCompletionProbe.cpp to the same order")
        # A sixth source added between the marks would pass the order check above, so the places
        # that add candidates are counted too: 3 loops over the fixed / palette / map tables and
        # 2 appends (documents, themes). A change here means the probe's copy needs a look.
        added = len(re.findall(r"candidates\.(?:emplace_back|push_back)\(", body))
        appended = len(re.findall(r"\bappend_\w+\(", body))
        self.assertEqual((added, appended), (3, 2),
                         "matching_candidates gained or lost a candidate source; rewrite "
                         "fixed_candidates in tests/performance/ExCompletionProbe.cpp and this "
                         "test to the new sources (#1182)")

    def test_probe_copy_equals_the_product_tables(self):
        product = product_candidates()
        probe = probe_candidates()
        self.assertGreater(len(product), 40)
        missing = [name for name in product if name not in probe]
        extra = [name for name in probe if name not in product]
        first = next((index for index, (left, right) in enumerate(zip(probe, product))
                      if left != right), min(len(probe), len(product)))
        self.assertEqual(probe, product,
                         "the Ex candidate tables changed; rewrite fixed_candidates in "
                         "tests/performance/ExCompletionProbe.cpp to the same list (#1182). "
                         f"missing from the probe: {missing}; not in the product: {extra}; "
                         f"first difference at index {first}")


if __name__ == "__main__":
    unittest.main()
