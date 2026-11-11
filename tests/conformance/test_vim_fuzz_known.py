import importlib.util
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("vim_fuzz", ROOT / "eng/vim-fuzz.py")
vim_fuzz = importlib.util.module_from_spec(spec)
sys.modules["vim_fuzz"] = vim_fuzz
spec.loader.exec_module(vim_fuzz)


def judged(words, first_visible=2, line=9, visible_lines=12, fields=("line",), shrunk=True,
           scrolloff=2):
    window = {"scrolloff": scrolloff,
              "viewport": {"visible_lines": visible_lines, "first_visible": first_visible,
                           "line": line, "column": 1}}
    return {"words": list(words), "fields": set(fields), "shrunk": shrunk, "window": window}


class StaleBottomAfterZbTests(unittest.TestCase):
    """#1483: the known rule for the old `w_botline` that `zb` / `z-` leave inside one `:normal!`."""

    def known(self, case):
        return vim_fuzz.default_known(case)

    def test_each_reader_after_zb_and_z_minus_is_known(self):
        for align in (("zb", "zb"), ("z-", "z-"), ("zb", "9zb"), ("z-", "9z-")):
            for reader in vim_fuzz.STALE_BOTTOM_READERS:
                with self.subTest(align=align, reader=reader):
                    self.assertEqual(self.known(judged([align, (reader, reader)])), "1483")

    def test_a_count_other_than_the_line_is_not_known(self):
        # 回数の行へ動く `5zb` は 1 本の `:normal!` でも古い下端を残さない（PR #1504 のレビュー・
        # 先頭 10・15 行目の `5zb<C-b>` は 3 つの形とも 5 / 1）。
        for align in (("zb", "5zb"), ("z-", "5z-"), ("zb", "8zb")):
            for reader in vim_fuzz.STALE_BOTTOM_READERS:
                with self.subTest(align=align, reader=reader):
                    self.assertIsNone(self.known(judged([align, (reader, reader)])))

    def test_scrolloff_zero_keeps_the_caret_on_c_b_and_c_y(self):
        for reader in ("<C-b>", "<C-y>"):
            with self.subTest(reader=reader):
                self.assertIsNone(self.known(judged([("zb", "zb"), (reader, reader)], scrolloff=0)))
        for reader in ("<C-f>", "L", "M"):
            with self.subTest(reader=reader):
                case = judged([("zb", "zb"), (reader, reader)], scrolloff=0)
                self.assertEqual(self.known(case), "1483")

    def test_scrolloff_that_keeps_the_top_off_line_one_is_not_known(self):
        # 高さ 12・`so=2` の 11 行目は先頭 2 で止まり、`so=3` の 10 行目も同じ（3 つの形が一致）。
        self.assertIsNone(self.known(judged([("zb", "zb"), ("L", "L")], line=11)))
        self.assertIsNone(self.known(judged([("zb", "zb"), ("L", "L")], line=10, scrolloff=3)))
        # 余白は窓の半分で頭打ち（`so=20` は高さ 12 で 5）: 7 行目は既知、8 行目は既知でない。
        self.assertEqual(self.known(judged([("zb", "zb"), ("L", "L")], line=7, scrolloff=20)),
                         "1483")
        self.assertIsNone(self.known(judged([("zb", "zb"), ("L", "L")], line=8, scrolloff=20)))
        # 行と余白でちょうど窓を満たす形は残る（高さ 12・`so=2` の 10 行目）。
        self.assertEqual(self.known(judged([("zb", "zb"), ("L", "L")], line=10)), "1483")

    def test_m_one_row_above_the_last_is_not_known(self):
        self.assertIsNone(self.known(judged([("zb", "zb"), ("M", "M")], line=11, scrolloff=0)))
        self.assertEqual(self.known(judged([("zb", "zb"), ("M", "M")], line=10, scrolloff=0)),
                         "1483")

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
        self.assertIsNone(self.known(judged([("zb", "20zb"), ("L", "L")], line=20)))

    def test_other_keys_and_unshrunk_cases_are_not_known(self):
        self.assertIsNone(self.known(judged([("zt", "zt"), ("<C-b>", "<C-b>")])))
        self.assertIsNone(self.known(judged([("zb", "zb"), ("H", "H")])))
        self.assertIsNone(self.known(judged([("zb", "zb"), ("<C-b>", "<C-b>")], shrunk=False)))

    def test_a_text_difference_is_not_known(self):
        case = judged([("zb", "zb"), ("<C-b>", "<C-b>")], fields=("line", "text"))
        self.assertIsNone(self.known(case))


if __name__ == "__main__":
    unittest.main()
