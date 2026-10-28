"""Positive and negative inputs for the repository conformance checker (QLT-007)."""

import datetime
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "eng"))
import conformance as cnf

spec = importlib.util.spec_from_file_location("git_conventions", ROOT / "eng/git-conventions.py")
git_conventions = importlib.util.module_from_spec(spec)
spec.loader.exec_module(git_conventions)
spec = importlib.util.spec_from_file_location("vim_oracle", ROOT / "eng/vim-oracle.py")
vim_oracle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vim_oracle)
CASE_MAPPING = cnf.case_mapping.case_mapping_header().encode("utf-8")
RULES = json.loads((ROOT / "eng/conformance-rules.json").read_text(encoding="utf-8"))
TODAY = datetime.date(2026, 9, 15)


class SourceChecks(unittest.TestCase):
    def ids(self, source, path="src/core/Color.hpp", waivers=None):
        return {f.rule for f in cnf.source_checks(path, source, RULES, waivers or {})}

    def details(self, source, path="src/core/Color.hpp"):
        return [f"{f.rule}: {f.detail}" for f in cnf.source_checks(path, source, RULES, {})]

    def test_cnf001_positive(self):
        self.assertFalse(self.ids('class Color {}; // class ColorHelper {};\n'))

    def test_cnf001_negative(self):
        self.assertIn("CNF-001", self.ids("class ColorHelper {};", "src/core/ColorHelper.hpp"))

    def test_cnf001_negative_snake(self):
        self.assertIn("CNF-001", self.ids("struct color_helper {};", "src/core/color_helper.hpp"))

    def test_cnf001_module(self):
        self.assertIn("CNF-001", self.ids("", "src/core/utils/Color.hpp"))

    def test_cnf001_alias(self):
        self.assertIn("CNF-001", self.ids("using ColorHelper = int;"))

    def test_cnf002_positive(self):
        self.assertNotIn("CNF-002", self.ids("class Color {};"))

    def test_cnf002_negative(self):
        self.assertIn("CNF-002", self.ids("class Color {}; class Theme {};"))

    def test_cnf002_filename(self):
        self.assertIn("CNF-002", self.ids("class Theme {};"))

    def test_cnf002_nested_positive(self):
        self.assertNotIn("CNF-002", self.ids("namespace nenenib { class Color { class Value {}; }; }"))

    def test_cnf002_namespace_negative(self):
        self.assertIn("CNF-002", self.ids("namespace nenenib { class Color {}; class Theme {}; }"))

    def test_cnf002_free_function_needs_no_prefix(self):
        # C++ では名前空間が接頭辞の役割を果たす。Folio の <stem>_ 規則は持ち込まない。
        source = "namespace nenenib\n{\nclass Color\n{\n};\nint read(const Color& color);\n}\n"
        self.assertNotIn("CNF-002", self.ids(source))

    def test_cnf003_quoted_pragma_positive(self):
        self.assertNotIn("CNF-003", self.ids('auto text = "#pragma warning(disable: 4062)";'))

    def test_cnf003_positive(self):
        waivers = {"WVR-0001": {"scope": "src/core/Color.hpp#Color", "rule": "CPP-003"}}
        self.assertNotIn("CNF-003", self.ids("// Waiver: WVR-0001\n// NOLINTNEXTLINE(clang-analyzer-core.NullDereference)\nclass Color {};", waivers=waivers))

    def test_cnf003_negative(self):
        self.assertIn("CNF-003", self.ids("// NOLINTNEXTLINE(check)\nclass Color {};"))

    def test_cnf003_broad(self):
        for source in ['#pragma clang diagnostic ignored "-Wswitch-enum"',
                       "#pragma warning(disable: 4062)",
                       '_Pragma("clang diagnostic push")',
                       "__pragma(warning(push))",
                       "// NOLINT",
                       "// NOLINTBEGIN(check)"]:
            with self.subTest(source=source):
                self.assertIn("CNF-003", self.ids(source))

    def test_cnf003_wrong_scope(self):
        waivers = {"WVR-0001": {"scope": "src/core/Other.hpp#Other", "rule": "CPP-003"}}
        self.assertIn("CNF-003", self.ids("// Waiver: WVR-0001\n// NOLINTNEXTLINE(check)", waivers=waivers))

    def test_arc007_negative(self):
        for path in ["src/core/Color.cpp", "tests/build/Color.cpp", "src/ui/win32/Window.cpp"]:
            with self.subTest(path=path):
                self.assertIn("ARC-007", self.ids("auto t = std::chrono::system_clock::now();", path))

    def test_arc007_adapter_positive(self):
        self.assertNotIn("ARC-007", self.ids("auto t = std::chrono::system_clock::now();", "src/adapters/win32/Clock.cpp"))

    def test_arc007_literals_positive(self):
        self.assertNotIn("ARC-007", self.ids('auto s = R"tag(system_clock::now())tag"; // getenv()'))

    def test_arc003_negative(self):
        for header in ["<windows.h>", "<filesystem>", "<iostream>", "<dwrite.h>", "<d2d1_1.h>", "<locale>"]:
            with self.subTest(header=header):
                self.assertIn("ARC-003", self.ids(f"#include {header}", "src/core/Color.cpp"))

    def test_arc003_adapter_positive(self):
        self.assertNotIn("ARC-003", self.ids("#include <windows.h>", "src/adapters/win32/FileStore.cpp"))

    # CNF-009 — 並行性ヘッダは src/adapters/win32/ だけ、SIMD ヘッダは src/core/simd/ だけ。
    def test_cnf009_concurrency_in_core_is_cpp013(self):
        for header in ["<thread>", "<mutex>", "<atomic>", "<future>", "<stop_token>", "<process.h>", "<synchapi.h>"]:
            with self.subTest(header=header):
                self.assertIn("CPP-013", self.ids(f"#include {header}", "src/core/Worker.hpp"))

    def test_cnf009_concurrency_in_ui_is_cpp013(self):
        self.assertIn("CPP-013", self.ids("#include <thread>", "src/ui/win32/Window.cpp"))

    def test_cnf009_concurrency_in_app_is_cpp013(self):
        self.assertIn("CPP-013", self.ids("#include <atomic>", "src/app/Main.cpp"))

    def test_cnf009_concurrency_in_worker_adapter_passes(self):
        self.assertNotIn("CPP-013", self.ids("#include <thread>", "src/adapters/win32/Worker.cpp"))

    def test_cnf009_concurrency_in_tests_passes(self):
        for path in ["tests/unit/WorkerTests.cpp", "tests/build/ToolchainSmoke.cpp"]:
            with self.subTest(path=path):
                self.assertNotIn("CPP-013", self.ids("#include <thread>", path))

    def test_cnf009_simd_in_simd_module_passes(self):
        self.assertNotIn("CPP-018", self.ids("#include <immintrin.h>", "src/core/simd/Blend.cpp"))

    def test_cnf009_simd_outside_simd_module_is_cpp018(self):
        for path in ["src/core/Text.cpp", "src/application/Frame.cpp", "src/adapters/win32/Worker.cpp",
                     "src/ui/win32/Window.cpp"]:
            with self.subTest(path=path):
                self.assertIn("CPP-018", self.ids("#include <immintrin.h>", path))

    def test_cnf009_simd_in_tests_passes(self):
        self.assertNotIn("CPP-018", self.ids("#include <immintrin.h>", "tests/unit/BlendTests.cpp"))

    # CNF-009 — md4c.h を読むのは src/core/MarkdownParse.cpp だけ（ADR 0151 の決定 5・#765）。
    def test_cnf009_md4c_in_markdown_parse_passes(self):
        for include in ["#include <md4c.h>", '#include "md4c.h"']:
            with self.subTest(include=include):
                self.assertNotIn("ARC-002", self.ids(include, "src/core/MarkdownParse.cpp"))

    def test_cnf009_md4c_elsewhere_is_arc002(self):
        for path in ["src/core/MarkdownBuilder.cpp", "src/core/MarkdownParse.hpp", "src/application/Preview.cpp",
                     "src/adapters/win32/MarkdownWorker.cpp", "src/ui/win32/PreviewRenderer.cpp"]:
            with self.subTest(path=path):
                self.assertIn("ARC-002", self.ids("#include <md4c.h>", path))

    def test_cnf009_md4c_through_a_directory_is_arc002(self):
        self.assertIn("ARC-002", self.ids('#include "../../third_party/md4c/md4c.h"', "src/core/Text.cpp"))

    def test_cnf009_md4c_in_tests_passes(self):
        self.assertNotIn("ARC-002", self.ids("#include <md4c.h>", "tests/unit/MarkdownTests.cpp"))

    def test_cnf009_names_the_parser_header(self):
        self.assertIn("ARC-002: parser header md4c.h outside src/core/MarkdownParse.cpp",
                      self.details("#include <md4c.h>", "src/core/Text.cpp"))

    def test_cnf009_names_the_header(self):
        self.assertIn("CPP-013: concurrency header thread outside src/adapters/win32/",
                      self.details("#include <thread>", "src/core/Worker.hpp"))


def sdk_abi_source(mark="// SDK-ABI: IDWriteTextRenderer::DrawUnderline",
                   suppression="// NOLINTNEXTLINE(readability-function-size)",
                   name="DrawUnderline", body_lines=1):
    body = "".join("    return E_NOTIMPL;\n" for _ in range(body_lines))
    return (f"namespace nenenib::ui::win32\n{{\n{mark}\n{suppression}\n"
            f"HRESULT STDMETHODCALLTYPE BodyGlyphCollector::{name}(void *, FLOAT, FLOAT,\n"
            f"                                                const DWRITE_UNDERLINE *,\n"
            f"                                                IUnknown *) noexcept\n"
            f"{{\n{body}}}\n}} // namespace nenenib::ui::win32\n")


class SdkAbiChecks(unittest.TestCase):
    """CNF-012: the SDK-ABI mark stands for a waiver only at a thin boundary (CPP-019・ADR 0076)."""

    SIGNATURES = cnf.sdk_abi_signatures(ROOT)
    PATH = "src/ui/win32/BodyGlyphCollector.cpp"

    def findings(self, source, path=PATH):
        return [str(f) for f in cnf.source_checks(path, source, RULES, {}, self.SIGNATURES)]

    def assert_cnf012(self, source, detail, path=PATH):
        found = self.findings(source, path)
        self.assertTrue(any(f.startswith("CNF-012") and detail in f for f in found), found)

    def test_cnf012_positive(self):
        self.assertEqual([], self.findings(sdk_abi_source()))

    def test_cnf012_six_body_lines_pass(self):
        self.assertEqual([], self.findings(sdk_abi_source(body_lines=6)))

    def test_cnf012_tests_ui_passes(self):
        self.assertEqual([], self.findings(sdk_abi_source(), "tests/ui/FakeRenderer.cpp"))

    def test_cnf012_early_return_boundary_passes(self):
        source = sdk_abi_source().replace(
            "    return E_NOTIMPL;\n",
            "    if (effect != nullptr)\n    {\n        return E_NOTIMPL;\n    }\n    return collect();\n")
        self.assertEqual([], self.findings(source))

    def test_cnf012_method_not_in_table(self):
        self.assert_cnf012(sdk_abi_source(mark="// SDK-ABI: IDWriteTextRenderer::DrawSomething",
                                          name="DrawSomething"),
                           "is not in eng/sdk-abi-signatures.json")

    def test_cnf012_definition_name_differs(self):
        self.assert_cnf012(sdk_abi_source(name="DrawStrikethrough"), "<Class>::DrawUnderline(")

    def test_cnf012_seven_body_lines(self):
        self.assert_cnf012(sdk_abi_source(body_lines=7), "body has 7 lines; at most 6")

    def test_cnf012_body_counts_comment_and_blank_lines(self):
        source = sdk_abi_source(body_lines=5).replace("{\n    return", "{\n\n    // note\n    return", 1)
        self.assert_cnf012(source, "body has 7 lines")

    def test_cnf012_suppression_mixes_another_check(self):
        source = sdk_abi_source(suppression="// NOLINTNEXTLINE(readability-function-size,bugprone-x)")
        self.assert_cnf012(source, "the next line must be exactly")
        self.assertIn("CNF-003", {f.split(":")[0] for f in self.findings(source)})

    def test_cnf012_wrong_place(self):
        self.assert_cnf012(sdk_abi_source(), "allowed only under", "src/application/Renderer.cpp")

    def test_cnf012_loose_mark_grammar(self):
        for mark in ["// SDK-ABI:  IDWriteTextRenderer::DrawUnderline",
                     "// SDK-ABI: IDWriteTextRenderer::DrawUnderline ",
                     "    // SDK-ABI: IDWriteTextRenderer::DrawUnderline",
                     "// SDK-ABI: DrawUnderline"]:
            with self.subTest(mark=mark):
                self.assert_cnf012(sdk_abi_source(mark=mark), "the mark must be exactly")

    def test_cnf012_declaration_without_body(self):
        source = ("// SDK-ABI: IDWriteTextRenderer::DrawUnderline\n"
                  "// NOLINTNEXTLINE(readability-function-size)\n"
                  "HRESULT BodyGlyphCollector::DrawUnderline(void *, FLOAT) noexcept;\n")
        self.assert_cnf012(source, "has no definition body")

    def test_cnf003_unmarked_suppression_still_needs_a_waiver(self):
        source = sdk_abi_source(mark="// Boundary of IDWriteTextRenderer")
        self.assertIn("CNF-003", {f.split(":")[0] for f in self.findings(source)})


class RepositoryChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def write(self, path, text):
        file = self.root / path
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(text, encoding="utf-8")

    def paths(self):
        return [p.relative_to(self.root) for p in self.root.rglob("*") if p.is_file()]

    def seed_waiver(self, expires="2026-09-16"):
        self.write("src/core/Color.hpp", "class Color {};")
        self.write("docs/waivers/README.md", "[WVR-0001](WVR-0001-color.md)")
        self.write("docs/waivers/WVR-0001-color.md", f"- Status: active\n- Rule: CPP-003\n- Issue: #1\n- Owner: hide\n- Created: 2026-09-01\n- Expires: {expires}\n- Scope: src/core/Color.hpp#Color\n")

    def test_cnf004_positive(self):
        self.seed_waiver()
        errors, valid = cnf.waiver_checks(self.root, self.paths(), TODAY)
        self.assertEqual([], errors)
        self.assertIn("WVR-0001", valid)

    def test_cnf004_expired(self):
        self.seed_waiver("2026-09-14")
        errors, valid = cnf.waiver_checks(self.root, self.paths(), TODAY)
        self.assertIn("CNF-004", {f.rule for f in errors})
        self.assertNotIn("WVR-0001", valid)

    def test_cnf004_expiry_day_valid(self):
        self.seed_waiver("2026-09-15")
        self.assertFalse(cnf.waiver_checks(self.root, self.paths(), TODAY)[0])

    def test_cnf004_missing_field(self):
        self.seed_waiver()
        self.write("docs/waivers/WVR-0001-color.md", "- Status: active")
        self.assertTrue(cnf.waiver_checks(self.root, self.paths(), TODAY)[0])

    def test_cnf004_stale_index(self):
        self.write("docs/waivers/README.md", "WVR-0001")
        self.assertTrue(cnf.waiver_checks(self.root, self.paths(), TODAY)[0])

    def test_cnf004_bad_scope_is_not_valid(self):
        self.seed_waiver()
        path = self.root / "docs/waivers/WVR-0001-color.md"
        path.write_text(path.read_text(encoding="utf-8").replace("src/core/Color.hpp#Color", "../outside.hpp#Color"), encoding="utf-8")
        errors, valid = cnf.waiver_checks(self.root, self.paths(), TODAY)
        self.assertTrue(errors)
        self.assertNotIn("WVR-0001", valid)

    def seed_config(self):
        self.write("eng/config-bindings.json", '{".clang-tidy": ["CMakeLists.txt"]}')
        self.write(".clang-tidy", "WarningsAsErrors: '*'\n")
        self.write("CMakeLists.txt", '# read .clang-tidy\n')

    def config_ids(self):
        return {f.rule for f in cnf.configuration_checks(self.root, self.paths(), RULES)}

    def test_cnf005_positive(self):
        self.seed_config()
        self.assertFalse(self.config_ids())

    def test_cnf005_filename(self):
        self.seed_config()
        self.write("baseline.json", "{}")
        self.assertIn("CNF-005", self.config_ids())

    def test_cnf005_option(self):
        self.seed_config()
        for option in ["add_compile_options(/WX-)", "add_compile_options(/wd4062)",
                       "add_compile_options(-Wno-error)", "add_compile_options(-w)"]:
            with self.subTest(option=option):
                self.write("eng/options.cmake", option)
                self.assertIn("CNF-005", self.config_ids())

    def test_cnf005_named_warning_switch_is_not_disabling(self):
        self.seed_config()
        self.write("eng/options.cmake", "add_compile_options(-Wno-switch-default -Wno-language-extension-token -Wswitch-enum)")
        self.assertNotIn("CNF-005", self.config_ids())

    def test_cnf007_positive(self):
        self.seed_config()
        self.assertNotIn("CNF-007", self.config_ids())

    def test_cnf007_missing_binding(self):
        self.seed_config()
        self.write("CMakeLists.txt", "project(test)")
        self.assertIn("CNF-007", self.config_ids())

    def test_cnf007_extra_config(self):
        self.seed_config()
        self.write("src/.clang-tidy", "Checks: '*'")
        self.assertIn("CNF-007", self.config_ids())

    def test_cnf008_positive(self):
        self.seed_config()
        self.write("src/Color.cpp", "// TODO #1: implement color")
        self.assertNotIn("CNF-008", self.config_ids())

    def test_cnf008_negative(self):
        self.seed_config()
        self.write("src/Color.cpp", "// FIXME: implement color")
        self.assertIn("CNF-008", self.config_ids())

    def seed_version_metadata(self):
        self.write(
            "CMakeLists.txt",
            "configure_file(src/app/NeNeNib.manifest.in generated/NeNeNib.manifest @ONLY)\n"
            "configure_file(src/app/NeNeNibVersion.h.in generated/NeNeNibVersion.h @ONLY)\n",
        )
        self.write(
            "src/app/NeNeNib.manifest.in",
            '<assemblyIdentity version="@PROJECT_VERSION_MAJOR@.@PROJECT_VERSION_MINOR@.@PROJECT_VERSION_PATCH@.0"/>',
        )
        self.write(
            "src/app/NeNeNibVersion.h.in",
            "@PROJECT_VERSION_MAJOR@, @PROJECT_VERSION_MINOR@, @PROJECT_VERSION_PATCH@, 0\n"
            '"@PROJECT_VERSION_MAJOR@.@PROJECT_VERSION_MINOR@.@PROJECT_VERSION_PATCH@.0\\0"\n'
            '"@PROJECT_VERSION@\\0"\n',
        )

    def test_cnf007_version_metadata_positive(self):
        self.seed_version_metadata()
        self.assertFalse(cnf.version_metadata_checks(self.root))

    def test_cnf007_fixed_manifest_negative(self):
        self.seed_version_metadata()
        self.write("src/app/NeNeNib.manifest", '<assemblyIdentity version="0.1.0.0"/>')
        self.assertTrue(cnf.version_metadata_checks(self.root))

    def test_cnf007_literal_manifest_version_negative(self):
        self.seed_version_metadata()
        self.write("src/app/NeNeNib.manifest.in", '<assemblyIdentity version="0.1.0.0"/>')
        self.assertTrue(cnf.version_metadata_checks(self.root))

    # CNF-010 — 生成物 VimFixtures.hpp が読んだ fixtures.json の SHA-256 と本数を名乗る。
    # 正例は oracle の header() が書いた行をそのまま検査器に読ませる（書き方と読み方を機械で結ぶ）。
    def vim_records(self, count):
        return [{"name": f"fixture-{number}", "text": "ab", "keys": "x", "expected": "b",
                 "line": 1, "column": 1, "register": "a", "register_kind": "v"}
                for number in range(count)]

    def seed_vim_fixtures(self, sources=1, recorded=None):
        source = [{key: record[key] for key in ("name", "text", "keys")} for record in self.vim_records(sources)]
        self.write("tests/vim/fixtures.json", json.dumps(source))
        digest = hashlib.sha256((self.root / "tests/vim/fixtures.json").read_bytes()).hexdigest()
        records = self.vim_records(sources if recorded is None else recorded)
        self.write("tests/vim/VimFixtures.hpp", vim_oracle.header(records, "VIM 9.1", digest))

    def fixture_details(self):
        return [f"{f.rule}: {f.detail}" for f in cnf.fixture_digest_checks(self.root, RULES)]

    def test_cnf010_positive(self):
        self.seed_vim_fixtures()
        self.assertEqual([], self.fixture_details())

    def test_cnf010_digest_mismatch(self):
        self.seed_vim_fixtures()
        self.write("tests/vim/fixtures.json", json.dumps([{"name": "fixture-0", "text": "abc", "keys": "x"}]))
        self.assertTrue(any("CNF-010" in detail and "digest" in detail for detail in self.fixture_details()))

    def test_cnf010_count_mismatch(self):
        self.seed_vim_fixtures(sources=1, recorded=2)
        self.assertTrue(any("CNF-010" in detail and "count 2 is not 1" in detail for detail in self.fixture_details()))

    def test_cnf010_missing_record(self):
        self.seed_vim_fixtures()
        path = self.root / "tests/vim/VimFixtures.hpp"
        kept = [line for line in path.read_text(encoding="utf-8").splitlines() if "sha256" not in line]
        path.write_text("\n".join(kept), encoding="utf-8")
        self.assertTrue(any("no digest" in detail for detail in self.fixture_details()))

    def test_cnf010_missing_files(self):
        self.assertTrue(any("missing" in detail for detail in self.fixture_details()))

    # CNF-010 — 生成物 VimKeyNames.hpp は oracle の KEY_TABLE から key_names_header() が作る全文と
    # バイト一致する（ADR 0054 の決定 6）。正例は同じ関数の出力をそのまま読ませる。
    def key_names_details(self):
        return [f"{f.rule}: {f.detail}" for f in cnf.key_names_checks(self.root)]

    def seed_key_names(self, content=None):
        path = self.root / "tests/vim/VimKeyNames.hpp"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(vim_oracle.key_names_header().encode("utf-8") if content is None else content)

    def test_cnf010_key_names_positive(self):
        self.seed_key_names()
        self.assertEqual([], self.key_names_details())

    def test_cnf010_key_names_stored_file_positive(self):
        stored = (ROOT / "tests/vim/VimKeyNames.hpp").read_bytes()
        self.seed_key_names(stored)
        self.assertEqual([], self.key_names_details())

    def test_cnf010_key_names_edited_row_negative(self):
        generated = vim_oracle.key_names_header().encode("utf-8")
        cases = {
            "wrong key": generated.replace(b"VimSpecialKey::arrow_left}", b"VimSpecialKey::arrow_right}", 1),
            "dropped row": generated.replace(b'    {"<Down>", nenenib::core::VimSpecialKey::arrow_down},\n', b"", 1),
            "added row": generated.replace(b"}};\n", b'    {"<Tab>", nenenib::core::VimCharacter{U\'\\x09\'}},\n}};\n', 1),
            "CRLF": generated.replace(b"\n", b"\r\n"),
        }
        for name, content in cases.items():
            with self.subTest(case=name):
                self.assertNotEqual(generated, content)
                self.seed_key_names(content)
                details = self.key_names_details()
                self.assertTrue(details)
                self.assertTrue(all(detail.startswith("CNF-010") for detail in details))
        self.seed_key_names(cases["wrong key"])
        self.assertTrue(any("--key-names" in detail for detail in self.key_names_details()))

    def test_cnf010_key_names_missing_negative(self):
        self.assertTrue(any("CNF-010" in detail and "missing" in detail for detail in self.key_names_details()))

    # CNF-010 — 生成物 CaseMappingTable.hpp は eng/case-mapping.py の case_mapping_header() が作る全文と
    # バイト一致する（Issue #781）。正例は同じ関数の出力と保存されているファイルをそのまま読ませる。
    def case_mapping_details(self):
        return [f"{f.rule}: {f.detail}" for f in cnf.case_mapping_checks(self.root)]

    def seed_case_mapping(self, content):
        path = self.root / "src/core/CaseMappingTable.hpp"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)

    def test_cnf010_case_mapping_positive(self):
        self.seed_case_mapping(CASE_MAPPING)
        self.assertEqual([], self.case_mapping_details())
        self.seed_case_mapping((ROOT / "src/core/CaseMappingTable.hpp").read_bytes())
        self.assertEqual([], self.case_mapping_details())

    def test_cnf010_case_mapping_edited_negative(self):
        cases = {
            "wrong delta": CASE_MAPPING.replace(b"{U'\\x61', U'\\x7A', 1, -32},", b"{U'\\x61', U'\\x7A', 1, -31},", 1),
            "dropped row": CASE_MAPPING.replace(b"    {U'\\xB5', U'\\xB5', 1, 743},\n", b"", 1),
            "other version": CASE_MAPPING.replace(b"// Unicode ", b"// Unicode 9", 1),
            "CRLF": CASE_MAPPING.replace(b"\n", b"\r\n"),
        }
        for name, content in cases.items():
            with self.subTest(case=name):
                self.assertNotEqual(CASE_MAPPING, content)
                self.seed_case_mapping(content)
                details = self.case_mapping_details()
                self.assertTrue(details)
                self.assertTrue(all(detail.startswith("CNF-010") and "--write" in detail for detail in details))

    def test_cnf010_case_mapping_missing_negative(self):
        self.assertTrue(any("CNF-010" in detail and "missing" in detail for detail in self.case_mapping_details()))

    # CNF-011 — fixtures.json は生成器が書く 1 つの形（1 行 1 件）だけを許す（Issue #98）。
    # 正例は oracle の canonical_fixtures_json が書いたバイト列そのままを読ませる。
    def vim_fixtures(self):
        return [{"name": "fixture-0", "text": "ab", "keys": "x"},
                {"name": "fixture-1", "text": "あ", "keys": "y", "settings": ["set expandtab"]},
                {"name": "fixture-2", "text": "ab", "keys": "z",
                 "viewport": {"visible_lines": 10, "first_visible": 6, "line": 10, "column": 1}},
                {"name": "fixture-3", "text": "ab", "keys": "@a",
                 "register": {"name": "a", "keys": "x"}}]

    def write_fixture_bytes(self, content):
        path = self.root / "tests/vim/fixtures.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)

    def format_details(self):
        return [f"{f.rule}: {f.detail}" for f in cnf.fixture_format_checks(self.root)]

    def test_cnf011_positive(self):
        self.write_fixture_bytes(vim_oracle.canonical_fixtures_json(self.vim_fixtures()))
        self.assertEqual([], self.format_details())

    def test_cnf011_negatives(self):
        fixtures = self.vim_fixtures()
        canonical = vim_oracle.canonical_fixtures_json(fixtures)
        first = b'{"name":"fixture-0","text":"ab","keys":"x"}'
        cases = {
            "indent": (json.dumps(fixtures, ensure_ascii=False, indent=2) + "\n").encode("utf-8"),
            "one line": (json.dumps(fixtures, ensure_ascii=False,
                                    separators=(",", ":")) + "\n").encode("utf-8"),
            "key order": canonical.replace(first, b'{"text":"ab","name":"fixture-0","keys":"x"}'),
            "empty settings": canonical.replace(
                first, b'{"name":"fixture-0","text":"ab","keys":"x","settings":[]}'),
            "escaped non-ASCII": canonical.replace("あ".encode("utf-8"), b"\\u3042"),
            "spaces after the separators": canonical.replace(first, first.replace(b",", b", ")),
            "CRLF": canonical.replace(b"\n", b"\r\n"),
            "no trailing newline": canonical[:-1],
        }
        for name, content in cases.items():
            with self.subTest(case=name):
                self.write_fixture_bytes(content)
                details = self.format_details()
                self.assertTrue(all("CNF-011" in detail for detail in details))
                self.assertTrue(details, "the canonical form must not accept this")
        self.write_fixture_bytes(cases["CRLF"])
        self.assertTrue(any("CRLF" in detail for detail in self.format_details()))
        self.write_fixture_bytes(cases["no trailing newline"])
        self.assertTrue(any("no newline at the end" in detail for detail in self.format_details()))

    def test_cnf011_names_the_first_wrong_line(self):
        lines = vim_oracle.canonical_fixtures_json(self.vim_fixtures()).split(b"\n")
        lines[2] = lines[2].replace(b'  {"name":"fixture-1"', b'   {"name":"fixture-1"')
        self.write_fixture_bytes(b"\n".join(lines))
        self.assertTrue(any("line 3" in detail for detail in self.format_details()))

    def test_cnf011_unreadable_fixtures(self):
        for content in [b"{}\n", b'[{"name":"a","text":"b","keys":"c","note":"d"}]\n',
                        b'[{"name":"a","text":"b"}]\n', b"[\n",
                        b'[{"name":"a","text":"b","keys":"c","viewport":{"line":1}}]\n',
                        b'[{"name":"a","text":"b","keys":"qaxq@a"}]\n']:
            with self.subTest(content=content):
                self.write_fixture_bytes(content)
                self.assertTrue(any("CNF-011" in detail for detail in self.format_details()))

    def test_cnf011_missing_file(self):
        self.assertTrue(any("missing" in detail for detail in self.format_details()))

    def test_vim_viewport_header_row(self):
        record = self.vim_records(1)[0]
        record["viewport"] = {"visible_lines": 10, "first_visible": 6, "line": 10,
                              "column": 1, "expected_first_visible": 11,
                              "expected_scroll_lines": 5}
        rendered = vim_oracle.header([record], "VIM 9.1", "digest")
        self.assertIn("VimViewportFixture{10, 6, 10, 1, 11, 5}", rendered)

    def test_vim_viewport_requires_exact_input(self):
        fixture = {"name": "viewport", "viewport": {"visible_lines": 10}}
        with self.assertRaises(ValueError):
            vim_oracle.viewport_of(fixture)

    def seed_docs(self):
        self.write("docs/QUALITY_GATES.md", "### CNF-006 — documents\n- 機械強制: **active**\n\n## 3. 強制マトリクス\n| CNF-006 | active | checker |\n\n## 4. Gates\n")
        self.write("docs/quality/gate-proofs.md", "| CNF-006 | test | passed |")

    def doc_errors(self):
        return cnf.document_checks(self.root, self.paths(), {"normativeFiles": ["docs/QUALITY_GATES.md"]})

    def test_cnf006_positive(self):
        self.seed_docs()
        self.assertFalse(self.doc_errors())

    def test_cnf006_undefined(self):
        self.seed_docs()
        self.write("README.md", "CPP-999")
        self.assertTrue(any("undefined" in f.detail for f in self.doc_errors()))

    def test_cnf006_placeholder(self):
        self.seed_docs()
        self.write("README.md", "{{TOOL}}")
        self.assertTrue(any("placeholder" in f.detail for f in self.doc_errors()))

    def test_cnf006_missing_proof(self):
        self.seed_docs()
        self.write("docs/quality/gate-proofs.md", "")
        self.assertTrue(any("proof" in f.detail for f in self.doc_errors()))

    def test_cnf006_duplicate(self):
        self.seed_docs()
        path = self.root / "docs/QUALITY_GATES.md"
        path.write_text(path.read_text(encoding="utf-8") + "\n### CNF-006 — duplicate\n- 機械強制: **active**\n", encoding="utf-8")
        self.assertTrue(any("duplicate" in f.detail for f in self.doc_errors()))

    def test_cnf006_status_mismatch(self):
        self.seed_docs()
        path = self.root / "docs/QUALITY_GATES.md"
        path.write_text(path.read_text(encoding="utf-8").replace("**active**", "**planned**"), encoding="utf-8")
        self.assertTrue(any("mismatch" in f.detail for f in self.doc_errors()))

    def test_arc002_graph_cycle(self):
        self.write("eng/architecture.json", json.dumps({"modules": {"core": {"path": "src/core", "dependencies": ["core"]}}, "runtimeDependencies": []}))
        self.assertIn("ARC-002", {f.rule for f in cnf.architecture_checks(self.root, self.paths(), None)})

    def test_arc002_relative_include(self):
        graph = json.loads((ROOT / "eng/architecture.json").read_text(encoding="utf-8"))
        self.write("eng/architecture.json", json.dumps(graph))
        self.write("src/core/Color.hpp", '#include "../ui/win32/Window.hpp"')
        self.write("src/ui/win32/Window.hpp", "class Window {};")
        self.assertIn("ARC-002", {f.rule for f in cnf.architecture_checks(self.root, self.paths(), None)})

    def seed_build_graph(self, include_unit):
        graph = json.loads((ROOT / "eng/architecture.json").read_text(encoding="utf-8"))
        self.write("eng/architecture.json", json.dumps(graph))
        self.write("tests/unit/Sample.cpp", "int main() { return 0; }")
        reply = "build/.cmake/api/v1/reply/"
        self.write(reply + "index-1.json", json.dumps({"reply": {"codemodel-v2": {"jsonFile": "model.json"}}}))
        targets = [{"id": "unit", "name": "unit", "jsonFile": "unit.json"}] if include_unit else []
        self.write(reply + "model.json", json.dumps({"configurations": [{"targets": targets}]}))
        self.write(reply + "unit.json", json.dumps({"name": "unit", "sources": [{"path": "tests/unit/Sample.cpp"}]}))

    def test_arc002_unit_translation_present(self):
        self.seed_build_graph(True)
        self.assertFalse(cnf.architecture_checks(self.root, self.paths(), self.root / "build"))

    def test_arc002_unit_translation_missing(self):
        self.seed_build_graph(False)
        findings = cnf.architecture_checks(self.root, self.paths(), self.root / "build")
        self.assertTrue(any(f.rule == "ARC-002" and "absent from the build" in f.detail for f in findings))

    # ADR 0151 の決定 14: File API の依存は推移的なので、宣言した依存の閉包と比べる。
    def seed_closure_graph(self, dependencies_of_top):
        modules = {"base": {"path": "src/base", "dependencies": []},
                   "middle": {"path": "src/middle", "dependencies": ["base"]},
                   "top": {"path": "src/top", "dependencies": ["middle"]},
                   "aside": {"path": "src/aside", "dependencies": []}}
        self.write("eng/architecture.json", json.dumps({"modules": modules, "runtimeDependencies": []}))
        reply = "build/.cmake/api/v1/reply/"
        self.write(reply + "index-1.json", json.dumps({"reply": {"codemodel-v2": {"jsonFile": "model.json"}}}))
        names = ["base", "middle", "top", "aside"]
        self.write(reply + "model.json", json.dumps({"configurations": [{"targets": [
            {"id": name, "name": name, "jsonFile": name + ".json"} for name in names]}]}))
        links = {"base": [], "middle": ["base"], "top": dependencies_of_top, "aside": []}
        for name in names:
            self.write(f"src/{name}/{name.capitalize()}.cpp", "int main() { return 0; }")
            self.write(reply + name + ".json", json.dumps({"name": name,
                       "sources": [{"path": f"src/{name}/{name.capitalize()}.cpp"}],
                       "dependencies": [{"id": d} for d in links[name]]}))

    def test_arc002_transitive_dependency_inside_closure_passes(self):
        self.seed_closure_graph(["middle", "base"])
        self.assertFalse(cnf.architecture_checks(self.root, self.paths(), self.root / "build"))

    def test_arc002_dependency_outside_closure_fails(self):
        self.seed_closure_graph(["middle", "base", "aside"])
        findings = cnf.architecture_checks(self.root, self.paths(), self.root / "build")
        self.assertEqual(["ARC-002: top: forbidden target dependency aside"], [str(f) for f in findings])

    # #1033: 同じ module の別の target への依存（試験の exe → 替え玉の exe）は境界の内側なので通す。
    def test_arc002_dependency_on_same_module_target_passes(self):
        self.seed_closure_graph(["middle", "helper"])
        reply = "build/.cmake/api/v1/reply/"
        model = json.loads((self.root / reply / "model.json").read_text(encoding="utf-8"))
        model["configurations"][0]["targets"].append({"id": "helper", "name": "helper", "jsonFile": "helper.json"})
        self.write(reply + "model.json", json.dumps(model))
        self.write("src/top/Helper.cpp", "int helper() { return 0; }")
        self.write(reply + "helper.json", json.dumps({"name": "helper", "sources": [{"path": "src/top/Helper.cpp"}],
                                                      "dependencies": []}))
        self.assertFalse(cnf.architecture_checks(self.root, self.paths(), self.root / "build"))


class ThirdPartyDigestChecks(unittest.TestCase):
    """CNF-013 — third_party/ の各ファイルは表の SHA-256 の上流のバイト列（ADR 0151）。

    照合するのは git の index の blob なので、試験は一時の git リポジトリに add してから読ませる。
    """

    BYTES = b"int md(void);\n"

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        subprocess.run(["git", "init", "-q"], cwd=self.root, check=True)
        subprocess.run(["git", "config", "core.autocrlf", "false"], cwd=self.root, check=True)

    def stage(self, path, content):
        file = self.root / path
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_bytes(content)
        subprocess.run(["git", "add", "--", path], cwd=self.root, check=True)

    def details(self, table, paths=None):
        rules = dict(RULES, thirdPartyDigests=table)
        listed = [p.relative_to(self.root) for p in self.root.rglob("*") if p.is_file() and ".git" not in p.parts]
        return [f"{f.rule}: {f.path}: {f.detail}"
                for f in cnf.third_party_digest_checks(self.root, listed if paths is None else paths, rules)]

    def test_cnf013_positive(self):
        self.stage("third_party/md/md.h", self.BYTES)
        self.assertEqual([], self.details({"third_party/md/md.h": hashlib.sha256(self.BYTES).hexdigest()}))

    def test_cnf013_digest_mismatch(self):
        self.stage("third_party/md/md.h", self.BYTES.replace(b"\n", b"\r\n"))
        details = self.details({"third_party/md/md.h": hashlib.sha256(self.BYTES).hexdigest()})
        self.assertTrue(any(d.startswith("CNF-013: third_party/md/md.h: SHA-256") for d in details))

    def test_cnf013_reads_the_index_not_the_working_tree(self):
        self.stage("third_party/md/md.h", self.BYTES)
        (self.root / "third_party/md/md.h").write_bytes(b"edited\n")
        self.assertEqual([], self.details({"third_party/md/md.h": hashlib.sha256(self.BYTES).hexdigest()}))

    def test_cnf013_listed_file_missing_from_index(self):
        details = self.details({"third_party/md/md.h": hashlib.sha256(self.BYTES).hexdigest()})
        self.assertTrue(any("not in the git index" in d for d in details))

    def test_cnf013_unlisted_file_under_third_party(self):
        self.stage("third_party/md/md.h", self.BYTES)
        self.stage("third_party/md/extra.c", self.BYTES)
        details = self.details({"third_party/md/md.h": hashlib.sha256(self.BYTES).hexdigest()})
        self.assertEqual(["CNF-013: third_party/md/extra.c: not in thirdPartyDigests of eng/conformance-rules.json"], details)

    def test_cnf013_files_outside_third_party_are_not_listed(self):
        self.stage("src/core/Color.hpp", b"class Color {};\n")
        self.assertEqual([], self.details({}))

    def test_cnf013_repository_table_matches_the_vendored_md4c(self):
        self.assertEqual([], [str(f) for f in cnf.third_party_digest_checks(ROOT, cnf.inventory(ROOT), RULES)])


class TranslationUnitSizeChecks(unittest.TestCase):
    """CNF-014 — src/ と tests/ の .cpp は 3,000 行以下（ADR 0158 の決定 6）。third_party/ は見ない。"""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def write(self, path, count):
        file = self.root / path
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text("int x;\n" * count, encoding="utf-8")
        return Path(path)

    def details(self, *paths):
        return [str(f) for f in cnf.translation_unit_size_checks(self.root, list(paths))]

    def test_cnf014_positive_at_the_limit(self):
        self.assertEqual([], self.details(self.write("src/core/Big.cpp", 3000),
                                          self.write("tests/unit/Big.cpp", 3000)))

    def test_cnf014_negative_in_src(self):
        self.assertEqual(["CNF-014: src/core/Big.cpp: 3001 lines > 3000"],
                         self.details(self.write("src/core/Big.cpp", 3001)))

    def test_cnf014_negative_in_tests(self):
        self.assertEqual(["CNF-014: tests/unit/Big.cpp: 4000 lines > 3000"],
                         self.details(self.write("tests/unit/Big.cpp", 4000)))

    def test_cnf014_third_party_and_headers_are_not_counted(self):
        self.assertEqual([], self.details(self.write("third_party/md4c/src/md4c.cpp", 7000),
                                          self.write("src/core/Big.hpp", 7000),
                                          self.write("eng/probes/Big.cpp", 7000)))

    def test_cnf014_repository_passes(self):
        self.assertEqual([], [str(f) for f in cnf.translation_unit_size_checks(ROOT, cnf.inventory(ROOT))])


class GitChecks(unittest.TestCase):
    def test_valid(self):
        self.assertEqual([], git_conventions.validate("build: 検査を固定する (#1)"))

    def test_missing_issue(self):
        self.assertTrue(git_conventions.validate("build: 検査を固定する"))

    def test_english_description(self):
        self.assertTrue(git_conventions.validate("build: enforce checks (#1)"))

    def test_breaking_footer(self):
        self.assertTrue(git_conventions.validate("build!: 契約を変える (#1)"))
        self.assertFalse(git_conventions.validate("build!: 契約を変える (#1)\n\nBREAKING CHANGE: 契約を更新"))


if __name__ == "__main__":
    unittest.main()
