import importlib.util
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("vim_fuzz", ROOT / "eng/vim-fuzz.py")
vim_fuzz = importlib.util.module_from_spec(spec)
sys.modules["vim_fuzz"] = vim_fuzz
spec.loader.exec_module(vim_fuzz)


def judged(words, first_visible=2, line=9, visible_lines=12, fields=("line",), shrunk=True):
    window = {"scrolloff": 2,
              "viewport": {"visible_lines": visible_lines, "first_visible": first_visible,
                           "line": line, "column": 1}}
    return {"words": list(words), "fields": set(fields), "shrunk": shrunk, "window": window}


class StaleBottomAfterZbTests(unittest.TestCase):
    """#1483: the known rule for the old `w_botline` that `zb` / `z-` leave inside one `:normal!`."""

    def known(self, case):
        return vim_fuzz.default_known(case)

    def test_each_reader_after_zb_and_z_minus_is_known(self):
        for align in (("zb", "zb"), ("z-", "z-"), ("zb", "5zb")):
            for reader in vim_fuzz.STALE_BOTTOM_READERS:
                with self.subTest(align=align, reader=reader):
                    self.assertEqual(self.known(judged([align, (reader, reader)])), "1483")

    def test_window_and_caret_fields_are_known(self):
        case = judged([("zb", "zb"), ("<C-f>", "<C-f>")], fields=("line", "first_visible"))
        self.assertEqual(self.known(case), "1483")

    def test_a_refreshing_key_between_is_not_known(self):
        for between in ("j", "k", "l"):
            with self.subTest(between=between):
                case = judged([("zb", "zb"), (between, between), ("<C-b>", "<C-b>")])
                self.assertIsNone(self.known(case))

    def test_top_already_on_line_one_is_not_known(self):
        self.assertIsNone(self.known(judged([("zb", "zb"), ("<C-b>", "<C-b>")], first_visible=1)))

    def test_zb_on_the_last_row_or_below_is_not_known(self):
        self.assertIsNone(self.known(judged([("zb", "zb"), ("L", "L")], line=12)))
        self.assertIsNone(self.known(judged([("zb", "20zb"), ("L", "L")])))

    def test_other_keys_and_unshrunk_cases_are_not_known(self):
        self.assertIsNone(self.known(judged([("zt", "zt"), ("<C-b>", "<C-b>")])))
        self.assertIsNone(self.known(judged([("zb", "zb"), ("H", "H")])))
        self.assertIsNone(self.known(judged([("zb", "zb"), ("<C-b>", "<C-b>")], shrunk=False)))

    def test_a_text_difference_is_not_known(self):
        case = judged([("zb", "zb"), ("<C-b>", "<C-b>")], fields=("line", "text"))
        self.assertIsNone(self.known(case))


if __name__ == "__main__":
    unittest.main()
