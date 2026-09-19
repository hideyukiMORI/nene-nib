"""Positive and negative inputs for the linker-level symbol check (ARC-003 / ARC-007 / CPP-013)."""

import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("symbols", ROOT / "eng/symbols.py")
symbols = importlib.util.module_from_spec(spec)
spec.loader.exec_module(symbols)
ALLOWLIST = json.loads((ROOT / "eng/symbol-allowlist.json").read_text(encoding="utf-8"))
MODULES = json.loads((ROOT / "eng/architecture.json").read_text(encoding="utf-8"))["modules"]

NM_OUTPUT = """
TextBuffer.cpp.obj:
0000000000000000 T ?insert@TextBuffer@nenenib@@QEAAXH@Z
0000000000000000 R ??_C@_05MJPCMENP@notes?$AA@
0000000000000000 t local_helper
                 U memcpy
                 U _Xtime_get_ticks
                 U __imp_GetTickCount64
                 U __chkstk
                 U SomeUnknownLibraryCall

NoteLedger.cpp.obj:
0000000000000000 T ?count@NoteLedger@nenenib@@QEBAHXZ
                 U ?insert@TextBuffer@nenenib@@QEAAXH@Z
"""


class SymbolTests(unittest.TestCase):
    def test_parses_defined_and_undefined(self):
        defined, undefined = symbols.symbol_table(NM_OUTPUT)
        self.assertEqual({"?insert@TextBuffer@nenenib@@QEAAXH@Z", "??_C@_05MJPCMENP@notes?$AA@",
                          "?count@NoteLedger@nenenib@@QEBAHXZ"}, defined)
        self.assertEqual({"memcpy", "_Xtime_get_ticks", "__imp_GetTickCount64", "__chkstk",
                          "SomeUnknownLibraryCall", "?insert@TextBuffer@nenenib@@QEAAXH@Z"}, undefined)

    def test_archive_internal_symbols_resolve(self):
        tables = {"core": symbols.symbol_table(NM_OUTPUT)}
        self.assertNotIn("?insert@TextBuffer@nenenib@@QEAAXH@Z", symbols.unresolved("core", tables, MODULES))
        self.assertIn("memcpy", symbols.unresolved("core", tables, MODULES))

    def test_declared_dependency_symbols_resolve(self):
        tables = {"core": ({"?count@NoteLedger@nenenib@@QEBAHXZ"}, set()),
                  "application": (set(), {"?count@NoteLedger@nenenib@@QEBAHXZ", "memcpy"})}
        self.assertEqual({"memcpy"}, symbols.unresolved("application", tables, MODULES))

    def test_undeclared_module_symbols_do_not_resolve(self):
        tables = {"application": ({"?run@NibState@nenenib@@QEAAXXZ"}, set()),
                  "core": (set(), {"?run@NibState@nenenib@@QEAAXXZ"})}
        self.assertEqual({"?run@NibState@nenenib@@QEAAXXZ"}, symbols.unresolved("core", tables, MODULES))

    def test_allowed_symbols_pass(self):
        allowed = {"memcpy", "__chkstk", "__asan_init", "__ubsan_handle_add_overflow",
                   "_Asan_vector_should_annotate", "??2@YAPEAX_K@Z", "??3@YAXPEAX_K@Z",
                   "??_7type_info@@6B@", "?_Xlength_error@std@@YAXPEBD@Z", "_CxxThrowException",
                   "__CxxFrameHandler3", "__std_exception_copy", "__std_terminate", "_invoke_watson"}
        self.assertEqual([], symbols.classify(allowed, "core", ALLOWLIST))

    def test_stl_baseline_of_phase0_is_allowed(self):
        # out/phase0/results.json の L3-stl-baseline-undefined（14 個）がそのまま通ること。
        baseline = {"??2@YAPEAX_K@Z", "??3@YAXPEAX_K@Z", "??_7type_info@@6B@",
                    "?_Xlength_error@std@@YAXPEBD@Z", "_CxxThrowException", "__CxxFrameHandler3",
                    "__security_check_cookie", "__security_cookie", "__std_exception_copy",
                    "__std_exception_destroy", "_invoke_watson", "memcpy", "memmove", "memset"}
        self.assertEqual([], symbols.classify(baseline, "core", ALLOWLIST))

    def test_stl_vectorized_search_is_allowed(self):
        self.assertEqual([], symbols.classify({"__std_find_trivial_1", "__std_mismatch_1"}, "core", ALLOWLIST))

    def test_time_is_arc007(self):
        findings = symbols.classify({"_Xtime_get_ticks"}, "core", ALLOWLIST)
        self.assertEqual(1, len(findings))
        self.assertTrue(findings[0].startswith("ARC-007"))

    def test_ex_stl_dependencies_are_exact_and_deterministic(self):
        permitted = {"?_Large_power_data@std@@3QBIB", "__std_find_end_1", "__std_min_8u"}
        self.assertEqual([], symbols.classify(permitted, "core", ALLOWLIST))
        for forbidden in {"__std_find_end_2", "__std_min_f", "localeconv", "calloc",
                          "?_Init@locale@std@@CAPEAV_Locimp@12@_N@Z", "SomeUnknownLibraryCall"}:
            with self.subTest(symbol=forbidden):
                self.assertTrue(symbols.classify({forbidden}, "core", ALLOWLIST))

    def test_steady_clock_is_arc007(self):
        self.assertTrue(symbols.classify({"_Query_perf_counter"}, "core", ALLOWLIST)[0].startswith("ARC-007"))

    def test_win32_tick_is_arc007(self):
        self.assertTrue(symbols.classify({"__imp_GetTickCount64"}, "application", ALLOWLIST)[0].startswith("ARC-007"))

    def test_locale_is_arc007(self):
        self.assertTrue(symbols.classify({"?_Init@locale@std@@CAPEAV_Locimp@12@_N@Z"}, "core", ALLOWLIST)[0].startswith("ARC-007"))

    def test_unknown_is_arc003(self):
        self.assertTrue(symbols.classify({"SomeUnknownLibraryCall"}, "core", ALLOWLIST)[0].startswith("ARC-003"))

    def test_filesystem_is_arc003(self):
        self.assertTrue(symbols.classify({"__std_fs_get_stats"}, "core", ALLOWLIST)[0].startswith("ARC-003"))

    def test_product_prefix_is_not_a_free_pass(self):
        self.assertTrue(symbols.classify({"nenenib_now"}, "core", ALLOWLIST)[0].startswith("ARC-003"))

    def test_win32_import_in_core_is_arc003(self):
        self.assertTrue(symbols.classify({"__imp_CreateFileW"}, "core", ALLOWLIST)[0].startswith("ARC-003"))

    def test_thread_creation_is_cpp013(self):
        for symbol in ["_beginthreadex", "__imp_CreateThread", "_Mtx_lock", "_Cnd_wait", "_Thrd_join",
                       "__std_atomic_wait_direct", "_Init_thread_header", "_tls_index",
                       "?_Schedule_chore@details@Concurrency@@YAHPEAU_Threadpool_chore@12@@Z",
                       "__imp_WaitForSingleObject", "__imp_EnterCriticalSection", "__imp_PostMessageW"]:
            with self.subTest(symbol=symbol):
                findings = symbols.classify({symbol}, "core", ALLOWLIST)
                self.assertEqual(1, len(findings))
                self.assertTrue(findings[0].startswith("CPP-013"), findings)

    def test_partial_match_is_not_allowed(self):
        self.assertTrue(symbols.classify({"memcpy_s"}, "core", ALLOWLIST))


if __name__ == "__main__":
    unittest.main()
