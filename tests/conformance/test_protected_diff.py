"""eng/protected-diff.py の分類だけを純関数で回す（Issue #130）。

build も nib_tests の実行もしない。git の履歴にも頼らない（CI の checkout が浅くても通る）。
diff の行は #99（fixture を足しただけ）と #85（-crlf の 10 件を消した）の実物から形を借りる。
"""

import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("protected_diff", ROOT / "eng/protected-diff.py")
PROTECTED = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PROTECTED)

OLD_DIGEST = "-// fixtures.json: sha256 " + "a" * 64 + " / 1055 fixtures"
NEW_DIGEST = "+// fixtures.json: sha256 " + "b" * 64 + " / 1056 fixtures"
METADATA_ONLY = "\n".join([
    "diff --git a/tests/vim/VimFixtures.hpp b/tests/vim/VimFixtures.hpp",
    "--- a/tests/vim/VimFixtures.hpp",
    "+++ b/tests/vim/VimFixtures.hpp",
    "@@ -5 +5 @@",
    OLD_DIGEST,
    NEW_DIGEST,
    "@@ -14 +14 @@ namespace nenenib::tests",
    "-constexpr std::array<VimFixture, 1055> vim_fixtures{{",
    "+constexpr std::array<VimFixture, 1056> vim_fixtures{{",
    "@@ -1068,0 +1069 @@ constexpr std::array<VimFixture, 1055> vim_fixtures{{",
    '+    {"text-object-new", "ab", "diw", "", 1, 1, "ab", "v", std::nullopt},',
])
ONE_DELETED = "\n".join([
    "@@ -416 +415,0 @@ constexpr std::array<VimFixture, 1320> vim_fixtures{{",
    '-    {"line-jump-G-crlf", "a\\r\\n  b\\r\\nc", "2G", "a\\n  b\\nc", 2, 3, "", "", std::nullopt},',
])
ONE_CHANGED = "\n".join([
    "@@ -20 +20 @@",
    '-    {"h-with-a-count", "alpha", "$3h", "alpha", 1, 2, "", "", std::nullopt},',
    '+    {"h-with-a-count", "alpha", "$3h", "alpha", 1, 3, "", "", std::nullopt},',
])


class HeaderDiff(unittest.TestCase):
    def test_metadata_lines_and_added_fixtures_are_not_protected_differences(self):
        result = PROTECTED.classify_header_diff(METADATA_ONLY)
        self.assertEqual(result["metadata"], 2)
        self.assertEqual(result["deleted"], [])
        self.assertEqual(result["changed"], [])
        self.assertEqual(result["added"], [{"name": "text-object-new", "line": 1069}])

    def test_a_deleted_fixture_is_listed_with_its_base_line(self):
        result = PROTECTED.classify_header_diff(ONE_DELETED)
        self.assertEqual(result["metadata"], 0)
        self.assertEqual(result["deleted"], [{"name": "line-jump-G-crlf", "line": 416}])

    def test_a_rewritten_expectation_is_a_change_not_an_addition(self):
        result = PROTECTED.classify_header_diff(ONE_CHANGED)
        self.assertEqual(result["deleted"], [])
        self.assertEqual(result["added"], [])
        self.assertEqual(result["changed"], [{"name": "h-with-a-count", "line": 20}])


class FixtureElements(unittest.TestCase):
    BASE = [{"name": "a", "text": "abc", "keys": "x"}, {"name": "b", "text": "de", "keys": "dd"}]

    def test_changed_keys_are_a_change(self):
        head = [dict(self.BASE[0]), {"name": "b", "text": "de", "keys": "D"}]
        self.assertEqual(PROTECTED.compare_fixtures(self.BASE, head),
                         {"deleted": [], "changed": ["b"], "added": []})

    def test_order_and_added_elements_are_not_protected_differences(self):
        head = [{"keys": "dd", "text": "de", "name": "b"}, dict(self.BASE[0]),
                {"name": "c", "text": "", "keys": "i"}]
        self.assertEqual(PROTECTED.compare_fixtures(self.BASE, head),
                         {"deleted": [], "changed": [], "added": ["c"]})


class Scopes(unittest.TestCase):
    def test_the_scopes_array_of_head_is_read_without_the_contracts_array(self):
        source = (ROOT / "tests/unit/NibTests.cpp").read_text(encoding="utf-8")
        scopes = PROTECTED.parse_scopes(source)
        self.assertEqual(len(scopes), 19)
        self.assertEqual(len(set(scopes)), 19)
        self.assertIn("--vim-search-highlight", scopes)
        self.assertTrue(all(scope.startswith("--") for scope in scopes))

    def test_allow_takes_a_value_that_starts_with_dashes(self):
        arguments = PROTECTED.parse_arguments(
            ["--base", "main", "--allow", "--vim-dot", "--allow", "ex-settings"])
        self.assertEqual(arguments.allow, ["--vim-dot", "ex-settings"])
        self.assertEqual(arguments.head, "HEAD")
        self.assertFalse(arguments.build)


if __name__ == "__main__":
    unittest.main()
