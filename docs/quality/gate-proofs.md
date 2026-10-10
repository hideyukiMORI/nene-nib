# ゲート発火の証明 — NeNe Nib

> Status: 記録 / 最終実測 2026-09-15（Issue #1・Phase 2 と Issue #3・最初の縦切り。core / application の実ライブラリに対して ARC-002 / ARC-003 / ARC-007 / CPP-013 / QLT-009 / CNF-007 を結線した）
> 根拠となる規則: QLT-007（カスタムゲートには negative proof が要る）

**検査は「落ちること」を見るまで信用しない。** 各ゲートについて、最小の違反を仕込んだ状態で
意図した規則 ID によって失敗すること、そして元に戻すと対応する最小の検査が成功することを実測する。
ゲートを変えたら、この記録も同じ変更で更新する。

2026-09-20 以降の実行頻度と結果の再利用は [ADR 0021](../adr/0021-diff-scoped-verification-and-result-reuse.md) が正。
本書の過去の全件コマンド・CI 実行回数は歴史的な実測記録であり、現在の再実行指示ではない。

### 2026-09-20・Issue #61: 検証の選択と再利用

- 対象・退行: check.ps1 の全件誤起動、PR 検証記録の欠落、既存のコミット形式検査への影響。製品ソース・ビルド定義・性能や coverage の判定は変更していない。
- `python -m unittest discover -s tests/conformance -p test_verification_policy.py -v`: 7 tests、終了 0（11.103 秒）。指定なし・Full のみ・空の理由・理由のみは QLT-001 で拒否。理由付き Full は隔離 fixture の toolchain sentinel まで到達して停止し、全件ゲート本体は実行していない。
- 同テストで PR 記録の正例・CRLF と再利用記録・各項目の欠落/空白・CLI の非 0・validate-git.ps1 の模擬 PR イベントの正例と反例を確認した。CI のテスト未呼出と軽いフックの維持も確認した。
- `python -m unittest discover -s tests/conformance -p test_conformance.py -k GitChecks -v`: 既存の Git 検査 4 tests、終了 0。
- 成功後の説明文追記やコミット・PR 工程では上記結果を再利用する。対象コード・テスト・関連依存は不変。アプリの動作・Vim oracle・性能・coverage・全件検証はこの規約更新では実行しない。

🔴 **この文書に結果を先に書かない。** 雛形の段階で「失敗」「成功」と書いてあった結果は、
実測していないのに測ったように見える（NeNe Loupe ADR 0003 が雛形の欠陥として指摘・2026-09-06）。
結果の列は実測するまで **未実測** のままにする。

環境: Windows 11 Pro 10.0.26200 x64。clang-cl 19.1.5（LLVM、Visual Studio Build Tools 同梱）、MSVC toolset 14.44.35207（cl 19.44.35228 はリンカ・SDK・Phase 0 の比較対象）、
Windows SDK 10.0.26100、CMake 3.31.6-msvc6、Ninja 1.12.1、Python 3.12.10、PowerShell 7.6.5。今後使う版の正本は `eng/tool-versions.json`。
CI: GitHub Actions `windows-2022`（PR の ready_for_review で起動。ローカルの実測はこの文書、CI の実行証拠は Issue #1 の PR で取る）

Phase 0 の言語の実測（114 記録）は [phase0-results.json](phase0-results.json)。再現は `pwsh -NoProfile -File ./eng/measure-language.ps1`。
本書はゲート（`eng/check.ps1` に結線した検査）の発火だけを扱う。

---

## 1. 実測結果

### 1-a. active の規則（規則 ID ごとの発火と復帰）

🔴 **文書整合検査（CNF-006）は「`| <規則 ID> |` で始まる行」を active の証明として読む。** 下の候補表（`| P1 | ARC-007 |`）は
規則 ID が 2 列目なので証明にならない。active に書き換える規則は、必ずこの表に規則 ID を行頭にして 1 行足す。

| 規則 | 最小の違反 | 検証経路 | 実測 2026-09-15 |
| --- | --- | --- | --- |
| QLT-002 | 未使用変数・プロトタイプ無し・lint 違反・整形違反 | eng/prove-gates.py / 実 CMake ビルド・clang-tidy・clang-format | `-Wunused-variable` / `-Wmissing-prototypes` / clang-tidy の各診断 / `clang-format-violations` で非 0。各復帰は 0 |
| QLT-004 | 一行に詰めた main | eng/prove-gates.py / clang-format --dry-run --Werror | `clang-format-violations` で非 0。元の整形は 0 |
| CNF-006 | 未定義 ID・重複定義・状態不一致・証明行欠落・未置換値 | tests/conformance の document_checks 正例・反例 | 正例は指摘 0、反例は CNF-006。本文書を書く前の版では `{{` の残りと未定義 ID を 3 件検出した |
| CNF-008 | Issue 番号の無いタスクコメント | tests/conformance の configuration_checks 正例・反例 | 番号付きは指摘 0、番号なしは CNF-008 |
| CNF-009 | core の `<thread>` / `<atomic>`・ui/win32 の `<thread>`・`src/core/simd` 以外の `<immintrin.h>` | tests/conformance の source_checks 正例・反例 | core / ui の並行性ヘッダは CPP-013、simd 以外の SIMD ヘッダは CPP-018。adapters/win32 の `<thread>`・`src/core/simd` の `<immintrin.h>`・tests/ は指摘 0 |
| ARC-002 | CMake で宣言外の依存と OS ライブラリを結ぶ・宣言外モジュールへの相対 include・ビルドに無い翻訳単位 | eng/prove-gates.py（configure）/ tests/conformance の architecture_checks | configure が `ARC-002: forbidden dependency` / `no platform libraries` で非 0（P2）。相対 include と File API の実グラフは tests/conformance で ARC-002。実ターゲット 6 つ（core / application / adapters_win32 / ui_win32 / app / unit_tests）に対し `--build-dir build` が 0 件（Issue #3・2026-09-15） |
| ARC-003 | 中核相当のオブジェクトで `CreateFileW` / `std::filesystem::exists` を呼ぶ・必須モジュールの欠落 | eng/prove-gates.py（`eng/symbols.py --object` / `--require`）/ tests/conformance の test_symbols | `ARC-003: core: undeclared external symbol __imp_CreateFileW` / `__std_fs_get_stats` で非 0（P17）。`--require application` を単一オブジェクトに対して要求すると `required module application has no static library` で非 0（P18）。実ライブラリは `Symbols: 2 libraries checked, 0 violation(s)`（Issue #3） |
| ARC-007 | 中核相当のオブジェクトで `system_clock::now()` / `GetTickCount()` を呼ぶ・**実物の `nenenib_core.lib` に時刻を読む翻訳単位を足す** | eng/prove-gates.py（`eng/symbols.py --object` と実ライブラリ） | `ARC-007: core: non-deterministic input symbol _Xtime_get_ticks` / `__imp_GetTickCount` で非 0（P1）。実ライブラリでも `_Xtime_get_ticks (real static library)` で非 0、戻すと 0（P24・Issue #3） |
| CPP-013 | 中核相当で `std::thread` / `std::mutex` / `CreateThread` を使う | eng/prove-gates.py（`eng/symbols.py --object`） | `CPP-013: core: concurrency symbol outside the worker adapter _beginthreadex` / `_Mtx_lock` / `__imp_CreateThread` で非 0（P19）。実ライブラリ 2 本は 0 件（Issue #3） |
| QLT-009 | 失敗系の単体テストを省いて実行（`--coverage-negative`） | eng/coverage.py / 同一 exe の別プロファイル | 39.06% で `QLT-009: branch coverage 39.06% < 90%`。全テストへ復帰すると 61/64 分岐＝95.31% で成功（P25・Issue #3） |
| CNF-007 | 固定 manifest を置く・manifest の版をリテラルで書く・どこからも読み込まれない設定ファイル | tests/conformance の configuration_checks / version_metadata_checks 正例・反例 | 固定 `NeNeNib.manifest` と版リテラルを CNF-007、正例（`@PROJECT_VERSION_*@` から導出）は指摘 0（P11・P26・Issue #3） |
| CNF-010 | `tests/vim/fixtures.json` の 1 文字を変える（fixture 名の末尾 `s` → `z`。長さは変えない）・生成物から SHA の行を消す・本数だけ違う行を書く | `python eng/conformance.py`（1 文字の実測）/ tests/conformance の fixture_digest_checks 正例・反例 | 2026-09-18: 1 文字変えると `CNF-010: tests/vim/VimFixtures.hpp: recorded digest 15758fd4… is not 3277c07b…; regenerate` で `Conformance: 1 violation(s)`・終了 1。戻すと `0 violation(s)`・終了 0。記述なし・本数不一致・正例（oracle の `header()` が書いた行）は `tests/conformance` の 5 件（Issue #44）。2026-09-29（Issue #229・ADR 0054）: 生成物 `tests/vim/VimKeyNames.hpp` の `<Left>` の行を `arrow_right` に書き換えると `CNF-010: tests/vim/VimKeyNames.hpp: line 27: ... is not the generated ...; run eng/vim-oracle.py --key-names` で `1 violation(s)`・終了 1、消すと `the generated key names are missing` で終了 1、戻すと 0 violations。反例は 5-bn |
| CNF-011 | `tests/vim/fixtures.json` を古い形に戻す（indent 2・空の `"settings": []` を足す・キー順を入れ替える・区切りの後に空白・`\u` エスケープ・1 行に全部・CRLF・末尾改行を消す）・未知のキー / キー不足 / 壊れた JSON / 不正な `viewport` | `python -X utf8 eng/conformance.py`（実リポジトリの実測）/ tests/conformance の fixture_format_checks 正例・反例 | 2026-09-22: 実物の 1 件に `"settings":[]` を足すと `CNF-011: tests/vim/fixtures.json: line 3: '  {"name":"h-with-a-count","text":"alpha","keys":"$3h","sett...' is not the canonical '  {"name":"h-with-a-count","text":"alpha","keys":"$3h"},'; run eng/vim-oracle.py --format` と CNF-010 の SHA 不一致で `Conformance: 2 violation(s)`・終了 1。末尾改行を消すと `no newline at the end of the file`（行の指摘は重ねない）で同じく終了 1。戻すと `0 violation(s)`・終了 0。反例 15 通りと正例（oracle の `canonical_fixtures_json` が書いたバイト列そのまま）は `tests/conformance` の 5 件（Issue #98） |
| CNF-012 | 表に無い method・関数名が印と違う・本体 7 行・NOLINT に別の check・置き場が違う（ほかに印の文法 4 通り・本体の無い宣言・空行とコメントを数える） | `python eng/conformance.py`（実リポジトリの実測）/ tests/conformance の `SdkAbiChecks` 正例・反例 | 2026-10-06: 実物の `BodyGlyphCollector.cpp` の印を `IDWriteTextRenderer::DrawGlyphRuns` に変えると `CNF-012: ... line 90: IDWriteTextRenderer::DrawGlyphRuns is not in eng/sdk-abi-signatures.json` と `CNF-003: ... line 91: missing valid, scoped waiver` で終了 1。戻すと `0 violation(s)`・終了 0。本体 6 行ちょうど・早期 return の境界・`tests/ui/` は指摘 0、印の無い NOLINT は CNF-003（Issue #299・ADR 0076・5-cr） |
| QLT-014 | 基準値の複製を 1 本だけ厳しくして `eng/measure-speed.py --check --reference <複製> --values <測った値>` | eng/prove-gates.py（`prove_speed_reference`。exe も窓も要らない経路） | `QLT-014: startup-first-frame: 100.000 ms exceeds 12.500 ms` で非 0、戻した複製は 0（P27・Issue #16・2026-09-16）。実機の `--check` は `Speed: 4 benches checked, 0 regression(s)`。CI（指紋 `e7a87d5b6ac1e14b`・ADR 0016・Issue #47）は基準値を足したので `Speed: 5 benches checked, N regression(s), N unmeasurable` の判定に変わる。CI の実 run（PR #48・run 35357889836）: attempt 1 は指紋 `196309bb`（EPYC 9V45）に当たり `Speed: no reference for 196309bbf27c16b1 (AMD EPYC 9V45 96-Core Processor); recorded only -- QLT-014 is not judged on this host` が本文とまとめの 2 回出て終了 0、artifact `speed-records`（3,058 バイト・90 日）が上がった。attempt 2（rerun）は指紋 `e7a87d5b`（EPYC 7763）に当たり **`Speed: 5 benches checked, 0 regression(s), 0 unmeasurable`**（起動 34.535 / 窓 25.129 / single 1.374 / burst 2.829 / 16 MiB 86.696 ms・欠測 0）で、CI で基準値との判定が動いた。手元では CI の 16 run のログを `out/speed/*.json` の形に起こして `--check --values` に流した（2026-09-18・Issue #47）: 指紋 `e7a87d5b` の #24 以降の 7 run は全部 `Speed: 5 benches checked, 0 regression(s), 0 unmeasurable`・終了 0、他の 5 つの指紋の 6 run は `Speed: no reference for <指紋> (<CPU>); recorded only -- QLT-014 is not judged on this host`・終了 0 |

### 1-b. 反例の一覧（planned の部分証明を含む）

2026-09-15 に `pwsh -NoProfile -File ./eng/check.ps1` を通した結果。`eng/prove-gates.py` は毎回のゲートで下の実ツール反例を仕込み直し、
復帰まで確かめる（`out/proofs/results.json`）。反例は `out/proofs/` 以下の使い捨てプロジェクトへ入れ、本体のソースは変更しない。

| # | 規則 | 仕込む違反 | 実行するタスク | 結果 |
| --- | --- | --- | --- | --- |
| P1 | ARC-007 | 中核相当のオブジェクトで `system_clock::now()` / `GetTickCount()` を呼ぶ | `eng/prove-gates.py`（clang-cl でコンパイル → `eng/symbols.py --object … --module core`） | `ARC-007: core: non-deterministic input symbol _Xtime_get_ticks` / `__imp_GetTickCount` で非 0。`memcmp` だけの probe は 0 |
| P2 | ARC-002 | CMake で宣言外の依存と OS ライブラリを結ぶ | `eng/prove-gates.py`（configure） | configure が `ARC-002: forbidden dependency verification -> verification` / `no platform libraries for verification` で非 0。戻すと configure・build とも 0 |
| P3 | CPP-002 | 分岐漏れ（`default:` 付き）／網羅済みの `default` | `eng/prove-gates.py`（実ビルド） | `-Wswitch-enum` と `-Wcovered-switch-default` で非 0。元のソースは 0。`default:` 無しの分岐漏れは先に `-Wswitch` で落ちる |
| P4 | QLT-002 | 未使用変数・プロトタイプ無し | `eng/prove-gates.py`（実ビルド） | `-Wunused-variable` と `-Wmissing-prototypes` で非 0。元のソースは 0 |
| P5 | CNF-001 | 型を `ColorHelper` / `note_helper` と命名する・`utils/` ディレクトリ | `tests/conformance` | CamelCase と snake_case の語尾、モジュール名を CNF-001。普通の名前は指摘 0 |
| P6 | CNF-003 | waiver 無しの抑制を書く | `tests/conformance` | pragma・`_Pragma`・`NOLINT`・waiver 無しの `NOLINTNEXTLINE` を CNF-003。有効な行単位 waiver（大文字を含む検査名）は通る |
| P7 | CNF-004 | 期限切れの waiver を置く | `tests/conformance` | 期限切れ・必須項目欠落・不整合索引・範囲外 Scope を CNF-004。期限当日は通る |
| P8 | QLT-004 | 整形を崩す | `eng/prove-gates.py`（clang-format） | `clang-format --dry-run --Werror` が `clang-format-violations` で非 0。元の整形は 0 |
| P9 | CNF-005 | `baseline` を名に含む設定ファイル・`/WX-`・`-Wno-error`・`-w` | `tests/conformance` | CNF-005。名指しの `-Wno-switch-default` / `-Wno-language-extension-token` は通る |
| P10 | CNF-006 | マトリクスに無い規則 ID・未置換値・証明行の欠落・重複定義・状態不一致 | `tests/conformance` | CNF-006。正例は指摘 0 |
| P11 | CNF-007 | どこからも読み込まれない設定ファイルを置く | `tests/conformance` | 参照欠落と余分な `.clang-tidy` を CNF-007。正常な参照は通る |
| P12 | GIT-003 | 形に合わないコミット件名 | `tests/conformance` | Issue 番号無し・英語説明・`BREAKING CHANGE` フッタ欠落を GIT-003。正しい件名は通る |
| P13 | CPP-003 | C スタイルキャストで const を剥がす／`const_cast` | `eng/prove-gates.py`（`-Wold-style-cast` / clang-tidy） | `-Wold-style-cast` と `cppcoreguidelines-pro-type-const-cast` で非 0。元のソースは 0 |
| P14 | CPP-004 | `nullptr` を逆参照する／引数の optional を `value()` で読む | `eng/prove-gates.py`（clang-tidy） | `clang-analyzer-core.NullDereference` と `bugprone-unchecked-optional-access` で非 0。元のソースは 0 |
| P15 | CPP-012 | 5 引数の関数 | `eng/prove-gates.py`（clang-tidy） | `readability-function-size` で非 0。元のソースは 0 |
| P16 | CPP-016 | 可変長配列／`std::move` 後の再利用／所有者の無い `new` | `eng/prove-gates.py`（実ビルド・clang-tidy） | `vla-cxx-extension`（clang-tidy では `clang-diagnostic-vla-cxx-extension`）・`bugprone-use-after-move`・`clang-analyzer-cplusplus.NewDeleteLeaks` で非 0。元のソースは 0 |
| P17 | ARC-003 | 中核相当で `CreateFileW` / `std::filesystem::exists` を呼ぶ | `eng/prove-gates.py`（`eng/symbols.py`） | `ARC-003: core: undeclared external symbol __imp_CreateFileW` / `__std_fs_get_stats` で非 0 |
| P18 | ARC-003 | 必須モジュールの静的ライブラリが無い | `eng/prove-gates.py`（`eng/symbols.py --object … --require application`） | `required module application has no static library in the build` で非 0。`--require core` は 0 |
| P19 | CPP-013 | 中核相当で `std::thread` / `std::mutex` / `CreateThread` を使う | `eng/prove-gates.py`（`eng/symbols.py`） | `CPP-013: core: concurrency symbol outside the worker adapter _beginthreadex` / `_Mtx_lock` / `__imp_CreateThread` で非 0 |
| P20 | CPP-018 | `/arch` も target 属性も無い関数で `_mm256_add_epi32` を書く | `eng/prove-gates.py`（実ビルド） | `always_inline function '_mm256_set1_epi32' requires target feature 'avx'` で非 0。元のソースは 0 |
| P21 | ARC-005 | 可変グローバル変数 | `eng/prove-gates.py`（clang-tidy） | `cppcoreguidelines-avoid-non-const-global-variables` で非 0。元のソースは 0 |
| P22 | CNF-009 | core で `<thread>` / `<atomic>` を include・`src/core/simd` 以外で `<immintrin.h>` を include | `tests/conformance` | CPP-013 / CPP-018。adapters/win32・`src/core/simd`・tests/ は指摘 0 |
| P23 | CNF-008 | Issue 番号の無い `TODO` | `tests/conformance` | CNF-008。番号付きは指摘 0 |
| P24 | ARC-007 | 実物の `nenenib_core.lib` に `system_clock::now()` を呼ぶ翻訳単位を足して再ビルド | `eng/prove-gates.py`（実ビルド → `eng/symbols.py --build-dir --require core application`） | `ARC-007: core: non-deterministic input symbol _Xtime_get_ticks (real static library)` で非 0。戻すと `2 libraries checked, 0 violation(s)`（Issue #3） |
| P25 | QLT-009 | `nib_tests --coverage-negative` で失敗系を省く | `eng/coverage.py`（測定ビルド） | 25/64 分岐＝39.06% で非 0。全テストで 61/64＝95.31% が 0（Issue #3） |
| P26 | CNF-007 | 固定 manifest・版リテラル | `tests/conformance`（version_metadata_checks） | 反例 2 件を CNF-007、正例は指摘 0（Issue #3） |
| P28 | CNF-012 | 表に無い method・関数名が印と違う・本体 7 行・NOLINT に別の check・置き場が違う | `tests/conformance`（SdkAbiChecks） | 5 つとも CNF-012。別の check を混ぜた NOLINT は CNF-003 も。印の無い NOLINT は CNF-003。本体 6 行ちょうどは指摘 0（Issue #299） |

**復帰の確認**: 2026-09-15。P1〜P4・P8・P13〜P21・P24 は `eng/prove-gates.py` が各反例の直後に元へ戻して build / configure / clang-format / symbols を再実行し、
終了コード 0 を確かめた（26 反例）。P5〜P7・P9〜P12・P22・P23・P26 は正例テストが同じ suite にある（86 テスト）。P25 は `eng/coverage.py` が反例のあとに全テストの計測で 0 を確かめる。
最後にフルゲート全体が終了コード 0 で `NeNe Nib full gate passed` を出した。

**除外側の確認**: 例外区画について「禁止が効いていること」と「唯一の窓口が通ること」の両方を見る。

| 区画 | 適用しない禁止 | 呼んでいる禁止 API | 結果 |
| --- | --- | --- | --- |
| `src/adapters/win32` | 決定性・OS import | `__imp_RegGetValueW`（`Win32AppearanceAdapter.cpp`） | 2026-09-15: `eng/symbols.py --build-dir build --require core application` は core / application の 2 ライブラリだけを検査して `0 violation(s)`。adapters_win32 のライブラリは対象外なので上の import を持ったまま通る（唯一の窓口が通ること）。字句検査の正例は tests/conformance で `src/adapters/win32/` の `<thread>` と `time(0)` が通ることを確認 |
| `src/ui/win32` / `src/app` | OS import | `__imp_CreateWindowExW` / `__imp_D3D11CreateDevice` / `__imp_CreateDXGIFactory2` / `__imp_DCompositionCreateDevice` / `__imp_D2D1CreateFactory` / `__imp_DWriteCreateFactory` / `__imp_WaitForSingleObjectEx` | 同上。字句検査（並行性ヘッダ）は ui / app にも適用される |

---

## 2. 出力の抜粋（実行結果からの引用）

```
QLT-002: <repo>/out/proofs/build-*/tests/build/ToolchainSmoke.cpp(3,9): error: unused variable 'unused' [-Werror,-Wunused-variable]
CPP-003: ToolchainSmoke.cpp:3:5: error: do not use const_cast to remove const qualifier [cppcoreguidelines-pro-type-const-cast,-warnings-as-errors]
CPP-004: ToolchainSmoke.cpp:4:12: error: Dereference of null pointer (loaded from variable 'pointer') [clang-analyzer-core.NullDereference,-warnings-as-errors]
CPP-004: ToolchainSmoke.cpp:5:12: error: unchecked access to optional value [bugprone-unchecked-optional-access,-warnings-as-errors]
CPP-012: ToolchainSmoke.cpp:1:12: error: function 'pick' exceeds recommended size/complexity thresholds [readability-function-size,-warnings-as-errors]
CPP-016: ToolchainSmoke.cpp:4:16: error: variable length arrays in C++ are a Clang extension [clang-diagnostic-vla-cxx-extension]
CPP-016: ToolchainSmoke.cpp:8:29: error: 'first' used after it was moved [bugprone-use-after-move,-warnings-as-errors]
CPP-016: ToolchainSmoke.cpp:4:5: error: Potential leak of memory pointed to by 'value' [clang-analyzer-cplusplus.NewDeleteLeaks,-warnings-as-errors]
CPP-018: ToolchainSmoke.cpp(5,21): error: always_inline function '_mm256_set1_epi32' requires target feature 'avx', but would be inlined into function 'main' that is compiled without support for 'avx'
ARC-005: ToolchainSmoke.cpp:1:5: error: variable 'counter' is non-const and globally accessible, consider making it const [cppcoreguidelines-avoid-non-const-global-variables,-warnings-as-errors]
QLT-004: ToolchainSmoke.cpp:1:11: error: code should be clang-formatted [-Wclang-format-violations]
ARC-002: forbidden dependency verification -> verification
ARC-007: core: non-deterministic input symbol _Xtime_get_ticks
ARC-003: core: undeclared external symbol __std_fs_get_stats
CPP-013: core: concurrency symbol outside the worker adapter _Mtx_lock
```

フルゲートの構成で踏んだ 4 点（規則ではなく道具の癖。ADR 0003 に記録）:

- C++ の VLA を落とすのは `-Werror=vla` ではなく、既定で有効な `-Wvla-cxx-extension` を `/WX` がエラーへ上げる経路。`-Werror=vla` は C++ では効かなかったので、警告集合の名指しを `-Werror=vla-cxx-extension` に変えた
- `-Wswitch-enum` の証明には `default:` が要る。`default:` 無しで列挙子を欠くと clang は先に `-Wswitch` で落ちる
- clang-tidy はコンパイルより先に走り、コンパイラ診断に `clang-diagnostic-` の接頭辞を付ける。期待文字列は接頭辞なしの共通部分にしてある
- `misc-non-private-member-variables-in-classes` はメソッドを持たない素の aggregate には発火しない（Phase 0 の T1-tidy-aggregate-hole と同じ）。反例にはメソッド付きのクラスが要る

---

## 3. 事故から生まれた検査

<!-- 設定だけあって効いていなかった、検査を足した直後に迂回された、などの事故を Issue 番号つきで残す。
     前例: NeNeClock Issue #26（設定ファイルが読み込まれていなかった）/ #42（文言検査の迂回） -->

（まだ無い。Issue #1 で見つけた「`-Werror=vla` が C++ では効かない」は事故になる前に反例で捕まえたので、第 2 節に記録した）

---

## 4. リポジトリ設定（ruleset）

| 設定 | 値 | 確認日 |
| --- | --- | --- |
| PR 必須 | ruleset `main`（id 23337476・active・対象 `~DEFAULT_BRANCH`）の `pull_request`。承認 0・スレッド解決必須・`allowed_merge_methods: [squash]`・bypass actor 無し | 2026-09-15 |
| 必須 check | 同 ruleset の `required_status_checks`: context `check` | 2026-09-15 |
| strict up-to-date | 同 ruleset `strict_required_status_checks_policy: true` | 2026-09-15 |
| force push / ブランチ削除の禁止 | 同 ruleset の `non_fast_forward` と `deletion` | 2026-09-15 |
| squash のみ | リポジトリ設定 `allow_squash_merge` のみ true・`delete_branch_on_merge` true ＋ ruleset の `allowed_merge_methods` | 2026-09-15 |

読み戻し: `gh api repos/hideyukiMORI/nene-nib/rulesets/23337476`（2026-09-15）。

最初の [CI 実行](https://github.com/hideyukiMORI/nene-nib/actions/runs/34873383824) は PR #2 を Ready にした直後にコミット `6a51021` で
成功した（1 分 18 秒）。ローカルと同じ単一コマンドで規約検査 0 件、検査器 83 テスト、CTest 1 件、`Symbols: 0 libraries checked`、
実ツールの反例 25 本と復帰を実行した。Ready 後の head 更新を自動で Draft へ戻す処理と、Git 規約の全条件の反例証明は未実装なので、
GIT と QLT-012 の規則全体は planned を維持する。

🔴 **設定していないものを「必須になっている」と書かない。** 設定したら `gh api` で読み戻して記録する。

---

## 5. 環境依存の確認（QLT-013）

<!-- 表示・実機・実 GPU・oracle を伴う確認は、単体テストとは別にここに環境と手順を書く -->

### 5-a. Phase 0 の環境依存の実測（Issue #1・2026-09-15）

環境: 上記。4 モニタ（120 / 144 / 168 DPI）の端末だが、Phase 0 は窓を作っていない。

- D1: WARP の D3D11 device で Direct2D / DirectWrite / DXGI（flip model・waitable swap chain・composition）/ DirectComposition / DWM を呼び、描画して Present するまで終了 0。実 GPU では測っていない
- V2: `C:\Program Files\Vim\vim91\vim.exe`（9.1・2024-01-02）を `-u NONE -i NONE -N -n -es -S probe.vim` で 2 回起動し、同じ出力を得た。CI に Vim は無い
- MD1: md4c release-0.5.2 を GitHub から clone して測った。ネットワークが無ければ未実測になる設計

### 5-b. 最初の縦切り（Issue #3・ADR 0007・2026-09-15）

環境: Windows 11 Pro 10.0.26200・初期モニタ 120 DPI（125%）・実 GPU（既定アダプタ）・`build/NeNeNib.exe`（Debug 構成＝ASan / UBSan 付き）。
OS の「アプリのモード」はダーク（`AppsUseLightTheme` = 0）。

手順（`python eng/verify-window.py`。実ポインタ・キーボードは操作しない・CI からは呼ばない）:

1. 隔離した `LOCALAPPDATA` / `APPDATA` で exe を起動し、窓クラス `NeNeNib.Editor` を最大 5 秒待つ
2. `GetWindowRect` / `GetClientRect` / `GetDpiForWindow` を記録し、画面 DC から client 領域を `BitBlt` で取る
3. 中心画素と (8,8) を、現在の外観に対する `palette_for` の背景色と比べる。client 領域を BMP に保存する
4. `WM_CLOSE` を送り、終了コードを確認する

結果（`out/window-verification/first-slice-results.json`・`first-slice.bmp`）:

- 枠なし（`WS_POPUP`）の可視窓が `[1520, 825, 2320, 1275]` に出た。client は 800×450 物理画素＝120 DPI で 640×360 DIP ちょうど
- 中心画素と (8,8) は `[48, 10, 36]`＝茄子色 #300A24（D11 のダーク背景）と一致。BMP の左上 x22-175 / y28-44 に文字色 #EEEEEC の字形画素が 935 個あり、1 行が描けている
- `WM_CLOSE` で終了コード 0
- DirectComposition の swap chain でも画面 DC からの `BitBlt` で合成後の画素が読めた
- exe の import（`llvm-readobj --coff-imports`）: Release は `USER32` `ADVAPI32` `KERNEL32` `d3d11` `dxgi` `dcomp` `d2d1` `DWrite` の 8 本（全部 OS）。Debug はこれに ASan 由来の `api-ms-win-core-synch-l1-2-0.dll` が加わる
- 見ていないもの: ライトの外観・`WM_SETTINGCHANGE` によるテーマ追従の実機・DPI の切り替え・複数モニタ・device lost の再生成・実 GPU の遅延。単体テストはこれらの証拠にならない

### 5-c. 見た目の縦切り（Issue #5・ADR 0008・2026-09-15）

環境: 5-b と同じ（Windows 11 build 26200・120 DPI・実 GPU・ダーク）。

手順（`python eng/verify-window.py`。実ポインタ・キーボードは操作しない）: 5-b の手順に加えて、窓の様式（`GWL_STYLE` に `WS_THICKFRAME`・`WS_POPUP` 無し）、`WM_NCHITTEST`（閉じるボタンの中心と
タイトルバーの中央へ `SendMessageW`）、タイトルバー中央の画素と本文背景の差（Mica）、ステータスバーのトグルの `Vim` 側へ `WM_LBUTTONDOWN` / `WM_LBUTTONUP` を `PostMessageW` してから
その画素、起動直後に最初に見えた窓の矩形。

結果（`out/window-verification/look-slice-results.json`・`look-slice.bmp`・`look-slice-vim.bmp`）:

- 様式は `WS_OVERLAPPEDWINDOW` 相当で `WS_POPUP` 無し。`WM_NCHITTEST` は閉じるボタンで 20（`HTCLOSE`）、タイトルバー中央で 2（`HTCAPTION`）
- タイトルバー中央の画素は (42,42,42) で本文の #300A24 と違う＝Mica が透けている。`DWMWA_USE_IMMERSIVE_DARK_MODE` を渡す前は (244,244,244) だった
- トグルの `Vim` 側の中心画素はクリック前 (74,30,61)＝`toggle`、クリック後 (233,84,32)＝`accent` #E95420。`通常` 側は逆に戻った
- 最初に見えた窓の矩形は `[1520, 825, 2320, 1275]`（配置後に表示。(0,0) 起点でない）。`WM_CLOSE` で終了 0
- 最大化（使い捨ての probe で `HTMAXBUTTON` を送信）: `IsZoomed` true、client 3840×2100 は作業領域と一致、最大化中も閉じるのヒットは 20、復元後の矩形は元どおり
- 見ていないもの: `WM_DPICHANGED`（DPI の違うモニタ間の移動）・96 DPI・Mica 非対応環境の fallback・Snap Layouts のホバー・ライトの外観。この機は Segoe UI Variable Text と Cascadia Code を両方持つのでフォントの fallback は未実行

### 5-d. 編集の縦切り（Issue #7・ADR 0009・2026-09-15）

環境: 5-b と同じ（Windows 11 build 26200・120 DPI・実 GPU・ダーク）。client 800×450 物理画素・本文の最初の行の上端 65・行高 30・行番号の欄 70・見えている行 11。

手順（`python eng/verify-window.py`。`WM_CHAR` / `WM_KEYDOWN` / `WM_LBUTTONDOWN` を `PostMessageW` で送る。実ポインタ・実キーボードは操作しない）:
`a` `b` `c` と Enter → 2 行目の行頭にキャレット、行番号 2 の描画、ステータスの変化 / Backspace 2 回 → 1 行に戻る / Vim へトグル → ブロックのキャレット、通常へ戻す → バー /
Enter 200 回 → 1 行目が見えなくなる、PgUp 30 回 → 1 行目が上端、PgDn 1 回 → 見えなくなる / Esc → 窓は閉じない / 2 行目の行頭へクリック → キャレットが 2 行目 / `WM_CLOSE` → 終了 0。

結果（`out/window-verification/editing-slice*.json` / `.bmp`）:

- キャレットの画素は `accent` (233,84,32)。Enter で 2 行目へ移り、2 行目の面が `current_line` (62,26,50)、1 行目が `background` に戻った。「abc」の字形画素 136、行番号 2 の帯の画素 49、ステータスの画素数 218 → 221
- Backspace 2 回で 1 行に戻り「ab」（画素 92）が残った。Vim へトグルするとキャレットの脇が `accent`（ブロック）、通常へ戻すと `current_line`（バー）
- Enter 200 回のあと 1 行目は見えず、PgUp 30 回で上端に戻り、PgDn 1 回で再び見えなくなった。撮影時点（6 秒後）では 181 行までしか進んでいなかった＝1 打鍵 1 フレームで流れる
- `WM_CLOSE` で終了 0
- **測れないもの**: Ctrl の組み合わせ（Ctrl+A / C / X / V / Z / Y）と Shift の選択。`GetKeyState` は生入力キューを通った鍵しか見ないので `PostMessageW` では駆動できない。undo / redo・選択・クリップボードは単体テスト（偽の `ClipboardPort`）で表示値として測った（427 チェック）
- 手元の計測（`/O2`・使い捨て・QLT-014 の本測定ではない）: 1 MB の挿入 0.62 ms、1 万行 CRLF の読み込み 1.93 ms、1.22 MB の中央への 1000 挿入 47.5 ms

### 5-e. ファイルの縦切り（Issue #11・ADR 0010・2026-09-15）

環境: 5-b と同じ（Windows 11 build 26200・120 DPI・実 GPU・ダーク）。client 800×450 物理画素。ステータスバー右の文字コードの項目は 72 DIP。

手順（`python eng/verify-window.py` の `documents` 部。一時ファイルを作り、**起動引数**で開く。`WM_CHAR` は `PostMessageW`、Ctrl+S だけは `AttachThreadInput` で前面を取ってから `SendInput`。実ポインタ・実キーボードは操作しない）:
UTF-8 CRLF 3 行（「一行目」「二行目」「三行目」）→ 3 行の描画・題名・ステータス / 1 文字打つ → 題名に「● 」 / Ctrl+S → ファイルの大きさと題名 / `WM_CLOSE` /
Shift_JIS LF 2 行（「日本語」「二行目」）→ 描画・ステータス / 1 文字打つ → 「● 」 / `WM_CLOSE` → 「保存しますか」に いいえ（`WM_COMMAND(IDNO)`）/
存在しない経路 → `MessageBoxW`（`#32770`）が出て、`WM_CLOSE` で閉じると「無題 - NeNe Nib」で続き、未保存の確認は出ない。

結果（`out/window-verification/file-slice-*.bmp`）:

- UTF-8 CRLF: 題名 `utf8-crlf.txt - NeNe Nib`、3 行の字形画素 165 / 174 / 184（4 行目 0）、行番号の帯 38 / 49 / 46、ステータスの `UTF-8` 171・`CRLF` 150。1 文字打つと `● utf8-crlf.txt - NeNe Nib`
- **Ctrl+S は `SendInput` で駆動できた**: ファイルが 31 → 32 バイトに変わり、題名から「● 」が消え、保存済みなので `WM_CLOSE` で確認は出なかった（終了 0）
- Shift_JIS LF: 題名 `sjis-lf.txt - NeNe Nib`、2 行の字形画素 255 / 174（3 行目 0）、`Shift_JIS` 260・`LF` 57（UTF-8 側と別の画）。1 文字打つと「● 」、`WM_CLOSE` の確認に いいえ で終了 0
- 存在しない経路: 理由の箱が出て（`reported: true`）、閉じると `無題 - NeNe Nib`、確認なしで終了 0
- 単体テスト（偽の `FilePort` / `CodePagePort`）: 判別 16 件・改行 8 件・`FilePath` 10 件・題名 9 件・開く失敗 8 件・保存の失敗と保存状態の遷移。adapter テスト `nib_adapters`（実ファイル・ビルドディレクトリ配下の固定名フォルダ）: UTF-8 / BOM / Shift_JIS の往復、置換、`not_found`、フォルダ → `unreadable`、上限 4 バイトで `too_large`、1 MiB の書き戻し、CP932 の `unencodable`（😀）
- **測れないもの**: Ctrl+O・Ctrl+Shift+S・`IFileOpenDialog` / `IFileSaveDialog`（既定の拡張子 `.txt` を含む）・「保存しますか」の はい／キャンセル はモーダルで自動検査に載っていない。32 MiB 超の分割書き込みの 2 周目以降。`ReplaceFileW` の別ボリューム・同期フォルダでの失敗

### 5-f. 速さの縦切り（Issue #16・ADR 0011・2026-09-16）

環境: 5-b と同じ機械（Intel Core i9-10850K / NVIDIA GeForce RTX 3090 / 120 DPI / Windows 11 build 26200・ダーク）。指紋 `bc8a356f37c68491`。**Release 構成の `build-release/NeNeNib.exe`** を測る（Debug は ASan / UBSan の数字になる）。

手順（`python eng/measure-speed.py --record`。`eng/window_driver.py` で起動し、節目は exe が `--measure` で書く JSON から読む）:
① 引数なしで起動 → 最初の `frame_presented` / ② 窓を前景にして暖機の 1 打鍵を捨て、1 打鍵と、窓のスレッドを止めてから 200 打鍵をまとめて post → 節目の差 / ③ 16,800,000 バイト・200,000 行の UTF-8 CRLF を起動引数で開く → 最初の `frame_presented`。各 5 回の中央値。

結果（`docs/quality/speed-reference.md` に詳細。`eng/perf-reference.json` に採用）:

| ベンチ | 中央値 | 5 回の幅 | 2 回目の中央値 |
| --- | --- | --- | --- |
| startup-first-frame | 191.5 ms | 185.4〜197.6 | 198.7 |
| key-to-frame-single | 0.906 ms | 0.888〜1.056 | 0.999 |
| key-to-frame-burst-200 | 2.695 ms | 2.324〜2.754 | 2.450 |
| open-large-file-16mib | 249.8 ms | 234.9〜250.5 | 263.0 |

- `WM_PAINT` への集約の効果（Debug で前後比較）: 200 打鍵の合計 6631 ms → 29.9 ms、1 打鍵 3.53 → 3.55 ms（変わらない）
- `--check` は実機で約 33 秒。フルゲート全体は Release の差分ビルドを含めて 2 分 19 秒
- **測れないもの・揺れ**: 画面が光るまで（`Present` が返るまでを測る）。200 打鍵は post する側が負けて「まとめて」届かない回があり、節目の到着幅が 50 ms を越えた試行は最大 3 回測り直す。それでも残った回は数百 ms として記録され、中央値が守る。CI では窓が作れるかをこの PR で初めて見る（指紋が無いので記録だけ）

### 5-g. Vim エンジンの最初の縦切り（Issue #22・ADR 0012・2026-09-16）

環境: 5-f と同じ機械（Windows 11 build 26200・120 DPI・実 GPU・ダーク）。oracle は `C:\Program Files\Vim\vim91\vim.exe`
＝ `VIM - Vi IMproved 9.1 (2024 Jan 02, compiled Jan  3 2024 23:53:58)`・適用済パッチ 1-4。`where vim` が返す Git 同梱の 9.0 は使わない。版は `eng/tool-versions.json` の `"vim"`。CI に Vim は無い。

手順（`python eng/vim-oracle.py --regenerate`）: `tests/vim/fixtures.json` の各項目について、`text` を `input.txt` に UTF-8 で書き、
`set nocompatible` / `set backspace=indent,eol,start` と項目の `settings`・`call cursor(1, 1)`・`execute "normal! …"`・`writefile(getline(1,'$') + cursor + reg)` を書いた `probe.vim` を
`-u NONE -i NONE -N -n -es -S probe.vim input.txt` で走らせる（Phase 0 の V2 と同じ呼び方）。結果を `tests/vim/VimFixtures.hpp`（`constexpr` の配列）に書く。

結果（2026-09-16 の実測）:

- **fixture 87 件**。`--regenerate` を 2 回走らせて生成物は 1 バイトも変わらない（SHA-256 `457e5495a55ff2f320ff7817572dc689baf9ad222b64decc44e8fa83c3b0ab57`）。3 回目も同じ
- `nib_unit` が 87 件すべてを再生し、本文・キャレットの行とバイト桁・無名レジスタが全部一致した（全体で 1298 件の検査）。テストは Vim を要らない
- `&encoding` は `-u NONE` でも `utf-8` だった（この Vim 9.1 の Windows 版の既定）。`set encoding=utf-8` は既定の設定に**足していない**（ADR 0012 の決定 8 のまま）
- oracle の作法として機械が拒むもの: `text` が改行で終わる項目（Vim の行数とこちらの行数がずれる）と、NORMAL で終わらない `keys`（同じ鍵の末尾に `<Esc>` を 1 つ足した実行と結果が一致しなければ落とす）
- **oracle で測れないもの**: `:normal!` の 1 回の実行はまるごと 1 つの undo の単位になるので、`xxu` は Vim では `hello` に戻る（対話の Vim なら `ello`）。undo の区切りの fixture は「1 回の変更 → `u`」に限り、`i a I A` の出入りが単位を閉じることは手書きの単体テストで測る。
  また `:normal!` は失敗した鍵のあとの鍵を捨てることがある（`hx` は `h` が行頭で失敗するので `x` が効かない）ので、失敗する鍵は列の最後にだけ置く。CRLF の本文は Vim が `fileformat=dos` として CR を落とすので流せない（決定 9・手書きの単体テスト 1 本）
- **2026-09-18（Issue #44・CNF-010）**: 生成物の先頭に `// fixtures.json: sha256 … / 87 fixtures` の 1 行を足した。同じ 87 件を同じ Vim 9.1 で `--regenerate` し、
  **本体の配列は 1 バイトも変わらず、差分はこの 1 行だけ**（`git diff` で確認）。続けてもう 1 回走らせた生成物は同一（生成物の SHA-256 `edeae01f37be34e11b57e81c29e0292d8a57832d9f61e5717276639bbc585164`。
  上の `457e5495…` はこの行が無い版の値）。`fixtures.json` の SHA-256 は `15758fd401ff69391128aa748ceaf91a7441caed9553e8b35cde8ac07736ac48`

実機の窓（`python eng/verify-window.py`。終了 0・`out/window-verification/look-slice-results.json` の `editing.vim`）:

- トグルで Vim に入り、`i` でステータスバーのモード名の画素が変わり（INSERT）、`hello` の字形画素 218、Esc でモード名の画素が NORMAL の絵に戻る
- キャレット: 1 桁目の升の橙の画素は INSERT で 72、NORMAL で 202（ブロックはバーの 2 倍より広い）。1 文字の上に載るとブロックの中に字形が乗るので、画素 1 点ではなく升の中の橙の数で見る
- `0x` で字形画素が 218 → 196（`h` が 1 つ消えた）、`u` を 2 回で 0（`x` と挿入 1 回ぶんが別の単位に閉じている）

#### Vim の 2 本目（Issue #43・ADR 0015・2026-09-18）

oracle も機械も 5-g と同じ（Vim 9.1・同じ呼び方）。報告に `getregtype('"')` を足し、`KEY_NAMES` に `<Home>` `<End>` を足した。

- **fixture 87 → 195 件**（足したのは 108 件。`c` `y` `p` `P`・回数の掛け算・`e` `^` `D` `C` `Y`・`<Home>` `<End>`・
  行単位と文字単位の貼り分け・日本語）。`--regenerate` を **3 回**走らせて生成物は 1 バイトも変わらない
  （生成物の SHA-256 `b6302dc5b361f6c105f8f44c9ac354d3259be2ff1c61f6c6307d4c5ceea1918e`・`fixtures.json` の SHA-256
  `392af23f22fe2749a53d14b4f0fdcfd14740e0bba78605173d28b3ba86576f25`。CNF-010 の 1 行もこの値を名乗る）
- `nib_unit` が 195 件すべてを再生し、本文・キャレット・無名レジスタの**本文と種類**（`v` / `V` / 未使用の空）が全部一致（全体で 2335 件の検査）
- **oracle に実装を合わせた点**（規則を実装の前に書き切らず、答えに合わせたもの）:
  1. `dd` `yy` `cc` と `dj` `dk` の回数は、**最終行（最初の行）にいるときだけ**失敗して何も起きず、そうでなければ本文の端で止まる
     （Vim の `cursor_down` / `cursor_up`）。`5dd` は 3 行の本文を全部消し、`j2dd` は最終行では何もしない。#22 の実装は「はみ出したら何もしない」だったので直した
  2. **exclusive な移動の 2 つの言い換え**（`:help exclusive`）: 行頭で終わる `w` `b` の範囲は 1 つ前の行の末尾までになり、
     始まりが行の字下げの中なら**行単位**になる。`dw` が空行を丸ごと消すのも `db` が上の行を消すのも `cw` が空行で行単位になるのもこれで、
     レジスタの種類が `V` になることは `getregtype` を足して初めて見えた
  3. **`op_delete` の「奇妙な Vi の振る舞い」**: 複数行にまたがる文字単位の**削除だけ**は、終わりの後ろが空白だけかつ始まりが字下げの中なら行単位になる（`2D`・`de` の行またぎ）。`c` と `y` には無い
  4. `$` は回数を取る（`2$` は 1 行下の行末）。`D` `C` はその `$` に回数を渡すので、`2D` が「行末まで ＋ 次の行」になり、3 の規則で行単位になる
  5. `cw` の特例は「キャレットの下に**空白でない文字がある**とき」で、空行（NUL）では効かない（Vim の `gchar_cursor() != NUL && !VIM_ISWHITE`）。
     語の最後の文字の上では `ce` と違って**その 1 文字だけ**を変える（`end_word` の `stop`）
  6. 行単位の `y` のキャレットは範囲の最初の行の**同じ桁**（短い行では最後の文字へ寄る）。文字単位の `y` は範囲の先頭。`yy` と `yj` は動かない
  7. 空の文字単位の範囲（空行の `D`・行頭の `d0`・`c0`）は**無名レジスタを書き換えない**。`c` は範囲が空でも INSERT に入る
- **oracle で測れないものが増えた**: `:normal!` の 1 回が丸ごと 1 単位なので、`xxu` は Vim では `hello` に戻る（対話の Vim なら `ello`）ことを 2026-09-18 に測り直した。
  そのため `dd` → `p` → `u` のような**変更が 2 回ある fixture は置けない**（いったん置いた `undo-takes-back-a-put` は外した）。undo の単位は手書きの単体テスト
  （`verify_vim_insert_undo_unit` / `verify_vim_change_undo_unit` / `verify_vim_insert_motion_breaks_the_unit` / `verify_history_absorbing`）で測る
- **矢印で undo の単位が切れることも oracle では測れない**: `ia<Left>b<Esc>u` は oracle では `hello` に戻る（実測）が、これは上と同じ `:normal!` の限界であって対話の Vim の振る舞いではない。
  対話の Vim は `:help ins-special-special` のとおり矢印・Home / End で単位を切る（"The changes … before and after these keys can be undone separately"）＝ ADR 0015 の決定 5 のまま。
  **fixture には置けない**（置けば oracle の限界のほうに落ちる）ので、単体テスト `verify_vim_insert_motion_breaks_the_unit` が守る
- CRLF の文書のレジスタと `p`（レジスタは LF・貼った本文は CRLF・キャレットは CR のぶんずれる）は oracle に流せないので手書きの単体テスト（`verify_vim_put_line_endings`）
- 分岐カバレッジ（`python eng/coverage.py`）: 全体 92.20 %（1180 分岐中 1088・下限 90 %）。`src/core/VimStep.cpp` は 91.62 %（358 中 328）

実機の窓（`python eng/verify-window.py`・2026-09-18・終了 0・同じ機械）: `verify_vim` に `yyp` を 1 つ足した。
`0x` のあとの本文は 1 行（2 行目の字形画素 **0**）で、`yyp` で 2 行目に字形画素 **196** が出る（1 行目の `ello` と同じ数＝同じ行が貼られた）。
`u` は 3 回（挿入 1 回・`x`・`p` がそれぞれ 1 単位）で本文が空に戻る。画は `vim-put.bmp`。

#### Vim の 3 本目・VISUAL（Issue #53・ADR 0018・2026-09-19）

oracle も機械も 5-g と同じ（Vim 9.1・同じ呼び方・記法の追加は無し）。`v` `V` `o` は普通の文字なので `KEY_NAMES` は増えていない。

- **fixture 195 → 256 件**（足したのは 61 件。`v` / `V` の出入りと切り替え・`h j k l 0 $ ^ w b e` と `<Home>` `<End>` での広げ方・`o`・
  回数・`d x y c`・空行・日本語・行単位と文字単位のレジスタ・`u`）。`--regenerate` を **3 回**走らせて生成物は 1 バイトも変わらない
  （生成物の SHA-256 `7370ca5546a4371628ded3732f6d027568a03b32afc3fcc8e23dd25d114a3d0e`・`fixtures.json` の SHA-256
  `b24311a61e7e9d36bf7a3a33eb3f4577744b1bf9d583c2d4a71b5ba9076ddec4`。CNF-010 の 1 行もこの値を名乗る）
- `nib_unit` が 256 件すべてを再生し、本文・キャレット・無名レジスタの本文と種類が全部一致（全体で 2862 件の検査）
- **oracle に実装を合わせた点**:
  1. **VISUAL のキャレットは行の内容の終わり（Vim が NUL を置く桁）に載る。** `v$d` は改行まで消して次の行と繋がり（`"abc\ndef"` → `"def"`・レジスタ `"abc\n"` の `v`）、
     `llvld` も `"abdef"` になる。Vim の `coladvance` の `one_more` が `VIsual_active` で立つのと同じ。`vim_resting_caret` を当てるかどうかをモードで分けた（`rested_in`）。
     `$` の欲しい列（`at_line_end`）を持ったまま `j` で降りても NUL の桁に載る（`v$jd` は 2 行とも消える）
  2. **`op_delete` の「奇妙な Vi の振る舞い」（5-g の 3）は VISUAL には掛からない**（Vim の条件が `!oap->is_VIsual`）。`"  abc\n   "` の `vjd` は
     行単位にならず `"  abc\n "` を文字単位で消す。`whole_lines_for_delete` を通さない入口（`removed_exactly`）を分けた
  3. **`3v` は 3 文字・`3V` は 3 行を選ぶ**（ADR 0018 の草稿の決定 7 は「VISUAL に入る前の回数は捨てる」だった）。`:help v` が
     「前の Visual の操作が無ければ `[count]` 文字を選ぶ。カーソルを右へ N × `[count]` 動かすのと同じで、`'selection'` が `"exclusive"` でなければ 1 つ少ない」、
     `:help V` が同じく「`[count]` 行を選ぶ」と書いている。**実装は Vim に合わせた**（入った直後に `count - 1` だけ `v` は右へ・`V` は下へ）。ADR の決定 7 の括弧は次の改訂で直す
- **oracle で測れないもの**: `p` `u` `~` `>` `<` `J` `r` `I` `A` と `X` `D` `C` `Y` は本物の VISUAL では効く（`p` は選択を置き換え・`u` は小文字化・`D` は行削除…）が、
  この縦切りでは**何もしない**（ADR 0018 の決定 7・8）。同じ鍵の fixture を置くと oracle と食い違うので**置いていない**＝この差は機械が見張っていない。手書きの単体テスト
  `verify_vim_visual_step_edges` が「何もしないこと」だけを測る
- 表示の範囲と操作の範囲が同じであることは fixture では見えない（fixture は本文とレジスタしか見ない）ので、単体テスト
  `verify_vim_visual_selection_and_clipboard` が `EditorFrame` の選択スパンと Ctrl+C の中身を `vim_visual_range` と突き合わせる
- 分岐カバレッジ（`python eng/coverage.py`）: 全体 **91.29 %**（1354 分岐中 1236・下限 90 %）。`src/core/VimStep.cpp` は 89.88 %（494 中 444）、
  `src/core/VimVisualRange.cpp` は 91.67 %（12 中 11）。VISUAL の `switch` には届かない枝（表から引ける移動しか来ない `motion_for` の失敗側・
  `widened` の NORMAL / INSERT）があり、そのぶん #43 の 92.20 % から下がった

実機の窓（`python eng/verify-window.py`・2026-09-19・終了 0・同じ機械。`out/window-verification/look-slice-results.json` の `editing.vim`）:
`verify_vim` に `V` → `d` を足した。`yyp` で 2 行目に字形画素 **196** が出たあと、`V` でモード名の画素が NORMAL と変わり（VISUAL LINE）、
`d` で 2 行目の字形画素が **0** に戻る（行がまるごと消えた）。`u` は 4 回（挿入 1 回・`x`・`p`・`Vd` がそれぞれ 1 単位）で本文が空に戻る。

### 5-h. IME（IMM32）の縦切り（Issue #28・ADR 0014・2026-09-17）

環境: 5-g と同じ機械（Windows 11 Pro 10.0.26200・120 DPI・実 GPU・ダーク）。`build/NeNeNib.exe`（Debug 構成＝ASan / UBSan 付き）。
IME は **Microsoft IME**（日本語・`HKCU\Keyboard Layout\Preload` は `00000411` と `00000409`・`InputMethod\JPN` は 10.0.26100.1・`imm32.dll` 10.0.26100.9278）で、入力方式はローマ字・変換は既定。
Google 日本語入力・ATOK は**この機械に無いので測っていない**（ADR 0014 の「不能」のとおり。`GCS_COMPATTR` の癖は利用者の報告で拾う）。

手順（`python eng/verify-window.py` の `verify_ime`。**別の 1 回の起動**で測る）:

1. 隔離した `LOCALAPPDATA` / `APPDATA` で exe を起動して最前面にし、窓のスレッドの `GetKeyboardLayout` が `0x0411` であることと `ImmGetDefaultIMEWnd` が窓を返すことを確かめる。どちらかが欠ければ「この機械に日本語 IME が無い」と記録して節を飛ばす
2. **開閉（決定 5）**: 既定 IME 窓へ `WM_IME_CONTROL` の `IMC_SETOPENSTATUS` / `IMC_GETOPENSTATUS` を送る。これは**外から読み書きできる**ので、この節はキーボードに触らずに測れる。IME を on にしてから、通常モードで 1 文字 → Vim トグル（NORMAL）→ `i`（INSERT）→ Esc（NORMAL）→ 通常トグル、と**投げたメッセージだけ**で動かして、各段の開閉を読む
3. **変換（決定 2・3・7）**: 投げたメッセージは IME に届かない（`ImmProcessKey` は入力キューが本当に運んだ鍵にしか掛からない）ので、ここだけ前景を取ってから `SendInput` で本物の鍵を打つ。`n i h o n g o` → 画素 → Space → 画素 → Enter → 画素
4. 最後に**見つけたときの開閉に戻す**。前景が取れない・IME が変換しない環境では、その旨を記録して落とさない

結果（2026-09-17 の実測・`out/window-verification/look-slice-results.json` の `ime`）:

- **開閉**: on にして `1` → 通常モードで打っても `1`（**通常モードは IME に触らない**）→ Vim の NORMAL で `0` → `i` で `1`（控えた値が戻る）→ Esc で `0` → 通常モードへ戻して `1`。機械の開閉は見つけたときの `0` に戻した。ここは 7 つとも**表明**（落ちたらゲートが赤くなる）
- **変換**: 前景が取れた。`nihongo` で本文の行に「にほんご」が出て、字形の画素 344・`ime`（#D7C4E5）の下線の画素 **111**・橙の画素 72（変換中のキャレットのバー）。
  Space で注目文節が付き、橙の画素が **72 → 135**（`accent` の 2 DIP の下線と `selection` と同じ面）・その面を含む画素が 1306。
  Enter で本文に「日本語」が入り、字形の画素 306・`ime` の下線の画素 **0**（変換中の下線が消えて本文の字色になった）。画は `ime-slice.bmp` / `ime-slice-converted.bmp` / `ime-slice-committed.bmp`
- 変換中の「にほんご」は `TextBuffer` に入っていない（`EditorState` の `std::optional<Composition>`）。**本文と履歴が変わらないこと・確定 1 回が undo 1 単位であることは単体テストの側**で測る（`verify_composition_ordinary` / `verify_composition_state`）。画素では「本文に無い」ことを直接は測れない
- **手で確かめたもの（画面全体の写真で。ゲートは見ていない）**: 候補窓は別プロセス（TextInputHost）の窓なので、窓のクライアント領域の画素には写らない。画面全体を撮って目で見た。
  Vim の INSERT で `nihongo` を打つと、変換中の「にほんご」の行の**直下・変換中の文字列の左端に寄った位置**に Microsoft IME の候補の一覧（1 日本語 / 2 日本語フォント / …）が出た
  ＝ `ImmSetCandidateWindow(CFS_CANDIDATEPOS)` にキャレットの左下を渡した結果である（決定 6）。写真は `out/window-verification/ime-manual-candidate-window.png`（git の対象外）
- **手で確かめたもの**: 変換中の Esc は変換だけを取り消し、**Vim の INSERT のまま**である（決定 5）。上と同じ場面で Esc を打つと、変換中の文字列が消えて本文は `abcdef` のまま・
  ステータスバーは `INSERT` のまま・桁も 6 のままだった（Esc は IME が食ったので Vim の鍵にならなかった）。写真は `ime-manual-escape-in-insert.png`
- 同じ写真で、**Vim の INSERT で日本語が打てる**ことも確かめた（引き継ぎ 09-17 の「未確認」に挙がっていた項目）
- **Space に対する IME の答えは一定しない**: 予測候補の一覧がもう出ていると、Space は変換に入らずその候補を確定することがある（5 回のうち 1 回）。
  そのため注目文節の橙の増加は**表明していない**（`spaceOpenedATargetClause` に記録するだけ）。変換中の `ime` の下線と、確定で下線が消えて本文に字が入ることは毎回表明する
- **実行のばらつき**: `python eng/verify-window.py` を 8 回走らせて 6 回は終了 0。落ちた 2 回のうち 1 回は上の Space（この版で記録に変えた）、
  もう 1 回は IME とは無関係の `verify_missing_document`（起動引数のファイルが無い節）で、題名が `● 無題` になっていた＝空の本文に鍵が 1 つ入っていた。
  この節の前に走る `try_saving` が `SendInput` で本物の Ctrl+S を打つので、離鍵を取りこぼすと次の窓に鍵が流れ込む見立て。**同じ並びを 3 回再現しても出なかった**ので原因は確定していない。
  IME の節を足す前からある `SendInput` の経路の話で、この Issue の変更とは切り離して見るべきもの（設計リナへの申し送り）
- **測っていないもの**: ライトの外観での `ime`（#5E2750）の下線。96 DPI。Google 日本語入力・ATOK。IME の既定の変換窓に頼る古い IME（ADR 0014 の「正直に」のとおり）
### 5-i. タブの帯の画素（Issue #31・D16・2026-09-17）

環境: 5-h と同じ機械（Windows 11 Pro 10.0.26200・120 DPI・実 GPU・ダーク）。`build/NeNeNib.exe`（Debug）。クライアント 800×450。

手順（`python eng/verify-window.py` の `verify`）: 帯の地（幅の中央・帯の高さの半分。同じ点の `WM_NCHITTEST` が `HTCAPTION`＝タブでも ＋ でも窓の操作でもないことを同時に表明する）と、
アクティブなタブの内側（`active_tab_point`: 題名の左余白の半分・タブの高さの半分。角丸・字形・橙の下線を避けた点。120 DPI で (19, 30)）を 1 点ずつ読む。

結果（`out/window-verification/look-slice-results.json`・`look-slice.bmp`）:

- 帯の画素は **(30,5,22)** ＝ `title_bar` #1E0516、アクティブなタブは **(48,10,36)** ＝ `tab_active` #300A24 ＝本文の地。3 つとも表明（`titleBarPixel` / `activeTabPixel` / 「タブの面は本文の地と同じ」）
- 5-c の「タイトルバー中央は (42,42,42) で Mica が透けている」は D16 で置き換わった。帯は不透明なので Mica は隠れる。`DwmSetWindowAttribute` の 2 属性は掛けたまま（ADR 0008 の決定 9 への追記）
- 起動直後の面（5-f / ADR 0013 の `verify_first_paint`）は**変わらない**。あの節が見ているのはクライアント領域の中央＝本文の地が来る場所で、帯ではない。この実行でも最初のフレームまでの画素は (32,32,32) の Mica で、黒も白も出ていない
- ライトの外観（帯 #E1E4E9・タブ #F4F5F7）は**画素では測っていない**。表明は `TITLE_BAR` / `TAB_ACTIVE` の表にあり、OS をライトにすれば同じ経路で測れる。96 DPI と非アクティブなタブの面（D16 では変えていない）も測っていない
- **施主の目視（2026-09-17 夜・Release `build-release\NeNeNib.exe`・実機 120 DPI）**: ダークの帯とアクティブなタブが案 C の絵と同じ。OS をライトに切り替えても起動したまま追従し、帯 #E1E4E9 / タブ #F4F5F7 の見た目。IME の変換・確定・Vim NORMAL での切断も「問題ない」（hide）。#31 の受け入れ条件の 3 つ目

### 5-j. Vim の表示領域 oracle（Issue #58・ADR 0019・2026-09-19）

oracle は 5-g と同じ固定した Vim 9.1。help と鍵を送った結果だけを参照し、Vim の実装ソースは参照しない。

- `-es` で `set lines=10/20` を実行しても `winheight(0)` は 24。`:resize` では高さが変わるが、`winsaveview().topline` と `line('w0')` / `line('w$')` が矛盾するため、画面移動の期待値には採用しなかった。
- `-es` を外す通常端末モードでは、標準の Python `subprocess` のパイプだけで `resize` と `winrestview` の入力に一致する画面を得た。PTY・追加依存・実キーボード入力は不要。最終版は `--not-a-term` で pipe を意図した実行と明示する。同梱 help の説明どおり警告と 2 秒の待機だけが消え、通常・VISUAL の画面移動と yank の結果は同じだった。
- 高さ 10、カーソル 50 行の初期画面 46〜55 行では `H / M / L` が 46 / 50 / 55 行、`Ctrl-d` がカーソル 55・先頭 51 行となった。高さ 20 の初期画面 41〜60 行では同じ鍵が 41 / 50 / 60 行、`Ctrl-d` がカーソル 60・先頭 51 行となった。
- `eng/vim-oracle.py` の任意の `viewport` 入力がこの実行方法を選ぶ。初期の高さ・カーソル・表示先頭と、最終の保存先頭・表示先頭・表示末尾の整合を検査し、失敗や 30 秒の timeout は fixture にしない。画面依存の fixture は折り返しを止め、既存 256 件の設定と `-es` は変えない。
- fixture は本文・カーソルの行と UTF-8 バイト桁・レジスタの本文と種類に、表示先頭と window-local な `'scroll'` を加える。末尾にもう 1 つ Esc を送った結果も一致しなければ拒否する。単体テストと CI の再生には Vim を必要としない。
- `python eng/vim-oracle.py --regenerate` を最終の **329 件（既存 256 ＋追加 73）**で 2 回実行し、生成ヘッダの SHA-256 はともに `814d77cddb785aa0aa9a01fde186d74ec7b34e14025bda94b519bfe468b120e4`。`fixtures.json` は `5b39fb1da0521b537a2fa6780283e852ee70f54518c0f3793fd1d69e3de8d416`。元の 256 件の入力と期待値 8 項目は `origin/main` と比較し、全件不変を確認した。途中の 313 件から追加した後も、先頭 313 行の期待値は不変だった。
- `python eng/conformance.py` は違反 0、`python eng/test-conformance.py` は 131 件成功。後者には表示領域の不正入力・実行モードと timeout 指定・旧 fixture の空の表示領域・新しい生成行のテストが含まれる。
- 最終 329 件の生成物を含む Debug build は成功。`nib_tests` は 3954 checks、CTest は 3/3 成功。`python eng/coverage.py` は全体 **91.51 %**（1566 分岐中 1433・下限 90 %）、検出能力を確認する negative proof は 2.87 % だった。

`python eng/verify-window.py` は Debug・Windows 11 build 26200・120 DPI・ダーク・実 GPU で終了 0。高さ 11 行の画面で H / M / L のアクセント画素は 208 / 266 / 266。先頭の文字の画素は 392、PgDn 後は 0、PgUp 後は 392。実入力の Ctrl-d/u と Ctrl-f/b も両方 `confirmed`、各々 392 → 0 → 392。undo 後は 0。既存の通常編集・ファイル・IME の検査も通過した。

検査側の 2 回の失敗は、`WM_CHAR` の改行が Enter と同じではないことと、丸角キャレットの端の 2 画素を文字と誤認したこと。前者は `VK_RETURN` の経路へ直し、後者は文字の検査領域をキャレットの外へ置いた。空行の期待値 0 と H / M / L の期待位置は変えていない。途中画面は `out/window-verification/vim-viewport-*.bmp`、最終結果は同ディレクトリの `look-slice-results.json`（git 対象外）。

この追加操作の実機確認は 120 DPI・ダークでの結果であり、96 DPI や別の外観を実測したとは扱わない。最終 HEAD のフルゲートと CI の結果は [PR #59](https://github.com/hideyukiMORI/nene-nib/pull/59) に記録する。

### 5-k. 設定と本文フォント（Issue #60・ADR 0020・2026-09-20）

対象は設定の読込/保存/競合、新しい中核状態、本文フォントからの配置・クリック・IME、起動/打鍵への影響。ADR 0021 に従い既存の成功を再利用し、フルゲート・Vim oracle再生成・無変更の16 MiB入出力ベンチは実行しない。

| 検査 | 実測 |
| --- | --- |
| `cmake --build build --target nib_tests NeNeNib` と `build/nib_tests.exe` | Debug + ASan/UBSan、3,992 checks成功。サイズ境界・NaN/inf・設定失敗・本文/選択/undo保持・明示テーマ/system・96/120/192 DPIの配置。公開状態/ctorの全呼出元を同じunit targetが覆う |
| `cmake --build build --target nib_adapter_tests NeNeNib` と `ctest --test-dir build -R '^nib_adapters$' --output-on-failure` | 109 checks成功。初回/再起動、UTF-8/BOM/CRLF/ASCII空白/名前中の=、重複/未知キー/欠落/版/非数、4096 bytes境界、競合・lock・read-only、失敗後の元bytes保持。独立レビューのVT/FF修正後はこのtargetだけ再検証 |
| `python eng/symbols.py --build-dir build --require core application` | 新しい中核の外部依存を確認。2 libraries、0 violation(s) |
| `python eng/coverage.py` | 新しい状態と失敗分岐を含め1,454/1,588 = 91.56%。90%下限維持。negativeは2.96%で拒否 |
| `python eng/verify-settings.py` | Windows 11 / 120 DPI / Microsoft日本語IME。主キー/テンキー、通常/Vim、Ctrl+wheel差分累積、8/40pt境界、復帰、21.5ptでのクリック→保存の本文一致、再起動、18pt/Consolas/neutral-lightの復元を確認。画面は out/settings-verification/*.bmp |
| 上記から既存 `verify_ime(..., points=21.5)` | 実打鍵で変換→候補→確定。変換中下線401 pixels、確定後0。IME開閉状態を復元。out/settings-verification/settings-results.json |
| `python eng/verify-settings.py --only unchanged-size` | 最大40ptでスクロール後、Ctrl+拡大しても前後の画面bytesが一致。no-opで表示行数通知を送らない修正の回帰検査 |
| `cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release` / `cmake --build build-release --target NeNeNib` | Release製品target成功 |
| `python out/git/issue60-speed.py`（既存 measure-speed の prepare / bench_startup / bench_keys / summarise / compare を使用、5試行） | 指紋 bc8a356f37c68491・120 DPI。起動初回描画209.147ms / 窓31.7409ms / 単打鍵0.803ms / 200打鍵2.419ms。既存基準値・許容値で4指標とも退行0・欠測0。out/speed/issue60-selected.json |

実機の最初の成功後、空白のcodec修正はadapter検査で、比例係数/OSテーマの共通化は既存unitとsymbolsで確認。無変更時のスクロール修正は該当nativeケースだけ追加した。全実機シナリオを繰り返していない。独立レビューは読み取り専用、追加の未解決指摘なし。

未確認: 別モニターへのDPI移動、変換中の拡縮での候補窓の目視、他IME。外部エディタが比較後に書く競合は保証外。FR-016/FR-017のEx/選択UI部分はC3。Waivers: none。

### 5-l. Ex入力と設定コマンド（Issue #64・ADR 0022・2026-09-20）

対象はNORMALのEx入口、本文と独立した入力、設定の共通保存、入力欄/補完/結果の描画とイベント配線。既存C2のファイル競合・不変の保存形式・IME変換、変更していないVim操作の成功結果を再利用する。全件unit・全件GUI・coverage全件再測定・Vim oracle生成・16 MiB入出力は実行していない（QLT-001 / QLT-012）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build --target nib_tests nib_adapter_tests NeNeNib --parallel 4` | 閉じた意図/効果、公開状態と直接呼出元。Debug + ASan/UBSan + tidy成功 |
| `build/nib_tests.exe --ex-settings` | 153 checks成功。設定の共通保存/失敗/同値、全テーマ・system、pt解析、未対応構文、UTF-8編集、Tab巡回、配置、本文/undo/レジスタの保持 |
| `build/nib_adapter_tests.exe --settings-codec` | 23 checks成功。pt解析をcoreへ移した直接呼出元の保存形式と値の拒否を確認 |
| `python eng/symbols.py --build-dir build --require core application` | 2 libraries / 0 violations。`std::format`が文字列版でも持ち込むlocale参照を検出して除去。追加許可は固定STLの不変表と純走査3シンボルだけ（ADR 0022） |
| `python -m unittest discover -s tests/conformance -p test_symbols.py -k ex_stl -v` | 許可した3シンボルと、locale/未知関数/範囲外STLの拒否を1 test内で確認、成功 |
| `python eng/verify-ex.py` | Windows 11 / 120 DPI。NORMAL入口、Tab、Home/End/Delete/Backspace、Esc、Ctrl+Zの本文への流出防止、テーマ切替、Consolas 21.5pt、無効値、長文clipとcaret追従、右status/本文保持、再起動。`out/ex-verification/ex-results.json` とBMP |
| `python eng/verify-ex.py --only input-guards` | 独立レビュー指摘への追加検査。tabクリックのcaret保持、結果表示中の隠れたtoggle非作動、実SendInput Ctrl+Alt+C/Vで画面不変、guifont内pipe拒否。`out/ex-verification/ex-guards-results.json` |
| `cmake --build build-release --target NeNeNib --parallel 4` | 新UI文字書式の起動・通常入力の経路を測るRelease製品target、成功 |
| `python out/git/issue64-speed.py`（既存 measure-speed 関数を対象指定、5試行） | 指紋bc8a356f37c68491 / 120 DPI。中央値: 初回描画206.5191ms、窓31.8138ms、単打鍵1.157ms、200打鍵2.463ms。既存基準で退行0・欠測0。`out/speed/issue64-selected.json` |
| `python eng/conformance.py --build-dir build` / changed C++ clang-format / `git diff --check` | 新ファイルの所属/依存と文書参照、変更ファイルの整形と空白を確認、成功 |

独立レビューは読み取りのみ。pipe、AltGr、本文外クリック、結果中のhidden toggleの4点を修正し、未解決指摘なし。検証後の文書・commit・push・mergeでは同じ成功結果を使う。保存schema変更なし、新規runtime依存なし、Waivers: none。

未対応: 一般Ex、範囲/回数付きEx、VISUAL範囲、VimScript、`:w`/`:q`、履歴、Ctrl+P、専用フォント選択UI。ExのIMEはNORMALと同じ閉状態、Unicode名はUTF-8/単行貼付。nativeは120 DPI、日本語キーボード環境での実測。Ctrl+PなどFR-016全体は後続。

### 5-m. Ctrl+P設定一覧（Issue #66・ADR 0023・2026-09-20）

対象は候補の照合・選択、Exと共通化した入力session、設定評価/保存、Win32の配送と採用済み一覧の描画。保存形式/競合/adapter、Vim engine、Unicode編集本体は不変で5-k/5-lの成功を再利用する。新規依存・schema・waiver・全件検証なし。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build --target nib_tests NeNeNib --parallel 4` | 新しい閉じた意図、状態と直接呼出元。Debug + ASan/UBSan + tidy成功 |
| `build/nib_tests.exe --command-palette` | 最終130 checks成功。9テーマ/system、大小文字/部分列/順位/未知theme、fill、Unicode編集と256 bytes、候補巡回、96/120/192 DPI配置、保存失敗、本文/選択/undo/Vim count/pending保持、Exとの排他、遅れて届くIME/変換中の入口拒否 |
| 初回の同selector（当時は `verify_ex_settings` も併実行） | 初回274 checks成功のうちEx153 checksを再利用。後のpalette候補修正はEx側のコードを変えず、selectorをpaletteだけに絞って再実行した |
| `python eng/verify-palette.py` | 120 DPI、設定用に隔離したprofile。通常/Vim NORMAL/INSERT/VISUAL/VISUAL LINE、選択/caretの画面bytes保持、IME閉/復元、テーマ部分列、font値補完、クリック適用と配置、無効値、ホイールの本文/拡縮への漏れ防止、460x360狭窓、長入力clip、undo、本文保存一致、再起動。`out/palette-verification/palette-results.json` とBMP |
| `python eng/verify-ex.py` | 共通sessionと入力/表示配送を変えた直接呼出元。Exの入力/Tab/編集/設定/本文と右status保持/長文clip/再起動が成功。5-lのinput-guardsは不変なので再実行なし |
| `python eng/verify-palette.py --only surface` | 画面確認後に加えた候補件数と、独立レビューで修正した未知themeの経路だけ追加確認。件数描画、設定を書かず閉じて新たに一覧を開けることが成功。`palette-surface-results.json` とBMP |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py --build-dir build` | 新しい候補順位と状態の境界、ファイルの所属/依存を確認。2 libraries / 0 violations、conformance 0 violations。許可表は変更なし |
| `cmake --build build-release --target NeNeNib --parallel 4` / `python out/git/issue66-speed.py` | 共通状態と通常のキー/frame経路への負荷だけ測定。既存measure-speedの起動/打鍵を5試行、指紋bc8a356f37c68491 / 120 DPI。中央値: 初回描画211.0728ms、窓33.7074ms、単打鍵0.872ms、200打鍵2.483ms。退行0・欠測0。`out/speed/issue66-selected.json`。無関係なファイルI/Oは測定しない |
| changed C++ `clang-format --dry-run --Werror` / `git diff --check` | 変更ソースの整形と差分空白を確認、成功 |

独立レビューは読み取り専用。`colorscheme missing` が無候補で止まる不一致を修正し、完全な未知themeの入力はExと同じエラーへ接続した。部分的な名前の絞り込みは維持する。文書・commit・push・mergeだけでは検証を繰り返さない。

未対応: Ctrl+Pのファイル/履歴/フォルダ/ブックマーク統合、一般Ex、専用フォント一覧、利用者テーマ。nativeは120 DPI/Microsoft日本語IME、別モニターへのDPI移動・他IMEは未確認。Waivers: none。

### 5-n. 利用者テーマの形式・所有・読込（Issue #68・ADR 0024・2026-09-20）

C4aのcodecと新しい所有値、共通のfield解析へ変えた既存SettingsCodecだけを検証した。UIへの接続はまだ無く、GUI/性能/Vim/保存競合/全件検証は実行していない。名前/文字列の意味はcore、形式/OS/libmはadaptersに隔離した。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build --target nib_theme_tests nib_adapter_tests --parallel 4` / `ctest --test-dir build -R '^nib_themes$' --output-on-failure` | Debug + ASan/UBSan + tidy、127 checks成功。本文/UI全トークンの割当、導出と上書き、UTF-8・BOM/CRLF/空白、RGB/RGBA、版・名前・reserved・重複/未知/欠落、色/メタデータ拒否、4.5の両側、16KiBちょうどと超過、copy/moveと入力破棄後のview、実FilePortのmissing/正常/壊れた/大きい/directoryを確認。`build/Testing/Temporary/LastTest.log` |
| `build/nib_adapter_tests.exe --settings-codec` | 23 checks成功。field分割の唯一の旧呼出元について受理/拒否と出力の同一性を確認。設定schemaは不変 |
| `cmake --build build --target nib_tests nib_theme_tests --parallel 4` / `build/nib_tests.exe --user-theme-values` | 21 checks成功。名前の18 checksをadapterからpure coreの通常単体へ移し、ThemeDocumentの独立copyとviewの3 checksも追加。移動だけではadapter全件を繰り返さない |
| `cmake --build build --target nib_theme_tests --parallel 4` / build内で `nib_theme_tests.exe --file-loading` | 独立レビューのbasename指摘への修正。loader自身が期待名をパスから導く2引数APIに変更。誤拡張子/underscore/空stemをread前に拒否、本文nameとの一致を19 checksで確認。codec/所有値/設定の成功は不変なので再利用 |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py --build-dir build` | 2 libraries / 0 violations、conformance 0。新coreはOS/libmを参照しない。外部依存許可は変更なし |

`ThemeDocument`のrvalueから借りるAPIはdeleted overload。実行時の所有/寿命はASan対象テストで確認。独立レビューは読み取りのみで、basename修正後に追加指摘なし。変更C++のclang-formatと差分空白も成功。利用者向けの形式と例は `docs/design/user-theme-format.md`。既存settingsの形式は変えない。C4bのカタログ/選択/保存/Ex/Ctrl+Pへの接続は未実装。Waivers: none。

### 5-o. 利用者テーマの選択・保存・復元（Issue #70・ADR 0025・2026-09-20）

対象はThemeChoice/ThemeCatalogの不変共有、SettingsIssueの名前付き失敗、ThemePortからの起動時読込、設定codecとEx/Tab/Ctrl+Pの共通検索。設定の4キー、C4aの色/UTF-8/コントラストcodec、Vim engineと本文編集は不変。全件・coverage・無関係なGUI/ファイルI/Oベンチは実行しない。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build --target NeNeNib nib_tests nib_adapter_tests nib_theme_tests --parallel 4`（変更後は該当targetのみ増分build） | Debug + ASan/UBSan + tidy成功。core/application/adaptersと直接のUI/合成ルートを確認 |
| `build/nib_tests.exe --user-theme-selection` | 最終88 checks成功。catalog順序/予約/重複/名前一致/alias、選択参照の所有、named failure、Ex補完/編集/Palette fillでのcatalog保持、保存/失敗と本文・undo不変。初期文書付きnotice、settings失敗でも文書を開くこと、一度だけread、blocked状態での同値操作も失敗保持。rvalueからのborrow禁止はrequires/static_assertでコンパイル時にも確認 |
| `build/nib_tests.exe --ex-settings` / `--command-palette` | 直接変更した設定選択と共通候補・入力の旧契約を153 / 130 checksで確認。persist_settings修正後は直接影響する前者だけ再実行し成功。後者の入力操作/配置は初回成功を再利用 |
| `build/nib_adapter_tests.exe --settings-codec` | 23 checks成功。既存の4キーと組み込み/systemの互換、missing名の保持 |
| build内で `nib_theme_tests.exe --catalog` | 301 checks成功。隔離フォルダの欠落・不正名/予約名・正常/破損・非再帰・順序・長いUnicode名の診断・128件/129件、設定保存と新adapterでの復元、missing/brokenで後続writeを拒否し元bytes保持。後のAPI ref限定だけでは実行を繰り返さず、再コンパイル成功と初回結果を再利用 |
| `python eng/verify-user-themes.py` | 最終成功。120 DPI/隔離profile、Ctrl+P利用者選択、NORMALのTab補完、独自body/title色、本文保存一致、broken選択時に設定bytes不変、再起動復元、壊れた保存済みthemeの名前付き警告と同値操作を含むwrite block。`out/user-theme-verification/results.json` とBMP |
| `python eng/verify-user-themes.py --only startup-notice` | 初期ファイル付きでnoticeを表示。`startup-notice-with-document.bmp`を目視し本文と左statusのinvalid filename診断を確認 |
| `python eng/conformance.py --build-dir build` / `python eng/symbols.py --build-dir build --require core application` / changed C++ clang-format / `git diff --check` | 新しい境界/所属/借用制約、2 libraries、いずれも違反0 |
| `cmake --build build-release --target NeNeNib --parallel 4` | 起動時読込と通常のframe/キー経路の測定用、成功 |

native初回は結果メッセージ上のクリックをtoggleと誤認した検証手順で停止。次はEx/Palette失敗がinline表示なのにmodalを期待した検証側の誤りで停止した。操作契約に合わせて修正し、saved-errorだけを追加確認。その後productionの初期入力/no-op修正が入ったため、関係する起動・復元・保護workflowを再実行した。成功するまでの無変更再試行はしていない。

読み取り専用の独立レビューで、一時所有者からのborrow、初期文書によるnotice消去、blocked時同値の成功扱いを修正。追加指摘なし。列挙途中失敗は集合と上限を確認できないため、部分集合を採用せず組み込み＋noticeとする判断をADRへ明記。形式はuser-theme-format.md。runtime依存・settings schema・waiver変更なし。nativeは120 DPIで、他IME/DPI移動は本件では測らない。検証対象が不変のpush/review/mergeでは同じ成功結果を再利用する。

性能は `python out/git/issue70-speed.py` で既存measure-speedの起動/打鍵4指標だけを5試行。指紋bc8a356f37c68491 / 120 DPI、中央値は初回描画209.9973ms、窓表示32.813ms、単打鍵1.14ms、200打鍵2.627ms。退行0・欠測0（`out/speed/issue70-selected.json`）。通常の空profileの基準比較であり、128個の最大サイズテーマを読む最悪時間の保証とは扱わない。

### 5-p. Vimの行内文字検索（Issue #72・ADR 0026・2026-09-20）

対象は `f/F/t/T` と `;` / `,` の待ち・記憶・回数・行内UTF-8走査、既存operator/VISUALへの範囲接続、およびoracle生成器の対象指定。設定・テーマ・ファイルcodec・描画・保存schemaは不変であり、無関係な検証は再実行しない。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `python -m unittest tests.conformance.test_vim_oracle` | 10 tests成功。選択だけを測定し旧期待行を維持する正例、入力・SHA/件数・Vim版/場所/設定・測定AST/既存import・行順の不一致拒否、コメント/docstring変更の許容、空/未知prefix拒否 |
| `python -X utf8 eng/vim-oracle.py --regenerate --only char-search- --reuse-ref b6d19ff` | 新しいCLIの実Vim接続と混合生成を確認。60 measured / 329 reused。固定Vim 9.1（2024 Jan 02, compiled Jan 3 2024 23:53:58）。`out/issue72-oracle/generation.log` |
| `parsed_header` と `fixture_row` による生成行照合 | 旧commitの329行を逐語維持。追加60行は探索時 `out/issue72-oracle/fixture-results.json` と完全一致。現在の入力/生成物389件のSHAと本数も同じparserで一致 |
| `. ./eng/toolchain.ps1` → `cmake --build build --target nib_tests NeNeNib --parallel 4` | 新しい状態と閉じた命令、直接呼出元をDebug + ASan/UBSan + tidyでコンパイル。成功 |
| `build/nib_tests.exe --vim-character-search` | 621 checks成功。追加60fixture、文字待ちでの数字/命令文字、記憶/反転/取消/未発見/欲しい列、制御キー、最大count、モード切替、CRLF、undo、空/未設定/通常register。既存step/visual/viewport状態、insert/change undo、put改行の直接呼出元だけ選択 |
| `python eng/symbols.py --build-dir build --require core application` | 行内走査と追加した所有値がOS/locale等の参照を増やしていないことを確認。2 libraries / 0 violations。`out/issue72-oracle/symbols.log` |
| `python -X utf8 eng/conformance.py --build-dir build` | 新しい型の所属/依存、文書参照、389件の生成物整合を確認。最終0 violations。`out/issue72-oracle/conformance.log` |
| 変更C++の `clang-format --dry-run --Werror` / `git diff --check` | 新規headerを含む差分の整形・空白を確認、成功 |
| `python -X utf8 eng/verify-vim-character-search.py`、修正後は `--only unicodeSurrogatePair` / `delete` / `change` / `yank` | 隔離profileで7シナリオ成功。検索待ちがdirtyにしないこと、Esc後のxとundo、ASCII/BMP日本語/絵文字の検索、d/c/yの保存本文とundoを既存Win32入力経路で確認。後続4件の実測は120 DPI。`out/issue72-native/vim-character-search-results-summary.json` |

探索では同じ固定Vimの `getcharsearch()` / `getcurpos()` を追加観測した。生の記録は `out/issue72-oracle/priority-probes.json` / `control-special-probes.json`。fixtureにしない取消後のVISUAL継続、失敗後の新しい打鍵、欲しい列の保持は、複数の `normal!` を分けて観測し単体テストの根拠とする。CRLFの原データもVimで `fileformat=dos` を確認した。Vimのソースコードは参照していない。

探索用スクリプトを増補する際、既存ケースを再実行する構造のまま走らせた不備があった。priorityの最終48ケースに対して累計118回、既存分70回が余分な測定。以後は新規対象だけの実行へ変更した。探索fixtureは60件の通常/+Esc比較と入力を訂正した3件の比較、別の制御キー4件を含め、探索段階のVim起動は248回。全329件のoracle再測定は行っていない。正式生成後は同じ入力・測定処理・Vim版の再実行を行わない。

空範囲のレジスタ境界は追加の新規4ケースだけを観測した。`adjacent-T-register-probes.json` の3ケースと `existing-empty-yank-probe.json` の1ケースで、空yankの種別更新と空delete/changeの旧値保持、既存 `y0` にも同じ意味があることを確認した。

読み取り専用の独立レビューでは、非空レジスタを持つ空delete/changeの検査不足を指摘し、`yy`で行単位の値を作った後に `$dTa` / `$cTa<Esc>` が本文とレジスタを維持する2ケースを追加した。追加差分の確認後は未解決指摘なし。yankは空範囲も既存 `register_of` の経路へ揃えた。

初回buildではtidyが走査の入れ子2件、命令分岐の長さ2件、optionalの保証3件を検出し、共通の一致判定・命令ごとの小関数・明示した保証へ分割した。次にテスト入口の複雑度1件を、commandの一度の正規化で修正した。分割後の命令振り分けも独立レビューで元の意味との同等性を確認済み。conformanceはその際追加した走査値の型配置をCNF-002で検出したため、同名headerへ移して違反0を確認した。ゲートの抑制・緩和は行っていない。

型配置変更は同じstruct定義の移動だけなので、coreの増分buildと対象2実行ファイルの再リンクで確認し、unit621とsymbolsの成功結果を再利用した。再リンク記録は `out/issue72-oracle/final-build.log`。その後の文書・push・review・mergeでも関連入力が同じ結果は再実行しない。

native初回はpending/Esc・ASCII・BMP日本語を通過した後、検証側が非BMPのcode pointを単一WM_CHARとして送っていたため絵文字で停止した。刺激をUTF-16のcode unitへ分けて既存driverへ渡すよう直し、`--only`で絵文字と未実行のoperator3件だけを確認した。製品の入力経路は変更していない。初回3件の成功は途中まで到達した実行と保存結果、後続4件は各実行出力を集約した記録であり、7件を最後にまとめて再実行したものではない。

結果の保存も各selector別ファイルと成功直後のcheckpointへ改善し、`py_compile`で確認した。保存の変更だけではnativeを再実行していない。NORMAL/VISUALのIMEを閉じる既存仕様は継続し、Unicode確認はWM_CHARから届く文字だけを対象とする。保存形式・runtime依存は不変、Waivers: none。

### 5-q. Vimの指定行移動（Issue #76・ADR 0027・2026-09-20）

対象は `gg` / `G`・絶対行番号、operatorとmotionの明示count、排他的な次キー待ち、既存の行単位範囲とVISUALへの接続。新しい待ち表現が文字検索にも直接影響するため、その既存contractも対象とする。描画、ファイルcodec、テーマ/設定、入力driver、oracle生成器は不変。

固定Vim 9.1（2024 Jan 02, compiled Jan 3 2024 23:53:58）の既存 `measure` に48種類の入力を一度ずつ渡した。通常/+Escの比較が各1回で計96起動、別の分割normal probeが6起動。`out/line-jump-oracle/measure_cases.py <新規fixture名...>` の完了名guardと逐次保存により、完了ケースの再実測を拒否する。gの取消/無効入力後の継続は `python -X utf8 out/line-jump-oracle/prefix_probes.py` で確認した。Vimソースは読んでいない。

採用は42件。nostartofline参考3件、同じnormal内で残りのキーが捨てられる無効prefix2件、重複したVISUAL到達点1件は採用しない。名前だけの3件の訂正はREADMEの対応表に残し、再測定しなかった。元入力・測定結果・採用入力は `out/line-jump-oracle/{fixture-inputs,fixture-results,candidate-fixtures}.json`、分割probeは `prefix-results.json`。

`python -X utf8 out/git/assemble-line-jump-fixtures.py` は実測済みの42件を既存 `fixture_row` / `header_from_rows` で生成した。旧main `2e222818230e591a58a28040ec67735ccb693366` の入力/生成物を `parsed_header` で読み、389行の逐語一致、入力順/件数/SHA、測定AST/import、固定Vimの場所/版/既定設定、新規入力全体と実測時入力の一致を確認。新しい実Vim測定0、計431件、SHA-256 `ef1760d91b52228ae4b52329a36b2ba307765c9fbfb651c4560f339285acdb86`。証拠は `out/line-jump-oracle/assembly-result.json`。生成器の実装は変わらず、#72のツール10 testsを再利用する。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `. ./eng/toolchain.ps1` → `cmake --build build --target nib_tests NeNeNib --parallel 4` | 新しい和型・閉じた命令と直接呼出元のDebug + ASan/UBSan + tidyが成功。初回の関数長2件は役割別helperへ分割して解消。`out/issue76-unit/build-first.log` / `build-second.log` / `build.log` |
| `build/nib_tests.exe --vim-line-jumps` | 新42fixture、排他的wait/count/取消、巨大count、CRLF/undoと、直接影響する既存文字検索scope（621 checks）を重複なく実行。初回989 checks中987成功、2件は下記の切り分け。42fixtureに失敗なし。`out/issue76-unit/vim-line-jumps.log` |
| `cmake --build build --target nib_tests --parallel 4` → `build/nib_tests.exe --vim-line-jump-recovery` | 変更した続行・undo確認の2関数だけを再実行し15 checks成功。製品コードは不変で、他の成功987 checksは再利用。`out/issue76-unit/build-recovery-final.log` / `recovery.log` |
| `python -X utf8 eng/symbols.py --build-dir build --require core application` | 和型と行索引を使う中核にOS/時刻/locale等の参照が増えていないことを確認。2 libraries / 0 violations。`out/issue76-unit/symbols.log` |
| `python -X utf8 eng/conformance.py --build-dir build` | 新headerの型配置・依存と431件の生成物整合を確認。0 violations。`out/issue76-unit/conformance.log` |
| 変更C++の `clang-format --dry-run --Werror` / `git diff --check` | 新headerを含む整形・差分の空白が成功。`out/issue76-unit/clang-format.log` |
| `python -X utf8 eng/verify-vim-line-jumps.py` | 隔離profile・120 DPI・表示9行に対する80行文書で6シナリオ成功。g待ち/Esc、G→80行、Ggg→1行、G12G→12行の表示と保存後の編集位置、CRLFの12GdGとundoのbytes、3Gyggpとundo。`out/issue76-native/vim-line-jumps-all-results.json` / `native.log` |

nativeの3枚のcaptureでも本文のblock caretとstatusの行番号80/1/12を確認した。入力・save・画面captureは既存driverを再利用し、製品の入力/UIは変更していない。実行exeのSHA-256は結果JSONに保存。初回6件すべて成功し、nativeの再実行はない。

unit初回の2件は次のように切り分けた。undoは可視1行の `vim_body(frame)` を全文と比較していたテスト側の誤りで、既存 `whole_vim_body` に修正。VISUALは `$v` の時点で行末希望列が落ちる既存の `entered_visual` が原因で、baseと関数が同一であることを確認し [Issue #77](https://github.com/hideyukiMORI/nene-nib/issues/77)へ分離した。実測したappの `$vg<Esc>jj` は3行目列4。gなしのapp `$vjj` は未実行で、同じ結果になることは不変のコードからの推論。今回のg取消のテストは `v$g<Esc>jj` の列7へ修正し、状態保持と次入力を確認した。

追加の `python -X utf8 out/line-jump-oracle/continuation_probe.py` は3回起動した。初回はEsc記法の誤りでcase間にVISUAL状態が漏れ、2回目は修正用の文字列置換が適用されず同じ無効なprobeを実行した。3回目だけ各caseを `enew!` で分離し、`$v` / `$vg<Esc>` / `v$g<Esc>` の後に別のnormalでjjを送り、いずれもVISUAL・3行目列7・MAXCOLと確認。無効2回の記録を `continuation-probe-invalid*.json`、有効結果を `continuation-probe-result.json`、経緯を `continuation-probe-notes.md` に保持した。成功したprobeの再実行はしていない。今回のVim起動はfixture96、初期分割probe6、追加probe3の計105回。

独立レビューは型/所有・count・範囲・取消・dispatcher分割を読み取り専用で確認し、未解決指摘なし。rootレビューで検出した既存operatorへの余分な移動走査は、新しいdocument yankの場合だけに限定した。全oracle再測定、無関係な設定/テーマ/GUI/性能、全件unitは実行していない。上記の成功結果は実装・対象テスト・依存・必要な環境が不変のpush/review/mergeでも再利用する。保存schema・runtime依存は不変、Waivers: none。


### 5-r. Vimの開行と回数付きINSERT（Issue #79・ADR 0028・2026-09-20）

hideが#76の実機確認後にo/Oを挙げて続行を指示したため、NORMALの開行、INSERTの反復と履歴を今回の焦点にした。coreの既存行索引とINSERT、applicationの改行変換/replace/undoが正典。VimPutStringはVimInsertAtへ同じ変更で移行し、履歴境界を明示する。UI・codec・保存schema・依存・設定は不変。

固定Vim 9.1の既存measureで40入力を各1回測定（通常/+Escで計80起動）。元の36件は `out/issue79-oracle/measure_open.py`、追加4件は `measure_extra.py`。`do`はdiff操作となり未設定環境でexit 1のため除外（追加1起動）、後続dOは未実行。Vimソースは読んでいない。実測記録は `fixture-results.json` / `extra-results.json`、採用入力は `adopted-inputs.json`。

`python out/git/assemble-open-line-fixtures.py` は既存parser/formatterを使い、base `8f48f4950385051c988f197b3fd5889e6efdaf61` の431件を逐語維持して40件を追加した。測定ソースAST/import、固定Vimの場所/版/既定設定、入力全体と測定結果、旧行とSHAを照合。再測定0、計471件、入力SHA-256 `07d4aa391caa2bb45b0b4c2d235a1e04596eedb48061a4788e8eeca8f8925646`。`assembly-result.json`に証拠がある。oracle生成器は不変で、その既存ツール検証は再利用する。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `. ./eng/toolchain.ps1` → `cmake --build build --target nib_tests NeNeNib --parallel 4` | 新しい型・engine・履歴・直接呼出元をDebug + ASan/UBSan + tidyで確認。最終成功は `out/issue79-build-external.log`。初回 `out/issue79-build.log` も成功 |
| `build/nib_tests.exe --vim-open-lines` | 初回551中549成功、40fixtureに失敗なし。o/O/count/Esc/BS/Enter/移動/UTF-8/CRLF/途中INSERT/IME確定/undo/redo、直接影響するp/Pと既存INSERT、EditHistoryを確認。`out/issue79-unit.log` |
| `build/nib_tests.exe --vim-open-line-recovery` | 430 checks成功。空ファイルの保存期待値修正、共通反復サイズguard、空入力、反復helperが変わる40fixtureとp/Pを限定して再生。`out/issue79-recovery.log` |
| `build/nib_tests.exe --vim-open-line-external` | 46 checks成功。マウス同位置移動・外部選択・Ctrl+Z・未記録編集のrepeat解除、移動のundo境界、p/Pを確認。`out/issue79-external.log` |
| `python -X utf8 eng/symbols.py --build-dir build --require core application` | 2 libraries / 0 violations。反復と編集合成にOS/時刻などの参照が増えていない。`out/issue79-symbols.log` |
| `python -X utf8 eng/conformance.py --build-dir build` | 新しい型/依存/生成物の整合、0 violations。`out/issue79-conformance.log` |
| 変更C++の `clang-format --dry-run --Werror` / `git diff --check` | 成功。新headerを含む。`out/issue79-format.log`。native scriptのPython compileも成功 |
| `python -X utf8 eng/verify-vim-open-lines.py --only belowEof` / `--only aboveEmpty` / `--only joinBefore` | 隔離profile・120 DPIで3件成功。40行CRLF末尾のG3oと日本語/絵文字/Enter、空ファイルの3Oと無効/有効BS、途中Oの前行結合。開行直後/入力後/Esc後の保存bytes、保存境界ごとのundo、表示9行での追従を確認。結果は `out/issue79-native/vim-open-lines-*-results.json`、ログは `out/issue79-native-*.log` |

初回unitの2失敗は、空ファイルの保存改行をテスト側がLFと期待したもの。既存LineEndingの正典は改行を含まない文書をCRLFとするため、製品を変えず期待値をCRLFへ訂正した。その他の成功を再利用した。回数guardと追加selectorのbuildではテストmainが行数/複雑度に抵触したため、名前と関数のconstexpr表へ整理した（`out/issue79-build-recovery.log`）。既存selectorの対象は維持し、閾値や抑制を変えていない。

native初回はbelowEofの開行・入力・反復・保存を通過し、途中で3回保存したのにundo1回で初期本文へ戻ると誤って期待して停止した（`out/issue79-native.log`）。保存ごとに履歴を閉じる既存ADR 0010の契約に合わせ、typed→opened→initialの3段階へ訂正。失敗したbelowEofを限定再実行し、未実行のaboveEmpty/joinBeforeを各1回実行した。保存を挟まない1回undoは単体のround-tripで確認している。

captureではbelowEofのINSERT・行41列1のbar、Esc後のNORMAL・行46列1のblockと日本語/絵文字3回分、aboveEmptyの行3列2、joinBeforeの行1列5を目視確認。native exe SHA-256は `75e89f89f9a4f92534979ecf45f5017a37938149b0dbe62b1d21172bca0e2e12`。入力刺激は既存WM_CHARのUTF-16配送とWM_KEYDOWN、保存だけSendInput Ctrl+S。IME確定経路は単体で確認し、OSの日本語変換そのものは今回再検証していない。

設計と差分の独立レビューは読み取り専用。実装/調査/検証はroot単体で実施した。回数の積のoverflowは共通helperで型付きfailureにし、p/Pは拒否、o/Oは初回入力を保持してEsc完了とした。実メモリ予算は設けておらず、大きな有効サイズでのメモリ不足は残る制限。

検証範囲の異なる3回のchecksを合算して全件成功とは主張しない。overflow前の成功のうち反復helper関連は430で更新し、controller中断の直接影響は46で更新した。他の成功は関連実装不変のため再利用する。push/review/merge・文書追記・SHAだけでは再実行しない。全oracle再測定、全件unit、設定/テーマ/性能の無関係な測定は未実行。autoindent、一般i/a回数、dot、矩形、既存Issue #77は範囲外。Waivers: none。


### 5-s. VISUAL移行時の希望列（Issue #77・2026-09-20）

統合単位は [PR #82](https://github.com/hideyukiMORI/nene-nib/pull/82)。9月21日未明の文書追記・レビュー・統合でも、関連入力不変の成功結果を再利用する。

NORMAL→v/V、v↔V、同じキーでの終了、回数付き選択を、既存VimState::wanted_columnで表す。変更したproductionはVimStep.cppのwidened / entered_visual / visual_switchedだけ。横に実際に広がった場合は既存wanted_afterで到達列へ更新し、縦は元の希望列を使う。空行/EOFで移動不能なら位置も希望列も保つ。Escの終了経路は不変。ADR 0018へ実測に基づく補足を入れ、型・所有・効果・UI・保存schemaは変えていない。

固定Vimの新規fixture測定は19件＋空行2件（measureの通常/+Escで計42起動）。別の分割normal probeは開始/切替/終了12件、明示1と回数6件、空行2件、最終行2件を4起動で測定。合計46起動。Vimソースは読まず、#76のg取消に関する3ケースのprobeは結果を再利用した。証拠は `out/issue77-oracle/{fixture-results,empty-count,transitions,count-one,empty-transitions,last-line-transitions}.json`。

最初のmeasure.pyは日本語/絵文字ケースの結果をcheckpoint保存した後、端末のcp932出力で例外になった。`python -X utf8 out/issue77-oracle/measure.py`で未測定の末尾2件だけ続行し、成功済み17件は再測定していない。空行の `$2vj<Esc>` は同じnormal内の後続キーが打ち切られるためfixtureへ採用せず、別normalへjを送るprobeの結果を手書きテストへ採用。空行2件はfixture数に含めない。

`python -X utf8 out/git/assemble-visual-wanted-fixtures.py`でbase `69012f9eafa5d67dbd0bb7ba89829f7bab16649c` の471行の逐語一致と入力/測定ソース/固定Vim/metadataを照合し、一旦19件を追加した。そのうち `$2Vy` は修正前後で同じcolumn比較に失敗する既存の行単位VISUAL yank問題で、[Issue #81](https://github.com/hideyukiMORI/nene-nib/issues/81)へ分離した。yanked_caretはbaseから不変。実appの列番号をログには出しておらず、不一致以上の値は主張しない。

`python -X utf8 out/git/exclude-visual-yank-fixture.py`はその1件だけを除き、他の全行が元のcanonical formatter出力と逐語一致することを検証した。最終採用18件、計489件、SHA-256 `7182d15458c4439674b35c5fc8b7c086ac4202d35878dd24654f13f64051fd2f`。記録は `assembly-result.json` / `exclusion-result.json`。回数付きVの到達列は分割probeとpure coreの選択位置テストで直接確認している。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `. ./eng/toolchain.ps1` → `cmake -S . -B build -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=<repo>/build/issue77` | 実行中のhideの旧版を保持し、同じCMake target/compile flagsで出力先だけを分離。`out/issue77-configure.log` |
| `cmake --build build --target nib_tests --parallel 4` → `build/issue77/nib_tests.exe --vim-visual-wanted`（production修正前） | 212 checks中37失敗。gなしの$vjj、開始/切替/希望列、count、選択の範囲ずれを再現。`out/issue77-build-before.log` / `out/issue77-before.log` |
| `cmake --build build --target nib_tests NeNeNib --parallel 4` → `build/issue77/nib_tests.exe --vim-visual-wanted`（修正後） | 空行/EOFの4確認を追加し216 checks中215成功。残る1失敗は修正前から同じcolumn比較に失敗している#81。対象の希望列・18fixture・待ち取消・範囲/CRLF/undo・既存VISUAL境界は成功。`out/issue77-build.log` / `out/issue77-unit.log` |
| `cmake --build build --target nib_tests --parallel 4`（#81 fixture除外後） | 生成headerとselector件数だけを再コンパイル。成功したproduction/各テストは不変なので、実行を繰り返さず上記の成功を再利用。`out/issue77-build-fixture.log` |
| `python -X utf8 out/issue77-native/verify.py` | 120 DPI、CRLF文書の$vjjでVISUALの行3列7・選択表示をcapture目視、dでabcTAIL、uで元bytesを復元。`out/issue77-native/result.json` / `visual-wanted.bmp` / `out/issue77-native-visible.log` |
| `python -X utf8 eng/symbols.py --build-dir build --require core application` / `python -X utf8 eng/conformance.py --build-dir build` | 2libs/0 violations、conformance0。変更coreの外部参照と489件の生成整合。`out/issue77-symbols.log` / `out/issue77-conformance.log` |
| `clang-format --dry-run --Werror src/core/VimStep.cpp tests/unit/NibTests.cpp` / `git diff --check` | 成功。DebugのbuildにはASan/UBSanとtidyを含む。`out/issue77-format.log` |

native初回は窓を前面へ上げない手順でaccent検出0となり停止した（`out/issue77-native.log`）。既存window_driver.raise_windowを追加し、同じ表示/保存/undoの確認を実行して成功した。判定条件は緩めていない。実機exe SHA-256 `935b8362853cbd7889d86c6ea843375668b1c43f0ebbf9bc2858bedf3224ef93`、出力は `build/issue77/NeNeNib.exe`。起動中の旧版PID41000には入力も終了も送っていない。確認後はCMakeの出力先設定だけを既定へ戻し、旧版への再リンクは行わない。

変更は3つの局所関数なのでrootの自己レビューで網羅性・optional・状態所有・回数境界を確認し、追加の独立レビューは使わなかった。一般のvisual_restingを変えず、未対応キーやo/Oなど別の寿命へ変更を広げていない。#81を直して全件を再実行することはせず、成功した関連範囲をpush/review/mergeでも再利用する。保存形式/依存/waiver変更なし、Waivers: none。

### 5-t. 行単位VISUAL yankの戻り位置（Issue #81・2026-09-21）

統合単位は [PR #83](https://github.com/hideyukiMORI/nene-nib/pull/83)。以下の成功結果は文書追記・レビュー・統合でも再利用する。

baseは `3b02f136c41905bb2667ddcf97f04087fcfa5fae`。変更productionは `VimStep.cpp::yanked_caret` の1関数で、行単位VISUALに限り、現在行が範囲の最終行なら範囲先頭、そうでなければ現在位置を返す。単一行/下向きでは列1、上向きの複数行では現在列となる。行末の寄せは既存controllerを使い、NORMALのyankと文字単位、レジスタ本文/種類は不変。ADR 0018の狭い実測からの一般化を訂正した。

`python -X utf8 out/issue81-oracle/measure.py` はcanonical `eng/vim-oracle.py::measure` で新規22件（通常/+Escで44起動）を測定し、成功結果を逐次保存した。単一行・方向・左右移動・字下げ/タブ・空行・$・回数・o・v/V切替・Unicode/CRLF・後続縦移動が対象。#77で分離した `$2Vy` は本文/キー/期待値/測定実装/環境が不変なので再測定せず、名前だけvisual-yank-count-long-lineに変えて再利用した。`fixture-inputs.json` / `fixture-results.json` が正。

`python -X utf8 out/issue81-oracle/split.py` は1起動で12件を測った。startoflineの有無を比較する単一行/下向き/上向き10件では戻り位置に差が無い。末尾の回数付きVの2件は、ケース間のVISUAL記憶を隔離できず独立起動より広い範囲になったため採用しない。回数/oはcanonical fixtureの独立起動を採用した。新規Vim起動は計45回。Vimソースは読んでいない。

`python -X utf8 out/issue81-oracle/assemble.py` はcanonical parser/formatterを使い、既存489行の逐語一致とmetadata、測定ソースの一致、#77再利用行の名前以外の逐語的な値の一致を確認して追加23件を組み立てた。最終512件、入力SHA-256 `da104be451921a9d231baf27401d97651f9d341f49b7252bca4a656536b6ff51`。記録は `out/issue81-oracle/assembly-result.json`。生成ツール・ゲート・除外設定は変更していない。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `. ./eng/toolchain.ps1` → `cmake -S . -B build -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=C:/Users/info/WORKS/NeNeNib/build/issue81` | 起動中の旧版を保持し、既存target/flags/objectを使って出力先だけ分離。`out/issue81-configure.log` |
| `cmake --build build --target nib_tests --parallel 4` → `build/issue81/nib_tests.exe --vim-visual-yank`（修正前） | 新規23fixtureと共有経路の既存10fixture。265 checks中13失敗、すべてcolumn。`out/issue81-build-before.log` / `out/issue81-before.log` |
| `cmake --build build --target nib_tests NeNeNib --parallel 4` → `build/issue81/nib_tests.exe --vim-visual-yank`（修正後） | Debug/tidy/ASan/UBSanでbuild成功、265 checksすべて成功。位置・本文・レジスタ本文/種類・後続移動、NORMAL/文字単位の直接境界を確認。`out/issue81-build.log` / `out/issue81-unit.log` |
| `python -X utf8 eng/symbols.py --build-dir build --require core application` | core修正の外部参照を確認。2libs、0 violations。`out/issue81-symbols.log` |
| `python -X utf8 eng/conformance.py --build-dir build` | 修正実装の規約と512fixtureの生成整合。0 violations。`out/issue81-conformance.log` |
| `clang-format --dry-run --Werror src/core/VimStep.cpp tests/unit/NibTests.cpp` / `git diff --check` | 変更C++の整形と差分の空白、成功。`out/issue81-format.log` |
| `cmake -S . -B build -U CMAKE_RUNTIME_OUTPUT_DIRECTORY` | 一時出力先を既定へ復元、成功。再ビルドはしない。`out/issue81-restore-output.log` |

実機操作はcomputer-useスキルの `@oai/sky` をnode_replでimportしたが、`sky.list_apps()` がnative pipeへの接続エラー（os error 2）で失敗した。画面確認は未実施。新規アプリの起動・入力も行っていない。記録は `out/issue81-native-unavailable.json`。ビルド成果物 `build/issue81/NeNeNib.exe` のSHA-256は `d02d7d3af67b0a89c3821c61a857c121d57815d48aeedf78b72203d26d741c82`。旧版 `build/NeNeNib.exe` のPID41000は維持した。

規則FR-003 / ARC-001/004/007 / CPP-002/004 / QLT-001/008/012/013 / CNF-010を自己レビュー。閉じたswitch/既存optionalの扱い、状態所有、方向を既存入力から導けることを確認した。局所1関数の修正なので追加の独立レビューは無し。成功後は文書だけの変更で、push/review/mergeでも結果を再利用する。全件unit・全oracle・#77の開始/切替テスト・無関係なGUI/設定/性能は再実行していない。保存schema・依存変更なし。Waivers: none。

### 5-u. rの次文字待ちと範囲置換（Issue #84・2026-09-21）

統合単位は [PR #86](https://github.com/hideyukiMORI/nene-nib/pull/86)。以下の成功結果を文書追記・レビュー・統合でも再利用する。

base `8d7b3307b748b8341c1e6751631a365e0725f596`。ADR 0029を先に受理。VimAction/Prefixのrを既存VimInputWaitへ足し、VimStepの純粋な範囲/文字変換からVimReplaceRangeを返す。EditorControllerは既存with_document_newlines/replaceへ1回渡し、EditBoundary::separateで履歴を区切る。VimInsertAtのcaret_after_insertは同じ処理を引数だけ汎用化して共有した。新しい本文/選択/履歴所有、UI/IME、schema、依存、ゲートの変更はない。

`python -X utf8 out/issue84-oracle/measure.py` は固定Vim9.1とcanonical measure()で41ケースを通常/+Escの82起動で測定。`python -X utf8 out/issue84-oracle/input-probe.py` は11起動で途中状態/取消後の操作とliteral CRを確認。合計93起動で、Vimソースは読んでいない。`fixture-inputs.json` / `fixture-results.json` / `input-probe.json`が証拠。

VISUAL r<Enter>の2ケースはliteral CRを既存oracleのread_textがLFに変えていた。JSON probeのnormal!/feedkeys両方でCRを確認。TextBufferが内容末尾CRを区別しない問題とともにIssue #85へ分離した。今回は該当キーを取消扱いにし、generator/TextBufferを変えず2fixtureを採用しない。制御文字引用/置換とCtrl-e/yも本件の対象外。

`python -X utf8 out/issue84-oracle/assemble.py` はcanonical parser/formatterを使い、旧512行の逐語一致、入力/固定Vim/metadata/測定ソースを照合して39件だけ追加した。551件の入力SHA-256は `4a783276e3e41033ba914b6142fcadc79cbc45bba028be3929868a221d044483`。`assembly-result.json`に記録。組立時の再測定は0。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `. ./eng/toolchain.ps1` → `cmake -S . -B build -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=C:/Users/info/WORKS/NeNeNib/build/issue84` | 起動中の旧版を保持し、同じtarget/flagsで出力先だけ分離。成功、`out/issue84-configure.log` |
| `cmake --build build --target nib_tests NeNeNib --parallel 4`（初回） | visual_actedの61行がtidyの60行上限で失敗。`out/issue84-build.log`。NORMAL/VISUALの検索/g/r開始を既存input_actionにまとめて修正し、上限や除外を変更していない |
| 同build（修正後） | 新しいenum/variantの写し先、共有controller、対象テストをDebug/tidy/ASan/UBSanでbuild成功。`out/issue84-build-fixed.log` |
| `build/issue84/nib_tests.exe --vim-replace` | 460 checksすべて成功。新規39fixture＋共有挿入/caret/検索/gの既存12fixture、回数不足直後の独立キー、取消後VISUALの後続行、CRLF保存bytes、別々のundo/redo、モード切替を確認。`out/issue84-unit.log` |
| `python -X utf8 eng/symbols.py --build-dir build --require core application` | 変更した純粋経路にOS依存が混ざらないこと。2libs/0 violations、`out/issue84-symbols.log` |
| `python -X utf8 eng/conformance.py --build-dir build` | 新しい型/効果の規約と551fixtureの生成整合。0 violations、`out/issue84-conformance.log` |
| `clang-format --dry-run --Werror src/core/VimReplaceRange.hpp src/core/VimEffect.hpp src/core/VimAction.hpp src/core/VimPrefix.hpp src/core/VimStep.cpp src/application/EditorController.hpp src/application/EditorController.cpp tests/unit/NibTests.cpp` | 変更C++の整形、成功。`out/issue84-format.log` |
| `cmake -S . -B build -U CMAKE_RUNTIME_OUTPUT_DIRECTORY` | 一時出力先だけ既定へ復元。成功、`out/issue84-configure-restore.log`。旧版の再リンクはしない |

computer-useの `sky.list_apps()` はnative pipe接続エラー（os error 2）。`out/issue84-native-unavailable.json`。今回の実機画面確認・新規起動・入力は未実施で、旧版PID41000を維持した。成果物は `build/issue84/NeNeNib.exe`、SHA-256 `985ee9f578f84613a24b1a21d121721724a95b75865fb10072035ac20134fe07`。

FR-003 / ARC-001/004/007/009 / CPP-002/004/006/011/012 / QLT-001/008/012/013 / CNF-010を自己レビュー。optionalはhas_value/value、閉じたenumは網羅、count不足は待ち/対象文字列の反復確保前に拒否する。新効果と共有caret変換についてreadonly独立レビューを行い、回数不足後のキーとCRLF/既存挿入境界を対象テストに反映。整理後の再レビューに未解決指摘なし。実行検証はrootが担当した。

文書更新後の `git diff --check` も成功。成功後の変更は文書だけで、production/テスト/測定ソース/関連依存は不変。push/review/mergeでも上記を再利用し、全件unit・全oracle・無関係な設定/テーマ/性能は実行しない。Waivers: none。

### 5-w. Vimのテキストオブジェクト（Issue #93・ADR 0031・2026-09-22）

統合単位は [PR #96](https://github.com/hideyukiMORI/nene-nib/pull/96)。以下の成功結果を文書追記・レビュー・統合でも再利用する。

base `e2518cd`（ADR 0031 の 1 commit を含む `feat/93-vim-text-objects`。production の base は `d26c232`）。ADR 0031 を実測のあとで受理し、食い違った 4 点は決定の側を直した。`VimInputWait` に `VimTextObjectScope` を足し、新しい純関数 `vim_text_object_range`（`src/core/VimTextObjectRange.cpp`）1 本がオペレータの後ろでも VISUAL でも同じ範囲を決める。語の種類の表は `vim_character_class` として `VimWordMotion` から公開し、`iW` は同じ表の畳み方の違いだけにした（表は 1 つ・ARC-001）。UI・IME・描画・保存形式・schema・依存・ゲートは変更していない。

`python out/issue93-oracle/probe.py`（202）/ `probe2.py`（63）/ `probe3.py`（71）/ `probe4.py`（28）を実装の前に実行し、固定 Vim 9.1 を 364 ケース起動して決定を確かめた。証拠は `out/issue93-oracle/probe*.json` / `probe*.txt`。Vim ソースは読んでいない。すべてのケースを命令の切れ目で区切って測っている（5-v の教訓）。決定 1・2・5・6・8 は提案のまま一致。食い違った 4 点（VISUAL は行単位にならない / 括弧は中に居なくても前の塊を使う / `i(` の 2 つの寄せは独立 / `y` のキャレットは範囲の先頭の桁）は ADR 0031 の「文脈」に書き、**期待値ではなく決定と実装を直した**。合わせていない 2 点（回数が尽きた `d9iw` の Vim のキャレット移動、塊の外からの `2i(` が内側へ入ること）は fixture を採っていない。

`python out/issue93-oracle/add-fixtures.py --write` は候補 192 件を「区切って測った答え」と「1 回の `:normal!` で測った答え」の両方で測り、食い違う候補を拒否する（0 件拒否）。`python -X utf8 eng/vim-oracle.py --regenerate --only text-object-` は 192 件だけを測り、既存 650 行を `e2518cd` から逐語再利用した（`out/issue93-oracle/regenerate.log`）。`git diff` の削除行は metadata 2 行だけ。842 件の入力 SHA-256 は `58ba91c91e65162e9c651acb53711f9b9c5d68e96bc70c1231874c9d1c0ee94a`。初回の再生 192 件のうち 191 件が実装と一致し、`text-object-block-nested-too-many` だけが落ちた。これは「外向きに始めた括弧の走査は前へ折り返さない」を実測から読み直して実装を直したもので、fixture の期待値は測った値のまま。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `. ./eng/toolchain.ps1` → `cmake -S . -B build -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=C:/Users/info/WORKS/NeNeNib/build/issue93` | 起動中の旧版を保持し、同じ target / flags で出力先だけ分離。成功、`out/issue93-configure.log` |
| `cmake --build build --target nib_tests --parallel 4`（初回） | 新しい範囲関数・待ち・表を Debug / tidy / ASan / UBSan で。`vim_word_object_end` の認知的複雑度、`scanned_back` / `scanned_forward` のネスト 4、`visual_acted` の 62 行で拒否。`out/issue93-build.log` / `build2.log`。空白の枝を `blank_object_end` へ、括弧 1 文字の判定を `opens_the_block` / `closes_the_block` へ、VISUAL の `i` / `a` を既存の `input_action`（次キー待ちの 1 か所）へ寄せて修正した。閾値・除外・重大度は変えていない |
| 同 build（修正後・最終） | 整形後の最終形も build 成功。`out/issue93-build-final.log` |
| `build/issue93/nib_tests.exe --vim-text-objects` | 1623 checks すべて成功。新規 192 fixture ＋ 共有境界 6 件（r・f/t・g 待ちと `.` の再生代表）と、待ちの排他・取消がモード/選択/希望列/検索記憶を保つこと・VISUAL の置換と種類切替・伸ばし（`viwiw`）・`.` の記録と再生・undo 1 単位と redo・CRLF の保存 bytes を確認。`out/issue93-unit-text-objects.log` |
| `build/issue93/nib_tests.exe --vim-dot` / `--vim-replace` / `--vim-character-search` / `--vim-line-jumps` | `VimInputWait` に値を足したので、待ちを共有する `.`・r・f/t/;・g の退行を確認。909 / 460 / 621 / 989 checks すべて成功。`out/issue93-unit-waits.log` |
| `build/issue93/nib_tests.exe --vim-visual-yank` | VISUAL の選択と yank の経路を共有するため。265 checks 成功。`out/issue93-unit-waits.log` |
| `build/issue93/nib_tests.exe` | `VimWordMotion` の内部（`VimScanPoint` に語の切れ方を持たせた）に触れたので、w / b / e を含む 842 fixture 全部を再生する unit 全体も 1 回実行した。8733 checks すべて成功。`out/issue93-unit-all.log` |
| `python -X utf8 eng/symbols.py --build-dir build --require core application` | 新しい core の 1 本（`std::vector<Offset>` を使う）に OS 依存が混ざらないこと。2 libs / 0 violations、`out/issue93-symbols.log` |
| `python -X utf8 eng/conformance.py --build-dir build` | 新しい 6 型の 1 ファイル 1 型と、842 fixture の生成整合（CNF-010）。0 violations、`out/issue93-conformance.log` |
| `clang-format --dry-run --Werror`（変更・追加した C++ 14 ファイル ＋ 生成 header） | 変更 C++ の整形。初回は `VimTextObjectRange.cpp` と `NibTests.cpp` が拒否され、`clang-format -i` のあと build と対象テストを再実行して成功 |
| `cmake -S . -B build -U CMAKE_RUNTIME_OUTPUT_DIRECTORY` | 一時出力先だけ既定へ復元。成功、`out/issue93-configure-restore.log` |

computer-use による実機の画面確認は未実施（native pipe が繋がらないため試みていない）。起動中の旧版 PID には触れていない。`NeNeNib.exe` はこの Issue では作っていない（engine と対象テストだけの変更で、窓・描画・保存に触れていないため）。

FR-003 / ARC-001/004/007/009 / CPP-002/004/006/011/012 / QLT-001/008/012/013 / CNF-010 を自己レビュー。`optional` は `has_value` / `value` / `value_or` だけで読み、閉じた分岐（`VimTextObject` / `VimTextObjectScope` / `VimWordClass` / `VimInputWait` の visit）に `default` は無い。範囲関数は本文・履歴・レジスタを持たず、UI 側にテキストオブジェクトの知識を置いていない。`reinterpret_cast`・時刻・OS・スレッドは増やしていない（新しい `__std_*` も出ていない）。

性能は測っていない。1 打鍵あたりの走査は行単位に本文を引くので、括弧の探索だけが本文の長さに比例し得る。速さの予算への影響は次に速さを測る機会に ADR 0016 の基準値と突き合わせる（ADR 0021）。関連しない設定・テーマ・性能・全 oracle は実行しない。push / レビューでも上記の成功結果を再利用する。Waivers: none。

### 5-v. Vimの`.`（直前の変更の再生）（Issue #87・ADR 0030・2026-09-22）

統合単位は [PR #90](https://github.com/hideyukiMORI/nene-nib/pull/90)。以下の成功結果を文書追記・レビュー・統合でも再利用する。

base `2c00655ca7e50c7275c68d7f4aef2425ecf0ad08`（ADR 0030 の 2 commit を含む `feat/87-vim-dot-repeat`。production の base は `82f260d`）。ADR 0030 を実測のあとで受理。`VimState` に `recording` / `last_change`（どちらも `optional<VimRepeatRecord>`）を足し、`VimEffect` に `VimReplay`、`VimAction` に `repeat_change` を足した。記録の確定・破棄は `vim_step` の後段にある純関数 1 か所（`vim_recorded` と 4 つの小さな判定）だけが書き、鍵の意味ではなく前のモードと効果で分ける。controller は `VimReplay` を受けて前後で履歴を閉じ、同じ `accept(VimKeyPress)` へ鍵を 1 つずつ流す。`.` は記録されないので再帰は深さ 1。UI・IME・描画・保存形式・schema・依存・ゲートは変更していない。

実装の途中で ADR 0028 の穴が出た。`N.` が挿入命令を繰り返すには `3ifoo<Esc>` が `foofoofoo` でなければならないが、回数付きの `i a I A` は回数を捨てていた。既存の `VimInsertRepeat` を `entered_insert` でも立てて塞ぎ、`counted-insert-*` 5 件で固定 Vim と一致を確認した。これに伴い controller の `interrupt_vim_insert`（外からの割り込みで入力記録を捨てる）と `perform(VimMoveTo)`（Vim の鍵による移動で履歴だけ閉じる）を分けた。入力記録を捨てる判断は engine の 1 か所に残る。

`python out/issue87-oracle/probe.py`（172 ケース）と `python out/issue87-oracle/probe2.py`（29 ケース）を実装の前に実行し、固定 Vim 9.1 を 201 ケース起動して ADR の決定を確かめた。証拠は `out/issue87-oracle/probe.json` / `probe.txt` / `probe2.json` / `probe2.txt`。Vim ソースは読んでいない。決定 2（回数の積を 1 つに畳む。`2d3w5.` と `6dw5.` が同じ 5 語）、決定 4（`ifoo<Home>bar<Esc>.` は `bar` だけ。`<End>` / `<Left>` / `<Down>` / `o` からの挿入も同じ）、決定 7（`x3..` / `3x3..` / `cwfoo<Esc>3..` が回数 3 を引き継ぐ）は一致した。

食い違いは 2 つで、どちらも期待値を合わせず ADR のまま実装し、該当 fixture を採っていない。(1) 決定 5 は Vim と違う。Vim は `vlld.` を同じ大きさの範囲で再生し、`xjvlld.` でも VISUAL の削除を繰り返す。本実装は直前の変更を消して何もしない（Issue #91）。(2) 決定 3 に残る穴と書いた点は、**下の追記で撤回した**（測り方の誤りで、固定 Vim も決定 3 と一致する）。

`python -X utf8 eng/vim-oracle.py --regenerate --only dot- --only counted-insert-` は 97 件だけを測り、既存 551 行を `2c00655` から逐語再利用した（`out/issue87-oracle/regenerate.log`）。`git diff` の削除行は metadata 2 行だけで、既存 fixture の期待値は測り直していない。648 件の入力 SHA-256 は `18c89df3a95008f534956eda918869fd4dcf5ff1df24fb6e71238e8dd4fda55d`。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `. ./eng/toolchain.ps1` → `cmake -S . -B build -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=C:/Users/info/WORKS/NeNeNib/build/issue87` | 起動中の旧版を保持し、同じ target / flags で出力先だけ分離。成功、`out/issue87-configure.log` |
| `cmake --build build --target nib_tests NeNeNib --parallel 4`（初回） | `commanded` と `visual_acted` が新しい action 1 つで tidy の 60 行上限を超えて失敗。`out/issue87-build.log`。半画面と 1 画面の巻きを NORMAL / VISUAL 共通の `scroll_action` にまとめ、`.` を `history_action` に寄せて修正した。閾値・除外・重大度は変えていない |
| 同 build（修正後・最終） | 新しい型/効果/action の写し先、記録の純関数、controller の再生、対象テストを Debug / tidy / ASan / UBSan で build 成功。`out/issue87-build-final.log` |
| `build/issue87/nib_tests.exe --vim-dot` | 869 checks すべて成功。新規 97 fixture ＋ 共有境界 8 件（o/O の回数反復・r・f/t・g 待ち）と、記録の確定/破棄・回数の折り畳みと置換・`.` が `.` を記録しないこと・undo 1 単位と redo・CRLF の保存 bytes・1 行の表示領域での再生・VISUAL の変更が記録を消すこと・INSERT の `.` が文字であることを確認。`out/issue87-unit-dot.log` |
| `build/issue87/nib_tests.exe --vim-open-lines` / `--vim-open-line-external` / `--vim-open-line-recovery` | ADR 0028 の入力記録に手を入れたので、o/O の回数反復と外からの割り込み（クリック・Ctrl+Z・全選択・外部編集）の退行を確認。565 / 46 / 430 checks すべて成功。`out/issue87-unit-open-lines.log` |
| `build/issue87/nib_tests.exe --vim-replace` / `--vim-character-search` / `--vim-line-jumps` | 記録が次キー待ち（r・f/t/;・g）をまたぐので、待ちと取消の退行を確認。460 / 621 / 989 checks すべて成功。`out/issue87-unit-waits.log` |
| `build/issue87/nib_tests.exe` | `vim_step` の後段を全 Vim 経路が通るため、unit 全体も 1 回だけ実行した。7181 checks すべて成功、648 fixture を再生。`out/issue87-unit-all.log` |
| `python -X utf8 eng/symbols.py --build-dir build --require core application` | `std::vector<VimKey>` を core に足したので、純粋経路に OS 依存が混ざらないこと。2 libs / 0 violations、`out/issue87-symbols.log` |
| `python -X utf8 eng/conformance.py --build-dir build` | 新しい 2 型の 1 ファイル 1 型と、648 fixture の生成整合（CNF-010）。0 violations、`out/issue87-conformance.log` |
| `clang-format --dry-run --Werror`（変更した C++ 9 ファイル） | 変更 C++ の整形。初回は `VimEffect.hpp` ほかが拒否され、`clang-format -i` で整えてから再実行し成功。`out/issue87-format.log` |
| `cmake -S . -B build -U CMAKE_RUNTIME_OUTPUT_DIRECTORY` | 一時出力先だけ既定へ復元。成功、`out/issue87-configure-restore.log`。旧版の再リンクはしない |

computer-use による実機の画面確認は未実施（前回と同じく native pipe が繋がらないため試みていない）。起動中の旧版 PID には触れていない。成果物は `build/issue87/NeNeNib.exe`、SHA-256 `19952fcabafd0f1885c2a732802ec89b1ea238f651722e401e0e8371769da068`。

FR-003 / ARC-001/004/007/009 / CPP-002/004/006/011/012 / QLT-001/008/012/013 / CNF-010 を自己レビュー。`optional` は `has_value` / `value` / `value_or`、効果の分類は 14 個の overload を `std::visit` で網羅、INSERT の記録を取り直す鍵は `VimSpecialKey` の網羅 `switch`、閉じた分岐に `default` は無い。記録は本文・履歴・レジスタを持たず、UI 側に `.` の知識を置いていない。

性能は測っていない。1 打鍵あたり小さな `vector` の複製が増えるが、予算 0.9 ms に対する影響は速さのゲートが見張る（ADR 0016 / 0021）。関連しない設定・テーマ・性能・全 oracle は実行しない。push / レビュー / 統合でも上記の成功結果を再利用する。Waivers: none。

**追記（2026-09-22・取消の扱いを測り直した）。** 最初の probe は `xdk.` / `xGdj.` から「Vim は失敗したオペレータで記録を入れ替える」と読んだが、これは測り方の誤りだった。失敗してビープする命令のあと、`:normal!` に積んだ残りの鍵は捨てられるので、`.` がそもそも実行されていない（`xdkj` の最終カーソルが 1 行目のままなのが実測の証拠。Vim ソースは読んでいない）。`python out/issue87-oracle/probe3.py`（28 ケース）で取消の全経路を測り、`python out/issue87-oracle/probe4.py`（21 ケース × 区切った形と 1 回にまとめた形 = 42 起動）で鍵を命令の切れ目で区切って測り直した。結果は決定 3 と一致する。範囲の作れない `dk` / `dj` / `2dd` / `2D`、回数の入らない `3r`、外れた `f` / `;`、motion でない鍵 `dq`、`g` の続きが無い `dgz`、失敗した yank `yk`、Esc の取消 `d<Esc>` / `r<Esc>` のいずれも、取消になった命令は自分の鍵を捨てるだけで直前の変更を変えない。**engine は変更していない。**証拠は `probe3.json` / `probe3.txt` / `probe4.json` / `probe4.txt`。

ビープする命令の後ろに鍵が続く列は、1 回 `:normal!` の既存生成器では fixture にできない（生成器が残りの鍵を捨てた答えを記録してしまう）。生成器は変更せず、ビープしない Esc の取消 `xd<Esc>j.` と `xr<Esc>j.` の 2 件だけを fixture に採って 650 件とし（`--only dot- --only counted-insert-` の再生成で 99 measured / 551 reused、既存 97 行は同じ値で再現・`out/issue87-oracle/regenerate2.log`）、残りの 12 の取消経路は `--vim-dot` の対象 unit が直接確かめる。650 件の入力 SHA-256 は `e7d90990f8eed17ae5b3ce86b8da6823a074f492eb65b5753b00a10276a8b4fb`。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build --target nib_tests NeNeNib --parallel 4` | 取消の unit を足したテストの build。成功、`out/issue87-build-cancel.log` |
| `build/issue87/nib_tests.exe --vim-dot` | 909 checks すべて成功（取消 12 経路 × 記録と本文、追加 2 fixture を含む）。`out/issue87-unit-cancel.log` |
| `build/issue87/nib_tests.exe --vim-replace` / `--vim-character-search` / `--vim-line-jumps` | 取消経路を共有する r・f/t/;・g 待ちの退行。460 / 621 / 989 checks すべて成功。`out/issue87-unit-cancel.log` |
| `eng/symbols.py --build-dir build --require core application` | 2 libs / 0 violations、`out/issue87-symbols2.log` |
| `eng/conformance.py --build-dir build` | ADR の追記と 650 fixture の生成整合（CNF-010）。0 violations、`out/issue87-conformance2.log` |
| `clang-format --dry-run --Werror tests/unit/NibTests.cpp` | 追加した unit の整形。成功、`out/issue87-format2.log` |

engine と production の C++ は追記の前後で不変なので、unit 全体・o/O の scope・symbols 以外の既存成功結果は再利用し、測り直していない。

### 5-y. scope専用の契約を既定の単体実行へ（Issue #97・2026-09-22）

統合単位は [PR #102](https://github.com/hideyukiMORI/nene-nib/pull/102)。以下の成功結果を文書追記・レビュー・統合でも再利用する。

base `e6dfc14`（`test/97-default-run-contracts`）。変更は `tests/unit/NibTests.cpp` 1 本だけで、production の C++・fixture・閾値・期待文言は一切触れていない。`#87` / `#93` で足した `--vim-dot` / `--vim-text-objects` の契約（取消 12 経路・待ちの排他・記録の破棄・VISUAL の置換など）は selector を指定したときだけ走り、引数なしの既定実行＝ CTest の `nib_unit` では走っていなかった。各 scope の fixture 部分と契約部分を分け（`verify_vim_dot_contracts` / `verify_vim_text_object_contracts` / `verify_vim_visual_wanted_contracts`）、既定実行が回す契約を `verify_vim_scope_contracts` の 1 つの表にまとめて `main` から呼ぶ。selector の表と対になる位置に置いたので、新しい scope を足すときに既定から漏れたことが 2 つの表の差として見える。selector は「その scope だけを速く回す」絞り込みのまま変えていない。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build --target nib_tests`（Debug / tidy / ASan / UBSan） | 契約を束ね直した関数と表の build。成功、64.3 s（初回・base）と 43.8 s（変更後の再 build） |
| `build/nib_tests.exe`（引数なし） | 既定実行に 2 scope の契約が載ったこと。8733 → **8823 checks**（+90 = `--vim-dot` と `--vim-text-objects` の契約分）すべて成功。所要 1.68 → 1.73 s |
| `build/nib_tests.exe --vim-dot` / `--vim-text-objects` | selector の内容が変わっていないこと。909 / 1623 checks で変更前と同数・すべて成功 |
| `build/nib_tests.exe --coverage-negative` | 早期 return の経路が既定の追加に巻き込まれていないこと。22 checks 成功 |
| `ctest --test-dir build -R '^nib_unit$' --output-on-failure` | CTest から見た既定実行。成功、1.45 → **1.56 s**（+0.11 s） |
| `clang-format --dry-run --Werror tests/unit/NibTests.cpp` | 変更した C++ の整形。成功 |
| `python eng/conformance.py` | 1 ファイル 1 型と生成物の一致（CNF-010）。0 violations |

テストの内容・閾値・fixture・oracle は変更していないので、oracle の再生成・`eng/symbols.py`・性能（QLT-014）・Release build・全件（`check.ps1 -Full`）は実行していない。production の C++ とリンク境界が不変で、842 fixture の入力も不変だからである（QLT-001 / QLT-012・ADR 0021）。push / レビュー / 統合でも上記の成功結果を再利用する。Waivers: none。

QLT-001 / QLT-008 / QLT-012 / CNF-010 / CPP-011 を自己レビュー。新しい型は足していない（表は既存の selector 表と同じ `std::pair` の `constexpr` 配列）。閉じた分岐・`optional` の読み方・`reinterpret_cast` に関わる変更はない。

### 5-z. Vimの検索 `/ ? n N * #`（Issue #100・ADR 0032・2026-09-22）

統合単位は [PR #103](https://github.com/hideyukiMORI/nene-nib/pull/103)。以下の成功結果を文書追記・レビュー・統合でも再利用する。

base `b4f23e8`（ADR 0032 の 3 commit を含む `feat/100-vim-search`。production の base は origin/main `eb79415`）。ADR 0032 を実測のあとで受理し、食い違った 1 点（`\<あいう\>` はひらがな → 漢字の境界で `あいう漢字` に一致する）は**期待値ではなく ADR の補足を直した**。`CommandInput` に `SearchLine` を足し、入力行の見え方は `InputLineView`（プロンプトの閉じた enum ＋ 文字列 ＋ caret ＋ 候補）1 つに畳んで描画の経路を 1 本にした。照合器 `VimPattern`（`magic` の部分集合・未対応構文は `VimPatternFailure` で拒否）と走査 `vim_search` は core の純関数で、`std::regex` と `<locale>` は使っていない。engine は `VimKey` に `VimSearchPattern`、`VimEffect` に `VimOpenSearch`、`VimState` に `last_search`、`VimStep` に `notice` を足し、exclusive の規則は `exclusive_range` 1 か所を検索と移動が共用する。保存形式・schema・依存・ゲートの閾値は変更していない。

`out/issue100-oracle/probe.py`（120）/ `probe2.py`（41）/ `probe3.py`（27）/ `probe4.py`（30）を**実装の前**に実行し、固定 Vim 9.1 を 218 ケース起動して決定を確かめてある（前セッション。証拠は `out/issue100-oracle/probe*.json` / `probe*.txt`）。Vim ソースは読んでいない。実装は決定 1〜7 のまま通り、実測と食い違ったのは上の 1 点だけである。

`python -X utf8 out/issue100-oracle/add-fixtures.py --write` は候補 136 件を「命令の切れ目で区切った形」と「1 回の `:normal!` の形」の両方で測り、食い違う 1 件（空入力の Backspace の取消）を機械的に拒否した（`out/issue100-oracle/add-fixtures.txt`）。`python -X utf8 eng/vim-oracle.py --regenerate --only search-` は 135 件だけを測り、既存 842 行を `37816a1` から逐語再利用した（`out/issue100-oracle/regenerate.log`）。`git diff` の削除行は metadata 2 行だけ。842 件の入力 SHA-256 は来歴どおり `58ba91c91e65162e9c651acb53711f9b9c5d68e96bc70c1231874c9d1c0ee94a` で、977 件は `2e36c3b63bd65ea79ce3fed9af4c1423d70f8d6376005570bf3119c10b546ac1` になった。同じ選択で 2 回目を再生成して**バイト単位で同一**（`git status` に差分なし・`out/issue100-oracle/regenerate2.log`）。初回の再生で 135 件すべてが実装と一致した（期待値を直した fixture は無い）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `. ./eng/toolchain.ps1` → `cmake -S . -B build -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=C:/Users/info/WORKS/NeNeNib/build/issue100` | 起動中の旧版を保持し、同じ target / flags で出力先だけ分離。成功、`out/issue100-configure.log` |
| `cmake --build build --target nib_tests --parallel 4`（入力行を畳んだ 1 つめの commit） | `EditorFrame.command_line` の型と renderer のプロンプトを変えた整えの build。成功、`out/issue100-build1.log`。`NeNeNib` も成功（`out/issue100-build1-app.log`） |
| `build/issue100/nib_tests.exe --command-palette` / `--ex-settings` | 入力行の見え方を畳んだので Ex と設定一覧の退行を確認。130 / 153 checks すべて成功（変更前と同数） |
| `cmake --build build --target nenenib_core --parallel 4`（照合器と走査） | 新しい 10 型と 2 本の純関数を Debug / tidy / ASan / UBSan で。初回から成功、`out/issue100-build2.log` |
| `cmake --build build --target nenenib_core --parallel 4`（engine） | `VimKey` / `VimEffect` / `VimState` / `VimStep` の拡張と分岐の表を。初回から成功、`out/issue100-build4.log` |
| `cmake --build build --target nib_tests NeNeNib --parallel 4`（controller と窓） | 初回は clang-tidy が `state_.command_input().value()` の未検査アクセスと、`VimKey` が 3 択になったことで `for (const VimKey key : …)` の複製を拒否した（`out/issue100-build5.log`）。前者は `has_value` の分岐を挟み、後者は参照で受けて修正した。閾値・除外・重大度は変えていない。修正後の build は成功（`out/issue100-build6.log` / `build7.log`） |
| `build/issue100/nib_tests.exe --vim-search` | **1375 checks すべて成功**。新規 135 fixture ＋ 共有境界 6 件（r・f/t・gg・`.`）と、照合器の部分集合・語の境界・未対応構文の拒否 18 件・走査の規則・折り返し・UTF-8・CRLF・`*` / `#` の語・鍵の到達位置と向きと回数・オペレータの exclusive と行単位化・VISUAL の端点・報せの 6 文言・入力行と取消・`.` の再生を確認 |
| `build/issue100/nib_tests.exe`（引数なし） | `vim_step` の分岐の表と `VimKey` の visit を全 Vim 経路が通るので、977 fixture 全部の再生を含む unit 全体を実行した。8823 → **10149 checks** すべて成功 |
| `build/issue100/nib_tests.exe --vim-dot` / `--vim-text-objects` / `--vim-character-search` | 鍵の分類の表と次キー待ちの写し先を共有するため。909 / 1623 / 621 checks すべて成功（変更前と同数） |
| `build/issue100/nib_tests.exe --coverage-negative` | 早期 return の経路。22 checks 成功 |
| `ctest --test-dir build -R '^nib_unit$' --output-on-failure` | CTest から見た既定実行。成功、2.33 s（#97 の 1.56 s から fixture 135 件ぶん増えた） |
| `python -X utf8 eng/symbols.py --build-dir build --require core application` | 照合器と走査が core の外へロケール・時刻・OS・スレッドのシンボルを出さないこと。**2 libs / 0 violations**、新しい `__std_*` は出ていない（allowlist は変更なし）。`out/issue100-symbols.log` |
| `python -X utf8 eng/conformance.py --build-dir build` | 新しい 16 型の 1 ファイル 1 型と、977 fixture の生成整合（CNF-010）。0 violations、`out/issue100-conformance.log` |
| `clang-format --dry-run --Werror`（`eb79415..HEAD` の C++ 42 ファイル） | 変更 C++ の整形。初回は `Direct2DRenderer.cpp` / `VimPattern.hpp` / `VimState.hpp` ほかが拒否され、`clang-format -i` のあと build と対象テストを再実行して成功 |
| `cmake -S . -B build -U CMAKE_RUNTIME_OUTPUT_DIRECTORY` | 一時出力先だけ既定へ復元。成功、`out/issue100-configure-restore.log`。旧版の再リンクはしない |

computer-use による実機の画面確認は**未実施**（native pipe が繋がらないため試みていない）。起動中の旧版 PID には触れていない。成果物は `build/issue100/NeNeNib.exe`、SHA-256 `93c3029038b12855f00e24bc78e8fdc1e013f6de8f22ac7102af896e5d70d561`。`build/NeNeNib.exe` は旧版なので取り違えない。

FR-003 / ARC-001/003/004/007/009 / CPP-002/004/005/006/011/012 / QLT-001/008/012/013 / CNF-010 を自己レビュー。`optional` は `has_value` / `value` / `value_or` だけで読み、閉じた分岐（`VimKey` / `CommandInput` / `VimEffect` の visit、`VimActionGroup` / `VimPatternAtomKind` / `VimPatternFailure` / `VimSearchNoticeKind` / `InputLinePrompt` の switch）に `default` は無い。照合の失敗は `std::expected` と閉じた enum で返し、例外は使っていない。`reinterpret_cast`・時刻・OS・スレッドは増やしていない。48 を超えた `VimAction` の分岐が関数長 60 行に収まらなくなったので、CPP-012 のとおり「動作 → 大分類」を `constexpr` の表にし、NORMAL と VISUAL は `VimActionGroup` を網羅する switch で写した（閾値は触っていない）。表の欠落と重複は `static_assert` 2 つが落とす（動作の個数を末尾の値から導いて表の大きさに固定し、どの値も表にちょうど 1 行あることを `constexpr` の関数で確かめる。反例として動作を 1 つ足して行を足さない形が実際にコンパイルで落ちることを確認した・`out/issue100-build-negative.log`）。

性能は測っていない（差分は入力と編集の経路で、描画とファイルは触っていない）。**残るリスクは、照合が 1 行ずつ本文を引くので、見つからない検索が本文の行数と長さに比例することだけである**。1 打鍵 0.9 ms の予算への影響は次に速さを測る機会に ADR 0016 の基準値と突き合わせる（ADR 0021）。関連しない設定・テーマ・利用者テーマ・性能・Release build・`check.ps1 -Full` は実行していない。push / レビュー / 統合でも上記の成功結果を再利用する。Waivers: none。

### 5-aa. fixtures.jsonの整形を1つに固定（Issue #98・2026-09-22）

統合単位は [PR #105](https://github.com/hideyukiMORI/nene-nib/pull/105)。以下の成功結果を文書追記・レビュー・統合でも再利用する。節番号は #91 の並行作業と衝突しないように 5-x を空けて 5-aa を取った。

base `b07c574`（`chore/98-fixtures-json-format`）。`tests/vim/fixtures.json` は **3 通り**の形で混ざっていた（Issue #98 は 2 通りと書いたが、数えたら 3 通りあった）: 295 件が indent 2 ＋ 空の `"settings": []` の古い形、668 件が 1 行 1 件の新しい形、そして `viewport-follow-*` の **14 件は 1 行に全部**詰まっていた（295 ＋ 668 ＋ 14 = 977）。整形を決める関数を `eng/vim-oracle.py` に 1 つ置き（`canonical_fixtures_json`）、書き戻す側（`--format` / `--regenerate`）と検査する側（CNF-011・`eng/conformance.py` の `fixture_format_checks`）が同じ 1 か所を呼ぶ形にした（ARC-001）。**既存 668 件の 1 行 1 件の行は、この関数の出力とバイト単位で一致した**（追記者が手で書いてきた形をそのまま正準形にしたので、新しい書き方を誰も覚え直さない）。

整形は `python -X utf8 eng/vim-oracle.py --format` の 1 回だけ実行した（`out/issue98-format.log`）。**0 measured / 977 reused**、Vim は起動していない。977 件の記録（`name` / `text` / `keys` / `settings` / `viewport` の値と順序）は 1 つも変わっておらず、変わったのは JSON のバイト列（108687 → 96756 bytes・2459 → 979 行・SHA-256 は `2e36c3b63bd65ea79ce3fed9af4c1423d70f8d6376005570bf3119c10b546ac1` → `d86f6d3214c9bd8f62f31426a219c1faa834df0ec633a18b8792027a681619a1`）と、生成物 `tests/vim/VimFixtures.hpp` の SHA 行 **1 行だけ**である（`git diff --stat` は `VimFixtures.hpp | 2 +-`）。生成行 977 本は reuse ref から逐語再利用したので、期待値は 1 件も測り直していない。`--format` は reuse ref の fixture の正準形が書き戻したバイト列と一致しないかぎり書かないので、整形が期待値を書き替える経路は無い。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `python -X utf8 eng/vim-oracle.py --format`（1 回目） | 整形と SHA 行の更新。0 measured / 977 reused、`out/issue98-format.log` |
| 同（2 回目・べき等） | 同じ出力で作業ツリーに新しい差分が出ないこと。0 measured / 977 reused、`git status` は同じ 2 ファイルのまま |
| 記録の同一性（`out/issue98-equivalence.log`） | 整形が入力を変えていないこと。977 件の正準形・名前と順序・（空の `settings` を無視した）入力 5 項目がすべて `HEAD` と一致 |
| `python -X utf8 eng/vim-oracle.py --regenerate --only search-dot-count` | 整形後の JSON から部分再生成が通ること。**1 measured / 976 reused** で生成物はバイト単位で同一（`git status` に差分なし）。固定 Vim 9.1（2024 Jan 02, compiled Jan 3 2024 23:53:58）、`out/issue98-regenerate-only.log` |
| `python -X utf8 eng/conformance.py` | CNF-011 の正例と CNF-010 の SHA の整合。0 violations・終了 0、`out/issue98-conformance.log` |
| 同（反例 2 通り・実リポジトリ） | 空の `"settings":[]` を 1 件足す / 末尾改行を消す。どちらも CNF-011 と CNF-010 で `Conformance: 2 violation(s)`・終了 1。戻すと 0 violations・終了 0。`out/issue98-conformance-negative.log` / `negative2.log` |
| `python -X utf8 -m unittest discover -s tests/conformance -p 'test_conformance.py'` | CNF-011 の正例・反例 15 通り（indent 2・1 行に全部・キー順・空の settings・`\u` エスケープ・区切りの後の空白・CRLF・末尾改行なし・行番号の指摘・配列でない JSON・未知のキー・キー不足・壊れた JSON・不正な viewport・ファイル欠落）と既存 CNF 検査の退行。**74 tests OK**（69 → 74） |
| `python -X utf8 -m unittest discover -s tests/conformance -p 'test_vim_oracle.py'` | 正準形そのもの（1 行 1 件・キー順・非 ASCII・べき等・`[]`）・`--format` の逐語再利用と SHA 行だけの差分・入力が変わる整形と別の oracle の拒否・書き戻しの後片付け。**16 tests OK**（10 → 16） |
| `cmake --build build --target nib_tests --parallel 4` | 生成物の 1 行（SHA のコメント）が変わったので再 build した。成功、`out/issue98-build.log` |
| `build/nib_tests.exe`（引数なし） | 977 fixture の再生が変わっていないこと。**10149 checks すべて成功**（#100 と同数）、`out/issue98-unit.log` |
| `ctest --test-dir build -R '^nib_unit$' --output-on-failure` | CTest から見た既定実行。成功、2.13 s、`out/issue98-ctest.log` |
| `clang-format --dry-run --Werror tests/vim/VimFixtures.hpp` | 生成物の整形（ファイル全体が `// clang-format off`）。指摘なし・終了 0 |

production の C++・fixture の入力・期待値・閾値・除外・重大度は 1 つも変えていない。`eng/symbols.py`・カバレッジ・速さ（QLT-014）・Release build・`check.ps1 -Full` は実行していない。リンク境界と core / application の翻訳単位が不変で、差分が JSON のバイト列と検査器・生成器の Python だけだからである（QLT-001 / QLT-012・ADR 0021）。push / レビュー / 統合でも上記の成功結果を再利用する。Waivers: none。

**無関係な既存の失敗（この作業が原因ではない）。** `python eng/test-conformance.py`（conformance 全体・157 tests）は `tests/conformance/test_verification_policy.py` の 6 件で落ちる。5 件は `subprocess` の `text=True` が cp932 の端末出力を読めず `result.stdout` / `stderr` が `None` になるもので、**未変更の `b07c574` を別 worktree に出して同じテストだけを走らせても同じ 5 件が落ちる**（この機械の環境依存）。6 件目は `test_pr_event_uses_the_record_validator` で、`eng/validate-git.ps1` が `git show -s --format=%B` の UTF-8 出力を pwsh の入力符号化（この機械は Shift_JIS）で受けるため、日本語の subject が壊れて GIT-003 になる。**既存の main の commit `8f2c363` を同じ経路に流しても同じく落ちる**（`chore(eng): fixtures.jsonの正準形めEか所…` のように化ける）ので、#98 の commit に固有の問題ではない。Issue #94 が直したのは python 側の出力（`PYTHONUTF8`）で、pwsh 側の入力符号化は残っている。設計リナへ報告して別 Issue に切る。#98 に関わる `test_conformance.py` / `test_vim_oracle.py` / `test_coverage.py` / `test_symbols.py` / `test_speed.py` は成功している。

### 5-ab. VISUALの変更の`.`（Issue #91・ADR 0033・2026-09-22）

統合単位は [PR #107](https://github.com/hideyukiMORI/nene-nib/pull/107)。以下の成功結果を文書追記・レビュー・統合でも再利用する。

base `a44f380`（origin/main。ADR 0033 の 1 commit を積んだ `feat/91-vim-visual-dot` を #98 の merge のあとに rebase した）。ADR 0033 を実測のあとで**受理**にし、**実測と食い違った 2 つの決定は期待値ではなく ADR と実装を直した**（決定 6 の `N.`・決定 5 の桁の数え方）。`VimRepeatRecord` に `std::optional<VimVisualExtent> visual`、`VimReplay` に `std::optional<VimVisualExtent> reselect` を足し、新しい 3 型（`VimCharacterExtent` / `VimLineExtent` / `VimVisualExtent`）と core の純関数 1 本（`vim_visual_reselect`）を置いた。記録の分岐は `vim_step` の後段の 1 か所のまま（`vim_recorded` が前のモードで分け、`visual_recorded` が VISUAL の記録を作る）。controller は `VimReplay.reselect` があれば `VimSelect` と同じ写しで選択を置いてから鍵を流す。UI・IME・描画・保存形式・schema・依存・ゲートの閾値は変更していない。

`python -X utf8 out/issue91-oracle/probe.py`（71）/ `probe2.py`（31）/ `probe3.py`（23）/ `probe4.py`（13）を**実装の前**に実行し、固定 Vim 9.1 を 138 ケース起動して決定を確かめた。証拠は `out/issue91-oracle/probe*.json` / `probe*.txt`。Vim ソースは読んでいない。すべてのケースを命令の切れ目で区切って測っている（5-v の教訓）。決定 1〜4・7〜9 は提案のまま一致。食い違った 2 点は ADR 0033 の「文脈」に実測を書き、決定 5・6 を直した。(1) `N.` は VISUAL の記録では回数を使わない（`vlld` `2.` は 3 文字・`Vd` `2.` は 1 行）。(2) 桁は Vim では**仮想桁**で、1 行の記録は個数、複数行の記録は最終行の**絶対**桁、`$` の記録は「行末まで」のまま。1 行の個数・複数行の絶対桁・`$` は実装で合わせ、**仮想桁だけは合わせていない**（この engine の `Column` は code point・`wanted_column` から `H M L` まで一貫している。`virtcol` は `Ctrl-v` と同じ前提なので別の Issue）。Tab と幅の混ざった本文の fixture は採らず、`--vim-dot` の対象 unit が engine の答えを固定する。

`python -X utf8 out/issue91-oracle/add-fixtures.py --write` は候補 82 件を「命令の切れ目で区切った形」と「1 回の `:normal!` の形」の両方で測り、食い違う 4 件（`clamp-columns-then-dot` / `empty-line-source` / `empty-line-record` / `dot-inside-visual`。どれもビープが残りの鍵を捨てる列）を機械的に拒否した（`out/issue91-oracle/add-fixtures.txt`）。`python -X utf8 eng/vim-oracle.py --regenerate --only visual-dot-` は 78 件だけを測り、既存 977 行を `453a7f7` から逐語再利用した（`out/issue91-oracle/regenerate.log`）。`git diff` の削除行は metadata 2 行だけ。977 件の入力 SHA-256 は #98 の正準形どおり `d86f6d3214c9bd8f62f31426a219c1faa834df0ec633a18b8792027a681619a1` で、1055 件は `fd673b1a3f9bfd93c8628049bd058558ea3aeaac1d9a5a048401d79b62ba135d` になった。同じ選択で 2 回目を再生成して**バイト単位で同一**（`out/issue91-oracle/regenerate2.log`）。**初回の再生で 78 件すべてが実装と一致した**（期待値を直した fixture は無い）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `. ./eng/toolchain.ps1` → `cmake -S . -B build -G Ninja -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=C:/Users/info/WORKS/NeNeNib-91/build/issue91` | worktree の新しい build。起動中の旧版を保持し、同じ target / flags で出力先だけ分離。成功、`out/issue91-configure.log`（generator を明示しないと CMake が Visual Studio を選び `CXX=clang-cl` を無視するので、`-G Ninja` を足した。本体の build と同じ generator） |
| `cmake --build build --target nib_tests --parallel 4`（engine の 1 つめ） | 新しい 3 型・純関数・記録の分岐の build。初回は clang-tidy が `visual_recorded` / `vim_recorded` の**引数 5 個**（上限 4）を拒否した（`out/issue91-build1.log`）。`VimState` と `VimEffect` を `VimStep` 1 つにまとめて修正し成功（`out/issue91-build2.log`）。閾値・除外・重大度は変えていない |
| `cmake --build build --target nib_tests --parallel 4`（unit の契約） | 初回は clang-tidy が `last_change.value().visual` の未検査アクセスを拒否した（`out/issue91-build3.log`）。`has_value` を挟む小さな述語に寄せて成功（`out/issue91-build4.log`） |
| `build/issue91/nib_tests.exe --vim-dot` | **1593 checks すべて成功**。新規 78 fixture ＋ 既存 99 dot / 8 共有境界と、記録の中身（大きさ・鍵の列・回数を持たないこと）・選び直しと再生・`N.` が大きさを変えないこと・畳まれても記録が縮まないこと・記録の入れ替え・VISUAL の中の `.`・undo 1 単位と redo・CRLF の保存 bytes・**仮想桁との差（Tab と幅の混在）**を確認 |
| `build/issue91/nib_tests.exe`（引数なし） | `vim_step` の後段を全 Vim 経路が通るので、1055 fixture 全部の再生を含む unit 全体を実行した。10149 → **10833 checks** すべて成功 |
| `build/issue91/nib_tests.exe --vim-search` / `--vim-visual-yank` / `--vim-visual-wanted` / `--vim-text-objects` / `--vim-replace` / `--vim-character-search` | VISUAL の選択・待ち・`.` の記録を共有するため。1375 / 265 / 208 / 1623 / 460 / 621 checks すべて成功（変更前と同数） |
| `build/issue91/nib_tests.exe --coverage-negative` | 早期 return の経路。22 checks 成功 |
| `ctest --test-dir build -R '^nib_unit$' --output-on-failure` | CTest から見た既定実行。成功、2.28 s（#98 の 2.13 s から fixture 78 件ぶん増えた） |
| `python -X utf8 eng/symbols.py --build-dir build --require core application` | `std::variant` の大きさの型と新しい純関数が core の外へロケール・時刻・OS・スレッドのシンボルを出さないこと。**2 libs / 0 violations**、新しい `__std_*` は出ていない（allowlist は変更なし）。`out/issue91-symbols.log` |
| `python -X utf8 eng/conformance.py --build-dir build` | 新しい 3 型の 1 ファイル 1 型と、1055 fixture の生成整合（CNF-010）・正準形（CNF-011）。0 violations、`out/issue91-conformance.log` |
| `clang-format --dry-run --Werror`（`a44f380..HEAD` と作業ツリーの C++ 10 ファイル） | 変更 C++ の整形。初回は `VimVisualReselect.cpp` / `VimStep.cpp` / `NibTests.cpp` が拒否され、`clang-format -i` のあと build と対象テストを再実行して成功。`out/issue91-format.log` |
| `cmake --build build --target nib_tests NeNeNib --parallel 4`（最終） | 整形後の全 target。成功、`out/issue91-build6.log` |

computer-use による実機の画面確認は**未実施**（native pipe が繋がらないため試みていない）。起動中の旧版 PID には触れていない。本体の `C:\Users\info\WORKS\NeNeNib` には触れていない（worktree `NeNeNib-91` の中だけで build した）。成果物は `build/issue91/NeNeNib.exe`。

FR-003 / ARC-001/004/007/009 / CPP-002/003/004/006/011/012 / QLT-001/008/012/013 / CNF-010/011 を自己レビュー。`optional` は `has_value` / `value` / `value_or` だけで読み、閉じた分岐（`VimVisualExtent` の visit、`VimMode` / `VimColumnWish` の switch）に `default` は無い。新しい 3 型はどれも公開 aggregate で、比較は非メンバー（CPP-003）。`reinterpret_cast`・時刻・OS・スレッドは増やしていない。記録は本文・履歴・レジスタを持たず、選択の正本は `EditorState` のまま（engine は選択を持たない・ADR 0018 の決定 3）。

性能は測っていない（差分は入力と編集の経路で、描画とファイルは触っていない）。`VimRepeatRecord` が `optional<variant>` 1 つぶん大きくなるので、1 打鍵あたりの複製がわずかに増える。1 打鍵 0.9 ms の予算への影響は次に速さを測る機会に ADR 0016 の基準値と突き合わせる（ADR 0021）。関連しない設定・テーマ・利用者テーマ・性能・Release build・`check.ps1 -Full` は実行していない。push / レビュー / 統合でも上記の成功結果を再利用する。Waivers: none。

### 5-ac. 割り込みをengineの1本の経路へ（Issue #92・2026-09-22）

統合単位は [PR #109](https://github.com/hideyukiMORI/nene-nib/pull/109)。以下の成功結果を文書追記・レビュー・統合でも再利用する。

base `433bb9a`（origin/main。#91 統合後）。worktree `C:\Users\info\WORKS\NeNeNib-92` の中だけで作業し、本体の `C:\Users\info\WORKS\NeNeNib` には触れていない。**ADR は書いていない**（ARC-004 の適用で設計は変わらないため。ADR 0030 の「結果」の 1 文だけを更新した）。engine に純関数 `vim_interrupted(const VimState &) -> VimState` を 1 つ足し、controller の `interrupt_vim_insert` はそれを呼んで `VimState` を置き換え、履歴の単位を切るだけにした。捨てる範囲は入力行の取消（`vim_cancelled_input`・ADR 0032 の決定 1）と同じなので無名名前空間の `input_discarded` 1 か所に書き、取消はそれに「欲しい列を捨てる」と「モードを休止へ戻す」を足したものとして書き直した（モードの規則は `rested_mode` 1 か所へ抜き出し、`search_rested` もそれを使う）。新しい鍵・`VimKey` の選択肢・`VimEffect`・fixture・UI・IME・描画・保存形式・schema・依存・ゲートの閾値は 1 つも増やしていない。

**振る舞いが変わらないことの測り方**: production の 3 ファイルを `git stash` して base の状態で同じ build を作り、対象 scope と unit 全体を先に測ってから戻して測り直した（`out/issue92-unit.log` は最終の値）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake -S . -B build/issue92 -G Ninja -DCMAKE_BUILD_TYPE=Debug` | worktree の新しい build（`-G Ninja` を明示しないと Visual Studio が選ばれて `CXX=clang-cl` が無視される・#91 の教訓）。`build/issue92/.cmake/api/v1/query/codemodel-v2` も手で作った。成功 |
| `cmake --build build/issue92`（Debug・clang-tidy・ASan / UBSan） | 新しい純関数と controller の書き換え。unit の契約で `before.last_character_search.value()` が `bugprone-unchecked-optional-access` に拒否されたので、`has_value` を挟む形に直して成功（閾値・除外・重大度は触っていない） |
| `build/issue92/nib_tests.exe --vim-open-line-external` | 割り込みそのもの（同位置クリック・Ctrl+Z・全選択・未記録の編集で反復が解除されること）。**46 checks（変更前と同数）**→ 契約を足して **54 checks**。すべて成功 |
| `build/issue92/nib_tests.exe --vim-open-lines` / `--vim-open-line-recovery` | `o` / `O` の回数反復と `VimInsertRepeat` の後段。**565 → 573 checks**（+8 は足した契約ぶん）/ **430 checks（変更前と同数）**。すべて成功 |
| `build/issue92/nib_tests.exe --vim-dot` | `recording` / `last_change` を捨てる範囲に足したので、`.` の記録の確定・破棄・回数の置換・VISUAL の再生の退行を確認。**1593 checks（変更前と同数）**すべて成功 |
| `build/issue92/nib_tests.exe --vim-search` / `--vim-character-search` | `vim_cancelled_input` と `search_rested` を書き換えたので、入力行の取消が捨てるものと `last_search` / `;` `,` の記憶が保たれることを確認。**1375 / 621 checks（どちらも変更前と同数）**すべて成功 |
| `build/issue92/nib_tests.exe`（引数なし） | `VimState` の後段は全 Vim 経路が通るので unit 全体を 1 回。**10833 checks（変更前と同数）**→ 契約を足して **10841 checks**。1055 fixture の再生を含めすべて成功 |
| `build/issue92/nib_tests.exe --coverage-negative` | 早期 return の経路。22 checks 成功 |
| `ctest --test-dir build/issue92 --output-on-failure --no-tests=error` | 4 件すべて成功（`toolchain_smoke` / `nib_unit` / `nib_adapters` / `nib_themes`）。`nib_unit` 単体は 2.33 s（#91 の 2.28 s と同程度） |
| `python eng/symbols.py --build-dir build/issue92 --require core application` | 新しい純関数が core の外へロケール・時刻・OS・スレッドのシンボルを出さないこと。**2 libs / 0 violations**、allowlist は変更なし |
| `python eng/conformance.py` / `python eng/conformance.py --build-dir build/issue92` | 字句検査と実際の build graph（1 ファイル 1 型・`<atomic>`・生成物の SHA）。どちらも **0 violations** |
| `clang-format --dry-run --Werror`（変更した C++ 4 ファイル） | 整形。指摘なし |

受け入れ条件の「controller に `VimState` のフィールド代入が残らないこと」は grep で確かめた。`git grep -nE "vim\(\)\.[a-z_]+ *=|next\.(mode|count|pending|input_wait|last_character_search|last_search|wanted_column|scroll_lines|insert_repeat|recording|last_change|unnamed_register) *=" -- src/application src/ui` が返すのは `state_.vim().mode == core::VimMode::insert` の比較 2 行と `EditorState.hpp` のメンバー宣言 1 行だけで、代入は 1 つも無い。CNF の規則にはしていない（字句で「代入」を一般に言い当てる検査は偽陽性が多く、engine の外で `VimState` を組み立てる正当な経路＝`vim_resting_from` と区別できないため）。engine が捨てる範囲そのものは `--vim-open-line-external` の契約が守る。

対象を限定した理由: 差分は engine の純関数 1 本と controller の 1 関数と unit の契約 1 つだけで、fixture の入力・期待値・閾値・除外・重大度・生成物・schema・依存・UI は 1 つも変えていない。したがってカバレッジ・速さ（QLT-014）・Release build・`check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。画面確認は**未実施**（描画に関わる差分が無く、exe も作っていない）。push / レビュー / 統合でも上記の成功結果を再利用する。Waivers: none。

**設計の通りに「捨てるもの」を広げた 1 点を記録する。** base の `interrupt_vim_insert` は `insert_repeat` だけを消していたが、`vim_interrupted` は組み立て中の `.` の記録（`recording`）・保留オペレータ・回数・次キー待ちも捨てる。この経路は INSERT でだけ呼ばれ（`vim_boundary()` が INSERT では `absorb` を返すので Vim 自身の編集はこの経路を通らない）、INSERT で立ち得るのは `recording` だけなので、実際に増えるのは「割り込まれた INSERT の `.` の記録を捨てる」1 点である。engine の外から入った本文は鍵の列に入らず再生できないので、`insert_repeat` を捨てるのと同じ理由で捨てるのが正しい。固定 Vim に外部の割り込みは無いので oracle では測れず、fixture ではなく `--vim-open-line-external` の契約が守る。

### 5-ad. テキストオブジェクトの残る3点（Issue #99・ADR 0031 の補足・2026-09-22）

統合単位は [PR #110](https://github.com/hideyukiMORI/nene-nib/pull/110)（draft・ブランチ `feat/99-vim-text-object-residuals`）。以下の成功結果を文書追記・レビュー・統合でも再利用する。節番号は #92 の並行作業と衝突しないよう 5-ad を取った。

base `9e41f79`（origin/main。実装の 1 commit を積んだあと #92 の merge の上へ rebase した。衝突なし）。ADR 0031 は新しく書かず「補足（2026-09-22・Issue #99）」を足し、**決定 2（返り値）と決定 4（VISUAL の両端）を実測に合わせて直した**。`vim_text_object_range` は 1 本のままで（ARC-001）、返り値を `std::optional<VimMotionRange>` から閉じた和型 `VimTextObjectOutcome = std::variant<VimTextObjectSpan, VimTextObjectCancel>` に変えた。新しい 3 型は `VimTextObjectSpan`（範囲＋VISUAL が置く選択）・`VimTextObjectCancel`（取消のあとに残る選択）・`VimTextObjectOutcome`（別名）で、どれも 1 ファイル 1 型（CPP-011）。後ろ向きの走査は `VimWordMotion` の 2 本（`vim_word_object_begin` / `vim_word_object_previous_end`）に寄せ、前向きの走査と同じ表を引く。engine は `std::visit` で 2 つの答えに写し、取消を `VimMoveTo` / `VimSelect` / `VimNoEffect` にする。UI・IME・描画・保存形式・schema・依存・ゲートの閾値は変更していない。

`python -X utf8 out/issue99-oracle/probe.py`（113）/ `probe2.py`（79）/ `probe3.py`（16）を**実装の前**に実行し、固定 Vim 9.1 を **208 ケース**起動して 3 点の規則を閉じた。証拠は `out/issue99-oracle/probe*.json` / `probe*.txt`。Vim ソースは読んでいない。すべてのケースを命令の切れ目で区切って測っている（5-v の教訓）。(a) は「走査が止まった端」（語は前向きなら本文の終わり・後ろ向きなら本文の先頭、引用符と括弧は動かない）、(b) は**実装が既に正しく**決定 7 のままで一致、(c) は種類ごとに置き方が違う（語は anchor を動かさず caret だけ・括弧は必ず前向き・引用符は向きのまま）。閉じなかったのは「後ろ向きの選択で引用符のどの対を選ぶか」の 1 点だけで、ADR 0031 の「残る穴」に残して fixture を採っていない。

`python -X utf8 out/issue99-oracle/add-fixtures.py --write` は候補 35 件を「命令の切れ目で区切った形」と「1 回の `:normal!` の形」の両方で測り、**食い違いは 0 件**だった（`out/issue99-oracle/add-fixtures.txt`）。`python -X utf8 eng/vim-oracle.py --regenerate --only text-object-residual-` は **35 measured / 1055 reused**（reuse ref `24f0ca7`・`out/issue99-oracle/regenerate.log`）。`git diff` の削除行は metadata 2 行だけで、既存 1055 行は逐語再利用した。1090 件の入力 SHA-256 は `fd673b1a3f9bfd93c8628049bd058558ea3aeaac1d9a5a048401d79b62ba135d` → `e4c933becb6ac8a8ce3125af3a9b8405d4c417c2956373a56fb7a4bae36287ef`。**初回の再生で 35 件すべてが実装と一致した**（期待値を直した fixture は無い）。VISUAL の取消はビープが後続の鍵を捨てるので 1 回の `:normal!` に乗らず、対象 unit の契約 7 件（`verify_vim_text_object_residuals`）が engine の答えを固定する。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `. ./eng/toolchain.ps1` → `cmake -S . -B build -G Ninja -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=C:/Users/info/WORKS/NeNeNib/build/issue99` | 起動中の旧版を保持し、同じ target / flags で出力先だけ分離。成功、`out/issue99-configure.log` |
| `cmake --build build --target nib_tests --parallel 4`（範囲関数と engine） | 新しい 3 型・後ろ向きの走査 2 本・返り値の型の変更を Debug / tidy / ASan / UBSan で。**初回から成功**（clang-tidy の指摘なし）、`out/issue99-build1.log`。rebase 後の再 build も成功（`out/issue99-build2.log`） |
| `build/issue99/nib_tests.exe --vim-text-objects` | **1912 checks すべて成功**（1623 から新規 35 fixture ＋ 契約 7 件ぶん）。範囲・取消・VISUAL の置換・後ろ向きの伸ばし・`.` の記録を確認。初回は契約 1 件が落ちたが、原因は**テスト側**で `vim_replay` を連ねたときに `4l` が行頭からでなく前の位置から動いていたことで、各列に `0` を足して修正した（実装と期待値は変えていない） |
| `build/issue99/nib_tests.exe`（引数なし） | 範囲関数の返り値の型を変えたので、1090 fixture 全部の再生を含む unit 全体を 1 回実行した。10833 → **11130 checks** すべて成功 |
| `build/issue99/nib_tests.exe --vim-dot` / `--vim-visual-wanted` / `--vim-visual-yank` | 取消の写し先（`VimMoveTo`）と VISUAL の選択・希望列を共有するため。1593 / 208 / 265 checks すべて成功（変更前と同数） |
| `build/issue99/nib_tests.exe --coverage-negative` | 早期 return の経路。22 checks 成功 |
| `ctest --test-dir build -R '^nib_unit$' --output-on-failure` | CTest から見た既定実行。成功、2.32 s（#92 の 2.28 s から fixture 35 件ぶん増えた） |
| `python -X utf8 eng/symbols.py --build-dir build --require core application` | 新しい走査が core の外へロケール・時刻・OS・スレッドのシンボルを出さないこと。**2 libs / 0 violations**、新しい `__std_*` は出ていない（allowlist は変更なし）。`out/issue99-symbols.log` |
| `python -X utf8 eng/conformance.py --build-dir build` | 新しい 3 型の 1 ファイル 1 型と、1090 fixture の生成整合（CNF-010）・正準形（CNF-011）。0 violations、`out/issue99-conformance.log` |
| `clang-format --dry-run --Werror`（`9e41f79..HEAD` と作業ツリーの C++ 10 ファイル） | 変更 C++ の整形。初回は `VimStep.cpp` / `VimTextObjectRange.cpp` の三項演算子の折り返しが拒否され、`clang-format -i` のあと build と対象テストを再実行して成功 |
| `cmake --build build --target nib_tests NeNeNib --parallel 4`（最終） | 整形後の全 target。成功、`out/issue99-build5.log` |

computer-use による実機の画面確認は**未実施**（native pipe が繋がらないため試みていない）。起動中の旧版 PID には触れていない。成果物は `build/issue99/NeNeNib.exe`。`build/NeNeNib.exe` は旧版なので取り違えない。

FR-003 / ARC-001/004/007/009 / CPP-002/003/004/006/011/012 / QLT-001/008/012/013 / CNF-010/011 を自己レビュー。`optional` は `has_value` / `value` / `value_or` だけで読み、閉じた分岐（`VimTextObject` / `VimTextObjectScope` / `VimRegisterKind` の switch、`VimTextObjectOutcome` の visit）に `default` は無い。新しい 3 型はどれも公開 aggregate でメソッドを持たない（CPP-003）。`reinterpret_cast`・時刻・OS・スレッドは増やしていない。範囲関数は純関数のままで、選択の正本は `EditorState`（engine は選択を持たない・ADR 0018 の決定 3）。

性能は測っていない（差分は入力と編集の経路で、描画とファイルは触っていない）。`VimTextObjectOutcome` は `VimMotionRange` ＋ `Selection` ぶんだけ戻り値が大きくなるが、テキストオブジェクトの鍵 1 打ごとに 1 つ作るだけである。1 打鍵 0.9 ms の予算への影響は次に速さを測る機会に ADR 0016 の基準値と突き合わせる（ADR 0021）。関連しない設定・テーマ・利用者テーマ・性能・Release build・`check.ps1 -Full` は実行していない。push / レビュー / 統合でも上記の成功結果を再利用する。Waivers: none。

### 5-ae. Vim の桁を仮想桁で数える経路（Issue #108・ADR 0034・2026-09-22）

統合単位は [PR #113](https://github.com/hideyukiMORI/nene-nib/pull/113)（draft・ブランチ `feat/108-vim-virtual-column`）。以下の成功結果を文書追記・レビュー・統合でも再利用する。節番号は #99（5-ad）の次を取った。

base `220fe76`（origin/main。ADR 0034 の 1 commit を積んだあと #99 の merge の上へ rebase した。衝突なし）。ADR 0034 は**実測のあとで受理**にし、決定 1・2・3 を 4 点直して「補足」に差を残した（DEL は 1 桁・`<200b>` の 6 桁の class・Tab の上のキャレットは最後の桁・逆引きの端は行の内容の終わり）。新しい型は `DisplayWidth` / `DisplayWidthRange` / `VirtualColumn` の 3 つで、どれも 1 ファイル 1 型（CPP-011）。`VimWantedColumn.column` と `VimCharacterExtent.column` が `VirtualColumn` になり、着地は `offset_at_virtual_column` 1 本を通る（`caret_on_line` の `TextBuffer::offset_of` を置き換えた・ARC-001）。UI・IME・描画・保存形式・schema・依存・ゲートの閾値は変更していない。

`python out/issue108-oracle/probe.py`（73 ケース＋ 164 code point）/ `probe2.py`（全 1,114,112 code point の掃引）/ `probe3.py`（24 ＋ viewport 6）/ `probe4.py`（12）/ `probe5.py`（9 ＋ 12）を**実装の前**に実行し、固定 Vim 9.1 で桁の規則を閉じた。証拠は `out/issue108-oracle/probe*.json` / `probe*.txt`（`out/` は追跡外なので作業機にだけある）。Vim ソースは読んでいない。表示幅の表 491 行は `probe2` の掃引値そのもので、`make-table.py` が 1 度だけ C++ に写した。

`python out/issue108-oracle/add-fixtures.py --write` は候補 47 件（うち viewport 3 件）を「命令の切れ目で区切った形」と「1 回の `:normal!` の形」の両方で測り、**食い違いは 0 件**だった（`out/issue108-oracle/add-fixtures.txt`）。`python eng/vim-oracle.py --regenerate --only virtcol-` は **47 measured / 1090 reused**（reuse ref `274a280`）。既存 1090 行は逐語再利用し、削除行は metadata 2 行だけ。1137 件の入力 SHA-256 は `e4c933becb6ac8a8ce3125af3a9b8405d4c417c2956373a56fb7a4bae36287ef` → `0b0756ba9406f285d2939252bd0effac7e2485b7f757b07a922bf657479df80a`。**初回の再生で 47 件すべてが実装と一致した**（期待値を直した fixture は無い）。

**桁の意味を変えても既存が壊れていない証拠**は 2 つある。(1) Tab か全角を含む本文に `j` `k` `H M L` `.` を打つ既存 fixture 9 件（`visual-wanted-unicode` / `visual-yank-tab-indent` / `dot-tab-remove` / `dot-tab-change` / `dot-tab-replace` / `visual-dot-utf8-one-line` / `visual-dot-utf8-last-line` / `visual-dot-utf8-multiline` / `visual-dot-utf8-linewise`）が引き続き一致する。(2) 既定実行の 11130 → 11553 checks のうち、期待値を直したのは ADR 0033 が「穴」として残していた `--vim-dot` の契約 2 件（`ab<Tab>cd` の `vll` と、ASCII 3 桁から全角の行へ）だけで、どちらも固定 Vim の答え（`cd\nrq` / `defghij\nうえおかきくけこ`）に合わせた。同じ 3 件は `virtcol-dot-*` の fixture にも採ってある。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake -S . -B build/issue108 -G Ninja -DCMAKE_BUILD_TYPE=Debug` → `cmake --build build/issue108` | 新しい 3 型・491 行の表・`std::ranges::lower_bound` の探索・型を変えた 2 つの値を Debug / clang-tidy / ASan / UBSan で。全 target 成功（clang-tidy の指摘なし） |
| `build/issue108/nib_tests.exe --vim-virtual-column` | 新しい対象。**424 checks 成功**（fixture 47 件＋表の境界 24 点＋桁の 3 関数＋逆引きの端） |
| `build/issue108/nib_tests.exe`（引数なし） | `VimWantedColumn` の型を変えたので 1137 fixture の再生を含む unit 全体を 1 回。11130 → **11553 checks** すべて成功 |
| `build/issue108/nib_tests.exe --vim-visual-wanted` / `--vim-line-jumps` / `--vim-line-jump-recovery` / `--vim-dot` | 欲しい列を使う経路（`j` `k` `H M L` `gg G` `Ctrl-d/u/f/b`）と VISUAL の `.` の桁。208 / 989 / 15 / 1593 checks すべて成功（`--vim-dot` は #99 と同数） |
| `build/issue108/nib_tests.exe --vim-text-objects` / `--vim-search` | 桁を **変えない** と決めた経路（決定 5）が動いていないこと。1912 / 1375 checks 成功（#99 と同数） |
| `ctest --test-dir build/issue108 --output-on-failure --no-tests=error` | 4 件すべて成功。`nib_unit` は 2.74 s（#99 の 2.32 s から fixture 47 件ぶん増えた） |
| `python eng/symbols.py --build-dir build/issue108 --require core application` | 表と桁の関数が core の外へロケール・時刻・OS・スレッドのシンボルを出さないこと。**2 libs / 0 violations**、新しい `__std_*` は出ていない（allowlist は変更なし） |
| `python eng/conformance.py` / `--build-dir build/issue108` | 新しい 3 型の 1 ファイル 1 型（CNF-002）と、1137 fixture の生成整合（CNF-010）・正準形（CNF-011）。どちらも **0 violations** |
| `clang-format --dry-run --Werror`（変更した C++ 9 ファイル） | 整形。初回は表の 491 行が 1 行 1 件では拒否されたので `clang-format -i` で 2 件ずつに詰め、build と対象テストを再実行して成功 |
| `python eng/measure-speed.py --check --executable build/issue108-release/NeNeNib.exe` | **1 打鍵に行頭からの走査が増える**ので QLT-014 を明示実行した（ADR 0021 の「差分が速さに関わるとき」）。Release を別に build し、5 本すべてを 5 回。**0 regression**：1 打鍵 **0.936 ms**（基準 0.906）・16 MiB **270.950 ms**（基準 249.783）・起動 214.929 ms（基準 191.488）・窓 34.531 ms（基準 34.933）・200 打鍵 2.702 ms（基準 2.695）。基準値・許容（25 % / 下限 2 ms）は変更していない |

対象を限定した理由: 差分は core の新しい純関数 3 本＋表 1 つ、engine の桁の型 2 つとその参照、unit の対象 1 つと期待値 2 件、fixture 47 件である。`VimWantedColumn` の型が engine 全体に触るので unit 全体を 1 回回し、1 打鍵の走査が増えるので速さを明示実行した。設定・テーマ・利用者テーマ・Ex・パレット・adapters・`check.ps1 -Full` は差分の依存先でも呼び出し元でもないので実行していない（QLT-001 / QLT-012・ADR 0021）。画面確認は**未実施**（描画の桁は DirectWrite のままで、仮想桁は engine の意味論にしか出ない）。push / レビュー / 統合でも上記の成功結果を再利用する。Waivers: none。

**表の出典を「Unicode の版」から「Vim の実測」に変えた 1 点を記録する。** ADR 0034 の提案は EastAsianWidth の版をコメントに書くとしていたが、Vim 9.1 に版を問い合わせる手立てが無く、Unicode の表を写すと Vim との差が黙って入る。`strdisplaywidth('a' . nr2char(cp)) - 1` を全 code point で測った値をそのまま表にし、`DisplayWidthRange.hpp` の冒頭にその測り方を書いた。ゲートが守るのは「Vim と同じ答えを返すこと」（fixture・CNF-010）なので、正本も Vim に寄せてある。表の昇順と重なりの無さは `static_assert` が守る。

### 5-af. 矩形 VISUAL（Issue #112・ADR 0035・2026-09-22）

統合単位は [PR #118](https://github.com/hideyukiMORI/nene-nib/pull/118)（draft・ブランチ `feat/112-vim-visual-block`）。以下の成功結果を文書追記・レビュー・統合でも再利用する。節番号は引き継ぎが #112 に予約していた 5-af を取った（#111 が先に 5-ag へ入った）。

base `87d8d71`（origin/main。ADR 0035 の 1 commit を積んだあと #111 の merge の上へ rebase した。衝突なし）。ADR 0035 は**実測のあとで受理**にし、決定 2・4・5・6・7 を 6 点直して「補足」に差を残した（端に掛かった文字は丸ごと外して空白に置き換える・取る空白と残す空白は内側と外側で逆・`$` の印はレジスタに残らない・`r` は桁の数だけ書く・貼付の右の埋めは本文が続くときだけ・再生は角ではなく記録した幅から）。新しい型は `VimBlockWidth` / `VimBlockLine` / `VimBlockEdit` / `VimBlockExtent` / `VimRemoveBlock` / `VimReplaceBlock` / `VimInsertBlock` / `VimBlockRange` の 8 つで、どれも 1 ファイル 1 型（CPP-011）。`VimMode` / `VimSpecialKey` / `VimRegisterKind` / `VimVisualExtent` / `VimEffect` の 5 つの閉じた和型が増え、写し漏れは `switch` / `std::visit` の網羅性が落とした（CPP-002）。保存形式・schema・依存・ゲートの閾値・描画（renderer）は変更していない。

`python out/issue112-oracle/probe.py`（109 ケース）/ `probe2.py`（39）/ `probe3.py`（25）/ `probe4.py`（21）/ `probe5.py`（20）/ `probe6.py`（14）を**実装の前**に実行し、固定 Vim 9.1 で矩形の幾何・編集・貼付・`.`・取消を閉じた。証拠は `out/issue112-oracle/probe*.json` / `probe*.txt`（`out/` は追跡外なので作業機にだけある）。Vim ソースは読んでいない。

`python out/issue112-oracle/add-fixtures.py --write` は候補 151 件を「命令の切れ目で区切った形」と「1 回の `:normal!` の形」の両方で測り、**5 件を拒否**した（`y-all-short` / `y-last-short` / `y-find` / `d-all-short` / `r-beyond-line`。走査の失敗が残りの打鍵を捨てる・Issue #87）。拒否した 3 つの境界（行が矩形より手前で終わるときの `y` `d` `r`）は `--vim-visual-block` の契約で測ってある。さらに `u`（VISUAL では小文字化）と `.` のあとの `u` の 2 件は、固定 Vim と単位・モードが違うので採らなかった（ADR 0035 の補足）。採用は **144 件**・計 1320 件。

oracle の鍵の記法に `<C-v>` を足した（`KEY_NAMES`）ので測定コードが変わり、`--only` の部分再生成は使えない。**1 回目は全件（`--regenerate`・1322 measured / 0 reused）**で回し、**既存 1176 行が 1 行残らず逐語で一致した**（差分は metadata 2 行と追記 148 行だけ）＝ `<C-v>` の追加が既存の測定を動かしていない証拠になる。そのあと 2 件を落として `--regenerate --only block-`（**144 measured / 1176 reused**・reuse ref `d52bb35`）で締めた。既存 1176 行は逐語再利用し、削除行は metadata 2 行だけ。入力の SHA-256 は `5cae75d18203f861d928001a1e2b85be1526e60981a677bdf5f57a29db84b54e`（1176 件）→ `cd5bf5811457f569ccd2c303abc0f444bc176f9de0e2132e78558f65b5faf7d5`（1320 件）。**初回の再生で 144 件中 141 件が実装と一致し、3 件（`block-dot-short-line` / `block-undo-dot-r` / `block-u-does-not-undo`）だけが食い違った**。期待値は 1 つも直していない: 1 件は実装を直し（再生の幅を記録から取る・ADR 0035 の補足 6）、2 件は固定 Vim と合わせられない差として候補から外した。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake -S . -B build/issue112 -G Ninja -DCMAKE_BUILD_TYPE=Debug` → `cmake --build build/issue112` | 8 つの新しい型・5 つの和型の写し先・controller の 3 つの効果・窓の鍵の写しを Debug / clang-tidy / ASan / UBSan で。全 target 成功（clang-tidy の指摘なし） |
| `build/issue112/nib_tests.exe --vim-visual-block` | 新しい対象。**1204 checks 成功**（fixture 144 件＋描画と Ctrl+C / Ctrl+X ＋短い行の 3 境界＋ undo 1 単位＋ CRLF ＋範囲外の 9 鍵＋ `vim_block_range` と `vim_visual_reselect` の直接測定） |
| `build/issue112/nib_tests.exe`（引数なし） | `VimMode` / `VimEffect` / `VimRegisterKind` / `VimState` を変えたので 1320 fixture の再生を含む unit 全体を 1 回。11873 → **13076 checks** すべて成功 |
| `build/issue112/nib_tests.exe --vim-dot` / `--vim-visual-yank` / `--vim-visual-wanted` / `--vim-text-objects` / `--vim-virtual-column` / `--vim-replace` | VISUAL・`.`・欲しい列・`r` を共有する経路。1593 / 265 / 208 / 2232 / 424 / 460 checks すべて成功（どれも #111 と同数） |
| `ctest --test-dir build/issue112 --output-on-failure --no-tests=error` | 4 件すべて成功。`nib_unit` は 2.91 s（#111 の 2.74 s から fixture 144 件ぶん増えた） |
| `python eng/symbols.py --build-dir build/issue112 --require core application` | 矩形の純関数が core の外へロケール・時刻・OS・スレッドのシンボルを出さないこと。**2 libs / 0 violations**、新しい `__std_*` は出ていない（allowlist は変更なし） |
| `python eng/conformance.py` / `--build-dir build/issue112` | 8 つの新しい型の 1 ファイル 1 型（CNF-002）と、1320 fixture の生成整合（CNF-010）・正準形（CNF-011）。どちらも **0 violations** |
| `clang-format --dry-run --Werror`（変更した C++ 24 ファイル） | 整形。3 回 `clang-format -i` を掛けて成功 |
| `python eng/measure-speed.py --check --executable build/issue112-release/NeNeNib.exe` | **矩形の範囲の列が行数に比例する**ので QLT-014 を明示実行した（ADR 0021 の「差分が速さに関わるとき」）。Release を別に build し、5 本すべてを 5 回。**0 regression / 0 unmeasurable**：1 打鍵 **0.925 ms**（基準 0.906）・200 打鍵 2.563 ms（2.695）・起動 210.491 ms（191.488）・窓 32.221 ms（34.933）・16 MiB 265.646 ms（249.783）。基準値・許容（25 % / 下限 2 ms）は変更していない |

対象を限定した理由: 差分は core の新しい純関数 2 本（`vim_block_range` / `vim_replayed_block_range`）と 8 つの型、engine の矩形の枝、controller の効果 3 つと描画・クリップボードの分岐、窓の鍵の写し 1 か所、unit の対象 1 つ、fixture 144 件である。`VimMode` と `VimEffect` と `VimRegisterKind` が engine 全体に触るので unit 全体を 1 回回し、矩形の走査が行数に比例するので速さを明示実行した。設定・テーマ・利用者テーマ・Ex・パレット・adapters・`check.ps1 -Full` は差分の依存先でも呼び出し元でもないので実行していない（QLT-001 / QLT-012・ADR 0021）。**画面確認は未実施**（`NeNeNib.exe` は Debug と Release の両方を作った。Ctrl+V の写しと矩形の描画は実機でしか見えないので、hide の手元での確認が要る）。push / レビュー / 統合でも上記の成功結果を再利用する。Waivers: none。

**窓の Ctrl+V だけは unit で測れない**（`src/ui/win32` は unit の対象外・ARC-011）。`vim_block_key` は `core::VimMode` の閉じた switch で、INSERT だけ偽を返す 1 行の関数にしてある。矩形の鍵として送るかどうかは `EditorWindow` が `frame.vim_mode` を覚えた値で決め、通常モードと Vim の INSERT は既存の OS 貼付の表へ落ちる。

### 5-ag. 後ろ向きの VISUAL の引用符の対（Issue #111・ADR 0031 の補足・2026-09-22）

統合単位は [PR #116](https://github.com/hideyukiMORI/nene-nib/pull/116)（draft・ブランチ `feat/111-vim-quote-object-backward`）。以下の成功結果を文書追記・レビュー・統合でも再利用する。節記号は #112 の並行作業（5-af）と衝突しないよう 5-ag を取った。

base `75bf047`（origin/main）。ADR 0031 は新しく書かず「補足（2026-09-22・Issue #111）」を足し、**決定 4（VISUAL の両端）と決定 6（引用符）を実測に合わせて直した**。`vim_text_object_range` は 1 本のままで（ARC-001）、引用符の枝だけを `quote_outcome` に分け、対の選び方（畳んだ位置 / 前向き / 後ろ向きの 3 本）と VISUAL の置き方を同じ関数の中に閉じた。括弧の「もう 1 段外へ」は `paired_outcome` に残り、引用符は Vim と同じ「選択がちょうど内側なら引用符ごと」に置き換えた。新しい型は無い。engine・UI・IME・描画・保存形式・schema・依存・ゲートの閾値は変更していない。

`python out/issue111-oracle/probe.py`（618）/ `probe2.py`（27）を**実装の前**に実行し、固定 Vim 9.1 を **645 ケース**起動して規則を閉じた。証拠は `out/issue111-oracle/probe*.json` / `probe*.txt` と、645 ケースすべてを 1 つの模型で説明できることを確かめた `model.py`（`out/` は追跡外なので作業機にだけある）。Vim ソースは読んでいない。どのケースも命令の切れ目で区切って測り、選択の両端は `'<` `'>` で読んでいる。穴の正体は「奇数」でも「隙間」でもなく、**非空の選択では対の選び方そのものが向きで変わる**ことだった（caret の手前の引用符の行頭からの番号が奇数＝対の外なら、前向きは後ろの対・後ろ向きは手前の対へ渡る）。caret が引用符の上にあるとき・anchor がその場に残る条件・行をまたぐ選択の取消も同じ実測で閉じた。

`python out/issue111-oracle/add-fixtures.py --write` は候補 40 件を「命令の切れ目で区切った形」と「1 回の `:normal!` の形」の両方で測り、**1 件を機械が拒否**した（前向きの隙間から `i'` はビープして後続の鍵が消える）。`python eng/vim-oracle.py --regenerate --only text-object-quote-` は **73 measured / 1103 reused**（reuse ref `75bf047`。73 のうち 34 件は #93 が採った既存の `text-object-quote-*` で、再測定しても値は変わっていない）。`git diff` の削除行は metadata 2 行だけで、既存 1137 行は逐語再利用した。1176 件の入力 SHA-256 は `0b0756ba9406f285d2939252bd0effac7e2485b7f757b07a922bf657479df80a` → `5cae75d18203f861d928001a1e2b85be1526e60981a677bdf5f57a29db84b54e`。**初回の再生で 39 件すべてが実装と一致した**（期待値を直した fixture は無い）。取消はビープが後続の鍵を捨てて 1 回の `:normal!` に乗らないので、対象 unit の契約 5 件（`verify_vim_text_object_quote_pairs`）が engine の答えを固定する。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `. ./eng/toolchain.ps1` → `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug` → `cmake --build build` | 引用符の枝の書き換えを Debug / clang-tidy / ASan / UBSan で。初回は `readability-function-size` の入れ子 4 段で落ち、対の走査を `counted_quote_after` に分けて成功（閾値は触っていない） |
| `build/nib_tests.exe --vim-text-objects` | 対象。1912 → **2232 checks すべて成功**（新規 fixture 39 件＋契約 5 件ぶん）。範囲・取消・VISUAL の置換・向き・`.` の記録を確認 |
| `build/nib_tests.exe --vim-dot` | `ci"` などを鍵の列として再生する経路。**1593 checks 成功**（変更前と同数） |
| `build/nib_tests.exe`（引数なし） | 引用符の枝は VISUAL の選択と register を通って他の scope の fixture にも出るので、unit 全体を 1 回。11553 → **11873 checks** すべて成功 |
| `ctest --test-dir build --output-on-failure --no-tests=error` | CTest から見た既定実行。4 件すべて成功、`nib_unit` 2.47 s（#108 の 2.74 s から fixture 39 件を足して短縮側に振れたのは測定のばらつき） |
| `python eng/symbols.py --build-dir build --require core application` | 走査を増やした core が外へロケール・時刻・OS・スレッドのシンボルを出さないこと。**2 libs / 0 violations**、新しい `__std_*` は出ていない（allowlist は変更なし） |
| `python eng/conformance.py` / `--build-dir build` | 1 ファイル 1 型（CNF-002）と、1176 fixture の生成整合（CNF-010）・正準形（CNF-011）。どちらも **0 violations** |
| `clang-format --dry-run --Werror`（`src/core/VimTextObjectRange.cpp` / `tests/unit/NibTests.cpp`） | 変更した C++ の整形。指摘なし |

対象を限定した理由: 差分は core の 1 ファイルの引用符の枝と、unit の対象 1 つ、fixture 39 件である。引用符の範囲は VISUAL の選択・register・`.` を通って他の scope にも出るので unit 全体を 1 回回した。描画・ファイル・設定・テーマ・Ex・パレット・adapters・速さ・Release build・`check.ps1 -Full` は差分の依存先でも呼び出し元でもないので実行していない（QLT-001 / QLT-012・ADR 0021）。画面確認は**未実施**（差分は engine の意味論にしか出ない）。exe は作っていない。push / レビュー / 統合でも上記の成功結果を再利用する。Waivers: none。

FR-003 / ARC-001/004/007 / CPP-002/003/004/006/011/012 / QLT-001/008/012/013 / CNF-010/011 を自己レビュー。`optional` は `has_value` / `value` / `value_or` だけで読み、閉じた分岐（`VimTextObject` の switch）に `default` は無い。新しい型・`reinterpret_cast`・時刻・OS・スレッドは増やしていない。範囲関数は純関数のままで、選択の正本は `EditorState`。

### 5-ah. literal CR を oracle と文書の改行模型で保つ（Issue #85・ADR 0036・2026-09-22）

統合単位は [PR #120](https://github.com/hideyukiMORI/nene-nib/pull/120)（draft・ブランチ `feat/85-literal-cr-line-model`）。以下の成功結果を文書追記・レビュー・統合でも再利用する。節記号は #112（5-af）・#111（5-ag）と衝突しないよう 5-ah を取った。

base `b1be094`（origin/main・#112 の矩形 VISUAL と #106 の符号化の直しを取り込んだあとに rebase した）。ADR 0036 を**受理**にし、決定 4 に受け付けの条件を 1 つ足した。`TextBuffer` が `LineEnding` を 1 つ持ち（`from_utf8` が `detect_line_ending` で 1 度だけ判別・`insert` / `erase` が引き継ぐ）、`line_end` は `crlf` のときだけ `\n` の直前の `\r` を外す。`EditorState` は写しを捨てて `text_.line_ending()` を返すので、判別の経路は 1 本になった（`with_opened` の引数が 1 つ減る）。`replacement_text` とレジスタの `without_newline_carriage_returns` も同じ規則に従う。VISUAL（文字単位・行単位）の `r<CR>` は選んだ各文字を literal CR にし、NORMAL の `r<CR>`（行を割る）と矩形 VISUAL（未測・ADR 0035 の範囲のまま）は変えていない。新しい型は無い。UI・IME・描画・保存形式・schema・依存・ゲートの閾値は変更していない。

`python out/issue85-oracle/probe.py`（25 ケース × 2 形 ＋ 読み方だけ 2 件 = 52 起動）と `probe2.py`（9 ケース × 2 形 = 18 起動）を**実装の前**に実行した。`normal!` と `feedkeys(..., 'xt')` の 2 形はすべて一致した（後ろ向き VISUAL の 1 件だけが命令の切れ目の問題で食い違い、候補から外した）。証拠は `out/issue85-oracle/probe*.json`（`out/` は追跡外なので作業機にだけある）。Vim ソースは読まず、`:help 'fileformats'` と実測を根拠にした。

**実測で分かった 3 点**（詳細は ADR 0036 の補足）。(1) Vim は「全部の行が CR で終わる」ときだけ `dos` と読む。`a\r\nbc\ndef` は `unix` で CR を残す。(2) そのため既存の `-crlf` の fixture 10 件は **CRLF の fixture ではなく、`read_text()` の universal newline が CR を落としていたから偶然一致していた**。決定 4 の (b) で入力が受け付けられなくなるので、10 件は fixture から外して `verify_vim_crlf_documents`（同じ `verify_vim_fixture` で再生する engine の契約 10 件）へ移した。(3) `register_of` が CR を無条件に落としていたので、LF 文書の `yy` / `dd` / `x` のレジスタから literal CR が消えていた。決定 2 と同じ分岐にして直した。**期待値を直した fixture は 1 件も無い。**

`python eng/vim-oracle.py --regenerate` は測定境界（`run_vim` の読み方と `measure` の検査）が変わったので **1339 measured / 0 reused**（`measurement_sources_match` の規則どおり 1 回きりの全件再測定。**113 秒**。rebase の前に 1195 件で 1 度測っており、rebase 後の 1 回でまとめた）。`git diff origin/main -- tests/vim/VimFixtures.hpp` の削除行は **metadata 2 行と上記の 10 行だけ**で、#112 の矩形 144 行を含む既存 1310 行は逐語で同じ値だった。入力の SHA-256 は `cd5bf5811457f569ccd2c303abc0f444bc176f9de0e2132e78558f65b5faf7d5`（1320 件）→ `71bc134e72bf415249c59ad36940f6a5829f35213a108933ee64ce6e345c95e6`（1339 件）。**初回の再生で新規 29 件すべてが実装と一致した**（合わせたのは実装のほうで、レジスタの CR の分岐 1 か所）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `. ./eng/toolchain.ps1` → `cmake -S . -B build/issue85 -G Ninja -DCMAKE_BUILD_TYPE=Debug` → `cmake --build build/issue85` | `TextBuffer` の行の切り方と `r` の枝を Debug / clang-tidy / ASan / UBSan で。初回は `readability-function-cognitive-complexity` で落ち、`unsupported_replacement` と `replacement_body` に分けて成功（閾値は触っていない） |
| `build/issue85/nib_tests.exe`（引数なし） | 行の切り方は本文・キャレット・描画・レジスタ・ファイルのすべてを通るので unit 全体を 1 回。13235 checks すべて成功（`verify_line_ending_model` の lf / crlf × 「`\r` が行末」「`\r\r\n`」「単独の `\r`」の契約、`verify_vim_crlf_documents` の 10 件、fixture 1339 件を含む） |
| `build/issue85/nib_tests.exe --vim-replace` | 対象。**613 checks 成功**（`replace-char-` 37 ＋ `literal-cr-` 29 ＋ 共有境界 10） |
| `--vim-dot` / `--vim-visual-yank` / `--vim-visual-wanted` / `--vim-open-lines` / `--vim-line-jumps` | 外した 10 件が載っていた scope。1479 / 234 / 211 / 560 / 999 checks すべて成功 |
| `--vim-visual-block` | rebase で合流した #112 の矩形。`r` の分岐を共有するので確認した。**1105 checks 成功** |
| `ctest --test-dir build/issue85 --output-on-failure --no-tests=error` | CTest から見た既定実行。4 件すべて成功（`nib_unit` 2.5 s）。ADR 0010 の偽ポートを使うファイルの開閉と保存は `nib_adapter_tests` の **109 checks**（変更前と同数）と unit の `verify_controller_intents` で成功 |
| `python eng/symbols.py --build-dir build/issue85 --require core application` | `LineEnding` を持ち回るようになった core が外へロケール・時刻・OS・スレッドのシンボルを出さないこと。**2 libs / 0 violations**、新しい `__std_*` は出ていない（allowlist は変更なし） |
| `python eng/conformance.py` / `--build-dir build/issue85` | 1 ファイル 1 型（CNF-002）と、1339 fixture の生成整合（CNF-010）・正準形（CNF-011）。どちらも **0 violations** |
| `python -m unittest tests.conformance.test_vim_oracle tests.conformance.test_conformance` | oracle の読み方と受け付けの検査。**93 tests OK**（`run_vim` が literal CR を保つ正例、`check_literal_cr` の正例 3 と反例 3、Vim を起動する前に `measure` が断ること） |
| `cmake --build build/issue85-release` → `python eng/measure-speed.py --check --executable build/issue85-release/NeNeNib.exe` | `line_end` に分岐が 1 つ増えたので 1 回。**5 benches / 0 regressions**（1 打鍵 0.850 ms・窓が見えるまで 32.791 ms・16 MiB 269.188 ms）。基準値は変更していない |
| `clang-format --dry-run --Werror`（変更した C++ 7 ファイル） | 指摘なし |

対象を限定した理由: 差分は core の 2 ファイル（`TextBuffer` と `VimStep` の `r` とレジスタ）、application の 3 ファイル（改行の形の写しを捨てる）、oracle の読み方と検査、unit の契約、fixture の入れ替えである。行の切り方は本文・キャレット・描画・レジスタ・ファイルのすべての呼び出し元を持つので unit 全体と ctest を 1 回ずつ回し、`line_end` に分岐が 1 つ増えるので速さも 1 回測った。テーマ・Ex・パレット・設定・`check.ps1 -Full` は差分の依存先でも呼び出し元でもないので実行していない（QLT-001 / QLT-012・ADR 0021）。画面確認は**未実施**（`\r` の描画は変えていない）。push / レビュー / 統合でも上記の成功結果を再利用する。Waivers: none。

作業の途中で見つけた既存の失敗は #106 が閉じた。`test_pr_event_uses_the_record_validator` が、この機械の端末符号化（cp932）で `validate-git.ps1` が `git show` の UTF-8 を読めずに落ちることを **未変更の `87d8d71` / `8c76e25`（どちらも main の commit）を base にしても同じく落ちる**ことで確かめ、設計リナへ報告した。#106（main `b1be094`）を取り込んだあとに rebase してから `python -m unittest discover -s tests/conformance` を回し直し、**160 tests すべて成功**している。

FR-003 / FR-008 / ARC-001/003/004/005/007/009 / CPP-002/004/005/006/011/012 / QLT-001/008/012/013 / CNF-002/010/011 を自己レビュー。`optional` は `has_value` / `value` / `value_or` だけで読み、閉じた分岐（`LineEnding` / `VimMode` の switch）に `default` は無い。新しい型・`reinterpret_cast`・時刻・OS・スレッドは増やしていない。`TextBuffer` は不変のままで、改行の形の正本は本文 1 か所である。

### 5-ai. 検査の入出力を端末符号化から切り離す（Issue #106・2026-09-22）

統合単位は [PR #119](https://github.com/hideyukiMORI/nene-nib/pull/119)（draft・ブランチ `fix/106-cp932-utf8-io`）。以下の成功結果を文書追記・レビュー・統合でも再利用する。節記号は #85 の並行作業と衝突しないよう 5-ai を取った（5-ah と 5-x は空き）。

base `54b3a21`（origin/main）。**製品の C++・fixture・CMake・schema には触れていない。検査の閾値・違反文言・規則・`eng/git-conventions.py` も変えていない。** 直したのは子プロセスとの入出力の符号化だけで、置き場所は 2 か所である（ARC-001）。

**経路（誰が誰を呼び、どこで UTF-8 に固定されるか）**

- `python eng/test-conformance.py` → `test_verification_policy.py` の `run_tool` → 子（`python eng/git-conventions.py` / `pwsh eng/check.ps1` / `pwsh eng/validate-git.ps1`）… **ここ 1 か所**で読む側を `encoding="utf-8"` ＋ `errors="replace"` に、子の環境を `PYTHONUTF8=1` に固定する（#106）
- `pwsh eng/validate-git.ps1` → `git show -s --format=%B` の出力を受ける … **ここ 1 か所**で `[Console]::OutputEncoding` と `$OutputEncoding` を UTF-8 にする（#106。`$OutputEncoding` は逆向き＝子へ渡す側）
- `pwsh eng/validate-git.ps1` → `python eng/git-conventions.py` の日本語の違反文言を印字 … 同じ前置きの `$env:PYTHONUTF8 = '1'`（#94。今回は触らず、隣に入力側を足しただけ）
- `.githooks/commit-msg` → `eng/validate-commit-message.ps1` → `python eng/git-conventions.py <file>` … 文字列は渡さずファイル経由で、読み込みは `read_text(encoding="utf-8-sig")`（もとから固定・変更なし）
- `.github/workflows/check.yml`（UTF-8 runner）→ `pwsh ./eng/validate-git.ps1` … 同じ 1 本。runner では符号化が既に UTF-8 なので判定は変わらない

二重の固定にはならない。#94 は python の**出力**（`PYTHONUTF8`）を pwsh 側で、#106 は pwsh の**入力**（`[Console]::OutputEncoding`）を同じ前置きで、python の**読み取り**を `run_tool` で固定する。どれも同じ向きを 2 度設定していない。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `python eng/test-conformance.py`（cp932 console・`PYTHONUTF8=1`＝ `toolchain.ps1` を読んだ開発シェル相当） | 変更前は `test_full_gate_requires_explicit_scope_and_reason` の 4 subTest が ERROR（pwsh のエラー表示の cp932 の省略記号を厳密な UTF-8 で読んで reader thread が落ち `stderr` が `None`）。変更後 **157 tests すべて成功** |
| `python eng/test-conformance.py`（cp932 console・`PYTHONUTF8` なし） | 変更前は `test_record_cli_returns_failure` が ERROR（子の UTF-8 出力を locale の cp932 で読めず `stdout` が `None`）。変更後 **157 tests すべて成功** |
| `python eng/test-conformance.py`（UTF-8 console `chcp 65001`） / `python -X utf8 eng/test-conformance.py` | CI と同じ側の端末で判定が変わっていないこと。どちらも **157 tests すべて成功**（変更前と同数） |
| `pwsh -NoProfile -File eng/validate-git.ps1`（cp932 console・`GITHUB_EVENT_PATH` で base を `8f2c363~1` に固定＝日本語 subject の commit 11 本・文書 commit のあとは 13 本） | 変更前は `GIT-003: invalid commit 8f2c363c3495d84a01cba53ec79b633566393e3a`（mojibake で subject が 100 文字を超える）。変更後 **`Git conventions passed (11 new commit(s)).` 終了 0**（文書 commit のあとに再実行して 13 本でも終了 0） |
| `pwsh -NoProfile -File eng/validate-git.ps1`（cp932 console・既存の呼び方・`origin/main..HEAD`） | 変更前後とも終了 0 だが、`out/git/message.txt` の subject が変更前は `fix(eng): 蟄舌・繝ｭ繧ｻ繧ｹ縺ｮ蜈･蜃ｺ蜉帙ｒUTF-8縺ｫ…`、**変更後は `git show -s --format=%B` と一致**（壊れた入力が偶然 100 文字に収まっていただけで、検査に渡る本文は壊れていた） |
| `pwsh -NoProfile -File eng/validate-git.ps1`（UTF-8 console） | UTF-8 の端末でも同じ 1 本が通ること。終了 0 |
| `.githooks/commit-msg`（`eng/validate-commit-message.ps1` → `git-conventions.py`） / `.githooks/pre-commit`（`git diff --cached --check`） | 本 PR の 2 commit で自然に実行。どちらも成功 |

対象を限定した理由: 差分は conformance テスト 1 ファイルの `subprocess` の呼び方と、`eng/validate-git.ps1` の前置き 3 行である。製品の C++・リンク境界・fixture・CMake・速さの入力はどれも不変なので、build / `ctest` / `eng/symbols.py` / `eng/measure-speed.py` / Release build / `check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。`eng/conformance.py` は文書を変えたので実行する（CNF-006）。画面確認は不要（窓に出る差分が無い）。push / レビュー / 統合でも上記の成功結果を再利用する。Waivers: none。

QLT-001 / QLT-007 / QLT-012 / GIT-003 / CNF-006 を自己レビュー。新しい規則・新しい検査・新しい閾値は足していない。`tests/conformance/test_vim_oracle.py` の `subprocess` は Issue #85 が同じファイルを触っているので今回は寄せていない（`vim_oracle.subprocess.run` を patch する作りで端末符号化に依らない。残りとして申し送り）。
### 5-aj. 検索の当たりの強調（Issue #123・ADR 0037・2026-09-22）

統合単位は [PR #125](https://github.com/hideyukiMORI/nene-nib/pull/125)（draft・ブランチ `feat/123-search-highlight`）。以下の成功結果を文書追記・レビュー・統合でも再利用する。節記号は 5-ai までの並びの次を取った。

base `4c0414c`（設計リナが `ae460a6`（origin/main）の上に ADR 0037 を積んだ commit）。ADR 0037 を**受理**にし、補足を 5 点足した。`VimState` に閉じた 3 値 `VimSearchHighlight`（既定 `on`・施主決定 D17）が載り、`vim_resting_from` が `last_search` と同じように持ち越す。`vim_step` の検索の 3 つの入口（`searched_key` / `repeated_search` / `word_search`）だけが `suspended` を `on` へ戻す。`ExResult` に `std::optional<VimSearchHighlight>` が増え、`:set hlsearch` / `:set nohlsearch` / `:nohlsearch` / `:noh` が返して controller の Ex の写し 1 か所が `requested_highlight` で `VimState` へ置く。照合器から `vim_line_matches(line, pattern)` を公開し、`vim_search` の `first_match` / `last_match` をそれに寄せたので走査の規則は 1 本になった。application は `EditorFrame` を作るときに、Vim モードで強調が `on` で解析できる `last_search` があるときだけ、**見えている行だけ**を数えて `LineView` の `matches` / `current_match` を既存の `span_of` で作る。renderer は当たり → 選択 → 本文 → 現在の当たりの枠 → キャレットの順で、`palette.search` の面と `palette.accent` の 1 DIP の枠だけを使う（ui/win32 に色のリテラルは増えていない）。新しいトークン・新しい型（enum 1 つを除く）・保存 schema・依存・ゲートの閾値は変えていない。

実測は**実装の前**に行った。`python out/issue123-oracle/probe.py`（25 ケース）と `probe3.py`（4 ケース）が固定 Vim 9.1 の `v:hlsearch` の遷移を、`probe2.py`（14 ケース）が `searchcount()` の総数を測った。**決定 1 は 29 ケースすべて一致**し、実測で足りなかった 3 点（`off` のときの `:noh` を折る `requested_highlight`・長さ 0 の一致は数えるが塗らない・`aaaa` の `/aa` は 2 つ）を ADR の補足に書いた。強調そのものは Vim の報告に出ないので fixture にはできず（決定 8）、`--vim-search-highlight` の契約が正本である。`tests/vim/` の fixture は 1 件も増減していない。証拠は `out/issue123-oracle/probe*.json` / `probe*.txt`（`out/` は追跡外なので作業機にだけある）。Vim ソースは読まず、help と実測だけを根拠にした。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `. ./eng/toolchain.ps1` → `cmake -S . -B build/issue123 -G Ninja -DCMAKE_BUILD_TYPE=Debug` → `cmake --build build/issue123` | `VimState` / `ExResult` / `LineView` の形の変更と renderer の追加を Debug / clang-tidy / ASan / UBSan で。指摘なしで成功（閾値は触っていない） |
| `build/issue123/nib_tests.exe --vim-search-highlight` | 対象。**74 checks 成功**（3 値の遷移 21・`vim_line_matches` 11・resting と畳み 6・Ex と補完 12・フレームの当たり 8・見えている行と強調しない場合 7・桁と CRLF と長さ 0 の 9） |
| `build/issue123/nib_tests.exe --vim-search` | `vim_search` の行内走査を `vim_line_matches` に寄せたので、同じ答えであること。**1290 checks 成功**（この scope のテスト本文は変更していない） |
| `build/issue123/nib_tests.exe --ex-settings` / `--command-palette` | `ExResult` の形と補完候補の変更。**153 / 130 checks 成功**。`command_completions("")` の末尾が `set guifont=` から `set nohlsearch` になったので既存の期待値 1 行を直した（候補が 2 つ増えたことの直接の帰結） |
| `build/issue123/nib_tests.exe`（引数なし） | `VimState` / `ExResult` / `LineView` は Vim・Ex・描画・controller のすべてが通る形なので unit 全体を 1 回。**13309 checks すべて成功**（変更前 13235 ＋ 新規 74） |
| `ctest --test-dir build/issue123 --output-on-failure --no-tests=error` | CTest から見た既定実行。4 件すべて成功（`nib_unit` 3.5 s） |
| `python eng/symbols.py --build-dir build/issue123 --require core application` | `vim_line_matches` と強調の判定が core の外へロケール・時刻・OS・スレッドのシンボルを出さないこと。**2 libs / 0 violations**、新しい `__std_*` は出ていない（allowlist は変更なし） |
| `python eng/conformance.py` / `--build-dir build/issue123` | 1 ファイル 1 型（CNF-002・`VimSearchHighlight.hpp` が 1 つの enum）と fixture の生成整合（CNF-010 / CNF-011）。どちらも **0 violations** |
| `cmake --build build/issue123-release` → `python eng/measure-speed.py --check --executable build/issue123-release/NeNeNib.exe` | 1 フレームに走査が増えるので 1 回。**5 benches / 1 regression**: `startup-window-shown` が median 46.782 ms で上限 43.666 ms を超えた（1 打鍵 0.914 ms・16 MiB 284.805 ms・起動 232.974 ms はすべて許容内）。**本件が原因ではない**（下記）。基準値は変更していない |
| `python eng/measure-speed.py --check --executable build/release-main/NeNeNib.exe`（対照・変更前の Release exe・同じ機械で 1 分後） | **5 benches / 0 regressions**だが `startup-window-shown` は median 38.741 ms（最大 62.021 ms）で上限 43.666 ms に近く、こちらの試行にも外れ値がある。本件の実行の最小値 34.188 ms は対照の中央値より小さい |
| 走査の時間（ADR 0037 の決定 7・ゲートではない） | 対象 unit には時計を入れられない（`tests/unit/NibTests.cpp` 冒頭・ARC-007）ので、core の純関数だけを呼ぶ使い捨ての計測を scratchpad で 1 回。**見えている 60 行 × 1 行 2 一致で 1 フレームあたり 0.067〜0.073 ms**（Release・予算 0.9 ms のおよそ 8 %）。基準値には足していない |
| `clang-format --dry-run --Werror`（変更した C++ 13 ファイル） | 指摘なし |

`startup-window-shown` の退行を本件の原因としない根拠: このベンチが測るのは `window_shown` の目印までで、`D3D11CreateDevice`（内訳で 173.5 ms）より**手前**である。本件が足したのは `EditorController::frame()` の中の走査だけで、`search_pattern()` は Vim モードでなければ最初の比較で `nullopt` を返す（起動時は通常モードで `last_search` も無い）。同じ機械で 1 分後に変更前の exe を測ると 0 regression だが中央値 38.741 ms・最大 62.021 ms で、本件の 5 試行の最小値 34.188 ms はその中央値より小さい。つまりこの機械のこのベンチが今この時間帯に不安定で、判定が上限の前後を行き来している。**再試行で緑を引きに行くことはしていない**（QLT-012）。基準値・閾値・除外はいっさい触っていないので、静かな機械での 1 回の測り直しは Ready の前に設計リナが行う。

対象を限定した理由: 差分は core の 6 ファイル（enum 1 つ・`VimState` の 1 メンバー・`VimSearch` の走査の公開・`VimStep` の入口 3 か所・`ExResult` の 4 命令と候補 2 つ）、application の 3 ファイル（`LineView` の 2 メンバーと `EditorFrame` の作り方・Ex の写し 1 か所）、ui の 2 ファイル（塗りの経路の追加）である。`VimState` と `ExResult` と `LineView` は Vim・Ex・描画・controller のすべての呼び出し元を持つので unit 全体と ctest を 1 回ずつ回し、1 フレームの走査が増えるので速さも 1 回測った。テーマ・設定の保存・ファイル・IME・oracle の再生成は差分の依存先でも呼び出し元でもないので実行していない（QLT-001 / QLT-012・ADR 0021）。`check.ps1 -Full` は回していない。画面確認は**未実施**（hide の手元で行う。起動中の `build/release-main/NeNeNib.exe` PID 45480 には触れていない）。push / レビュー / 統合でも上記の成功結果を再利用する。Waivers: none。

FR-003 / ARC-001/003/004/007/010/011 / CPP-002/003/004/005/006/011/012 / QLT-001/008/012/013/014 / CNF-002/006 を自己レビュー。`optional` は `has_value` / `value` だけで読み、閉じた分岐（`VimSearchHighlight` の `switch`）に `default` は無い。`reinterpret_cast`・時刻・OS・スレッド・新しいトークン・色のリテラルは増やしていない。走査の規則は `vim_line_matches` の 1 本、桁の計算は `span_of` の 1 本のままである。

### 5-ak. 実機用 Release の作成と SHA の記録をスクリプトに（Issue #129・ADR 0038 の決定 5・2026-09-23）

統合単位は [PR #133](https://github.com/hideyukiMORI/nene-nib/pull/133)（draft・ブランチ `chore/129-build-release-script`）。以下の成功結果を文書追記・レビュー・統合でも再利用する。節記号は 5-aj の次を取った。

base `bfa91f0`（origin/main）。**製品の C++・CMake・fixture・保存 schema・ゲートの閾値・違反文言・規則には触れていない。** 足したのは `eng/build-release.ps1` 1 本と、その引数の検査の正例・反例（`tests/conformance/test_verification_policy.py` の `ReleaseBuildArguments`）、`docs/DEVELOPMENT_WORKFLOW.md` の 9 節の 1 段落だけである。2026-09-22 に同じ手順を 2 回モデルに踏ませた（`build/release-main` / `build/release-main-123`）ので、3 回目からスクリプトが正本になる（ADR 0038 の決定 5）。この経路は**ゲートに載せない**（QLT-013: ゲートに Release も display も要らない）。

**断る 3 つ（QLT-013・「この exe はどの ref のものか」を偽らない）**: ref を名指ししない呼び方・無い ref・dirty な作業ツリー。どれも `eng/toolchain.ps1` を dot-source する**前**に終了 1 で止まるので、断られた呼び方は cmake にも ninja にも届かない。起動はしない（起動は設計席が `Start-Process` で行う）。出力先は短い SHA で決まるので、起動中の `build/release-main-123/NeNeNib.exe`・ゲートの `build/`・`eng/measure-speed.py` の `build-release/` のどれとも衝突しない。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `pwsh -NoProfile -File eng/build-release.ps1 -Ref main` | 対象の正例（ref が HEAD と違う＝ worktree 経路）。終了 0。`build/release-bfa91f0/NeNeNib.exe`（978944 bytes・SHA-256 `DE4D3900…59AFC`）と `out/release/bfa91f0.json` ができ、configure 1.839 s / build 93.687 s / total 96.037 s。`build/worktree-bfa91f0` は終了時に消えて `git worktree list` は本体 1 つだけ、本体の作業ツリーは clean のまま |
| `pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` | もう 1 本の経路（ref が HEAD ＝ worktree を作らない）。終了 0・`builtFromWorktree: false`・configure 2.603 s / build 101.920 s |
| 手で作った `build/release-main-123/NeNeNib.exe`（main `f9e4704`）との照合 | 受け入れ条件「同じ手順で再現できる」。978944 bytes で**違うのは 2 バイトだけ**（offset `0x80`〜`0x81` ＝ PE の COFF ヘッダの `TimeDateStamp`。`0x6AB279E3` → `0x6AB29E23`）。残り 978942 バイトは完全一致。`-Ref main` と `-Ref HEAD` の 2 つの生成物どうしも同じ 2 バイトだけが違う（`bfa91f0` と `1473b2e` の差は docs と `eng/` だけなので、製品のバイト列が変わっていないことの確認にもなる） |
| `python -m unittest tests.conformance.test_verification_policy`（反例 3 通り） | 引数無し → `QLT-013: name the ref`、`-Ref no-such-ref` → `QLT-013: unknown ref`、untracked を 1 つ置いた作業ツリー → `QLT-013: the working tree is not clean (1 entries)`。3 つとも終了 1 で、fixture の `eng/toolchain.ps1` が投げる sentinel は**出ない**（＝何もせずに止まった） |
| 同（正例） | `-Ref HEAD` を clean な fixture リポジトリで呼ぶと sentinel `fixture-toolchain-stop` に届き、`QLT-013` は出ない（検査を通り抜けたことだけを見て、製品は build も起動もしない・#61 の約束） |
| `python eng/test-conformance.py` | conformance の自己テスト全体。**163 tests OK**（160 → 163・新規は `ReleaseBuildArguments` の 3 件） |
| `python eng/conformance.py` | 新しい `.ps1` が CNF-005（ゲート無効化の綴り）・CNF-008（Issue 番号の無い TODO）に触れないこと、文書の相対リンクと規則 ID（CNF-006）。**0 violations** |

対象を限定した理由: 差分は `eng/` の新しいスクリプト 1 本と conformance のテスト 1 ファイル、文書 4 か所である。製品の C++・リンク境界・fixture・CMake・速さの入力はどれも不変なので、build / `ctest` / `eng/symbols.py` / `eng/measure-speed.py` / `check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。文書を変えたので `eng/conformance.py` は実行する（CNF-006）。画面確認は不要（窓に出る差分が無い）。push / レビュー / 統合でも上記の成功結果を再利用する。Waivers: none。

ARC-001 / QLT-001 / QLT-012 / QLT-013 / CNF-005 / CNF-006 / CNF-008 / GIT-001〜004 を自己レビュー。新しい規則・新しいゲート・新しい閾値は足していない（このスクリプトはゲートではない）。Release の作り方の正本は `eng/build-release.ps1` の 1 か所で、文書はそれを指すだけである（ARC-001）。残るのは PE の `TimeDateStamp` が link 時刻であること（`/Brepro` は入れていない）と、ゲートに載っていないので壊れたことは次に使うときにしか分からないことの 2 点。

### 5-al. verify-window の PNG 保存と前後の画素比較（Issue #131・ADR 0038 の決定 5・2026-09-23）

base `f677a7e`（main）。**製品の C++・CMake・fixture・保存 schema・`eng/check.ps1`・`eng/conformance-rules.json` には触れていない。** 撮る経路は `eng/window_driver.py` の `capture(window)` 1 本に移し（画面 DC からの `BitBlt` のまま・ARC-001）、`verify-window.py` の `capture` はそれを呼んで大きさを確かめるだけの薄い口になった。既存の `.bmp` 出力は残す。PNG は `zlib` だけで書く・読む（依存ゼロ・DEVELOPMENT_WORKFLOW 6 節）。ゲートには載せない（QLT-013）。実行した exe は既存の Debug の `build/NeNeNib.exe`（2026-09-22 17:42 の build。以後の main の差分は docs と `eng/` だけ）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `python eng/verify-window.py --capture out/frames-131-all` | 撮る経路を driver へ移したことの退行（既存の全節）と `--capture`。**終了 0（1 回目で通過）**。PNG は **9 枚**（`look` `vim` `vimViewport` `typing` `caretShapes` `escape` `clickedCaret` `backspace` `scrolling`）、どれも 800×450・5559〜7103 bytes。別起動の節（`documents` `ime` `firstPaint`）は窓を閉じて終わるので PNG は撮らない（従来の `.bmp` は残る） |
| `python eng/verify-window.py --capture out/frames-131 --keys "ihello<Esc>"` | `--keys` の経路。終了 0。`before.png`（5559 bytes）・`after.png`（6205 bytes）ともに 800×450、`frames.json` の `body` は `{"x": 0, "y": 50, "w": 800, "h": 365}`・`dpi` 120。起動直後の窓は通常モードなので `ihello` は 6 文字の本文になる |
| `python eng/compare-frames.py out/frames-131/before.png out/frames-131/after.png --inside 0,50,800,365` | **終了 1・`inside: false`**。出力そのまま: `{"size": {"w": 800, "h": 450}, "differentPixels": 1441, "bounds": {"x": 29, "y": 23, "w": 581, "h": 418}, "insideOf": {"x": 0, "y": 50, "w": 800, "h": 365}, "inside": false}`。外接矩形が本文の外へ出たのは、タブの未保存の印「● 」（上端 y=23）とステータスバーの「行 1, 桁 7」（下端 y=440・本文は y=415 まで）が本文と一緒に変わるから（`after.png` を目で見て確認）。道具の誤りではなく受け入れ条件の矩形の取り方の問題なので、判断は設計席へ返す |
| `python eng/test-conformance.py` | **171 tests OK**（163 → 171・新規は `tests/conformance/test_frame_capture.py` の 8 件: PNG の往復・PNG でないファイルの拒否・比較の正例 2（内側・差分 0）と反例 2（はみ出し終了 1・大きさ違い終了 2）・鍵の記法の正例と反例 `<Foo>`） |
| `python eng/conformance.py` | 文書の相対リンクと規則 ID（CNF-006）ほか。**0 violations** |

対象を限定した理由: 差分は `eng/` の Python 3 本とテスト 1 ファイル、文書 2 か所である。製品の C++・リンク境界・fixture・CMake・速さの入力はどれも不変なので、build / `ctest` / `eng/symbols.py` / `eng/measure-speed.py` / `check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。Waivers: none。

**訂正（差し戻し 1 回目・2026-09-23）。** 設計席が受け入れ条件を「本文の内側」から「期待した領域の外に差分が無い」に直した（Issue #131 の設計席のコメント）。`ihello<Esc>` が変えるタブの未保存の印とステータスバーの「行 1, 桁 7」は製品の正しい振る舞いだからである。`frames.json` に `regions`（`title` はタブ帯 `{"x": 0, "y": 0, "w": 800, "h": 50}`・`body` `{"x": 0, "y": 50, "w": 800, "h": 365}`・`status` は `core::status_bar_layout` と同じ `{"x": 0, "y": 415, "w": 800, "h": 35}`。重ならず 50 + 365 + 35 = 450 で隙間なし）を書き、`compare-frames.py --regions` が領域ごとの差分と、どの領域にも入らない `outside` を数える（`--inside` は残す・両方あれば `--regions` が優先・閾値なし）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `python eng/verify-window.py --capture out/frames-131 --keys "ihello<Esc>"` | `regions` を書く経路。終了 0。ただし **4 回のうち 2 回は鍵が窓に届かず `after.png` が `before.png` と同じ（5559 bytes・8 秒の期限切れ）**で、同じ手で未変更の `a1f8e07` も 1 回走らせて届くことを確かめた（投函の経路の揺れで、今回の差分は撮った後の JSON だけ）。採った回は `after.png` 6205 bytes |
| `python eng/compare-frames.py out/frames-131/before.png out/frames-131/after.png --regions out/frames-131/frames.json` | **終了 0・`inside: true`**。出力そのまま: `{"size": {"w": 800, "h": 450}, "differentPixels": 1441, "bounds": {"x": 29, "y": 23, "w": 581, "h": 418}, "regions": {"title": {"differentPixels": 406, "bounds": {"x": 29, "y": 23, "w": 44, "h": 14}}, "body": {"differentPixels": 679, "bounds": {"x": 70, "y": 68, "w": 82, "h": 30}}, "status": {"differentPixels": 356, "bounds": {"x": 551, "y": 427, "w": 59, "h": 14}}}, "outside": {"differentPixels": 0, "bounds": null}, "inside": true}` |
| `python eng/test-conformance.py` | **173 tests OK**（171 → 173・`--regions` の正例（3 領域の内側だけ・終了 0）と反例（領域の外に 1 画素・終了 1・`outside.differentPixels == 1`）。既存 8 件は不変） |
| `python eng/conformance.py` | **0 violations** |

**訂正 2（差し戻し 2 回目・2026-09-23）。** 上の表の 2 行目のとおり、鍵が届かなかった回にも `--regions` は差分 0・`inside: true`・終了 0 を返し、「何も起きなかった」を「期待どおり」と言っていた。設計席の裁定で、道具は無変化を成功に見せない。`verify-window.py --keys` は `after` が `before` と画素まで同じなら `frames.json` に `"changed": false` を書き（画像は両方残す）、stdout に `keys did not change the window within 8 s` と出して**終了 1**、変わっていれば `"changed": true`。判定は `compare-frames.py` の純関数 `frames_changed`（画素列 2 つの比較）1 本で、`verify-window.py` はそれを読み込む（ARC-001）。`compare-frames.py --expect <region>[,<region>...]`（`--regions` と一緒にだけ）は名指しした領域ごとに `"expected": {"<region>": true/false}` を足し、差分 0 の領域があれば**終了 1**。`--expect` が無ければ従来どおり差分 0 は終了 0（同じ画面の比較は正当な使い方）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `python eng/verify-window.py --capture out/frames-131 --keys "ihello<Esc>"` | `changed` を書く経路。**1 回目で鍵が届き終了 0・`"changed": true`**。`before.png` 5559 bytes・`after.png` 6205 bytes。鍵が届かない回には当たらなかったので、`changed: false` と終了 1 は下の conformance の反例で見る |
| `python eng/compare-frames.py out/frames-131/before.png out/frames-131/after.png --regions out/frames-131/frames.json --expect body,title,status` | **終了 0**。出力そのまま: `{"size": {"w": 800, "h": 450}, "differentPixels": 1441, "bounds": {"x": 29, "y": 23, "w": 581, "h": 418}, "regions": {"title": {"differentPixels": 406, "bounds": {"x": 29, "y": 23, "w": 44, "h": 14}}, "body": {"differentPixels": 679, "bounds": {"x": 70, "y": 68, "w": 82, "h": 30}}, "status": {"differentPixels": 356, "bounds": {"x": 551, "y": 427, "w": 59, "h": 14}}}, "outside": {"differentPixels": 0, "bounds": null}, "inside": true, "expected": {"body": true, "title": true, "status": true}}` |
| `python eng/test-conformance.py` | **177 tests OK**（173 → 177・`--expect` の正例（期待した領域に差分・終了 0）と反例（`body` に差分 0・終了 1・`expected.body == false`）、`frames_changed` の正例と反例（同じ画素列は `false`）。既存は不変） |
| `python eng/conformance.py` | **0 violations** |

鍵が窓に届かない揺れ（差し戻し 1 回目の 4 回中 2 回）の原因は調べていない。既存の投函経路の flake で、別 Issue の候補として設計席へ返した。

### 5-am. 保護対象の差分と scope ごとの checks 数を記録するスクリプト（Issue #130・ADR 0038 の決定 5・2026-09-23）

base `e0c8298`（main）。**製品の C++・CMake・fixture・保存 schema・`eng/check.ps1`・`eng/conformance-rules.json` には触れていない。** 足したのは `eng/protected-diff.py` 1 本と、その純関数の正例・反例（`tests/conformance/test_protected_diff.py`）、`docs/DEVELOPMENT_WORKFLOW.md` 7 節の 1 行と `docs/PROJECT_LAYOUT.md` の道具一覧の 1 行だけである。PR の「保護対象は差分 0」を文ではなく実出力で書くための道具で、**ゲートには載せない**（QLT-010 / QLT-013）。fixture は `fixtures.json` を `name` をキーに要素で比べ（整形の差は差にならない）、`VimFixtures.hpp` の `-U0` の行を metadata（sha256 の行と配列の大きさの行の 2 種類）・削除・変更・追加に分ける。削除・変更があれば終了 1。他の保護対象 5 ファイル（`eng/perf-reference.json`・`eng/symbol-allowlist.json`・`eng/conformance-rules.json`・`src/adapters/win32/SettingsCodec.hpp` / `.cpp`）は有無だけを書いて落とさない。scope の一覧は各 ref の `tests/unit/NibTests.cpp` の `scopes` 配列を静的に読み、`--build` で `build/protected-<短い SHA>` に作った Debug の `nib_tests` を scope ごとに 1 回走らせる（`eng/build-release.ps1` と同じ `eng/toolchain.ps1` と一時 worktree の流儀・既存の同じ SHA の build は使い回す）。exe が無ければ `"checks": "未測"` で落とさない。無い ref だけ終了 2。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `python eng/protected-diff.py --base ae460a6 --head f9e4704 --allow --vim-search-highlight --build`（#125） | **終了 0**。`fixtures 1339 -> 1339 / metadata 0 / deleted 0 / changed 0 / added 0`・保護対象 5 ファイルはすべて `unchanged`・`--vim-search-highlight 新規 - -> 74 allowed`・`scopes 19 / same 18`。base と head の 2 本を build して 3 分 35 秒。`build/worktree-*` は終了時に消えて `git worktree list` は元の 2 つだけ |
| `python eng/protected-diff.py --base b1be094 --head b8e16ca --build`（#85） | **終了 1**（反例の実例）。`fixtures 1320 -> 1339 / metadata 2 / deleted 10 / changed 0 / added 29`。削除 10 件は `line-jump-G-crlf`（base L416）・`open-line-crlf-below`（L460）・`open-line-crlf-above`（L461）・`visual-wanted-crlf`（L502）・`visual-yank-crlf`（L518）・`replace-char-crlf`（L546）・`replace-char-visual-crlf`（L564）・`dot-crlf-change-word`（L660）・`dot-crlf-open-below`（L661）・`dot-crlf-remove-line`（L662）。scope は 18 のうち 12 が変化（`--vim-dot 1593 -> 1479`・`--vim-search 1375 -> 1290`・`--vim-text-objects 2232 -> 1992`・`--vim-replace 460 -> 613`・`--vim-visual-yank 265 -> 234`・`--vim-visual-wanted 208 -> 211`・`--vim-open-lines 573 -> 560`・`--vim-open-line-recovery 430 -> 417`・`--vim-character-search 621 -> 639`・`--vim-line-jumps 989 -> 999`・`--vim-virtual-column 424 -> 417`・`--vim-visual-block 1204 -> 1105`）で、`--allow` が無いのですべて FAIL と書く |
| `python eng/protected-diff.py --base 9e41f79 --head 220fe76 --build`（#99） | fixture は `fixtures 1055 -> 1090 / metadata 2 / deleted 0 / changed 0 / added 35`（metadata 2 行だけ）。scope は 16 のうち `--vim-text-objects 1623 -> 1912` が変化し、`--allow` を付けていないので**終了 1**（実測のまま。fixture を足した PR では、その fixture を回す scope の checks 数も増える） |
| `python eng/protected-diff.py --base nope` | 無い ref は終了 2 |
| `python eng/test-conformance.py` | **184 tests OK**（177 → 184・新規は `test_protected_diff.py` の 7 件: diff の行分類の正例（metadata 2 行と追加 1 件 → 削除・変更 0）と反例 2（削除 1 件は base の行番号つき・期待値の書き換えは変更）、`fixtures.json` の要素比較の反例（`keys` が変わった要素 1 つ）と正例（順序と整形と追加は差にならない）、HEAD の `scopes` 配列の静的パースが 19 件（`contracts` 配列を拾わない）、`--allow --vim-dot` の読み取り） |
| `python eng/conformance.py` | 文書の相対リンクと規則 ID（CNF-006）ほか。**0 violations** |

対象を限定した理由: 差分は `eng/` の Python 1 本とテスト 1 ファイル、文書 3 か所である。製品の C++・リンク境界・fixture・CMake・速さの入力はどれも不変なので、既定の `build/` の build / `ctest` / `eng/symbols.py` / `eng/measure-speed.py` / `check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。上の 3 例の `nib_tests` は過去の ref の Debug build で、今回の差分の検証ではなく道具の受け入れの実測である。Waivers: none。

ARC-001 / QLT-001 / QLT-010 / QLT-012 / QLT-013 / CNF-006 / GIT-001〜004 を自己レビュー。新しい規則・新しいゲート・新しい閾値は足していない。残るのは、scope の checks 数が fixture の件数も含むので fixture を足した PR（#99）でも「変化」になり `--allow` の名指しが要ること（これが道具の使い方。下の訂正）と、ゲートに載っていないので壊れたことは次に使うときにしか分からないことの 2 点。

**訂正（差し戻し 1 回目・2026-09-23）**: 設計席の裁定で 2 点を直した。(a) #99 の終了 1 は正しい。scope の checks 数には fixture の件数が入るので、fixture を足した PR は増えた scope を `--allow` で名指しする。Issue #130 のコメントの「終了 0」は `--allow --vim-text-objects` を付けた場合と読み替え、その形で撮り直した（上の終了 1 の記録はそのまま残す）。(b) `scopes` 配列の静的パースが base か head で 0 件なら、警告 1 行で通すのをやめて**終了 2**（無い ref と同じ「測れない」）にした。checks の比較が空のまま成功に見えるのを防ぐ（#131 の「無変化を成功に見せない」と同じ）。fixture と他の保護対象の照合はその前に終えて JSON に残し、`"scopes"` の代わりに `"error": "scopes not found in <ref>:tests/unit/NibTests.cpp"` を書き、stdout に `Protected: error …` の 1 行を出す。build はしない。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `python eng/protected-diff.py --base 9e41f79 --head 220fe76 --allow --vim-text-objects --base-tests build/protected-9e41f79/nib_tests.exe --head-tests build/protected-220fe76/nib_tests.exe`（#99・撮り直し） | **終了 0**。`fixtures 1055 -> 1090 / metadata 2 / deleted 0 / changed 0 / added 35`・保護対象 5 ファイルは `none`・`--vim-text-objects 変化 1623 -> 1912 allowed`（JSON は `"allowed": true, "fails": false`）・`scopes 16 / same 15 / 未測 0`。exe は前の席の `build/protected-*` をそのまま使い、作り直していない |
| `python eng/protected-diff.py --base <root commit 07ded54> --head HEAD` | **終了 2**（反例の実例）。root に `tests/unit/NibTests.cpp` が無く base の `scopes` が 0 件。JSON に `"error": "scopes not found in 07ded54…:tests/unit/NibTests.cpp"`、`"fixtures"` と `"protectedFiles"` は残り `"scopes"` は無い |
| `python eng/test-conformance.py` | **185 tests OK**（184 → 185・新規は `scopes` 配列の無い文字列で `parse_scopes` が 0 件を返し、純関数 `scopes_error` が「測れない」の理由 `scopes not found in HEAD:tests/unit/NibTests.cpp` を返す反例と、両方あれば `None` の正例を 1 件に） |
| `python eng/conformance.py` | **0 violations** |

### 5-an. transcript の usage を席ごとに集計するスクリプト（Issue #146・ADR 0038 の決定 5・ADR 0039 の決定 6・2026-09-23）

base `d4de8c2`（main）。**製品の C++・CMake・fixture・保存 schema・`eng/check.ps1`・`eng/conformance-rules.json` には触れていない。** 足したのは `eng/usage-report.py` 1 本と、その数え方の正例・反例（`tests/conformance/test_usage_report.py`）、`docs/PROJECT_LAYOUT.md` の道具一覧の 1 行と引き継ぎ 09-23 の「今日入った道具」の 1 行だけである。「計測して言う」（ADR 0039 決定 6）のための道具で、**ゲートには載せない**（QLT-010 / QLT-013）。`~/.claude/projects/<dir>/**.jsonl` を読むだけで、書くのは `out/usage/` だけ。本文（`message.content`）は読み出さず、鍵と数字だけを出す。費用の換算と週間枠の推定はしない。jsonl 1 本が 1 席（`isSidechain` か `agentId` があれば背景席 `agent-<id 先頭 8>`・無ければ本流でセッション id の先頭 8）、1 ターンは assistant の `message.id` 1 つ（streaming の断片は 1 つにまとめ、`output_tokens` は最終値・他は最初の値）、`<synthetic>` は除く、JSON でない行と `usage` の無い assistant 行は `skipped` に数える。`--since` / `--until` は `timestamp` のローカル日付（施主の機械では JST）で比べる。`seat_tokens` は input + cache_creation + output（Agent の完了通知の `subagent_tokens` の物差し）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `python eng/test-conformance.py` | **191 tests OK**（185 → 191・新規は `test_usage_report.py` の 6 件: 同じ `message.id` の 3 行が turns 1・output は最終値 364（先頭の 8 ではない）、`<synthetic>` を turns に入れない、`isSidechain` の有無で本流 `86c7704f` と背景席 `agent-ac78e56b` に分かれる、壊れた行と `usage` の無い行が `skipped` 2、`--since 2026-09-22` が UTC 09-21T16:31（JST 09-22 01:31）を含み UTC 09-21T14:00 を外す、範囲に行の無いファイルは席にならない） |
| `python eng/conformance.py` | **0 violations** |
| `python eng/usage-report.py --since 2026-09-22`（受け入れの実測） | **終了 0**。`seats 33 / skipped 0`。probe-146 の手計算と一致: 09-22 の `agent-a71e4bf5` が turns 217・max_context 430,079・cache_read 61,322,076、09-23 の `agent-ac78e56b` の seat_tokens 117,427（日報の「棚卸し #124 117K」）、`agent-adabc244` 127,216（「#131 実装 131K」）。`claude-opus-5-5` はセッション `4187e184` の背景席 7 本（下の表） |

| 席 | model | turns | max_context | cache_read | seat_tokens |
| --- | --- | ---: | ---: | ---: | ---: |
| `dc0aed1f` | claude-fable-5-1 | 94 | 353,077 | 19,693,711 | 472,892 |
| `agent-a71e4bf5` | claude-opus-5 | 217 | 430,079 | 61,322,076 | 621,041 |
| `agent-ac888d76` | claude-opus-5 | 125 | 478,674 | 40,751,179 | 630,659 |
| `agent-a8a557d1` | claude-opus-5 | 51 | 119,819 | 4,292,197 | 134,058 |
| `agent-a7477189` | claude-opus-5 | 56 | 226,434 | 7,457,534 | 256,651 |
| `86c7704f` | claude-fable-5-1 | 183 | 573,777 | 59,624,633 | 2,940,855 |
| `agent-a9088861` | claude-opus-5 | 233 | 567,597 | 85,121,857 | 630,584 |
| `agent-adb99db1` | claude-opus-5 | 95 | 254,626 | 17,201,791 | 248,618 |
| `agent-ac47c9f7` | claude-opus-5 | 161 | 372,745 | 38,633,609 | 390,186 |
| `agent-a45ec9f3` | claude-opus-5 | 112 | 314,779 | 24,297,911 | 563,084 |
| `agent-a43dd5a9` | claude-opus-5 | 85 | 186,037 | 10,569,553 | 173,264 |
| `agent-ae45540a` | claude-opus-5 | 174 | 380,419 | 43,420,845 | 447,446 |
| `agent-a44e7665` | claude-opus-5 | 201 | 545,439 | 66,982,468 | 642,571 |
| `agent-ac60a345` | claude-opus-5 | 85 | 308,543 | 18,224,862 | 306,340 |
| `agent-ad4c5623` | claude-opus-5 | 157 | 307,248 | 31,344,042 | 325,137 |
| `agent-ac78e56b` | claude-opus-5 | 53 | 135,036 | 4,771,019 | 117,427 |
| `agent-a2c3e8c5` | claude-opus-5 | 11 | 72,093 | 595,245 | 50,536 |
| `agent-a83bdda2` | claude-opus-5 | 106 | 272,237 | 18,360,832 | 429,763 |
| `agent-ae105c49` | claude-opus-5 | 49 | 165,736 | 5,737,619 | 295,396 |
| `4187e184` | claude-fable-5-1 | 106 | 298,297 | 18,402,128 | 398,689 |
| `agent-a7e8d0a4` | claude-sonnet-5 | 37 | 86,135 | 2,378,017 | 90,791 |
| `agent-ab448b4a` | claude-sonnet-5 | 25 | 91,982 | 1,656,941 | 97,034 |
| `agent-adabc244` | claude-opus-5-5 | 40 | 129,281 | 3,778,347 | 127,216 |
| `agent-aaa28a83` | claude-sonnet-5 | 12 | 117,118 | 882,581 | 102,569 |
| `agent-a3e05a2e` | claude-opus-5-5 | 22 | 82,004 | 1,397,616 | 61,411 |
| `agent-a86308ea` | claude-opus-5-5 | 19 | 81,631 | 1,218,772 | 60,525 |
| `agent-a24866e8` | claude-opus-5-5 | 29 | 110,490 | 2,310,226 | 163,985 |
| `agent-ad047984` | claude-opus-5-5 | 21 | 79,326 | 1,350,818 | 58,411 |
| `agent-a427f4b0` | claude-sonnet-5 | 24 | 119,311 | 2,002,684 | 135,976 |
| `agent-aae09915` | claude-sonnet-5 | 33 | 113,482 | 2,683,310 | 119,283 |
| `agent-a96a5b66` | claude-sonnet-5 | 29 | 89,244 | 1,920,315 | 66,612 |
| `agent-acc42210` | claude-opus-5-5 | 8 | 74,594 | 432,255 | 57,202 |
| `agent-add38e8d` | claude-opus-5-5 | 15 | 77,257 | 909,844 | 55,773 |

| model | turns | cache_read | output | seat_tokens |
| --- | ---: | ---: | ---: | ---: |
| claude-fable-5-1 | 383 | 97,720,472 | 520,466 | 3,812,436 |
| claude-opus-5 | 1,971 | 479,084,639 | 1,056,599 | 6,262,761 |
| claude-opus-5-5 | 154 | 11,397,878 | 36,897 | 584,523 |
| claude-sonnet-5 | 160 | 11,523,848 | 49,770 | 612,265 |

対象を限定した理由: 差分は `eng/` の Python 1 本とテスト 1 ファイル、文書 3 か所である。製品の C++・リンク境界・fixture・CMake・速さの入力はどれも不変なので、build / `ctest` / `eng/symbols.py` / `eng/measure-speed.py` / `check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。Waivers: none。

ARC-001 / QLT-001 / QLT-010 / QLT-012 / QLT-013 / CNF-006 / GIT-001〜004 を自己レビュー。新しい規則・新しいゲート・新しい閾値は足していない。残るのは、transcript の形（鍵名）は Claude Code の版で変わり得て、変わっても落ちずに turns 0 や `skipped` の増加として出ること（ゲートに載っていないので次に使うときに数字で気づく）と、実行中の席は途中までの数字になることの 2 点。

### 5-ao. 制御文字を `^X`・書式用文字を `<xxxx>` で描く（Issue #117・ADR 0040・2026-09-23）

base `d4de8c2`（main）の上に 2 commit（`0b0a8b9` core・`4dcc918` application と renderer）。core の `display_line`（命題 1）を `LineView.display` に載せ、renderer は `display.text` で layout を作る。桁 → 位置は無名名前空間の `displayed`（`SelectionSpan` の両端も同じ 1 本）で `display_position` を通してから UTF-16 にし、位置 → 桁は `HitTestPoint` の位置を `source_column` で戻す（`column_at` は `LineView` を受ける）。IME の差し込みも描画用の行の桁で探す。置き換えた文字は `muted` で `tint_runs`（面は塗らない）。`LineView.text` を layout に渡す経路は 0 件（`grep -n "layout_of(" src/ui/win32/Direct2DRenderer.cpp` は `display.text` と IME の `shown` だけ）。fixture・保存 schema・色トークン・Tab は不変。`eng/verify-window.py` に `--open`（`--keys` の起動でファイルを開く）を足した。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy 込み） | 成功・警告 0 |
| `nib_tests.exe --display-line` | **150 checks passed**（123 → 150・新規 27 は application の `verify_display_line_views`: `\r` `\x01` U+200B を含む LF 文書の行で `display.text` / `starts` が core の `display_line` と一致・置き換えは桁 1 3 5 だけ・`/beta` の当たりと現在の当たりとキャレットは本文の桁 8〜12 のまま・`0vll` の選択は本文の桁 [1, 4)・CRLF 文書は `^M` を描かない） |
| `nib_tests.exe --vim-search-highlight` | **74 checks passed**（前と同数。強調の桁は壊れていない） |
| `nib_tests.exe`（既定） | **13459 checks passed**（13432 → 13459・差は上の 27） |
| `python eng/protected-diff.py --base origin/main --allow --display-line --build` | **終了 0**。`dbe8333..4dcc918`・`fixtures 1339 -> 1339 / metadata 0 / deleted 0 / changed 0 / added 0`・保護対象 5 ファイルは `none`・`--display-line 新規 - -> 150 allowed`・`scopes 20 / same 19 / 未測 0` |
| `python eng/symbols.py --build-dir build --require core application` | 2 libraries, **0 violation(s)** |
| `python eng/conformance.py` | **0 violations** |
| `python eng/verify-window.py --capture out/frames-117 --open out/117-cr.txt --keys "x"` | 終了 0（`changed: true`）。`out/117-cr.txt` は `a\rb\n` と `\x01x` U+200B `y beta` の LF 文書。`before.png` で 1 行目が `a^Mb`、2 行目が `^Ax<200b>y beta` と描かれ、置き換えた文字は `muted` の字色 |

対象を限定した理由: 差分は application の表示値 1 欄・renderer の桁の変換・unit・verify-window の引数 1 つで、fixture・engine・保存・速さの入力に触れない。速さ・fixture の再生成・`check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。Waivers: none。

ARC-001 / ARC-004 / ARC-011 / CPP-002 / CPP-009 / CPP-011 / CPP-014 / QLT-001 / QLT-012 を自己レビュー。残るのは、ブロックのキャレットが `^M` の上では `^` の 1 文字ぶんの幅になること（Vim は 2 桁を覆う）と、IME の変換中の行では置き換えた文字を `muted` にしないこと（本文の字色のまま）の 2 点。

### 5-ap. NORMAL のブロックキャレットが置き換えた文字の全幅を覆う（Issue #151・ADR 0040 の決定 3・2026-09-23）

base `1b6b253`（main）の上に `33f4b7b`。`draw_block_caret` は `DisplayLine` を受け、幅を本文の桁の描画上の範囲（`displayed(column)` と `displayed(column + 1)` をそれぞれ UTF-16 にして `HitTestTextPosition` で引いた x の差）にする。桁の変換は #117 の `displayed` 1 本のまま。`^M` なら 2 文字、全角なら字幅、行末は両端が同じ位置になり今までどおり `block_minimum_dips` に畳む。INSERT の細いキャレット・選択・IME（#152）は不変。`eng/verify-window.py` に `--vim`（`--keys` の前に `verify_vim` と同じにトグルを押して Vim NORMAL へ入る）を足した。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy 込み） | 成功・警告 0 |
| `nib_tests.exe`（既定） | **13459 checks passed**（main と同数） |
| `python eng/protected-diff.py --base origin/main --build` | **終了 0**。`1b6b253..33f4b7b`・`fixtures 1339 -> 1339 / metadata 0 / deleted 0 / changed 0 / added 0`・保護対象 `none`・`scopes 20 / same 20 / 未測 0` |
| `python eng/conformance.py` | **0 violations** |
| `clang-format --dry-run --Werror`（renderer 2 ファイル）・`git diff --check` | 差分なし |
| `python eng/verify-window.py --open out/117-cr.txt --vim --capture out/frames-151 --keys "l"` | 終了 0（`changed: true`）。`before.png` はブロックが `a` の 1 文字、`after.png` はブロックが `^M` の 2 文字を覆う |

対象を限定した理由: 差分は renderer のブロックの幅と verify-window の引数 1 つで、core・application・fixture・保存・速さの入力に触れない。`eng/symbols.py`（core / application のリンク境界は不変）・速さ・`check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。Waivers: none。

ARC-001 / CPP-002 / CPP-009 / CPP-014 / QLT-001 / QLT-012 を自己レビュー。残るのは、INSERT の細いキャレット・選択・IME の変換中の行の置き換え文字（#152）。

### 5-aq. C1 制御文字を 4 桁の `hex` として `<85>` で描く（Issue #147・ADR 0034 / 0040 の補足・2026-09-23）

base `1b6b253`（main）の上に `72b451f`。`DisplayWidth` に `hex`（4 桁）を足し、表に `{0x0080, 0x009F, hex}` を 1 範囲足した（491 → 492 行・clang-format が 2 件ずつに詰め直すので表の後半全体が差分に出る）。`display_line` は `hex` を `<` と小文字 16 進 2 桁と `>` に置き換える。enum が増えて落ちる `switch` は 3 本（`VirtualColumn.cpp` の `cells_of` → 4・`DisplayLine.cpp` の `append_display` → `<xx>`・unit の `expected_cells` → 4）で、どれも `default` を書かずに枝を足した。Vim の実測（`strdisplaywidth("\x85") == 4`）は設計席が済ませた。fixture に C1 は無いので再生成していない。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy 込み・全 target） | 成功・警告 0 |
| `nib_tests.exe --vim-virtual-column` | **424 checks passed**（417 → 424・表の境界 U+0080 / U+0085 / U+009F / U+00A0 と `a\u0085x` の桁 2〜5・次の文字 6・逆引き） |
| `nib_tests.exe --display-line` | **189 checks passed**（150 → 189・C1 32 個が `<xx>` の 4 文字・`<85>` `<80>` `<9f>`・`a<85>x` の `starts` {0,1,5,6} と `is_replaced`・対応表の往復） |
| `nib_tests.exe`（既定） | **13505 checks passed**（13459 → 13505・差は上の 46） |
| `python eng/protected-diff.py --base origin/main --allow --vim-virtual-column --allow --display-line --build` | **終了 0**。`1b6b253..72b451f`・`fixtures 1339 -> 1339 / metadata 0 / deleted 0 / changed 0 / added 0`・`--display-line 150 -> 189 allowed`・`--vim-virtual-column 417 -> 424 allowed`・`scopes 20 / same 18 / 未測 0` |
| `python eng/symbols.py --build-dir build --require core application` | 2 libraries, **0 violation(s)** |
| `python eng/conformance.py` | **0 violation(s)** |

対象を限定した理由: 差分は core の表 1 範囲・enum 1 値・switch の枝 2 本と unit で、engine の経路・保存・速さの入力に触れない（C1 を含む行の桁だけが変わる）。速さ・fixture の再生成・verify-window・`check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。Waivers: none。

ARC-001 / ARC-003 / CPP-002 / CPP-011 / QLT-001 / QLT-010 / QLT-012 を自己レビュー。残るのは、表の出典の掃引（`'a' . nr2char(cp)` で測った probe2）と C1 の実測（`strdisplaywidth("\x85")`）で測り方が違うことで、C1 以外に同じ食い違いが無いかは測っていない。

### 5-ar. IME の変換中の行でも置き換えた文字を muted で描く（Issue #152・ADR 0040 の決定 4・ADR 0014・2026-09-23）

base `6466fcc`（main）の上に `9e25e3f`。変換中の行の layout は描画用の行に差し込み位置 `base`（UTF-16）で変換中の文字列を差し込んだもの。無名名前空間の `replaced_ranges(DisplayLine, inserted)` が `is_replaced` の桁の UTF-16 の範囲を作り、`base` 以降の範囲だけ変換中の文字列の UTF-16 長ぶん右へずらす。本文だけの行は長さ 0 の差し込みとして同じ関数を通す（経路は 1 本）。`draw_replaced` は範囲の列を受けて `muted` で `tint_runs` し、変換中の行では本文の描画の後・文節の前に呼ぶ。差し込み位置は桁の境目なので置き換えた文字の範囲をまたがず、IME の節（`ime`）とも重ならない。候補窓・変換中の文字列の色・INSERT の細いキャレットは不変。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy 込み） | 成功・警告 0 |
| `nib_tests.exe`（既定） | **13459 checks passed**（main と同数） |
| `python eng/protected-diff.py --base origin/main --build` | **終了 0**。`6466fcc..9e25e3f`・`fixtures 1339 -> 1339 / metadata 0 / deleted 0 / changed 0 / added 0`・保護対象 `none`・`scopes 20 / same 20 / 未測 0` |
| `python eng/conformance.py` | **0 violations** |
| `clang-format --dry-run --Werror`（renderer 2 ファイル）・`git diff --check` | 差分なし |
| 実機の IME（scratchpad の撮影スクリプトが `window_driver` の `start` / `send_keys` / `capture_png` を使う・`verify_ime` と同じに実鍵で `a` を 1 文字変換中） | `out/117-cr.txt` の 1 行目 `a^Mb` で、行末（`out/frames-152/end.png`）と `^M` の前（`before-cr.png`・ずらす側）の両方で `^M` の最も明るい画素が `(189, 176, 184)`（muted）、`a` `b` は `(238, 238, 236)`（text） |

対象を限定した理由: 差分は renderer の変換中の行の置き換え文字の塗りだけで、core・application・fixture・保存・速さの入力に触れない。`eng/symbols.py`・速さ・`check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。Waivers: none。

ARC-001 / CPP-002 / CPP-011 / CPP-014 / QLT-001 / QLT-012 を自己レビュー。残るのは INSERT の細いキャレットと選択の置き換え文字の幅。

### 5-as. incsearch の preview（Issue #148・ADR 0041・2026-09-23）

ブランチ `feat/148-incsearch`。命題 1（core・`87c15da`）が `vim_find_match` 1 本と `VimState::incsearch`（既定 on）・`:set (no)incsearch` を置き、命題 2（application）が preview を足した。節記号は main の 5-ao の次を取った（main への追いつきは統合のときに設計席が行う）。application の `SearchPreview { std::optional<core::TextPosition> match; ScrollState origin; }`（1 ファイル 1 型・ADR の型に `optional` を補った）を `EditorState` が持ち、検索の入力行が閉じると `with_command_input` が一緒に消す。`perform(VimOpenSearch)` が今のスクロールを `origin` に置き、`accept(CommandText)` / `accept(EditCommand)` の後の `update_search_preview()` 1 か所が、`origin` の先頭行へ戻してから `vim_find_match(text, pattern, VimMatchRequest{caret, direction, 1})` の当たりを置いて `follow_position`（`follow_caret` から切り出した同じ計算）で見せる。incsearch off・空・解析の失敗・不一致は当たり無しで報せも出さない。`search_pattern()` は入力中なら入力のパターンを返し、`current_match` は preview の当たりを含む一致になる。Esc と Enter は入力前の先頭行へ戻してから今までどおり動く（Enter の前に戻すのは Vim の `finish_incsearch_highlighting` と同じで、incsearch の on / off で着いた後の画面が一致する）。`ExResult.incsearch` を `VimState::incsearch` へ写す 1 行を hlsearch の隣に足した。renderer・core・fixture・保存 schema・閾値は変えていない。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan） | `EditorState` の欄と controller の経路。指摘なしで成功 |
| `build/nib_tests.exe --vim-search-incremental` | 対象。**108 checks 成功**（契約の expect 91: 打った途中の 12 入力 × 5・全一致と打ち足し・BS・Enter 9・画面の追従と Esc の巻き戻しと on / off の Enter の一致と窓の大きさ 9・後ろ向きと折り返し 5・`:set (no)incsearch` と off のときの既存の強調とモードの切替 8。残りは文書を開く手順の前提の expect） |
| `build/nib_tests.exe --vim-search` / `--vim-search-highlight` | 入力中は本文とキャレットが不変（`verify_vim_search_input` は変更なし）・確定後の強調。**1356 / 74 checks 成功** |
| `build/nib_tests.exe`（引数なし） | `EditorState` と controller はすべての経路が通る。**13483 checks 成功**（13375 ＋ 108） |
| `ctest --test-dir build` | 4 件すべて成功（fixture 1339 件の再生を含む） |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py` | **0 violation / 0 violation** |
| `python eng/protected-diff.py --base dbe8333 --allow --vim-search --allow --vim-search-incremental`（exe は `build/protected-*` を再利用） | **終了 0**。`fixtures 1339 -> 1339 / metadata 0 / deleted 0 / changed 0 / added 0`・保護対象は `none`・`--vim-search 1290 -> 1356`・`--vim-search-incremental 新規 108`・`scopes 20 / same 18`。`--base origin/main`（`1b6b253`）では #117 の `--display-line` が head に無く終了 1 になる（分岐元が #117 より前のため・追いつき後に解消） |
| `python eng/verify-window.py --capture out/frames-148` | `verify_vim` に incsearch の節を足した（`ialpha beta<Esc>0/be` → 面と枠が出る・Esc で消える・`u` で空に戻る）。**終了 0**・`incsearchPaintedTheFirstRow` / `escapeTookThePreviewAway` が true。`out/frames-148/vimIncsearch.png` |
| clang-format（変更した C++ 6 ファイル）・`git diff --check` | 指摘なし |

**訂正（rebase 後・base `de0eebd`・head `e79b9e1`）**: 設計席の差し戻し 1 回目で、入力中の全一致の面（`matches`）は `VimState::highlight` が on のときだけにした（Vim は `hlsearch` off なら今の当たりだけ光る）。`current_match` は `matches` と独立に preview の当たりから作る（`line_view` の 1 か所・`previewed_offset` を切り出した）。契約 2 つ（`:set nohlsearch` 後の `/be` は `matches` が空で `current_match` だけ・`:set hlsearch` で両方）を `--vim-search-incremental` に足した。rebase の衝突解消で `verify_display_line_scope` の閉じ括弧が落ちて `8801448` の `nib_tests` が組めなかったので戻した。上の表の数字は rebase 前のもので、rebase 後は次のとおり。

| 検査 | 実測（rebase 後） |
| --- | --- |
| `cmake --build build --clean-first`（Debug・clang-tidy・ASan・UBSan） | 81 / 81・警告 0 |
| `build/nib_tests.exe --vim-search-incremental` | **111 checks 成功**（108 ＋ 3・足した契約の関数 1 本の分） |
| `build/nib_tests.exe --vim-search` / `--vim-search-highlight` / `--display-line` | **1356 / 74 / 189 checks 成功** |
| `build/nib_tests.exe`（引数なし） | **13682 checks 成功** |
| `ctest --test-dir build` | 4 / 4 成功 |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py`（`--build-dir build` も） | **0 violation / 0 violation** |
| `python eng/protected-diff.py --base origin/main --allow --vim-search --allow --vim-search-incremental --build` | **終了 0**。`de0eebd..e79b9e1`・`fixtures 1339 -> 1339 / metadata 0 / deleted 0 / changed 0 / added 0`・保護対象は `none`・`--vim-search 1290 -> 1356`・`--vim-search-incremental 新規 111`・`scopes 21 / same 19 / 未測 0` |
| `python eng/verify-window.py --capture out/frames-148` | **終了 0**・`incsearchPaintedTheFirstRow` が true。`out/frames-148/vimIncsearch.png`（`/be` で「beta」の be に面と枠・キャレットは 行 1 桁 1） |
| clang-format（変更した C++ 2 ファイル）・`git diff --check` | 指摘なし |

対象を限定した理由: 差分は application の 5 ファイルと単体テスト・`eng/verify-window.py` の 1 節である。core・renderer・fixture・速さの入力は不変なので、fixture の再生成・`eng/measure-speed.py`・`check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。preview は 1 打鍵ごとに本文全体の検索と見えている行の照合を行うが、速さのゲートの `keystroke` は通常の入力なので測っていない（ADR 0041 の結果）。Waivers: none。

FR-003 / ARC-001/004/010 / CPP-002/003/004/005/011 / QLT-001/012 を自己レビュー。次の一致を求める経路は確定の鍵と preview で `vim_find_match` の 1 本、スクロールの追従は `follow_position` の 1 本。`optional` は `has_value` / `value` / `value_or` だけで読む。

### 5-at. 単体テストを scope ごとの翻訳単位に分ける（Issue #160・ADR 0042・2026-09-23）

`tests/unit/NibTests.cpp`（8224 行）を 37 ファイル（`.cpp` 27・`.hpp` 10）に分けた。移す手は scratchpad の 1 度きりのスクリプトで、実体（関数・型・定数）383 個のうち 382 個は本文が一字一句同じであることを同じスクリプトで照合した（残る 1 個は `vim_key_names` を `inline constexpr` にした宣言の変更）。`main` の本文も同じ。`NibTests.cpp` は `main`・`report`・`contracts` / `scopes` の 2 表で 137 行。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan） | 新しい翻訳単位とヘッダ。警告 0 |
| `build/nib_tests.exe`（引数なし）/ `--vim-search` / `--vim-dot` / `--display-line` | 呼ぶ順と中身の不変。**13682 / 1356 / 1479 / 189 checks 成功**（分ける前の `fac82f7` と同じ数） |
| `ctest --test-dir build` | 4 / 4 成功（fixture 1339 件の再生を含む） |
| `python eng/protected-diff.py --base origin/main --build`（`--allow` 無し） | **終了 0**。`3863fbe..c707911`・`fixtures 1339 -> 1339 / metadata 0 / deleted 0 / changed 0 / added 0`・保護対象は `none`・`scopes 21 / same 21 / 未測 0` |
| `python eng/conformance.py`（`--build-dir build` も） | **0 violation / 0 violation** |
| clang-format（`tests/unit` の全ファイル）・`git diff --check` | 指摘なし |
| `cmake --build build --target nib_tests --clean-first` の時間（1 回ずつ・core / application の再ビルドを含む） | 分ける前 87.8 s → 分けた後 94.0 s。翻訳単位ごとに製品ヘッダと clang-tidy を読み直すぶん増えた（ADR 0042 の結果の「速くなる見込み」はこの機械では外れた） |

対象を限定した理由: 差分は `tests/unit/`・`CMakeLists.txt` の `nib_tests` の行・ADR 0042 だけで、`src/` と `eng/` と fixture は変えていない。Release・速さ・symbols・`--regenerate` は実行していない（QLT-001 / QLT-012・ADR 0021）。`c707911` から `origin/main`（docs だけの `3863fbe`）へ rebase した後は、テストの木が同じなので上の結果を再利用した。Waivers: none。

CPP-008 / CPP-011 / ARC-001 / ARC-012 / QLT-001 / QLT-012 を自己レビュー。scope の入口・契約・既定実行の入口と、2 つのファイルから呼ばれる検証は `Scopes.hpp` の 1 本に宣言し（決定 2）、それ以外は各 `.cpp` の無名名前空間に閉じた。共有の足場のうち型はそれぞれのヘッダ（`Editing.hpp` と `Scripted*.hpp` 6 本・CPP-011）、関数は `TestSupport.hpp` / `VimTestSupport.hpp` に宣言した。

### 5-au. verify-window が撮る前に窓の面が自分のものかを確かめる（Issue #140・2026-09-23）

ブランチ `eng/140-keys-diagnosis`。5-al の「`--keys "ihello<Esc>"` が 4 回中 2 回、`after.png` が `before.png` と画素まで同じ」の揺れを 2 工程で扱った。診断の工程（`0bba56d`・rebase 前は `06b2105`）は `drive_keys` が `--measure` を付けて起動し、節目（`input_received` / `frame_presented`）を `frames.json` の `measure` に写すようにした。直しの工程は `eng/window_driver.py` に `assert_uncovered(window)` を 1 本置き、`capture` が毎回その後で撮るようにした。クライアント領域の中心と四隅の内側 8 物理画素の 5 点で `WindowFromPoint` → `GetAncestor(GA_ROOT)` が自分の hwnd でなければ `WindowCovered`（相手のクラス名・pid・hwnd・点）を投げる。判定は純関数 `cover_points` / `first_cover` に切った。`--keys` は `frames.json` に `"covered": true` と `coveredBy` を書き、`another window covers the capture: <class> pid <n>` を出して終了 1。節の実行（`verify_vim` などすべての `capture` と `snapshot`）も同じ関数を通り、覆われたら同じ 1 行を出して終了 1（`--capture` があれば `frames.json` に `covered` を書く）。成功の回の `frames.json` は `"covered": false` を持つ。待ち条件（固定 0.4 s）と `await_new_frame` の判定は変えていない。src/ は不変。

診断の 16 回（Debug・rebase 前の `06b2105`・直列 8 回と間を置かない 8 回）:

| 回 | changed | input_received | frame_presented | 初回 frame ms | 最初の鍵 ms | 最後の鍵 ms | 最後の frame ms |
|---|---|---|---|---|---|---|---|
| run-1 | true | 8 | 3 | 214.4 | 645.7 | 647.1 | 653.2 |
| run-2 | true | 8 | 3 | 202.3 | 616.8 | 618.4 | 621.2 |
| run-3 | true | 8 | 3 | 192.4 | 624.7 | 626.4 | 629.4 |
| run-4 | true | 8 | 3 | 201.5 | 637.6 | 639.0 | 644.3 |
| run-5 | true | 8 | 3 | 206.5 | 636.0 | 638.0 | 645.3 |
| run-6 | true | 8 | 3 | 275.3 | 698.6 | 700.7 | 705.1 |
| run-7 | true | 8 | 3 | 201.0 | 621.0 | 622.6 | 625.9 |
| run-8 | true | 8 | 3 | 199.8 | 617.9 | 619.3 | 622.1 |
| burst-1〜8 | 全 true | 全 8 | 全 3 | 184〜211 | 605〜640 | 607〜642 | 610〜645 |

7 鍵に対して `input_received` が 8 なのは、投函した `WM_KEYDOWN`(Esc) から `TranslateMessage` が `WM_CHAR`(0x1B) を作るため。16 回とも鍵は届いて描かれており、揺れは再現しなかった。5-al の失敗回の `after.png`（5559 bytes）は正常な `before.png` と同じ大きさで、「窓は撮れたが鍵の結果が写っていない」形である。

直しの実測（Debug・main `3863fbe` の src を組み直した `build/NeNeNib.exe`）:

| 検査 | 結果 |
| --- | --- |
| `python eng/verify-window.py --capture out/140-fix/run-N --keys "ihello<Esc>"` を直列 8 回（間に 2 秒） | **8 / 8 終了 0**。全回 `changed: true`・`covered: false`・`input_received` 8・`frame_presented` 3・初回 frame 172〜218 ms・最初の鍵 596〜649 ms・最後の frame 601〜654 ms |
| 重なりの反例: `--keys "<Esc>"`（本文を変えないので 8 秒のあいだ撮り続ける）を走らせ、2 秒後に 2 本目の NeNeNib を同じ位置に出して `raise_window` で `HWND_TOPMOST` にする | **終了 1**。`another window covers the capture: NeNeNib.Editor pid 43248`・`frames.json` は `"covered": true`・`coveredBy` の点は中心 `[400, 225]`・`after.png` は書かれない |
| `python eng/verify-window.py --capture out/140-fix/sections`（節の実行・IME の変換と候補窓を含む） | **終了 0**。すべての `capture` が確かめを通っても覆われた回は無い |
| `python -m unittest tests/conformance/test_frame_capture.py` | **19 件成功**（`CoveredCapture` 5 件: 全点が自分なら覆われていない・1 点でも他人なら覆われている・点に窓が無いのも覆われている・5 点の位置・小さな窓でも点は内側） |
| `python eng/conformance.py` | 0 violation |

反例の作り方の注: 依頼は「別の verify-window を同時に走らせる」だったが、2 本目の `start()` は `FindWindowW(WINDOW_CLASS)` が z 順で先に返す 1 本目の窓（`HWND_TOPMOST`）の pid が合わないまま 5 秒で `WindowUnavailable` になり、窓を重ねられなかった（`out/140-fix` の 1 回目の試み）。そこで 2 本目は同じクラスの窓を pid で探して `raise_window` する使い捨ての補助（scratchpad・リポジトリには入れない）で出した。つまり verify-window を 2 本同時に走らせても 2 本目は 1 本目が閉じるまで窓を掴めず、09-23 の揺れが「別の席の verify-window の窓」だったとは考えにくい。覆ったのが NeNeNib 以外の窓（別の最前面の窓や通知）だった可能性は残り、次に出た回は `coveredBy` のクラス名と pid で分かる。

`python eng/test-conformance.py` は 196 件中 1 件失敗した。`test_protected_diff.Scopes.test_the_scopes_array_of_head_is_read_without_the_contracts_array` が HEAD の scope 数を 19 と固定しているのに対し、main は 21 である。本件の変更を stash しても同じく落ちるので、main の既存の失敗で本件とは無関係である。

対象を限定した理由: 差分は `eng/window_driver.py`・`eng/verify-window.py`・conformance の単体テストだけで、製品のコードは変えていない。C++ のビルドは古い exe を組み直しただけで、ctest・fixture・速さのゲート・`check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。Waivers: none。

訂正（差し戻し 1 回目）: 直しの工程は `assert_uncovered`（`WindowFromPoint` → `GA_ROOT` で比べる）を足したが、`eng/measure-speed.py` と first paint の `ours` が使う `covered_by` は `WindowFromPoint` の生の hwnd を自分と比べたままで、「この点の面は自分か」に 2 本の計算があった（ARC-001）。`covered_by` は `cover_points(...)[0]` の中心を `root_at` で引き、`first_cover` で比べるようにした。返り値（`None` なら自分・文字列なら相手の題名か `no window at that point`）と呼び出し側の意味（measure-speed は印だけ・first paint の `ours`）は変えていない。

| 検査（出力は `out/140-return1/`） | 結果 |
| --- | --- |
| `python -m unittest tests.conformance.test_frame_capture` | **19 件成功** |
| `python eng/conformance.py` | 0 violation |
| `python eng/verify-window.py --capture out/140-return1 --keys "ihello<Esc>"`（1 回） | **終了 0**・`changed: true`・`covered: false` |
| `python eng/verify-window.py --capture out/140-return1-first`（節の実行 1 回） | **終了 1**。IME の節（本物の鍵盤入力・`covered_by` を通らない）で変換中の下線 0 画素・ink が打つ前と同じ 526 のまま落ち、first paint の節まで進まなかった |
| first paint の節だけを `verify_first_paint` で 1 回（scratchpad の補助） | **落ちた**。全サンプル `ours: false`・画素 `[0, 0, 0]`。起動した窓が前面を取れず、中心の面は Windows Terminal（`CASCADIA_HOSTING_WINDOW_CLASS`）だった |
| 同じ起動で中心を直接引く（scratchpad の補助） | 生の `WindowFromPoint` も Terminal の最上位窓 `0x702fa` を返した＝旧い規則でも「覆われている」。`raise_window` の後は `covered_by` が `None`。両方の枝が新しい計算で正しく答える |

first paint の節は前面を取れない実行環境（設計席の端末が前にある）で落ちており、本件の計算の違いではない（旧い規則でも同じ答え）。`ours: true` のまま通ることは、前面を空けた実行で確かめる必要が残る。

### 5-av. test_protected_diff が scope の件数を固定しない（Issue #165・2026-09-23）

`tests/conformance/test_protected_diff.py` の `Scopes` は `scopes` 表の件数を `19` と固定していて、#148 で 21 になった main では落ちていた。件数の固定を外し、`tests/unit/NibTests.cpp` の `std::array<std::pair<std::string_view, void (*)()>, N> scopes{{` の `N` を正規表現で読み、`parse_scopes` が読めた件数と重複の無い件数がどちらも `N` であること、`N >= 19`（過去の件数を下回らない）を確かめる形にした。scope が増えても落ちず、表の宣言と中身の食い違いは落ちる。

| 検査 | 本件の前（`2f01a45`） | 本件の後 |
| --- | --- | --- |
| `python -m unittest tests.conformance.test_protected_diff` | 8 件中 1 件失敗（`21 != 19`） | **8 件成功** |
| `python eng/test-conformance.py` | 191 件中 1 件失敗（同じ 1 件） | **191 件成功** |
| `python eng/conformance.py` | — | **0 violation** |

対象を限定した理由: 差分はテスト 1 本と本節だけで、`eng/protected-diff.py`・`src/`・`tests/unit/` は変えていない。ビルド・アプリの検証は実行していない（QLT-001 / QLT-012・ADR 0021）。依頼書と Issue の「196 件」はこの枝の実測では 191 件。Waivers: none。

QLT-001 / QLT-012 / CNF-001 を自己レビュー。件数の正本は `NibTests.cpp` の宣言 1 か所で、テストに第 2 の件数を持たない（ARC-012）。

### 5-aw. incsearch の Ctrl-G / Ctrl-T（Issue #168・ADR 0043・2026-09-23）

ブランチ `feat/168-incsearch-hop`（main `f0b453d` から）。core は `VimSearchPattern` に `std::optional<TextPosition> from`（比較も）を足し、`vim_step` の検索の鍵は起点があればそこから `vim_find_match` を引く（範囲の端は元のキャレット）。`last_search` と `.` の記録は起点を持たない（記録へ足すときに外す）。回数は `vim_search_count(const VimState&)`（engine の `resolved_count` をそのまま公開）で application が読む。application は `SearchPreview.from`・`SearchHop { core::VimSearchDirection relative; }`（`EditorIntent` の和型に 1 つ）・`accept(SearchHop)`・`update_search_preview()` は `from` から・`submit(SearchLine)` は `from` を鍵に載せる。ui/win32 は `press_command_control_key` の `G` / `T` を `SearchHop` に写す（Ex / 設定一覧では controller が何もしない）。renderer・fixture・oracle は不変。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan） | 検索の鍵の型と `EditorIntent` の網羅（`std::visit`）。警告 0 で成功 |
| `build/nib_tests.exe --vim-search-incremental` | 対象。**139 checks 成功**（111 ＋ 28・契約の関数 4 本: `/be` の Ctrl-G ×2 と Enter・全一致は不変・文字を入れない・`last_search` は起点無し / Ctrl-T の折り返しと Enter / `?be` の Ctrl-G は上へ / BS の編集後も起点が残る / `2/be` の preview は 2 件目で Ctrl-G は起点から 2 件先 / 不一致・Ex・noincsearch では何もしない / `d/be` ＋ hop と `.` の記録に起点が無い / 画面の追従と Esc の巻き戻し） |
| `build/nib_tests.exe --vim-search` / `--vim-search-highlight` | 確定の鍵と強調。**1356 / 74 checks 成功** |
| `build/nib_tests.exe`（引数なし） | **13710 checks 成功**（13682 ＋ 28） |
| `ctest --test-dir build` | 4 / 4 成功（fixture 1339 件の再生を含む） |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py`（`--build-dir build` も） | **0 violation / 0 violation** |
| `python eng/protected-diff.py --base origin/main --allow --vim-search-incremental --build` | **終了 0**。`f0b453d..7d8edd0`・`fixtures 1339 -> 1339 / metadata 0 / deleted 0 / changed 0 / added 0`・保護対象は `none`・`--vim-search-incremental 111 -> 139`・`scopes 21 / same 20 / 未測 0` |
| `python eng/verify-window.py --capture out/frames-168` | `verify_vim` の incsearch の節を `ialpha beta beta` にし、`/be` の後に Ctrl+G（`press_chord`）を足した。**終了 0**・`incsearchHop.status` が `confirmed`・`movedTheCurrentMatch` が true。`out/frames-168/vimIncsearch.png`（枠は 1 つ目の be）→ `out/frames-168/vimIncsearchHop.png`（枠は 2 つ目の be・キャレットは 行 1 桁 1 のまま） |
| clang-format（変更した C++ 10 ファイル）・`git diff --check` | 指摘なし |

対象を限定した理由: 差分は検索の鍵の型と `vim_step` の検索の鍵 1 か所・application の preview と意図・ui の Ctrl 鍵 2 つ・単体テスト・`eng/verify-window.py` の 1 節である。renderer・fixture・速さの入力（通常の打鍵）は不変なので、`--regenerate`・`eng/measure-speed.py`・Release・`check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。

FR-003 / ARC-001/010/011 / CPP-002/003/004/005/011 / QLT-001/012 を自己レビュー。次の一致を求める経路は `vim_step`・`update_search_preview`・`accept(SearchHop)` の 3 か所とも `vim_find_match` の 1 本（planned・レビュー事項）。回数は engine の `resolved_count` 1 本を `vim_search_count` で読む。`optional` は `has_value` / `value` / `value_or` だけで読む。

**訂正（差し戻し 1 回目・`4564f2b`）**: 上の表の「`?be` の Ctrl-G は上へ」は誤り。ADR 0043 決定 2（設計席が `4a9d17b` で直した）に合わせ、hop の向きは本文の順（Ctrl-G は下へ・Ctrl-T は上へ・`/` `?` に依らない）にした。起点は Ctrl-G / Ctrl-T で分けず、新しい当たり `M'` から検索の向きの逆へ同じ回数戻った当たり 1 本（`/` の Ctrl-G では今の当たり）。`SearchHop.relative` の意味は本文の順で、ui/win32 の写像（G → forward・T → backward）は不変。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug） | `accept(SearchHop)` と契約。警告 0 で成功 |
| `build/nib_tests.exe --vim-search-incremental` | **141 checks 成功**（139 ＋ 2・`?be` の Ctrl-G は下へ折り返して 行 1 桁 7 に着き Enter でそこへ / `?be` の Ctrl-T は上へ動き Enter でそこへ。`/be` の契約は向きが変わらないので不変） |
| `build/nib_tests.exe`（引数なし）・`ctest --test-dir build` | **13712 checks 成功**・4 / 4 成功 |
| `python eng/protected-diff.py --base origin/main --allow --vim-search-incremental --build` | **終了 0**。`f0b453d..4564f2b`・`fixtures 1339 -> 1339`・`--vim-search-incremental 111 -> 141`・`scopes 21 / same 20 / 未測 0` |
| `python eng/conformance.py`（`--build-dir build` も）・`python eng/symbols.py --build-dir build --require core application` | 0 violation / 0 violation / 0 violation |
| clang-format（変更した C++ 3 ファイル）・`git diff --check` | 指摘なし |

`eng/verify-window.py` は `/be` の Ctrl-G だけを撮っており向きが変わらないので、PNG は撮り直していない（前の結果を再利用・ADR 0021）。

### 5-ax. 本文の Tab を空白 8 個ぶんの tab stop で描く（Issue #175・ADR 0045・2026-09-23）

ブランチ `feat/175-tab-stops`（main `86ab4ba` から）。`Direct2DRenderer::create_body_formats`（本文の `TextFormat` を作る唯一の所・フォント名・サイズ・DPI の変更はすべてここを通る）が、作った本文の書式を `set_tab_stops` に渡す。`set_tab_stops` は同じ書式で `" "` の layout を作り `GetMetrics` の `widthIncludingTrailingWhitespace` × 8（`tab_stop_spaces`）を `SetIncrementalTabStop` に渡す。layout の作成・計測が失敗したときや幅が 0 以下のときは何もせず DirectWrite の既定のまま描く（CPP-005）。行番号の書式・UI の書式・core・application・`display_line` は不変。`eng/verify-window.py` は全節の実行に Tab の節 `verify_tab_stops` を足し（`a<Tab>b` / 空白 8 個 ＋ `b` / `<Tab>c` / `ab<Tab>c` の CRLF を起動引数で開き、各行のインクの右端の x を読んで 1 行目と 2 行目、3 行目と 4 行目が同じ画素であることを assert・`--capture` なら `tabStops.png`）、`--open <file> --capture <dir>` を `--keys` なしでも受けて `opened.png` を 1 枚撮るようにした。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan） | renderer の書式の作成。警告 0 で成功 |
| `python eng/verify-window.py --open out/tab-175.txt --capture out/frames-175` | 終了 0。`out/frames-175/opened.png` で `a<Tab>b` の `b` と空白 8 個の `b`、`<Tab>c` と `ab<Tab>c` の `c` が同じ x（DPI 120・800×450） |
| 同じ `--open` を main `86ab4ba` の Debug exe で（`--executable ../NeNeNib/build/NeNeNib.exe --capture out/frames-175-main`） | 負の対照。`out/frames-175-main/opened.png` では DirectWrite の既定の tab stop で `a<Tab>b` の `b` が空白 8 個の `b` より左に描かれ、揃わない |
| `python eng/verify-window.py --capture out/frames-175`（全節） | **終了 0**。既存の節はすべて通り、`documents.tabs.rightInk` が `[187, 187, 187, 187]`（4 行のインクの右端が同じ画素・DPI 120）。`out/frames-175/tabStops.png` |
| `build/nib_tests.exe`（引数なし）・`ctest --test-dir build -R nib_unit` | **13712 checks 成功**（main と同数）・成功 |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py`（`--build-dir build` も） | 0 violation / 0 violation / 0 violation |
| clang-format（renderer 2 ファイル）・`git diff --check` | 指摘なし |

対象を限定した理由: 差分は本文の書式を作る 1 か所への tab stop の設定と、`eng/verify-window.py` の 1 節・引数の組み合わせ 1 つである。core・application・fixture・保存・打鍵の経路に触れず、書式はフォントの変更時にだけ作り直すので速さの入力（起動・1 打鍵・16 MiB）の描画経路は不変。`--regenerate`・`eng/measure-speed.py`・Release・`check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。Waivers: none。

FR-003 / ARC-001 / CPP-005 / CPP-009 / CPP-017 / QLT-001 / QLT-012 を自己レビュー。tab stop の値は renderer の 1 か所で、ADR 0034 の仮想桁（core の `tab_stop = 8`）とは同じ数を別に持つ（`tabstop` を設定にするときに 1 経路へ畳む・ADR 0045 の決定 3）。残るのは、等幅でない guifont と、Tab の前に全角文字がある行（DirectWrite は画素で、Vim は桁で次の tab stop を決めるので、全角の字幅が空白 2 個ぶんでないフォントでは揃わない）。PNG の受理は planned（設計席が Read で見る）。

### 5-ay. add バッファを 64 KiB の chunk の列にする（Issue #174・ADR 0044・2026-09-23）

ブランチ `refactor/174-add-buffer-chunks`（main `86ab4ba` から）。core は `AddChunk.hpp`（`struct AddChunk { std::string bytes; }`）を足し、`Piece` に `chunk`（add の chunk の番号・original は 0）を足した。`TextBuffer` の `add_` は `std::vector<std::shared_ptr<AddChunk>>` と `add_fill_` になり、`replaced` の非空の分岐は `appended`（`bytes.size() == add_fill_` かつ `chunk_bytes = 64 KiB` の残りに収まるときだけその場で `append`、そうでなければ `max(chunk_bytes, text.size())` を `reserve` した新しい chunk）と `append_insertion`（同じ chunk の中で続くときだけ piece を伸ばす）を通る。`view_of` は `add_.at(piece.chunk)->bytes` から 1 本の `string_view` のまま（呼び出し側は不変）。コンストラクタの引数は 3 つにし、`add_` と `add_fill_` は `replaced` が組み立てた値へ入れる（引数 4 つの上限）。application・ui・eng・fixture は不変。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan） | `Piece` の欄と `TextBuffer` の構築 3 か所。警告 0 で成功 |
| `build/nib_tests.exe`（引数なし） | **13727 checks 成功**（13712 ＋ 15・契約の関数 3 本: 同じ値から 2 回分岐した `x…` / `y…` と元の値の不変・先端を知る値は piece 1 のままその場で伸び、先端を知らない値は piece 2 で新しい chunk / 70,000 回の 1 文字入力（1000 字ごとに改行）の本文・71 行・**piece_count 2**・chunk 境界を跨ぐ行 66 / 200 KiB の 1 回の挿入は **piece_count 3**、その直後の追記は **4**（大きい chunk は伸ばさない）/ `erase` → `insert` で戻した本文と erase 前の値の本文。増分 15 は `expect` 14 と `buffer_of` の 1。旧 core にこのテストを当てると同じ 13727 件で 3 件が落ちる＝件数は core に依らず、契約は旧実装を拒む） |
| `ctest --test-dir build` | 4 / 4 成功（fixture 1339 件の再生を含む） |
| `python eng/protected-diff.py --base origin/main --build`（`--allow` 無し） | **終了 0**。`86ab4ba..2a3dadb`・`fixtures 1339 -> 1339 / metadata 0 / deleted 0 / changed 0 / added 0`・保護対象は `none`・`scopes 21 / same 21 / 未測 0` |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py`（`--build-dir build` も） | **0 violation / 0 violation / 0 violation** |
| `pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` | `build/release-2a3dadb/NeNeNib.exe`（sha256 `1CA2FF4B…D1CF0F`・994304 bytes）。速さは設計席が測る |
| clang-format（変更した C++ 5 ファイル）・`git diff --check` | 指摘なし |

対象を限定した理由: 差分は core の `TextBuffer` の add の持ち方と `Piece` の 1 欄・`CoreTests.cpp` の契約である。本文の値・行索引・公開の関数は変えていないので、fixture の再生（ctest）と scope ごとの checks 数（protected-diff）で退行を見る。`--regenerate`・`eng/measure-speed.py`・`check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021。速さは ADR 0044 の決定 6 の後続と設計席）。

ARC-003/007 / CPP-001/002/004/011 / QLT-001/012 を自己レビュー。`PieceSource` の `switch` は `default` 無し（CPP-002）。`AddChunk` は 1 ファイル 1 型（CPP-011）で、`.cpp` の補助は自由関数と `using` 別名だけ。chunk の大きさは `constexpr`・時刻・スレッド・OS に触れない（ARC-003/007・symbols 0）。「追記がその場で行われ複製が起きない」は planned（レビュー事項・ADR 0044 の強制）。

設計席の速さの明示実行（QLT-014・差分が本文の経路に関わるので `--check` を Release で 4 回・2026-09-23）: #174 の exe（`build/release-2a3dadb`）は 1 回目 `startup-first-frame` 242 ms（上限 239）で 1 本、2 回目 245 ms と `startup-window-shown` 44.1 ms（上限 43.7）で 2 本落ち、3 回目は 214 ms / 39.0 ms で 0 regression。対照の今の main の exe（`build/release-3041c31`・同じ時間帯・別の席がビルド中）も 242 ms / 44.6 ms で同じ 2 本が落ちた。落ちた区間はどれも `device_created`（190 ms 前後・GPU ドライバ）と `window_shown` で、本文の経路より手前（`document_opened` は 0.2 ms で不変）。変更が触る打鍵の 2 本は 4 回とも基準内（`key-to-frame-single` 0.86〜1.06 ms・`burst-200` 3.0〜3.7 ms）。機械の雑音と判断して受理（ログは設計席の `out/174-speed*.log` と `out/174-control*.log`・記録は `out/speed/`）。

### 5-az. マクロ `q` `@`（Issue #176・ADR 0046・2026-09-23）

ブランチ `feat/176-vim-macros`（main `29ce210` へ rebase）。core は `VimMacroRegisters`（a〜z の鍵の列）・`VimMacroRecording`・`VimState` の `macros` / `macro_recording` / `last_macro`・`VimPrefix` の `q` / `at`・`VimAction` の `record_macro` / `replay_macro`（表 2 つに 1 行ずつ）・`VimStep.failure`（閉じた失敗の理由・`VimRepeatFailure` に `not_moved` / `not_found` / `refused`）・`VimEditorView.source`（`VimKeySource` の typed / replayed）・`vim_macro_stored`。録画は `vim_step` の出口の `macro_recorded` 1 か所が積む。application は `step_vim`（1 鍵の唯一の経路・失敗を返す）・`perform(VimReplay)` を鍵の列（`std::deque`）に積んで流す形にし、入れ子の再生は列の頭へ差し込み（深さ 100 で打ち切り）、失敗で残りを捨て、2 つ以上の編集は前後の本文の違いを覆う 1 つの Edit に畳む。`StoreVimMacro`（`EditorIntent` に 1 つ・`:let @a` に当たる）。oracle は `register` 欄（`let @a = "…"`）と、NORMAL の `q` を含む fixture の拒否（`canonical_fixture`・CNF-011）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan） | 鍵の表・`VimPrefix` / `EditorIntent` の網羅。警告 0 で成功 |
| `build/nib_tests.exe --vim-macro` | 対象。**280 checks 成功**（fixture 20 件の再生と契約 8 本: probe 2 節の 9 行の録画→再生・`q` / `@` の待ちと VISUAL・空のレジスタと `@@`・打った鍵だけを録る（`@a` `.` と確定した検索 1 鍵）・再生の中の `q` は無効・失敗で入れ子の外側まで捨てる・深さ 100 と報せ・大文字の追記） |
| `build/nib_tests.exe --vim-dot` | 失敗の打ち切りを `.` と共用。**1479 checks 成功**（不変） |
| `build/nib_tests.exe`（引数なし） | **13986 checks 成功**（13727 ＋ 259） |
| `ctest --test-dir build` | 4 / 4 成功（fixture 1359 件の再生を含む） |
| `python eng/vim-oracle.py --regenerate` × 2 | 1 回目 1359 measured（既存 1339 行は逐語不変・20 行を追加）、2 回目はバイト一致（差分 0） |
| `python eng/protected-diff.py --base origin/main --allow --vim-macro --build` | **終了 0**。`29ce210..e203ce2`・`fixtures 1339 -> 1359 / metadata 2 / deleted 0 / changed 0 / added 20`・保護対象は `none`・`--vim-macro 新規 - -> 280`・`scopes 22 / same 21 / 未測 0` |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py`（`--build-dir build` も） | **0 violation / 0 violation / 0 violation** |
| `python eng/test-conformance.py` | 200 tests 成功（`register` の正準の位置・`q` の拒否 5 件と検索の中の `q` は通る・`let` の行・生成行の末尾・CNF-011 の反例 `qaxq@a`） |
| clang-format（変更した C++ 23 ファイル）・`git diff --check` | 指摘なし |

対象を限定した理由: 差分は Vim の engine と controller の再生の経路・oracle の入力欄・単体テストである。renderer・ui/win32・速さの入力（通常の打鍵は `step_vim` を通るだけで経路は同じ）は不変なので、Release・`eng/measure-speed.py`・`verify-window`・`check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。

FR-003 / ARC-001/004/010 / CPP-002/004/005/011/012 / QLT-001/012 / CNF-010/011 を自己レビュー。`.` と `@` の再生は `perform(VimReplay)` の 1 本（planned・レビュー事項）。失敗の判定は `VimStep.failure` の 1 本で、`.` と `@` が同じ規則を使う。

### 5-ba. マクロの録画中の表示 `recording @a`（Issue #180・ADR 0046 の決定 8・2026-09-23）

ブランチ `feat/180-recording-indicator`（main `9198fea` から）。application の `EditorFrame` に `recording`（`std::optional<char>`）を 1 欄足し、`EditorController::recording_name` が core の `VimState::macro_recording` を読むだけで埋める（Vim モードのときだけ値を持つ・通常モードでは鍵が engine を通らないので出さない・追記の `qA` は Vim の `reg_recording` と同じく打った大文字）。renderer の `draw_recording` はモード表示の文字の幅（`widthIncludingTrailingWhitespace`）＋ 12 DIP の後ろから最初の状態項目の手前までに `recording @a` を `command_format_`（Cascadia・Vim のコマンド行と同じ等幅）と `muted` のトークンで書く。色のリテラルは無い（ADR 0008 決定 8）。core・fixture は不変。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan） | `EditorFrame` の集成体の初期化（`frame()` の 1 か所）と renderer。警告 0 で成功 |
| `build/nib_tests.exe --vim-macro` | 対象。**289 checks 成功**（280 ＋ 9・契約 2 本: `qa` で `'a'`・INSERT の中も残る・止める `q` で消える・`qA` は `'A'` / 通常モードでは出さず Vim へ戻ると同じ録画がまた見える。増分 9 は `expect` 7 と `applied` 2） |
| `build/nib_tests.exe`（引数なし） | **13995 checks 成功**（13986 ＋ 9） |
| `python eng/protected-diff.py --base origin/main --allow --vim-macro --build` | **終了 0**。`9198fea..f249c9a`・`fixtures 1359 -> 1359 / metadata 0 / deleted 0 / changed 0 / added 0`・保護対象は `none`・`--vim-macro 変化 280 -> 289 allowed`・`scopes 22 / same 21 / 未測 0` |
| `python eng/verify-window.py --capture out/frames-180` | **終了 0**。`verify_vim` の `recordingChangedTheStatusBar: true`（`qa` の後はモード表示から最初の状態項目までの箱の画素が NORMAL と違う）・`stoppingTookTheRecordingAway: true`（`q` の後は NORMAL と画素一致）。`out/frames-180/vimRecording.png` に `NORMAL  recording @a` |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py`（`--build-dir build` も） | **0 violation / 0 violation / 0 violation** |
| clang-format（変更した C++ 6 ファイル）・`git diff --check`・`eng/validate-git.ps1` | 指摘なし |

対象を限定した理由: 差分は application の表示値 1 欄・renderer の 1 関数・verify-window の 1 節・単体の契約である。core・fixture・打鍵の経路は不変で、描く文字は録画中だけ増えるので、`--regenerate`・`eng/measure-speed.py`・`check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。

FR-003 / FR-004 / ARC-001/011 / CPP-004/009/011 / QLT-001/012 を自己レビュー。表示値は `frame()` の 1 経路（ARC-001）、`optional` は `value()` で読む（CPP-004）、`reinterpret_cast` 無し（CPP-009）。

### 5-bb. 16 MiB を開いてから 200 打鍵するベンチ（Issue #179・ADR 0044 決定 6・2026-09-23）

ブランチ `chore/179-keys-16mib-bench`（main `9198fea` から）。`eng/measure-speed.py` の `BENCHES` に 6 本目 `key-to-frame-burst-200-16mib` を足した。`bench_keys` / `keys_trial` は開く本文（`document`）を任意で受け、`open-large-file-16mib` と同じ `large.txt` を起動引数に渡して同じ試行（暖機 1 打鍵・1 打鍵・200 打鍵の post・未保存の確認に「いいえ」）を回す。値にするのは burst だけ（`keys_values`）。`measure` は `bench_large_file` の後にこの試行を 1 回回す。`--bench` の選択肢・`--check` の比較・`--adopt --bench` の書き込み・`describe` はどれも `BENCHES` の 1 表を読むので、6 本目もそのまま同じ経路に乗る。基準値の無い機械・ベンチは今のとおり判定しない（`compare` の `against is None`）。`eng/perf-reference.json` は `benches` に説明を 1 行足しただけで、基準値は変えていない（実機の値は設計席が `--adopt --bench key-to-frame-burst-200-16mib` で記録する・CI の指紋は ADR 0016 の手順で後から）。src・製品は不変。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `python eng/test-conformance.py` | **206 tests 成功**（200 ＋ 6: 表の正例 6 本・`perf-reference.json` の説明が表と同じ並び（`eng/prove-gates.py` の QLT-014 の証明はこの説明の鍵から値を組む）・6 本目の基準値の無い機械は残り 5 本で判定・`adopt_one` は 6 本目だけを書き他の値と `recordedAt` を動かさない・16 MiB の試行は burst だけを値にし、欠けたときは 6 本目の名前で報せる） |
| `python eng/measure-speed.py --help` | 終了 0。`--bench` の選択肢に 6 本目 |
| `python eng/measure-speed.py --record --bench key-to-frame-burst-200-16mib --repetitions 1 --executable ../NeNeNib/build/NeNeNib.exe`（main `9198fea` の Debug の exe・src は不変） | 終了 0・例外なし・6 本すべてが記録に載る。Debug（ASan）では 200 打鍵の到着幅が 50 ms を越え、`key-to-frame-burst-200` と同じく 6 本目も 3 回とも測り直して missing（経路の確認だけで値は見ない・判定は Release で設計席） |
| `python eng/protected-diff.py --base origin/main` | 終了 0。`fixtures 1359 -> 1359`・保護対象は `eng/perf-reference.json`（記録のみ・ADR 0044 決定 6 が根拠） |
| `git diff --check` | 指摘なし |

対象を限定した理由: 差分は速さのゲートの道具とその基準値の説明・conformance だけである。製品のコードは変えていないので、ビルド・ctest・`--check` / `--adopt`（この机では別の席がビルド中で雑音が乗る）は実行していない（QLT-001 / QLT-012・ADR 0021）。受け入れ条件の「`--check` が 6 本を測り 0 regression」と基準値の記録は設計席の実機で行う。

QLT-014 / ADR 0011 / 0016 / 0044 / QLT-001/012 を自己レビュー。ベンチ名は `BENCHES` の 1 表で、6 本目のための第 2 の選択・比較・書き込みの経路は作っていない（ARC-001）。

#### 訂正（差し戻し 1 回目・2026-09-23）

設計席が Release（`build/release-37a70ca`）で `--check` を回すと、6 本目は 5 試行 × 3 回の 15 回すべてが「the 200 keystrokes reached the window over N ms, not together」（432〜1758 ms）で測り直しになり、`missing 5 of 5` で測れなかった。`post_together` は窓のスレッドを `SuspendThread` で止めてから 200 本を積んで再開するので、到着は一斉である。`input_received` の幅は鍵を読む時間そのもので、16 MiB の本文では 200 鍵が 50 ms を必ず越える。上の表の「Debug（ASan）では到着幅が 50 ms を越え…missing（期待どおり）」は、この幅を poster の遅れと読んだ誤りである。

- `burst_failure(burst, span, delivered, name)`: 判定の関数は 1 本のまま、ベンチ名を受ける。`BURST_SPAN_LIMIT_MS` を当てるのは `key-to-frame-burst-200`（空の文書）だけで、16 MiB では `delivered < 202` と frame が無いことだけが欠測の理由になる。
- 到着の幅は記録（`out/speed/*.json`）の `breakdown` に、起動の内訳と同じ形（`medianMs` / `minimumMs` / `maximumMs`）で `key-to-frame-burst-200-16mib.keysArrivalSpan` として残す（有効な試行だけ・`keys_parts`）。値の本体は burst のままである。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `python eng/test-conformance.py` | **208 tests 成功**（206 ＋ 2: 16 MiB では到着幅 80 ms でも値になり幅が記録の内訳に載る正例・空の文書では同じ幅が 3 回とも「not together」で missing のままの反例） |
| `git diff --check` | 指摘なし |

計測（`--check` 6 本・`--adopt --bench key-to-frame-burst-200-16mib`・#174 の前後）は設計席が Release で行うので、この工程では実行していない（QLT-001 / QLT-012・ADR 0021）。

設計席の計測（2026-09-23・Release `build/release-37a70ca`・実機 `bc8a356f37c68491`・差し戻し 1 回目の後）: `--check` は 6 本で 0 regression（`key-to-frame-burst-200-16mib` 447.5 ms・min 431.9 / max 478.6・到着幅の中央値 444.0 ms・既存 5 本は基準内）。`--adopt --bench key-to-frame-burst-200-16mib` で基準値 427.7 ms（5 回の中央値・min 410.6 / max 519.5）を `eng/perf-reference.json` に記録（他の値と `recordedAt` は不変・保護対象の変更の根拠は ADR 0044 決定 6）。#174 の前後: 前の exe（main `3041c31`）は同じベンチで 513.6 ms（min 429.7 / max 783.6）、後（本枝・本文は `9198fea` と同じ）は 427.7〜447.5 ms。16 MiB の 1 打鍵は約 2.2 ms で、空の文書の 0.9 ms との差の経路は別 Issue（Sonnet の probe 中）。ログは `out/179-check-2.log`・`out/179-adopt.log`・`out/179-before174.log`・記録は `out/speed/`。

### 5-bc. 改行の索引をバッファごとに 1 本共有し piece は窓だけを持つ（Issue #184・ADR 0047・2026-09-23）

ブランチ `refactor/184-shared-newline-index`（main `376affd` から）。`Piece` は `newlines`（`std::vector<Offset>`）を捨てて `newline_begin` / `newline_end`（索引の中の添字・`std::size_t`）を持つ。original の索引は `TextBuffer::original_newlines_`（`std::shared_ptr<const std::vector<Offset>>`・`from_utf8` の 1 回の走査）、add の索引は `AddChunk::newlines`（chunk の中の位置・`appended` が `bytes.append` と同じ「先端」の分岐で足した `text` だけを走査して伸ばす）。索引の本体は `index_of(piece)` 1 本で引く。`clipped` は窓の中を `lower_bound` 2 回（本文を読まない）、`append_insertion` は `newline_end` を進めるだけ、`newline_offset` / `newlines_before` は窓の中を引く。コンストラクタは引数 4 つ（`original_newlines_` を足した）。`Piece{...}` の構築は `from_utf8`・`appended`・`clipped` の 3 か所のまま。application・ui・eng・fixture・`eng/perf-reference.json` は不変。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan） | `Piece` の欄・`TextBuffer` の構築・索引の引き方。警告 0 で成功 |
| `build/nib_tests.exe`（引数なし） | **14009 checks 成功**（13995 ＋ 14・契約の関数 3 本: 200,000 行を `from_utf8` してから先頭・中央・末尾に 200 回ずつ `x` と改行を入れ 3 回に 1 回は続く 1 文字ごと消す 600 回の編集で、毎回の `line_count` と編集した行の `line_start` / `line_end` / `line_text`・40 回ごとに 1009 行おきの行の先頭・行番号（`position_of`）・文字列・最後に本文が素朴な `std::string` と一致 / 70,000 回の 1 文字入力（100 字ごとに改行）の全行・chunk 境界を跨いで消した後・最初の chunk の中へ入れた後の全行 / 同じ値から先端で伸ばした・先端の後ろで分岐した・piece を割った 3 つの値と元の値の全行、伸ばした piece と共有の piece を切った後の全行。増分 14 は `expect` 13 と `buffer_of` の 1） |
| `ctest --test-dir build` | 4 / 4 成功（fixture 1359 件の再生を含む） |
| `python eng/protected-diff.py --base origin/main --build`（`--allow` 無し） | **終了 0**。`376affd..2ce95b6`・`fixtures 1359 -> 1359 / metadata 0 / deleted 0 / changed 0 / added 0`・保護対象は `none`・`scopes 22 / same 22 / 未測 0` |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py`（`--build-dir build` も） | **0 violation / 0 violation / 0 violation** |
| `pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` | `build/release-2ce95b6/NeNeNib.exe`（sha256 `788922F4…0B86F4D`・1010688 bytes）。速さは設計席が測る |
| clang-format（変更した C++ 5 ファイル）・`git diff --check` | 指摘なし |

対象を限定した理由: 差分は core の `TextBuffer` の改行の索引の持ち方と `Piece` / `AddChunk` の欄・`CoreTests.cpp` の契約である。本文の値・行番号・公開の関数は変えていないので、fixture の再生（ctest）と scope ごとの checks 数（protected-diff）で退行を見る。`--regenerate`・`eng/measure-speed.py`・`check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021。速さと `--adopt --bench key-to-frame-burst-200-16mib` は ADR 0047 の決定 7 で設計席）。

ARC-003/007 / CPP-001/002/004/011 / QLT-001/012 を自己レビュー。窓の添字は `std::size_t`・バイト位置は `Offset` のまま混ぜない（CPP-001）。`index_of` の `PieceSource` の `switch` は `default` 無し（CPP-002）。`.cpp` の補助は自由関数と `using Window = std::span<const Offset>` だけ（CPP-011）。時刻・スレッド・OS に触れない（ARC-003/007・symbols 0）。「本文を走査し直さない」は基準値を締めるまで契約（正しさだけ）で守る（ADR 0047 の強制）。

設計席の計測（2026-09-23・Release `build/release-2ce95b6`・実機 `bc8a356f37c68491`）: `--check` 6 本を 2 回。`key-to-frame-burst-200-16mib` は 9.2 ms / 7.8 ms（基準値 427.7 ms・#179 の 447 ms から約 50 分の 1）。同じ時間帯の対照（main 相当の exe `build/release-3041c31`）は 461.7 ms。落ちたのは起動の区間だけ（`startup-first-frame` 249.5 / 267.7 ms・対照も 260.1 ms で同じく落ちる・`device_created` が 190 ms 前後）で、本文の経路より手前なので機械の雑音と判断。打鍵の 2 本は基準内。`document_opened` は 64〜71 ms（対照 56 ms・索引を 1 回作る）。ADR 0047 決定 7 のとおり `--adopt --bench key-to-frame-burst-200-16mib` で基準値を締めた。5 回の中央値 6.9 ms（最大 808 ms の外れ値あり）は上限が 8.9 ms になり今日の 3 回のうち 1 回（9.2 ms）が落ちる詰めすぎなので、同じ手順で `--repetitions 15` の中央値を採った（他の値と `recordedAt` は不変・保護対象の変更の根拠は ADR 0047）。ログは `out/184-check.log`・`out/184-check-2.log`・`out/184-control.log`・`out/184-adopt.log`。

### 5-bd. 数字レジスタ `"0`〜`"9` と小削除 `"-`（Issue #204・ADR 0050・2026-09-29）

ブランチ `feat/204-numbered-registers`（main `8b32b9b` から・工程 1 `0bd66d4`・工程 2 `5388b1d`）。工程 1: `VimNumberedRegisters`（10 本の表）・`VimNumberedRule`（`by_extent` / `always`）・`VimRegisterTarget` の `numbered` / `small_delete`・`VimState` の `numbered` / `small_delete`、書き手 `registers_written` の 4 段（名指し → `"1` の繰り下がり → `"-` → 無名）と `p` `P` の読み `register_read`。工程 2: `normal_recording` が `"` の待ちの次の数字を記録に残し、`numbered_advanced`（記録の先頭に並ぶ `"{名前}` の最後の組が `1`〜`8` なら 1 つ進める）を `repeated_change` と `replayed_visual` が通す。`@` の読みは `register_read(state, selection)` の 1 本へ寄せ、名前は `register_selection_of` で解く（`@{0-9}` `@-`・`last_macro` は数字と `-` も覚える・`@_` は今までどおり拒否）。`q{0-9}` は数字へ文字単位で置き換えて録り（繰り下がらない・無名は変えない）、`q-` は拒否。`vim_register_stored` は `0`〜`9` と `-` を置き換えで受ける。application・ui・eng・oracle の測定コードは不変。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan） | `VimRegisterTarget` / `VimNumberedRule` の網羅・関数長。工程 1・2 とも警告 0 で成功 |
| `build/nib_tests.exe --vim-macro` | 対象。工程 1 **1234 checks**（625 から）→ 工程 2 **1392 checks 成功**（fixture 127 件 = macro 20 ＋ register 107 の再生と、`q0` `q1` `q9` の録画・`q-` の拒否・`vim_register_stored` の `0` `9` `-` と `@-` `@@` `@0` `@9` の契約 2 本） |
| `build/nib_tests.exe --vim-dot` | `.` の記録と再生の経路。**1479 checks 成功**（不変） |
| `build/nib_tests.exe`（引数なし） | 工程 1 15145 → 工程 2 **15287 checks 成功** |
| `ctest --test-dir build -R nib_unit` | 1 / 1 成功 |
| `python eng/vim-oracle.py --regenerate --only register-` × 2 | 工程 2: 107 measured / 1398 reused。2 回で `VimFixtures.hpp`（`09F880AE…6EA6DFC`）と `fixtures.json`（`B6E28638…9F9B65DF`）の SHA-256 が一致 |
| `python eng/protected-diff.py --base origin/main --build --allow --vim-macro` | **終了 0**。`8b32b9b..5388b1d`・`fixtures 1423 -> 1505 / metadata 2 / deleted 0 / changed 0 / added 82`・保護対象は `none`・`--vim-macro 変化 625 -> 1392 allowed`・`scopes 22 / same 21 / 未測 0` |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py --build-dir build` | **0 violation / 0 violation** |
| clang-format（変更した C++ ファイル）・`git diff --check` | 指摘なし |

fixture は 82 件（工程 1 の 66 件と工程 2 の `register-dot-*` 11 件・`register-at-*` 5 件）。`"1pu.u.` は `:normal!` の中でも `u` が塊を区切る形（前置きを `"1yy` `"2yy` `"3yy` の yank だけにした `"1Pu.u.`）で fixture にした。`p` で書くと Nib の `u` 後のキャレットが Vim と 1 行ずれる（行単位の `p` の取消・本 Issue の前からの差）ので `P` にした。

対象を限定した理由: 差分は core の Vim の engine（レジスタの置き場・書き手・読み・`.` の記録と再生の鍵・`@` `q`）と `--vim-macro` の単体テストと fixture である。renderer・ui/win32・application の経路は不変なので、Release・`eng/measure-speed.py`・`verify-window`・`check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021。速さは設計席）。

FR-003 / ARC-001/004 / CPP-002/003/004/011 / QLT-001/012 / CNF-010/011 を自己レビュー。書き手は `registers_written` の 1 本・読みは `register_read` の 1 本（planned・レビュー事項・`grep -n "\.numbered\|small_delete" src/core/VimStep.cpp` が書き手の 4 段・`register_read`・`register_selection_of`・録画の停止・`vim_register_stored`・範囲の印だけ）。

### 5-be. クリップボードのレジスタ `"+` `"*`（Issue #210・ADR 0051・2026-09-29）

ブランチ `feat/210-clipboard-registers`（main `5706c55` から・工程 1 `97f140f`・工程 2 `33f1172`）。工程 1（core）: `VimRegisterTarget::clipboard`・`VimState.clipboard`（命令が終わると消える写し）・`VimStep.clipboard`（OS へ出す本文）・述語 `vim_reads_clipboard`・置く口 `vim_clipboard_loaded`・純関数 `vim_register_of_clipboard`（`VimClipboardText`）・書き手 `registers_written` の名指しの書き先に `clipboard`。工程 2（application）: `step_vim` が `vim_step` を呼ぶ直前に `load_vim_clipboard`（述語が真なら `ClipboardPort::read` を 1 回・失敗は空のレジスタ）、効果を写した後に `send_vim_clipboard`（`with_document_newlines` で文書の改行にして `ClipboardPort::write`・失敗は黙って続ける）。替え玉 `ScriptedClipboard` に書かれた回数と読まれた回数。ui・adapters・eng・fixture は不変。OS のクリップボードには触れていない（替え玉だけ）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan） | `VimRegisterTarget` の網羅・関数の認知的複雑度（`step_vim` は読みと書きを private の 2 関数に分けて 10 以内）。工程 1・2 とも警告 0 で成功 |
| `build/nib_tests.exe --vim-clipboard` | 対象（新しい scope）。**191 checks 成功**（controller を通す契約 11 本: 読みの表 9 行・`"+p` `"*p` `3"+p` `"+3p` `"+P` の 13 行と書かれた回数 0・読む回数（普通の打鍵 0・`"+yy` 2・`"+p` 1）・`"+yy` `"*yy` `"+yj` `"+yw` `"+dd` `"*dd` `"+x` `vj"+y` の OS 側の本文を LF 文書と CRLF 文書で・`"0` `"1` `"-` と `"1` の繰り下がり・書きの失敗・読みの失敗と空文字列・`.` の読み直し（種類も新しい方）・`@+` `@@` `@*`・`"ayy@a` の中の `"+p`・矩形の `"+y` と続く `"+p`） |
| `build/nib_tests.exe --vim-macro` | 工程 1 の core の契約 6 本。main 1392 → **1500 checks 成功**（工程 2 で不変） |
| `build/nib_tests.exe`（引数なし） | 工程 1 15395 → 工程 2 **15586 checks 成功**（＋191 = 新しい scope の契約） |
| `ctest --test-dir build -R nib_unit` | 1 / 1 成功 |
| `python eng/protected-diff.py --base origin/main --build --allow --vim-macro` | **終了 0**。`5706c55..33f1172`・`fixtures 1505 -> 1505 / metadata 0 / deleted 0 / changed 0 / added 0`・保護対象は `none`・`--vim-macro 変化 1392 -> 1500 allowed`・`--vim-clipboard 新規 - -> 191`・`scopes 23 / same 21 / 未測 0` |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py --build-dir build` | **0 violation / 0 violation** |
| clang-format（変更した C++ ファイル）・`git diff --check`・`eng/validate-git.ps1` | 指摘なし |

対象を限定した理由: 差分は core の Vim の engine（レジスタの書き先・読み・`VimStep` の欄）と application の `step_vim` の前後 2 か所と単体テストである。renderer・ui/win32・adapters の経路は不変で、`ClipboardPort` の実装も変えていないので、Release・`eng/measure-speed.py`・`verify-window`・`check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021。実機のクリップボードでの確認と速さは設計席）。fixture は oracle が実機のクリップボードを書き換えるので作らない（ADR 0051 の強制）。

FR-003 / ARC-001/003/004/007/010 / CPP-002/005/011 / QLT-001/012 を自己レビュー。OS との往復は `step_vim` の前後の 2 関数だけ（planned・レビュー事項・`grep -n "ports_.clipboard" src/application/EditorController.cpp` は `copy_selection` `cut_selection` `paste_clipboard`・入力行の `PasteCommand`（本 Issue の前から）と本 Issue の 2 行）。engine は OS に触れない（symbols 0）。

### 5-bf. 次の 1 鍵を待つ状態の数字を `.` の記録に残す（Issue #209・2026-09-29）

ブランチ `fix/209-dot-keeps-waited-digit`（main `ba1e8ca` から）。先に fixture `dot-waited-digit-*` 7 件（`r1l.` `3r13l.` `r0l.` `df1.` `ct2X<Esc>l.` `$dF0.` `vlr1ll.`）を本物の Vim から生成し、直す前の Nib で `--vim-dot` を回して `r1` `3r1` `df1` `ct2` の 4 件が本文・レジスタ・桁で落ちることを確かめた（`r0` `dF0` は `0` が回数の途中でないので桁にならず、VISUAL の記録は数字を落とさないのでどちらも直す前から通る）。`normal_recording` の `"` の待ちだけの特例（ADR 0050 の決定 9）を「`before.input_wait` があるとき」の 1 本の規則に広げた。`counts_as_digit` は不変で、オペレータ待ち（`pending`）は待ちではないので `d2w` の `2` は今までどおり回数。application・ui・adapters・eng は不変。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan） | 警告 0 で成功 |
| `build/nib_tests.exe --vim-dot` | 対象。直す前 **8 of 1528 checks failed** → 直した後 **1528 checks 成功**（1479 から。fixture の再生は 181 → 188 件） |
| `build/nib_tests.exe`（引数なし） | **15635 checks 成功**（15586 から ＋49 = 新しい fixture 7 件の再生） |
| `ctest --test-dir build -R nib_unit` | 1 / 1 成功 |
| `python eng/vim-oracle.py --regenerate --only dot-waited-digit-` × 2 | 7 measured / 1505 reused。2 回で `VimFixtures.hpp`（`AC7B3FDD…01182EAC`）と `fixtures.json`（`BEC608B5…D0FE8C67`）の SHA-256 が一致 |
| `python eng/protected-diff.py --base origin/main --build --allow --vim-dot` | **終了 0**。`ba1e8ca..3fea3c0`（この節を足す前のコミット・コードと fixture は同じ）・`fixtures 1505 -> 1512 / metadata 2 / deleted 0 / changed 0 / added 7`・保護対象は `none`・`--vim-dot 変化 1479 -> 1528 allowed`・`scopes 23 / same 22 / 未測 0` |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py --build-dir build` | **0 violation / 0 violation** |
| clang-format（変更した C++ ファイル）・`git diff --check` | 指摘なし |

対象を限定した理由: 差分は core の `normal_recording` の条件 1 行と `--vim-dot` の単体テストと fixture である。renderer・ui/win32・application の経路は不変なので、Release・`eng/measure-speed.py`・`verify-window`・`check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。

FR-003 / ARC-001 / CPP-002/004 / QLT-001/012 / CNF-010/011 を自己レビュー。記録の更新は `vim_recorded` の 1 か所のまま（ARC-001）。

### 5-bg. 改行を含む文字単位の `p` `P` のキャレット（Issue #205・2026-09-29）

ブランチ `fix/205-put-caret-multiline-characters`（main `2592a57` から・`78c0b35`）。先に fixture `put-caret-*` 13 件（`vjy` の `p` `P` `3p` `3P`・別の位置の `p`・`v$y` の `p` `P`・`y/ind<CR>` の `p`・`p.` `P.`・本文が改行で始まる `p` `P`・対照の 1 行の `yw3p`）を本物の Vim から生成し、#204 の 3 件（`register-numbered-di-paren-lines` `-3dw-crosses-lines` `-visual-lines-delete`）の `keys` の末尾の `gg` を外して作り直した。直す前の Nib では 13 件のうち対照の `yw3p` を除く 12 件と #204 の 3 件が桁・行（`.` の 2 件は本文も）で落ちた（25 of 15757 checks failed）。`put_characters` は本文に改行を含むとき貼った本文の先頭（回数つきは最初のコピーの先頭）にキャレットを置き、含まないときは今までどおり最後の文字に置く。先頭が行末を越える `put-caret-newline-start-*` は controller の既存の寄せでそのまま Vim と一致した（`put_characters` に寄せを書いていない）。application・ui・adapters・eng・単体テストは不変。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan） | 警告 0 で成功 |
| `build/nib_tests.exe`（引数なし） | 対象（`put-caret-*` を再生するのは既定実行だけ・既存の `p-*` と同じ）。直す前 **25 of 15757 checks failed** → 直した後 **15738 checks 成功**（15635 から ＋103 = 新しい fixture 13 件 × 7 と、キャレットが 2 行目の fixture ごとの行の検査 10 と #204 の 3 件の 2） |
| `build/nib_tests.exe --vim-macro` | #204 の 3 件の再生。1500 → **1502 checks 成功**（`vim_byte_column` がキャレットの行の手前の行ごとに 1 つ数える。2 件がキャレット 2 行目になった） |
| `build/nib_tests.exe --vim-dot` | `.` の記録と再生の経路。**1528 checks 成功**（不変） |
| `ctest --test-dir build -R nib_unit` | 1 / 1 成功 |
| `python eng/vim-oracle.py --regenerate --only put-caret-` × 2・`--only register-numbered-` × 2 | 13 measured / 1512 reused と 46 measured / 1479 reused。4 回で `VimFixtures.hpp`（`862E5AD9…00DF4D04`）と `fixtures.json`（`F9D7285D…6A12FEB3`）の SHA-256 が一致 |
| `python eng/protected-diff.py --base origin/main --build --allow --vim-macro` | **終了 1（予定どおり）**。`2592a57..78c0b35`・`fixtures 1512 -> 1525 / metadata 2 / deleted 0 / changed 3 / added 13`・changed は `register-numbered-di-paren-lines`（L1433）`register-numbered-3dw-crosses-lines`（L1434）`register-numbered-visual-lines-delete`（L1436）の 3 件だけで、Issue の受け入れ条件どおり `gg` を外してキャレットも守る形にした変更・保護対象は `none`・`--vim-macro 変化 1500 -> 1502 allowed`・`scopes 23 / same 22 / 未測 0` |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py --build-dir build` | **0 violation / 0 violation** |
| clang-format（`VimStep.cpp`）・`git diff --check` | 指摘なし |

対象を限定した理由: 差分は core の `put_characters` のキャレットの式 1 行と fixture である。行単位・矩形の put、renderer・ui/win32・application の経路は不変なので、Release・`eng/measure-speed.py`・`verify-window`・`check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。

FR-003 / ARC-001 / CPP-002/004 / QLT-001/012 / CNF-010/011 を自己レビュー。行末を越えたキャレットの寄せは controller の 1 か所のまま（ARC-001）。

### 5-bh. NORMAL の `X`（Issue #206・2026-09-29）

ブランチ `feat/206-normal-capital-x`（main `dd3c1dd` から・`08f1eb6`）。先に fixture 11 件（`capital-x-*` 10 件: `$X` `$3X`・行頭の `jX` `j3X`・行頭を越える `j$9X`・`"aX` の後の `"ap`・`$2X.`・`X` の後の `"-p`・全角の前・Issue 本文の `dd$Xyy"-p"1p`、と `macro-capital-x-at-the-line-start-does-not-stop` 1 件）を本物の Vim から生成し、直す前の Nib で既定の実行が **34 of 15821 checks failed**（`capital-x-*` だけ）で落ちることを確かめた。`X` は表に 1 行・`VimAction::remove_character_before`（分類は `edit_line` なので VISUAL では今のまま効かない）で、`x` の隣の分岐が `motion_range(view, VimMotion::left, count, remove)` → `performed` を通す（`X` のための範囲・失敗・レジスタの規則は足していない）。Issue 本文の「行頭では失敗」は Vim 9.1 の実測（レジスタ a=`Xj` の `j2@a` がキャレット 3 行目＝マクロが止まらない・`Xx` と `dhx` は後ろの `x` が走り `hx` は止まる）で訂正し、行頭の `X` は `dh` と同じく空の範囲の削除で失敗の印を付けない。結合文字の前の `X` は Vim が 1 文字（`é`）として消すが、Nib の `h` `x` `dh` 共通の `backward_characters` / `forward_characters` がコードポイント単位なので合わず、fixture から外した（範囲外・報告のみ）。`--vim-macro` の契約（選ぶ fixture は `macro-` か `register-`・件数 127）に合わせて macro の fixture を `macro-` で始め、件数を 128 にした。application・ui・adapters・eng は不変。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan） | 警告 0 で成功 |
| `build/nib_tests.exe`（引数なし） | 対象（`capital-x-*` を再生）。直す前 **34 of 15821 checks failed** → 直した後 **15825 checks 成功**（15738 から） |
| `build/nib_tests.exe --vim-macro` | macro の fixture の再生と scope の契約。1502 → **1514 checks 成功** |
| `build/nib_tests.exe --vim-dot` | `.` の記録と再生の経路。**1528 checks 成功**（不変） |
| `ctest --test-dir build -R nib_unit` | 1 / 1 成功 |
| `python eng/vim-oracle.py --regenerate --only capital-x- --only macro-capital-x-` × 2 | 11 measured / 1525 reused。2 回で `VimFixtures.hpp`（`DC3F3091…9F7F1E35`）と `fixtures.json`（`EF439DEF…DDA03836`）の SHA-256 が一致 |
| `python eng/protected-diff.py --base origin/main --build --allow --vim-macro` | **終了 0**。`dd3c1dd..08f1eb6`（この節を足す前のコミット）・`fixtures 1525 -> 1536 / metadata 2 / deleted 0 / changed 0 / added 11`・保護対象は `none`・`--vim-macro 変化 1502 -> 1514 allowed`・`scopes 23 / same 22 / 未測 0` |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py --build-dir build` | **0 violation / 0 violation** |
| clang-format（変更した C++ ファイル）・`git diff --check` | 指摘なし |

対象を限定した理由: 差分は core の鍵の表 1 行と動作 1 値と `x` の隣の分岐、`--vim-macro` の件数の契約、fixture である。renderer・ui/win32・application の経路は不変なので、Release・`eng/measure-speed.py`・`verify-window`・`check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。

FR-003 / ARC-001 / CPP-002/004/012 / QLT-001/012 / CNF-010/011 を自己レビュー。範囲は `motion_range` の left の 1 本を再利用（ARC-001）・動作の分類の網羅は `every_action_has_one_row` の static_assert で守られる（CPP-002）。

### 5-bi. `u` と Ctrl-r の後のキャレット（Issue #208・ADR 0052・2026-09-29）

ブランチ `fix/208-undo-caret-restore`（main `f73a23c` から）。3 工程で進めた。工程 1（`b0e1e0a`）は `core::Edit` に戻り先 `restore` を足して `replace` の 1 か所で書き、履歴の畳みと再生の畳み（`merge_replayed_edits`）は最初の値を残し、Vim の `u` / Ctrl-r は `edit.at` ではなく `restore`（Ctrl-r は `vim_same_line_and_column` で同じ行と桁）へ置く。工程 2（`0fdf052`）は engine が範囲の先頭か着地を `VimStep.restore` に載せ、controller が編集の前にキャレットを置く（1 行の `dd` `cc` の min・`cj` の 1 行下・行単位の移動の着地・VISUAL の固定の端・矩形の左上）。工程 3 は変更が 2 つ以上ある形を `--vim-dot` の契約で守り（期待値は `out/probes/probe-undocaret-2026-09-29.md` の U10〜U13 の Vim 9.1 の実測・本文は probe と同じ 8 行）、#204 の `register-dot-undo-advances` の隣に `p` の形 `register-dot-undo-advances-after`（`"1pu.u.`）を足した。契約: `.` の後の `u` `u` `<C-r>` `<C-r>`（`dw` `ixyz<Esc>` `oabc<Esc>` `dd` `Axyz<Esc>` と `x` の `3.`）・`@a` `2@a`（`xjx`）と `@a`（`ddjdd` `Axyz<Esc>jIabc<Esc>`）の 1 回の `u` と `<C-r>`・3 か所の変更の `u` × 3 と `<C-r>` × 3（`dw / x / D`・下から上の `x`・`ixyz / Axyz / dd`）・最終行の `dd` のやり直し（行が無いので最後の行の最初の非空白）と最終行に `o` した行の `dd`・通常モードの Ctrl+Z / Ctrl+Y は今までどおり戻した本文の末尾。どれも工程 1・2 の実装のまま Vim の実測と一致した（`src/` は工程 3 で不変）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan） | 3 工程とも警告 0 で成功 |
| `build/nib_tests.exe`（引数なし） | 工程 1: 12 of 16224 checks failed（VISUAL の `u` の既存 fixture 7 件・工程 2 で直す途中の状態）→ 工程 2: **16764 checks 成功** → 工程 3: **16863 checks 成功** |
| `build/nib_tests.exe --vim-dot` | 足した契約（U10〜U13 と通常モードの履歴）。1528 → **1619 checks 成功** |
| `build/nib_tests.exe --vim-macro` | `register-` の fixture を選ぶ scope。件数の契約を 108 件（計 129）にして 1514 → **1523 checks 成功** |
| `ctest --test-dir build -R nib_unit` | 工程 2・3 とも 1 / 1 成功 |
| `python eng/vim-oracle.py --regenerate --only undo-caret-` × 2（工程 1・2）・`--only register-dot-` × 2（工程 3） | 工程 1: 51 measured / 1536 reused・工程 2: 109 measured / 1536 reused・工程 3: 12 measured / 1634 reused。工程 3 の 2 回で `VimFixtures.hpp`（`E6C2D897…C370144F`）と `fixtures.json`（`433E77EB…AB4A2BD6`）の SHA-256 が一致。`register-dot-undo-advances-after` は Vim が本文 `one\nthree\ntwo\nthree\nfour`・(2,1)・`three\n` `V` で、Nib も同じ |
| `python eng/protected-diff.py --base origin/main --build --allow vim-dot --allow vim-macro` | **終了 0**。`f73a23c..2c39b24`（この節を足す前のコミット）・`fixtures 1536 -> 1646 / metadata 2 / deleted 0 / changed 0 / added 110`・保護対象は `none`・`--vim-dot 変化 1528 -> 1619 allowed`・`--vim-macro 変化 1514 -> 1523 allowed`・`scopes 23 / same 21 / 未測 0` |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py --build-dir build` | 3 工程とも **0 violation / 0 violation** |
| clang-format（変更した C++ ファイル）・`git diff --check` | 指摘なし |

対象を限定した理由: 差分は core の `Edit`・`EditHistory`・`VimStep`・`VimCaret` と application の `u` / Ctrl-r と `step_vim` の 1 か所、単体テストと fixture である。renderer・ui/win32・adapters の経路は不変。Release・`eng/measure-speed.py`・`check.ps1 -Full` は実行していない（設計席が回す・QLT-001 / QLT-012・ADR 0021）。

FR-003 / ARC-001 / ARC-004 / CPP-002/003/004 / QLT-001/012 / CNF-010/011 を自己レビュー。戻り先を書くのは `replace` の 1 か所と engine の `VimStep.restore`（ARC-001）・`Edit` の欄は既定値なしの集成初期化で書き忘れがコンパイルで落ちる（ADR 0052 の強制）。

### 5-bj. Vim の 1 文字と結合文字（Issue #216・ADR 0053・2026-09-29）

ブランチ `fix/216-vim-character-with-combining`（main `9b28d10` から）。2 工程で進めた。工程 1（`a45a959`）は core に文字を歩く 1 対 `vim_character_end` / `vim_character_start`（判定は仮想桁の表の `DisplayWidth::zero` の 1 つ）を足し、`forward_characters` / `backward_characters`・`r`・VISUAL の端・NORMAL のキャレットの寄せをその境に揃えた（fixture `combining-*` 49 件）。工程 2 は、寄せ（`vim_resting_caret` / `vim_line_and_column`）が行の全体を写さずキャレットの手前 64 バイトと後ろ 64 バイトの窓だけを読み、見つけた文字の先頭が窓の先頭なら手前へ・文字の終わりが窓の終わりに近ければ後ろへ窓を倍に広げる形にした（`4a7603b`・1 MiB の 1 行で行の途中・200 バイトの結合文字の列の上・行末を単体で確かめる）。続けて fixture 43 件（語 `w e b dw de`・`diw daw diW yiw di" di(`・`f t F T ; , 2fe dfe dtb dTa`・put `p P 3p`・矩形 `d y r $`・INSERT の `<BS>` と `3a`）を先に生成し、直す前に 6 件 9 checks（`ee` `tb;` と矩形 4 件）が落ちることを確かめてから、語の走査の 1 歩・テキストオブジェクトの行の中の次と前・`f t F T` の走査と着地・矩形の端の文字の終わりを 1 対に置き換えた（`85e4cf7`）。`W E B ge` は Nib に無い移動なので fixture から外した。INSERT の `<BS>`（Vim の既定で文字の全体を消す）・`3a` の反復・put の後のキャレットは直さずに Vim と一致した。application・ui・adapters・eng は不変。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan） | 2 工程とも警告 0 で成功 |
| `build/nib_tests.exe`（引数なし） | 工程 1: **17218 checks 成功** → 工程 2: **17527 checks 成功** |
| `build/nib_tests.exe --vim-characters` | 工程 1: 356 → 工程 2: 直す前 **9 of 686 checks failed** → **665 checks 成功**（`W E B ge` の 4 件を外した後） |
| `build/nib_tests.exe --vim-text-objects` / `--vim-character-search` / `--vim-visual-block` / `--vim-dot` / `--vim-virtual-column` | 置き換えた走査の既存の経路。**1992 / 639 / 1105 / 1619 / 424 checks 成功** |
| `ctest --test-dir build -R nib_unit` | 2 工程とも 1 / 1 成功 |
| `python eng/vim-oracle.py --regenerate --only combining-` × 2 | 工程 1: 49 measured / 1646 reused・工程 2: 92 measured / 1646 reused。工程 2 の 2 回で `VimFixtures.hpp`（`684CD082…CBB892C1`）がバイト一致・`fixtures.json` は `38D19A05…4CD87FB2` |
| `python eng/protected-diff.py --base origin/main --build` | **終了 0**。`9b28d10..85e4cf7`（この節を足す前のコミット）・`fixtures 1646 -> 1738 / metadata 2 / deleted 0 / changed 0 / added 92`・保護対象は `none`・`--vim-characters 新規 - -> 665`・`scopes 24 / same 23 / 未測 0`（`--allow` 不要） |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py --build-dir build` | 2 工程とも **0 violation / 0 violation** |
| clang-format（変更した C++ ファイル）・`git diff --check` | 指摘なし |

対象を限定した理由: 差分は core の文字の 1 対・寄せ・語・テキストオブジェクト・`f t`・矩形の端と、単体テストと fixture である。renderer・ui/win32・application・adapters の経路は不変。Release・`eng/measure-speed.py`（寄せの窓と 1 歩ごとの表の引き）・`check.ps1 -Full` は実行していない（設計席が回す・QLT-001 / QLT-012・ADR 0021）。

FR-003 / ARC-001 / ARC-012 / CPP-002/004/011 / QLT-001/012 / CNF-010/011 を自己レビュー。文字の境は 1 対だけ（ARC-001）・`grep -rln "DisplayWidth::zero" src/core` は仮想桁・表示・1 対だけ（寄せの窓は 1 対で判定し表を 2 度引かない）・バイト列の走査（照合器・UTF-16 との変換・`*` の語の切り出し）は変えていない（ADR 0053 の決定 3・4）。

### 5-bk. WORD の移動 `W` `E` `B` と `ge` `gE`（Issue #222・2026-09-29）

ブランチ `feat/222-word-motions-and-ge`（main `47eb938` から・`0b905fb`）。先に fixture `word-motion-*` 51 件（`W E B ge gE`・回数 `3W 3E 2B 2ge 2gE`・行またぎ・空行・記号の混じる語 `foo.bar(baz) qux`・Tab・文書の端での失敗・`dW cW yE dE dB dge dgE cge d2W d3B d3ge`・VISUAL の `vWd vgey v2Ey vBd`・`.` の `dW. $dge. cWx<Esc>W.`・結合文字つきの語 6 件（#216 で外した `wge` `W` `E` `$B` を含む））を本物の Vim から生成し、直す前の Nib で既定の実行が **101 of 17892 checks failed**（`word-motion-*` 48 件）で落ちることを確かめた。`W B E` は鍵の表に 3 行、`ge gE` は `g` の接頭辞の表（`gg` と同じ表・`VimKeyTable` の `g_bindings`）に 2 行で、どれも `VimMotion` の値になって `w b e` と同じ分岐を通る。語の分類は `VimKeyTable` の表 `big_word_motions` が決め、`VimWordMotion` の既存の走査に `VimWordWalk`（回数と `VimWordClass`）で渡す（WORD の分類は `iW aW` と同じ `vim_character_class`）。`ge gE` は Vim の `bckend_word(eol=FALSE)` で、テキストオブジェクトの手前の語の末尾（`eol=TRUE`）と同じ 1 周 `backward_word_end` を共有する。範囲は inclusive で、本文の先頭で動けなければオペレータを打ち消す。`cW` は `cw` と同じ特例（`big_word_end_for_change`）。oracle の `q` の拒否の状態機械は NORMAL とオペレータの後ろで `g` の次の鍵を引数として読み、VISUAL の `vge` は VISUAL のまま読む（`q` を含む fixture は無い）。application・ui・adapters・eng は不変。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan） | 警告 0 で成功 |
| `build/nib_tests.exe`（引数なし） | 対象（`word-motion-*` を再生）。直す前 **101 of 17892 checks failed** → 直した後 **17889 checks 成功** |
| `build/nib_tests.exe --vim-text-objects` / `--vim-line-jumps` / `--vim-characters` / `--vim-dot` | 共有した手前の語の末尾（`iw aw` の後ろ向き）・`g` の接頭辞の表（`gg`）・語の 1 歩・`.`。**1992 / 999 / 665 / 1619 checks 成功**（不変） |
| `ctest --test-dir build -R nib_unit` | 1 / 1 成功 |
| `python eng/vim-oracle.py --regenerate --only word-motion-` × 2 | 51 measured / 1738 reused。2 回で `VimFixtures.hpp`（`10CBAC71…8A294C61`）と `fixtures.json`（`7C18D0A4…099C8F40`）の SHA-256 が一致 |
| `python eng/protected-diff.py --base origin/main --build` | **終了 0**。`47eb938..0b905fb`（この節を足す前のコミット）・`fixtures 1738 -> 1789 / metadata 2 / deleted 0 / changed 0 / added 51`・保護対象は `none`・`scopes 24 / same 24 / 未測 0`（`--allow` 不要） |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py --build-dir build` | **0 violation / 0 violation** |
| clang-format（変更した C++ ファイル）・`git diff --check` | 指摘なし |

対象を限定した理由: 差分は core の鍵の表・動作と移動の値・語の走査と `VimStep` の移動の分岐と、単体テストと fixture である。renderer・ui/win32・application・adapters の経路は不変。Release・`eng/measure-speed.py`・`check.ps1 -Full` は実行していない（設計席が回す・QLT-001 / QLT-012・ADR 0021）。

FR-003 / ARC-001 / CPP-002/011/012 / QLT-001/012 / CNF-010/011 を自己レビュー。WORD のための 2 つ目の歩き方は無い（ARC-001）・動作の表の欠落と重複は `every_action_has_one_row` の static_assert、移動の値の網羅は `switch` が守る（CPP-002）・鍵と分類は表（CPP-012）・`VimWordWalk` は 1 ファイル 1 型（CPP-011）。

### 5-bl. 回数つきの語の移動が本文の端に当たったとき（Issue #224・2026-09-29）

ブランチ `fix/224-counted-word-motion-at-text-edge`（main `efe0cd3` から・`afefd23`）。先に fixture `word-edge-*` 34 件（後ろ向き: 本文 `\nabc` と `abc def` で `2b 9b d2b d9b y2b c2bX<Esc> d2B d2ge d9ge 2ge` と回数がちょうど足りる対照 `d2b`・前向き: `9w d9w y9w c9wX<Esc> 9e d9e 9W d9W` と行をまたぐ `d9w d9e`・対照 `d2w d2e`）を本物の Vim から生成し、直す前の Nib で既定の実行が **20 of 18127 checks failed**（`word-edge-*` のうち 1 行目が空行の本文でオペレータを付けた 7 件 `d2b d9b y2b c2b d2B d2ge d9ge` の本文とレジスタ）で落ちることを確かめた。Vim の実測は `bck_word` / `bckend_word` の読みどおりで、周の始めに本文の先頭にいたとき（空行で止まった次の周）だけ FAIL になり、裸の移動は着地まで動いてビープし、オペレータは打ち消されてキャレットだけ着地へ動く。周の途中で本文の先頭に当たったときは OK（`wd2b` は `abc ` を消す）。前向きの `w e W` は回数の途中で本文の終わりに当たってもオペレータは効き（Vim の `nv_wordcmd` は FAIL をオペレータの無いときだけ見る）、直す前の Nib と一致していた。語の歩き方は 1 本のまま、`vim_previous_word` / `vim_previous_word_end` が着地と失敗の印 `VimMotionLanding`（`std::optional<VimRepeatFailure>`）を返し、`VimStep` は裸の移動で失敗の印を添え（`moved_or_failed`）、オペレータでは範囲を作らず着地へ動く（`cancelled_at_landing`）。application・ui・adapters・eng は不変。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan） | 警告 0 で成功 |
| `build/nib_tests.exe`（引数なし） | 対象（`word-edge-*` を再生・`vim_previous_word` の失敗の印の単体テスト 4 件）。直す前 **20 of 18127 checks failed** → 直した後 **18131 checks 成功** |
| `ctest --test-dir build -R nib_unit` | 1 / 1 成功 |
| `python eng/vim-oracle.py --regenerate --only word-edge-` × 2 | 34 measured / 1789 reused。2 回で `VimFixtures.hpp`（`EB587152…05CE7DA4`）と `fixtures.json`（`36F693B0…24144EDC`）の SHA-256 が一致 |
| `python eng/protected-diff.py --base origin/main --build` | **終了 0**。`efe0cd3..afefd23`（この節を足す前のコミット）・`fixtures 1789 -> 1823 / metadata 2 / deleted 0 / changed 0 / added 34`・保護対象は `none`・`scopes 24 / same 24 / 未測 0`（`--allow` 不要） |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py --build-dir build` | **0 violation / 0 violation** |
| clang-format（変更した C++ ファイル）・`git diff --check` | 指摘なし |

対象を限定した理由: 差分は core の語の後ろ向きの走査の戻り値と `VimStep` の移動・オペレータの失敗の扱いと、単体テストと fixture である。renderer・ui/win32・application・adapters の経路は不変。Release・`eng/measure-speed.py`・`check.ps1 -Full` は実行していない（設計席が回す・QLT-001 / QLT-012・ADR 0021）。

FR-003 / ARC-001 / CPP-002/004/005/011 / QLT-001/012 / CNF-010/011 を自己レビュー。端のための 2 つ目の歩き方は無い（ARC-001）・`landing_of` の移動の値は `switch` で網羅（CPP-002）・失敗は `std::optional<VimRepeatFailure>` で返し `value()` で読む（CPP-004 / CPP-005）・`VimMotionLanding` は 1 ファイル 1 型（CPP-011）。

### 5-bm. 前向きの語の移動が本文の終わりに当たったときの失敗（Issue #226・2026-09-29）

ブランチ `fix/226-forward-word-motion-failure`（main `1d6173f` から・`10e2e61`）。Issue 本文の読み（「`9w` は回数の途中で本文の終わりに当たると失敗」）は `w` について誤りで、本物の Vim 9.1 の実測で訂正した。裸の移動の失敗は本文・キャレット・レジスタに出ないので、マクロの打ち切りで観測する fixture `macro-word-edge-*` 21 件（`register` 欄・a = `9wx` `9wrZ` `2wx` `wx` `9Wx` `Wx` `9ex` `2ex` `ex` `9Ex` `Ex` など・本文 `abc def` `a.c d.f` `abc d` `abc  ` `abc\ndef`）を生成した。Vim の規則は 2 本（関数の写し）: `w` `W`（`fwd_word`）は周の始めに本文の最後の文字にいて出られないときだけ FAIL で、周の途中で本文が尽きたら OK（`abc def` の `9wx` は `x` が走り、`abc d` の `9wx` は 2 周目が最後の文字から始まって打ち切り）。`e` `E`（`end_word`）は周のどの歩でも本文の終わりの先へ出ようとしたら FAIL（`9ex`・`abc  ` の `c` からの `ex` は打ち切り）。直す前の Nib はマクロの 6 件（`w` の 1 件と `e` `E` の 5 件）の本文・キャレット・レジスタで落ちた（probe 30 件の段階で **48 of 18385 checks failed**）。`:normal!` 1 本の形（鍵列 `9ex` をそのまま）は Nib の fixture の再生が失敗の後も鍵を流すので使わない（Issue #230）。歩き方は 1 本のまま、`forward_word` の 1 周の結果を閉じた `VimWordAdvance`（続ける・止まった＝OK・端から出られない＝FAIL）にし、`vim_next_word` / `vim_word_end` が `VimMotionLanding` を返す。`landing_of` の前向きの枝（`forward_word_landing`）が失敗の印を運び、オペレータの範囲（`forward_word_range`）は着地だけを使う（Vim の `nv_wordcmd` は FAIL を OP_NOP のときだけ見る）。application・ui・adapters・eng は不変。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan） | 警告 0 で成功（途中で `motion_range` が関数の大きさで落ちたので範囲を `forward_word_range` へ出した） |
| `build/nib_tests.exe`（引数なし） | 対象（前向きの失敗の単体テスト `verify_vim_forward_word_failures` と既存の `vim_next_word` の着地）。**18332 checks 成功** |
| `build/nib_tests.exe --vim-macro` | 対象（`macro-word-edge-*` 21 件を再生・scope の件数 129 → 150）。**1735 checks 成功** |
| `ctest --test-dir build -R nib_unit` | 1 / 1 成功 |
| `python eng/vim-oracle.py --regenerate --only macro-word-edge-` × 2 | 21 measured / 1823 reused。2 回で `VimFixtures.hpp`（`1A0A57B9…A6F99EC7`）と `fixtures.json`（`B4027B10…1F3B93F0`）の SHA-256 が一致 |
| `python eng/protected-diff.py --base origin/main --build --allow --vim-macro` | **終了 0**。`1d6173f..10e2e61`（この節を足す前のコミット）・`fixtures 1823 -> 1844 / metadata 2 / deleted 0 / changed 0 / added 21`・保護対象は `none`・`--vim-macro 1523 -> 1735 allowed`・`scopes 24 / same 23 / 未測 0` |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py --build-dir build` | **0 violation / 0 violation** |
| clang-format（変更した C++ ファイル）・`git diff --check` | 指摘なし |

対象を限定した理由: 差分は core の語の前向きの走査の戻り値と `VimStep` の移動の着地・オペレータの範囲の取り出しと、単体テストと fixture である。renderer・ui/win32・application・adapters の経路は不変。Release・`eng/measure-speed.py`・`check.ps1 -Full` は実行していない（設計席が回す・QLT-001 / QLT-012・ADR 0021）。

FR-003 / ARC-001 / ARC-010 / CPP-002/005/011 / QLT-001/012 / CNF-010/011 を自己レビュー。端のための 2 つ目の歩き方は無く、裸の移動の着地（`moved_by`）も `forward_word_landing` を通る（ARC-001）・`VimWordAdvance` は `switch` で網羅し `default` を書かない（CPP-002）・失敗は `VimMotionLanding` の `std::optional<VimRepeatFailure>` で返す（ARC-010 / CPP-005）・`VimWordAdvance` は 1 ファイル 1 型（CPP-011）。

### 5-bn. fixture の鍵の記法の表を oracle の 1 つにする（Issue #229・ADR 0054・2026-09-29）

ブランチ `chore/229-fixture-key-notation-one-table`（main `b672d59` から・ADR `80030e7`・実装 `509998f`）。記法の表を `eng/vim-oracle.py` の測定の領域の `KEY_TABLE`（19 行・名前・Vim の書き方・Nib の鍵）の 1 つにし、`KEY_NAMES` は `key_names_of(KEY_TABLE)` の導出の値にした（使い手の `vim_keys` `key_tokens` は不変・行の順も前の `KEY_NAMES` と同じ）。C++ の表は `key_names_header()` の生成物 `tests/vim/VimKeyNames.hpp`（`--key-names` が Vim なしで書き、`--regenerate` も書く）。`VimNamedKey` と `VimKeyName` は手書きの `tests/vim/VimKeyName.hpp` へ移し、`tests/unit/VimTestSupport.hpp` の手書きの 19 行は消した。対応は前の C++ の表から写し、`<NL>` は U+000A・`<Space>` は U+0020 の文字。生成物の並びは表の順（`<NL>` が `<C-v>` の後へ動いた。名前が互いの接頭辞にならないことを表の検査が守るので、読みの結果は順に依らない）。`src/` と `fixtures.json` は不変。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `python eng/vim-oracle.py --regenerate`（`--only` なし・全件） | 移行の証明（決定 8）。**1844 measured / 0 reused・3 分 20 秒**。`tests/vim/VimFixtures.hpp` と `fixtures.json` は `git diff --stat` に出ない（バイト一致）。`out/229-regenerate.log` |
| `python eng/vim-oracle.py --key-names` × 2 | 2 回とも `VimKeyNames.hpp` の SHA-256 が `19608703…7278E56D` で不変。全件の再生成の後も同じ値 |
| `cmake --build build --target nib_tests`（Debug・clang-tidy・ASan・UBSan） | 警告 0 で成功 |
| 同（反例・決定 5） | 生成物の `arrow_left` を `arrow_leftward` に書き換えると `VimKeyNames.hpp:27:46: error: no member named 'arrow_leftward' in 'nenenib::core::VimSpecialKey'` で落ちる。戻して警告 0 で成功。`out/229-build-neg.log` |
| `build/nib_tests.exe`（引数なし）・`ctest --test-dir build -R nib_unit` | **18332 checks 成功**（#226 の記録と同じ数）・1 / 1 成功 |
| `python -m unittest discover -s tests/conformance -p 'test_vim_oracle.py'` | 27 tests OK。表の検査の反例 8 通り（重複・接頭辞・`<` が無い・`>` が無い・空の Vim の書き方・列挙子の名前でない・コードポイントでない・2 列）・`KEY_NAMES` が表の順の導出であること・表を変えると部分再生成が `measurement code changed` で拒むこと（決定 7） |
| `python -m unittest discover -s tests/conformance -p 'test_conformance.py'` | 78 tests OK。CNF-010 の鍵の記法の正例 2（関数の出力・保存されている生成物）と反例（`<Left>` の鍵を書き換える・行を消す・行を足す・CRLF・ファイルが無い） |
| `python eng/conformance.py --build-dir build` | 0 violation。実リポジトリの反例 2 通り（生成物の 1 行を書き換える / 消す）はどちらも CNF-010 で `1 violation(s)`・終了 1、戻すと 0。`out/229-cnf-neg.log` / `229-cnf-neg2.log` |
| `python eng/protected-diff.py --base origin/main --build` | **終了 0**。`b672d59..509998f`・`fixtures 1844 -> 1844 / metadata 0 / deleted 0 / changed 0 / added 0`・`scopes 24 / same 24 / 未測 0` |
| `python eng/symbols.py --build-dir build --require core application` | 0 violation |
| clang-format（`VimKeyName.hpp` `VimKeyNames.hpp` `VimTestSupport.hpp`）・`git diff --check` | 指摘なし |

対象を限定した理由: 差分は道具（oracle・conformance）とテストの足場で、製品のコードは触っていない。Release・`eng/measure-speed.py`・`check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。

ARC-001 / ARC-012 / CPP-002 / QLT-001 / QLT-012 / QLT-013 / CNF-010 を自己レビュー。記法の表は 1 つ（ARC-001 / ARC-012）・生成物の形は `key_names_header` の 1 か所で、書く側（`--key-names` / `--regenerate`）と検査する側（CNF-010）が同じ関数を呼ぶ・`KEY_TABLE` と `key_names_of` は測定の領域（`literal()` より上）、生成の関数は下にある。

### 5-bo. 貼り付けた本文の改行を文書の改行の形に揃える（Issue #235・ADR 0055・施主決定 D19・2026-09-29）

ブランチ `fix/235-paste-document-line-ending`（main `c72e144` から・ADR `083e21e`・実装 `a207006`）。畳む関数 `clipboard_line_feeds`（`src/core/ClipboardText.hpp` / `.cpp`）を core の 1 本にし、`vim_register_of_clipboard` はそれを呼んでから種類だけを決める（畳みの本体は移しただけで振る舞いは不変）。`EditorController::paste_clipboard` は `with_document_newlines(clipboard_line_feeds(本文), 文書の改行)` を `replace` へ渡す（undo の単位・選択の置き換え・キャレット・読みの失敗は不変）。契約は `tests/unit/ApplicationTests.cpp`（既存のクリップボードの契約と同じ翻訳単位・引数なしの実行）に `verify_clipboard_line_feeds`（表 7 行）・`verify_controller_paste_line_endings`（LF / CRLF の文書 × 本文の表 9 行・選択の置き換えとキャレット・Vim の INSERT の Ctrl+V）・`verify_controller_paste_undo_and_failure`（Ctrl+Z 1 回・読みの失敗）。本文の改行の形は保存の口へ出したバイト列（`ScriptedFiles::written`）で見る。

| 検査 | 退行の対象と実測 |
| --- | --- |
| 契約を先に書いて直す前の Nib で `build/nib_tests.exe` | **9 checks が落ちる**（表の 6 行・選択の置き換え・Vim の INSERT・CRLF の文書へ LF の 3 行）。`out/235-tests-before.log`（記録には期待値の誤りの 1 件〔桁は 1 始まり〕も入っていて、直して 9 件） |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan） | 警告 0 で成功 |
| `build/nib_tests.exe`（引数なし）・`ctest --test-dir build -R nib_unit` | **18381 checks 成功**・1 / 1 成功 |
| `build/nib_tests.exe --vim-clipboard` / `--vim-macro` | 191 / 1735 checks 成功（`"+p` の既存の契約は不変） |
| `python eng/protected-diff.py --base origin/main --build` | **終了 0**。`c72e144..a207006`・`fixtures 1844 -> 1844 / metadata 0 / deleted 0 / changed 0 / added 0`・`scopes 24 / same 24 / 未測 0` |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py --build-dir build` | **0 violation / 0 violation** |
| clang-format（変更した C++ ファイル）・`git diff --check` | 指摘なし |

対象を限定した理由: 差分は core の文字列の純関数 1 本と application の貼り付けの 1 関数と単体テストである。コピー・切り取り・入力行への貼り付け・ui/win32・adapters・fixture は不変。実機の確認（ほかのアプリの本文を LF の文書へ Ctrl+V）は設計席が行う。Release・`eng/measure-speed.py`・`check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。

FR-002 / ARC-001 / ARC-009 / CPP-007 / QLT-001 / QLT-012 を自己レビュー。畳む規則は `clipboard_line_feeds` の 1 本（`grep -rn "clipboard_line_feeds" src` は `ClipboardText.*`・`VimClipboardText.cpp`・`EditorController.cpp` だけ・ARC-001）・文書の改行へ直すのは `EditorController.cpp` の `with_document_newlines` の 1 本で 2 つ目を作らない（ARC-009）・OS に触れない純関数で core に置く（ARC-007）。

### 5-bp. fixture の再生は `:normal!` と同じく失敗した鍵の後ろを打ち切る（Issue #230・2026-09-29）

ブランチ `test/230-replay-stops-at-failure`（main `c72e144` から・`47384cd` へ rebase・実装 `8af6621`）。oracle は fixture の鍵列を 1 本の `:normal!` で Vim へ流し、Vim は鍵が失敗すると残りを打ち切るが、Nib の fixture の再生（`vim_replay`）は失敗の後も鍵を流していた。controller に harness の口 `press_vim_keys(std::span<const core::VimKey>)` を 1 つ足し（1 鍵の流し方は `press_vim_key` と同じ `begin_intent` と `deliver_vim_key`・打ち切りの判定は再生の打ち切りと同じ `deliver_vim_key` の返り値・失敗の値を外へ出す口は作らない）、テストの足場に `vim_normal` を足して `verify_vim_fixture` だけをそれへ切り替えた（fixture を再生する scope はどれも `verify_vim_fixture` を通る）。打った鍵の `vim_replay` と手書きの契約（ADR 0046 の決定 3 を前提に鍵を 1 本に並べた 25 checks）は変えない。`register` 欄の録画（`store_vim_fixture_macro`）は、oracle が鍵を実行せず `let @a = "..."` で文字のまま置くので、途中で失敗しても全部の鍵を録る打った鍵の `vim_replay` のまま。#226 から外した `word-edge-forward-*` 9 件（`9wx` `$wx` `9ex` `$ex` `9Wx` `9Ex` `2wx` と `abc d` の `9wx`・`abc  ` の `2lex`）を本物の Vim で生成した。打ち切りの契約 `verify_vim_normal_stops_at_failure`（本文 `abc` の `hx` は `vim_normal` で不変・`vim_replay` で `bc`）を足した。製品の打鍵と再生の経路は不変。

下ごしらえ（`out/probes/probe-replay-abort-2026-09-29.md`）の実測: 打ち切りにしても既存の fixture 1844 件は 1 件も落ちず、打ち切りなしでは `word-edge-forward-*` のうち 6 件（`last-char-w` `9e` `last-char-e` `9E` `9w-one-letter-last-word` `e-trailing-blank`）が本文・キャレット・レジスタの 24 checks で落ちる（打ち切りの反例）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan） | 警告 0 で成功 |
| `build/nib_tests.exe`（引数なし） | 対象（fixture 全件の再生・打ち切りの契約・手書きの契約）。**18448 checks 成功**（main `47384cd` の 18381 から +67・fixture 9 件と打ち切りの契約 1 本） |
| `build/nib_tests.exe --<scope>`（24 scope 全部） | fixture を再生する scope を含めて全部成功（件数は下の protected-diff と同じ） |
| `ctest --test-dir build -R nib_unit` | 1 / 1 成功 |
| `python eng/vim-oracle.py --regenerate --only word-edge-forward-` × 2 | 9 measured / 1844 reused。2 回で `VimFixtures.hpp` と `fixtures.json`（`9C03B3D7…87D28A21`）の SHA-256 が一致 |
| `python eng/protected-diff.py --base origin/main --build` | **終了 0**。`47384cd..8af6621`・`fixtures 1844 -> 1853 / metadata 2 / deleted 0 / changed 0 / added 9`・保護対象は `none`・`scopes 24 / same 24 / 未測 0` |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py --build-dir build` | **0 violation / 0 violation** |
| clang-format（変更した C++ ファイル）・`git diff --check` | 指摘なし |

対象を限定した理由: 差分は application の harness の口 1 本（製品の打鍵・再生からは呼ばれない）とテストの足場・契約・fixture である。renderer・ui/win32・adapters・core は不変。Release・`eng/measure-speed.py`・`check.ps1 -Full` は実行していない（QLT-001 / QLT-012・ADR 0021）。

ARC-001 / ARC-010 / CPP-005 / QLT-001 / QLT-012 / CNF-010 を自己レビュー。打ち切りの判定は controller の `deliver_vim_key` の返り値の 1 つで、再生（`perform(VimReplay)`）と同じ値を読み、テストの側に 2 つ目の判定を書かない（ARC-001）・失敗は既存の `std::optional<VimRepeatFailure>` のまま外へ出さない（ARC-010 / CPP-005）。

### 5-bq. 複数タブの状態・切り替え・閉じる・開く・起動（Issue #237・ADR 0056・施主決定 D20〜D22・2026-09-29）

ブランチ `feat/237-tabs-state`（main `3198c71` へ rebase・ADR `2f97224` / `2fa9ea2`・工程 1 `ae539c6`・工程 2 `f235242`）。縦切り 1/4 で、ui（帯の描画・hit test・鍵）は変えず、見た目と操作は変わらない。

- 工程 1: アクティブな文書は `EditorState` の今の欄のまま、ほかのタブは不変の束 `DocumentState`（`src/application/DocumentState.hpp`）を `shared_ptr<const>` の列 `parked_` に置く。意図 `NewTab` `SwitchTab` `StepTab` `CloseTab`、core の純関数 `vim_switched_document`、切り替えの 1 本（`leave_document` → `with_switched` / `with_new_tab` / `with_closed` → `enter_document`）、`EditorFrame` の `tabs` `active_tab` `closing`。
- 工程 2: `accept(OpenDocument)` を決定 5 の 3 つの枝にする（(a) `open_tab_of` が `FilePort::same_file` で見つけたタブへ `SwitchTab`・読み直さない → (b) `blank_untitled`〔パスが無い・本文が空・履歴が空〕なら `with_opened` → (c) `leave_document` → `with_new_tab().with_opened(...)` → `enter_document`。読めなければタブを足さず `fail`）。`Win32FileAdapter::same_file` は絶対パスを `CompareStringOrdinal(..., TRUE)` で比べる。`EditorController` のコンストラクタは `std::vector<OpenDocument>` を受けて順に `apply` し、最後の失敗を控えて最初のフレームへ載せ直す。`Main.cpp` の `initial_documents` は `--measure <json>` の組を飛ばした引数を全部 `absolute_file_path` で渡す（0 個と 1 個の経路は今と同じ重さ・`document_opened` の節目の位置は不変）。
- 差し戻し（工程 3・`e02a26f`）: 窓を閉じる（`WM_CLOSE` → `EditorWindow::close_window`）は、application の純関数 `next_unsaved_tab`（`src/application/UnsavedTab.hpp`・帯の位置 from から右へ最初の未保存）で未保存のタブを左から順に `SwitchTab` で映して今の `confirm_discard` を出し、キャンセルか保存の失敗で止めて窓を閉じない（見えないタブの変更が確認なしに消える道を塞ぐ・#239 から前倒し）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan） | 工程 1・工程 2 とも警告 0 で成功（`out/237-step1-build.log` / `out/237-step2-build.log`） |
| `build/nib_tests.exe --tabs` | 工程 1 **51 checks**（新しいタブ・切り替えで文書ごとに保つ・undo は文書ごと・折り返し・閉じた後のアクティブ・最後の 1 つで closing・Vim の保留と INSERT・入力行と IME・脇の束の参照）→ 工程 2 **88 checks**（同じファイルは読み直さず切り替え・替え玉が同じと答えた組・何も書いていない無題に開く／打った・取り消した無題は新しいタブ・アクティブの右に入る・開けなければタブを足さない・起動の引数 3 つ／2 つ目が開けない／なし／同じファイル 2 回） |
| `build/nib_tests.exe`（引数なし）・`ctest --test-dir build -R "nib_unit\|nib_adapters"` | 工程 1 18432 checks（rebase の前）・工程 2 **18536 checks 成功**・2 / 2 成功 |
| `build/nib_adapter_tests.exe`（`nib_adapters`） | **113 checks 成功**（`same_file`: 大文字と小文字だけ違う絶対パスは同じ・同じ経路は同じ・違う経路は違う） |
| 工程 3 の `cmake --build build`・`build/nib_tests.exe --tabs` / 引数なし・`ctest -R nib_unit`・`protected-diff` | 警告 0・**95 checks**（+7: 保存済み 1 本は確かめない・保存済みを飛ばし右へ探す・切り替えて映るタブが確かめるタブ・0 1 3 4 の順に 1 度ずつ）・18543 checks・1 / 1・`3198c71..e02a26f` で changed 0 / added 0 / deleted 0・same 24（`out/237-step3-*.log`・`out/protected/e02a26f.json`） |
| `python eng/protected-diff.py --base origin/main --build` | **終了 0**。`3198c71..f235242`・`fixtures 1853 -> 1853 / metadata 0 / deleted 0 / changed 0 / added 0`・`scopes 25 / same 24 / 未測 0`・`--tabs` 新規 88（`out/protected/f235242.json`） |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py --build-dir build` | **0 violation / 0 violation**（工程 1・2 とも） |
| clang-format（変更した C++ ファイル）・`git diff --check` | 指摘なし |

対象を限定した理由: 差分は application の状態と意図・controller の開くとコンストラクタ・adapters の比較 1 本・起動引数の読み方と単体テストである。renderer・ui/win32・fixture は不変。1 打鍵で写すのは参照の列だけ（決定 2）。Release・`eng/measure-speed.py`（タブ 50 本の打鍵の実測を含む）・`check.ps1 -Full`・実機は設計席が行う（QLT-001 / QLT-012 / QLT-014・ADR 0021）。

ARC-001 / ARC-004 / ARC-005 / ARC-007 / ARC-010 / CPP-002 / CPP-003 / CPP-011 を自己レビュー。開く経路は `accept(OpenDocument)` の 1 本で、起動引数もダイアログも同じ意図を通る（ARC-001）・同じファイルの比べ方は `FilePort` の 1 つで adapters だけが OS に触れる（ARC-007）・タブを足す経路は `leave_document` → 状態 → `enter_document` の 1 本（工程 1 と共有）・開けなかった理由は既存の `FileFailure` のまま（ARC-010）。残る穴: 起動引数の失敗は最後の 1 件だけが窓の告知に届く（決定 13 の「1 件ずつ」は ui の告知が表示値の 1 件を読む形のままなので満たしていない・どの Issue で直すかは設計席）・Ctrl+O は新しいタブに開く場合も今の「保存しますか」を先に出す（#238 で外す）。

### 5-br. タブの帯に全部のタブを描きマウスで切り替えて閉じて足す（Issue #238・ADR 0056 決定 8・9・12・施主決定 D20〜D22・2026-09-29）

ブランチ `feat/238-tab-band`（main `ac0fe14` の上・ADR `f8d1e5c` / `ce23093` / `5a4177b`・工程 1 `0141cd8`〔報告の `23f8fc6` を rebase した同じ patch-id の commit〕・工程 2 `7982358`・差し戻し `08a3b23` / `22e146a`・design `026c73e`）。縦切り 2/4 で、鍵（#239）と「∨」の一覧（#240）は変えない。

- 工程 1: core の帯の配置を 1 つの入力 `TitleBarInput`（幅・DPI・タブの数・アクティブ・送り量・hover）から計算する `title_bar_layout` にし、位置つきの結果 `TitleBarTarget` を `title_bar_target` が返す（あふれ・viewport・「∨」・× の領域 24 DIP・掴む余白 40・ホイール 1 刻み 122）。application に意図 `TitleBarWidth` `ScrollTabs` `PointTitleBar` と状態の帯の幅・送り量・hover。色のトークン `Palette::tab_hover`（ユーザーテーマの `ui.tab_hover`・省略時は上書き後の `tab_active`）。
- 工程 2: renderer が全部のタブを viewport で切り抜いて描き（アクティブ = `tab_active` と下線・hover = `tab_hover`・× は `tab_close_rect` の所だけ）、窓が同じ入力の配置で hit test・左クリック（押下で切り替え・× と「＋」は離したとき）・中ボタン・hover（`TrackMouseEvent` の `WM_MOUSELEAVE`）・帯の上のホイールを当てる。閉じる流れは `tab_unsaved` なら切り替えて `confirm_discard`・最後の 1 本で窓を閉じる（D22）。窓の最小の大きさ 360 × 200 DIP（`minimum_window`・ADR `ce23093`）。Ctrl+O は「保存しますか」を先に出さない。
- 差し戻し: 押した要素を覚えて離した要素と同じときだけ動かす（core の純関数 `title_bar_released`・ui の `left_pressed_` / `middle_pressed_`）・配置の入力は表示値を作らずに状態の 4 つの値から読む（`EditorController::title_bar_input(width, dpi)`）・入りきらない題名は文字単位で切って「…」（`DWRITE_TRIMMING_GRANULARITY_CHARACTER`）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・clang-tidy・ASan・UBSan・ui を含む全 target） | 工程 1・工程 2・差し戻しとも警告 0・エラー 0（`out/238-step1-build.log` / `out/238-step2-build.log` / `out/238-step2r-build.log`・`out/238-step2r-build3.log`）。工程 2 の途中で `dispatch` が readability-function-size で落ち、分割して直した |
| `build/nib_tests.exe --tabs` | 工程 1 **110 checks**（95 → 110・送り量・hover）→ 工程 2 **119 checks**（配置の入力の写し・`tab_unsaved`・`title_bar_hover` の 9 通り・`minimum_window`・最小幅 360 で 1 本はあふれない）→ 差し戻し **133 checks**（`title_bar_released` の 4 通り・送った後に欠けたタブの × に当たる点で値なし・状態から読む配置の入力が 6 欄一致） |
| `build/nib_tests.exe`（引数なし）・`ctest --test-dir build -R nib_unit` | 工程 1 18614 checks・工程 2 18623 checks・差し戻し **18637 checks 成功**・1 / 1 成功（工程 1 は `ThemeCodec` を変えたので `-R "nib_unit\|nib_themes"` で 2 / 2） |
| `python eng/protected-diff.py --base origin/main --build --allow=--tabs` | **終了 0**（`23f8fc6` / `7982358` / `08a3b23` / `22e146a`）。`fixtures 1853 -> 1853 / changed 0 / added 0 / deleted 0`・`scopes 25 / same 24 / 未測 0`・`--tabs` 95 → 133 allowed（`out/protected/22e146a.json` ほか） |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py --build-dir build` | **0 violation / 0 violation**（工程 1・工程 2・差し戻し。差し戻しの省略記号は ui だけなので symbols は 1+2 の後） |
| clang-format（変更した C++ ファイル）・`git diff --check`・`eng/validate-git.ps1` | 指摘なし・passed |
| 実機の画（設計席・施主の了承の後・Release `build/release-7982358` と `build/release-22e146a`・125%） | 1 回目: タブ 1 本の見た目・「＋」で足す・切り替えと窓の題名・「● 」・hover の面と × と `toggle`・caption と本文へ出ると hover が消える・12 本のあふれと「∨」・帯のホイール 1 刻み 1 本と端・本文のホイールで帯は動かない・「∨」は何もしない・× と中ボタンで閉じる・未保存の確認・最後の 1 本で窓が閉じる（終了コード 0）・最小 450 × 250 px（= 360 × 200 DIP）は設計どおり。右が欠けたタブの右寄りを押すと閉じる不具合と題名のぶつ切りを見つけて差し戻した。2 回目（`22e146a`）: 欠けたタブは切り替わって閉じない・題名は「…」・本文で押して「＋」で離してもタブはできない・中ボタンは別のタブで離すと閉じない・「＋」で押して離すとできる（`out/window-verification/tabs-238/*.png`・1 回目は `first/`・記録は `out/reports/done-238-design.md`） |
| `python eng/measure-speed.py --check --executable build/release-22e146a/NeNeNib.exe`（設計席） | **6 benches checked, 0 regression(s), 0 unmeasurable**（機械 bc8a356f37c68491・5 回・startup-first-frame 206.8 ms・startup-window-shown 34.1 ms・key-to-frame-single 0.838 ms・key-to-frame-burst-200 3.454 ms・open-large-file-16mib 252.1 ms・key-to-frame-burst-200-16mib 6.572 ms・`out/speed/2026-09-29T13-26-07Z.json`） |
| 工程 3（本節の追記だけ）の `python eng/conformance.py --build-dir build`・`git diff --check` | **0 violation**・指摘なし |

対象を限定した理由: 差分は core の帯の配置と純関数・application の意図と状態・ui/win32 の描画とポインタの処理・色のトークンとユーザーテーマの読み 1 欄・単体テストである。Vim の engine・fixture・ファイルの経路は不変（protected-diff で changed 0）。帯の見た目と押す・離すの流れは契約で覆えないので設計席が実機で見た。工程 3 は本節の追記だけで、実装・テスト・依存が不変なので工程 1・2・差し戻しの成功結果と設計席の実機・速さの結果を再利用する（QLT-001 / QLT-012 / QLT-014・ADR 0021）。

ARC-001 / ARC-011 / CPP-002 / CPP-003 / CPP-009 / CPP-011 / CPP-012 / CPP-017 を自己レビュー。帯の配置は `title_bar_layout` の 1 本で renderer と hit test が同じ入力を使い（ARC-001）・入力は `title_bar_input` の 1 本・閉じる流れの確かめる判定は `tab_unsaved` の 1 本・押した要素と離した要素の突き合わせは `title_bar_released` の 1 本・色は `Palette` のトークンだけ。残る穴: マウスを動かしたときの重さはベンチに無い（入力は状態の 4 つの値だけを読む形にした）・別の DPI のモニターへ移す・最大化と元に戻すの実機の確認は無い（`TitleBarWidth` を送り直す経路は `resize` と `change_dpi` の 2 か所・契約は application まで）・ライトと組み込み 9 テーマの `tab_hover` の画は無い（値は契約で守る）・窓の外で左ボタンを離すと `left_pressed_` が残る（次の押下で必ず上書き・消去される）・「∨」は #240・鍵は #239。

### 5-bs. タブの鍵で足して切り替えて閉じる（Issue #239・ADR 0056 決定 10・2026-09-29）

ブランチ `feat/239-tab-keys`（main `6522d21` の上・ADR `6e53eed`・工程 1 `ca337bf` / `0159b6e`）。縦切り 3/4 で、「∨」の一覧・Ex の `tabnext`・Vim の `gt`（#240）は変えない。

- 工程 1: core に閉じた `enum` の `TabKey`（`control_t` `control_tab` `control_shift_tab` `control_f4` `control_w`）と `TabCommand`（`open` `next` `previous` `close`）、モードとの組から命令を決める純関数 `tab_command_for(TabKey, EditMode) -> std::optional<TabCommand>`（`src/core/TabKeyTable.hpp/.cpp`・`default` なし・Vim の Ctrl+W は値なし）。ui/win32 の `tab_shortcut(WPARAM, bool shift)`（OS の仮想キーなので `default` あり・CPP-017・Shift と組むのは Tab だけ）と `EditorWindow::press_tab_key` / `run_tab_command`。`press_key` は入力行の分岐（入力行の間は今のまま・Ctrl+T は SearchHop）の直後、フォントの鍵と Vim の Ctrl の表より前に引く。命令は #237 の意図 `NewTab` `StepTab` と #238 の閉じる流れ `close_tab` を通す。
- 実機の検査の道具: `eng/window_driver.py` の `press_combination`（複数の修飾鍵）と `eng/verify-window.py --tabs`（既定の全体実行の末尾にも載る）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・ui を含む全 target） | 警告 0・エラー 0（`out/239-step1-build2.log`） |
| `build/nib_tests.exe --tabs` | 133 → **143 checks**（5 つの鍵 × 2 つのモードの 10 通り・`out/239-step1-tabs.log`） |
| `build/nib_tests.exe`（引数なし）・`ctest --test-dir build -R nib_unit` | **18647 checks 成功**・1 / 1 成功（`out/239-step1-unit.log` / `out/239-step1-ctest.log`） |
| `python eng/protected-diff.py --base origin/main --build --allow=--tabs` | **終了 0**。`fixtures 1853 -> 1853 / changed 0 / added 0 / deleted 0`・`scopes 25 / same 24`・`--tabs` 133 → 143 allowed（`out/protected/0159b6e.json`） |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py --build-dir build` | **0 violation / 0 violation**（conformance は C++ の commit の前と両 commit の後の 2 回） |
| clang-format（変更した C++ 8 ファイル）・`python -m py_compile eng/verify-window.py eng/window_driver.py` | 指摘なし（1 回目は長い注釈で落ち、注釈を短くして通した）・OK |
| 実機（設計席・施主の了承の後・Release `build/release-0159b6e/NeNeNib.exe`・125%）`python eng/verify-window.py --tabs --executable build/release-0159b6e/NeNeNib.exe --capture out/239-tabs-frames` | **終了 0・`driven: true`**。起動 1 本・0 → Ctrl+T × 2 で 3 本・2 → Ctrl+Tab で 0（末尾から先頭へ折り返す）→ Ctrl+Shift+Tab で 2 → Ctrl+F4 で 2 本・1 → 通常モードの Ctrl+W で 1 本・0（確認なし）→ Vim モードの Ctrl+W で 1 本のまま（窓も残る）→ `/` の入力行で Ctrl+T で 1 本のまま。未保存の 2 本で窓を閉じると確認が 2 回・題名は `● tab-left.txt` → `● tab-right.txt` の順・「いいえ」「キャンセル」の後に窓が残りファイルは書き換わらない。Ctrl+Tab と Ctrl+F4 が `WM_KEYDOWN` で届く前提を確かめた（`out/logs/239-verify-tabs.log`・`out/239-tabs-frames/tabsOpened.png` `tabsSearchLine.png`・記録は `out/reports/done-239-design.md`） |
| `python eng/measure-speed.py --check --executable build/release-0159b6e/NeNeNib.exe`（設計席） | **6 benches checked, 0 regression(s), 0 unmeasurable**（機械 bc8a356f37c68491・5 回・startup-first-frame 195.4 ms・startup-window-shown 32.3 ms・key-to-frame-single 1.000 ms・key-to-frame-burst-200 3.501 ms・open-large-file-16mib 251.3 ms・key-to-frame-burst-200-16mib 7.083 ms・`out/speed/2026-09-29T13-54-41Z.json`） |
| 工程 2（本節の追記だけ）の `python eng/conformance.py --build-dir build`・`git diff --check` | **0 violation**・指摘なし |

対象を限定した理由: 差分は core の鍵と命令の閉じた型と純関数 1 本・ui/win32 の鍵の写しと命令の実行・単体テスト・実機の検査の道具である。application の意図と状態・renderer・Vim の engine・fixture は不変（protected-diff で changed 0）。鍵が OS から届く形は契約で覆えないので設計席が実機で確かめた。工程 2 は本節の追記だけで、実装・テスト・依存が不変なので工程 1 の成功結果と設計席の実機・速さの結果を再利用する（QLT-001 / QLT-012 / QLT-014・ADR 0021）。

ARC-001 / CPP-002 / CPP-011 / CPP-017 を自己レビュー。鍵 → 命令は `tab_command_for` の 1 本で、タブの操作は #237 の意図と #238 の閉じる流れを通る（ARC-001）・閉じた選択肢の `switch` に `default` なし（CPP-002）・`default` は OS の仮想キーの写しだけ（CPP-017）。残る穴: Ctrl+1〜9・使った順の切り替え・Vim の Ctrl-W の実装は後続（決定 15）・「∨」の一覧と `tabnext` `gt` は #240・IME の変換中にタブの鍵を押したときと入力行の間の Ctrl+Shift の組は実機で確かめていない・本物のポインタがタブの上にあると hover で塗られる（色は `tab_active` と違うので判定は崩れない想定）・`eng/verify-window.py --tabs` は本物のキー入力を送るので CI では回さない。

### 5-bt. 開いているタブの一覧と Ex の `tabnext` と Vim の `gt` を行き先の 1 本に通す（Issue #240・ADR 0057・2026-09-30）

ブランチ `feat/240-tab-list`（main `d6537e1` の上・ADR `e640603` / `2d89dec`・工程 1 `b1a1147` / `110843e` / `60637a9`・工程 2 `2680406`）。縦切り 4/4。

- 工程 1（core と application）: 行き先を決める純関数 `tab_destination(const TabJump &, std::size_t active, std::size_t tab_count) -> std::optional<std::size_t>`（`src/core/TabDestination.hpp/.cpp`・閉じた `enum` の `TabJumpDirection`）を 1 本置き、Ctrl+Tab（`StepTab`）・Vim の `gt` `gT` `{N}gt` `{N}gT`（`VimAction::next_tab` `previous_tab`・`VimEditorView.tabs`・効果 `VimSwitchTab`・行き先なしは `not_moved` の失敗）・Ex の `:tabnext` `:tabprevious` `:tabnew` `:tabclose` `:tabs`（閉じた `ExTabVerb` と `ExTabRequest`・行き先なしは `E475: Invalid argument: <N>`・受けない形は `Not supported: <入力>`）がそこを通る。一覧は Ctrl+P の面そのもの（`CommandPaletteSource`・`tab_list_choices`・行の場所は `CommandChoice.detail`）で、意図 `OpenTabList` が開く。`:tabclose` は状態を変えず `EditorFrame.close_request` に位置を載せる。再生の途中の切り替えは出ていく文書の編集をその文書の 1 単位に畳む。
- 工程 2（ui/win32）: 「∨」を離したときに `OpenTabList`（押した要素と離した要素が同じときだけ）・`send` が `close_request` を読んで × と同じ `close_tab` を通す・`detail` を題名の後ろに `muted` で描き入りきらなければ「…」で切る。一覧が開いている間の「∨」のクリックは面の外のクリックとして閉じる（ADR 0057 決定 7 の追記 `2d89dec`）。
- Vim の実測: 下ごしらえの席の probe `out/probes/probe-vimtabs-2026-09-29.md`（Vim 9.1 patch 1-4・`-u NONE -i NONE -N -n -es`・約 240 件・クリップボードに触れていない）。fixture は oracle がタブを観測できないので不能で、契約で守る（ADR 0057「強制」）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・ui を含む全 target・clang-tidy 込み） | 工程 1・工程 2 とも警告 0・エラー 0（`out/240-step1-build.log` / `out/240-step2-build.log`） |
| `build/nib_tests.exe --tabs` / `--ex-settings` / `--command-palette` | `--tabs` 143 → **248 checks**・`--ex-settings` 153 → **195**・`--command-palette` 130 → **146**（工程 1 `out/240-step1-tabs.log` `out/240-step1-ex-settings.log` `out/240-step1-command-palette.log`・工程 2 の後も 248 / 146 `out/240-step2tabs.log` `out/240-step2command-palette.log`） |
| `build/nib_tests.exe`（引数なし）・`ctest --test-dir build -R nib_unit` | **18824 checks 成功**・1 / 1 成功（工程 1 `out/240-step1-unit.log` `out/240-step1-ctest.log`・工程 2 `out/240-step2-unit.log` `out/240-step2-ctest.log`） |
| `python eng/protected-diff.py --base origin/main --build --allow --tabs --allow --ex-settings --allow --command-palette` | **終了 0**。`d6537e1..2680406`・`fixtures 1853 -> 1853 / changed 0 / added 0 / deleted 0`・`scopes 25 / same 22`・変化 3 つは allowed（工程 1 `out/protected/60637a9.json`・工程 2 `out/protected/2680406.json` `out/240-step2-protected.log`）。既存の契約のうち新しい候補で変わったのは 2 つ（`--command-palette` の空の入力の先頭 `colorscheme` → `tabs`・`--ex-settings` の後ろ向き補完の最初 `set nohlsearch` → `tabclose`。点数が候補の長さなので足した候補の結果） |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py --build-dir build` | **0 violation / 0 violation**（工程 1 `out/240-step1-symbols-4.log` `out/240-step1-conformance-4.log`・工程 2 `out/240-step2-symbols.log` `out/240-step2-conformance.log`）。工程 1 の途中で `std::ranges::stable_sort` が `?nothrow@std@@3Unothrow_t@1@B` を core の外へ出して ARC-003 が落ち、許可の表は触らず `std::ranges::sort` に書き換えて 0 に戻した |
| clang-format --dry-run --Werror（工程 1 の C++ 26 ファイル・工程 2 の 3 ファイル） | 指摘なし（工程 2 の 1 回目は `DrawTextLayout` の折り返しで落ち、`clang-format -i` で直した・`out/240-step2-format.log`） |
| 実機（設計席・施主の了承の後・Release `build/release-2d89dec/NeNeNib.exe`・125%（DPI 120）・窓 800 × 450 px・`eng/window_driver.py` を使う 1 回限りのスクリプトで窓へ post・本物のキーボードとポインタと OS のクリップボードは使っていない） | タブ 12 本で「∨」→ Ctrl+P の面に一覧（入力は空・アクティブの行が選ばれ 12 / 12・題名の後ろに場所が `muted`・入りきらない場所は「…」）→ `vim` で 2 件に絞られ Enter で切り替わって面が閉じる・「∨」→ Esc は閉じてタブは変わらない。Vim の 3 本で `gt` は 1 本目へ・`gT` は 3 本目へ折り返す・`2gt` は 2 本目・`9gt` は動かない・2 本目から `2gT` は 3 本目。`:tabnext 1` は 1 本目・`:tabnext 4` は動かず `E475: Invalid argument: 4`・`:tabp` は折り返す・`:tabnew` は右隣に「無題」・`:tabnext +1` は `Not supported: tabnext +1`・`:tabs` は一覧を開く。`:tabclose` は未保存でなければ確認なしで閉じ・未保存なら「保存しますか」でキャンセルなら閉じず「いいえ」で閉じ・最後の 1 本なら窓が閉じてプロセスが終了コード 0 で終わる（画は `out/window-verification/tabs-240/*.png` 20 枚・記録は `out/reports/done-240-design.md`） |
| `python eng/measure-speed.py --check --executable build/release-2d89dec/NeNeNib.exe`（設計席） | **6 benches checked, 0 regression(s), 0 unmeasurable**（機械 bc8a356f37c68491・5 回・startup-first-frame 218.4 ms・startup-window-shown 36.4 ms・key-to-frame-single 1.036 ms・key-to-frame-burst-200 3.591 ms・open-large-file-16mib 265.7 ms・key-to-frame-burst-200-16mib 6.502 ms・`out/speed/2026-09-29T15-19-34Z.json`。各ベンチの 1〜2 回目に外れ値があり、ゲートは中央値で判定して通っている） |
| 工程 3（本節の追記と ADR 0057「強制」の `planned` → `active` だけ）の `python eng/conformance.py --build-dir build`・`git diff --check` | **0 violation**・指摘なし |

対象を限定した理由: 差分は core の行き先の純関数と閉じた型・Vim の engine の鍵 2 つと効果 1 つ・Ex の名前の表とタブの命令・Ctrl+P の面の出どころと行の場所・application の意図と `close_request`・ui/win32 の「∨」と `close_request` と場所の描画・単体テストである。既存の fixture は不変（protected-diff で changed 0）。ui の見た目と OS から届くクリックは契約で覆えないので設計席が実機で確かめた。速さは打鍵の経路（`step_vim` がタブの値を渡す・再生の畳みの起点）に触れるので設計席が測った。工程 3 は文書だけで、実装・テスト・依存が不変なので工程 1・2 の成功結果と設計席の実機・速さの結果を再利用する（QLT-001 / QLT-012 / QLT-014・ADR 0021）。

ARC-001 / ARC-003 / ARC-011 / CPP-002 / CPP-004 / CPP-009 / CPP-011 / CPP-012 / CPP-017 を自己レビュー。行き先の数え方は `tab_destination` の 1 本で Ctrl+Tab・`gt`・`:tabnext`・一覧が共用し、閉じる流れは #238 の `close_tab` の 1 本（ARC-001）・core は OS に触れない（ARC-003・symbols 0）・閉じた選択肢の `switch` に `default` なし（CPP-002）・鍵は `g` の表の 2 行（CPP-012）・描画の色は Palette のトークンだけ（ADR 0008 決定 8）。残る穴: `:tabclose` は Vim と違う（E784 / E37 を出さず確認して閉じる・最後の 1 本は窓を閉じる・決定 6）・Ex の相対の引数（`+N` `-N` `$`）・範囲の形・`:tabclose N` と `!`・`:tabnew <file>`・`g<Tab>`・`:tabfirst` `:tablast` は後続（決定 9）・一覧は「∨」の真下ではなく Ctrl+P の面の位置に出て一覧からタブを閉じる操作は無い・一覧の行のクリックでの切り替えとマクロの途中の `gt` の undo の単位は実機で撮っていない（前者は Ctrl+P と同じ `ActivateCommandChoice`・後者は契約）・再生の途中で A → B → A と戻ると A の編集は 2 つの単位になる（Vim は未測）・`v9gt` は VISUAL のまま失敗・大きすぎる数の `:tabnext` は unsupported（どちらも Vim は未測）。

### 5-bu. Ctrl+Tab は最近使った順に歩き Ctrl を離したときに確定する（Issue #248・ADR 0058・施主決定 D23・2026-09-30）

ブランチ `fix/248-ctrl-tab-recent`（main `32e7087` の上・ADR `9a1bd5c`・工程 1 `3ae7dca`・差し戻し `f140fcc`・工程 2 は本節と ADR 0058 の 2 か所だけ）。
仕様の D6 / FR-005 は「Ctrl+Tab は最近使った順」だったが、ADR 0056 で設計席が施主に確かめずに「帯の位置の順」を選び #239 がそのまま入った。施主決定 D23 で最近使った順に直す。

- 工程 1: core に使った順の値 `TabRecency`（`src/core/TabRecency.hpp/.cpp`）と純関数 `tab_recency_touched` `tab_recency_opened` `tab_recency_closed` `tab_recency_walked`。application の `EditorState` が `recency_` と歩きの印 `tab_walk_` を持ち、`StepTab` を `WalkRecentTab` と `SettleRecentTab` に置き換えた。ui は Ctrl+Tab / Ctrl+Shift+Tab を `WalkRecentTab` に写し、`WM_KEYUP`（`VK_CONTROL`）と `WM_KILLFOCUS` で歩いているときだけ `SettleRecentTab` を送る。`eng/window_driver.py` に `press_held_repeat`・`eng/verify-window.py --tabs` の Ctrl+Tab の期待値を使った順に。
- 差し戻し（設計席）: 歩きを続けない意図の前の確定が ui（`EditorWindow::send`）にあったのを `EditorController` の入口（`apply`・`press_vim_key`・`press_vim_keys`）へ移した（ARC-004）。controller を直に叩く契約 `verify_walk_interrupted` と実機の結果が揃い、期待値 4 つを確定の後の順へ直した。
- fixture は oracle がタブを観測できないので不能で、契約で守る（ADR 0058「強制」）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・ui を含む全 target・clang-tidy 込み） | 工程 1・差し戻しとも警告 0・エラー 0（`out/248-step1-build.log` / `out/248-rework-build.log`） |
| `build/nib_tests.exe --tabs` | 248 → **281 checks**（工程 1 `out/248-step1-tabs.log`）→ **288 checks**（差し戻し `out/248-rework-tabs.log`）。工程 1 の途中で 1 件落ちた（22 手の後の列の本数を 4 と書いた期待値の誤り・実装は不変）・差し戻しの途中で 4 件落ちた（`verify_walk_interrupted` の旧期待値） |
| `build/nib_tests.exe`（引数なし）・`ctest --test-dir build -R nib_unit` | 工程 1 **18857 checks 成功**・差し戻し **18864 checks 成功**・どちらも 1 / 1 成功（`out/248-step1-unit.log` `out/248-step1-ctest.log` / `out/248-rework-unit.log` `out/248-rework-ctest.log`） |
| `python eng/protected-diff.py --base origin/main --build --allow=--tabs` | **終了 0**。`fixtures 1853 -> 1853 / changed 0 / added 0 / deleted 0`・`--tabs` の変化だけ allowed・`scopes 25 / same 24`（工程 1 `out/248-step1-protected.log` `out/protected/3ae7dca.json`・差し戻し `out/248-rework-protected.log` `out/protected/f140fcc.json`） |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py --build-dir build` | 工程 1・差し戻しとも **0 violation / 0 violation**（工程 1 `out/248-step1-symbols.log` `out/248-step1-conformance.log`） |
| clang-format --dry-run --Werror（工程 1 の C++ 15 ファイル・差し戻しの 4 ファイル） | 指摘なし |
| 実機（設計席・施主の了承の後・Release `build/release-f140fcc/NeNeNib.exe`・sha256 DF26EFA4…9E476F・125%）`python eng/verify-window.py --tabs --executable build/release-f140fcc/NeNeNib.exe --capture out/248-tabs-frames` | **終了 0**・`driven: true`。起動 1 本 → Ctrl+T × 2 で 3 本・アクティブ 2 → Ctrl+Tab で 1（直前のタブ）→ もう一度で 2（行き来）→ Ctrl を押したまま Tab × 2 で 0（使った順の 3 番目）→ Ctrl+Shift+Tab で 1 → Ctrl+F4 で 2 本・1 → 通常モードの Ctrl+W で 1 本・0（確認なし）→ Vim の Ctrl+W と `/` の入力行の Ctrl+T は 1 本のまま。未保存の 2 本で窓を閉じると確認が 2 回（帯の左から）で「いいえ」「キャンセル」の後に窓が残りファイルは書き換わらない（`out/logs/248-verify-tabs.log`・画は `out/248-tabs-frames/`・記録は `out/reports/done-248-design.md`） |
| `python eng/measure-speed.py --check --executable build/release-f140fcc/NeNeNib.exe`（設計席） | **6 benches checked, 0 regression(s), 0 unmeasurable**（機械 bc8a356f37c68491・5 回・startup-first-frame 219.4 ms・startup-window-shown 37.2 ms・key-to-frame-single 0.890 ms・key-to-frame-burst-200 3.441 ms・open-large-file-16mib 270.1 ms・key-to-frame-burst-200-16mib 6.271 ms・`out/speed/2026-09-29T16-14-05Z.json`。各ベンチの 1 回目に外れ値があり、起動の 2 本の中央値は #239 の計測（195.4 ms / 32.3 ms）より大きいがゲートの基準内・本 Issue の差分は起動の経路に触れていない） |
| 工程 2（本節の追記と ADR 0058 の「強制」の `planned` → `active` と決定 4 の 1 文だけ）の `python eng/conformance.py --build-dir build`・`git diff --check` | **0 violation**・指摘なし |

対象を限定した理由: 差分は core の使った順の値と純関数・application の意図 2 つと状態の 2 欄と controller の入口の確定・ui/win32 の Ctrl の上げとフォーカスの喪失・`eng/window_driver.py` と `eng/verify-window.py --tabs`・単体テストである。既存の fixture は不変（protected-diff で changed 0）。本物の Ctrl の押し離しは契約で覆えないので設計席が実機で確かめた。速さは切り替えの経路に触れるので設計席が測った。工程 2 は文書だけで、実装・テスト・依存が不変なので工程 1・差し戻しの成功結果と設計席の実機・速さの結果（`f140fcc` の Release）を再利用する（QLT-001 / QLT-012 / QLT-014・ADR 0021）。

ARC-001 / ARC-003 / ARC-004 / CPP-002 / CPP-003 / CPP-007 / CPP-011 / CPP-012 / CPP-017 を自己レビュー。使った順は application の `EditorState` の 1 欄で順を直すのは帯の本数とアクティブを変える 3 か所だけ（ARC-001 / ARC-004）・core は OS に触れない（ARC-003・symbols 0）・閉じた選択肢の `switch` に `default` なし（CPP-002・`key_message` の既定分岐は `DefWindowProcW` へ渡す CPP-017 の例外）。残る穴: 歩いている間に出す一覧の面は無い（Ctrl+Tab の行き先は帯を見ただけでは分からない・決定 7）・Ctrl+PageDown / Ctrl+PageUp・Ctrl+1〜9 は後続・`WM_KILLFOCUS` の確定は実機の検査に入っていない（契約は `SettleRecentTab` まで）・左右の Ctrl を両方押して片方を離すとそこで確定する・歩きの途中の IME の変換も確定する・起動の 2 本の中央値が上がった理由は調べていない。

### 5-bv. 窓を閉じるときに開いていたタブを `session.v1` に覚える（Issue #252・ADR 0059 決定 1〜3・7・施主決定 D24〜D27・2026-09-30）

ブランチ `feat/252-session-remember`（main `43f8baf` の上・ADR `1f41f02`・工程 1 `afd027e`・工程 2 `00660f2`・工程 3 は本節と ADR 0059 の 2 か所と `docs/todo/current.md` の数字の 1 行だけ）。
覚えるだけの 1 本目（ADR 0059 決定 8）。一覧を読んで戻すのは #253 で、この Issue の後の main では `session.v1` は書かれるだけ。

- 工程 1: application の値 `Session` `SessionTab`・port `SessionPort`・閉じた enum `SessionFailure` `SessionEnd`・意図 `EndSession`。`EditorController` が `EndSession` のときだけ状態から一覧を作り（無題を入れない・帯の順・`active` は一覧の中の位置・使った順の順位を詰め直す・`last_tab_closed` は空の一覧）`SessionPort::write` へ 1 回渡して結果を捨てる。契約は新しい scope `--session`（`tests/unit/SessionTests.cpp`・`TabsTests.cpp` が 1580 行あるため）。
- 工程 2: adapters の `SessionCodec`（`version=1`・`active=`・1 行 `<行>,<桁>,<画面の先頭の行>,<順位>,<パス>`・拒否は全体・256 タブ・1 MiB）と `Win32SessionAdapter`（`beside_local_settings("session.v1")`・書きは `FilePort::write`）・`ensure_parent_directory` と `rooted_path_text` を設定と共用の 1 本に。ui は窓を壊す直前の 1 本 `leave`（× で閉じる・最後の 1 本の `close_tab`）と `WM_ENDSESSION` で `controller_.apply(EndSession{…})` を直に呼ぶ。`eng/window_driver.py` の `start` は起動の前に道具の profile の `session.v1` を消す（本物の `LOCALAPPDATA` と同じ場所では消さない・`keep_session=True` で残す）。契約は `tests/adapters/SessionAdapterTests.cpp`（CTest `nib_sessions`）。
- fixture は oracle の対象ではないので不能で、契約で守る（ADR 0059「強制」）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・ui を含む全 target・clang-tidy 込み） | 工程 1・工程 2 とも exit 0・警告 0（`out/252-step1-build.log` / `out/252-step2-build.log`）。工程 1 の途中で tidy が `session_of` の認知複雑度と unchecked optional を 3 件、工程 2 の途中で試験の関数の引数 5 個と unchecked optional の 2 件とコンパイルの `core::` の付け忘れ 1 件を落とし、直した |
| `build/nib_tests.exe --session` | 新規 **38 checks**（工程 1 `out/252-step1-session.log`・工程 2 `out/252-step2-session.log`） |
| `build/nib_tests.exe --tabs` | **288 checks**（工程 1 `out/252-step1-tabs.log`） |
| `build/nib_tests.exe`（引数なし） | 18864 → **18902 checks 成功**（工程 1 `out/252-step1-default.log`・工程 2 `out/252-step2-default.log`） |
| `ctest --test-dir build -R nib_unit`（工程 1）・`ctest --test-dir build -R "nib_sessions\|nib_adapters\|nib_themes\|nib_unit"`（工程 2） | 1 / 1・**4 / 4 成功**（`out/252-step1-ctest.log` / `out/252-step2-ctest.log`）。`nib_adapters` と `nib_themes` は設定とテーマの場所・親のフォルダの関数を 1 本にした退行を見るため |
| `build/nib_session_tests.exe` / `build/nib_adapter_tests.exe` / `build/nib_theme_tests.exe`（工程 2） | **472 checks, 0 failures** / **113 checks passed** / **421 checks, 0 failures**（`out/252-step2-sessions.log` `out/252-step2-adapters.log` `out/252-step2-themes.log`） |
| `python eng/protected-diff.py --base origin/main --build --allow --session`（各工程の commit 後） | **終了 0**。`fixtures 1853 -> 1853 / changed 0 / added 0 / deleted 0`・`scopes 26 / same 25`・`--session` の新規 38 だけ allowed（`out/252-step1-protected.log` / `out/252-step2-protected.log`） |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py --build-dir build` | 工程 1・工程 2 とも **2 libraries・0 violation / 0 violation**（`out/252-step1-symbols.log` `out/252-step1-conformance.log` / `out/252-step2-symbols.log` `out/252-step2-conformance.log`） |
| clang-format --dry-run --Werror（工程 1 の C++ 20 ファイル・工程 2 の adapters/win32 の全ファイルと `Main.cpp` `EditorWindow.*` と新しい試験） | 指摘なし（`out/252-step2-format.log`） |
| `python -m py_compile eng/window_driver.py` と scratchpad の使い捨ての確認（`subprocess.Popen` を替え玉にして exe は起動しない） | ok・既定で消す / `keep_session=True` で残す / 本物と同じ場所では消さない / 綴りの違う同じ場所でも消さない / 無い・環境に無いは何もしない、の 6 件 ok（`out/252-step2-driver.log`） |
| 実機（設計席・施主の了承の後・Release `build/release-00660f2/NeNeNib.exe`・sha256 C78046B1…C8F875FF・125%・`eng/window_driver.py` を使う 1 回限りのスクリプト・profile は `out/window-verification/session-252/profile`） | ファイル 3 つで起動 → 3 本目で下 2・右 3 → 1 本目をクリック → 下 1 → 窓を閉じる: `version=1`・`active=0`・`2,1,1,0,…` `1,1,1,2,…` `3,4,1,1,…`。ファイル 1 つ → そのタブの ×（D22）: 空の一覧。引数なし → 文字を打つ → 閉じる → 「いいえ」: 空の一覧（無題は入らない・D9）。ファイル 2 つ → 「＋」で無題がアクティブ → 閉じる: 2 行・`active=1`。どの回も終了コード 0（記録は `out/reports/done-252-design.md`） |
| `python eng/measure-speed.py --check --executable build/release-00660f2/NeNeNib.exe`（設計席） | **6 benches checked, 0 regression(s), 0 unmeasurable**（機械 bc8a356f37c68491・5 回・startup-first-frame 200.8 ms・startup-window-shown 33.1 ms・key-to-frame-single 1.000 ms・key-to-frame-burst-200 3.699 ms・open-large-file-16mib 258.0 ms・key-to-frame-burst-200-16mib 7.228 ms・`out/speed/2026-09-29T17-34-25Z.json`）。#248 の計測で上がっていた起動の 2 本（219.4 ms / 37.2 ms）は今回 200.8 ms / 33.1 ms で、機械の状態の揺れだったと見る |
| 工程 3（本節の追記と ADR 0059 の「強制」の #252 の分の `planned` → `active` と決定 2 の 1 文と `current.md` の 1 行だけ）の `python eng/conformance.py --build-dir build`・`git diff --check`・`eng/validate-git.ps1` | **0 violation**（`out/252-step3-conformance.log`）・`git diff --check` 指摘なし・`eng/validate-git.ps1` passed |

対象を限定した理由: 差分は application の値と port と意図と controller の 1 意図・adapters の一覧の形式と adapter と設定と共用の場所と親のフォルダの関数・ui/win32 の窓を壊す直前の 1 本と `WM_ENDSESSION`・`Main.cpp` の合成・`eng/window_driver.py`・単体テストと adapter の試験である。既存の fixture は不変（protected-diff で changed 0）。設定とテーマの場所の関数を寄せたので `nib_adapters` と `nib_themes` を回した。窓を閉じる本物の流れは契約で覆えないので設計席が実機で確かめた。速さは起動と窓の出口に触れるので設計席が測った。工程 3 は文書だけで、実装・テスト・依存が不変なので工程 1・2 の成功結果と設計席の実機・速さの結果（`00660f2` の Release）を再利用する（QLT-001 / QLT-012 / QLT-014・ADR 0021）。

ARC-001 / ARC-003 / ARC-007 / ARC-010 / CPP-002 / CPP-003 / CPP-004 / CPP-005 / CPP-009 / CPP-011 / CPP-017 / QLT-013 を自己レビュー。一覧を作るのは controller の `EndSession` の 1 か所で 1 打鍵の道には何も足していない（ARC-001）・core と application は OS とファイルに触れない（ARC-003 / ARC-007・symbols 0）・期待される失敗は `SessionFailure` の結果型（ARC-010 / CPP-005）・`SessionEnd` の `switch` に `default` なし（CPP-002）。残る穴: 一覧を読んで戻すのは #253・`WM_ENDSESSION`（サインアウト・シャットダウン）の実機の確認はしていない（未保存の変更があるまま OS が終了したときも確認を出さずに書く）・強制終了のときは前に窓を閉じたときの一覧が残る（決定 3）・窓を 2 つ開いていたら後から閉じたほうの一覧だけが残る・`LocalSettingsPath` の絶対の検査を UTF-8 の後へ移したので、先頭が非 ASCII で 2 文字目が `:` の `LOCALAPPDATA` だけ拒むようになる（Windows のパスとして成り立たない形）。

### 5-bw. ファイルを指定せずに起動したとき前回のタブを戻し見るときに読む（Issue #253・ADR 0059 決定 4〜7・施主決定 D24〜D27・2026-09-30）

ブランチ `feat/253-session-restore`（main `02e4c5c` の上・ADR `94d986a`・工程 1 `2a19d54`・工程 2 `e2f4936`・工程 4 は本節と ADR 0059 の「強制」の 4 行と `docs/todo/current.md` の数字の 1 行だけ）。
戻す 2 本目（ADR 0059 決定 8）。引数なしの起動で `session.v1` のタブを帯に並べ、読むのは見ていたタブだけ。ほかのタブは見るときに読む（D27）。

- 工程 1: application の型 `UnloadedDocument` と `ParkedTab`（`std::variant` の 2 つの形）・`EditorState` の `unloaded_at` `with_restored` `with_loaded` `with_dropped` `with_recency_ranked`・読む 1 本 `EditorController::reach_tab`（開く経路と同じ読み・位置は読んだ本文の範囲へ寄せる・読めなければ帯から外して「開けませんでした: <名前>（ほか N 件）」）・起動の手順 `restore_session` / `restore_tabs`（引数があるときは一覧を読まない・D24）。途中で `std::ranges::stable_sort` が `eng/symbols.py` の ARC-003 に落ちたので、許可の表は触らず選び出しに書き直した。
- 工程 2: `accept(SwitchTab)`（クリック・Ex `:tabnext`・Vim `gt` `gT`・一覧・同じファイルを開く）・`accept(WalkRecentTab)`・`accept(CloseTab)` の後の隣（新しい `close_active_tab`）で `reach_tab` を呼ぶ。読めなければ何も切り替えず、入力行と変換中の文字列を閉じない。`eng/verify-window.py --restore`（窓へ post するクリックと WM_CLOSE だけ）。
- fixture は oracle の対象ではないので不能で、契約で守る（ADR 0059「強制」）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・全 target・clang-tidy 込み） | 工程 1・工程 2 とも exit 0・警告 0（`out/253-step1-build.log` / `out/253-step2-build.log`） |
| `build/nib_tests.exe --session` | 38 → **131 checks**（工程 1 `out/253-step1-session.log`）→ **199 checks**（工程 2 `out/253-step2-session.log`。途中 1 回 `verify_list_after_reaching` が `VisibleLines` を置いていなかったため落ち、試験に `VisibleLines{3}` を足した） |
| `build/nib_tests.exe --tabs` | **288 checks**（`out/253-step1-tabs.log` / `out/253-step2-tabs.log`） |
| `build/nib_tests.exe`（引数なし） | 18902 → **18995**（工程 1 `out/253-step1-default.log`）→ **19063 checks 成功**（工程 2 `out/253-step2-default.log`） |
| `ctest --test-dir build -R "nib_unit\|nib_sessions"` | 工程 1・工程 2 とも **2 / 2 成功**（`out/253-step1-ctest.log` / `out/253-step2-ctest.log`） |
| `python eng/symbols.py --build-dir build --require core application` / `python eng/conformance.py --build-dir build` | 工程 1・工程 2 とも **2 libraries・0 violation / 0 violation**（`out/253-step{1,2}-symbols.log` `out/253-step{1,2}-conformance.log`） |
| clang-format --dry-run --Werror（工程 1 の変更・追加 10 ファイル・工程 2 の変更 3 ファイル） | 指摘なし |
| `python -m py_compile eng/verify-window.py eng/window_driver.py`（工程 2） | exit 0（`out/253-step2-pycompile.log`） |
| `python eng/protected-diff.py --base origin/main --build --allow --session --allow --tabs`（各工程の commit 後） | **終了 0**。`fixtures 1853 -> 1853 / changed 0 / added 0 / deleted 0`・`scopes 26 / same 25`・`--session` 38 → 131 / 38 → 199 だけ allowed（`out/253-step1-protected.log` / `out/253-step2-protected.log`） |
| 実機（設計席・施主の了承の後・Debug `build/NeNeNib.exe`・`e2f4936`・125%・`python eng/verify-window.py --restore --capture out/frames-253`） | **exit 0**（`out/253-accept-restore.log`）。送ったのは窓へ post するクリックと WM_CLOSE だけ。(a) 引数 3 本で起動 → 2 本目 → 行番号の 3 行目 → 閉じる: 終了コード 0・`session.v1` が書かれた。(b) 引数なしで起動: 帯 3 本・アクティブ 2 本目・題名 `restore-second.txt - NeNe Nib`・CURRENT_LINE とキャレットが 3 行目（`restoreBack.png`・「行 3, 桁 1」）。(c) 3 本目をクリック: 題名 `restore-third.txt - NeNe Nib`（`restoreThird.png`）。(d) `restore-first.txt` を消して起動 → 1 本目をクリック: 帯 2 本・アクティブは third のまま・ステータスバーに「開けませんでした: restore-first.txt」・窓は止まらない（`restoreMissing.png`・D26）。(e) `restore-second.txt` を引数に起動: 帯 1 本（D24）。記録は `out/reports/done-253-design.md` |
| `python eng/measure-speed.py --check --executable build/release-e2f4936/NeNeNib.exe`（設計席・Release sha256 8F478684…14F3C106） | **6 benches checked, 0 regression(s), 0 unmeasurable**（機械 bc8a356f37c68491・5 回・startup-first-frame 206.7 ms・startup-window-shown 33.2 ms・key-to-frame-single 0.848 ms・key-to-frame-burst-200 3.370 ms・open-large-file-16mib 251.7 ms・key-to-frame-burst-200-16mib 6.279 ms・`out/speed/2026-09-30T07-20-35Z.json`・`out/253-accept-speed.log`） |
| 工程 4（本節と ADR 0059 の「強制」の #253 の分と `current.md` の 1 行だけ）の `python eng/conformance.py --build-dir build`・`git diff --check`・`eng/validate-git.ps1` | **0 violation**（`out/253-step4-conformance.log`）・`git diff --check` 指摘なし・`eng/validate-git.ps1` passed（`out/253-step4-git.log`） |

タブ 20 本の一覧で起動した時間（ADR 0059 決定 7・設計席が手で測る・QLT-014）。同じ Release の exe を `--measure` で起動して閉じる 1 回限りのスクリプト（`eng/window_driver.py` の `start`）で、3 つの profile を交互に 9 回ずつ。ファイルはどれも 1 MiB・見ていたタブはどちらも `tab-20.txt`・最前面の要求なし。記録 `out/speed-253/restore-startup-2.json`。

| 条件 | 最初のフレームまで（中央値・最小〜最大） | 窓が見えるまで | `document_opened` の区間 | `frame_presented` の区間 |
| --- | --- | --- | --- | --- |
| 一覧なし（空の無題） | 198.1 ms（194.2〜206.5） | 31.7 ms | 0.19 ms | 4.18 ms |
| 一覧 1 本 | 202.7 ms（200.2〜207.7） | 36.2 ms | 3.49 ms | 4.76 ms |
| 一覧 20 本 | 203.4 ms（199.7〜205.1） | 35.3 ms | 3.53 ms | 4.94 ms |

1 本と 20 本の差は 0.7 ms で揺れの中、読む区間は 3.49 ms と 3.53 ms で同じ（見ていた 1 本だけを読む・D27）。起動の重さはタブの数に依らない。1 回目（区間の内訳なし・`restore-startup.json`）は 1 本と 20 本に 5 ms の差が出たが、内訳を取った 2 回目で再現しなかった（`device_created` の約 157 ms の揺れ）。
2026-09-30 の 04:19〜04:44 ごろ OS が異常終了した（Kernel-Power 41・失われたものは無い）ので、実機の確認と速さはその再起動の後に回した。

対象を限定した理由: 差分は application の状態と controller の起動・切り替え・歩き・閉じるの入口・単体テスト・`eng/verify-window.py` の新しい節で、adapters と ui/win32 と core は変えていない（`Main.cpp` はコメント 1 か所）。既存の fixture は不変（protected-diff で changed 0）。タブの切り替えに触れるので `--tabs` を回した。起動の本物の流れと知らせの描画は契約で覆えないので設計席が実機で確かめ、起動の出口に触れるので速さとタブ 20 本の起動を設計席が測った。工程 4 は文書だけで、実装・テスト・依存が不変なので工程 1・2 の成功結果と設計席の実機・速さの結果（`e2f4936`）を再利用する（QLT-001 / QLT-012 / QLT-014・ADR 0021）。

ARC-001 / ARC-003 / ARC-010 / CPP-002 / CPP-003 / CPP-004 / CPP-011 / CPP-012 / QLT-013 / QLT-014 を自己レビュー。読むのは `reach_tab` の 1 本で起動と切り替え・歩き・閉じた後の隣が共用（ARC-001）・core と application は OS とファイルに触れない（ARC-003・symbols 0）・読めないのは結果型から知らせへ写し例外にしない（ARC-010）・`ParkedTab` を読む所はどれも `std::visit`（CPP-002）。残る穴: 起動の重さがタブの数に依らないことはベンチに足すまで機械では守られていない（ADR 0059「強制」は planned）・無くなったファイルに気づくのはそのタブを見るとき（D27）・Vim の `gt` `gT` の行き先が読めなかったとき engine は再生を打ち切らない（決定 5）・読めなかったタブを外したとき帯の上のマウスの印は位置のまま残る（次のマウスの移動で直る）・通常モードでは覚えたカーソルが結合文字の間に置かれうる・強制終了のときは前に窓を閉じたときの一覧が残る（決定 3。今回の OS の異常終了がその例）。

### 5-bx. Ctrl+P を開くと開いているタブの一覧が出て行頭の記号で出どころを絞る（Issue #258・ADR 0060 決定 1〜6・9・施主決定 D28〜D30・2026-09-30）

ブランチ `feat/258-palette-file-list`（main `0fe30b8` の上・ADR `3069393`・工程 1 `3371ead`・工程 2 `590e512`・差し戻し `6dbfe63`・工程 3 は本節と ADR 0060 の「強制」の #258 の分と `docs/todo/current.md` の数字の 2 行と README の FR-006 の 1 項目だけ）。
統合の一覧の土台（ADR 0060 決定 11 の 1 本目）。Ctrl+P は空の入力で開いて開いているタブを出し、入力の行頭の `#` はタブだけ、`:` は設定のコマンドを出す。履歴（`@`）は #259。

- 工程 1: core の出どころの記号の表 `palette_marks`（`#` タブ・`:` 設定）・`palette_query_of`・`palette_mark_hint`・閉じた enum `PaletteScope` `PaletteOrigin`・`CommandChoice.origin`・`tab_list_choices` を置き換える `listed_choices`（名前 → 場所＋名前に罰点・同点は列の順・`std::stable_sort` は使わない）・`CommandPalette::opened` の 1 本（`CommandPaletteSource` は削除）・controller の `palette_entries()` と 2 つの入口（Ctrl+P は入力が空・「∨」と `:tabs` は `#` でアクティブのタブ）。途中で clang-tidy の `readability-function-size` と `readability-function-cognitive-complexity` に落ちたので、閾値は触らず `std::ranges::find` と `in_score_order` へ切り出した。
- 工程 2: 配置の純関数 `palette_row_note` `palette_row_label` `palette_input_hint`（整数演算だけ）・`CommandPaletteView.hint`（入力が空のときだけ）・描画の `write_right`（書式は `status_format_`・色は `muted`・新しい書式と色は無し）。
- 差し戻し 1 回（`6dbfe63`）: 設計席の 1 回目の画で、行の右端の補足が「開い」で切れ入力が空のときの案内が見えなかった。原因は、もともと右寄せ（TRAILING）の `status_format_` に `write_right` が幅を測って原点をさらに右へずらしていたこと（右寄せが二重）。右寄せは text layout の TRAILING に任せ、原点を欄の左端に置いた。
- fixture は oracle の対象ではないので不能で、契約で守る（ADR 0060「強制」）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・全 target・clang-tidy 込み） | 工程 1・工程 2・差し戻しとも成功・警告 0（`out/258-step1-build.log` / `out/258-step2-build.log` / `out/258-rework1-build.log`） |
| `build/nib_tests.exe --command-palette` | 146 → **189 checks**（工程 1）→ **207 checks**（工程 2）・差し戻しの後も **207 checks** 通過（`out/258-rework1-tests.log`） |
| `build/nib_tests.exe --tabs` | 288 → **289 checks**（工程 1） |
| `build/nib_tests.exe --ex-settings` / `--session`（工程 1） | 195 → 195 / 199 → 199 |
| `build/nib_tests.exe`（引数なし） | 19063 → **19107**（工程 1 `out/258-step1-tests.log`）→ **19125 checks 成功**（工程 2 `out/258-step2-tests.log`） |
| `ctest --test-dir build -R nib_unit` | 工程 1・工程 2 とも **1 / 1 成功** |
| `python eng/symbols.py --build-dir build --require core application` | 工程 1・工程 2 とも **2 libraries・0 violation**（`std::make_shared` と `std::ranges::find` は通る・`out/258-step{1,2}-symbols.log`） |
| `python eng/conformance.py --build-dir build` | 工程 1・工程 2・差し戻しとも **0 violation**（`out/258-step{1,2}-conformance.log` / `out/258-rework1-conformance.log`） |
| clang-format --dry-run --Werror（工程 1 の変更 16 ファイル・工程 2 の変更 7 ファイル・差し戻しの `Direct2DRenderer.cpp`） | 指摘なし（`out/258-rework1-format.log`） |
| `python eng/protected-diff.py --base origin/main --build --allow --command-palette --allow --tabs`（工程 1・工程 2 の commit 後） | **終了 0**。`fixtures 1853 -> 1853 / changed 0`・`scopes 26 / same 24`・`--command-palette` 146 → 189 / 146 → 207 と `--tabs` 288 → 289 だけ allowed（`out/protected/3371ead.json` / `out/protected/590e512.json`） |
| `pwsh -NoProfile -File eng/validate-git.ps1` | 工程 1・工程 2・差し戻しとも passed（`out/258-step{1,2}-validate-git.log` / `out/258-rework1-validate-git.log`） |
| 実機（設計席・施主の了承の後・Debug `build/NeNeNib.exe`・`6dbfe63`・125%・`eng/window_driver.py` を使う 1 回限りのスクリプト `D:\NeNeNib\scripts\palette_frames.py`） | 終了コード 0（`out/258-accept-frames.log`）。クリック・文字・鍵はどれも窓へ post（本物のキーボードとポインタは使っていない）。ファイル 3 つ（1 つは下位のフォルダ `notes`）で起動 → トグルで Vim → `:tabs` と Enter → Backspace → 文字。画は `out/frames-258/`・記録は `out/reports/done-258-design.md` |
| 工程 3（本節と ADR 0060 の「強制」と `current.md` の 2 行と README の 1 項目だけ）の `python eng/conformance.py --build-dir build`・`git diff --check`・`eng/validate-git.ps1` | **0 violation**（`out/258-step3-conformance.log`）・`git diff --check` 指摘なし・`eng/validate-git.ps1` passed（`out/258-step3-git.log`） |

設計席の実機の画（2 回目・`6dbfe63`）:

| 画 | 操作 | 見えたもの |
| --- | --- | --- |
| `1-tab-list-mark.png` | `:tabs` と Enter | 入力欄に `#`・タブ 3 本が帯の順・選ばれているのはアクティブの 3 本目・「3 / 3」・行の右端に「開いているタブ」 |
| `2-all-empty-input.png` | Backspace（`#` を消す） | 入力が空・全部の候補・先頭が選ばれる・検索欄の右に「# タブ　: 設定」 |
| `3-query-name.png` | `sec` | `palette-second.md` の 1 件 |
| `4-query-place.png` | `notes` | 場所に当たった候補 2 件（`notes` のフォルダの 1 件が先） |
| `5-commands.png` | `:` | 設定のコマンド 22 件（補足なし・案内なし・今までと同じ一覧） |
| `6-after-enter.png` | `:` を消す → 下 → Enter | 2 本目のタブへ切り替わる（題名 `palette-second.md - NeNe Nib`） |

長い場所は「…」で切れ、補足の欄に重ならない。選択中の行の上でも補足は読める。1 回目（`590e512`）の画で見つかった右寄せの二重は上の差し戻しで直した。

回していないものと理由: 速さの 6 本（差分は面が開いているときの候補の組み立てと描画だけで、起動と本文の打鍵の経路に触れていない・QLT-014）・本物の Ctrl+P の鍵（鍵から意図 `OpenCommandPalette` への経路の ui は変えていない。意図の後は契約 `verify_palette_entries_controller` が覆う）・「∨」のクリック（経路 `OpenTabList` は `:tabs` と同じ意図で ui は変えていない）。

対象を限定した理由: 差分は core の出どころの表・絞り込み・面の形と配置、application の面の入口と view、ui/win32 の面の描画、単体テストで、adapters と起動と本文の編集の経路は変えていない。既存の fixture は不変（protected-diff で changed 0）で、変わった scope は許可した `--command-palette` と `--tabs` だけ。面の描画は契約で覆えないので設計席が実機の画で確かめた。工程 3 は文書だけで、実装・テスト・依存が不変なので工程 1・2 と差し戻しの成功結果と設計席の実機の結果（`6dbfe63`）を再利用する（QLT-001 / QLT-012 / QLT-014・ADR 0021）。

ARC-001 / ARC-003 / ARC-010 / CPP-002 / CPP-004 / CPP-011 / CPP-012 / ADR 0008 決定 8 / QLT-013 / QLT-014 を自己レビュー。出どころの記号を引くのは `palette_query_of` の 1 本・候補の列を作るのは `palette_entries()` の 1 本で Ctrl+P と「∨」と `:tabs` が共用（ARC-001）・core と application は OS とファイルに触れない（ARC-003・symbols 0）・`PaletteScope` `PaletteOrigin` の `switch` に `default` は無い（CPP-002）・色は今のトークンだけ（ADR 0008 決定 8）。残る穴: 履歴は #259 で、今の Ctrl+P の一覧に出るファイルは開いているタブだけ・設定のコマンドは Ctrl+P の後に `:` を打って出す・「∨」の一覧の入力欄に `#` が見える・`:` を消した後は Ex の前方一致の補完が出ない（面では Tab が選択の上下なので見える違いは無い）・照合はバイト単位のまま（日本語の名前の照合は面の中の日本語入力の Issue）・候補は 1 つの意図で 2 回以上作り直す（タブ 256 本までなら問題にならない）・面が開いている間にタブが切り替わっても列は開いたときのまま・`palette_origin_label` の宣言は `PaletteOrigin.hpp` で定義は `CommandChoice.cpp`（#259 で寄せるか決める）・面の実機の検査は `eng/verify-window.py` に無い（ADR 0060 決定 12 の後続）。

### 5-by. 閉じたファイルを履歴に覚え Ctrl+P の一覧から開く（Issue #259・ADR 0060 決定 7・8・施主決定 D28〜D30・2026-09-30）

ブランチ `feat/259-palette-history`（main `9246767` の上・工程 1 `6d36aa8`・工程 2 `d9d0203`・工程 3 は本節と ADR 0060 の決定 8 の型の名前と「強制」の #259 の分と `docs/todo/current.md` の数字の 2 行と README の FR-006 の 1 項目と `docs/PROJECT_LAYOUT.md` の利用者データの 1 行だけ）。
統合の一覧の 2 本目（ADR 0060 決定 11）。パスのあるタブを閉じたときと窓が閉じるときに、そのファイルを `history.v1` に新しい順で 100 件まで覚え、Ctrl+P の面でタブの後ろに出す。行頭の `@` は履歴だけ。選ぶと開く道で開き、無くなったファイルは 1 行知らせて履歴から外す。

- 工程 1: application の値 `FileHistory`・port `HistoryPort`・閉じた enum `FileHistoryFailure`・記録の純関数 `history_recorded` `history_forgotten`（`same_file` で比較・100 件で切る）・`EditorPorts` の `HistoryPort &history`、adapters の `HistoryCodec`（`version=1` の次に 1 行 1 パス・BOM と CRLF を受ける・1 MiB と行数の上限）と `Win32HistoryAdapter`（`local_history_path` は `beside_local_settings("history.v1")`）、`session.v1` と共用する行の割り方 `TextLines`、`Main.cpp` の合成、替え玉 `ScriptedHistory`、`eng/window_driver.py` が `history.v1` も消す。製品の動きは不変。
- 工程 2: core の `PaletteScope::history` `PaletteOrigin::history` `CommandChoiceKind::open` と記号の表の `@`（案内「# タブ　@ 履歴　: 設定」）、controller の `open_document` `open_listed` `remember` `forget`、`CloseTab` と `EndSession` で記録（`window_closed` は最後に見ていたタブが先頭・`last_tab_closed` はその 1 本）、`palette_entries()` に履歴（開いているタブと同じファイルは除く・読めなければ履歴なしで知らせない）、`submit_palette` の `open`（`not_found` だけ履歴から外す）。
- 逸脱を 1 つ受理（設計席）: 失敗の enum は ADR の `HistoryFailure` ではなく `FileHistoryFailure`。core に undo の端に使う同じ名前の型（`src/core/HistoryFailure.hpp`）があり include が取り違えられてビルドが落ちたため。ADR 0060 の決定 8 の文言は工程 3 で直した。
- fixture は oracle の対象ではないので不能で、契約で守る（ADR 0060「強制」）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・全 target・clang-tidy 込み） | 工程 1・工程 2 とも成功・警告 0（`out/259-step1-build.log` / `out/259-step2-build.log`。工程 2 は途中で clang-tidy の入れ子 3 段を 2 か所で直した） |
| `build/nib_tests.exe --history` | 新規 **23 checks**（工程 1 `out/259-step1-history.log`）→ **38 checks**（工程 2） |
| `build/nib_tests.exe --command-palette` | 207 → **222 checks**（工程 2） |
| `build/nib_tests.exe --session` / `--tabs`（工程 2） | 199 → 199 / 289 → 289 |
| `build/nib_tests.exe`（引数なし） | 19125 → **19148**（工程 1 `out/259-step1-unit.log`）→ **19178 checks 成功**（工程 2 `out/259-step2-unit.log`） |
| `ctest --test-dir build -R "nib_unit\|nib_histories\|nib_sessions\|nib_adapters"`（工程 1） | **4 / 4 成功**。`nib_history_tests` **1210 checks**・0 failures／`nib_session_tests` 472 checks・0 failures（`TextLines` への移し替えの退行なし・`out/259-step1-ctest.log`） |
| `ctest --test-dir build -R "nib_unit\|nib_histories"`（工程 2） | **2 / 2 成功**（`out/259-step2-ctest.log`） |
| `python eng/symbols.py --build-dir build --require core application` | 工程 1・工程 2 とも **2 libraries・0 violation**（`out/259-step{1,2}-symbols.log`） |
| `python eng/conformance.py --build-dir build` | 工程 1・工程 2 とも **0 violation**（`out/259-step{1,2}-conformance.log`） |
| clang-format --dry-run --Werror（工程 1 の変更・新規の C++ 全部・工程 2 の変更 12 ファイル） | 指摘なし（`out/259-step{1,2}-format.log`） |
| `python -m py_compile eng/window_driver.py` と使い捨ての確認（`subprocess.Popen` を替え玉・exe は起動しない）（工程 1） | 成功・**6 / 6 ok**（既定で `session.v1` と `history.v1` を消す・`keep_session=True` で両方残す・自分の `LOCALAPPDATA` では消さない ほか・`out/259-step1-driver.log`） |
| `python eng/protected-diff.py --base origin/main --build --allow --history`（工程 1）／`--allow --history --allow --command-palette`（工程 2） | **終了 0**。`fixtures 1853 -> 1853`・保護ファイル不変・`--history` 新規 23 / 38 と `--command-palette` 207 → 222 だけ allowed（`out/protected/6d36aa8.json` / `out/protected/d9d0203.json`） |
| `pwsh -NoProfile -File eng/validate-git.ps1` | 工程 1・工程 2 とも passed（`out/259-step1-validate-git.log` / `out/259-step2-git.log`） |
| 実機（設計席・施主の了承の後・Debug `build/NeNeNib.exe`・`d9d0203`・125%・`eng/window_driver.py` を使う 1 回限りのスクリプト `D:\NeNeNib\scripts\history_frames.py`） | 2 回とも終了コード 0（`out/259-accept-frames.log`）。クリック・文字・鍵はどれも窓へ post（本物のキーボードとポインタは使っていない）。タブを閉じるのは Vim の `:tabclose`、面を開くのは `:tabs` の後に Backspace。profile と文書は `D:\NeNeNib\evidence\frames-259\`（HDD）。画は `out/frames-259/`・記録は `out/reports/done-259-design.md` |
| 速さ（設計席・`python eng/measure-speed.py --check --executable build/release-d9d0203/NeNeNib.exe`） | **6 benches checked, 0 regression(s), 0 unmeasurable**（Release sha256 B87AEF5B…CF64BF12D・`out/release/d9d0203.json`・機械 bc8a356f37c68491・5 回・`out/speed/2026-09-30T09-46-23Z.json`・`out/259-accept-speed.log`） |
| 工程 3（文書だけ）の `python eng/conformance.py --build-dir build`・`git diff --check`・`eng/validate-git.ps1` | **0 violation**（`out/259-step3-conformance.log`）・`git diff --check` 指摘なし・`eng/validate-git.ps1` passed（`out/259-step3-git.log`） |

設計席の実機の画（2 回目・`d9d0203`。1 回目は待ち時間 0.6 秒で 4 枚目が知らせの出る前の画になったため、1.5 秒にして撮り直した。Debug の exe と HDD の上の profile では履歴の書き込みに数百 ms かかる）:

| 画 | 操作 | 見えたもの・`history.v1` |
| --- | --- | --- |
| `1-tabs-then-history.png` | ファイル 3 つで起動 → 3 本目を閉じる → 面を開いて `#` を消す | タブ 2 本（「開いているタブ」）の後ろに `palette-third.txt`（「履歴」）・案内は「# タブ　@ 履歴　: 設定」・`history.v1` は third の 1 行 |
| `2-history-only.png` | `@` | 履歴の 1 件だけ（1 / 1） |
| `3-opened-from-history.png` | Enter | 3 本目が新しいタブで開く（題名 `palette-third.txt - NeNe Nib`） |
| `4-missing-notice.png` | 3 本目を閉じる → ファイルを消す → 面で `@` と Enter | タブは 2 本のまま・ステータスバーの左に「開けませんでした: palette-third.txt」・ダイアログなし・`history.v1` は `version=1` だけ（外れた） |
| `5-history-after-drop.png` | 面で `@` | 「候補なし」（0 / 0） |
| `6-next-start.png` | 窓を閉じる → `palette-first.txt` を指定して起動（一覧と履歴を残す）→ 面を開いて `#` を消す | タブ 1 本の後ろに `palette-second.md`（「履歴」）。窓を閉じたときの `history.v1` は second（最後に見ていた）→ first の順 |

速さの 6 本（設計席・`d9d0203` の Release・中央値）:

| ベンチ | 中央値 |
| --- | --- |
| startup-first-frame | 208.8 ms |
| startup-window-shown | 34.3 ms |
| key-to-frame-single | 0.936 ms |
| key-to-frame-burst-200 | 3.545 ms |
| open-large-file-16mib | 257.7 ms |
| key-to-frame-burst-200-16mib | 7.210 ms |

起動の内訳の `document_opened` は 0.2 ms（引数なし）と 50.5 ms（16 MiB）で #253 のときと同じ。起動の道に履歴の読み書きは無い。

履歴の 1 回の書き込みの時間（設計席が手で測った・窓なし。タブを閉じるたびに走る「読む → 書く → ディスクへ確定（`FlushFileBuffers`）→ 置き換え」と同じ手順を Python で 15 回ずつ・50 行の履歴。製品の exe では測っていない）:

| 置き場所 | 中央値 | 最小〜最大 |
| --- | --- | --- |
| C（NVMe の SSD。施主の `%LOCALAPPDATA%` と同じドライブ・リポジトリの `out/io-probe`） | 1.9 ms | 1.6〜7.0 ms |
| D（HDD・`D:\NeNeNib\evidence\io-probe`） | 36.6 ms | 28.1〜379.3 ms（最大は 1 回目） |

回していないものと理由: 本物の Ctrl+P の鍵・「∨」のクリック・× のクリックでの撮影（意図は同じ `OpenCommandPalette` / `OpenTabList` / `CloseTab` で ui は変えていない。意図の後は契約 `verify_close_records` `verify_palette_history_rows` `verify_palette_history_open` が覆う）・面を開く時間のベンチ（ベンチが無い）。

対象を限定した理由: 差分は application の履歴の値と port と純関数と controller の記録・面の入口と確定、core の記号の表と閉じた enum の値、adapters の `history.v1` の codec と adapter と行の割り方、`Main.cpp` の合成、単体テストと adapter の試験、`eng/window_driver.py` の消すもので、ui/win32 と本文の編集の経路は変えていない。既存の fixture は不変（protected-diff で changed 0）で、変わった scope は許可した `--history` と `--command-palette` だけ。起動の道に読み書きを足していないことは契約 `verify_quiet_paths` と設計席の速さの 6 本で確かめた。工程 3 は文書だけで、実装・テスト・依存が不変なので工程 1・2 の成功結果と設計席の実機と速さの結果（`d9d0203`）を再利用する（QLT-001 / QLT-012 / QLT-014・ADR 0021）。

ARC-001 / ARC-003 / ARC-007 / ARC-010 / CPP-002 / CPP-004 / CPP-005 / CPP-011 / QLT-013 / QLT-014 / ADR 0042 を自己レビュー。記録は `history_recorded` の 1 本・外すのは `history_forgotten` の 1 本・開くのは `open_document` の 1 本で `OpenDocument` と履歴の行が共用・行の割り方は `TextLines` の 1 本で `session.v1` と `history.v1` が共用（ARC-001）・core と application は OS とファイルに触れず場所とファイルは adapters の `Win32HistoryAdapter` だけ（ARC-003 / ARC-007・symbols 0）・失敗は `std::expected` と閉じた enum `FileHistoryFailure`（ARC-010 / CPP-005）・`CommandChoiceKind` `PaletteScope` `PaletteOrigin` の `switch` に `default` は無い（CPP-002）・単体テストは scope `--history` の 1 翻訳単位（ADR 0042）。残る穴: 履歴を書くのは閉じたときだけで、強制終了のときはその回に閉じていないファイルは入らない・システムドライブが HDD の機械ではタブを閉じるたびに 30〜40 ms の書き込みが入る（今の `FilePort::write` のまま・ベンチに無い）・履歴に時刻と種別のアイコンは無い・大きすぎる / 読めないファイルは知らせるだけで履歴に残る（選ぶたびに知らせが出る）・面を開くときの履歴の読みは 1 回（ディスクから）で面を開く時間のベンチは無い・面の中の日本語入力・同じフォルダ・ブックマーク・Vim の `:e` `:b` `:ls`・履歴を消すコマンドと件数の設定は後続・面の実機の検査は `eng/verify-window.py` に無い（ADR 0060 決定 12 の後続）。

### 5-bz. Ctrl+P の面で日本語入力を受け名前の照合をコードポイントの境目で行う（Issue #264・ADR 0061 決定 1〜6・施主決定 D31・D32・2026-10-02）

ブランチ `feat/264-palette-ime`（main `4aa169a` の上・ADR `c19fee1`・工程 1 `3eb04a6`・工程 2 `a0cb11c`・差し戻し 1 `8e181e0`・差し戻し 2 `7f99031`・工程 3 は本節と ADR 0061 の「強制」と「結果」の 4 行と `docs/todo/current.md` の数字の 2 行と #264 の行と README の FR-006 と FR-012 の項目だけ）。
2026-10-02 に設計席が `git rebase origin/main` で main に追いつかせた。席の報告と日報・引き継ぎは rebase の前の番号（ADR `049064c`・工程 1 `56d1c35`・工程 2 `c5d7ae6`・差し戻し 1 `6a7330e`・差し戻し 2 `992df2f`）で書いてある。rebase で変わったのは main に入った文書だけで、それ以外の差分は 0（`git diff --stat 992df2f 7f99031 -- . ':!docs/handoffs' ':!docs/reports' ':!docs/todo' ':!CLAUDE.md'` が空）。
Ctrl+P の面は IME がオフで開き、使う人が「半角/全角」で開いた IME はそのまま使える。面の変換は面の入力欄に描き、確定は面の入力に入って絞り込む。面を閉じると IME は開く前の状態に戻る。名前の照合はコードポイントの境目で行う。

- 工程 1: core の `match_score` は query をコードポイントごとに歩き、候補の中を `next_code_point` の境目ごとに比べる（点の決め方は今のまま）。application の閉じた enum `ImeStance { as_left, closed, closed_once }` と純関数 `ime_stance_of`（入力は `std::visit`・入力なしはモードの `switch`）・`EditorFrame` の `ime` と `command_composition` と `composing()`。`composition_ignored` は面が開いていれば捨てない、`CommitText` は面が開いていれば `CommandText` の道で面の入力へ、入力行を閉じるのは `close_command_input()` の 1 本。
- 工程 2: ui の `ime_blocked` を消し、`follow_ime` は `frame.ime` を `default` の無い `switch` で実行（`closed_once` は前の構えが違うときだけ閉じる）。`close_ime` は控えの有無に依らず閉じる（既に閉じていれば OS を呼ばない）。面の入力行の変換が消えたとき `cancel_ime_composition`（`ImmNotifyIME` の `CPS_CANCEL`）を `follow_ime` の前に呼ぶ。`draw_command` は入力行の変換を差し込んで本文と同じ `draw_clauses` で下線を引き、キャレットの矩形を `caret_rectangle_` に書く（候補窓は入力欄の下）。
- 差し戻し 1（application・実機の画で見つけた: 案内が変換中も出たままで、長い変換の文字列と重なる）: 面の記号の案内は、入力が空で入力行が変換中でないときだけ出す（`command_palette_view()` の 1 か所・契約 `verify_palette_hint_composing`）。
- 差し戻し 2（ui・実機の画で見つけた: 下線が入力欄の下端に出て、変換の対象の面が入力欄の上下いっぱいの帯になる）: 入力行の変換の面と下線は文字の 1 行の上下に引く（`draw_command_clauses` が上下を画素に揃えて `draw_clauses` へ渡し、変換中はキャレットの棒も同じ上下）。席が見つけて直したこと: 矩形の上端を下げるだけだと `tint_runs` が渡された上端から layout を描き直して字が下へずれるので、変換中だけ layout の高さを 1 行ぶんに詰めた。
- 設計席が差分で確かめたこと（工程 2）: 構えの判断が ui に残っていない・IME の変換の取り消しは IME を開け閉めする前・取り消しが送り返す通知の再入は 1 段で止まる・本文のキャレットの後に入力行のキャレットを描くので候補窓の位置は入力行のもの。
- fixture は oracle の対象ではないので不能で、契約で守る（ADR 0061「強制」）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・全 target・clang-tidy 込み） | 工程 1・工程 2・差し戻し 1・差し戻し 2 とも成功・警告 0（`out/264-step1-build.log` / `out/264-step2-build.log` / `out/264-rework1-build.log` / `out/264-rework2-build.log`。工程 2 は途中で `readability-function-size`（引数 4・60 行）で落ち、閾値は触らず形を直した） |
| `build/nib_tests.exe --command-palette` | 222 → **257 checks**（工程 1）→ **261 checks**（差し戻し 1・`out/264-rework1-palette.log`） |
| `build/nib_tests.exe`（引数なし） | 19178 → **19261**（工程 1 `out/264-step1-tests.log`・面 +35・構え +48）→ 19261（工程 2 `out/264-step2-tests.log`）→ **19265 checks 成功**（差し戻し 1 `out/264-rework1-tests.log`・差し戻し 2 `out/264-rework2-tests.log`） |
| `ctest --test-dir build -R nib_unit`（工程 1） | **1 / 1 成功**（`out/264-step1-ctest.log`） |
| `python eng/symbols.py --build-dir build --require core application` | 工程 1・工程 2・差し戻し 1 とも **0 violation**（`out/264-step1-symbols.log` / `out/264-step2-symbols.log` / `out/264-rework1-symbols.log`） |
| `python eng/conformance.py --build-dir build` | 工程 1・工程 2・差し戻し 1・差し戻し 2 とも **0 violation**（`out/264-{step1,step2,rework1,rework2}-conformance.log`） |
| clang-format --dry-run --Werror（工程 1 の 9 ファイル・工程 2 の 4 ファイル・差し戻し 1 の 3 ファイル・差し戻し 2 の 2 ファイル） | 指摘なし（差し戻しの分は `out/264-rework1-format.log` / `out/264-rework2-format.log`） |
| `clang-tidy -p build src/ui/win32/Direct2DRenderer.cpp`（差し戻し 2・関数長の確認） | 0（`out/264-rework2-tidy.log`） |
| `python eng/protected-diff.py --base origin/main --build --allow --command-palette`（工程 1・工程 2・差し戻し 1） | **終了 0**。`fixtures 1853 -> 1853`・changed 0・scopes 27 / same 26・`--command-palette` だけ 222 → 257 / 261 allowed（`out/protected/56d1c35.json` / `out/protected/c5d7ae6.json` / `out/protected/6a7330e.json`） |
| `pwsh -NoProfile -File eng/validate-git.ps1` | 工程 1・工程 2・差し戻し 1・差し戻し 2 とも passed（`out/264-step1-validate-git.log` / `out/264-step2-validate-git.log` / `out/264-rework1-git.log` / `out/264-rework2-validate-git.log`） |
| 実機（設計席・施主の了承の後・1 回限りのスクリプト `D:\NeNeNib\scripts\palette_ime_frames.py`・125%） | どの回も終了コード 0。profile と文書は `D:\NeNeNib\evidence\frames-264\`・画と記録は `out/frames-264/`（差し戻し 2 の前の画は `out/frames-264/6a7330e/`）。終わりに機械の IME の開閉を見つけたとおりに戻した。OS のクリップボードには触れていない |
| 速さ（設計席・`python eng/measure-speed.py --check --executable build/release-6a7330e/NeNeNib.exe`・2026-10-01） | **6 benches checked, 0 regression(s), 0 unmeasurable**（Release は差し戻し 1 まで・sha256 は `out/release/6a7330e.json`・機械 bc8a356f37c68491（i9-10850K / RTX 3090 / 120 dpi）・`out/speed/2026-10-01T14-01-04Z.json`・`out/264-accept-speed.log`） |
| 工程 3（文書だけ）の `python eng/conformance.py --build-dir build`・`git diff --check`・`eng/validate-git.ps1` | **0 violation**（`out/264-step3-conformance.log`）・`git diff --check` 指摘なし・`eng/validate-git.ps1` passed（`out/264-step3-git.log`） |

設計席の実機の IME の開閉（`states`・2026-10-01・Release `build/release-6a7330e`・窓へ post するだけで本物のキー入力なし・`out/frames-264/states.json`）:

| 場面 | IME の開閉 |
| --- | --- |
| 通常モードで IME をオンにする | 1 |
| Vim の NORMAL へ | 0 |
| NORMAL から面を開く（`:tabs`） | 0 |
| 面の中で外から IME を開く（「半角/全角」の代わり） | 1 |
| 面の中で文字を打つ・Backspace | 1・1（閉じ直さない） |
| 面を閉じて NORMAL | 0 |
| `i` で INSERT | 1（控えに戻る） |
| Esc で NORMAL | 0 |
| 通常モードへ | 1 |

設計席の実機の変換（`compose`・本物のキー入力・2 回。1 回目は 2026-10-01・Release `build/release-6a7330e`（差し戻し 1 まで）。2 回目は 2026-10-02・Release `build/release-992df2f`（差し戻し 2 まで・sha256 280A7BDD37C428F9D42CB86EBB2721EEFBD643EF22EE435F5D160EF5996D5AE5・`out/release/992df2f.json`・記録 `out/frames-264/compose.json`・ログ `out/264-accept-frames-992df2f.log`）。下は 2 回目。IME の開閉と題名の値は 1 回目と同じ）:

| 画 | 操作 | 見えたもの・値 |
| --- | --- | --- |
| `c1-palette-opened.png` | 通常モードで IME オンのまま Ctrl+P | 面は IME オフで開く（開閉 0） |
| `c2-composing.png` | 面の中で IME を開いて「にほんご」 | 入力欄に下線つきで出る。下線は字のすぐ下（1 画素・y 159）で、キャレットの棒（y 142〜162）の中に収まる。候補窓は入力欄の下。一覧は動かず（1 / 3 のまま）、案内は出ない |
| `c3-converted.png` | Space | 変換の対象の面は y 142〜162（キャレットの棒と同じ上下）、太い下線は y 157〜159。入力欄の上下いっぱいの帯ではなくなった |
| `c4-committed.png` | Enter | 面は開いたまま・入力が「日本語」・一覧が `日本語の予定.md` の 1 件（1 / 1）。確定の後の IME は 1・題名は変わらない（Enter は面に届いていない） |
| `c5-activated.png` | Enter | そのタブへ切り替わる（題名 `日本語の予定.md - NeNe Nib`）。IME は 1（面を開く前の状態） |
| `c6-escape-while-composing.png` | Ctrl+P → 「にほんご」→ Esc | 変換だけが消え、面は開いたまま（入力は空・案内が戻る）。開いた直後の IME は 0 |
| `c7-closed-while-composing.png` | 「にほんご」の変換中に面の外（面の左の余白）を押す | 面が閉じる。本文に変換の残りが無い。IME は 1 |
| `c8-fresh-composition-in-body.png` | 本文で `a` | 本文の変換は「あ」だけ（前の「にほんご」が IME の側に残っていない） |
| `c9-body-untouched.png` | Esc | 本文は元のまま。題名に「● 」なし。終了のとき「保存しますか」は出ない・終了 0 |

差し戻し 2 の前後で字は動いていない: 入力行の字の明るい画素の範囲は、`c2` が y 146〜156・x 41〜86、`c3` と `c4` が y 147〜156・x 43〜83 で、前（`6a7330e`）と後（`992df2f`）で同じ。変換中の字（`c3`）と確定した字（`c4`）も同じ高さ。
下線の位置の読み（設計席）: 下線は本文と同じ `underline_runs` が「渡した矩形の下端からキャレットの余白（2 DIP・125% で 3 画素）だけ上」に引く。本文では 1 行の矩形がキャレットの棒より上下 3 画素ずつ広いので下線は棒の下端に来て、入力行では 1 行の矩形が棒と同じなので棒の下端より 3 画素上（字のすぐ下）に来る。面と下線の関係は本文と同じで、直す所は無いと判断した。
`c2` の画は rebase の前の `992df2f` の exe で撮った。rebase の後の `7f99031` と `src` `tests` `eng` の差分は 0 なので、撮り直していない。

速さの 6 本（設計席・`6a7330e` の Release・中央値）:

| ベンチ | 中央値 |
| --- | --- |
| startup-first-frame | 213.7 ms |
| startup-window-shown | 32.6 ms |
| key-to-frame-single | 0.883 ms |
| key-to-frame-burst-200 | 3.888 ms |
| open-large-file-16mib | 252.8 ms |
| key-to-frame-burst-200-16mib | 7.532 ms |

差し戻し 2 の後は測り直していない。差し戻し 2 が触るのは `src/ui/win32/Direct2DRenderer.cpp` と `.hpp` の 2 ファイルで、変えたのは面の入力行が変換中のときに通る描画だけ。変換が無いときのキャレットの棒の式は前と同じ評価順（席の報告）で、起動と打鍵の道に触れない（ADR 0021）。

回していないものと理由: 実装席の Release・`-Full`・`eng/verify-window.py`（実装席は exe を起動しない・面の IME の検査は `eng/verify-window.py` に無い・ADR 0061 決定 7 の後続）・差し戻し 2 の後の速さ（上のとおり）・入力欄の幅を越える長い変換の画（横ずらしを画素に揃える所は差分の読みだけ）・ほかの DPI とほかの IME（Google 日本語入力・ATOK）。

対象を限定した理由: 差分は core の `match_score`、application の `ImeStance` `ime_stance_of` `EditorFrame` `EditorController`（変換の行き先・入力行を閉じる 1 本・案内）と `CommandPaletteView` のコメント、ui/win32 の `EditorWindow`（構えの実行・変換の取り消し）と `Direct2DRenderer`（入力行の変換の描画）、`CMakeLists.txt`、単体テスト `CommandPaletteTests.cpp` `ApplicationTests.cpp` で、Vim の engine と本文の編集の経路は変えていない。既存の fixture は不変（protected-diff で changed 0）で、変わった scope は許可した `--command-palette` だけ。工程 3 は文書だけで、実装・テスト・依存が不変なので工程 1・2 と差し戻し 1・2 の成功結果と設計席の実機と速さの結果を再利用する（QLT-001 / QLT-012 / QLT-014・ADR 0021）。

ARC-001 / ARC-003 / ARC-004 / CPP-002 / CPP-004 / CPP-011 / QLT-001 / QLT-012 / QLT-013 / ADR 0042 を自己レビュー。IME の構えを決めるのは application の `ime_stance_of` の 1 本で ui は frame の値を実行するだけ・入力行を閉じるのは `close_command_input()` の 1 本・確定は打った文字と同じ `CommandText` の道・案内の判断は `command_palette_view()` の 1 か所（ARC-001）・core と application は OS に触れず IMM32 は ui/win32 だけ（ARC-003・symbols 0）・`ImeStance` の `switch` に `default` は無く入力の種類は `std::visit`（CPP-002）・`std::optional` は `value()` / `value_or()` で読む（CPP-004）・`ImeStance` は 1 ファイル 1 型（CPP-011）・単体テストは既存の scope の翻訳単位（ADR 0042）。残る穴: 本文の変換を消す既存の道（モードの切り替え・タブの切り替え・開く）は IME の側を取り消さない・Vim の NORMAL では打鍵ごとに IME の開閉を読み（`ImmGetContext` / `ImmGetOpenStatus`・閉じていれば OS へは何も送らない）6 本のベンチは通常モードの打鍵なのでこの分は測れていない・変換中は入力行のキャレットの棒と候補窓の位置が上下に最大 1 画素動く・最初の変換の 1 回目の候補窓の位置は前の描画の矩形を使う・入力欄の幅を越える長い変換の画は撮っていない・実機の確認は 125% の 1 台・Microsoft IME だけ・面の IME の検査は `eng/verify-window.py` に無い・Ex の行と検索の行の日本語入力・変換中の文字列での絞り込み・全角の `＃` `＠` `：`・全角と半角やひらがなとカタカナを同じとみなす照合は後続（決定 7）。

### 5-ca. Ctrl+P の面は絞り込みの結果を入力と列が変わったときだけ作り frame には見えている行だけを載せる（Issue #270・ADR 0062 決定 1〜4・2026-10-02）

ブランチ `refactor/270-palette-result-window`（main `cbaf4a9` の上・ADR `a3a5e31`・工程 1 `f8cf2aa`・工程 2 `592cd80`・工程 3 は本節と ADR 0062 の「強制」の #270 の行と「実機の確認」の 1 文と「結果」の 3 行と `docs/todo/current.md` の数字の 2 行と #270 の行だけ）。rebase はしていない（main は動いていない）。
ADR の commit は ADR 0062・仕様の D33・D34 と FR-006 の出どころ・索引・ADR 0004（後の変更の 1 行と「強制」の欄を active に）・ADR 0060 の決定 10（後の変更の 1 行）・用語集のワーカーの行・`docs/todo/current.md` を運ぶ。
使う人から見える動きは変えない。施主決定 D33・D34 は #271 / #272 の分。

- 工程 1: core の `CommandPalette` が絞り込みの結果を不変の共有の値（`std::shared_ptr<const Result>`・`Result` は `std::variant<std::vector<std::size_t>, std::vector<CommandChoice>>`）で持つ。作るのは private の `filtered` の 1 本（`opened`・`inserted`・入力を変える `edited`・`filled`）、選択だけを動かすのは private の `reselected` の 1 本（`moved`・`selected_at`）で前の結果を共有する。public の `choices()` を消して `count()`・`choice_at(index)`・`rows(first, limit)`・観測の口 `shares_result_with` を置いた。`listed_choices` は `listed_positions`（位置の列）の 1 本にした（同点は列の順・`std::stable_sort` は使わない）。`mutable` と遅延の計算は無い。
- 工程 2: core に行数の上限 `palette_row_limit`（8）と窓の先頭 `palette_window_first(selected)`。`palette_first_visible` と同じ内側の式 `first_row_of` の 1 本を呼ぶ。`CommandPaletteView` は `rows`（窓・最大 8 件）・`first`・`selected`・`total`・`hint`（公開 aggregate のまま）。`command_palette_view()` は窓だけを写す。ui（`Direct2DRenderer`・`EditorWindow::click_palette`）は件数を `total` で読み、行は窓の中の位置で読む（窓の外は読まない）。
- 設計席が差分で確かめたこと: 結果を作る所と選択だけを動かす所がそれぞれ 1 本・application と ui は 8 を数字で書かない・ui は範囲を確かめてから `rows.at` を読む・`ActivateCommandChoice` に渡すのは今と同じ結果の全体の中の位置・試験の期待値は変わっていない（`choices()` を補助 `rows_of` と `listed_choices_of`、frame の全件を `whole_commands_of` に替えただけ）。
- fixture は oracle の対象ではないので不能で、契約で守る（ADR 0062「強制」）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・全 target・clang-tidy 込み） | 工程 1・工程 2 とも成功・警告 0（`out/270-step1-build.log` / `out/270-step2-build.log`。工程 2 は途中で `readability-function-size`（入れ子 4）で落ち、閾値は触らず契約の補助関数に分けた） |
| `build/nib_tests.exe --command-palette` | 261 → **272 checks**（工程 1）→ **276 checks**（工程 2・`out/270-step2-palette.log`） |
| `build/nib_tests.exe`（引数なし） | 19265 → **19276**（工程 1）→ **19280 checks 成功**（工程 2・`out/270-step2-all.log`） |
| `python eng/symbols.py --build-dir build --require core application` | 工程 1・工程 2 とも **0 violation**（新しい `__std_*` なし） |
| `python eng/conformance.py --build-dir build` | 工程 1・工程 2 とも **0 violation** |
| clang-format --dry-run --Werror（工程 1 の 7 ファイル・工程 2 の 8 ファイル） | 指摘なし |
| `python eng/protected-diff.py --base origin/main --build --allow --command-palette`（工程 1・工程 2） | **終了 0**。`fixtures 1853 -> 1853`・scope 27 のうち `--command-palette` だけ 261 → 272 / 276 allowed・ほかの 26 scope は同じ（`out/protected/f8cf2aa.json` / `out/protected/592cd80.json`） |
| `pwsh -NoProfile -File eng/validate-git.ps1` | 工程 1・工程 2 とも passed |
| 実機（設計席・施主の了承の後・1 回限りのスクリプト `D:\NeNeNib\scripts\palette_window_frames.py`・125%） | 前後とも終了コード 0。画は `out/frames-270/before/` と `out/frames-270/after/`（各 15 枚と `record.json`）・ログは `out/270-accept-frames-before.log` `out/270-accept-frames-after.log`。比較は `D:\NeNeNib\scripts\palette_window_compare.py`（`out/270-accept-frames-compare.log`）で **15 枚すべて前後で違う画素 0**。窓へ post するだけで本物のキー入力なし・OS のクリップボードに触れていない |
| 速さ（設計席・`python eng/measure-speed.py --check --executable build/release-592cd80/NeNeNib.exe`・2026-10-02・施主の了承の後） | **6 benches checked, 0 regression(s), 0 unmeasurable**（機械 bc8a356f37c68491（i9-10850K / RTX 3090 / 120 dpi）・5 回・`out/speed/2026-10-02T13-44-21Z.json`・`out/270-accept-speed.log`） |
| 工程 3（文書だけ）の `python eng/conformance.py --build-dir build`・`git diff --check`・`eng/validate-git.ps1` | **0 violation**（`out/270-step3-conformance.log`）・`git diff --check` 指摘なし・`eng/validate-git.ps1` passed（`out/270-step3-git.log`） |

設計席の実機の画の前後比較（2026-10-02）。前は `build/release-992df2f/NeNeNib.exe`（`src` `tests` `eng` は main `cbaf4a9` と同じ・sha256 280A7BDD…996D5AE5・`out/release/992df2f.json`）、後は `build/release-592cd80/NeNeNib.exe`（sha256 DBC2421A9A251F209EDEA4475D57B537FD573A9F2ABE740ECD1383904C1D6B2C・`out/release/592cd80.json`）。タブ 12 本。Vim の NORMAL の `:tabs` で一覧を開き、Backspace で `#` を消して全部の候補にしてから操作した。題名も前後で同じ（低い窓のクリックの後 `p04.txt - NeNe Nib`・高い窓のクリックの後 `p06.txt - NeNe Nib`）:

| 画 | 窓 | 操作 | 見えたもの（前後で同じ） |
| --- | --- | --- | --- |
| `a1-tab-list-active-last` | 800 × 450（見える行 3） | `:tabs` | `#`・アクティブ（12 本目）が一番下の行 |
| `a2-all-first` | 同 | Backspace | 入力が空・選択は先頭 |
| `a3-down-5` | 同 | ↓ 5 回 | p04〜p06・選択は p06・`6 / 12` |
| `a4-wrapped-down` | 同 | ↓ 8 回 | 12 本を越えて折り返す |
| `a5-wrapped-up` | 同 | ↑ 3 回 | 先頭を越えて後ろへ |
| `a6-query` | 同 | `p1` | 絞り込み |
| `a7-commands` | 同 | `:` | 設定のコマンド |
| `a8-commands-down-10` | 同 | ↓ 10 回 | `set fontsize=` `set incsearch` `set nohlsearch`・`11 / 22` |
| `a9-before-click` `a10-after-click` | 同 | 全部の候補で ↓ 4 回 → 見えている 2 行目をクリック | p04 のタブへ切り替わる |
| `b1-all-first` | 1000 × 800（見える行 8） | 一覧を開いて Backspace | 8 行 |
| `b2-down-9` | 同 | ↓ 9 回 | p03〜p10・選択は p10・`10 / 12` |
| `b3-wrapped` | 同 | ↓ 3 回 | 折り返して先頭 |
| `b4-up-2` | 同 | ↑ 2 回 | 後ろから 2 本目 |
| `b5-after-click` | 同 | 見えている 3 行目をクリック | p06 のタブへ切り替わる |

設計席は後の 15 枚を目でも見た（件数の表示は `a1` 12 / 12・`a2` 1 / 12・`a4` 2 / 12・`a5` 11 / 12・`a6` 1 / 4・`a7` 1 / 22・`a9` 5 / 12・`b1` 1 / 12・`b3` 1 / 12・`b4` 11 / 12）。

速さの 6 本（設計席・`592cd80` の Release・中央値）:

| ベンチ | 中央値 |
| --- | --- |
| startup-first-frame | 217.173 ms |
| startup-window-shown | 37.616 ms |
| key-to-frame-single | 0.960 ms |
| key-to-frame-burst-200 | 3.489 ms |
| open-large-file-16mib | 262.512 ms |
| key-to-frame-burst-200-16mib | 7.721 ms |

6 本は面を開かない道（起動と本文の打鍵）。面の中の 1 打鍵が軽くなった分は、この 6 本では測れていない（面の中の 1 打鍵のベンチは #272 で足す・ADR 0062 の決定 19）。

回していないものと理由: 実装席の Release・`-Full`・`eng/verify-window.py`（実装席は exe を起動しない・面の検査は `eng/verify-window.py` に無い）・面の中の 1 打鍵の速さ（ベンチが無い・#272）・履歴の候補（`@`）とホイールと本物の Ctrl+P の鍵とほかの DPI の画（frame の同じ欄を同じ式で読む。ホイールは ↑↓ と同じ意図 `complete_*`）。

対象を限定した理由: 差分は core の `CommandPalette` `CommandChoice` `PaletteLayout`、application の `CommandPaletteView` と `EditorController`（面の view・`ActivateCommandChoice`・`submit_palette`）、ui/win32 の `Direct2DRenderer`（面の行と件数）と `EditorWindow::click_palette`、単体テスト `CommandPaletteTests.cpp` `UserThemeSelectionTests.cpp` `TabsTests.cpp` で、Vim の engine と本文の編集の経路は変えていない。既存の fixture は不変（protected-diff で changed 0）で、変わった scope は許可した `--command-palette` だけ。工程 3 は文書だけで、実装・テスト・依存が不変なので工程 1・2 の成功結果と設計席の実機と速さの結果を再利用する（QLT-001 / QLT-012 / QLT-014・ADR 0021）。

ARC-001 / ARC-003 / CPP-002 / CPP-003 / CPP-004 / CPP-011 / QLT-001 / QLT-012 / QLT-013 / ADR 0042 を自己レビュー。結果を作るのは `filtered` の 1 本・選択だけを動かすのは `reselected` の 1 本・絞り込みと順は `listed_positions` の 1 本・窓の先頭は `first_row_of` の 1 つの式を `palette_first_visible` と `palette_window_first` が共用・上限は core の `palette_row_limit` の 1 か所（ARC-001）・core と application は OS に触れない（ARC-003・symbols 0）・結果の 2 つの形は `std::visit` で型ごとの `row_of` へ写す（CPP-002）・`CommandPaletteView` は公開 aggregate のままでメソッドを持たない（CPP-003）・`choice_at` の `std::optional` は `value()` / `has_value()` で読む（CPP-004）・新しい struct とファイルは無い（CPP-011）・単体テストは既存の scope の翻訳単位（ADR 0042）。残る穴: 面の中の 1 打鍵が軽くなった量は測っていない（ベンチは #272）・実機の画は 125% の 1 台・ダークのテーマ・タブの候補と設定のコマンドだけ・候補の列を後から伸ばす口はまだ無い（#272）・ワーカーとフォルダの列挙は #271・観測の口 `shares_result_with` は製品のコードからは呼ばれない（契約のためだけにある）。

### 5-cb. 同じフォルダは裏のワーカー 1 本が読み application は型のある FolderPort で頼んで受け取る（Issue #271・ADR 0062 決定 5〜10・ADR 0004・2026-10-02）

ブランチ `feat/271-folder-worker`（main `e570fc3` の上・工程 1 `dcc9d31`・工程 2 `1770825`・工程 3 は本節と ADR 0062 の「強制」の #271 の行と「実機の確認」の 1 文と「結果」の 4 行と `docs/todo/current.md` の数字の 2 行と同じフォルダの行だけ）。rebase はしていない（main は動いていない）。
使う人から見える動きは変えない（controller は `list` も `collect` も呼ばない。呼ぶのは #272）。

- 工程 1（`dcc9d31`）: application に `FolderRequest`（フォルダと券）・`FolderProgress`（閉じた enum `more` `complete` `truncated` `failed`）・`FolderBatch`・`FolderPort`（`list` / `[[nodiscard]] collect`）。adapters に `Win32Worker`（スレッド 1 本・キュー 1 本・最初の仕事で `std::jthread` を起こす・壊すときは止める合図の後「`CancelSynchronousIo` → スレッドのハンドルを 10 ms 待つ」をスレッドが終わるまで繰り返して join・`TerminateThread` は無い）・`FolderShelf`（たまり・世代・合図を 1 つのロックで守り、合図はたまりが空から 1 つ以上になったときだけロックの外で呼ぶ）・`FolderListing`（1 つの要求の列挙・1 件ごとに止める合図と世代を見る）・`Win32FolderAdapter`（既定値 `folder_batch_files` 1024・`folder_file_limit` 8192 の 1 か所）。CTest `nib_folders`（`tests/adapters/FolderAdapterTests.cpp`・90 checks・`TIMEOUT 60`）。
- 工程 2（`1770825`）: `EditorPorts` に 9 番目の `folders`・替え玉 `ScriptedFolders`・中身の無い意図 `WorkCompleted`（controller の入口の 1 か所で Ctrl+Tab の歩きを続け知らせを消さない意図に数え、`accept` は空）・ui の合図の窓メッセージ `work_message`（`WM_APP + 1`・`EditorWindow.cpp` の 1 か所）と `work_signal()`・`send` の深さを数えて途中で届いた合図をいちばん外の `send` の終わりで 1 回送る・`src/app` の合成（ワーカーと adapter を controller より前に宣言・窓ができた後に `bind`）・新しい scope `--background-work`（46 checks）。`--history` の `verify_quiet_paths` にフォルダの口の回数を足した（checks 数は不変）。
- 設計席が差分で見つけた 1 か所（差し戻し・工程 2 の席が直した）: `FolderListing::run` が最初の `FindFirstFileExW` を呼んだ後で初めて世代を見ていた。キューで待つ間に古くなった列挙が走り出すたびに 1 回ずつ無駄な I/O を呼ぶので、`run` の先頭で `live(stop)` を見るようにした（外から見える結果は変わらない）。
- 設計席が決めたこと: スレッドは最初の仕事を受けたときに起こす（Ctrl+P を開かない人はスレッドを持たない）・batch の件数と上限は adapter を作るときに渡せる値で既定値は 1 か所の定数（試験は小さい数で確かめる。数字は #272 の実測で見直す）・合図は「たまりが空から 1 つ以上になったとき」だけ・ロックの外で呼ぶ。
- fixture は oracle の対象ではないので不能。adapters の試験と契約で守る（ADR 0062「強制」）。

| 検査 | 退行の対象と実測 |
| --- | --- |
| `cmake --build build`（Debug・ASan / UBSan・clang-tidy 込み） | 工程 1・工程 2 とも成功・警告 0（`out/271-step1-build.log` / `out/271-step2-build.log`） |
| `ctest --test-dir build -R nib_folders`（工程 1）・`-R "nib_folders\|nib_adapters"`（工程 2） | 工程 1 は 3 回とも成功（**90 checks**・0 failures・`out/271-step1-ctest-{1,2,3}.log`）・工程 2 は 3 回とも 2/2 成功（`out/271-step2-ctest-{1,2,3}.log`） |
| `build/nib_tests.exe --background-work` / `--history` | **46 checks** 成功 / 38 checks 成功（工程 2） |
| `build/nib_tests.exe`（引数なし） | 19280（工程 1）→ **19326 checks 成功**（工程 2・`out/271-step2-unit.log`） |
| 契約の効き目（工程 2） | controller の入口の 2 行を外した変異で 4 件落ちることを確かめて戻した（`out/271-step2-mutant.log`） |
| `python eng/symbols.py --build-dir build --require core application` | 工程 1・工程 2 とも 2 libraries・**0 violation**（core と application にスレッドと `PostMessage` のシンボルが出ない） |
| `python eng/conformance.py --build-dir build` | 工程 1・工程 2 とも **0 violation** |
| clang-format --dry-run --Werror（工程 1 の 13 ファイル・工程 2 の 19 ファイル） | 指摘なし |
| `python eng/protected-diff.py --base origin/main --build`（工程 1）・`--allow=--background-work`（工程 2） | **終了 0**。`fixtures 1853 -> 1853`・工程 1 は scope 27 same 27・工程 2 は scope 28（same 27・`--background-work` 新規 46 allowed）（`out/protected/dcc9d31.json` / `out/protected/1770825.json`） |
| `pwsh -NoProfile -File eng/validate-git.ps1` | 工程 1・工程 2 とも passed |
| 速さ（設計席・`python eng/measure-speed.py --check --executable build/release-1770825/NeNeNib.exe`・2026-10-02・施主の了承の後） | **6 benches checked, 0 regression(s), 0 unmeasurable**（機械 bc8a356f37c68491（i9-10850K / RTX 3090 / 120 dpi）・5 回・`out/speed/2026-10-02T15-08-39Z.json`・`out/271-accept-speed.log`。Release の sha256 262A90B985068F3B9981C462F59E4A0A17F8ED6062F71EFC21477E63A4867612・`out/release/1770825.json`） |
| 工程 3（文書だけ）の `python eng/conformance.py --build-dir build`・`git diff --check`・`eng/validate-git.ps1` | **0 violation**（`out/271-step3-conformance.log`）・`git diff --check` 指摘なし・`eng/validate-git.ps1` passed（`out/271-step3-git.log`） |

`eng/architecture.json` と許可表は変えていない（`adapters_win32` は kernel32 だけで link が通った）。

速さの 6 本（設計席・中央値）:

| ベンチ | `1770825`（#271 の後） | #270 の後（`e570fc3` と同じ実装・`out/speed/2026-10-02T13-44-21Z.json`） |
| --- | --- | --- |
| startup-first-frame | 212.547 ms | 217.173 ms |
| startup-window-shown | 35.155 ms | 37.616 ms |
| key-to-frame-single | 0.911 ms | 0.960 ms |
| key-to-frame-burst-200 | 4.604 ms | 3.489 ms |
| open-large-file-16mib | 273.350 ms | 262.512 ms |
| key-to-frame-burst-200-16mib | 6.329 ms | 7.721 ms |

起動は前と同じ（合成でワーカーと adapter を作るが、スレッドは起こさない）。200 打鍵（空の文書）は 5 回の幅が 3.369〜8.219 ms で、中央値は基準の許容（±25%・下限 2 ms）の中。6 本は面を開かないので、ワーカーのスレッドは 1 度も起きていない。exe の終了は 6 本の全部の回で正常。

回していないものと理由: 実機の画（#271 は使う人から見える動きと見た目を変えず、合図の窓メッセージは届く道がまだ無い・`Direct2DRenderer` は不変）・実装席の Release・`-Full`・`eng/verify-window.py`（実装席は exe を起動しない）・Ctrl+P を初めて開いたときにスレッドを起こす時間（#272 の面を開く道で測る）。

対象を限定した理由: 差分は application の port と値と意図 `WorkCompleted`・controller の入口・adapters の新しい 4 つのクラス・ui の `EditorWindow`（合図と `send` の深さ）・`src/app` の合成・単体テストの替え玉と新しい scope と `EditorPorts` を作る所で、core と Vim の engine と本文の編集の経路は変えていない。既存の fixture は不変（protected-diff で changed 0）で、変わった scope は許可した新規の `--background-work` だけ。新しい adapters の試験は CTest `nib_folders` で 3 回続けて回した。工程 3 は文書だけで、実装・テスト・依存が不変なので工程 1・2 の成功結果と設計席の速さの結果を再利用する（QLT-001 / QLT-012 / QLT-014・ADR 0021）。

ARC-003 / ARC-007 / ARC-010 / CPP-002 / CPP-003 / CPP-005 / CPP-011 / CPP-013 / CNF-009 / QLT-010 / ADR 0004 / ADR 0042 を自己レビュー。スレッド・ロック・セマフォの待ちは `src/adapters/win32` と adapters の試験の中だけで、core と application の symbols は違反 0・ui にスレッドと同期のヘッダは無い（ARC-003 / ARC-007 / CPP-013 / CNF-009）・期待される失敗は `FolderProgress::failed` で返す（ARC-010）・`run` は `noexcept` で広い catch を書かない（CPP-005）・新しい型は 1 ファイル 1 型（CPP-011）・`WorkCompleted` は公開 aggregate でメソッドを持たない（CPP-003）・閾値と除外は触らず `dispatch` の関数長は `lifetime_message` へ出して通した（QLT-010）・要求は値の写し（ADR 0004）・新しい scope は 1 翻訳単位（ADR 0042）。残る穴: 合図の窓メッセージと `send` の再入の扱いは単体テストが無い（窓が要る・実際に走るのは #272 から）・止まっているネットワークの I/O を `CancelSynchronousIo` で起こす道は試験で再現していない・「OS が返す順」の試験は NTFS の名前順を当てにしている・ワーカーは 1 本なので遅い場所の列挙が I/O の中で止まっている間は次の列挙が待つ・`PostMessageW` の失敗を見ない・adapter が壊れる直前に写した合図が壊れた後に 1 回呼ばれ得る・`FolderProgress` を読む `switch` はまだ無い（#272）。

## 5-cc — Ctrl+P の同じフォルダ（Issue #272・ADR 0062）

2026-10-03。基点 `ddde80b`、製品の実装は `eb5b0e7` → `f6ec509` → `10cab8c`、最後のコメント整形は `edeb38b`。記号 `/`、既知の非テキスト拡張子70件の除外、同じフォルダの非同期列挙、タブ・履歴との重複除外、追加後の選択保持、D35の持続する打ち切り案内を実装した。フォルダの取得は `FolderPort`、受信は `WorkCompleted`、絞り込みは `listed_positions`、開く道は `open_listed` の既存の正典を使う。

### 再利用する製品検証

下記は前の実装席・設計席の成功記録を読み、対象コードと依存の不変性を確認したもの。2026-10-03の再開後に同じ製品テストを再実行してはいない（QLT-001 / QLT-012・ADR 0021）。

| コマンド・対象 | 結果と記録 | 再利用する根拠 |
| --- | --- | --- |
| `cmake --build build`（Debug・ASan / UBSan・clang-tidy） | 成功・警告0。`out/272-rework1-build.log` | `10cab8c` の後の製品差分はコメントだけ |
| `build/nib_tests.exe --background-work` | 89 checks成功。`out/272-rework1-bg.log` | 券・重複・静かな道・選択・打ち切りの実装とテストは不変 |
| `build/nib_tests.exe --command-palette` / `--history` | 356 / 38 checks成功。`out/272-step2-palette.log` / `out/272-step2-history.log` | 記号・拡張子・列の追加・履歴の対象実装は不変。持続表示の変更は上の89 checksが確認 |
| `build/nib_tests.exe`（既存記録） | 19449 checks成功。`out/272-rework1-all.log` | 既存の成功を参照。工程3のPython・文書のために全件を繰り返さない |
| `python eng/conformance.py --build-dir build` / `python eng/symbols.py --build-dir build --require core application` | 各0 violation。`out/272-rework1-conformance.log` / `out/272-rework1-symbols.log` | 製品の依存・シンボル・検査設定は不変。新しい文書の整合は別途確認 |
| `python eng/protected-diff.py --base origin/main --build --allow=--background-work --allow=--command-palette` | 終了0。`out/272-rework1-protected2.log` / `out/protected/10cab8c.json` | fixtures 1853→1853・metadata/deleted/changed/added各0、scope28のうち26不変、palette276→356・background46→89だけ許可。以後の製品・fixture・単体テストは不変 |

券の比較、開く時の読み残し破棄、出どころを条件にした履歴削除の3箇所を外した変異で、`--background-work` が11件失敗した既存記録も確認した（`out/272-step2-mutant.log`）。

再開後の追加確認: `clang-format --dry-run --Werror src/application/EditorController.cpp` は終了0（`out/272-sana-format.log`）。前の `out/272-rework1-format2.log` は成功ではなく、コメントの折り返し4件の指摘であった。`edeb38b` の修正を今回このファイルだけで確認した。既存5コミットの `pwsh -NoProfile -File eng/validate-git.ps1` は成功（`out/272-sana-git-existing.log`）。

### 再利用する実機の確認

前の設計席がhideの了承の後に撮った画と終了記録。125% DPI。撮影は `D:\NeNeNib\scripts\samefolder_frames.py`。新しい計測器と文書は製品の描画を変えないため撮り直さない。

- 小フォルダ: Release `8d0e6ff`、`out/frames-272/s*.png` / `small.json`。タブ2本に続く5候補、既知のバイナリ・隠し・下のフォルダの除外、`/`・日本語の絞り込み、新しいタブへのOpen、消えた候補の1行の知らせと履歴書込なしを確認。終了0。
- 8200ファイル: Release `edeb38b`、`out/frames-272/edeb38b/`。8192件までで打ち切り、`/8191` は候補あり、`/8192` は候補なし。入力・選択後もD35の案内が残り、閉じると消える。終了0。再開後も `b3-last-listed.png` を目視した。
- ReleaseのSHA-256は `F0FEC8C0BD99EE356ED5E9D788605C30E6B1FCFEBB4C50FCBBC7F33FE6E40857`（`out/release/edeb38b.json`）。再開後の `Get-FileHash build/release-edeb38b/NeNeNib.exe -Algorithm SHA256` が一致し、`git diff --exit-code edeb38b -- src CMakeLists.txt eng/targets.cmake eng/tool-versions.json` は終了0。実機の計測にも同じexeを使える。

### 計測器とCIの限定検証

- `python -m unittest discover -s tests/conformance -p test_speed.py -v`: **64 tests / OK**（`out/272-step3-sana-tests-02.log`）。`measured_palette` の最後の入力と次の描画、入力不足・描画欠如、Ctrl+P失敗で文字を送らないこと、本文を変えた試行の欠測、計測区間の前の候補件数の撮影、覆いと撮影失敗、各benchの対象選択、過去6本の記録再利用、部分記録で全基準値を置き換えないこと、基準値なしと欠測の区別を確認。製品のテストは起動しない。
- `python eng/measure-speed.py --help`・`git diff --check`: 終了0（`out/272-step3-sana-help-02.log` / `out/272-step3-sana-diff-check-02.log`）。新しい計測器と引数の入口を確認した。
- 新規 `.github/workflows/measure-speed.yml`: YAML解析・起動条件5ケース・既存pin・単一job・正典Release・失敗即停止・artifact90日・既存check.yml不変を確認（`out/272-ci-sana-structure.log`）。PNG保存追加後のartifact構造も成功（`out/272-ci-sana-artifact-structure.log`）。PowerShellの3ブロックは構文エラー0（`out/272-ci-sana-pwsh-syntax.log`）。通常push/同期では実行せず、明示ラベルか手動で新しい1本だけを測る。

計測の変更は `eng/measure-speed.py`・基準値の説明と `tests/conformance/test_speed.py`。既存の6個の数値と許容25% / floor2msは変えていない。5000ファイルを用意し、Ctrl+P→1.5秒待ち→暖機f→0の1文字から描画までを測る。待機時間だけでは候補の到着は証明できないため、各試行の0直前のPNGで件数を確認してから採用する。

実機はhideの了承後、Release `edeb38b` で `python eng/measure-speed.py --check --bench key-to-frame-palette-5000 --executable build/release-edeb38b/NeNeNib.exe` を実行。5試行すべて有効、中央値 **2.630 ms**（2.526〜2.715 ms・`out/speed/2026-10-03T05-15-16Z.json`・`out/272-sana-speed-palette.log`）。各試行の `palette-2026-10-03T05-15-*.png` 5枚すべてに暖機fと件数 **1 / 5000** が写り、設計サナが目視した。`--adopt --bench key-to-frame-palette-5000 --values out/speed/2026-10-03T05-15-16Z.json` で、この1本だけを実機 `bc8a356f37c68491` の新しい基準値として採用した（`out/272-sana-adopt.log`）。1024件のbatchと8192件の上限は、今回の5000件の入力速度と前の8200件の実機確認を根拠に維持する。

共通の `command_message()` が全frameで呼ばれるため、同じexeで `--check --bench key-to-frame-single` も実行し、5試行すべて有効、中央値 **1.016 ms**（0.829〜1.614 ms）、1 bench checked・0 regression・0 unmeasurable（`out/speed/2026-10-03T05-15-53Z.json`・`out/272-sana-speed-single.log`）。CIの新しい測定と基準値の採用は下記のとおり完了。それ以外の起動・大容量ファイルを開く処理は不変で、#271の成功（`out/speed/2026-10-02T15-08-39Z.json`・5-cb）を再利用する。全件の製品テストと全性能ベンチを工程のために繰り返さない。

自己レビュー: FR-006 / D33〜D35 / ARC-001 / ARC-003 / ARC-004 / ARC-007 / ARC-010 / CPP-002 / CPP-004 / CPP-011 / QLT-001 / QLT-012 / QLT-013 / QLT-014。公開の値とportの境界を保ち、時刻・ファイル・スレッドを中核へ持ち込まず、閉じた分岐を網羅し、閾値・除外・依存を弱めていない。製品の保存スキーマ変更なし。Waivers: none。

残るリスク: 8192件は拡張子・重複除外前の上限であり、画像多数のフォルダではテキスト候補が少なくても後ろが打ち切られる。パスの比較は既存の `same_file`（大文字小文字を無視する文字列比較）であり、`/` と `\` の混在を同一視しない既存の制約がある。入れ子のメッセージループ中の完了合図と、停止したネットワークI/Oの解除は未確認。候補なしの字が左へ寄る既存の表示は本件で修正していない。

### CI の新しい基準値（2026-10-03）

[Actions run 37099450677](https://github.com/hideyukiMORI/nene-nib/actions/runs/37099450677) は成功。正典 `eng/build-release.ps1 -Ref HEAD` が `05792e5` の Release をビルドし、同じ host `e7a87d5b6ac1e14b`（AMD EPYC 7763 / Microsoft Hyper-V Video / 96 DPI）で `python eng/measure-speed.py --check --bench key-to-frame-palette-5000 --executable build/release-05792e5/NeNeNib.exe` を独立に3回実行した。新しい値はまだ基準が無いので記録だけと明示し、5試行ずつ15試行すべて有効・欠測0。通常のPR必須checkからは起動しない。

| 記録（UTC） | 中央値 ms | 5試行の最小〜最大 ms |
| --- | --- | --- |
| `2026-10-03T05-24-43Z.json` | 4.097 | 3.938〜4.337 |
| `2026-10-03T05-24-58Z.json` | 4.038 | 3.878〜4.085 |
| `2026-10-03T05-25-14Z.json` | 4.179 | 4.047〜4.428 |

artifact `palette-speed-records`（保持90日）には上記JSON・各試行の測定直前のPNG15枚・Releaseの記録がある。取得先は `D:/NeNeNib/evidence/272-ci-37099450677/`、ログは `out/272-sana-ci-speed.log`。Release SHA-256は `74089D9528CE3680778DDD736F6AF1407AA6FFFC709E2589DEF5B1B571B2FFA2`。PNG15枚すべてのSHA-256が `0E034FE4653BBDFF921256209503EE4A832F6A32C21AA87F24807B74774BE387` と一致し、設計サナが代表画像で暖機fと **1 / 5000** を目視した。従って15試行すべての測定直前の表示に5000候補が揃っている。

ADR 0016 / 0062決定19に従い、3中央値の中央値 **4.097 ms** をこの指紋の新ベンチだけの基準値として採用。minimumMs / maximumMsは3中央値の幅 **4.038〜4.179 ms**。出典・日付・集計方法を基準値のnoteへ追記した。既存6本の値・既存metadata・25% / floor2msの許容を機械比較し不変（`out/272-sana-ci-adopt.log`）。3つの保存記録それぞれへ `python eng/measure-speed.py --check --bench key-to-frame-palette-5000 --values <記録>` を適用し、各 **1 bench checked・0 regression・0 unmeasurable**（`out/272-sana-ci-reference-check.log`）。これは採用したデータと比較器の整合確認であり、新たな性能測定ではない。

CI後の変更は基準値と文書だけ。製品・計測器・テスト・workflowは不変であり、成功済みの64試験・実機2本・CI3回を統合時にも再利用する。文書のリンク・規則ID・強制状態の整合とGit/PR規約・空白だけを最終確認する。

### main の文書更新の取り込み

統合前に `origin/main` の `c328e0d`（PR #276・前日の日報と引き継ぎ）へrebaseした。競合はCLAUDE.mdとcurrent.mdの説明・進捗だけで、mainのワーカーの説明と履歴を残して今回の状態へ揃えた。受理・測定時の番号は証拠の出典として書き換えず残す。対応は `eb5b0e7→4d2babe`・`f6ec509→7fb002d`・`8d0e6ff→f8d7b19`・`10cab8c→3682ad3`・`edeb38b→2787f72`・`05792e5→6018734`。`git diff --exit-code 001debf -- src tests eng .github CMakeLists.txt` は終了0で、製品・試験・計測・workflowがrebase前後で同一と確認した。性能や製品テストを繰り返す変更ではない。

rebase後の最終文書確認: conformance.document_checksは0 violation（out/272-sana-docs-rebase.log）、git diff --checkとPR本文のgit-conventions.py --pr-bodyは終了0。確認対象は文書の相対リンク・規則ID・強制状態と差分の空白・検証記録の存在。


## 5-cd — 明示ブックマークとCtrl+Pの共有候補（Issue #278・ADR 0063）

2026-10-03、設計・実装・検証をサナが単体で実施。基点は #272 統合後の `2096565`。FR-006 / FR-010 と D36・D37 に従い、通常 Ctrl+D / Vim Ctrl+Shift+D の明示操作、`*` の候補、消えた登録の保持を追加した。保存先は独立した `bookmarks.v1`。既存の `FilePort::write` の原子的な置換と `same_file` を使い、名前のあるタブも `open_listed` → `open_document` の経路へ揃える。無題は既存の `tabnext N`。中核へOSのI/Oを持ち込まない。

### 対象検証と根拠

| 対象・退行の理由 | コマンド | 結果・記録 |
| --- | --- | --- |
| 新しい型・port・閉じた分岐とキー経路、未検査optional・複雑度 | `cmake --build build --target NeNeNib nib_tests nib_bookmark_tests`（`eng/toolchain.ps1`、Debug・clang-tidy・ASan/UBSan） | 成功。`out/278-sana-build05.log`。テストの期待値調整後は `--target nib_tests` で成功（`out/278-sana-build06.log`） |
| 更新・上限・キー対応・I/Oの時機・失敗時の保護・Vim待ち・IME・各出どころの重複 | `build/nib_tests.exe --bookmarks` | 40 checks成功、`out/278-bookmarks.log` |
| 名前ありタブのopen経路・一覧の順と確定 | `build/nib_tests.exe --tabs` | 291 checks成功、`out/278-tabs.log` |
| 新しいscope分岐・印・候補の保持・名前あり候補のパス | `build/nib_tests.exe --command-palette` | 356 checks成功、`out/278-command-palette.log` |
| ブックマークと分けた履歴の記録・終了時のI/O | `build/nib_tests.exe --history` | 38 checks成功、`out/278-history.log` |
| 既表示のファイルを除くフォルダの列・選択維持・静かな完了合図 | `build/nib_tests.exe --background-work` | 89 checks成功、`out/278-background-work.log` |
| 新schemaの正例・反例・上限・絶対パス・本物の保存の往復と再作成、読取専用の原本を失敗時に保護 | `ctest --test-dir build -R '^nib_bookmarks$' --output-on-failure` | 1 test / 1245 checks / 0 failures。`out/278-adapter.log`、`build/Testing/Temporary/LastTest.log` |
| 専用profileの初期化にbookmarksを追加し、本物のprofileを消さない | `python tests/conformance/test_frame_capture.py ProfileReset -v`（TEMP/TMP=`D:/NeNeNib/evidence/temp`） | 2 tests / OK。製品は起動しない |
| 新型とモジュール境界・規則・整形 | `python eng/conformance.py --build-dir build`、変更したC++への `clang-format --dry-run --Werror` | 違反0・終了0。`out/278-conformance.log` / `out/278-format.log` |
| core/applicationへOS依存を追加しない | `python eng/symbols.py --build-dir build --require core application` | 2 libraries / 0 violations、`out/278-symbols.log` |

最初のビルドでcodecと候補作成の複雑度を検出し、責務で分割した。続くoptional参照と新規テストの型指定を修正。旧タブ候補の `tabnext N` の期待値をパスによるopenへ更新し、IMEは変換を受ける通常モードでbodyとpaletteを測った。最終の上記実行はすべて成功。関係しないVim fixtureの全再生・全性能ベンチは実行していない。

再利用: 成功後に製品・関連テスト・依存・環境が変わらない検証はpush・レビュー・mergeでも再利用する。対象の期待値だけが変わった再ビルド後には `--bookmarks` / `--tabs` のみ実行し、成功済みの候補・履歴・裏の仕事・adapterは繰り返していない。実機表示・キーと絞り込みの新しい性能測定、保護対象の静的差分はこの時点では未実施。

規則: ARC-001 / ARC-003 / ARC-004 / ARC-007 / ARC-010 / ARC-011 / CPP-002 / CPP-004 / CPP-005 / CPP-011 / QLT-001 / QLT-012 / QLT-013 / QLT-014。閾値・除外・依存追加なし。Waivers: none。

残る制約: 既存のsame_fileは区切り文字混在を同一視しない。複数窓が同時にread-modify-writeしたときの直列化は範囲外。ほかの窓の変更は次に面を開くときに読み直す。登録解除後も履歴や同じフォルダとして出ることは正常。


### Releaseと保護対象の静的比較

`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` は `764fa20` で成功（`out/278-release.log` / `out/release/764fa20.json`）。`build/release-764fa20/NeNeNib.exe`、1,283,072 bytes、SHA-256 `0328FFAD811C0E77F2166C4002995C090358A9D08FDC99E49BF7EEA085DBF693`。このコマンドでは起動していない。

`python eng/protected-diff.py --base 2096565 --head 764fa20` は終了0（`out/278-protected.log` / `out/protected/764fa20.json`）。実出力は `fixtures 1853 -> 1853 / metadata 0 / deleted 0 / changed 0 / added 0`、`files changed none`。保護対象はperf-reference・symbol-allowlist・conformance-rules・SettingsCodec。既存scopeの件数比較は28本すべて「未測」、新規 `--bookmarks` もこの道具では未測。比較用exeも `--build` も渡していないためで、全scope不変の証拠とは扱わない。対象scopeの成否と件数は上の限定実行の記録が正。


### 実機の限定確認と性能

hideが2026-10-03 15:37 JSTに前面操作を了承した後、`python D:/NeNeNib/scripts/bookmarks-278.py --executable C:/Users/info/WORKS/NeNeNib/build/release-764fa20/NeNeNib.exe` を実行。終了0、11枚の画とJSONを `out/frames-278/` に保存（`out/278-window.log`）。専用profileと文書は `D:/NeNeNib/evidence/bookmarks-278/`。サナが11枚すべてを目視し、以下を受理した。

- 通常Ctrl+Dは長押しの4 key-downでも登録1回。版と登録内容を実ファイルで照合した。
- 空のCtrl+Pで5種類の記号の案内が収まり、登録済みのタブは「タブ・ブックマーク」の1行。`*`では同じ登録が1件だけ。
- Vim Ctrl+Dは行1から行5へ半画面移動し、登録のbytesは不変。Ctrl+Shift+Dで解除・再登録できる。
- 再起動後、開いた登録と閉じた登録が同じ `*` に並ぶ。存在しないgone.txtは「開けませんでした」の1行と登録保持、一覧で解除してもタブは増えない。
- 閉じたbeta.txtは新しいタブで開き、開いているalpha.txtは既存タブへ切り替わる。
- version=9の登録ファイルへ付け外しを試みてもbytesを変えず、読めなかったことを1行知らせる。

同じexeで `python eng/measure-speed.py --check --bench key-to-frame-palette-5000 --executable build/release-764fa20/NeNeNib.exe` を実行。5試行すべて有効、中央値 **2.708ms**、最小2.614・最大2.831ms（samples 2.614 / 2.708 / 2.831 / 2.658 / 2.797）。実機 `bc8a356f37c68491` の既存基準値2.630msへ比較して **1 bench checked / 0 regressions / 0 unmeasurable**。基準値は変更していない。記録は `out/278-speed-palette.log` / `out/speed/2026-10-03T06-38-44Z.json`。

測定直前のPNG5枚 `palette-2026-10-03T06-38-*.png` すべてを読み、暖機fと **1 / 5000** の表示を確認した。5枚のSHA-256もすべて `5873681DFF7C111BA6876363CC5D835A9E70F0CE908FB405C4967A42BB12B5E9` で一致。打鍵区間で5000候補が揃っていることを証明する。15:38 JSTにhideへ前面操作終了を通知済み。

自己レビュー: 既存の原子的な保存、パス同一性、開く経路、閉じたenum、状態の所有を維持した。追加されたI/OはBookmarkPortのread/writeに限定し、通常の起動/開く/保存/入力/切替/終了では呼ばないことを契約で確認。完全同時の複数窓の競合はADRで明示した範囲外。最終製品は実測した `764fa20` と同一で、以後の文書変更では成功済みの検証を再実行しない。PR #279に同じ検証と再利用根拠を記録する。

## 5-ce — Exから共通のファイル一覧（Issue #280・ADR 0064）

### 対象・依存・退行の根拠

FR-006 / D5 / D28の最後の入口を実装。`ExResult`の型付き一覧要求→`EditorController::run_palette_request`→既存`open_palette`を使う。`:e`は全候補、`:b` / `:ls`はタブ、引数は検索欄。`palette_marks`の逆引きも同じ表を使い、全候補の検索で先頭の記号を文字として保持する。候補列・照合器・読み込み・描画・IME処理・port・保存形式は変更しない。

固定した手元のVim 9.1（2024-01-03 build）の非対話probeを36ケース実行した。`python D:/NeNeNib/scripts/probe-ex-files.py`、`out/probes/ex-files-2026-10-03.json`。`e`〜`edit`、`b`〜`buffer`、`ls` / `files` / `buffers`の綴りを採用し、`fi` / `file` / `l` / `lis`が別命令であることを確認。Vimの直接open・reload・番号切替はFR-006の一覧を優先して実装しない。

### 自動検証

Windows 11・clang-cl 19.1.5の既存Debug環境（clang-tidy・ASan・UBSan）。各scopeは`build/nib_tests.exe <scope>`で実行した。

| 確認する退行 | コマンド | 結果・記録 |
| --- | --- | --- |
| 新しい公開型・controllerとの接続・静的規則 | `. ./eng/toolchain.ps1; cmake --build build --target nib_tests NeNeNib` | 成功、`out/280-build02.log`。テストの期待値修正後は`--target nib_tests`のみ再ビルド、`out/280-build03.log` |
| Exの名前・引数・拒否・256 bytes・記号と日本語・出どころ・選択・本文/undo/register・遅延open | `--ex-files` | 82 checks成功、`out/280-ex-files.log` |
| 共有Ex評価器と補完が既存設定を壊さない | `--ex-settings` | 195 checks成功、`out/280-ex-settings.log` |
| OpenTabListの共通化と既存Exタブ命令 | `--tabs` | 291 checks成功、`out/280-tabs.log` |
| 候補追加の順・共通入力・IME・照合の境界 | `--command-palette` | 356 checks成功、`out/280-command-palette-fixed.log` |
| モジュール境界・型・規約 | `python eng/conformance.py --build-dir build` | 0 violations、`out/280-conformance.log` |
| core/applicationにOS依存を足さない | `python eng/symbols.py --build-dir build --require core application` | 2 libraries / 0 violations、`out/280-symbols.log` |
| 変更C++の整形 | `clang-format --dry-run --Werror <変更C++>` | 終了0 |

初回ビルドでは新しいテストのoptional参照を静的解析が検出し、確認してから読む形へ修正した（`out/280-build01.log`）。初回`--command-palette`ではサナが書いた同点時の期待順が1件失敗。既存の設定候補は文字列順であることをコードで確認し、期待値を`ls, edit, tabs`へ直した。製品の順は変えていない。成功した他のscopeは再実行していない。

再利用: #278の5000候補ベンチ（5有効試行・中央値2.708ms、`out/speed/2026-10-03T06-38-44Z.json`）を再利用する。ファイル候補の生成・`listed_positions`・`CommandPalette`・描画・adapter・計測器・環境は同一。新しい逆変換は一覧を開く時だけで、打鍵区間へ入らない。Exの設定候補は5件増えたが全ファイルの照合対象には含まれない。全件Vim再生・全性能ベンチは不要。成功済みの実装・対象テスト・関連依存を変えない文書/PR/mergeの工程では再測定しない。

規則: ARC-001 / ARC-003 / ARC-004 / ARC-010 / ARC-012 / CPP-002 / CPP-004 / CPP-011 / CPP-012 / QLT-001 / QLT-012 / QLT-013。Waivers: none。Vimの互換範囲はADR 0064に明記。実機とRelease・保護対象の比較は以下に記録する。

### Releaseと保護対象の比較

`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` は実装`a0d4d0b9aa92aa5a880df8664f741fc6779a0075`で成功。`build/release-a0d4d0b/NeNeNib.exe`、1,288,192 bytes、SHA-256 `543C7BAF31024C8BCB7E045663B896CEEB49BA1CF2C9A67D2D1E2C020443ACE1`。configure 2.632s / build 148.375s。記録は`out/280-release.log` / `out/release/a0d4d0b.json`。この生成コマンドでは起動していない。

`python eng/protected-diff.py --base 4a13e4c --head a0d4d0b`は終了0（`out/280-protected.log` / `out/protected/a0d4d0b.json`）。実出力: `fixtures 1853 -> 1853 / metadata 0 / deleted 0 / changed 0 / added 0`、`files changed none`、`scopes 30 / same 0 / 未測 29`、新規`--ex-files`も未測。比較用exeを渡していないため、既存scope全件の件数不変の証明にはしない。perf-reference / symbol-allowlist / conformance-rules / SettingsCodecの静的差分は0。

### 実機の限定確認

専用profileと9場面の確認手順を準備して前面確認を依頼し、hideが2026-10-03 16:47 JSTに了承。`python D:/NeNeNib/scripts/ex-files-280.py --executable C:/Users/info/WORKS/NeNeNib/build/release-a0d4d0b/NeNeNib.exe`を1回実行し、終了0。ログは`out/280-window.log`、9枚のPNGと`record.json`は`out/frames-280/`。専用文書・profileは`D:/NeNeNib/evidence/ex-files-280/`。

サナが9枚すべてを読み、次を受理した。

- `:e`で空の検索欄と4候補が出る。タブと同じフォルダが同じ面に並ぶ。
- `:b alpha`は`#alpha`に1件へ絞り、確定前はbetaの本文のまま。Enter後は既存のalphaタブに移る。
- `:ls`はタブ2件だけで現在のalphaが選ばれ、未保存の印を表示する。betaへ移ってから`:buffer alpha`で戻っても`alpha body!`とカーソルが残る。
- `:e #日本語 memo`は先頭記号を検索文字として保持し、同じフォルダの1件が残る。Enterで新しいタブが開き、日本語本文が読める。
- `:edit!`は`Not supported: edit!`の1行。`:ls absent`は候補なし（0/0）で、Enterしても現在の文書を変えない。
- 終了前にalphaの編集をundoし、終了コード0とディスク上の元の本文をスクリプトが照合した。16:48 JSTにhideへ前面操作の終了を通知。

この確認ではIME変換そのものを再実行していない。日本語は共有入力へUTF-16の文字メッセージとして渡したもので、IMEの構えは`--ex-files`と`--command-palette`の対象契約、既存のUI/IME実装は不変性で確認する。

自己レビュー: ARC-001 / ARC-004は同じ表・評価器・一覧・open経路、ARC-003はsymbols、CPP-002 / CPP-004 / CPP-011は閉じた選択肢・安全なoptional参照・1ファイル1型、QLT-001 / QLT-012は対象指定と成功結果の再利用で確認。PR #281へ同じ検証を記録する。製品と対象テストは実測した`a0d4d0b`から変更していない。以降の文書・Ready・mergeだけを理由に製品テストを再実行しない。

## 5-cf — 通常モードの結合文字境界（Issue #282・ADR 0065）

2026-10-03。D38の「結合文字から段階的に」を受け、通常モードの左右とDeleteはアクセント・濁点・共通のVSまでを一つとして扱う。Backspaceはmark一つ、VSは直前のcode pointと消す。上下・ページの着地は途中なら先頭へ寄せる。`CaretMoveRequest`で通常とVim INSERTの単位を区別し、既存の選択・replace・undoへつなぐ。本文・UTF-8・保存形式は不変。

### 参照と限定

非表示の自前RICHEDIT50W controlへメッセージを送り、25ケースを観測した。`out/probes/ordinary-characters-2026-10-03.json` / `.log`、使用DLLは`C:/Windows/System32/msftedit.dll` 10.0.26100.8875、SHA-256 `490d872e894935df2df5f7f2b9b9bc6b00b91e74412be2af55714b301ef3e3ef`。前面化・本物の鍵・クリップボードは使っていない。Notepadパッケージ内のDLLの直接ロードはAccessDeniedで使えず、この参照をNotepad本体の試験とは呼ばない。Unicodeの全書記素境界や複雑な絵文字・各言語固有のまとまりはD38の後続範囲。

### 自動検証

Windows 11・clang-cl 19.1.5のDebug（clang-tidy・ASan・UBSan）。全件は実行せず、直接変更する境界だけを選んだ。

| 確認する退行 | コマンド | 結果・記録 |
| --- | --- | --- |
| 新しい型と共有移動の全呼び出し元、厳格な静的解析 | `. ./eng/toolchain.ps1; cmake --build build --target nib_tests NeNeNib` | 成功、`out/282-build02.log` |
| 結合文字/VS/孤立mark/UTF-8/CRLF/長い行とpiece/上下着地/Shift選択/削除/undoとredo、既存移動、Vim INSERT・外部DeleteText | `build/nib_tests.exe --ordinary-characters` | 233 checks成功、`out/282-ordinary-characters-final.log` |
| Vimの既存文字境界・移動/削除・fixtureが変わらない | `build/nib_tests.exe --vim-characters` | 665 checks成功、`out/282-vim-characters.log` |
| core/applicationへOS依存を持ち込まない | `python eng/symbols.py --build-dir build --require core application` | 2 libraries・違反0、`out/282-symbols.log` |
| 正典経路・型の配置・生成物の保護 | `python eng/conformance.py --build-dir build` | 違反0、`out/282-conformance-final.log` |
| 差分C++の整形と空白 | `clang-format --dry-run --Werror <差分C++>; git diff --check` | 終了0 |

初回ビルドはテストの補助型`Bytes`を誤った名前空間で参照して失敗した（`out/282-build01.log`）。既存のテスト型へ修正し、製品のゲートは変えていない。231 checks成功後、自己レビューでVimの外部DeleteText維持を明示する2 checksを追加した。製品は不変のまま`--target nib_tests`だけをビルド（`out/282-build03.log`）し、変更した`--ordinary-characters`だけを再実行した。`--vim-characters`の成功は再利用。

再利用: 製品・対象テスト・関連依存・環境が不変の文書/PR/mergeでは上記成功を再利用する。通常入力の挿入・一覧の照合・起動・ファイルI/O・描画は変えていないため、その全性能ベンチと無関係なscopeは実行しない。新境界の長い行の契約は時間を測らず、結果と終端を確認する。前面の確認はReleaseと専用手順を準備し、hideの了承後に下記の限定手順を実施した。

自己レビュー: ARC-001 / ARC-004は文字境界と移動単位をcoreへ一本化、選択・replace・undoは既存経路。ARC-003はsymbols、CPP-002 / CPP-011は網羅switch・1ファイル1型、QLT-001 / QLT-012は対象と再利用根拠で確認。Waivers: none。schema変更なし。Release・保護対象比較・実機の結果は以下に記録する。

### Releaseと保護対象の比較

`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD`は`f0efe72f476da2026219d7239f5015296a261c20`で成功。`build/release-f0efe72/NeNeNib.exe`、1,290,240 bytes、SHA-256 `445250FD4E42FF9E4FBCD67186357B5ABE0B211DBADBC39862F49CD2E289F058`。configure 2.218s / build 144.589s。`out/282-release.log` / `out/release/f0efe72.json`に記録した。この生成コマンドでは起動していない。

`python eng/protected-diff.py --base a787186 --head f0efe72`は終了0（`out/282-protected.log` / `out/protected/f0efe72.json`）。`fixtures 1853 -> 1853 / metadata 0 / deleted 0 / changed 0 / added 0`、`files changed none`、`scopes 31 / same 0 / 未測 30`、新規`--ordinary-characters`も未測。比較用exeを渡していないため、全scopeの件数不変の証明にはしない。`src/ui`・`src/adapters`・`eng`の静的差分も0。

### 実機の限定確認

専用profileと`combining.txt`を`D:/NeNeNib/evidence/ordinary-282/`に用意し、`D:/NeNeNib/scripts/ordinary-282.py --prepare`で作成した。今回分の前面確認を依頼し、hideが「やって」と了承した後、17:42〜17:43 JSTに`python D:/NeNeNib/scripts/ordinary-282.py --executable C:/Users/info/WORKS/NeNeNib/build/release-f0efe72/NeNeNib.exe`を1回実行。終了0、ログは`out/282-window.log`、9枚のPNGと`record.json`は`out/frames-282/`。17:43:06 JSTに前面操作の終了を知らせた。

サナが9枚すべてを読み、次を受理した。

- `e + U+0301`をRightでまとめて越え、表示は列3。Backspaceでaccentだけ外れた`ex`となり、undoで元の表示へ戻る。
- `か + U+3099`をShift+Rightで一緒に選択し、Delete後は`x`だけになる。
- `葛 + U+E0100`をDeleteで、`U+20000 + U+E01EF`をRight→Backspaceで削除すると、それぞれ`x`だけになる。
- `e + U+0301 + U+FE0F`のBackspaceはaccentとVSを除いて`ex`を残す。
- 5通りの削除後に保存し、全ファイルのUTF-8を期待値と照合してすべて一致。最後にundoで元の全バイト列へ戻して保存し、未保存印が消えたこと、終了コード0を確認した。

描画・IME実装は不変で、この確認ではIME変換自体を再実行していない。製品と対象テストは`f0efe72`から不変。以降の文書・PR・mergeでは898 checksとRelease・実機の成功結果を再利用する。PR #283へ同じ記録を載せる。

## 5-cg — Exの保存と終了（Issue #284・ADR 0066）

2026-10-03。FR-003 / FR-008 / D22に沿い、現在の文書の`:w` / `:q` / `:q!` / `:wq` / `:x`と省略名を追加。coreの名前表と閉じた要求を既存のEx評価から渡し、保存はGUIと共通の`save_document`、閉じる操作は既存の`CloseTab`。最後のタブの終了をuiの`deliver`末尾へ統一する。`:tabclose`やCtrl+F4の保存確認は維持する。

### 参照と限定

固定Vim 9.1を`-u NONE -i NONE -N -n -es`で非対話実行し、26命令×名前あり/無題・保存済み/未保存の4条件、計104ケースを観測した。`D:/NeNeNib/scripts/probe-ex-save-quit.py`、`out/probes/ex-save-quit-2026-10-03.json` / `.log`。`w` / `q` / `wq` / `x`系の省略、E32/E37、`:x`の変更時だけの保存を確認。専用文書以外への書き込み・前面化・クリップボード操作なし。ファイル名引数・範囲・追記・強制上書き・全タブ命令は本件の対象外。

### 自動検証

Windows 11・clang-cl 19.1.5・Debug（clang-tidy・ASan・UBSan）。共有保存とEx評価、タブ終了の直接依存に限定した。

| 確認する退行 | コマンド | 結果・記録 |
| --- | --- | --- |
| 型の追加・共有保存/終了の呼び出し・厳格な静的解析 | `. ./eng/toolchain.ps1; cmake --build build --target nib_tests NeNeNib` | 成功、`out/284-build02.log` |
| 省略と拒否、E32/E37、成功/失敗、文字コード/BOM/改行、undo/保存位置、他タブの保持、既存GUI保存 | `build/nib_tests.exe --ex-document` | 204 checks成功、`out/284-ex-document.log` |
| 新しい補完候補と設定の共有評価 | `build/nib_tests.exe --ex-settings` | 195 checks成功、`out/284-ex-settings-final.log` |
| `edit`と`exit`の候補衝突、ファイル一覧への要求 | `build/nib_tests.exe --ex-files` | 82 checks成功、`out/284-ex-files.log` |
| コマンド候補の順位と共通の入力/確定 | `build/nib_tests.exe --command-palette` | 356 checks成功、`out/284-command-palette.log` |
| 現在のタブを閉じた後の切替・dirtyと未読文書 | `build/nib_tests.exe --tabs` | 291 checks成功、`out/284-tabs.log` |
| 最後のタブを閉じた後のsessionの意味 | `build/nib_tests.exe --session` | 199 checks成功、`out/284-session.log` |
| 閉じるときだけのファイル履歴更新 | `build/nib_tests.exe --history` | 38 checks成功、`out/284-history.log` |
| core/applicationにファイルやOSの依存を増やさない | `python eng/symbols.py --build-dir build --require core application` | 2 libraries・違反0、`out/284-symbols.log` |
| 正典経路・型の配置・生成物 | `python eng/conformance.py --build-dir build` | 違反0、`out/284-conformance.log` |

計1365 checks。最初のビルドは`deliver`の認知的複雑度11が上限10を超えて停止（`out/284-build01.log`）。終了処理を`finish_tab_action`へ分けて修正し、規則は不変。最初の`--ex-settings`では補完末尾の旧期待値`buffers`が1件不一致だった。増えた名前表での正しい`exit`へテストを直し、`--target nib_tests`（`out/284-build03.log`）とこの195 checksだけ再実行した。他の6scopeの成功は関連入力不変なので再利用。差分C++の`clang-format --dry-run --Werror`と`git diff --check`も終了0。

自己レビュー: ARC-001 / ARC-004は名前表・保存関数・CloseTabの正典、ARC-003はsymbols、ARC-010 / CPP-002は閉じた要求と保存失敗、CPP-004 / CPP-011はoptional参照と1ファイル1型、QLT-001 / QLT-012は対象選択と再利用で確認。保存schema・port・adapterの変更なし。Waivers: none。

性能範囲: 起動・通常の文字挿入・一覧の照合/描画・ファイルI/Oの実装は不変。新しい保存/終了は明示したEx命令だけで実行され、通常打鍵のuiには終了のboolと条件分岐が加わるだけ。起動/巨大ファイル/5000候補等の全性能ベンチや全Vim再生は行わない。保存実装の抽出は既存保存の契約を新規scopeへ束ねて確認した。

製品・対象テスト・関連依存・環境が不変なら、文書・PR・mergeでは上記成功を再利用する。

### Releaseと保護対象の比較

`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD`は`d078f9a92cff3450724e8d26cdc9a867855a20e4`で成功。`build/release-d078f9a/NeNeNib.exe`、1,296,896 bytes、SHA-256 `10F0B2D3C2EA6B946236F89514D20C45BDE9969FAF56EB634EF829837D5A8505`。configure 2.158s / build 136.882s。`out/284-release.log` / `out/release/d078f9a.json`に記録。この生成コマンドでは起動していない。

`python eng/protected-diff.py --base ecf4aa9 --head d078f9a`は終了0（`out/284-protected.log` / `out/protected/d078f9a.json`）。`fixtures 1853 -> 1853 / metadata 0 / deleted 0 / changed 0 / added 0`、`files changed none`、`scopes 32 / same 0 / 未測 31`、新規`--ex-document`も未測。比較用exeを渡していないため、全scopeの件数不変の証明にはしない。perf-reference / symbol-allowlist / conformance-rules / SettingsCodecの静的差分0。

文書追加後は`conformance.document_checks(root, paths, rules)`だけを呼び、規則IDと相対リンク・強制状態の一致を確認して違反0（`out/284-doc-conformance.log`）。C++と関連依存は不変なので製品の検査は再実行しない。

### 実機の限定確認

専用profileとalpha/beta/blockedの3文書を`D:/NeNeNib/evidence/ex-document-284/`へ準備。Release生成と[PR #285](https://github.com/hideyukiMORI/nene-nib/pull/285)のDraft作成後、18:51 JSTに今回分の前面確認を依頼し、hideが「今、実行してよい」と了承した。18:54〜18:55 JSTに`python D:/NeNeNib/scripts/ex-document-284.py --executable C:/Users/info/WORKS/NeNeNib/build/release-d078f9a/NeNeNib.exe`を1回実行して終了0。`out/284-window.log`、7枚のPNGと`record.json`は`out/frames-284/`。18:55:01 JSTに前面操作の終了をhideへ通知した。

サナが7枚すべてを読み、次を受理した。

- 未保存のbetaの`:q`はE37を一行表示し、本文`beta!`・未保存印・両方のタブが残る。ダイアログも書き込みもない。
- `:w`で`Written`と保存済みの印へ変わり、実ファイルが`beta!`+LFに一致する。
- 続いて`?`を足した`:wq`は`beta!?`+LFを書いてbetaだけを閉じ、alphaへ戻る。
- 無題の`:w`はE32で止まり、`:q`は変更のない無題を閉じる。変更した無題の`:q!`もそのタブだけを閉じ、alphaを保つ。
- 最後の変更のないalphaを`:x`で閉じて終了0。本文と更新時刻が不変で、書き込みを省くことを確認する。
- 読み取り専用にした専用のblockedへの`:wq`は`Could not write file`を表示し、`blocked!`と未保存印を残す。元のディスク内容は不変。`:q!`で終了0。確認後は専用ファイルの読み取り専用属性を戻した。
- 既存の`:tabclose`は未保存の確認を出し、取消で本文を残す。続く実キーCtrl+F4にも確認があり、「保存しない」で最後のタブを閉じて終了0。ディスク内容は元のまま。

3回のプロセス終了すべてが0。製品・対象テスト・関連依存は`d078f9a`から不変で、文書更新時の`git diff --exit-code d078f9a -- src tests eng CMakeLists.txt`も差分0。PRのReady/mergeでは1365 checks・Release・実機の成功結果を再利用する。実機のIME変換・クリップボード・全件性能はこの変更の確認に必要なく、実行していない。

## 5-ch — 名前付きEx保存（Issue #286・ADR 0067）

2026-10-03。FR-003 / FR-008 / D8 / D22 / D39。`:w 名前` / `:wq 名前` / `:x 名前`と`:sav`〜`:saveas 名前`を既存保存へ接続。名前ありの別名writeはコピーで保存位置と名前を保ち、無題への保存とsaveasは成功時だけ命名する。hideは19:39 JSTに「良い。進めて。」とD39の推奨案を了承した。失敗時にも名前を変えるVimの一部の挙動とは意図的に異なる。

### 参照と検証範囲

固定Vim 9.1を`-u NONE -i NONE -N -n -es`で12命令×名前あり/無題・変更あり/なしの4条件、計48ケース観測した。`D:/NeNeNib/scripts/probe-ex-write-path.py`と`out/probes/ex-write-path-2026-10-03.json` / `.log`。コピー後のdirty、E13/E37、saveas、空白、失敗時の名前を確認。前面操作なし。範囲・強制保存・展開・追記・全タブ終了は対象外。

### 自動検証

Windows 11・clang-cl 19.1.5・Debug（clang-tidy / ASan / UBSan）。保存要求の呼び出し元と新規作成の境界へ限定した。

| 確認する退行 | コマンド | 結果・記録 |
| --- | --- | --- |
| FilePortの共有呼び出しと型・静的解析 | `. ./eng/toolchain.ps1; cmake --build build --target nib_adapter_tests nib_tests NeNeNib`、後続は`nib_tests NeNeNib` / `nib_tests`だけ | 成功、`out/286-build02.log`・`04.log`〜`07.log` |
| 新規作成/既存拒否、他者の一時ファイル保護、失敗した置換後の元ファイルと一時ファイル、既存形式・パス | `build`から`nib_adapter_tests.exe --files` | 64 checks・0 failures、`out/286-files.log` |
| 名前/空白/Windowsパスと拒否、コピーと命名、D39と変換失敗、undo/保存位置、終了条件、他タブ保護 | `build/nib_tests.exe --ex-write-path` | 90 checks成功、`out/286-ex-write-path-final.log` |
| 引数なしの既存Ex保存・終了とGUIの共通保存 | `build/nib_tests.exe --ex-document` | 204 checks成功、`out/286-ex-document.log` |
| 新候補saveasと共有設定評価 | `build/nib_tests.exe --ex-settings` | 195 checks成功、`out/286-ex-settings.log` |
| Ex解析の名前追加と既存のファイル一覧要求 | `build/nib_tests.exe --ex-files` | 82 checks成功、`out/286-ex-files.log` |
| 共通入力/確定と補完の順位 | `build/nib_tests.exe --command-palette` | 356 checks成功、`out/286-command-palette.log` |
| OS境界と未宣言の外部依存 | `python eng/symbols.py --build-dir build --require core application` | 2 libraries・違反0、`out/286-symbols-final.log` |
| 正典・型配置・依存・文書 | `python eng/conformance.py --build-dir build` | 違反0、`out/286-conformance.log`（文書追加後の記録は末尾へ） |

計991 checks。最初の基盤のEx204（`out/286-ex-document-baseline.log`）はcontroller実装後に再実行したため二重計数しない。adapter64はadapter/port/対象試験を変更していないため再利用。後半の修正は新規のファイル名の禁止文字判定と新規scopeのテスト準備のみで、引数なしEx・設定/候補・既存ファイル一覧の意味と対象試験は不変。837 checksを再利用する。タブの保存/同一パスの新しい呼び出しは新規scope内で確認し、既存CloseTab/session/historyは変更していないため#284の成功結果を再利用する。

初回の機能ビルド（`out/286-build03.log`）はテストのoptional未検査参照と認知的複雑度11（上限10）で停止。参照を束縛してガードを認識できる形にし、失敗1ケースの準備を関数へ分けた。規則は不変。初回の新規scopeは2/90失敗（`out/286-ex-write-path.log`）。表示高さの指定漏れと、行を`|`で連結する補助関数でLFを期待したテスト側の誤りを直した。本文は`vim_body`で評価する。symbolsは`string_view::find_first_of`由来の未宣言`memchr`を1件検出（`out/286-symbols.log`）。新しい文字判定を純粋な文字比較へ変更し、許可リストを変更せず違反0になった。修正したscopeとsymbolsだけを再実行した。

自己レビュー: ARC-001 / ARC-004は共通`save_document` / `FilePort::write` / `CloseTab`、ARC-003 / ARC-007 / ARC-009はresolveとOS APIのadapter境界、ARC-010 / CPP-002は閉じた要求と失敗、CPP-004 / CPP-011はoptionalと1ファイル1型、QLT-001 / QLT-012は差分に対応する範囲と再利用、QLT-013はReleaseと前面操作の分離で確認。UIの変更は失敗enumの網羅だけ。保存schema・性能基準・waiverに変更なし。

性能範囲: 新しい処理は明示したEx保存時だけで、通常打鍵・描画・一覧照合・起動の経路は不変。既存replaceのI/O列は維持し、新規作成モードは存在確認と上書きしない最終配置を使う。全性能/全Vimを再実行する根拠はない。Releaseと専用ファイルの実機確認を別途記録する。

差分C++の`clang-format --dry-run --Werror`、`git diff --check`、文書追加後の`python eng/conformance.py --build-dir build`が成功（違反0、`out/286-conformance-final.log`）。

### Releaseと保護対象の比較

`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD`はcleanな`591b99fa7bbd2b6a4149fe45b81c81cafeb3e03a`で成功。`build/release-591b99f/NeNeNib.exe`、1,302,528 bytes、SHA-256 `312B55BE0A62F8593CEB95AF1EDC87EB13C3BB7C7E39A739948A43DF939C66E6`。configure 2.148s / build 138.892s。`out/286-release.log` / `out/release/591b99f.json`。生成だけで起動はしていない。

`python eng/protected-diff.py --base 12a0a83 --head 591b99f`は終了0（`out/286-protected.log` / `out/protected/591b99f.json`）。`fixtures 1853 -> 1853 / metadata 0 / deleted 0 / changed 0 / added 0`、`files changed none`、`scopes 33 / same 0 / 未測 32`、新規`--ex-write-path`も比較器では未測。比較用exeを渡していないため全scopeの件数不変の証明にはしない。perf-reference / symbol-allowlist / conformance-rules / SettingsCodecの静的差分0。

[PR #287](https://github.com/hideyukiMORI/nene-nib/pull/287)をDraftで作成。専用profileと文書、8場面の手順は`D:/NeNeNib/scripts/ex-write-path-286.py`、専用文書は`D:/NeNeNib/evidence/ex-write-path-286/`。`--prepare`は成功し、前面操作は行っていない。20:02 JSTに今回分の前面確認を依頼した。実機結果は未確認で、了承後に同じReleaseを確認して記録する。

### 実機の確認

hideの「やって」を受け、20:09 JSTに`python D:/NeNeNib/scripts/ex-write-path-286.py --executable C:/Users/info/WORKS/NeNeNib/build/release-591b99f/NeNeNib.exe`を実行。名前あり文書の5場面は成功し、`out/frames-286/01`〜`05`のPNGをすべて目視で受理した。コピー後の名前/未保存印、E13で既存ファイル保護、コピーを作ったwqのE37、失敗したsaveasの旧名保持、相対の日本語名でのsaveas成功を確認。alphaの元バイト列、コピー2件の全バイト列、既存ファイルの不変も照合した。続く変更のないxは指定先を作らず、保存先の更新時刻も変えず終了0。

初回は無題文書のタイトル比較で停止して終了1（`out/286-window.log`）。入力をPostMessageで送った直後、処理完了前にもタイトルを読む手順だったため、未保存の無題タイトルへの反映を待ってから比較するよう外部スクリプトを修正した。製品コードは変更していない。20:09:40 JSTに前面操作を終了したと通知し、20:11 JSTに残り3場面だけの再開を依頼した。成功した5場面と正常終了1回は保持し、再実行しない。タイトル保持・無題の命名/同名保存終了・他タブ保護の残りは未確認。


hideの再開指示「やって」を受け、20:18〜20:19 JSTに残りだけを`python D:/NeNeNib/scripts/ex-write-path-286.py --remaining --executable C:/Users/info/WORKS/NeNeNib/build/release-591b99f/NeNeNib.exe`で実行して終了0（`out/286-window-remaining.log`）。処理後に比較した無題のタイトルは、保存失敗前後とも`● 無題 - NeNe Nib`で一致した。無題へ名前を付けて`draft`を書き、同じパスへのwqで`draft!`を書いて終了0。別の開いているタブへのsaveasはE139で拒否し、両文書の全バイト列を保持した。両タブは通常のqで閉じ、終了0。一時ファイルの残りもない。

20:19:15 JSTに前面操作終了をhideへ通知。サナが残りの06〜08を含む8枚すべてを読み、メッセージ・本文・名前・未保存印を受理した。正常終了は計3回（初回の中断した無題プロセスはこの数に含めない）。画像と`record.json`は`out/frames-286/`。初回に完了した5場面とcopy/saveas/clean-xの結果は保存して再利用し、残り3場面だけを再実行した。製品の修正・Release再生成・成功済みの自動テスト再実行は不要だった。

製品・対象テスト・関連依存は591b99fから不変。文書追加時の`git diff --exit-code 591b99f -- src tests eng CMakeLists.txt`が差分0。文書だけは`conformance.document_checks`で確認し、違反0（`out/286-doc-conformance-final.log`）。Ready/mergeでは991 checks・Release・8場面の成功を再利用する。実機IME・クリップボード・全件性能は本件の変更に必要なく実行していない。

## 5-ci — 名前付きEx仕様の追補とsplit前の区切り（Issue #288）

2026-10-03。FR-003 / FR-008 / D39 / QLT-001 / QLT-012 / GIT-001〜004。SPECIFICATION.md第5節に残っていた#284時点の説明を#286 / ADR0067の実装へ揃えた。文書だけの変更で、splitの製品挙動は加えない。main統合後、注釈付き`checkpoint/pre-split-20261003`を置き、別Issueで分割表示の範囲・速度の採用条件・限定試作を扱う。

確認する退行は仕様と実装の矛盾、文書の規則ID/リンク/強制状態と空白だけ。`conformance.document_checks(root, inventory(root), rules)`は違反0（`out/288-doc-conformance.log`）。`git diff --check`は終了0。`git diff --exit-code 591b99f -- src tests eng CMakeLists.txt`は差分0で、#286の製品・試験・関連依存を変更していない。

再利用: [5-ch](#5-ch--名前付きex保存issue-286adr-0067)の991 checks、Release生成、実機8場面と正常終了3回を再利用する。Releaseは`build/release-591b99f/NeNeNib.exe`、SHA-256 `312B55BE0A62F8593CEB95AF1EDC87EB13C3BB7C7E39A739948A43DF939C66E6`。実行元コミットは591b99fa7bbd2b6a4149fe45b81c81cafeb3e03a、製品の統合コミットは3d055dad43d3b87a38df9d8b3fa566298c29344a。性能の既存記録は5-cc / 5-cdで、タグだけを理由に測り直さない。splitの採用前後の比較条件は次Issueで定める。

自己レビュー: D39・ADR0067と保存系の解析/共通保存を読み合わせ、コピーと命名、失敗、E32/E37、残る未対応範囲を明記した。ゲート・schema・製品挙動の変更なし。Waivers: none。タグは開発の区切りで、配布Releaseではない。ローカルとoriginのタグ照合は統合後に行い、タグの注釈と後続の準備記録へ実際の参照先を残す。

## 5-cj — splitの下準備と限定した描画費用（Issue #290・ADR 0068）

2026-10-03。FR-003 / FR-005 / D6 / D12 / D22 / D40 / ARC-001 / ARC-003 / ARC-004 / ARC-007 / ARC-009 / ARC-011 / CPP-002 / CPP-012 / QLT-001 / QLT-012 / QLT-013 / QLT-014。hideの「その流れで進めて」「推しでいいと思う」「外出するので進めておいて」を受け、単体で下準備と描画実験を実施。文書タブ維持・最大2枠・本文/undoと枠の位置を分ける案、閉じる意味、未検証の境界を[準備資料](../design/2026-10-03-split-preparation.md)へ記録した。製品採用と大きな所有移行は実測後に判断する範囲で、T3の初版範囲を実装済みに変えていない。

### 比較点と試作の追跡

PR #289を統合したclean mainは`088481b0c79436e9616445559aa4268225bd2ac0`。`git tag -a checkpoint/pre-split-20261003 088481b0c79436e9616445559aa4268225bd2ac0 -F D:/NeNeNib/briefs/tag-pre-split-20261003.txt`と`git push origin refs/tags/checkpoint/pre-split-20261003`が成功した。`git cat-file -t refs/tags/checkpoint/pre-split-20261003`はtag、`git rev-parse 'checkpoint/pre-split-20261003^{}'`と`git ls-remote origin 'refs/tags/checkpoint/pre-split-20261003*'`の展開先はともに088481b。注釈オブジェクトは`6eae6f7657a9903d3a2559d74ba160fb4f7ff049`。Releaseの生成元・SHA-256・5-ch / 5-ciの再利用を注釈に記録した。

測定条件は`7e14f8d`で試作前にコミットした。`test/290-split-render-probe`はDirect2DRenderer.cpp / .hppだけを変える実験用ブランチ。本文の矩形を分割し、同じframeを既存のdraw_body / draw_lineで描く。1窓・1device・1swap chain・1回のPresentを維持する。二つ目の文書やframe生成、独立した位置・操作・IME・閉じる挙動は実装しない。このブランチを製品へ統合しない。

| 版 | 生成元 | 実行ファイル | Release生成の結果 |
| --- | --- | --- | --- |
| A 元版 | 591b99f（製品は088481bと同一） | `build/release-591b99f/NeNeNib.exe` | 5-chの成功を再利用。SHA-256を再照合 |
| S 1枠の試作 | 5fc65010482e38de8ce7be223f5b562b4b02cb5c | `build/release-5fc6501/NeNeNib.exe` | `pwsh -NoProfile -File eng/build-release.ps1 -Ref 5fc65010482e38de8ce7be223f5b562b4b02cb5c` 終了0、147.233秒 |
| H 上下2枠 | cb1992bd1c28ed65c840cb7bc1393680081d4190 | `build/release-cb1992b/NeNeNib.exe` | 同コマンドのRefをcb1992bd1c28ed65c840cb7bc1393680081d4190として実行、終了0、147.490秒 |
| V 左右2枠 | 7a4a6ac4078c994349823fd5648528b4a67de5a4 | `build/release-7a4a6ac/NeNeNib.exe` | 同コマンドのRefを7a4a6ac4078c994349823fd5648528b4a67de5a4として実行、終了0、154.384秒 |

ビルドの目的は、試作のC++/型/描画呼出しと静的解析を既存の警告設定のまま確認し、測定元を固定すること。ログは`out/290-build-single.log` / `290-build-stacked.log` / `290-build-side-by-side.log`、各版の正式メタデータは`out/release/<短いSHA>.json`。全exeのSHA-256・ツール版は[実測JSON](split-render-2026-10-03.json)にも保存した。新しいゲート・例外・描画器・製品用feature flagは作らない。

試作3コミットと準備文書の親は`D:/NeNeNib/evidence/290-split/source/probe.bundle`へ保存した。`git bundle verify`が成功し、必要な親は088481b。bundleのSHA-256は`CD71A2F2D7E4AC191D72F55CA83A1E32BC4243A115DF9E8F1DFC191737A366F4`。同じディレクトリに各版のsrc差分も保存した。追跡用bundleから`git fetch <bundleのパス> refs/heads/test/290-split-render-probe`で実験の履歴を復元できる。

### 画と測定器

外部スクリプト`D:/NeNeNib/scripts/290-split-render.py`は、既存`eng/measure-speed.py.keys_trial`と`eng/window_driver.py`を共用する。起動時だけ窓を(80,80)・1280×800へ固定し、専用profile、前面/覆われていないこと、DPI120を確認する。計測器の入力/TimingPort読取の実装は複製しない。新しい集計は6組の対応差と欠測の分離だけで、構文検査と6組/欠測1本の短い合成例を確認した。どちらも終了0。実測中はビルドやアプリのテストを走らせない。

`python D:/NeNeNib/scripts/290-split-render.py --manifest D:/NeNeNib/briefs/290-split-manifest.json --capture-only`は終了0（`out/290-preflight.log`）。4版の画像を目視し、単画面・上下・左右の矩形とクリップを確認。`python eng/compare-frames.py out/split-290/captures/preflight-A.png out/split-290/captures/preflight-S.png`はdifferentPixels=0 / bounds=null / 終了0。本文の行数はA/Sで19、Hで9×2、Vで19×2。同じframeを再使用するためキャレットも各枠に出る試作であり、操作中の枠のUXを受理した証拠ではない。

続いて22:38〜22:47 JSTに`python D:/NeNeNib/scripts/290-split-render.py --manifest D:/NeNeNib/briefs/290-split-manifest.json`を1回実行、終了0（`out/290-paired.log`）。3本文×4版×6組の72試行、欠測0、再試行0。奇数組A/S/H/V、偶数組V/H/S/A。全数値と環境・ファイルのhashは[実測JSON](split-render-2026-10-03.json)、元の集計は`out/split-290/paired.json`、生のmarksと画像はその下位ディレクトリ。空文書と長い日本語行の8枚も目視済み。16MiBの測定時4枚は、目視した事前4枚とPNGの全bytesが一致した（計12枚目視＋4枚一致確認）。

`python D:/NeNeNib/scripts/290-split-audit.py`は終了0（`out/290-audit.log` / `out/split-290/audit.json`）。全72本で202入力、暖機の提示→1文字→その提示→200文字の順を検証し、raw marksから1文字/200文字の時間を再計算して記録値と一致した。窓寸法・DPIも全72本で一致。集計器の初版は本文内の片方の指標だけを揺れで保留していたが、事前文書の「本文条件全体を保留」に合わせて判定記録を修正した。元の72試行と測定器は変更せず、再計測もしない。

### 結果と次の境界

1文字の中央値A/S/H/V（ms）は空文書0.9375 / 1.007 / 1.213 / 0.9255、16MiB短行1.9225 / 1.8195 / 1.6915 / 2.5325、長い日本語行326.2945 / 326.1265 / 314.106 / 657.696。200文字は空3.4275 / 3.676 / 4.0745 / 3.448、短行10.423 / 9.6725 / 9.7105 / 10.3015、長行487.3225 / 486.2145 / 471.791 / 811.316。6本しかないのでp95とは呼ばず、全数値・中央値・最小/最大と対応差を残した。

空文書は事前条件内。16MiBは元版自身の前半/後半の1文字の中央値差0.208msが許容0.19225msを越え、本文条件全体を保留。左右の1文字の増分0.610ms・総時間2.5325msも条件外だが、この条件で確定した退行とは主張しない。長い日本語行は6217 bytes/2077 code pointsの行を1000行。左右は全6組で元版より遅く、1文字の増分331.4015ms、200文字323.9935msで条件外。上下も1文字の総時間2msを満たさない。元版自身の長い行の課題は#291へ分離した。原因の内訳はまだ未計測で、今回修正を始めない。

**splitの製品採用と文書/枠の所有移行は保留。** 既存ゲートの合格を速度維持の証明にせず、測定後に条件を緩めない。同じframeを二度描く下限寄りの試作であり、独立した文書・frame生成・位置補正・undo・保存・IME・hit test・sessionの正しさや性能は未検証。次に進むなら#291で単画面の支配的な費用を特定してから再判断する。

全110ファイル（元の時刻・画像・ソースbundle/差分・測定スクリプト・manifest・ビルド記録）を`D:/NeNeNib/evidence/290-split/split-render-evidence.zip`へ保存し、ZIPのCRC検査も成功した。1166821 bytes、SHA-256 `cc8ae74fb956b24bcfb1cfe6b2d00c03e207015824003671afebfbb4462e1b57`。exe自体は含めず、既存Releaseと各生成元・hashで追跡する。

### 統合する差分と再利用

統合するのはSPECIFICATION・ADR/索引・設計・日報/current・本節・実測JSONだけ。`git diff --exit-code checkpoint/pre-split-20261003 -- src tests eng CMakeLists.txt`は終了0。`python eng/protected-diff.py --base 088481b --head 7e14f8d`も終了0（`out/290-protected.log` / `out/protected/7e14f8d.json`）で、fixtures 1853→1853、metadata/deleted/changed/addedはいずれも0、files changed none、scopes 33 / same 0 / 未測33。exeを渡しておらず、全scopeの件数を測った証明ではない。

文書の規則ID/リンク/状態・空白の不整合だけを対象に、追記後の`conformance.document_checks(root, inventory(root), rules)`は違反0（`out/290-doc-conformance.log`）。`git diff --check`も終了0。製品の実装・試験・関連依存は区切りのタグと同じなので、5-chの製品検証とReleaseを再利用し、文書の統合を理由に製品テストや72試行を再実行しない。全件テスト・新しいVim oracle・IME/クリップボードの試験は選んでいない。製品schema変更なし、実測記録JSONを追加。Waivers: none。

## 5-ck — 長い行の文字組み再利用（Issue #291・ADR 0069）

2026-10-03〜04。FR-012 / FR-015 / FR-017 / D12 / D14 / D15 / ARC-001 / ARC-004 / ARC-007 / ARC-011 / CPP-011 / CPP-012 / CPP-016 / CPP-017 / QLT-001 / QLT-012 / QLT-013 / QLT-014。hideの続行指示に沿い、単体で原因の計測・修正・表示検証・split再比較を実施した。

### 原因の切り分けと製品の差分

`test/291-render-profile`で既存TimingPort/Milestoneだけに一時計測点を追加し、controller.apply、frame生成、UTF-16、layout作成、DrawTextLayout、EndDraw、待機と提示を分けた。時計はadaptersのQPCだけ。容量は一時的に65536へ増やし、全試行で未到達を確認した。最初の計測版は節目の関数長制限で落ち、未使用の3節目を除いて修正した。抑制と閾値変更はない。

`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD`でfedf727、続いてGetMetricsを先行させる比較版a0b2c57を生成し、各々終了0。ログは`out/291-profile-build-2.log` / `out/291-shaping-build.log`、メタデータは`out/release/<SHA>.json`。各版に`python D:/NeNeNib/scripts/291-render-profile.py --release out/release/<SHA>.json`を実行（後者は`--name shaped`）、各6本を固定して終了0。各版は空文書3本＋長い日本語行3本、1280×800 / DPI120 / Cascadia Code13.5pt。全202入力・1文字の独立・節目の組を確認した。

長い行の中央値は325.999ms、19行のDrawTextLayoutが322.712ms。GetMetricsの先行版では文字組みを含むlayout作成が311.463ms、DrawTextLayoutが9.890ms。frame生成0.705ms、待機0.001msに対して、同じ表示行を毎回組み直すことが支配的だった。全段階の試行値は[実測JSON](long-line-render-2026-10-04.json)のprofileに保存した。

製品の修正はa1a0e4d50a7c9eb784652a4d5ee65e11e7953e0dのui/win32の3ファイル。BodyTextLayoutとDirect2DRenderer.cpp / .hppに、全文・幅・行高が一致する現在/直前のlayoutの再使用と、書式再生成時の破棄を入れた。通常行・IME・クリックは既存layout_ofの一本。文字の範囲、fallback、描画API、編集状態、schemaは変更しない。計測点・先行GetMetrics・2枠試作は製品へ入れない。

### 対象にした表示の検証

退行の対象は、再使用によって古い文字・位置・書式が残ることと、共有layoutに選択等の表示が混入すること。`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD`はa1a0e4dで終了0（`out/291-cache-build.log`）、警告とclang-tidyは既存設定。1307648 bytes、SHA-256 `20BDE40B142DCC16409E5CE4973B1FBA91B5FB739FB474835BDD35A88CF0FC4D`。

変更3ファイルへclang-formatのdry-run、conformance.source_checksを選び、ADR 0069へdocument_checksを実行して違反0。型の配置・禁止API・抑制・文書契約を確認するためで、無関係なcore/applicationの全件テストは実行しない。

`python D:/NeNeNib/scripts/291-layout-visual.py --executable <Release> --name before|after`を591b99fとa1a0e4dへ実行し、`--compare`は終了0。22場面の本文/ステータス全画素が一致（`out/291-visual/comparison.json`）。Tab、結合文字、異体字、絵文字、制御文字、双方向文字を含むfixtureで、再描画・入力・Backspace・改行・undo・3か所のクリック・選択・スクロール往復・窓幅往復・Vimのブロックキャレット・検索・フォントサイズ/名・テーマを確認した。刺激で画像が変わることと、設定の実保存値も検査した。初回の検証スクリプトはguifontの`:h<pt>`を欠き設定変更を検出できず停止したため、スクリプトだけを修正して再実施した。失敗した記録はbefore-invalid-fontへ保存した。

既存`eng/verify-window.py`のverify_imeだけを専用profileで呼び、終了0（`out/291-visual/ime/result.json`）。日本語IMEの実入力で下線189画素、変換後の強調186画素、確定後の下線0を確認し、IMEの開閉状態を元の0へ戻した。DPIは実機120で、別DPIへの実移動とdevice lostの実機再現はしていない。フォント変更と同じ資源破棄へ通るコードを確認した。

### 通常版とsplitの固定比較

Oは変更前591b99f、Aは修正通常版a1a0e4d、S/H/Vは4d60d79 / 58a48fb / fc0b4c0。試作は#290と同じ矩形・クリップ・同じframeの二度描画で、各Refをbuild-release.ps1で生成して終了0。Sは143.923秒、Hは176.354秒、Vは178.419秒。H/Vは隔離したworktreeから生成した。メタデータと全hashは実測JSONに保存し、ビルドを全て終了してから測った。

測定前にIssueへ3本文×6組×5版＝90本、奇数O/A/S/H/V・偶数V/H/S/A/O、欠測は記録して再試行なし、O/Aの前半後半の安定を要する通常版比較、#290と同じsplit条件を記録した。
`python D:/NeNeNib/scripts/291-paired-render.py --manifest D:/NeNeNib/evidence/291-paired/manifest.json`は終了0。通常版の改善とsplitを一度に測り、別々の測定を重複させなかった。窓は1280×800、DPI120、各回新規profile、fixtureと字体は#290と同じ。測定器は既存keys_trialを共用した。

`python D:/NeNeNib/scripts/291-render-audit.py`は終了0（out/291-render-audit.log）。90試行のうち87本は正確な202入力、暖機/1文字の独立、200文字後の提示、rawからの時間再計算、寸法/DPIを検証した。empty-3-O、empty-3-S、long-japanese-2-Oは207入力だったため欠測として保持した。値の補完・試行の置き換え・再試行はない。全て測れている修正通常版Aは3条件とも6本。

観測された1文字 / 200文字の中央値（ms）。Oの空/長行とSの空は5本、他は6本。欠測がある数値も見せるが、性能受入の合格値とはしない。

| 本文 | O 変更前 | A 修正版 | S 1枠試作 | H 上下 | V 左右 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 空文書 | 0.893 / 3.708 | 0.852 / 3.339 | 0.798 / 3.277 | 0.914 / 4.094 | 0.830 / 3.393 |
| 16MiB短行 | 1.770 / 9.800 | 1.171 / 9.520 | 1.140 / 8.564 | 1.157 / 8.704 | 1.720 / 8.992 |
| 長い日本語行 | 333.784 / 485.280 | 28.616 / 186.354 | 28.941 / 194.542 | 28.309 / 182.767 | 38.173 / 198.512 |

長行の1文字の観測中央値は91.43%小さく、対応が取れた5組は全て292〜321ms短縮した。200文字も同じ5組が全て285〜314ms短縮した。ただし通常版の厳格比較は、空/長行のOに欠測、短行のOの200文字に前半11.005ms→後半9.219msの揺れ1.786ms（許容0.980ms）があり、3条件とも保留。成功したAの18本を失敗扱いにはしないが、O/Aの安定が必要という事前条件を外して合格にしない。

splitはAの6本が全条件で安定していた。空のH/Vと短行のS/Hは事前条件内。空のSは欠測、短行のVは1文字追加0.549msが0.5msを越える。長行のSは200文字追加8.188msが許容18.6354ms以内でも5/6で増えたため、少差の一貫した増加を保留する条件に該当。H/Vの1文字の合計28.309 / 38.173msは2ms条件外で、Vの200文字追加12.1575msも1ms条件外。splitの製品採用と文書/枠の所有移行は引き続き保留する。同じ文字列を両枠へ出すため、cacheを共有できる有利な条件であり、独立した文書の費用は未測。

### 証拠、自己レビューと受入状態

profile.bundleとsplit-recheck.bundleはgit bundle verifyで成功し、共にmainの936b50bを必要な親として保存した。後者は修正元a1a0e4dも含むため、試作枝を消してもソースを復元できる。各試作の差分も保存した。
全235ファイルの束は`D:/NeNeNib/evidence/291-render/long-line-render-evidence.zip`、9082031 bytes、SHA-256 `6e1fe550cf41204aa58a3e739ff39ede1eae2fb384833f89e4f870bd239397e1`。ZIPのCRC検査は成功。exeは含めず、全Releaseの生成元とhash、fixture、raw時刻、画像、ソースbundle、測定器を含む。

自己レビュー: 本文/履歴を複製せず描画資源だけをuiに置く（ARC-001 / 004 / 011）。時刻と追加の非決定的入力を製品へ足さない（ARC-007）。COMはComPtr、移した空のlayoutは一致対象から除外し、フォント名/サイズ/DPIの書式再生成とframe終端で破棄する（CPP-016）。全ての本文の描画/IME/hit testがlayout_ofに通り、選択/検索の処理は共有layoutを書き換えない。CPP-011 / 012は型の配置と通常Releaseの静的解析で確認した。

90試行の時点では実装・表示検証済み、性能受入とmergeは保留。draft PRで差分と証拠を保持した。ゲート/既存速度基準値/schemaの変更なし、Waivers: none。00:40にhideから、測定中にキーボードを操作していたとの回答を得た。予定外の入力があった3本は欠測のまま保存した。

文書の整合はconformance.document_checksで違反0（`out/291-doc-conformance.log`）、`git diff --check`は終了0。`git diff --exit-code a1a0e4d -- src tests eng CMakeLists.txt`も終了0で、文書をまとめた後も検証対象の実装と依存は変わっていない。

再利用: a1a0e4d以降の変更は文書だけで、製品の3ファイル・対象検証・関連依存は不変。22場面の一致、IME、Releaseと今回の測定をPR工程だけの理由で繰り返さない。残るリスクは、編集した長い行と初めて見える行の全文字組み、frame生成、同じ描画の再実行、表示行ぶんの資源保持、DPI/device lostの実機未確認、および欠測/対照揺れによる厳格比較の未判定。

### 手動入力のない時間に行った通常版36本の別比較

00:47にIssueへ別計画を先に保存し、hideの「いいよ」を受けて00:53〜00:57に実施。O/Aだけを3本文×6組＝36本、奇数O/A・偶数A/Oの順に固定した。以前の90本は上書き・補完せず、バイナリ、fixture、窓、DPI、字体、keys_trialと判定条件は同じ。表示22場面、IME、Release、split比較は再利用して繰り返していない。

`python D:/NeNeNib/scripts/291-normal-paired.py --manifest D:/NeNeNib/evidence/291-normal-paired/manifest.json`は終了0。続く`python D:/NeNeNib/scripts/291-normal-audit.py`も終了0（`out/291-normal-audit.log`）。36本全て正確な202入力で欠測なし。暖機/1文字の独立、raw時刻からの再計算、窓1280×800 / DPI120も一致した。実測JSONのnormalRecomparisonに原記録と監査を追加した。

| 本文 | O 変更前 1文字 / 200文字 ms | A 修正版 1文字 / 200文字 ms | 事前条件での判定 |
| --- | ---: | ---: | --- |
| 空文書 | 0.8895 / 3.5225 | 0.8095 / 3.8920 | Aの200文字が前半/後半で不安定のため保留 |
| 16MiB短行 | 1.8190 / 10.1640 | 1.4485 / 8.7405 | O/Aとも安定、受入条件内 |
| 長い日本語行 | 333.9125 / 498.3855 | 30.2085 / 194.2555 | O/Aとも安定、受入条件内 |

長行の1文字は90.95%、200文字は61.02%短縮し、両方とも対応する6組全てで短縮した。短行も1文字6/6、200文字5/6で短縮した。
空文書は中央値の増分が1文字-0.0800ms、200文字+0.3695msで許容内だが、Aの200文字の前半4.494ms→後半3.256msの差1.238msが許容0.500msを超えた。Oの200文字は前半3.537ms→後半3.508msで安定。今回も空文書全体を保留とし、中央値だけで合格にしない。入力混入による欠測は解消したが、この揺れの原因は未特定であり、退行確定とも無関係な測定ノイズとも断定しない。追加の再試行は行わない。

別比較の束は`D:/NeNeNib/evidence/291-normal-paired/normal-render-evidence.zip`、57ファイル、1105344 bytes、SHA-256 `73580c19e1ec17ed7a1c7b1b7cc6a654e267dbc51c706a65c3f69c3ceb5547e5`。CRC検査成功。36本のraw、画像、fixture、測定器、事前計画、Releaseメタデータを含み、ソースは元の235ファイルの束をhashで参照する。

**現在の残件は空文書の連続入力の安定性。** [PR #293](https://github.com/hideyukiMORI/nene-nib/pull/293)はdraftを維持し、性能受入全体とmergeは保留。splitの採用保留も変わらない。成功した短行・長行と表示確認は保存して再利用し、原因を特定せず合格まで測り直すことはしない。基準値・閾値・schemaは変更せず、waiverなし。

追記後は文書の規則参照とPRの検証記録だけを確認した。document_checksは違反0（`out/291-normal-doc-conformance.log`）、`python eng/git-conventions.py D:/NeNeNib/briefs/pr-291-long-line-rendering.md --pr-body`と`git diff --check`は終了0。製品・対象テスト・依存はa1a0e4dから変わらず、アプリの再検証は行っていない。

### 並行作業がある環境での空文書の内訳診断

hideから他の作業が並行しGPUも使われているとの説明と続行指示を受けた。01:14にIssueへ固定12試行の診断計画を先に保存し、他プロセスや設定を変えずに実施した。共有負荷はhideからの説明であり、GPU占有率を別途計測したものではない。

変更前の計装版fedf727を再利用し、同じ計測点だけをa1a0e4dへ重ねた2d8a10eを`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD`で生成、終了0（`out/291-empty-profile-build.log`）。151.036秒、1308160 bytes、SHA-256 `88F95A6BE15095518474CDAF659C74781EE10F948008DF5C5115F4B04A8476DC`。2版の製品コードの追加/削除行が、検証済みのcache修正だけであることも比較した。最終的な製品枝には計装を入れない。GetMetrics先行版は使っていない。

`python D:/NeNeNib/scripts/291-empty-diagnostic.py`を01:19〜01:20に実行して終了0。空文書6組、奇数O/A・偶数A/Oの12本は全て202入力、欠測なし。単打の独立、全節目の対、rawからの時間再計算、1280×800 / DPI120を確認した。節目数はOが709、Aが706で容量65536に未到達。集計処理は先に保存済みのprofileのrawでも確認した。単打・200文字全体・最後の入力以降を分解し、重なるUTF変換/文字組み/描画の区間を二重加算していない。各区間はQPCの経過時間で、CPUの実使用時間ではない。

最後の入力から提示までに含まれる各区間の中央値（ms）。各行を別々に中央値へ取っているため合計は一致しない。

| 区間 | O 変更前 | A 修正版 |
| --- | ---: | ---: |
| 本文描画 | 0.2495 | 0.1460 |
| EndDraw | 0.3460 | 0.3165 |
| 交換鎖の待機 | 0.0010 | 0.0010 |
| Presentを含む提示 | 0.0480 | 0.0595 |
| 最後の入力から提示まで全体 | 1.0275 | 0.9565 |

本文描画は単打・連続入力とも対応する6組全てで短縮した。修正版の連続入力後の全体は0.929〜1.039msで、前回の1.536〜2.456msの跳ねは再現していない。修正版の第6組の単打だけは1.266msで、Presentを含む区間0.312msやstatus描画を含む区間0.404msなど複数箇所が長かったが、これを前回の連続入力と同一原因とは扱わない。

**追加の製品修正を入れる根拠は得られなかった。** 今回の修正部分は短縮したが、前回の揺れの原因は未特定。共有GPU負荷が原因だとも、この診断で通常版の空文書の受入が済んだとも主張しない。PR #293はdraftのまま、共有負荷が落ち着いた時の通常版の空文書確認を残件にする。12本を繰り返さず、短行・長行・表示・IME・splitは再利用する。

全記録は実測JSONのemptyDiagnostic。束は`D:/NeNeNib/evidence/291-empty-diagnostic/empty-diagnostic-evidence.zip`、27ファイル、106822 bytes、SHA-256 `4f148c785fe116bae1f3ca454ff802aebefee76cc16ad50495a79db05a547112`、CRC成功。raw、画像、事前計画、測定器、Releaseメタデータ、検証済みgit bundleと差分を含む。製品コードはa1a0e4dのまま、受入条件・schemaは変更せず、waiverなし。

文書追記後のdocument_checksは違反0（`out/291-empty-diagnostic-doc-conformance.log`）、PR本文のgit-conventions.pyと`git diff --check`は終了0。`git diff --exit-code dcf5811 -- src tests eng CMakeLists.txt`も終了0で、製品枝に計装が混入していない。文書のための製品テストは追加していない。

2026-10-04 02時台、hideの区切りの依頼で日報と引き継ぎ書を整理した。対象はCLAUDEの最新リンク、reports / handoffs / todoと本記録。リンク・規則参照の退行だけをdocument_checksで確認し、違反0（`out/291-handoff-doc-conformance.log`）。PR本文のgit-conventions.py、`git diff --check`、`git diff --exit-code 5e96aa7 -- src tests eng CMakeLists.txt`も終了0。製品・対象テスト・依存は不変で、ビルド・アプリ検証は再利用した。PR #293はdraft、空文書の通常版確認を残して区切る。

### 空文書限定の測定準備（2026-10-04夕方、実測なし）

hideの続行・分担指示により、設計サナが既存の状態・受入条件・コード経路をレビューし、SOLの実装サナがrunnerと監査を準備した。その後hideから「今は操作中。測定の準備まで進めて」と指示があり、窓の起動、前面操作、入力、実測を一切行っていない。Issue #291に固定計画を追記し、PR #293はdraftを維持する。

対象は未判定の通常版の空文書のみ。O=591b99f / A=a1a0e4d、6組12本、奇数O/A・偶数A/O、1280×800 / DPI120、設定ファイルのない新規profileで既定Cascadia Code13.5pt。既存`eng/measure-speed.py.keys_trial`を各試行1回だけ呼ぶ。202入力と暖機/単打の独立、200文字後の提示、rawから再計算した時間、既存50ms以内の到着幅を監査する。寸法/DPIは時刻raw自体には無く、入力前と閉じる直前の`observation.json`で照合する。

受入条件は前計画を維持する。O/A各6本が揃い、各版の前半3本/後半3本の中央値差が単打max(0.1ms,その版の全6中央値の10%)、200文字max(0.5ms,同10%)以内で、A-Oの中央値差もO基準の同許容幅以内のときだけaccepted。欠測・不安定はholdとし、追加試行・上書き・合格までの反復を拒否する。GPU負荷の有無は開始時にhideと確認し、他の作業を停止しない。

新しい道具だけを対象に、`python D:/NeNeNib/scripts/291-empty-acceptance.py --prepare`、`python D:/NeNeNib/scripts/291-empty-acceptance-audit.py --self-check`を実行し、両方終了0。合成rawの正例と、未完了JSON・提示欠落・入力不足・単打の独立性欠如、判定の安定した正例と不安定・許容差超過・欠測・実際に順序を入れ替えた反例を確認した。これは監査器の動作確認であり、製品性能の受入ではない。結果は出力領域の`self-check.json`。設計レビューで設定ファイルを書かない従来条件と、過去の準備版を実行領域に混ぜないことを確認した。

実行領域は`D:/NeNeNib/outputs/291-empty-acceptance-20261004-evening/`で、現在は`manifest.json` / `plan.json` / `self-check.json`のみ。開始記録・試行profile・rawは無い。runner SHA-256は`6b8226fdfbc6a4f0e1082f5abafd5278b9a92b52858dd58339dff983df509e10`、監査は`9f382496c5906ec9a91f19ab8c9ff725e0c2c1d405d85edf44cc04e63998c8ff`、固定planは`b51905ea80ee7086f1dadcafc969fed634edc1fb154e9a0d616033c7ae84e6ef`。自己確認が参照したコードと現行コード、plan内の依存hashが全て一致することを親でも確認した。

準備の証拠は`D:/NeNeNib/evidence/291-empty-acceptance-preparation/preparation-evidence.zip`。16ファイル、41561 bytes、SHA-256 `49bee5c0b1ba378875bd58aab19e9b9bede62aba814fc2a791e7fef83201815c`、ZIPのCRC成功。runner・監査・現行plan/manifest/自己確認・置き換えた未実行の準備記録・事前計画・Releaseメタデータ・共用測定器を収載した。実測を含まない束として区別する。

再利用の根拠: `Get-FileHash -Algorithm SHA256 build/release-a1a0e4d/NeNeNib.exe,build/release-591b99f/NeNeNib.exe`で既存metadataと一致、`git diff --exit-code a1a0e4d HEAD -- src tests eng CMakeLists.txt`も終了0。成功済みのRelease、表示22場面、IME、短行・長行、split比較を再実行しない。製品・schema・性能閾値の変更なし。適用はQLT-001 / QLT-012 / QLT-013 / QLT-014 / CNF-006 / GIT-004、Waivers: none。空文書の性能受入とmerge、splitの採用は未完了のまま。

準備の文書更新では、リンク・規則参照の退行だけを`conformance.document_checks(root, inventory(root), rules)`で確認し、違反0（`out/291-empty-prepare-doc-conformance.log`）。PR本文は`python eng/git-conventions.py D:/NeNeNib/briefs/pr-291-empty-prepared.md --pr-body`が終了0、`git diff --check`も終了0。製品テストは追加していない。追加worktreeは無く、未実施の固定計画を持つ出力と、未実行の準備履歴を持つ`D:/NeNeNib/outputs/291-empty-acceptance-preparation-history-20261004-evening/`を残す。#291の受理・統合後に証拠の収載を確認して整理し、ブランチとcommitは保持する。

### 通常版の空文書12本の結果（2026-10-04 22時台）

hideの「今作業してるエージェントが少ないからテストしてみて」を受け、夕方にIssueへ登録した固定計画を22:01:28〜22:02:50に一度だけ実行した。22:02:52に前面操作の終了を伝えた。稼働エージェントが少ないという申告は記録するが、GPU占有率や他プロセス負荷は測っておらず、無負荷環境とは扱わない。他の作業の停止・追加ビルドは行っていない。

`python -u D:/NeNeNib/scripts/291-empty-acceptance.py --run`は終了0（`out/291-empty-acceptance-run.log`）。通常版O=591b99f / A=a1a0e4dの6組12本は全て有効。`python D:/NeNeNib/scripts/291-empty-acceptance-audit.py`は終了2（`out/291-empty-acceptance-audit.log`）、理由は欠測ではなく事前の安定条件未達。全12本で202入力・暖機/単打の独立・200文字後の提示・時刻rawからの再計算・到着幅・入力前/終了前の1280×800 / DPI120・新規profileの設定ファイル不在を確認した。planの依存hashとReleaseは不変。追加試行は0。

| 指標 | O 変更前の中央値 ms | A 修正版の中央値 ms | A-O ms | O基準の許容幅 ms |
| --- | ---: | ---: | ---: | ---: |
| 1文字 | 1.0515 | 1.0470 | -0.0045 | 0.10515 |
| 200文字 | 3.6825 | 3.4670 | -0.2155 | 0.50000 |

中央値の差は両方とも許容内だが、両版の安定性を必要とする条件を満たさない。

| 対象 | 前半3本中央値 ms | 後半3本中央値 ms | 絶対差 ms | 許容幅 ms | 判定 |
| --- | ---: | ---: | ---: | ---: | --- |
| O 1文字 | 0.999 | 1.081 | 0.082 | 0.10515 | 安定 |
| O 200文字 | 4.202 | 3.638 | 0.564 | 0.50000 | 不安定 |
| A 1文字 | 1.122 | 0.876 | 0.246 | 0.10470 | 不安定 |
| A 200文字 | 3.347 | 3.656 | 0.309 | 0.50000 | 安定 |

したがって通常版の空文書は引き続きhold。修正版の中央値悪化や入力欠落を検出したという結果ではなく、対照も含む揺れにより性能比較の受入を確定できない。揺れの原因は未特定で、GPU・他作業・製品のいずれとも断定しない。現在の条件を緩めず、同じ試行を繰り返さない。

SOLの実装サナが読み取り専用で独立に再計算し、12本の入力・提示・寸法/DPI・profile・hashとhold判定が一致。200文字の途中の提示は全12本0枚。到着幅の前半/後半中央値はO 2.960/2.594ms、A 2.369/2.675ms、最後の入力→提示はO 1.130/1.046ms、A 0.963/0.981msだった。中央値の分解を足して原因の寄与率とは扱わない。報告は`D:/NeNeNib/outputs/291-empty-acceptance-review/review.txt`、SHA-256 `d1af1fc83e99d47a4e3c29c193e92d17820fd58aa16de12db7e9eea74f8e6800`。レビューでGUI・再測定・ビルドは行っていない。

原記録と監査は実測JSONの`emptyAcceptance`へ追加し、既存の全sectionを値比較で不変と確認した。証拠の束は`D:/NeNeNib/evidence/291-empty-acceptance/empty-acceptance-evidence.zip`、65ファイル、67478 bytes、SHA-256 `c5eeb7923c1acd4455d827d160028baf45c3dbb557b43fc6821eb584b268498b`、CRC成功。全12本のraw/observation/profile、plan、runner/監査、共用測定器、Releaseメタデータ、ログ、実行時の申告、独立レビューを含み、exeは含めない。旧90本・36本・計装診断12本・準備の束は置換しない。

製品はa1a0e4dのまま。成功済みのRelease・表示22場面・IME・短行・長行・split比較を再利用し、今回の未達で無関係な検証を追加しない。PR #293はdraft、mergeとsplit採用は保留。schema・基準値・受入条件の変更なし。適用はADR 0069 / QLT-001 / QLT-012 / QLT-013 / QLT-014、Waivers: none。追加worktreeは無く、未受理のrawを持つ実行領域と独立レビュー出力は#291の受理・証拠収載後の整理まで保持する。

記録更新の検証は文書参照とJSONの追加範囲に限定した。document_checksは違反0（`out/291-empty-result-doc-conformance.log`）、`python eng/git-conventions.py D:/NeNeNib/briefs/pr-291-empty-result.md --pr-body`と`git diff --check`は終了0。実測JSONは更新前のHEADと全既存sectionを値比較して一致し、追加はemptyAcceptanceだけ、12本有効とholdをそのまま保持している。

### hideの追加指示による通常版の空文書12本（2026-10-04 22:15）

hideの「もう一回やってみて」を受け、先のholdと原記録を残したまま、同条件の別計画をIssueへ先に記録した。環境を改善したと確認できたための再試行ではない。runnerはOUTPUTとAUDITORの参照先2行だけを変更し、監査器は同一。`D:/NeNeNib/scripts/291-empty-repeat-20261004-2214/291-empty-acceptance.py --prepare`と同所の監査器`--self-check`は終了0。

`python -u D:/NeNeNib/scripts/291-empty-repeat-20261004-2214/291-empty-acceptance.py --run`を22:15:31〜22:16:52に実行し終了0。同所の`291-empty-acceptance-audit.py`は終了2。全12本の入力・提示・寸法/DPI・profile・hashは有効だが、4条件全てで前後半の安定条件を満たさずhold。固定計画内の追加試行は0。22:16:53に前面操作の終了を伝えた。

| 指標 | O 変更前の中央値 ms | A 修正版の中央値 ms | A-O ms | O基準の許容幅 ms |
| --- | ---: | ---: | ---: | ---: |
| 1文字 | 1.1290 | 1.0145 | -0.1145 | 0.11290 |
| 200文字 | 4.4215 | 4.0865 | -0.3350 | 0.50000 |

| 対象 | 前半3本中央値 ms | 後半3本中央値 ms | 絶対差 ms | 許容幅 ms |
| --- | ---: | ---: | ---: | ---: |
| O 1文字 | 1.188 | 1.070 | 0.118 | 0.11290 |
| A 1文字 | 1.084 | 0.859 | 0.225 | 0.10145 |
| O 200文字 | 3.641 | 4.450 | 0.809 | 0.50000 |
| A 200文字 | 3.717 | 4.471 | 0.754 | 0.50000 |

SOLの実装サナの独立再計算でも、全rawの有効性とholdが一致。報告は`D:/NeNeNib/outputs/291-empty-repeat-review-2214/review.txt`、SHA-256 `76d5cf09b2f37b9316b69a89d54317d2f169a188f039b27a20c0c6bd791133c1`。実測JSONの`emptyAcceptanceRepeat`へ別sectionで収載し、前の結果を置き換えていない。

証拠の束は`D:/NeNeNib/evidence/291-empty-repeat-2214/empty-repeat-evidence.zip`、65ファイル、65782 bytes、SHA-256 `3a56d4071794e28f53e12b9aad830ef7fc17868d6a4348cb2828e819c67bd4d4`、CRC成功。raw・監査・plan・測定器・実行時の申告・Releaseメタデータ・独立レビューを保持し、exeは含めない。出力`D:/NeNeNib/outputs/291-empty-repeat-20261004-2214/`と独立レビューは未受理の証拠として保持する。

この比較でも製品退行やGPU起因は確定していない。製品はa1a0e4dのまま、PR #293はdraft。成功済みの関連検証は再利用し、受入条件・基準値・schemaは変更していない。適用はADR 0069 / QLT-001 / QLT-012 / QLT-013 / QLT-014、Waivers: none。

記録更新はdocument_checksが違反0（`D:/NeNeNib/outputs/291-empty-repeat-20261004-2214/doc-check.txt`）、PR本文の`python eng/git-conventions.py D:/NeNeNib/briefs/pr-291-repeat-result.md --pr-body`が終了0。実測JSONの既存sectionを更新前HEADと値比較して全て一致し、追加はemptyAcceptanceRepeatだけ。`git diff --exit-code a1a0e4d HEAD -- src tests eng CMakeLists.txt`も終了0で、記録のための製品テストを追加していない。

### 揺れの所在を調べる内訳診断とOS追跡の制約（2026-10-04 23時台）

hideの原因調査指示と「進めて」を受け、保存済み通常raw24本の分解・コード追跡に続いて、既存計装O=fedf727 / A=2d8a10eの固定6組12本を実施した。事前planと依存hashは`D:/NeNeNib/outputs/291-empty-stage-investigation-20261004/plan.json`、SHA-256 `69e8bf6758fbdc8edc14655f99ae2b6f1ab4e7ba466bb5c71b40cab5aebafc6b`。旧stage解析を変更せず再利用し、`--prepare`と既存raw再解析で一致を確認してから開始した。

`python -B -u D:/NeNeNib/scripts/291-empty-stage-investigation-20261004.py --run --plan-sha256 69e8bf6758fbdc8edc14655f99ae2b6f1ab4e7ba466bb5c71b40cab5aebafc6b`は23:11:52〜23:13:13、終了0（`D:/NeNeNib/outputs/291-empty-investigation-20261004/stage-run.log`）。全12本で202入力・暖機/単打の独立・提示・段階マークの対・1280×800 / DPI120・設定不在を確認。欠測0、追加試行0。23:13:13に前面操作終了を通知した。計装版の経過時間であり、通常版の性能受入へ代用しない。

O第2組の200文字は4.830ms、そのうち最初〜最後の入力受信が3.816ms。199入力間隔の内訳は、input→apply開始の合計0.020ms、apply内1.411ms、apply終了→次input2.385ms。最後の区間はほかのO5本で1.121〜1.148msだった。100µs以上の入力間隔が5か所あり、最大220µsは第53→54文字（apply6µs / apply後214µs）。単一の約1ms停止ではなく、主な増加がapply後へ分散していた。

apply後にはIME方針反映・再描画要求・題名更新判定・告知/終了判定・frameや一時文字列の解放・メッセージループ・OSの実行待ちが含まれる。controller.apply内のframe生成はapply側に計上済み。この入力経路はO/Aで同じで、途中に本文cacheを実行する経路はない。ただしOS待ちと未計装のCPU処理はまだ区別できない。

O第1組の単打1.863msではEndDrawが1.108ms（ほかのOは0.180〜0.277ms）だった。API内部のCPU処理・driver処理・GPU待ち・OSの実行待ちはrawだけでは識別できず、全ての単打の揺れが同じ箇所に出たわけでもない。測定した単打/連続入力の交換鎖Waitは0〜0.003msで、この待機の長さは今回の大きな増加を説明しない。

本文bodyは単打・連続入力とも全6対応組でAが短縮。中央値は単打O/A=0.2445/0.1185ms、連続入力後O/A=0.2600/0.1395ms。**今回のcacheを揺れの原因と判断する証拠は得られなかった。OSコンディションは有力な候補だが未確定**。独立再計算でも全raw/hash/段階/summaryと解釈が一致。報告は`D:/NeNeNib/outputs/291-empty-investigation-20261004/stage-review.txt`、SHA-256 `e01795a77cfdc3da3b5fdb0dd63d1b5cb3e5276f9441673fd84e74eefcc768b7`。

hideのOS条件についての問いを受け、診断専用のWPR設定`D:/NeNeNib/scripts/291-os-waits.wprp`を準備。単一32MiB collectorでCSwitch/ReadyThread/ProcessThread/ThreadPriority/Loader/CpuConfig/Power/DPC/Interruptのみを記録する設定は`wpr -profiledetails`終了0。独自`NeNeNib291Capability`のstartを1回だけ試み、`0x80070005 Access is denied`で拒否された。直後のstatusは独自session不在、ETL未取得。現在の権限ではOS実行状態の裏付けを取得できない。他session操作・権限変更・OS設定変更・再試行は行っていない。詳細は`D:/NeNeNib/outputs/291-os-wait-trace-preparation-20261004/capability-result.json`。これは原因診断のための外部観測であり、ADR 0011の正典の性能測定をETWへ置換するものではない。

実測JSONの`emptyStageInvestigation`へ別sectionで保存。証拠の束は`D:/NeNeNib/evidence/291-empty-stage-investigation-20261004/empty-stage-investigation-evidence.zip`、66ファイル、348776 bytes、SHA-256 `78d37a60b4a53742e4b17ed493110f749ae947a12df885c59c606003ad5e48a9`、CRC成功。raw/plan/trial/profile/測定器/Release metadata/通常rawと入力間隔の分解/独立レビュー/OS追跡の拒否記録と、旧計装・通常比較2回の証拠の束を含む。exeは含めない。

製品はa1a0e4dのまま、追加の修正・ビルドは行っていない。通常版のholdとdraft PR #293を維持。成功済みRelease・表示・IME・短行・長行を再利用し、受入条件・基準値・schemaは不変。適用はADR 0011/0069、QLT-001/012/013/014、ARC-007、Waivers: none。追加worktreeなし。未解決の調査資料と未受理の証拠をD領域に保持する。


### 入力後段の切り分けと重複無効化の改善（2026-10-04 23:27以降）

hideの「30分くらい放置するので自由にテスト・検証・改善」の指示で、設計サナが判断・計測・受理を担当し、既に指名されたSOLの実装サナが隔離した計装/通常版候補と読み取りレビューを担当した。前面操作は23:52:08に終了を伝えた。他プロセスの停止、OS設定や権限の変更は行っていない。

まず通常の入力経路と同じ計装Aへ入力後段6節目を足し、固定12本を23:37:32〜23:38:57に実行した。全202入力の順序、199区間の分解、途中描画/提示なし、1280×800・DPI120、新規profileの設定不在、raw/依存hashを確認し、欠測0。199区間の合計の中央値はapply 1.4745ms、invalidate 0.5785ms、messageLoop 0.7090ms。第12試行はburst 6.373msのうちarrival 5.448ms、tail 0.925msで、invalidate 1.317ms・messageLoop 2.309msが増えた。境界elapsedには隣接処理や計測費用が含まれ、messageLoopをOS待ち時間そのものとは扱わない。WPR権限拒否を迂回していない。

改善は`EditorWindow.cpp/.hpp`の2ファイルに限定した。同じ`invalidate()`入口で、全クライアント領域の`InvalidateRect`が成功したときだけ窓の`paint_pending_`を立て、次の`WM_PAINT`まで重複API要求を省く。`paint()`は常に実行し、`ValidateRect`直後・描画前に解除して再入による次要求を残す。失敗時は次の変更で再要求できる。本文/編集状態の所有やWM_PAINT集約は不変で、ADR 0011の決定6へ実装詳細を追記した。根拠となる契約は[InvalidateRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-invalidaterect)と[ValidateRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-validaterect)。

計装版A=`4d8040a`、同じ計測点で重複を省くB=`790e1bd`を固定6組12本（奇数A/B・偶数B/A）で比較した。欠測0、全試行202入力、途中描画/提示0。invalidate区間合計の中央値は0.6555→0.0190msで、対応6組すべて短縮。burst全体は4.438→3.567ms、短縮4/6組。singleは1.0520→1.0635ms、短縮3/6組。重複処理の軽減は確認できるが、全体が安定して改善したとは言わない。B第2組のmessageLoopは2.217msで、揺れは残った。

通常版候補は`a1fd1e16440352111f37797874d0e8029df9276f`、Release SHA-256 `FAEA73967345B4AD655E26AE68DD45506DAECD9A07A815F28FB8651163955D83`、1307648 bytes。計装点は含まない。O=`591b99f`対A=この候補を新しい固定6組12本で比較し、既存runner/監査の入力・幾何観測・判定式をそのまま再利用した。旧A=`a1a0e4d`との比較ではない。全12本有効だが**性能受入はhold**。

| 指標 | O 中央値 ms | 候補A 中央値 ms | A-O ms | O基準の許容幅 ms |
| --- | ---: | ---: | ---: | ---: |
| 1文字 | 0.9685 | 1.1080 | +0.1395 | 0.1000 |
| 200文字 | 4.4880 | 3.4830 | -1.0050 | 0.5000 |

O単打は前半0.962→後半1.238ms、差0.276>許容0.1ms。A連続入力は前半3.033→後半3.627ms、差0.594>許容0.5ms。さらに単打の中央値差も許容を超える。Oも不安定なので恒常的な製品退行やOS原因は確定せず、有利なburst中央値だけで合格にしない。各固定計画内の追加試行は0で、過去のholdと全rawを保持する。

| 検証コマンドと対象 | 確認する退行・結果 |
| --- | --- |
| 各D worktreeで`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD`、変更ファイルのclang-format、`git diff --check` | enum/入力経路の警告・型・整形。最初の計装`322924c`はmilestone_name 71行でclang-tidy失敗。不要な本文内部6節目を除いた`4d8040a`、`790e1bd`、通常`a1fd1e1`は終了0。抑制・閾値変更なし。各成功版は1回生成 |
| `python -B D:/NeNeNib/scripts/291-post-apply-diagnostic.py --self-check`、`--prepare`、`--run --plan-sha256 c21ad1776b2a5f95a250157f8ca95f80c8489f421c9f0234a8f0b54dee807a0b` | 境界解析の正例、欠落・順序誤り負例と実測12本。終了0。旧本文内部4段階は未計測と明記し0埋めしない |
| `python -B D:/NeNeNib/scripts/291-coalesce-paired.py --prepare --a <4d8040a.json> --b <790e1bd.json>`、`--run --plan-sha256 441fe056f31961beed88a22164346a02708af91fdd3c73dc98beade039f4fb46` | 同じ計測点で重複要求区間の比較。終了0・12本有効、前述の数値。raw/依存/集計の独立再計算も一致 |
| `python -B D:/NeNeNib/scripts/291-coalesce-visual.py --exe <各exe> --name A` / `--name B` / `--compare` | 無効化の抑止や解除漏れ。両版正常終了、12場面の本文/ステータス全画素一致。編集→次paint編集、外部再描画→編集、縮小/拡大、最小化復帰→編集、モード、字体サイズ、テーマを確認。独立した刺激が画に出たことも検査。代表3画像を目視 |
| `python -B D:/NeNeNib/scripts/291-coalesce-normal-acceptance.py --prepare` / `--run` / `--audit` | 計装なしの通常性能。準備/自己確認/実測は終了0。監査は非0（hold、外側PowerShellの終了値1）。全12有効、安定条件と単打数値条件未達。失敗を隠す再実行はしない |
| `python -B D:/NeNeNib/scripts/291-coalesce-ime.py` | 共通invalidateを通るIME変換中/確定の描画漏れ。既存verify_imeを1回呼び、実鍵で変換・Space・Enter、確定後下線0、通常/Vim NORMAL/INSERTの開閉、開始時IME状態への復元、正常終了。終了0 |
| `git diff --exit-code a1fd1e16440352111f37797874d0e8029df9276f -- src tests eng CMakeLists.txt` | 作業枝へ取り込んだソースと検証済み通常候補の同一性。終了0。文書・commit変更だけでビルド/GUIを再実行しない |

旧layout-only版の22場面、短行・長行の数値、split試作は歴史的な成功記録として保持。今回の候補では共通入力経路が変わるため、旧短行・長行の性能値を新候補の実測値とは呼ばない。今回追加した変更には上の12場面とIME、空文書固定比較を対応させた。core/application全件、起動、大容量open、splitを今回の変更のために再試行しない。

証拠は`D:/NeNeNib/evidence/291-coalesce-improvement-20261004/coalesce-improvement-evidence.zip`。211ファイル・3750342 bytes、SHA-256 `615974ded12f7581f7fa08a8c6e1377191f08bbc0c95de7f62a69f703d2838e7`、CRC成功。細分診断/計装比較/通常比較の全rawと計画、24画像、IME画像、測定器、依存源、独立レビュー、失敗を含むビルド記録、3枝のgit bundleを収載。bundleの必要な親は`936b50b`で`git bundle verify`終了0。23:11の証拠束も入れ、以前の原記録を置き換えない。

判断: 重複削減の実装を作業枝へ保存し、PR #293はdraftのまま。空文書の性能受入・merge・split採用は保留。DPI物理遷移、device lost、描画中の実再入、InvalidateRect失敗は実機未再現。OS由来の断定も未了。適用は#291 / ADR 0011・0069 / ARC-001・004・007・011 / CPP-012・017 / QLT-001・012・013・014 / CNF-006 / GIT-004、Waivers: none。

### 保存済み単打の再解析（2026-10-05 00時台）

hideの約10分の調査依頼を受け、既存3実験の単打36本をrawから再解析した。製品commitは`dda40e4`のまま、追加のアプリ起動・計測・ビルド・製品変更は行っていない。対象は23:11の元版O対layout再利用A、23:37の入力後段細分版A、23:44の同じ計装によるA対重複無効化削減B。異なる実験の母集団は混ぜない。

| 確認する誤りと対象 | 実行したコマンド・結果・証拠 |
| --- | --- |
| rawの取り違え、単打/burstの混在、区間の欠落・重複・不正な集計 | `python -B D:/NeNeNib/scripts/291-single-decomposition-20261005.py`、終了0。source/raw hash、各202入力、単打がburst前に提示済み、各節目1回・順序、11隣接区間の和と元singleMsの一致を全36本で確認。handler補助分解は11区間へ重ねて加算しない |
| 解析実装に依存した見落とし | 読み取り専用の独立レビューで36 rawから区間、合計、variant中央値、各対応差を再計算し一致。COM参照の保持と解放位置もソース照合。`D:/NeNeNib/outputs/291-single-decomposition-20261005/review.txt` |
| 証拠束への原記録の取り違えと破損 | `python -B D:/NeNeNib/scripts/291-single-decomposition-archive-20261005.py`、終了0。収載元hash照合とZIP CRC成功。再測定ではない |

集計は`D:/NeNeNib/outputs/291-single-decomposition-20261005/decomposition.json`、解釈は同じ場所の`findings.txt`。解析スクリプトSHA-256は`45cce9ba2a9fde0966bb28281813c106354d8947c5eda7cddde49410890dacdd`。

| 同一実験内の単打区間 | 元版/候補の中央値 ms | 確認できた範囲 |
| --- | ---: | --- |
| 23:11 O→layout再利用Aの本文body | 0.2445→0.1185 | body単独の短縮を全描画の短縮とはしない |
| 同じO→Aのbody終了→EndDraw開始 | 0.2430→0.3030 | 対応A-Oは+0.069/+0.064/+0.014/+0.054/+0.064/+0.124ms、全6組で増加 |
| 同じO→Aの各試行のbodyと後段の合計 | 0.4875→0.4225 | 対応差は-0.042/-0.032/-0.178/-0.073/-0.062/-0.021ms、全6組で短縮。中央値同士を足した値ではない |
| 23:44 A→重複無効化削減Bの入力受信→handler戻り | 0.0370→0.0360 | この計装実験では中央値の増加なし |
| 同じA→Bのapply後handler / handler戻り→paint | 0.0160→0.0155 / 0.0260→0.0250 | flag追加が通常版の悪化と無関係だと証明したものではない |

23:44のB第4組は単打全体でAより+0.497ms。増分は本文後+0.207ms、本文前+0.076ms、本文+0.063ms、提示/戻り+0.063ms、EndDraw+0.058msなどに分布し、入力handlerの増分は+0.031ms。単一の区間だけへ原因を帰属しない。

ソース上、元版は`draw_plain_line` / `draw_composed_line`のローカルCOM参照を関数終了時にReleaseする。再利用版はlayoutを保持し、前フレームの未再利用layoutをステータス/palette描画後の`previous_body_layouts_.clear()`で破棄する。この寿命と位置の変化により費用がbodyから後段へ移る可能性がある。ただし最終参照解放、Direct2D内部の参照保持、実際の破棄費用は未観測。後段にはステータス/palette・clear・実行待ちが含まれるため「解放時間」とは呼ばない。bodyと後段の合計も短縮しており、改善が全部見かけの移動だったとも断定しない。

通常版O=`591b99f`対候補`a1fd1e1`で残る単打+0.1395msへ、別の計装実験の内訳を遡及して割り当てない。通常版の数値許容と安定条件の未達、OS原因未確定、WPR権限拒否は未解消。性能受入・merge・split採用はholdのまま。

次回案は同じ出力先の`next-diagnostic-plan.json`。**未実装・未ビルド・未実行**で、`draw_status_bar`直後と`previous_body_layouts_.clear()`直後の2節目を追加し、既存のbody/EndDraw境界と組み合わせる。D側の隔離worktreeで既存TimingPort/Win32 adapterと入力driverを使い、破棄順序は変えない。現在候補1版・固定12本・202入力・1280×800/DPI120・新規既定profileの区間診断を想定。新しい前面操作時間の調整と実行前のhash/出力先固定が必要で、通常版受入条件の代用にはしない。

恒久証拠は`D:/NeNeNib/evidence/291-single-decomposition-20261005/single-decomposition-evidence.zip`。45ファイル、497764 bytes、SHA-256 `ad6eb10b3d759d424439a73e1418383996a4a96aea12e7f6dbe196df0f02c8bd`。解析・所見・次回案・独立レビュー、3実験の元集計と36 raw、解析/梱包スクリプトを収載。CRC成功は同じ場所の`archive.json`に記録済み。既存の原記録・証拠を置き換えていない。

今回の日報・引き継ぎ保存は文書と参照先だけを変更し、関連する製品・テスト・依存は不変。`git diff --check`は終了0、新しい2文書のローカルリンクと見出し参照6件、および日報のIssue要約6行を`python -B -`の限定照合で確認した。先行するRelease/静的検査/表示/IMEの成功結果と性能holdをそのまま再利用し、アプリの再起動や全件検証は行わない。適用は#291 / ADR 0011・0069 / QLT-001・012・013・014 / GIT-003・004。製品schemaと受入幅の変更なし、Waivers: none。

## 5-cl — ステータスの固定枠と単打分布（Issue #291・ADR 0070）

2026-10-05夜、hideから対象を局所的に閉じて速度と安定性を改善する指示と、食事中に改善・計測を進める許可を受け、現在のサナが単体で実装・検証した。前面操作は21:34 JSTに終了。通常版は`a1fd1e1`を対照O、ステータス固定枠を加えた`0dfad49d9f79e2efd7f165a35e9221f999c4a441`を候補Aと呼ぶ。Oにも本文layout再利用と重複無効化削減が入っており、元の`591b99f`との比較ではない。

### 対象と実装

本文後の区間を分けた計装版`5476c66`で、ステータス描画は単打中央値0.282ms・最大0.321ms、前フレームの本文layout解放は0.001ms・最大0.002msだった。対象をステータスに絞り、`Direct2DRenderer.cpp/.hpp`と新しい`StatusTextLayout.hpp`で最大7枠を所有する。`draw_status_bar`で枠位置を戻し、文字列全文・書式・幅・高さが同じ枠のUTF変換と文字組み生成を省く。色と原点は毎回使う。UI書式再作成前に全枠を破棄し、作成失敗では古い表示を出さない。コマンド入力の専用layout、本文cache、解放位置、入力経路は変更しない。

通常候補Releaseは1311744 bytes、SHA-256 `A68628E8E48D9D83063B26ECA1F8FE00E42797A74DB7C7369389EAE6A51918AE`。既存の作業枝`fix/291-long-line-rendering`を検証済みcommitへfast-forwardした。計装は診断枝だけに保持する。main・remote PRへの反映は行っていない。

### 表示・構造の検証

| 実行したコマンドと対象 | 確認する退行・結果 |
| --- | --- |
| 各D worktreeで`pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` | 型・所有・警告・clang-tidy・リンク。診断前`5476c66`、通常`0dfad49`、診断後`9c617c0`の3版が終了0。通常版total 130.202s、診断後129.643s。記録は証拠束の`logs/`と`releases/` |
| `clang-format --dry-run --Werror src/ui/win32/Direct2DRenderer.hpp src/ui/win32/Direct2DRenderer.cpp src/ui/win32/StatusTextLayout.hpp`、`git diff --check` | 変更したC++の整形。終了0 |
| `python -B -`で既存conformanceの`waiver_checks`・`document_checks`と変更した3ソースの`source_checks`を呼ぶ限定検査 | 新しい型・状態とADRが規約に従うこと。0 findings、終了0。`291-status-layout-conformance-20261005.log` |
| `python -B D:/NeNeNib/scripts/291-status-layout-visual-20261005.py --exe <a1fd1e1のexe> --name A`、`--exe <0dfad49のexe> --name B`、`--compare` | キャッシュの誤一致、変更されたラベルや色の描画漏れ、枠上限。22場面ずつ正常終了、本文/ステータスの差分0画素。ここだけA/Bは旧版/新版。7枠を使うNORMAL/INSERT録画、停止、通知、Ex入力、通常/Vim、桁上がり、改行、幅変更、テーマ、フォント、paletteを含む。重要な状態間で画素が変化したことも確認。代表画像4枚を目視 |
| `python -B eng/protected-diff.py --base e40742a --head 0dfad49` | 保護対象を変更していないこと。終了0、fixture 1853→1853、metadata/deleted/changed/added=0、protected files changed none。scopeは33・件数未測33。製品全件テストを追加せず、件数不変とは主張しない |

自己レビューでは、`write_status`がステータスからだけ呼ばれること、通常6枠・録画最大7枠・通知4枠・Ex入力時3枠で上限を越えないこと、書式変更前の破棄、layoutを外部へ貸さず変更しないこと、色と原点をキャッシュ条件に入れないことを確認した。公開API、保存形式、基準値、waiverは変更していない。

### 計装なしの固定6組12試行

`python -B D:/NeNeNib/scripts/291-status-layout-normal-20261005.py --prepare` / `--run` / `--audit`。準備・解析器の正例/負例・実測は終了0、監査は非0（Pythonの判定hold、外側PowerShellの終了値1）。既存`keys_trial`と監査を再利用し、奇数O/A・偶数A/O、新規profile、1280×800 / DPI120 / 既定Cascadia Code 13.5ptで固定。全12本で202入力、暖機/単打の独立、提示、raw再計算、入力前/終了前の幾何、設定不在、hashを確認。欠測0・追加試行0。

| 指標 | O 中央値 / 最大 ms | A 中央値 / 最大 ms | 対応組でAが短縮 |
| --- | ---: | ---: | ---: |
| 単打 | 1.084 / 1.107 | 0.732 / 0.758 | 6/6 |
| 200文字 | 3.237 / 3.773 | 3.169 / 3.577 | 4/6 |

単打中央値は32.5%短縮。O単打の前後半差0.027ms、A単打0.008ms、A burst 0.441msは既存の安定幅内だった。一方O burstは前半3.552→後半2.922ms、差0.630msが許容0.500msを越えた。**既存総合判定はhold**であり、単打改善を総合合格と言い換えない。通常入力の恒常的な最悪時間やOS原因はこの実験では確定しない。

### 各版240回の独立単打

通常の固定試行とは別に、1起動あたり暖機1文字+40文字、名目100ms間隔、6組12起動の計画を実行前に固定した。`python -B D:/NeNeNib/scripts/291-status-layout-tail-20261005.py --prepare` / `--run` / `--analyze`は全て終了0。解析器は正例と5負例で入力不足/余分・提示欠落・非単調時刻・次入力との混在を拒否。各rawはちょうど41入力、暖機も含め全入力が次入力前に提示済みで、上限4096marks未満。全480単打が有効、除外・再試行なし。

| 分布（nearest-rank） | O ms | A ms | 短縮率 |
| --- | ---: | ---: | ---: |
| 中央値 | 1.0250 | 0.6415 | 37.4% |
| p95 | 1.206 | 0.796 | 34.0% |
| p99 | 1.662 | 1.123 | 32.4% |
| 観測最大値 | 1.766 | 1.243 | 29.6% |

起動ごとの中央値と最大値も全6対応組でAが短縮。2ms・5ms・16.7ms超は両版とも0/240であり、遅延件数が減った証拠とはしない。閾値は分布を説明するための数値で、受入幅を置き換えない。文書は空から41文字へ増え、1起動内の値には相関がある。C++のQPCで`input_received`から`frame_presented`までを測り、WM_CHAR受信前の待ちや実画素の発光までの時間は含まない。最大値はこの240回の観測値で、絶対的な最悪時間の保証ではない。

### 同じ計測点による削減箇所の確認

`python -B D:/NeNeNib/scripts/291-render-capsule-diagnostic-20261005.py --output <before/afterの出力先> --prepare <Release metadata>` / `--run` / `--analyze`。元候補に計装を加えた`5476c66`と、新候補に同じ計装を加えた`9c617c0`で各12本、全て有効、終了0。9節目の順序と一意性、隣接8区間の和が単打全体と一致すること、202入力と提示、幾何、既定profileを確認した。解析器の4負例も拒否した。

ステータス区間の中央値は0.282→0.077ms、最大は0.321→0.269ms。本文layout解放は0.001→0.002ms、最大0.002→0.014ms。候補の第6試行は単打1.229msで、status 0.269ms・EndDraw 0.437msなどの増加を含み、そのまま保存した。前後は別時刻の診断群で、本文・EndDraw・Presentの分布も変わっている。通常版の短縮全量を特定区間へ割り当てたり、計装版を通常版の性能受入へ代用したりしない。

### 証拠・再利用・残る範囲

集計と原記録の参照/hashは[`status-layout-capsule-2026-10-05.json`](status-layout-capsule-2026-10-05.json)。原記録は`D:/NeNeNib/outputs/291-status-layout-normal-20261005`、`291-status-layout-tail-20261005`、`291-status-layout-visual-20261005`、`291-render-capsule-diagnosis-before-20261005`、`291-render-capsule-diagnosis-after-20261005`。事前plan、全rawとtrial、依存源、44画像、測定器、4版のRelease exe/metadata、ビルド記録、4枝のgit bundleを`D:/NeNeNib/evidence/291-status-layout-capsule-20261005/status-layout-capsule-evidence.zip`へ収載した。315原ファイル+manifest、3271724 bytes、SHA-256 `ec9e7369ac0f303bf42d41a9f5a1c6f0a9650a96f50c7b128285a6b746a0228b`。`python -B D:/NeNeNib/scripts/291-status-layout-archive-20261005.py`は終了0、全entry hash・ZIP CRC・bundle verify成功（親`936b50b`が必要）。以前の失敗や証拠束を置換していない。

今回の変更はWin32ステータス描画に閉じるため、core/application全件・無関係な起動/大容量open/IME/splitは再実行しない。文書・集計・commitだけが変わる工程では、検証済み`0dfad49`の実装・試験・依存の成功を再利用する。今回の候補の長行/16MiB性能は未測であり、過去候補の値を今回の実測値にしない。物理DPI遷移、device lost、DirectWrite作成失敗の実再現は未実施。最大7枠の保持メモリと、変更された文字列を組む費用は残る。

性能の総合受入とmain統合は保留。今回の3追加worktree（`291-render-capsule-profile-20261005`、`291-status-layout-capsule-20261005`、`291-status-layout-profile-20261005`）はD側にあり、未統合の診断/実装と測定済みReleaseの参照先なので保持する。完了後は保存済み証拠と稼働参照を確認して整理する。適用は#291 / FR-015 / ADR 0011・0069・0070 / ARC-001・004・005・008・011 / CPP-003・012・016・017 / QLT-001・012・013・014 / GIT-003・004、Waivers: none。

記録の保存では`python -B -`で文書/waiver検査0 findings、追加・変更したローカルリンク9件、原記録5集計の内容/hash一致とZIP hash一致を確認した。`git diff --check`と`git diff --exit-code 0dfad49 -- src tests eng CMakeLists.txt`も終了0。ログは`D:/NeNeNib/outputs/291-status-layout-document-check-20261005.log`。検証後の製品変更はなく、文書commitのためのアプリ再実行はしていない。


## 5-cm — 字体選択・可視字形・表示幅とsplit再評価（Issue #291・ADR 0071・0073・0074）

2026-10-05夜、hideの「思いつく対応を全てやってみて」を受け、長行の残費用を診断して複数案を隔離実装した。
通常候補は大幅に短縮したが、**長行単打の前後半安定条件とsplitの総時間2ms条件が未達なので、性能の総合受理・main統合・split採用は保留**。閾値・性能基準値・過去の失敗記録は変更していない。
集計と各版の全6値、判定理由、実行前plan、exeの生成元/hashは[`render-capsule-2026-10-05.json`](render-capsule-2026-10-05.json)。

### 実装と試した案

正典は既存の `display_width` → `DisplayLine` → `layout_of` → DirectWriteの経路。
`FontFallbackCache/Key/Entry/Request` は完結した最大256 UTF-16単位のOS字体選択を128件まで保持し、範囲外・未知のsource・OS失敗は同じOSへ委譲する。
`BodyGlyphCollector/Run` はDirectWriteの所有済み字形・位置を保持し、保守的な上界で画面外と証明できるrunの描画だけを省く。未知の装飾は収集全体を棄却して既存DrawTextLayoutを使う。0字形のTabは描画APIを呼ばない。
`VirtualColumn.cpp` は唯一の `DisplayWidthRange.hpp` から65536 byteのBMP索引をコンパイル時生成し、BMP外は従来の検索を使う。文字幅の意味の表を増やしていない。
`Direct2DRenderer` / `BodyTextLayout` とCMakeの所有先へ接続し、`DisplayLineTests.cpp` に全コードポイントの正本照合を追加した。公開のsplit機能・保存schema・別の状態所有者は追加していない。

| 案 | 観測と判断 |
| --- | --- |
| OS fallbackの明示設定、font axis/optical size、locale指定 | 独立DirectWrite診断では約16〜19msの文字組みが残り、採用しない |
| 観測した字体を全区間へ先に指定・runをまとめる | 約0.56msへ短縮するが、遠端の位置/HitTestに最大0.16015625px差。文字位置を変えるため不採用。collectionを省く全量MapCharactersはASCIIの字体も変わり、同じcollectionを渡す版は約142ms。いずれも不採用 |
| 完結したOS字体選択要求の保持 | 診断で字形・位置・全UTF-16位置のHitTestが一致。通常Releaseの長行単打28.5385→16.5005ms（各6本）。採用する通常候補に含める |
| 透明な行bitmap | Consolasの3場面で2〜18画素にRGB各1階調差。不採用 |
| 背景と文字を合成済みの不透明bitmap | 22場面一致、通常単打28.461→7.9965ms。ただし200文字の1本は292.590ms（入力到着範囲285.567ms）で保持。選択/検索で通常描画へ戻る条件とbitmap資源を増やすため、最終候補は字形保持へ一本化。実験枝のADR 0072と全記録を保存 |
| 可視字形の保持 | Tabの0字形でE_INVALIDARGになった初版を修正し、22場面一致。通常単打28.5585→7.6505ms。候補単打の前後半差1.677msは許容0.76505ms外で、成功に読み替えない |
| 正本の幅表からBMP索引を生成 | 全1114112コードポイント一致。volatile入力の診断で同じ結果を45.42〜46.08→7.46〜7.56msで求めた。最初の定数ループはコンパイラが外へ移せるため速度根拠から除外し、記録を残した。最終通常候補に含める |

各行の通常比較は別時刻の固定6組で、bitmapと字形保持の数値差を直接の優劣の証明にしない。安全な全文字組みの範囲を壊す先頭切り捨て、欠字になるfallback無効化は採用しない。

### 変更範囲の検証

| コマンド・対象 | 回帰の理由と実結果 |
| --- | --- |
| 各候補で `pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` | SDKのCOM境界、CMake追加、実際の最適化構成を確認。字体保持の初回54403dfは固定11引数のtidy違反で失敗し、4行の境界へ閉じWVR-0001適用後の382800fは成功。字形9900507、幅c637acc、最終311e78b、整形後f42a95d、S/H/VのReleaseは成功。全ビルドログを保存 |
| 実ソース `FontFallbackCache.cpp` / `FontFallbackKey.cpp` を `clang-cl /std:c++latest /Od /MT /W4 /WX -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all` でContractsStrict.cppと実行 | 誤一致・借用寿命・失敗の記憶・上限越えを防ぐ。604 checks成功。全文/属性/COM identity、範囲/名前上限、prefix/tail、QIのE_FAIL、OSのE_FAIL/S_FALSE、null font/scale、128件FIFOを確認。独立診断exeであり製品Releaseではない |
| 実ソースBodyGlyphCollectorを同sanitizerでGlyphPixels.cppと実行 | 収集後の参照寿命、clipによる欠け、代替描画。3字体×3サイズ×4幅×5文字列の180画素対、1131保持runが全て0差。layout/format/collectorを破棄後に所有字形を描いた。3未対応コールバックの拒否も成功 |
| `cmake -S . -B build/debug-width -G Ninja -DCMAKE_BUILD_TYPE=Debug` / `cmake --build build/debug-width --target nib_tests`、`build/debug-width/nib_tests.exe --display-line` / `--vim-virtual-column` | 幅の索引が表示文字列と仮想桁の意味を変えないこと。Debug ASan/UBSan、display-line 1114301 checks、vim-virtual-column 424 checks成功。他scopeは実行しない |
| `291-font-fallback-visual-20261005.py` と `291-visible-glyph-fixed-visual-20261005.py` の前後比較 | 編集・クリック・選択・検索・scroll・幅・3書式・テーマ・多言語/結合/Tab/双方向の表示を確認。各22場面で本文/ステータス0画素差。比較元22画像は同じ0dfad49の成功結果を再利用。初版71a2577の起動失敗も記録し、Tab修正後だけ再実行 |
| `python -B D:/NeNeNib/scripts/291-render-final-ime-20261005.py` | composed行の新描画経路とIME状態復元。311e78bで実キーによる日本語入力・Space変換・Enter確定、対象文節、ink/下線522/189→確定651/0、通常/Vim切替とIME開閉復元が成功。verify_imeだけを呼び、関係ないGUI試験は実行しない |
| 変更ソースの `git-clang-format`、既存conformanceのsource/document/waiver/architecture限定呼出し | 整形と新規型/翻訳単位の所有、規則の遵守。整形後0差、0 findings。誤ったPython module登録で検査補助が1回失敗し、sys.modulesへの登録を直したconformance2で成功。製品不具合と混同しない |

独立sanitizer診断2種は起動時に `interception_win: unhandled instruction` の警告を出した。終了0で検査は成功しsanitizer違反の報告はなかったが、全OS呼出しの完全な計装を主張しない。ログをそのまま保存した。

通常実測は **311e78b**、SHA-256 `5F2CFF9C74C37FC4BBE2D43E7481AC1CABBBDA7ADA8744B5E30EE37504AECB66`、1399296 bytes。
製品ソースの保存版 **f42a95d** はその後の空白整形とown-headerのinclude順だけで、対象差分を機械照合した。Releaseを再ビルドして成功、SHA-256 `44B017F33241E320BC5F182CEEB1CAFC6899255F898C0AC943703BC04F023900`、同bytes。動作・試験・依存が変わらないので、画素/IME/性能を再実行しない（QLT-001 / QLT-012）。

### 最終通常版とsplitの固定90試行

`python -B D:/NeNeNib/scripts/291-capsule-combined-20261005.py --output D:/NeNeNib/outputs/291-capsule-combined-20261005 --manifest D:/NeNeNib/briefs/291-capsule-combined-manifest-20261005.json --prepare` / 同outputの `--run` は終了0。
3本文×6組×O/A/S/H/V、奇数O→A→S→H→V・偶数逆順。既存 `keys_trial` / driver、1280×800 / DPI120 / Cascadia Code 13.5pt、新規profileを実行前固定。
Oは0dfad49、Aは311e78b、Sはdad9ab0、Hはc1ef96c、Vは9bb7d68。全90本が有効、各202入力、時刻単調・暖機と単打の独立・最終提示・記録上限4096未満・幾何・hashを確認。欠測/除外/再試行0。解析器は正例と5負例を確認。
空文書burstの到着範囲50ms条件を維持。長文書の到着範囲は記録し、空文書の条件を転用しない。

| 本文・指標 | O 中央値 / 最大 ms | A 中央値 / 最大 ms |
| --- | ---: | ---: |
| 空文書・単打 | 0.7395 / 1.285 | 0.7355 / 0.772 |
| 空文書・200文字 | 3.2275 / 3.879 | 3.1135 / 3.339 |
| 16 MiB短行・単打 | 1.2650 / 1.298 | 1.2430 / 1.359 |
| 16 MiB短行・200文字 | 8.6080 / 10.179 | 7.1165 / 7.289 |
| 長い日本語行・単打 | 28.5340 / 32.491 | 7.2695 / 8.107 |
| 長い日本語行・200文字 | 175.1165 / 181.212 | 91.5315 / 92.739 |

長行単打の中央値は74.5%、200文字は47.7%短縮し、両指標とも対応6組全て短縮した。空文書/短行の通常比較は従来条件内。
長行A単打は前半/後半中央値差0.800msが許容0.72695msを超える。速度改善は確認できるが、**総合holdを維持**する。OS等への原因帰属はしていない。

| 本文 | A 通常 | S 1枠試作 | H 上下 | V 左右 |
| --- | ---: | ---: | ---: | ---: |
| 空文書・単打 ms | 0.7355 | 0.7605 | 0.7440 | 0.7435 |
| 16 MiB短行・単打 ms | 1.2430 | 1.2270 | 1.2300 | 1.6610 |
| 長い日本語行・単打 ms | 7.2695 | 7.1070 | 6.8035 | 6.4095 |
| 空文書・200文字 ms | 3.1135 | 3.1715 | 3.0705 | 3.2455 |
| 16 MiB短行・200文字 ms | 7.1165 | 7.2495 | 7.2005 | 7.4520 |
| 長い日本語行・200文字 ms | 91.5315 | 90.3875 | 89.6230 | 91.3815 |

空文書と16 MiBはSの許容/一貫悪化条件、H/Vの増分と2ms条件内。長行はA安定条件に加えH/V総時間が2ms外なのでsplit不採用。V単打自身にも前後半差0.910ms > 許容0.64095msがある。
各本文のO/A/S初期画面は本文/ステータス0画素差、H/Vの配置も保存画像で確認。Hは合計表示行数が19→18となり、Vは同じ行を同じ幅で共有するため、Aより小さな観測値を「二つの独立文書でも軽い」と解釈しない。独立文書・scroll・2つ目のframe生成・所有移行は未測。

### 長行単打の分布

`python -B D:/NeNeNib/scripts/291-render-final-tail-20261005.py --prepare` / `--run` / `--analyze` は終了0。固定6組、各起動暖機1+40文字、名目100ms間隔で各版240件。長行fixtureへ追記し、通常の202入力試行とは別の実験。解析器5負例と、全41入力が次入力前に提示されたことを確認。全480件有効、除外・再試行なし。
runnerの継承によるtrial名の `empty-*` は識別ラベルだけで、実際の起動引数・fixture hash・planのstimulusは長い日本語文書で固定されている。

| 指標 | O ms | A ms |
| --- | ---: | ---: |
| 中央値 | 32.4595 | 6.8750 |
| p95（nearest-rank） | 36.362 | 7.513 |
| p99 | 37.198 | 7.652 |
| 観測最大 | 37.644 | 7.978 |
| 16.7ms超 | 240/240 | 0/240 |

2ms/5ms超は両版240/240。しきい値は説明用で、受理条件を変更しない。入力受信からPresentの戻りまでをC++のQPCで測り、Pythonは刺激と集計だけ。OSの入力queue滞留・物理的な発光時間を含まず、240件と起動内の相関から恒常的な最悪時間を保証しない。

### 保存と残る範囲

`python -B D:/NeNeNib/scripts/291-render-capsule-archive-20261005.py` は終了0。
`D:/NeNeNib/evidence/291-render-capsule-20261005/render-capsule-evidence.zip` は1023原ファイル+manifest、46224978 bytes、SHA-256 `4ea93d57c3a7ddf6d9a8ca01bd5f7d71a033f05607a18bef7fb88648c07c1bad`。
全entryのSHA、ZIP CRC、git bundle verify成功（親936b50bが必要）。失敗した起動・bitmap画素差・遅い試行・定数ループ診断・全候補のexe/metadata・scripts・fixtures・raw・profiles・画像・9枝を含む。後続の集計と文書検査は同evidenceの補足へ保存する。
`291-render-capsule-record-20261005.py` で保存90本のschedule、raw hash、全指標を再計算し一致した。測定の追加はしていない。

初回/編集行の全文字組み、長行ごとのDisplayLine再生成と入力ごとのframe生成は残る。字体の区切りを変えずに全文字組みを部分化する設計、frameの重複生成を省く設計は追加の所有/IME契約を要し、今回完了とはしない。物理DPI遷移・device lost・確保失敗の実再現も未了。
新規8 worktreeと従来の比較元は未受理・main未統合で、測定済みexeへの参照もあるためD側に保持する。具体的なパスは引き継ぎと証拠のworktree-inventory.jsonに記載。技術受理・統合・必要反映が完了したら安全確認後に整理する。
適用: #291 / ADR 0068・0071・0073・0074 / ARC-001・007・011 / CPP-012・016・017 / QLT-001・004・012・013・014 / GIT-003・004。
保存schema変更なし。Waivers: **WVR-0001 / WVR-0002**（SDK固定COM署名の引数数のみ、2026-11-04期限）。ゲート閾値・抑制範囲を広げていない。

追加の `python -B eng/protected-diff.py --base d620e1a --head f42a95d` は終了0。fixture 1853→1853、metadata/deleted/changed/added=0、保護ファイル変更なし。scope33の比較件数は未測で、件数不変とは主張しない。文書保存では `291-render-capsule-document-check-20261005.py` が終了0、document/waiver 0 findings、追加ローカル参照15件、plan/raw/tail/ZIP hash一致、製品差分なしとwhitespaceを確認した。全件テストは実行していない。

## 5-cn — 表示幅索引のmain構成への分離（Issue #294）

2026-10-05夜のhideの「可能なものはマージして」を受け、#291の`c637acc`から幅索引だけをmain `936b50b`上へ分離した。ソース`36a1c9c`、変更は`VirtualColumn.cpp`・`DisplayLineTests.cpp`・ADR 0074。本文cache・字体・字形・splitは含まない。

**判定: HOLD。main未統合。** 固定54回のうち52回が有効。`empty-1-O`は212入力、`16mib-2-O`は206入力で予定202入力に一致せず無効。最初の空文書の撮影にも予定外の本文がある。入力の発生元は特定していない。この2回を正常値へ補完せず、全原記録を保存した。

| 対象 | main O → 幅索引 A・中央値 ms | 判定 |
| --- | --- | --- |
| 空文書 単打 / 200文字 | 1.061 / 4.297 → 1.1305 / 4.414 | Oは5回の記述値。A単打の前後半差0.276 > 0.11305、burst差0.897 > 0.5。保留 |
| 16MiB短行 単打 / 200文字 | 2.244 / 11.057 → 1.853 / 7.6915 | Oは5回の記述値。比較不完備のため保留 |
| 日本語長行 単打 / 200文字 | 317.240 / 466.047 → 314.946 / 399.7125 | 両版6回有効、安定条件と中央値増分は事前条件内 |

範囲と退行の根拠: 幅の意味と直接の利用先（表示行・Vim仮想桁）、main描画と組み合わせた入力費用。全件テストは選んでいない。入力受領からPresent復帰までであり、光の遅延ではない。

- `git diff --exit-code c637acc 36a1c9c -- src/core src/application tests eng`成功。CMakeの差は未採用UI翻訳単位の登録だけで、対象のcompile設定/依存に変更なし。既存`build/debug-width/nib_tests.exe --display-line` 1114301 checks、`--vim-virtual-column` 424 checksを再利用（元の実行ログもarchiveに保存）。コマンドの元のbuild所在は#291のarchiveとRelease記録を参照。
- `pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD`成功、162.307秒、1368064 byte、SHA-256 `18D1D1013A0E94060EF753F84B662536428D33D75BA2C88D30F2208095BE5190`。`out/release/36a1c9c.json`。
- main `936b50b`と既存Release `591b99f`の`src tests eng CMakeLists.txt`一致を`git diff --exit-code`で確認して基準exeを再利用。
- `python -B D:/NeNeNib/scripts/294-295-main-comparison-20261005.py --output D:/NeNeNib/outputs/294-295-main-comparison-20261005 --run`は54回完了後に無効2回のassertで終了1。予定・入力・版・依存hashを先に固定。再試行なし。`294-295-main-audit-20261005.py`成功、全54原記録を再集計しHOLDを確認。空文書O/Aは929画素差（予定外入力）、16MiB/長行のO/Aは本文・ステータス0画素差。
- `294-295-isolated-static-20261005.py`成功、変更2ソース・所有/依存・文書/waiverで0 findings。LLVM 19.1.5の`git-clang-format --diff 936b50b 36a1c9c -- src/core/VirtualColumn.cpp tests/unit/DisplayLineTests.cpp`は差分なし。`git diff --check`成功。
- `python -B eng/protected-diff.py --base 936b50b --head 36a1c9c`成功。fixture 1853→1853、metadata/deleted/changed/added各0、protected files none。scope33は未測33（全件checks同一とは主張しない）。`out/protected/36a1c9c.json`。

規則: ARC-001 / CPP-016 / QLT-001 / QLT-004 / QLT-012 / QLT-013 / QLT-014 / GIT-003 / GIT-004。保存schema・基準値・ゲートは変更なし。Waivers: none。元の`fix/291-long-line-rendering`とPR #293は未統合のまま。

機械記録: [main-isolation-2026-10-05.json](main-isolation-2026-10-05.json)。全証拠は`D:/NeNeNib/evidence/294-295-main-isolation-20261005/main-isolation-evidence.zip`（306 files + manifest、SHA-256 `e4afca114af9083a75a61ca744c72ac7829a30fa70c65c3be84b3c4125975717`）。全entry hash / ZIP CRC / bundle検証成功。候補ソース・3版の実測exe・全raw・22場面比較・失敗記録を保存。

## 5-co — ステータス固定枠のmain構成への分離（Issue #295）

幅索引のみの`36a1c9c`へ、#291の`0dfad49`からステータス保持だけを分離した`1883e27`。変更は`Direct2DRenderer.cpp/.hpp`・`StatusTextLayout.hpp`・ADR 0070。本文はmainの既存経路で、本文cache・重複無効化削減・字体cache・字形保持・splitは含まない。

**判定: HOLD。main未統合。** #294に依存するDraft候補。固定比較のA/Sは各18回すべて有効だが、空文書単打の前後半差はA 0.276 > 0.11305ms、S 0.112 > 0.1ms。A burstも0.897 > 0.5ms。中央値改善だけでは安定条件を代替しない。#294自体にも基準側欠測がある。原記録と予定は5-cnの同一archiveへ保存し、再試行・基準更新なし。

| 対象 | 幅索引 A → ステータス追加 S・中央値 ms |
| --- | --- |
| 空文書 単打 / 200文字 | 1.1305 / 4.414 → 0.622 / 4.045 |
| 16MiB短行 単打 / 200文字 | 1.853 / 7.6915 → 1.5185 / 7.932 |
| 日本語長行 単打 / 200文字 | 314.946 / 399.7125 → 314.8175 / 396.613 |

範囲と退行の根拠: 7枠の上限・文字列/書式/寸法照合、表示の更新、main本文経路との描画費用。無関係なcore/IME/split全件は実行していない。

- `pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD`成功、143.992秒、1372160 byte、SHA-256 `B4B8FBFFAB0ABE41E39508E2EE9E37EBAFD9BFF37B2BF37135C8438080C7779D`。`out/release/1883e27.json`。
- `python -B D:/NeNeNib/scripts/295-status-isolated-visual-20261005.py --exe <A/Sの実測exe> --name <A/B>`を各1回、`--compare`成功。通常/Vim・桁上がり・7枠録画・通知・Ex入力・幅・テーマ・フォント・palette切替など22場面/44撮影で、本文とステータスは全場面0画素差。刺激で変化する場面が変わることも確認。原点/色/文字更新の直接境界を確認した。
- `294-295-isolated-static-20261005.py`成功、変更3ソース・所有/依存・文書/waiverで0 findings。LLVM 19.1.5の`git-clang-format --diff 36a1c9c 1883e27 -- src/ui/win32/Direct2DRenderer.cpp src/ui/win32/Direct2DRenderer.hpp src/ui/win32/StatusTextLayout.hpp`差分なし。`git diff --check`成功。
- `python -B eng/protected-diff.py --base 36a1c9c --head 1883e27`成功。fixture 1853→1853、metadata/deleted/changed/added各0、protected files none。scope33は未測33。`out/protected/1883e27.json`。
- 自己レビュー: 通常6枠、録画時7枠、通知4枠、Ex入力3枠で固定上限内。UI書式再生成前に全枠破棄。借用書式は同一性比較専用。作成失敗で古い枠を出さない。実際のDPI遷移・device lost・割当失敗は未再現。

規則: ARC-001 / ARC-011 / CPP-012 / CPP-016 / QLT-001 / QLT-004 / QLT-012 / QLT-013 / QLT-014 / GIT-003 / GIT-004。保存schema・基準値・ゲートは変更なし。Waivers: none。main統合前の文書追記やcommit SHA変更だけでは成功済み検証を再実行しない。

## 5-cp — 改善一式の手動試用を開始（Issue #291）

2026-10-06 00:15:30 JST、hideの明示依頼で改善一式の通常Releaseを起動した。生成元`f42a95d`、exeは`D:/NeNeNib/worktrees/291-display-width-capsule-20261005/build/release-f42a95d/NeNeNib.exe`。起動記録は[manual-launch-2026-10-06.json](manual-launch-2026-10-06.json)。main統合・性能受理の保留は継続し、実機試用の結果は未受領。

- 対象と理由: 試用するexeの版の取り違えと起動失敗を確認する。`git diff --exit-code f42a95d HEAD -- src tests eng CMakeLists.txt`成功。`Get-FileHash -LiteralPath <exe> -Algorithm SHA256`がRelease記録の`44B017F33241E320BC5F182CEEB1CAFC6899255F898C0AC943703BC04F023900`と一致した。
- `Start-Process -FilePath <exe> -WorkingDirectory C:/Users/info/WORKS/NeNeNib -WindowStyle Normal -PassThru`で起動。`WaitForInputIdle(10000)`がtrue、プロセス継続とウィンドウhandle `462050`を確認。起動時PID `568`。通常環境を継承し、profile上書き・測定引数なし。
- 再利用: 現在の製品入力と同一の既存Releaseを使用。追加変更は日報・引き継ぎ・状態要約・起動記録のみで、アプリの自動テスト・再ビルド・性能再測定は選ばない。文書は差分空白・リンク・JSONと原記録の一致を確認する。
- 規則: QLT-001 / QLT-012 / QLT-013 / GIT-003 / GIT-004。製品コード・保存schema・基準値・waiverは変更なし。既存WVR-0001/0002はSDK署名の局所抑制のまま。

起動後はアプリを開いたままhideへ渡した。2026-10-06 00:21:14 JSTに`@(Get-Process NeNeNib -ErrorAction SilentlyContinue).Count`で0件を確認した。こちらでは終了・再起動を行っていない。終了理由と利用者の試用結果は未受領。worktreeは未統合の候補として保持する。起動確認は利用者の受理や性能合格を意味しない。

## 5-cq — 改善一式の統合の受理（Issue #291・ADR 0075・施主決定 D41）

2026-10-06 夜、設計席が日報と引き継ぎから再開し、止まっていた理由を保存済みの記録から計算し直した。hide が「昨夜の試用は問題なかった」「統合の決め方はおすすめで進める」「速さの検査は準備ができ次第回してよい」と答えた（D41）。
**5-ck〜5-co の `hold` は、その実験の事前条件に対する結果としてそのまま残す。** この節は main へ入れるかを ADR 0075 の 4 つで決めた記録で、過去の判定の上書きではない。

### 揺れの条件の読み直し（新しい測定なし）

`docs/quality/render-capsule-2026-10-05.json` の `summary` の 12 指標（3 本文 × O / A × 1 打鍵 / 200 文字）を、`plan.record.stability` の式のまま計算し直した。

| 本文・版・指標 | 6 本の値 ms（測った順） | 許容 ms | 前後半の差 ms | 判定 |
| --- | --- | ---: | ---: | --- |
| 長行・O・1 打鍵 | 28.020 / 28.645 / 32.491 / 27.459 / 29.337 / 28.423 | 2.8534 | 0.222 | 条件内 |
| 長行・A・1 打鍵 | 7.633 / 8.107 / 6.906 / 6.833 / 7.990 / 5.690 | 0.72695 | 0.800 | 外れ |
| ほかの 10 指標（空文書・16 MiB 短行の O / A、長行の 200 文字） | JSON のとおり | — | 0.002〜1.808 | すべて条件内 |

A の 6 値は単調ではなく、3 本ずつに分けるどの分け方でも許容を越える（値の散らばりが約 1 ms、許容が 0.73 ms）。O は 5 ms 散っても許容 2.85 ms に収まる。条件が値に比例して縮むために、速くなった版だけが外れた。
差 21.26 ms は散らばりの 20 倍を越え、6 組すべてで短縮。別の固定実験（各 240 打鍵・`tail`）では A の観測最大 7.978 ms、O は 240 件すべてが 16.7 ms 超。
空文書と 16 MiB 短行は 8 指標すべてが条件内で、A の中央値は O 以下（0.7395 → 0.7355 / 3.2275 → 3.1135 / 1.2650 → 1.2430 / 8.6080 → 7.1165 ms）。
O は `0dfad49`（文字組みとステータスの再利用まで）で main ではない。main からの悪化が無いことは、次の正式なゲートで示す（ADR 0075 決定 4）。

### 正式な速さのゲート（QLT-014）

確認する退行: 描画の経路と無効化のまとめ方を変えたので、既存の 7 本（起動・1 打鍵・200 打鍵・16 MiB・Ctrl+P の 5000 件）が基準値から悪化していないこと。
対象: Release `f42a95d`（`D:/NeNeNib/worktrees/291-display-width-capsule-20261005/build/release-f42a95d/NeNeNib.exe`・SHA-256 `44B017F33241E320BC5F182CEEB1CAFC6899255F898C0AC943703BC04F023900` を実行の直前に照合）。
`git diff --exit-code f42a95d HEAD -- src tests eng CMakeLists.txt` と `git diff --exit-code main HEAD -- eng` はどちらも終了 0（製品は試用版と同じ・計測器と基準値は main と同じ）。
実行の直前にビルドとテストのプロセスが 0 件・CPU の負荷 8% を確かめ、hide の了承の後に 1 回だけ回した。

`python eng/measure-speed.py --check --executable <上の exe>` → **7 benches checked, 0 regression(s), 0 unmeasurable**・終了 0。記録 `out/speed/2026-10-06T14-00-58Z.json`・ログ `out/291-rina-speed-f42a95d.log`。指紋 `bc8a356f37c68491`。

| ベンチ | 基準値 ms | 今回の中央値 ms（最小〜最大） |
| --- | ---: | --- |
| startup-first-frame | 191.488 | 201.062（195.290〜245.217） |
| startup-window-shown | 34.933 | 35.306（31.309〜45.079） |
| key-to-frame-single | 0.906 | 0.642（0.492〜0.654） |
| key-to-frame-burst-200 | 2.695 | 3.120（2.714〜3.175） |
| open-large-file-16mib | 249.783 | 244.790（241.787〜257.211） |
| key-to-frame-burst-200-16mib | 7.411 | 5.580（4.315〜6.215） |
| key-to-frame-palette-5000 | 2.630 | 2.070（2.045〜2.359） |

基準値と許容（25% / 床 2 ms）は変えていない。採用（`--adopt`）もしていない。Ctrl+P の 5 枚の PNG のうち `palette-2026-10-06T14-00-27Z-31360.png` を設計席が見て、暖機の `f` と件数 **1 / 5000** を確かめた。
起動の 1 本目 245.217 ms は 5 本の最大で、中央値は許容内。原因は調べていない。

### 再利用（QLT-012）

表示 22 場面の 0 画素差・IME の変換と確定・sanitizer の契約 604 checks・字形 180 画素対・`--display-line` 1114301 checks・`--vim-virtual-column` 424 checks・`protected-diff`（fixture 1853 → 1853）は 5-cm の成功をそのまま使う。製品の入力が `f42a95d` から変わっていないので実行し直していない。

### 独立レビュー

書いた席（Codex）とは別の実装席（Opus）が、結論を渡されずに `git diff main...f42a95d -- src tests CMakeLists.txt`（18 ファイル・+663 / −21）を読んだ。読むだけで、ビルド・実行はしていない。報告は `out/reports/review-291.md`、依頼書は `D:/NeNeNib/briefs/review-291-rina-20261006.md`。

- **統合を止める: 0 件。** 保持する 4 つ（本文の layout の現在と直前・ステータスの 7 枠・字体選択の 128 件・字形）を、フォント・DPI・テーマ・device lost・幅・IME・選択と検索のそれぞれで追い、古い結果が出る道・解放済みを触る道・借用が保持より長生きする道は見つからなかった。無効化をまとめる変更で `paint_pending_` が true のまま固まる道も無い。規則 ID の違反と色のリテラルも無い。
- 統合の後で直せる 4 件: waiver の期限 2026-11-04（→ #299）・字体選択の保持の取り付けの失敗で窓が終わる・起動中のフォントの追加が表示中の行に反映されない・保持の契約の試験がリポジトリに無い（→ #300）。
- 所見 7 件のうち、字形の経路の前提（96 DPI・恒等変換・整数の原点）を機械が守っていない点は #300 に入れた。

設計席は L2 の該当箇所（`Direct2DRenderer.cpp` の `create_body_formats`）を読み、書式を作れなかったときの既存の失敗（`RenderFailure::directwrite`）と同じ扱いであること・Windows 11 専用（D1）で `IDWriteTextFormat2` が必ずあることから、統合を止めないと判断した。製品を `f42a95d` から動かすと表示・IME・速さの成功を使えなくなるので、直すのは #300 で行う。

### Debug 構成のビルドとシンボル（レビューの所見 O6）

確認する退行: 新しい翻訳単位（`BodyGlyphCollector.cpp`・`FontFallbackCache.cpp`・`FontFallbackKey.cpp`）が Debug（ASan / UBSan・clang-tidy）で通ること、core に足した索引が core の外へ許可の無いシンボルを出さないこと（ARC-003 / ARC-007）、実際のビルドグラフの依存方向（ARC-002）。5-cm は Release と `nib_tests` の 2 scope だけだった。

`. ./eng/toolchain.ps1` の後、`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug` → `cmake --build build`（全 target・80 手順・clang-tidy 込み）は終了 0。新しい 3 つの翻訳単位と `Direct2DRenderer.cpp`・`EditorWindow.cpp`・`VirtualColumn.cpp` を Debug で作り直し、`NeNeNib.exe` と `nib_tests.exe` を含む全 exe がリンクした（`out/291-rina-debug-build.log`）。
`python eng/conformance.py --build-dir build` → `Conformance: 0 violation(s)`。`python eng/symbols.py --build-dir build --require core application` → `Symbols: 2 libraries checked, 0 violation(s)`。文書を足した後の `python eng/conformance.py` も 0 violation(s)。
CTest は回していない（`--display-line` と `--vim-virtual-column` は同じソースの成功を再利用・ほかの scope は差分が触れない）。

### 4 つの条件の結果

| ADR 0075 の条件 | 結果 |
| --- | --- |
| 独立レビュー | 統合を止める所見 0 件（`out/reports/review-291.md`） |
| 正式な速さのゲート | 7 benches checked・0 regression・0 unmeasurable |
| 必須 check | Ready の後の run（PR #293） |
| 施主の実機の試用 | 2026-10-06・Release `f42a95d`・「問題なかった」 |

統合の後に残す仕事: #298 長い行のベンチ・#299 SDK の固定署名の恒久規則（期限 2026-11-04）・#300 保持の契約の試験と取り付けの失敗。split の採用は保留のまま。PR #296 / #297 は #293 に含まれるので統合せずに閉じる。
適用: #291 / ADR 0075 / D41 / QLT-001 / QLT-010 / QLT-012 / QLT-013 / QLT-014 / GIT-003 / GIT-004。製品コード・保存 schema・基準値・許容の変更なし。Waivers: WVR-0001 / WVR-0002（変更なし）。

## 5-cr — SDK が固定した COM の署名の印（Issue #299・ADR 0076）

確認する退行: CNF-003 の waiver の要求に例外を作るので、(1) 印の無い NOLINT とほかの check を混ぜた NOLINT が今までどおり落ちること、(2) 印の形・表・置き場・本体の行数が外れたら CNF-012 で落ちること、(3) 既存の 5 か所が clang-tidy で今までどおり黙ること、(4) 製品のコードの行が変わらないこと。

- `python eng/test-conformance.py` → `Ran 253 tests`・`OK`。`SdkAbiChecks` の 13 件: 正例 4（1 行の `E_NOTIMPL`・本体 6 行ちょうど・`if` の早期 return を含む境界・`tests/ui/`）、反例 9（表に無い method・関数名が印と違う・本体 7 行・空行とコメントを数えて 7 行・NOLINT に `bugprone-x` を混ぜる（CNF-003 も）・置き場が `src/application/`・印の文法 4 通り・本体の無い宣言・印の無い NOLINT は CNF-003）。
- 実リポジトリの反例: `BodyGlyphCollector.cpp` の印を `DrawGlyphRuns` に変えると `CNF-012 ... is not in eng/sdk-abi-signatures.json` と `CNF-003 ... missing valid, scoped waiver`・終了 1。戻すと `Conformance: 0 violation(s)`・終了 0。
- WVR-0001 / WVR-0002 は `Status: removed`・索引は「なし」。閉じた後の `python eng/conformance.py` は 0 violation（CNF-004 の指摘 0）。
- `. ./eng/toolchain.ps1` の後 Debug で `cmake --build build --target nenenib_window` → 終了 0。clang-tidy（`CXX_CLANG_TIDY`）は `FontFallbackCache.cpp` と `BodyGlyphCollector.cpp` で何も出さない。
- `git diff -U0 src` は 10 行（5 か所の `// Waiver: WVR-000N` → `// SDK-ABI: ...` の削除と追加だけ）。`clang-format --dry-run --Werror` と `git diff --check` は終了 0。
- 表の「SDK の引数の数」は Windows SDK 10.0.26100.0 の `dwrite_2.h`（`MapCharacters` 11）と `dwrite.h`（`DrawGlyphRun` 7・`DrawUnderline` 5・`DrawStrikethrough` 5・`DrawInlineObject` 7）を読んで書いた。機械では照合しない。
- やっていない: Release・GUI・速さ・全件の CTest（製品のコードの行が不変で、実行ファイルに入る差分が無い）。

適用: #299 / ADR 0076 / CPP-012 / CPP-015 / CPP-019 / CNF-003 / CNF-004 / CNF-012 / QLT-010。`.clang-tidy` の閾値と check の一覧は変更なし。Waivers: none。

## 5-cs — 長い行の 1 打鍵のベンチと実機の基準値（Issue #298・ADR 0075）

#291 で縮めた長い日本語行の 1 打鍵を、速さのゲート（QLT-014）の 8 本目 `key-to-frame-single-long-line` として足した。実装は実装席（Opus）、測定と採用は設計席。

- 確認する退行: 文書が #291 の fixture と 1 バイトでも違うこと・既存 7 本の選択と採用の振る舞い・既存の基準値と許容の書き換え。製品のコードは触っていない。
- 文書: 道具が `out/speed/long-japanese.txt` を作り、SHA-256 `ef2d1511a696984dcd4184498a852d944a8ca366bf847f440d1e46b73b32a6c5`（6,219,000 bytes）を照合する。実装席が文書を作る関数だけを呼び、`D:/NeNeNib/evidence/290-split/documents/long-japanese.txt` と `cmp` で同一を確かめた。
- `python -m unittest discover -s tests/conformance -p test_speed.py` → 74 tests 成功（64 → 74）。実装席が置いた「どの機械にも長行の基準値が無い」という試験は採用の前の状態を固定するものだったので、採用と同じ commit で「実機の基準値があり、許容は床が効く」に置き換えた（QUALITY_GATES の文と基準値がずれたら落ちる）。`python eng/conformance.py` → 0 violation(s)。
- 実機の測定（hide の了承の後・1 回だけ）: Release `f42a95d`（SHA-256 `44B017F33241E320BC5F182CEEB1CAFC6899255F898C0AC943703BC04F023900` を直前に照合。main `143284d` の `src tests CMakeLists.txt` は `f42a95d` と同じ・`git diff --exit-code` 終了 0）。実行の直前にビルドとテストのプロセス 0 件を確かめた。
  `python eng/measure-speed.py --check --bench key-to-frame-single-long-line --executable <exe>` → 5 試行すべて有効、中央値 **5.431 ms**（5.348〜6.658・samples 6.658 / 5.348 / 5.817 / 5.431 / 5.356）。基準値が無いので `recorded only` と 1 行言って終了 0。記録 `out/speed/2026-10-06T14-40-13Z.json`（worktree `D:/NeNeNib/worktrees/298-long-line-bench`）。
- 採用: `python eng/measure-speed.py --adopt --bench key-to-frame-single-long-line --values <記録>` で、実機 `bc8a356f37c68491` のこの 1 本だけを足した。`git diff eng/perf-reference.json` は足した 5 行だけで、既存 7 本の値と許容（25% / 床 2 ms）は不変。`--check --bench … --values <記録>` → 1 benches checked・0 regression・0 unmeasurable（採用した値と比較器の整合の確認で、新しい測定ではない）。
- 5-cm の固定実験の 7.27 ms とは窓の大きさと試行の形が違うので、値を比べない。この基準値は計測器 `eng/measure-speed.py` の条件での値。
- 許容は床 2 ms が効く（5.431 ms の 25% は 1.36 ms）。7.431 ms を越えると落ちる。#291 より前の約 326 ms への後戻りは確実に落とせるが、1 ms 台の悪化は見えない。
- 残り: CI の指紋 `e7a87d5b` は記録だけ。200 文字の長行版は足していない。

適用: #298 / ADR 0011 / ADR 0016 / ADR 0075 決定 7 / QLT-001 / QLT-012 / QLT-013 / QLT-014。Waivers: none。

## 5-ct — 描画の保持の契約を窓なしの試験で守る（Issue #300・ADR 0077）

#291 の独立レビューの L2・L4・O2 を受けた。実装は実装席（Opus）、受理と実機の確認は設計席。製品のコードを触ったので、ADR 0075 の形で確かめた。

**見つかった契約の破れ。** `tests/ui` の試験を書いたところ、ADR 0073 の「未対応の装飾が来たら収集の全体を棄却する」が実際の DirectWrite では成り立っていなかった。`IDWriteTextLayout::Draw` は callback の HRESULT を無視して S_OK を返すので、下線・取り消し線・inline object・effect を `E_NOTIMPL` で断っても字形の保持は「使える」のまま残り、装飾だけが黙って落ちる。本文の layout は装飾も effect も使っていない（grep 0 件）ので表示は壊れていなかった。`BodyGlyphCollector` に棄却の印を持たせ、字形を渡す口 `take()` の 1 か所が決める形に直した。

- 確認する退行: 字形の保持の決め方と字体選択の取り付けを変えたので、画面に出るもの・起動と描画の速さ・保持の契約（一致条件・上限・棄却）。
- `ctest --test-dir build -R nib_window` → 1/1 成功・**1521 checks**・測れなかった検査 0（実装席・Debug・clang-tidy 込み）。環境依存として分けたのは画素の一致 180 組（3 family × 3 大きさ × 4 幅 × 5 行。保持した字形 == `DrawTextLayout`）と画面外の判定の境界で、この機械では全部測れた。
- 反例 2 つ（実装席）: `FontFallbackKey.cpp` の locale の比較を外すと 2 件、`take()` で印を見る 1 行を外すと 8 件落ちる。どちらも戻して成功を確かめ、`git diff` が空。
- `python eng/conformance.py`・`--build-dir build`・`python eng/symbols.py --build-dir build --require core application` → 違反 0。NOLINT は `tests/ui/ScriptedFontFallback.cpp` の CPP-019 の印つき 1 行だけ（CNF-012 が通る）。
- Release: `pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` → `d59f0b2`・SHA-256 `15C59E33BAA030015A6314476DEA07F03DA5C9FE6DE5CE563A01BD24A5B2685C`・1399296 bytes（`out/release/d59f0b2.json`）。
- 表示の前後比較（hide の了承の後）: `python -B D:/NeNeNib/scripts/300-rina-visual-20261007.py`（#291 の 22 場面の道具の出力先だけを変えた写し）を、統合済みの `f42a95d` と `d59f0b2` で 1 回ずつ。編集・クリック・選択・検索・scroll・幅・書式とテーマの変更・多言語 / 結合 / Tab / 双方向の **22 場面すべて本文とステータス 0 画素差**。刺激で画が変わることも道具が確かめる。設計席が `15-search.png` を見た。出力は `D:/NeNeNib/outputs/300-visual-20261007/`。
- 速さ（QLT-014・8 本）は 2 回ある。**1 回目は無効な測定として残す。**
  - 1 回目（`out/speed/2026-10-06T15-21-22Z.json`）: 起動の 3 本が上限を越えて終了 1（起動 240.750 ms / 上限 239.360・窓 46.123 / 43.666・16 MiB を開く 322.197 / 312.229）。打鍵の 5 本は基準内。測定の途中に同じ機械で別の席のビルドが動いていた（設計席は開始の前の 6 秒しか確かめていなかった。直後にビルドのプロセス 16・CPU 負荷 58%）。hide も「Claude と Codex をたくさん立ち上げている」と答えた。起動の全区間が一様に約 25% 遅く、この変更が触れていない区間（プロセスの立ち上げ・device の生成）まで遅い。測定の前提（測定中に重い処理を走らせない）を満たしていない。
  - 2 回目（`out/speed/2026-10-06T16-40-36Z.json`）: hide の「測っていいよ」の後、負荷 4〜11%・ビルドのプロセス 0 を確かめてから 1 回。**8 benches checked・0 regression・0 unmeasurable**。起動 199.818 ms・窓 32.740・1 打鍵 0.642・200 打鍵 3.044・16 MiB を開く 245.312・16 MiB の 200 打鍵 5.226・Ctrl+P の 5000 件 2.321・長い行の 1 打鍵 6.339。基準値と許容は変えていない。
  - 合格するまで測り直したのではない。1 回目は原因（測定中の負荷）を特定し、前提を満たした状態で 1 回だけ測った。
- 残り: 取り付けが失敗する経路の試験は無い（`IDWriteFactory2` の替え玉を作らない設計・ADR 0077）。`WM_FONTCHANGE` は扱わない（ADR 0071 の限界）。

適用: #300 / ADR 0071 / 0073 / 0075 / 0076 / 0077 / CPP-019 / CNF-012 / QLT-001 / QLT-012 / QLT-013 / QLT-014。Waivers: none。

## 5-cu — F1 の操作の一覧と、操作の 1 つの表（Issue #303・ADR 0078・施主決定 D43）

操作の名前・鍵・実行の中身を core の 1 つの表に寄せ、鍵で押しても一覧（F1・Ctrl+P の面の `?`）から選んでも ui の `run_operation` の 1 本で実行する。設計と受理は設計席、実装は工程ごとに別の実装席（Opus）。

- 確認する退行: 鍵の道を表に移したことで鍵の振る舞いが変わること（Ctrl+O・S・P・Z・Y・文字の大きさ）、タブとブックマークの鍵の意味（表を読む口に変えた）、面の既存の出どころ（`#` `*` `@` `/` `:`）の候補と見た目、面と打鍵の速さ。
- 工程 1（core）: `nib_tests --operations` 344 → 工程 2 の後 **516 checks**。`--tabs` 291・`--bookmarks` 40 は試験を 1 文字も変えずに成功（`tab_command_for` と `toggles_bookmark` が表を読む口になっても意味が同じ）。表 18 行と今の ui の振る舞いを 1 行ずつ照合し、合わない行は無かった。
- 工程 2（面と頼まれた操作）: `--command-palette` 371・`--background-work` 89・`--history` 38（`verify_quiet_paths` を含む）・`--user-theme-selection` 88。直した既存の試験は、設計で期待が変わった 3 か所だけ（表に無い記号の例を `?a` から `!a` へ・案内の文字列に「? 操作」・Ctrl+Shift+O は `open_file`）。
- 工程 3（ui）: `--bookmarks` 41（`?` の面での Ctrl+D は何もしない、を足した）。Debug のビルドは clang-tidy 込みで警告 0。
- #300 の上に載せ直した後、設計席が Debug で全 target をビルドし、上の 6 scope と CTest `nib_window` を回して成功。`python eng/conformance.py --build-dir build` と `python eng/symbols.py --build-dir build --require core application` は違反 0。許可シンボル・NOLINT・waiver は足していない。
- Release: `pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` → `4d84334`・SHA-256 `6E488BC4C2FF64B924E26EACEF187894F7CE0E2F1D3128636147D594641869BB`・1415168 bytes。統合する先頭と `git diff --exit-code 4d84334 HEAD -- src tests eng CMakeLists.txt` が終了 0 なので、以下の実機の結果をそのまま使う（QLT-012）。
- 速さ（QLT-014・8 本・hide の「測っていいよ」の後・負荷 6%・ビルドのプロセス 0）: **8 benches checked・0 regression・0 unmeasurable**。起動 193.579 ms・窓 31.171・1 打鍵 0.683・200 打鍵 3.184・16 MiB を開く 242.312・16 MiB の 200 打鍵 5.696・Ctrl+P の 5000 件 2.444・長い行の 1 打鍵 6.171（worktree の `out/speed/2026-10-06T16-42-43Z.json`）。Ctrl+P の画 `palette-2026-10-06T16-41-50Z-5752.png` を設計席が見て、件数 1 / 5000 と、既存の面の行の見た目が変わっていないことを確かめた。
- 実機の操作（hide の了承の後・1 回）: `python -B D:/NeNeNib/scripts/303-rina-verify-20261007.py --executable <Release 4d84334> --name first` → 31 場面・終了 0。文字と F1 は投函、Ctrl の鍵だけ本物のキー入力。出力は `D:/NeNeNib/outputs/303-verify-20261007/`。
  - 設計席が見た画: F1 の一覧（行の右端の鍵の枠・16 件）・「ほぞん」で 2 件・Ctrl+P の面の案内（「? 操作」が欄に収まる）・`#` の面の補足が今のまま・Vim の「ctrl+f4」でタブを閉じるが `Ctrl+F4`・Vim の「もとにもどす」は候補なし・Ctrl+Shift+; で文字が大きくなる。
  - 画素の比較で確かめたこと: 一覧から新しいタブ／前に使ったタブ（1 歩で確定）／文字を大きく／戻す／Vim へ切り替えで画が変わる。文字の大きさを戻すと前の画に一致。打った後の画とやり直した後の画が一致（Ctrl+Z → Ctrl+Y）。Vim の Ctrl+Z の前後が一致（何も起きない）。`#` の面の上で F1 を押すと、Ctrl+P の面で `?` を打った画に一致。
  - 一覧から「ファイルを開く」で OS のダイアログが出て取消で戻る。Ctrl+S（無題）で保存のダイアログ。閉じるときの「保存しますか」。3 つとも道具が見つけて答え、終了コード 0。
- 振る舞いの変化（設計席が受理した）: Ctrl+Alt+O・S・Z・Y は効かなくなる（新しい道は Alt のとき鍵を写さない。AltGr の配列で誤って走らない向きで、文字の大きさ・タブ・ブックマークの鍵の今の守りと揃う）。Shift を見ていなかった鍵（Ctrl+Shift+O など）は、表に行を明示して今のまま。
- 見つけて直していないもの: 面の「候補なし」の文字が面の左端に寄っている（この差分は触っていない・前からの見た目）。別 Issue にする。
- 実機で確かめていないもの: ライトのテーマでの鍵の枠・一覧のクリックでの実行・Ctrl+Tab を鍵で押して離したときの確定（経路は変えていない）・Ctrl+ホイール。

適用: #303 / ADR 0078 / D43 / FR-018 / ARC-001 / ARC-012 / CPP-002 / CPP-012 / QLT-001 / QLT-012 / QLT-013 / QLT-014。保存 schema・基準値・許容の変更なし。Waivers: none。

## 5-cv — 操作表の案内と旧設定を残す移行（Issue #304・ADR 0079・D42）

[PR #313](https://github.com/hideyukiMORI/nene-nib/pull/313)。hide の今回の分担指示により、設計・受理・画面と性能確認は設計サナ（Astra）、実装と検証器は実装サナ（SOL）、文書・整理の調査はLUNA、独立レビューは実装していないSOLが担当。デザインリナのCLIレビューも採用済みHintA/Bの読み取りに使用した。固定委任を次の作業の既定にはしない。

**変更と確認する退行。** 操作表から本文3行とステータス2組の表示値を作り、applicationが表示文脈、coreが排他的な配置を決める。未編集の無題にB、入力後/保存済み空ファイルには余白があるときA。入力面・通知・録画・IME中は隠す。設定は既存persist経路で保存成功後だけ更新する。v2を優先し、不在時だけ厳密なv1を読む。最初の実変更でv2へ保存し、旧v1は書き換えない。両snapshot/lockで移行競合を拒否する。旧設定/本文/undoの保持、表示の重なり、F1の共通keycap抽出、追加UI書式とframeの費用を確認した。

| 対象・退行 | 実行したコマンド | 結果 |
| --- | --- | --- |
| 設定型・署名・新frameの直接呼出しと描画資源 | 固定toolchainでDebugの `cmake --build build --target NeNeNib nib_tests nib_adapter_tests nib_theme_tests`、UI工程は `--target NeNeNib nib_tests --parallel 4` | clang-tidyを含め成功。最終の試験修正はnib_testsの対象TUだけ再ビルド |
| Ex保存成功/失敗・同値・他設定変更時のguide保持・本文/選択/undo不変 | `build/nib_tests.exe --ex-settings` | 209 checks成功 |
| 共通候補とCtrl+Pのguide設定 | `build/nib_tests.exe --command-palette` | 375 checks成功 |
| v1/v2の版・全必須鍵・不正値・往復 | `build/nib_adapter_tests.exe --settings-codec` | 34 checks成功 |
| 両側競合・両lock・旧版不変・壊れたv2の保存拒否・失敗後の再保存・移行後のv1非参照 | `build/nib_adapter_tests.exe --settings` | 227 checks成功 |
| 直接変更した利用者テーマ設定の復元と保存拒否 | `build/nib_theme_tests.exe --catalog` | 302 checks成功 |
| 表示条件・配置・未編集/undo/タブ・本文/面IME・設定失敗 | `build/nib_tests.exe --guide` | 519 checks成功。96/120/192 DPI、360/640/960 DIP、200/560 DIP、8/40 ptの36組と物理1pxの境界 |
| 短縮名追加で既存F1の検索/名前/鍵を変えない | `build/nib_tests.exe --operations` | 516 checks成功 |
| profile整理が両設定を残す | `python -m unittest discover -s tests/conformance -p test_frame_capture.py -k ProfileReset -v` | 2 tests成功。変更した4検証道具の構文とv2出力/v1入力も確認 |
| 追加型・実依存・OS境界・正準整形 | `python eng/conformance.py --build-dir build`、`python eng/symbols.py --build-dir build --require core application`、変更C++の `clang-format --dry-run --Werror`、`git diff --check` | 0違反、symbolsは2 libraries。すべて終了0 |

初期のlint拒否（Ex/試験mainの複雑度、新試験のoptional/ネスト）は分割・明示処理で修正し、抑制は足していない。最初のguide試験は120DPIの中央配置で6/519失敗した。228DIP=285pxを偶数幅へ置くと左右に1px差が生じるため、製品を変えず試験へ整数丸めを明記し、519成功を確認した。失敗ログも保持した。

**Releaseと画。** `pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` → `c89cf00`、SHA-256 `94654A18679A27E49C3038BC39568E997C11DEBC986F7D9EC80606F67245A0BC`、1430528 bytes。構成22.068秒・ビルド174.258秒。専用profile、実機120DPIで次を実行した。

- `python -B D:/NeNeNib/scripts/304-guide-verify.py --mode guide --name candidate-guide --executable <Release>` → 終了0、43場面。設計サナが全場面の画像（9テーマの原寸切出しと25場面の一覧、重要場面は原寸全画面）を確認。B/Aの排他、入力/undo/新規タブ/保存済み空、各入力状態、8/40pt、360DIP、低い200DIPのAへの切替、guide保存・再起動を受理。v1不変と起動だけではv2を作らないことも自動確認。
- 同じ道具の `--mode palette-reference` を基準Release `4d84334` と候補で各1回、`--mode palette-compare` で比較。dark13.5/light13.5/dark24の3場面はF1領域0画素差。基準は `git diff --exit-code 4d84334 be455a6 -- src CMakeLists.txt eng/targets.cmake eng/tool-versions.json` 終了0で製品同一を確認した。24ptで既存キー表示が切れる問題は基準画像で見つけ、#312へ分離した。
- `python -B D:/NeNeNib/scripts/304-guide-ime.py --name candidate-real-ime --executable <Release>` → 終了0。実日本語IME/SendInputの6原寸画像を確認。未編集Bと入力済みAが変換中に消え、Esc取消でそれぞれ戻る。変換下線は0→189→0/0→195→0画素、本文inkも元へ復帰。IMEopenは0→0に復元。合成したIMEメッセージではない。

**性能は直接影響する3本だけ。** `python -B eng/measure-speed.py --check --bench <以下の名前> --executable <Release>`。機械 `bc8a356f37c68491`、120DPI。起動の追加書式/本文案内と、案内を持つframe/ステータス保持の短行・長行の費用を確認する。ファイル読込や大きな候補一覧の経路は変更していないので他5本は実行していない。

| bench | 5有効試行の中央値（範囲）ms | out/speedの記録 |
| --- | --- | --- |
| startup-first-frame | 235.064（224.677〜242.036） | `2026-10-07T15-22-09Z.json` |
| key-to-frame-single | 0.549（0.452〜0.743） | `2026-10-07T15-23-03Z.json` |
| key-to-frame-single-long-line | 5.079（4.686〜5.510） | `2026-10-07T15-24-37Z.json` |

各1 bench checked・0 regression・0 unmeasurable。基準値/許容は不変。測定前CPU14〜30%、測定後43%で、ビルドのプロセスは0。静かな機械だったとは主張しない。長行の最初の呼出しは親が通常打鍵のexec終了を待たずに開始したため、準備のexeコピーでWinError32になった（測定前）。このログも保存し、通常打鍵の正常終了を確認してから初めて長行を測定した。成功した測定は繰り返していない。

**独立レビューと再利用。** 設定 `be455a6..7cbba2f` とUI `7cbba2f..c89cf00` を読み取りで別々に確認し、must-fixなし。設定codec/adapter/Exの規則と試験は後続工程で不変なので再利用し、追加frame/描画はguide/operations/実機で確認した。以後は文書だけで、`git diff --exit-code c89cf00 HEAD -- src tests eng CMakeLists.txt` の一致をもってReady/mergeで再利用する。全件検証は実行していない。

恒久記録: `D:/NeNeNib/evidence/304-operation-guide/out/`（成功・失敗ログ40ファイル、manifestで元のSHA-256と照合）、画像・独立レビュー・設計レビューは `D:/NeNeNib/outputs/304-guide/`。実行ファイルとRelease JSONは同フォルダの `release-c89cf00/`。画の判定は `visual-review.md`、撮影時点の `record.json` のpendingとは別に明記した。

未確認/限界: 96/192 DPIは純粋配置試験だけでモニター跨ぎは実機未検証。専用processはdriverで停止し、自然終了の再検証はしていない。lockを無視する外部編集との比較後の競合は従来どおり保証外。Solarizedのmutedは淡いが判読でき、色トークンは変えていない（コントラスト規格の適合試験ではない）。既存#309/#312は別件。

適用: #304 / ADR 0079 / D42 / FR-018 / ARC-001 / ARC-004 / ARC-008 / ARC-009 / ARC-010 / ARC-011 / CPP-002 / CPP-004 / CPP-011 / CPP-012 / CPP-017 / QLT-001 / QLT-002 / QLT-012 / QLT-013 / QLT-014 / GIT-003 / GIT-004。保存schemaはv2追加、v1は保持。Waivers: none。

## 5-cw — F1 の鍵の文字を本文の倍率で拡大しない（Issue #312・ADR 0078 決定 12）

[PR #316](https://github.com/hideyukiMORI/nene-nib/pull/316)。hide の指示で、設計・受理・撮影は設計リナ（Fable）、実装と自動の検証は背景の実装リナ（Opus・1 工程 1 席）が担当した。この分担を次の既定にはしない。

**変更と確認する退行。** `create_body_formats` の鍵の書式 `key_format_` が `ui_text_dips * ratio`（本文の倍率つき）だった 1 か所を `ui_text_dips`（12 DIP 固定）にした。面の行の名前・説明（`command_format_`）と右端の欄（`palette_row_note`・112 DIP）は本文の大きさに追従しないので、鍵だけが追従すると 24 pt で `Ctrl+Shift+S` が欄に入らず切れていた。既定の 13.5 pt は倍率 1.0 なので画は変わらない。gutter の書式は倍率を掛けたまま。ADR 0078 の決定 12 に 1 文を足した。core・application・操作表・案内の鍵（`guide_key_format_`）は触らない。

| 対象・退行 | 実行したコマンド | 結果 |
| --- | --- | --- |
| 書式の生成・警告集合・clang-tidy | 固定 toolchain で Debug の `cmake --build build --target NeNeNib` | 成功・警告 0（`out/312-build2.log`） |
| 層・実依存・正準整形 | `python eng/conformance.py`・`python eng/conformance.py --build-dir build`・変更 C++ の `clang-format --dry-run --Werror`・`git diff --check` | 0 violation・差分なし（最初の `--build-dir` は CMake File API の query を置かずに configure したため ARC-002 で 1 件落ち、query を置いて再 configure して 0。コード起因ではない） |
| 単体テスト | 実行しない | core と application を触らず、ui/win32 の書式に対象の試験は無い |

**Release と画。** `pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` → `315b153`（rebase 前の commit。rebase 後の HEAD と製品ソース・試験・道具は `git diff --exit-code 315b153 HEAD -- src tests eng CMakeLists.txt` で同一）、SHA-256 `e2919e141466afdc606cc4c6bd2051d617c05bda6a14c8dccc55a75fb9441f32`、1430528 bytes。専用 profile・実機 120 DPI・機械がほぼ空いていること（CPU 1〜7%）を hide に確かめてから、2026-10-09 深夜に設計席が実行した。

- `python -B D:/NeNeNib/scripts/312-keycap-verify.py --mode capture --root <worktree> --name after-315b153 --executable <Release>` → 終了 0・5 場面（F1 → `ctrl+shift+s`。dark 13.5 / light 13.5 / dark 24 / dark 8 / dark 40）。
- 同じ道具の `--mode compare --reference D:/NeNeNib/outputs/304-guide/base-f1 --candidate D:/NeNeNib/outputs/312-keycap/after-315b153 --expect-same dark-13.5,light-13.5 --expect-different dark-24` → 終了 0。基準は #304 の `base-f1`（Release `4d84334`・5-cv で `c89cf00` と F1 の領域が 0 画素差）。

| 場面 | F1 の領域の差分画素 | 判定 |
| --- | --- | --- |
| dark-13.5 | 0 | 期待どおり同じ |
| light-13.5 | 0 | 期待どおり同じ |
| dark-24 | 1606（範囲 x 865・y 196・w 139・h 28 = 鍵の箱だけ） | 期待どおり違う。基準では `Ctrl+Shi` で切れ、候補では `Ctrl+Shift+S` が欄に収まる |
| dark-8 / dark-40 | 比較なし | 設計席が目視。鍵は 12 DIP のままで欄に収まり、行の名前・説明も不変 |

面の外（タブの帯・本文・ステータスバー）の差分は 5 場面とも 0。画は `D:/NeNeNib/outputs/312-keycap/after-315b153/`、判定は `compare-315b153/record.json`（`visualReview` は pending のままで、目視の合格はここに書く）。

**速さは測っていない。** 差分は鍵の書式の大きさだけで、起動・打鍵・開く道の呼び出しの回数と経路は不変（`make_format` の回数も同じ）。QLT-014 の 8 本に関わる差分ではない。

**再利用。** Release の後は文書だけ（ADR 0078 の 1 文・この節・current.md）で、`git diff --exit-code 315b153 HEAD -- src tests eng CMakeLists.txt` の一致をもって Ready / merge で再利用する。全件検証は実行していない。恒久記録: 席の報告と log は `D:/NeNeNib/evidence/312-palette-keycap-size/`、依頼書は `D:/NeNeNib/briefs/impl-312-rina-20261008.md`。

適用: #312 / ADR 0078 決定 12 / ARC-001 / ARC-012 / CPP-017 / QLT-001 / QLT-012 / GIT-003 / GIT-004。Waivers: none。

## 5-cx — Ctrl+P の面の「候補なし」を行の名前の欄に書く（Issue #309・ADR 0060 決定 9）

[PR #317](https://github.com/hideyukiMORI/nene-nib/pull/317)。#312 と同じ分担（設計・受理・撮影は設計リナ（Fable）、実装と自動の検証は背景の実装リナ（Opus・1 工程 1 席））。次の既定にはしない。

**変更と確認する退行。** `draw_palette_choices` の候補 0 件の枝が「候補なし」を行の矩形そのもの（`palette_row`）に書いていたのを、候補のある行の名前が使う内側の欄 `palette_row_label(row, dpi, false)` に変えた 1 か所。書式（`mode_format_`）・色（`muted`）・文言・候補のある行の描画（`draw_palette_choice`）は不変。core は触らない（配置の関数はもうある）。退行として見るのは「候補のある面の画が 1 画素も変わらない」こと。

| 対象・退行 | 実行したコマンド | 結果 |
| --- | --- | --- |
| 翻訳単位・警告集合・clang-tidy | 固定 toolchain で Debug の `cmake --build build --target NeNeNib` | 成功・警告 0（`out/309-build.log`） |
| 層・実依存・正準整形 | `python eng/conformance.py`・`python eng/conformance.py --build-dir build`・変更 C++ の `clang-format --dry-run --Werror`・`git diff --check` | 0 violation・差分なし（`--build-dir` の最初の 1 回は File API の query 不在で ARC-002 に 1 件。query を置いて再 configure し 0。5-cw と同じ） |
| 単体テスト | 実行しない | core を触らず、ui/win32 の矩形の選び方に対象の試験は無い |

**Release と画。** `pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` → `b9ef1b1`（rebase 前の commit。rebase 後の HEAD と製品ソース・試験・道具は `git diff --exit-code b9ef1b1 HEAD -- src tests eng CMakeLists.txt` で同一）、SHA-256 `5C62D74828D96F77706F6A384C0DDDE6E59D36C71D7221DDD6FA6D2F658112A0`、1430528 bytes。before は #304 の Release `c89cf00`（SHA-256 `94654A18…45A0BC`・製品ソースは main と同一）。専用 profile・実機 120 DPI・機械がほぼ空いた状態で、2026-10-09 深夜に設計席が実行した。

- `python -B D:/NeNeNib/scripts/309-empty-label-verify.py --mode capture --root <worktree> --name before --executable <c89cf00>` と `--name after --executable <b9ef1b1>` → どちらも終了 0・4 場面（`dark-empty`: F1 → `zzzz`・`light-empty`: `neutral-light` で同じ・`dark-one`: F1 → `ctrl+shift+s`・`dark-ctrlp-empty`: Ctrl+P → `zzzz`。本文 13.5 pt）。
- `--mode compare --reference .../before --candidate .../after --expect-same dark-one --expect-different dark-empty,light-empty,dark-ctrlp-empty` → 終了 0。

| 場面 | 面の領域の差分画素 | 判定 |
| --- | --- | --- |
| dark-one（候補 1 件） | 0 | 期待どおり同じ |
| dark-empty | 661 | 期待どおり違う。「候補なし」が面の左端から行の名前の左端（候補のある行の名前と同じ x）へ移った |
| light-empty | 661 | 同上（ライト） |
| dark-ctrlp-empty | 661 | 同上（Ctrl+P の面） |

面の外の差分は 4 場面とも 0。`zzzz` は F1 の操作の一覧にも Ctrl+P の候補にも当たらず 0 / 0 と出ることを after の画で確認した。画は `D:/NeNeNib/outputs/309-empty-label/{before,after}/`、判定は `compare/record.json`（`visualReview` は pending のままで、目視の合格はここに書く）。

**速さは測っていない。** 差分は 0 件の面を描くときの矩形の選び方だけで、QLT-014 の 8 本の経路（起動・打鍵・開く・候補 5000 件の面）に 0 件の面は無い。

**再利用。** Release の後は文書だけ（この節・current.md）で、`git diff --exit-code b9ef1b1 HEAD -- src tests eng CMakeLists.txt` の一致をもって Ready / merge で再利用する。全件検証は実行していない。恒久記録: 席の報告と log と Release は `D:/NeNeNib/evidence/309-empty-palette-label/`、依頼書は `D:/NeNeNib/briefs/impl-309-rina-20261008.md`。

適用: #309 / ADR 0060 決定 9 / ADR 0008 / ARC-001 / CPP-004 / CPP-017 / QLT-001 / QLT-012 / GIT-003 / GIT-004。Waivers: none。

## 5-cy — 固定した処理を窓なしで前後比較する道具（Issue #329・ADR 0082）

hideの「閉じた小さな部分を対応前後で比べる」指示に従い、開発target `nib_perf_probes` と `eng/compare-probes.py` を追加。八つの固定workloadを同じharnessで実行し、入力生成・ポート準備・正しさの照合は測定の外へ置く。時計とファイル出力は既存Win32TimingAdapterだけを使う。製品には未使用の二つのMilestoneと名前の網羅分岐を追加しただけで、製品の入力・描画経路は不変。

**確認する退行。** 違う本文・未実行の処理・欠測を改善と誤認すること、新しいtargetの依存違反、結果の選別と上書き。固定ABBA・既定3block/各20sampleの全値、対応比、exe/入力hash、build metadataを記録し、欠測・timeout・checksum不一致は非0。性能の合否閾値は設けない。

| 対象 | 実行したコマンド・結果 |
| --- | --- |
| 比較器 | `python -X utf8 -m unittest discover -s tests/conformance -p test_compare_probes.py -v` — 8テスト成功。壊れたmarks、片側失敗、timeout、metadata、checksum差、上書き拒否を含む |
| Debug/Release target | 固定toolchain、各CMake構成で `cmake --build build/probes-<debug/release> --target nib_perf_probes --parallel 2` — 成功、警告・clang-tidy・DebugのASan/UBSanを維持 |
| 八場面とCLI | 両構成で各warmup1+sample1、本文/行数/count/undo/redo/marksを照合して全終了0。未知名・反復0/2049は終了2 |
| 比較器の実行 | 同じRelease exeでdisplay-line-long、ABBA1block/各1sample — observed、4全値/2対応比を保存。同一exeの比0.815705は性能差の根拠に使わない |
| 規約・依存・整形 | 変更源のconformanceとCMake File API依存検査0違反。`python eng/symbols.py --build-dir build/probes-debug --require core application` — 2library/0違反。変更C++の固定clang-formatとgit diff --check成功 |
| レビュー | 実装席と別の設計席がC++の測定境界、正しさ確認、Pythonの全値保存/失敗処理、CMake/層の変更を読んで確認。統合を止める所見なし |

初回Debugで新しい入力生成関数のnesting4がclang-tidyに拒否された。private関数へ分割して4a42aa0で修正し、対象TUを再buildした。初回ログも保存。製品runtime・全件回帰は実行していない。対象は比較器そのものであり、正式QLT-014や製品性能の受理を代替しない。

**証拠と再利用。** 実装commit `4a42aa09c160d441283a23b214de725082c19a52`。cleanで明示configure後、build中のソース変更なし。Release probe SHA256 `d602d19400243299a55bc33663d2919c087ec8117bdad004802171d33714e928`。`D:/NeNeNib/evidence/329-scoped-probes/` に実装報告・全対象ログ・最小実行JSON・probe exeを収載した。以後はこの記録と進捗文書だけで、src/tests/eng/CMakeListsの一致をもって成功結果を再利用する。configure時のcommit/dirtyは自動追跡されないので、比較版はclean commit→明示configure→buildで作る。

規則: ARC-001/002/003/007、CPP-002/005/007/012/016、QLT-001/012/013/014、GIT-003/004。ADR 0082、PROJECT_LAYOUT、利用文書を追加。製品設定schema・perf-referenceは不変。waiver: none。

## 5-cz — 読込・状態・履歴・描画の閉じた比較（Issue #320〜#324・#330・#333・#335）

hideが2026-10-09に設計・判断をAstraの設計サナへ委ね、背景席での実装・調査・検証を指定した。その後「閉じた小さな部分の対応前後で、規約内の高速化を試す」と指定した。設計サナが先行ADR・同一harnessの比較計画・採否を担当し、実装と別の席が各差分をレビューした。分担は今回の指定範囲だけで、次の既定にはしない。

**経路と退行。** #320は検証済み所有値DetectedTextからTextBufferへ移し、改行索引と実FilePortの不要な初期化を減らす。#321は一時EditorStateを正典の更新経路へ渡す。#322は意図の反映値EditorDeliveryから可視行を分け、実描画/当たり判定でframeを作る。#323は確定済み履歴、Vimレジスタ本文、記録した鍵列を私有の不変値として共有する。#330は同じUTF8 validatorでASCII連続区間を数える。#324/#335は行番号・描画先・mode文字組みを寿命内で保持し、block caretも本文glyph経路へ通す。文字コード・BOM・改行・保存印・旧snapshot・undo/redo・Vim記録/再生・選択・clip・失敗時の再試行を直接の退行対象とした。

先行設計はADR0080/0081/0083/0084/0085/0086/0088/0089/0090。内部公開APIと所有者の変更を記録し、製品の保存schema・基準値・許容・抑制・allowlistは変更しない。比較器の追加4workloadはADR0082に先行追記し、旧8workloadを変更せずDebug/Release各warmup1/sample1、比較器9テスト、独立レビューを確認した。

| 対象と回帰の範囲 | 実行コマンドと成功結果（各作業木のout/reportsが全command/logを記録） |
| --- | --- |
| #320 所有移管・read長さ・encoding/保存 | 固定toolchain Debug対象build、`build/nib_tests.exe --file-text` 293、buildをcwdに `nib_adapter_tests.exe --files` 74 |
| #321 状態更新・一意の要求・公開caller | `build/nib_tests.exe --application` 15749、`--tabs` 291、`--operations` 516、`--background-work` 89、`--vim-search-incremental` 141 |
| #322 delivery/frame・IME snapshot・公開caller | 同じ5対象が15980/291/516/89/141。旧試験20fileの554呼出しをapply_frameへ機械置換した証拠を保存、期待値不変 |
| #323 履歴 | `--edit-history` 50、`--application` 15980。確定後の旧値、undo/redo、分岐、保存印を確認 |
| #323 レジスタ | `--vim-register-snapshot` 150、`--application` 15980、`--vim-macro` 1735、`--vim-clipboard` 191。raw入力/取得後の変更から隔離し、copy-only xvalueでも元を保つ |
| #323 鍵列 | `--vim-recorded-keys` 44、`--application` 15980、`--vim-dot` 1619、`--vim-macro` 1735、`--vim-search-incremental` 141。64鍵境界、旧値、分岐、search文字所有を確認 |
| #330 UTF8 | `--utf8` 2646、`--file-text` 293。word境界/端数/offset/NUL/非ASCII/不正列。既存scalar検査を維持 |
| #324 描画・UTF16 | `--utf16` 68、`--display-line` 1114303、`nib_window_tests.exe` 1560/0 failure/0 not measured。C4復元後もdisplay-lineだけ再確認 |
| #335 block clip・mode資源 | `nib_window_tests.exe` 2280/0 failure/0 not measured。既存180組×4clipの720比較を追加、Debug製品もbuild成功 |
| 依存/規約/保護 | 各採用差分でconformance/File API 0、固定clang-format/whitespace成功。変更したcore/applicationのsymbols 2libraries/0。protected fixture1853件の内容/metadata/増減は不変。全scope件数比較は未測 |

初回失敗と修正は捨てていない。#320の追加試験が後続fixtureを壊した順序、#321/#322/#324の追加試験のlint/include、#323 factoryのinline解析と新試験のuse-after-move/nesting、#330の初期configure/buildは各報告と最初のlogに残す。抑制や基準変更で通していない。copy-only値を再読する新試験は非const xvalueを明示し、将来本物のmoveで元が壊れれば落ちる契約を維持した。

**固定前後比較。** 同じtoolchain・入力・harnessでABBA 3block。通常20sample/実行（各側120）、大きい履歴・register・2000鍵は3（各側18）。warmup1は区間外、本文/undo/redo/countを区間外で照合し、全sample・対応比・metadata・exe/入力hash・marksを保存。下表は局所処理の中央値、単位us。別batchの数字を直列に足した改善率や起動全体の改善率にはしない。

| 候補と固定処理 | before → after | 対応after/before比中央値 | 判断 |
| --- | --- | --- | --- |
| #320 メモリFilePortから16MiBを開く | 49063.5 → 23901.5 | 0.488439 | 採用 |
| #321 通常200入力 | 1048 → 541 | 0.517074 | 採用 |
| #321 16MiB削除後200入力 | 5515758 → 1365361 | 0.244605 | コピー減、残る崖は#323で処理 |
| #322 通常200入力 | 572 → 287 | 0.514500 | 採用 |
| #323 履歴1MiB/16MiB削除後200入力 | 95850.5 → 301 / 1511898.5 → 304.5 | 0.003129 / 0.000210 | 保持サイズ由来の崖を除去 |
| #323 register1MiB/16MiB保持200入力 | 196116 → 3716.5 / 2858266 → 3582 | 0.019023 / 0.001286 | 採用 |
| #323 録画200/2000入力 | 2454.5 → 1123 / 150990.5 → 13902.5 | 0.459314 / 0.092829 | 採用、全ての入力費用を定数化したとはしない |
| #330 ASCII16.8MB検証 | 15741 → 1345.5 | 0.086486 | 採用 |
| #330 日本語6.219MB検証 | 5812.5 → 5626 | 0.965417 | 分布が重なり改善の主張なし |
| #330 メモリFilePortから16MiBを開く | 26135.5 → 10069 | 0.393079 | 採用 |
| #324 C4表示行の再encode省略 | 20 → 21 | 1.050000 | 改善せず不採用、元の実装へ復元 |
| #333 ThinLTO buffer/open/入力/表示 | 7105→7266.5 / 11271.5→11566.5 / 512.5→520.5 / 20→18 | 1.009759 / 1.007808 / 0.964231 / 0.900000 | 主要区間の明瞭な改善なし、製品不採用 |

#335は窓なしprobeで描画を測れないため、同じ多言語80行でNORMAL移動/録画移動をABBA・各側120入力、全12起動/45marks照合で比較した。input_received→frame_presentedは16988→16798us（0.978760）、17317→16614us（0.957283）。分布が重なるので高速化の量は主張せず、同じ保持資源と描画経路への集約として採用。全22本文/ステータス画像比較は0画素差。R2/R4/R5/R6個別の速度量は未測で、統合の正式8本と画素/契約で確認する。

batch3の冒頭8秒にregister席のRelease conformanceが重なった。`environment-note.json`に時刻を記録し、sampleは一つも捨てず測り直していない。register普通入力308.5→391us（対応比1.26418）を清浄な単独比較や改善として使わない。履歴の普通入力290→317.5usなど小さい区間の揺れも原記録に保持し、正式受理は5-daで判断する。外部案件の機械負荷は管理していない。

#333は実bitcodeのsymbolsが303違反。address表示を読み取り診断で補正してもcompiler由来`__ImageBase`がcore/application各1件残った。parser/allowlistを広げず不採用。構築と12workloadの短い正しさ確認、PE import同一、15872bytes縮小は確認したが、採用しない候補の全Release契約は実行していない。ADR0087は実験記録だけを収載、ThinLTO flagsは統合しない。

証拠は `D:/NeNeNib/evidence/speed-optimizations-20261009/`。3batchのplan/result/rawと各component out/report/製品/probe/独立レビューをsource-commit-mappingとSHA manifestで対応付ける。作業木名や文書commitをexeのcommitと同一視しない。候補C5〜C9/IO9、D2/R8/B3、tint_runs等は別調査であり、この工程ですべての候補を実験済みとはしない。

## 5-da — 高速化一式の統合受理（Issue #334・PR #336・D41）

採用差分を統合した製品commit `a64a5d473dffd72c32c4b5936e25073bc590ca27`。manual解決はADR索引、CMakeへの新TU、NibTestsの42selectors（旧35保持、新7）と既定契約接続。VimStepは受理済み2枝の正確な和、renderer/UIは#335の579e21bと同一。独立レビュー `D:/NeNeNib/outputs/334-review/review-334-integration.md` はP0/P1/P2なし。

**統合で追加確認した境界。** 最初の合成でfile-text295/utf8 2693/edit-history50/application15989、最後の合成でregister-snapshot150/recorded-keys44/application15989/macro1735が全成功。固定toolchain Debug `cmake --build build --target nib_tests NeNeNib --parallel 2`、`python -B eng/conformance.py --build-dir build` 0、`python -B eng/symbols.py --build-dir build --require core application` 2/0、protected1853件不変。結果はout/334-test-*.logと334-final-*.log。componentのdot/clipboard/search等は個々の契約の成功証拠として再利用し、共通状態の合成を新しいapplication/macroで確認した。依存全体が不変とは主張しない。renderer/WICは同一source/試験/環境なので#335の2280成功を再利用する。

**Release。** `CMAKE_BUILD_PARALLEL_LEVEL=1; pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` が成功。`build/release-a64a5d4/NeNeNib.exe`、SHA256 `B977BC8791CBA1D1925ABB50B55EF2D441F5470857191E9FEDD6628DE820F285`、1455104bytes。以後は文書だけで、src/tests/eng/CMakeListsの一致を確認してReady/mergeで成功結果を再利用する。

**hideの機械で設計席が試用。** `D:/NeNeNib/scripts/334-visible-speed-visual-20261009.py` により#320の206769f（描画sourceは変更前mainと同じ）とa64a5d4を専用profile・120DPIで撮影。既存の文字/編集/undo/選択/scroll/字体/テーマ22場面に、録画、F1標準/24pt、Ctrl+P候補あり/なし、最小化復元、録画中resizeを加えた35場面。`--compare` はすべて本文/ステータス0画素差。afterはsettings.v2と正常終了0も確認、代表画像を設計席が目視した。beforeの撮影後に旧診断のsettings.v1読取りが落ちたため、全35画像と実settings.v2を別検証で確認した。撮り直さず、旧logも保存。beforeの正常終了は未観測と記録した。

`D:/NeNeNib/scripts/334-read-save-20261009.py` は実FilePortでUTF8/UTF8 BOM/CP932、CRLF/LFの本文を開きXを入力して保存し、全byte一致/dirty印解消/正常終了0を各確認。初回の既存1.2秒待ち検証は一時ファイル生成中に失敗した。製品を変更せず、完了状態を最大20秒観測する診断に直しbefore2a31bb4/aftera64a5d4の各3件を確認した（after観測0.95/0.83/0.11秒）。最初の失敗・runner・一時ファイルを残す。これは機能診断で速度の再試行や基準変更ではない。

**正式速度。** 読込・通常/Vim状態・frame・共有rendererに跨る差分なので、正式8本がそれぞれ異なる直接経路を覆う。全件テストではなく、この8本を一回だけ選んだ。build/runtime/大きいcopy終了後、`python -B eng/measure-speed.py --check --executable <上記Release>` が終了0。`out/speed/2026-10-09T13-38-37Z.json`、指紋bc8a356f37c68491（i9-10850K/RTX3090/120DPI）、各5sample、欠測0。palette画像に候補5000件を確認した。

| 正式bench | 中央値ms | min〜max ms |
| --- | --- | --- |
| startup-first-frame | 228.385 | 217.271〜256.032 |
| startup-window-shown | 37.163 | 34.963〜42.978 |
| key-to-frame-single | 0.545 | 0.511〜1.320 |
| key-to-frame-burst-200 | 1.781 | 1.615〜1.872 |
| open-large-file-16mib | 243.259 | 230.405〜298.825 |
| key-to-frame-burst-200-16mib | 1.859 | 1.793〜3.521 |
| key-to-frame-palette-5000 | 2.300 | 2.206〜3.886 |
| key-to-frame-single-long-line | 4.831 | 4.725〜6.316 |

8 checked / 0 regression / 0 unmeasurable。各5sampleと内訳を保持し、正式道具の最終marks保存を全試行raw保存とは呼ばない。旧基準の既存失敗#331を上書きせず、本候補の結果として別記録にした。perf-reference/許容は不変。

残る限界: GPU device lossと物理DPI遷移/COM失敗注入は未再現。activeな履歴の入力文字列、Vimの回数付き入力など残る成長費用は別候補。hide本人が操作したとは記録しない。独立レビュー・正式8本・設計席の実機試用を満たし、必須CIを確認してsquash mergeする。規則 ARC-001/002/004/005/007/008/011/012、CPP-003/005/007/008/012/016/017/018、QLT-001/012/013/014、GIT-003/004、D41。waiver none。

## 5-db — 行取得・候補照合・文字コード変換の固定比較（Issue #337〜#341）

同一harness `77730f7a927dc89965ffad948880e68dbed137f7` をmain eef5aad/#338/#339/#340 A/B/Cへ適用。#337の追加12workloadは準備・入力hash・完全出力検証・結果破棄を2marksの外へ置き、旧12の本文/入力を維持した。Debug/Releaseの新12各warmup1/sample1、Python14例、conformance/File API、symbols2librariesが成功。性能試料へ短いsmokeの値を使わない。

**選んだ機能検証。** #338はpiece境界・CRLF/LFの行端・任意byte offsetの位置計数、#339は順位/包含/本文不変のcaret/失効/候補到着/原alias変更、#340は所有移管・全UTF8出力・CP932の不正列とNUL/空/長短の独立性が直接の退行リスク。固定toolchainのDebug target buildと次の最小scopeを実行した。

| 候補 | コマンドと実結果 |
| --- | --- |
| #338 | `nib_tests.exe --buffer-range` 227、`--application` 15989、`--vim-line-jumps` 999、全失敗0 |
| #339 | 所有境界修正後に `nib_tests.exe --command-palette` 480、`--operations` 516、`--background-work` 89、全失敗0 |
| #340 A/C | `nib_adapter_tests.exe --code-pages` 各22/失敗0。未知/余剰引数は終了1、file resetなし |
| #340 B | `nib_tests.exe --utf16` 71、`nib_adapter_tests.exe --code-pages` 22、全失敗0。Cはcore/tests不変なのでBのUTF16/symbolsを再利用 |
| 共通 | `python -B eng/conformance.py --build-dir <対象build>` 0、`python -B eng/symbols.py --build-dir <対象build> --require core application` 2libraries/0、固定clang-format/whitespace成功。`python -B eng/protected-diff.py --base eef5aad --head <対象HEAD>` はfixture1853・metadata/内容/増減・保護file不変。全scope runtime件数は未測 |

#339の独立レビューはraw vectorのmove前aliasが内部snapshotを書き換える反例を発見。ADR0091へ公開const&入口で一度copyする決定を先行追記し、opened/extendedと反例契約を修正した。旧471checks/f576製品は最終成功の代用にしない。修正後buildの新test optional guard4箇所をtidyが拒否した記録、正規File API query前のKeyError、修正成功を全て保存。規約や抑制を緩めていない。#337/#338/#339/#340の独立レビューは最終固定sourceでP0/P1/P2なし。

**固定比較。** `D:/NeNeNib/outputs/scoped-probes-batch4-20261009/predeclared-cases.md` を実行前に作成し、`plan.json`（SHA256 `0d8fdd9335cf148967bded7b575e91433f6945d1628aec6f77a79ff4093c8e84`）へ全6exe/fullcommit/SHA/入力SHA/比較器SHAを固定した。`python -B -X utf8 D:/NeNeNib/scripts/run-scoped-batch-20261009.py <plan.json>`。ABBA3blocks、warmup1は区間外、paletteは各側120samples、他は各側18samplesの計画。開始23:56:24 JSTにtaskのbuild/static/GUI/copy停止を確認、clang/ninja/cmake/lld/製品/probeのprocess inventoryは空。外部案件の活動は管理していない。

14組中13組はobserved。caretのみ分解能未満でnot-observedとなり、batchの終了値は2。欠測を成功とせず、全raw・試行順・stdout/stderr・失敗を保持し、再実行や除外を行っていない。13組の全exe SHAは終了後も不変。以下は局所処理の中央値usで、正式QLT-014や各機能全体の改善率ではない。

| 固定case | before → after us | 対応after/before比中央値 |
| --- | --- | --- |
| 338-crlf-lines | 557.5 → 7 | 0.011812 |
| 338-lf-lines | 291 → 6 | 0.017734 |
| 338-position-single | 6171.5 → 3144 | 0.506766 |
| 338-position-scattered | 9374.5 → 7831.5 | 0.800401 |
| 339-listed-name | 600.5 → 388 | 0.653393 |
| 339-listed-location | 1953.5 → 1440.5 | 0.722244 |
| 339-narrowing | 610.5 → 39 | 0.063794 |
| 339-caret | 計測分解能未満・not-observed | 算出しない |
| 340-transfer | 73656 → 72070.5 | 0.986353 |
| 340-reserve-japanese | 44585 → 35300 | 0.791671 |
| 340-reserve-ascii | 15955 → 17097 | 0.997582 |
| 340-reserve-supplementary | 31690 → 26212.5 | 0.825481 |
| 340-reserve-codepage | 70932.5 → 64380 | 0.918135 |
| 340-one-pass-codepage | 58699.5 → 47816.5 | 0.827020 |

caretはbefore1process20試料535〜1216us、after1process20試料0〜3us（0が10個）。比較器は非正の区間を拒否して停止した。全120試料が成功したとはしない。0を正数へ補正せず、paired ratioを作らない。この機能は本文不変時の結果共有という単一経路と契約試験を根拠に採用する。

#338はCRLF/LFの先頭30行抽出と57,344bytesの位置換算を採用。fragmented positionは範囲が重なるため、特定の20%を任意の入力へ一般化しない。#339は名前/場所のコピー削減と末尾絞り込みを採用し、場所＋名前の連結は残す。opened/extendedの防御copyは区間外なので、一覧を開く/到着する全体の改善を主張しない。

#340 Aは内部owned成功値の移管として採用、単独速度は分布重複で利益未確認。Bは日本語/補助平面の再確保を減らす利益を観測して採用。ASCIIは対応比0.997582・分布重複で利益未確認、要求capacityは最大3bytes/unit（従来の3倍）。CはCP932の前計数/zero fillを省き、検査済み上限へ一回書込。2byte日本語のwide要求capacityは実使用の最大2倍。この代償は明示し、RSSを測った倍率とはしない。A/B/Cの値は別の比較で、改善率を足さない。

統合source `2993bb482e7cf117aefe269226eac606d0e72b1c` は#337 finaldocs7e9155a、#338 3844e677、#339 7ebed77、#340 5f93135の合成。手動解決はADR索引の0091/0092の両行保持だけ。`out/341-component-reuse-proof.json` は各componentのsrc/tests/eng/CMake変更ファイルと統合版の完全一致、application sourceのbase一致を実証する。独立レビューでも新しい結合退行の具体的根拠が無ければ、componentの成功を工程やHEADの変更だけで再実行しない。統合の最終Release・実機・正式速度は5-dcへ分ける。

全26候補の採否と未実験の理由は[有限台帳](../design/2026-10-10-speed-candidate-disposition.md)。規則 ARC-001/002/003/004/005/007/012、CPP-003/005/007/008/012/016、QLT-001/012/013/014、GIT-003/004、D41。保存schema・閾値・allowlist・fixture期待値不変。waiver none。全証拠はD側恒久archiveへSHA/path/commit対応付きで追収載する。

## 5-dc — 行取得・候補照合・文字コード変換の統合受理（Issue #341・PR #342・D41）

製品source `2993bb482e7cf117aefe269226eac606d0e72b1c` は5-dbの4部品の和。独立レビュー `D:/NeNeNib/outputs/341-review/review-341.md` は各作業木673fileを照合し、製品と機能testの計14fileの変更が各成功時と一致すること、本文byte位置と候補indexを混ぜないこと、CP932→UTF8→TextBufferの防御境界が残ることを確認した。最終ログ追補を含めP0/P1/P2なし。部品scopeをもう一度走らせる根拠となる新しい結合差分はなく、`out/341-component-reuse-proof.json` と意味境界レビューを根拠に5-dbの成功結果を再利用した。

**正規Releaseと静的境界。** `CMAKE_BUILD_PARALLEL_LEVEL=2; pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` は固定clang-cl19.1.5/Releaseで成功。`build/release-2993bb4/NeNeNib.exe` は1447424bytes、SHA256 `CDF2433522FA5F672EAC2DB7B3747ED56F7337EDC0EB71160352DCBB20C30F8B`。`out/release/2993bb4.json` のconfigure2.959/build322.701/計325.66秒。最初の呼出しは出力先outが無くshellのredirectだけが失敗し、build自体は開始していない。out作成後の一回の実buildが上記であり、初回失敗を成功へ読み替えない。

最終CMake構成と規約境界を確かめるため、正規File API queryを置いた同じRelease構成に対して `python -B eng/conformance.py --build-dir build/release-2993bb4` は0、`python -B eng/symbols.py --build-dir build/release-2993bb4 --require core application` は2libraries/0。`python -B eng/protected-diff.py --base eef5aad --head 2993bb4` はfixture1853→1853、metadata0/deleted0/changed0/added0、保護files changed none。scopes43のうち新規buffer-rangeと旧42のruntime件数比較は未測で、同数成功とは記載しない。各logは `out/341-{release-file-api-configure,conformance,symbols,protected}.log`、機械記録は `out/protected/2993bb4.json`。

**設計席による実機の対象確認。** TextBufferと入力文字変換の接点、一覧の部分絞込と失効が直接影響するため、`python -B -X utf8 D:/NeNeNib/scripts/341-boundary-visual-20261009.py --executable <前後Release> --name <before|after>` と同runnerの `--compare` を選択。前は第一陣a64a5d4、後は2993bb4、共通の固定文書/設定v2/120DPI。同じCRLF/LF本文の断片化編集・行端・スクロール、一覧到着・名前/場所・末尾追加/途中編集/削除・caret・scope・操作一覧の25場面が本文/ステータス0画素差、刺激到達・設定v2・正常終了0も確認。`D:/NeNeNib/outputs/341-boundary-visual-20261009/comparison.json` と全PNGを保存し、代表画面を目視した。既存35場面とWIC2280はrenderer/UI不変なので再実行していない。

実FilePortから変換→本文→保存までを覆うため、`python -B -X utf8 D:/NeNeNib/scripts/341-read-save-20261009.py --executable <2993bb4 Release> --output D:/NeNeNib/outputs/341-read-save-20261010` を実行。UTF8、UTF8 BOM/CRLF、CP932/LFの3通りで編集後の全byte一致、未保存印解消、正常終了0。完了状態を最大20秒観測し、1.2秒固定待ちへ戻していない。結果は同outputのresults.jsonと全撮影/設定。今回のbeforeは同じ期待byteと前版で成功した5-daを再利用。

**正式速度の選択理由と結果。** core::to_utf8の確保変更はLocalSettingsPath/AbsolutePathの起動、EditorWindowの通常入力、FolderListingの候補、CP932に直接波及する。TextBufferは空/大文書/長行のframeと入力に、一覧は5000件絞込に直接使われるので、この異なる直接callerを覆う正式8本を一回選んだ。全件契約/coverage/oracleは実行しない。taskのbuild/static/GUI/copy停止とprocess inventory空を `out/341-formal-environment-before.json` に記録し、00:08:26 JSTに `python -B -X utf8 eng/measure-speed.py --check --executable D:/NeNeNib/worktrees/341-speed-integration/build/release-2993bb4/NeNeNib.exe` を開始。終了0、**8 checked / 0 regression / 0 unmeasurable**。

`out/speed/2026-10-09T15-10-44Z.json`、指紋bc8a356f37c68491（i9-10850K/RTX3090/120DPI）、各5sample。palette撮影に1/5000の候補表示を目視確認した。正式道具のsamplesと最終marksを保存するが、全試行のrawを保持する局所harnessとは区別する。

| 正式bench | 中央値ms | min〜max ms |
| --- | --- | --- |
| startup-first-frame | 221.150 | 211.118〜239.068 |
| startup-window-shown | 40.537 | 36.082〜45.818 |
| key-to-frame-single | 0.589 | 0.479〜0.672 |
| key-to-frame-burst-200 | 1.742 | 1.593〜2.115 |
| open-large-file-16mib | 249.044 | 237.587〜311.457 |
| key-to-frame-burst-200-16mib | 1.817 | 1.742〜2.163 |
| key-to-frame-palette-5000 | 2.470 | 2.040〜2.755 |
| key-to-frame-single-long-line | 4.631 | 4.548〜4.810 |

正式8本は既存基準に対する退行判定で、前の正式一回との差を因果的な改善量としない。外部案件の機械負荷は管理していない。旧版での#331は別記録として保持し、許容や基準を動かしていない。

全候補の未実験、caretの分解能未満、B ASCII/A移管の利益未確認、一覧入口copyの区間外、B/Cの要求容量増を5-dbと採否台帳から継承する。peak memory、物理DPI遷移、device-loss/COM失敗注入は未測。独立レビュー・実機対象確認・正式8本を満たし、必須CIを確認してsquash mergeする。以後文書だけなら関連source/test/toolの一致を確認して成功証拠を再利用する。規則ARC-001/003/004/005/007/012、CPP-003/005/007/008/012/016、QLT-001/012/013/014、GIT-003/004、D41。waiver none。

恒久証拠は `D:/NeNeNib/evidence/speed-optimizations-20261009/append-20261010-wave2/`。元path/commit/全file SHAのmanifestと最終main/PR対応、削除前監査を保存する。main統合・収載・必要反映の完了後、追加作業木と一時出力を整理しbranch/commitを保持する。

**統合・収載の実結果。** [PR #342](https://github.com/hideyukiMORI/nene-nib/pull/342)のhead310163cで必須checkが[run37950695621](https://github.com/hideyukiMORI/nene-nib/actions/runs/37950695621)に成功し、00:18:50 JSTにsquash mergeした。mainは `b79c9e6aa9b6444295725441db5e850db2c5bc85`、本体ff-only同期後clean。`git diff --quiet HEAD 2993bb4 -- src tests eng CMakeLists.txt` は0で、実測済み製品入力の一致を確認。文書だけの310163cと統合SHAを理由に製品検証を再実行していない。#337〜#341はclosed。

第二陣archiveの本manifest5637file/96,820,501bytesは全元/先SHA一致、manifest SHA256 `ea3032adebf8994fa375be6cbcfee63850181b3d1f17a6d2d47405f6e5a0bbc4`。最終reviewの1fileは `supplement-341-review/`、確定docs8file/報告/PR本文とmain対応は `supplement-341-final/` に独立追補する。元のmain manifestを上書きして原結果を消さない。cleanupの詳細は同恒久先の監査へ残し、branch/commitは保持する。

00:23:38 JSTまでに#337〜#340のD作業木4件と、完全収載済みのreview4件/batch4原出力を削除。`D:/NeNeNib/outputs/cleanup-speed-wave2-20261010/cleanup-result.json` は全remove exit0、4枝/commit保持、本体clean、live/links/未保存/唯一未収載なしを記録する。最終#341作業木と今回の他の重複出力は文書PR #343の統合後までに整理し、終了時の監査とmain SHAは恒久archiveのcloseoutへ追記する。

## 5-dd — 保存・断片収集・検索列挙の第三陣局所比較（#346〜#349）

hideの継続実験と今回の分担指定に従い、設計席が入力/区間/採否を決め、別席が実装と対象試験を行った。規則ARC-001/003/007/009、CPP-002/003/005/012/016、QLT-001/008/012/013/014、GIT-003/004。ADR0037の列挙共有とADR0082の固定harnessを使用。waiver none。公開API、設定schema、速度基準/許容、fixture期待値、抑制/allowlistは不変。

### 対象と正しさ

- #346：ADR0082 stage4をf09289dで先行決定し、a93e0e6で固定17処理を追加。既存24処理は37項目の静的一致で保持を確認。Debug/Releaseの追加17smokeは各17成功、Pythonの新定義1試験成功。準備・期待生成・全結果照合・破棄は区間外、実呼出しと結果保持だけがTimingPortの2marks内。保存のFilePort内部copyは計測へ含む。実機I/O時間の測定ではない。
- #347：5ee45bdでprivate encodedがtext()の生成stringを所有として受け、UTF8は移管、BOMは同じ値の先頭へ追加。CP932の同期借用と外部raw防御copyは不変。`nib_tests --file-save`38 / `--ex-document`206 / `--ex-write-path`90、計334checks成功。
- #348 B1：812c835でcollectを既存visit_text_rangeへ集め、空範囲と終端で停止。reserveや集計融合は含めない。`nib_tests --buffer-range`235checks成功。head/tail/断片・改行交差/逆順/本文外/全削除と旧snapshotを直接確認。
- #349：設計fdce98cの後de51fb9で、行頭からの非重複一致列挙をprivateな停止可能処理へ集めた。first/lastと全件強調が同じ列を使い、caretから直接照合しない。`--vim-search`1374 / `--vim-search-highlight`76 / `--vim-search-incremental`141、計1591checks成功。
- 各製品差分の通常Debug/tidy/ASan/UBSan、関連source規約/File API/symbolsは成功。独立read-only reviewは製品3件とharnessを別作者の席で実施し、受理を止める所見なし。自己実装を独立認定した扱いにしない。

### 固定比較と結果

`compare-wave3.py batch-347-348b1-349 --execute`が既存`eng/compare-probes.py`へ固定の各20反復×ABBA3block（12process、120対応組）を渡した。warmup各1は区間外。全席のbuild/test/GUI/重いcopyを止め、前後環境を記録。13条件すべてobserved、欠測/外側retryなし。全sample・metadata・marks・完全checksum・exe SHAを保持する。beforeは同じ41処理harnessのa93e0e6、afterは各製品へ同じharnessを取り込んだbd507e9/a2d534d/d20602d。比は対応組after/beforeの中央値で、列の時間は全sampleの中央値なので両者の商とは限らない。

| 固定処理 | before µs | after µs | 対応比中央値 | 短縮した組/120 |
| --- | ---: | ---: | ---: | ---: |
| UTF8保存16.8MB | 8354.5 | 5197.5 | 0.625208 | 118 |
| UTF8 BOM保存16.8MB | 9298.5 | 6150.0 | 0.657926 | 116 |
| 8193断片・先頭erase16回 | 2639.5 | 2659.5 | 1.001508 | 58 |
| 同・中央erase16回 | 2892.5 | 2700.0 | 0.954840 | 86 |
| 同・末尾erase16回 | 2678.5 | 2786.0 | 1.052850 | 38 |
| 同・全文erase16回 | 224.0 | 2.0 | 0.008316 | 120 |
| 1断片・中央erase16回 | 4.0 | 4.0 | 1.000000 | 同値主体 |
| 前方検索・先頭 | 6432.0 | 56.0 | 0.008857 | 120 |
| 前方検索・中央 | 7297.0 | 3288.5 | 0.454514 | 120 |
| 前方検索・末尾 | 13948.0 | 6202.5 | 0.445879 | 120 |
| 後方検索・先頭 | 12975.0 | 5243.0 | 0.407485 | 120 |
| 後方検索・中央 | 7272.0 | 3317.5 | 0.458164 | 120 |
| 後方検索・末尾 | 8152.0 | 6595.5 | 0.806687 | 120 |

保存2条件と検索6条件で短縮を観測。B1は全文削除の不要走査を除く一方、末尾条件は約5%遅い（16回の中央値差107.5µs、1回あたり約6.7µs）。この代償を消さず採否へ含める。先頭と1断片に改善は主張しない。全文削除のafter1〜2µsは分解能に近く、桁数の多い改善率を保証しない。全値の分布・範囲はsummary.jsonと各comparison JSONが正本。BOMの再確保可能性、piece列生成/集計、行所有copyは残り、RSS/ゼロcopy/全入力の速度を保証しない。

### 証拠と残る受理

原記録は `D:/NeNeNib/outputs/20261010-speed-stability/`、harness `outputs/20261010-probes/`、実装 `outputs/20261010-save-path/`・`outputs/20261010-buffer-paths/`・`outputs/20261010-search-enumeration/`、独立レビュー `outputs/20261010-review-wave3/`。恒久先は `D:/NeNeNib/evidence/speed-optimizations-20261010-wave3/` へ元path/SHA対応付きで収載する。収載前の追加作業木は保持する。#348Aのoffsetと#350照合器は別の入力/commit/計測/受理であり、この13条件の成功に混ぜない。

## 5-de — 第三陣3件の統合と直接実機確認（#351・進行中）

統合sourceは`c907c7a3bc312ef57f0c9aa81909a6b8b2ff01b7`。変更した製品は#347/#348B1/#349だけ。`351-reuse-identity.json`で各成功時の全変更source/testと共通toolchain/flags/規約/referenceの14項目が一致。保存は本文を読むだけでcollectを呼ばず、検索移動も本文を編集しないため、部品の334/235/1591checksをQLT-012により再利用する。統合した操作の結合は下記の実機で確認した。

- `pwsh -NoProfile -File eng/build-release.ps1 -Ref refactor/351-speed-integration`：通常Release113steps成功。`out/release/c907c7a.json`、exe1445888bytes、SHA256 `057E588D33C9F2391CD996773A74A6AF412F062A0B9A1BB02D9340FCE61E9A0B`。製品flags/依存は変更なし。
- `python eng/protected-diff.py --base d692c2e --head c907c7a`：fixture1853不変、protected filesnone、終了0。selectorはfile-saveを追加して44。exeを渡していない43既存scopeは未測であり、全件同数を主張しない。
- `verify-351-boundaries.py --label before/after --executable <固定exe>` と `--compare`：旧製品2993bb4と統合製品を同じprofile設定/120dpi/1280×800で比較。通常編集、UTF8/UTF8 BOM/CP932の:w実保存、非重複検索の次/前/折返し、a*入力中preview/取消の18場面が本文/ステータス0画素差。両版の3保存は独立expectedと全byte一致、6process正常終了、settings不変。7操作遷移に非0画素変化を確認し、届かなかった刺激を一致の根拠にしていない。画像も直接目視した。
- GUIの初回はCtrl+S後0.5秒の確認で旧版の保存が未反映、保存入口を:wへ切り替えた2回目は0.5秒の読取で共有拒否。元script/画面/文書/失敗recordを保持。v2からv3では本文/期待/製品/操作を保ち、保存完了と共有解除を最大15秒待つ同期へ直して上記成功。Ctrl+Sの未反映原因を特定したとはしない。待ち時間は性能値に転用しない。
- computer-useの@oai/skyはnative pipe不在で接続できず、再初期化後も同じだった。入力操作は行われず、既存window_driver/verify-windowの専用CLIを使用。失敗を隠してcomputer-use成功とは記さない。
- 正式QLT-014はcollectを通る空文書1打鍵/200打鍵/16MiB200打鍵/長い日本語行1打鍵の4条件を選び、`formal-351-plan.json`に測定前固定。起動/open/paletteの刺激は変更した関数を呼ばず、QLT-001により繰り返さない。基準値/25%許容/2ms床を変えず各5試行の正規`eng/measure-speed.py --check --bench`で判定する。正式4条件は各5試行で全件基準内、0退行/0計測不能。中央値は空文書1打鍵0.646ms、200打鍵2.073ms、16MiB200打鍵2.248ms、長い日本語行1打鍵5.950ms。指紋bc8a356f37c68491の既存基準で判定し、参考記録だけの成功とは区別した。CI・main統合・恒久収載は継続中。全試料はformal-351各logと4つの正規out/speed JSONへ保持。既存runnerが繰り返し上書きするmarksは各bench終了時にsnapshotを取り、全試行のmarksを保持したとは主張しない。

配置/失敗/探索誤り/実行順の詳細はjournal.md。worktree checkout完了待ち前のcherry-pick拒否も保持し、clean確認後に残りと未適用設計を取り込んだ。元の事前設計commitは実装より前で不変。採用済み・整理済みとするのはD41と収載/監査の完了後とする。

- 恒久証跡を`D:/NeNeNib/evidence/speed-optimizations-20261010-wave3/snapshot-1005/`へ収載し、997file/265,043,842bytesのコピー元/先SHA256一致を確認。manifest SHA256 `a8f4bdf49326557f43dcc138ed32429e738c69f2677976c08b766a504a48296b`。`files/`以下はD:/NeNeNibからの元pathを保持し、source/には6枝のHEADを保存。348A/350は未受理実験の時点snapshotとして含む。B1の古いtest exeは上書き済みであり、現存AのexeをB1のものと表示しない。再生成可能なobject/cacheは収載対象外。PR #352のCI後にmainへ統合し、完了した347/349/351の作業木は未保存/未追跡/無視file・リンク・稼働参照と取込を監査してから整理する。枝とcommitは保持する。

## 5-df — 桁位置と照合器の固定比較（#348A / #350）

規則ARC-001/003/005/007/008/009/012、CPP-002/004/005/008/012/014/016、QLT-001/008/012/013/014、D41。ADR0094を先行してoffsetを共通visitorの停止へ移し、ADR0093を先行して照合の再帰をprivate優先状態列へ移した。原記録は`D:/NeNeNib/outputs/20261010-speed-stability/`、有限比較は`outputs/20261010-pattern-design/`。基準値/fixture/schema/抑制/allowlist不変、waiver none。

### 桁位置の比較

`python compare-offset-a.py`の事前planで20iterations・3 ABBA blocks・warmup1区間外・120対応組を固定。各区間は128回の同じ変換と全結果照合、通常Release。B1時点a2d534dとA追加2e719acの比較であり、B1のraw/exeを上書きしない。全4条件observed、外側retry/除外なし。

| 条件（57344bytes） | before中央値µs | after中央値µs | 対応比中央値 | 短縮組/120 |
| --- | ---: | ---: | ---: | ---: |
| 長行先頭 | 155.5 | 1 | 0.006667 | 120 |
| 長行中央 | 3328.5 | 3118 | 0.933786 | 111 |
| 長行末尾 | 6516 | 6272.5 | 0.964706 | 108 |
| 8193断片中央 | 8968 | 5719.5 | 0.636695 | 120 |

先頭after1〜3µsは分解能近傍。中央と末尾の全試料改善は主張しない。初回nesting4の拒否をflattenし、symbolsの未初期化失敗は正規toolchainを初期化して確認した。buffer-range282checksと規約/symbols成功。独立`review-348-a.md`は受理阻害なし。要求容量やRSS改善の計測は行っていない。

### 照合器の初期比較と失敗

同じ48workloadのharness（追加7はADR0082で先行固定）を旧製品とcd972c8候補へ適用。`python compare-pattern-350.py`は5iterations・3 ABBA・30対応組、1区間16matchedを事前固定した。正常6条件は全観測。

| 条件 | before中央値µs | 初期候補中央値µs | 対応比中央値 | 短縮組/30 |
| --- | ---: | ---: | ---: | ---: |
| a*b不一致1024 | 168273 | 505 | 0.003012 | 30 |
| a*b不一致2048 | 654599.5 | 1007 | 0.001538 | 30 |
| a*b不一致4096 | 2558359 | 2012 | 0.000787 | 30 |
| a*a*a*a*b不一致32 | 352023 | 40 | 0.000112 | 30 |
| a.*a成功4096 | 1282.5 | 2066 | 1.610136 | 0 |
| literal末尾4102 | 622 | 458 | 0.734727 | 29 |

旧4096literalは最初のDebug smokeでASan stack-overflow、Release smokeで0xC00000FD。いずれもwarmup中でmetadata/marksなし。既知失敗を再試行せず比較の7本目は欠測、比率は作らない。候補単独は同じ4096literalの完全照合/marks/正常終了を確認。harness初回のnesting4拒否とgenerator抽出誤りを残し、表/期待/flagsを変えず補助関数の境界を直した。製品初回のnesting/optional診断と修正も保持。最終3検索scope1606checks成功。

### 同位置消費の共有と採否

初期greedyの悪化を受け、ADR0093追加決定ea09079を先にcommitし、0bd669fで消費判定を共用した。優先順/4vector/意味は不変。同位置の文字取得/前進を一度にする。`python compare-shared-consumption.py`は20iterations・3 ABBA・120対応組、16matched/区間を事前固定。正常な初期候補と最終候補の7条件に加え、旧版対最終版のgreedyだけを直接比較した。全8observed/外側retryなし。

| 条件 | 初期中央値µs | 最終中央値µs | 対応比中央値 | 短縮組/120 |
| --- | ---: | ---: | ---: | ---: |
| a*b不一致1024 | 505 | 396 | 0.783810 | 120 |
| a*b不一致2048 | 1008 | 787 | 0.780318 | 120 |
| a*b不一致4096 | 2011 | 1572 | 0.781701 | 116 |
| 複数star不一致32 | 40 | 31 | 0.756757 | 120 |
| greedy成功4096 | 2059 | 1632 | 0.792031 | 120 |
| literal末尾4102 | 458 | 458 | 1.000000 | 50（同値25/遅い45） |
| literal4096 | 377 | 377 | 1.000000 | 23（同値41/遅い56） |

別の直接比較の旧版→最終版greedyは1212→1633µs、対応比1.350581（範囲1.003688〜2.111511）、全120組で遅い。中央値差421µs/16=26.3125µs/照合は算術値でGUI応答の実測ではない。初期悪化61%を隠さず、異なるbatchを合成して改善率を作らない。再帰と組合せ探索を除く安定性、原子数に閉じた保持量を優先し、greedy約35%の代償込みで採用する。全条件高速化・平均/体感無影響・RSS改善は主張しない。

共用消費後のDebug検索3scope1606checks、source規約と完成core.libのsymbols成功。最初にobjectを渡したsymbolsで内部6参照が未宣言扱いとなったrawも保持し、allowlistは変更しない。新Release probe112steps成功、exe SHA256 `22AB4579BDA49379DA20C2CCC6CD893D1742581375E40D6087316A3D809F9BD4`。公開APIの有限C++は同じ14token/短本文/anchor/from系列1,382,940行、23,202,990bytesを旧版・初期版・共用版で全byte一致確認。SHA256 `8b4e57d670b3c41a636ae3ce4c0e028e1a7bf1d37e47645481b33dbea4f8f7c1`。`finite-cpp-shared-comparison.json`、build/run scriptと初回compile/link指定失敗も保存。これは有限範囲のC++比較でVim oracleや任意入力の証明ではない。UTF8検証済本文/codepoint先頭の既存前提外へ保証を拡張しない。

## 5-dg — 桁位置と照合器の統合受理（#353）

先行#351は[PR #352](https://github.com/hideyukiMORI/nene-nib/pull/352)で10:05:23 JSTに必須CI成功後squash merge。main `d5ef4ee3ed28b002d61d8b8a6aabcd90cc8bdeb8`へ同期し、347/349/351の3作業木は証拠収載/監査後に削除、枝とcommitは保持。受理JSON/CI/clean監査は恒久先`acceptance-351/`。5-deの進行中記載は当時の時点で、現在の状態は本節を優先する。

#353は#348Aと#350最終差分をd5ef4eeへ統合したsource `b8fd7cb9575a33a31aa06eb93749dbac75f16559`。VimSearchはmainと同一、matcher4filesは0bd669fと同一。Aの共有visitor、#349列挙と新照合器が同居するため、旧単独試験をそのまま使わず次の直接5scopeを一度確認した。

- `pwsh -NoProfile -File D:/NeNeNib/outputs/20261010-speed-stability/verify-353.ps1 -ExpectedHead b8fd7cb9575a33a31aa06eb93749dbac75f16559`：通常Debug/tidy/ASan/UBSan123steps、buffer-range282、ordinary-characters233、vim-search1409、vim-search-highlight76、vim-search-incremental141、計2141checks成功。ordinary-charactersは上下/ページ移動と編集callerを含む。対象source12と実CMake File APIの規約違反0、完成core/applicationのsymbols2libraries違反0。protected-diff base d5ef4ee/head b8fd7cbはfixture1853不変/protected filesnone、44scope全件は未測。出力はverify-353/と作業木out/protected。
- `pwsh -NoProfile -File eng/build-release.ps1 -Ref refactor/353-position-pattern-integration`：通常Release114steps成功、`out/release/b8fd7cb.json`。exe1448960bytes、SHA256 `73711B91ED238647F1B9A89B283E7C70AE7606149500E204839070B0E9680CEF`。buildの並行状況を速度比較に使わない。
- `python verify-353-boundaries.py --label before/after --executable <固定exe>`と`--compare`：先行351の保存済み製品と統合製品で、短/長行の上下移動、断片編集、非重複n/N、greedy、文字集合star、語境界、preview取消、空行、日本語後方検索、Vim縦移動の19場面を本文/ステータス0画素差で確認。7刺激遷移は非0、前後2process正常終了。候補単独4096文字語の*→n→Nを5場面で確認し正常終了。画像を目視し、元文書/profile不変。保存機能は今回の範囲外。
- GUI初回の12場面後にfixture鍵parserが語境界記法を拒否した。製品失敗とはせずscript/rawを保持し、v2はその入力だけwrite_text+Enterへ変更。旧版から全19場面を確認し直した。速度値へ転用しない。
- `python run-formal-353.py`：Aが変えた共通walkerを通る4編集条件をformal-353-plan.jsonに先行固定。4編集条件で、空/短い反復/大文書/長い行が共有するwalkerとframe生成を直接確認する。起動/openにもframeの共有経路はあるが、新しい持続状態はなく、別処理を多く含む4条件を追加する必要はないと判断した。正規`eng/measure-speed.py --check --bench`の各5sample、同じ指紋bc8a356f37c68491の実reference/25%許容/2ms床で全件終了0、0退行/0計測不能。中央値は1打鍵0.623ms、200打鍵2.104ms、16MiB200打鍵2.198ms、長い日本語行1打鍵5.435ms。raw全sample/log/終了時marksをformal-353へ保持し、全試行marks保存とはしない。この正式刺激でmatcherは測らず、別の局所比較と直接実機で確認する。
- 計測前後の環境JSON、事前固定plan、source/exe/reference/scriptのhashを保存。同機i9-10850K/RTX3090/120dpi、HP推奨plan不変、build/test/GUI/重いcopy/hashを止め他の重いprocess0。すべての試料/初回失敗を保持し、成功までの測り直しなし。
- 別作者のread-only独立レビューはA、#350初期、tradeoff、共用消費後と統合を確認しP0/P1/P2なし。`outputs/20261010-review-wave3/`に全報告。共用後のレビューは自身のA著作を除き、Aは既存の別作者レビューを使用。製品検証・正式4結果は関連source/test/toolchain/referenceが不変の文書更新/push/review/mergeで再実行しない。

残る限界はgreedy直接比較約35%悪化、有限入力の同値確認、原子数×本文長の仕事量、RSS未測。4vector要求はA/A/A+1/A+1で実capacityを測定したとはしない。schema/速度基準/fixture期待/抑制/allowlist変更なし、waiver none。恒久収載とCI/main反映は後続の受理記録へ追記する。

- #353恒久snapshotを`D:/NeNeNib/evidence/speed-optimizations-20261010-wave3/snapshot-353/`へ収載。1248file/285,653,640bytesのコピー元/先SHA一致、manifest SHA256 `72c90e21046512e2dc7c0786c0497e6ddd10a559210f3f1dcc64350cd86c78b7`。4枝のsource、原出力、前後probe、統合製品/Debug試験exe、全失敗/script/reviewを保持。消した347/349の旧exeは先行snapshot-1005のhash一致を確認し参照する。後続の文書/CI/main/整理はacceptance-353へ追補する。


### #353のmain反映と整理（10:37 JST確定）

[PR #354](https://github.com/hideyukiMORI/nene-nib/pull/354)は10:37:03 JSTにsquash merge、main `163c31434777aa9f0817dcf6238e2301e8a8553b`へff同期した。受理HEAD fcbe9a44567513a99a5fe31b731492778974466cとmainの全tree一致/cleanを確認。必須checkは[run38013811414](https://github.com/hideyukiMORI/nene-nib/actions/runs/38013811414)成功。初回run38013766761はPR titleのtype perfがGIT-003の規定外で拒否され、refactorへ直した編集イベントで成功した。製品/試験/ゲートは変更せず、初回logも保持。

恒久先`acceptance-353/`へPR/CI/最終main source/文書/枝別source一致/監査/整理JSONを追補した。346/348/350/353の4作業木は絶対Dパス、取込、clean/未追跡無し、links無し、稼働参照無しを確認。ignoredは813/460/596/321件、うちout196/26/30/13件は全archive hash一致。固定exe/sourceを収載した後のbuild/cacheは再生成可能と分類。初回監査は346のbuild-releaseが未分類で停止し、削除せず生成物を確認後に最終監査した。4worktreeを削除し全枝/commit保持を確認。結果はcleanup-result.json/worktrees-after.txt。後続の355/356だけは新しい未完了taskとしてDに作成した。

正式4条件の範囲説明を訂正した。原planの「起動/open/paletteの刺激は変更関数を呼ばない」は共有walkerからframeへの経路まで含めると厳密でない。4編集条件が同じ共有経路の直接境界を覆うという根拠で選定する。元plan/全sample/結果は改変せず、同じ実装への工程理由の再試験も行っていない。

## 5-dh — 行内の選択・検索spanの投影（#355 / 固定比較器 #356）

main163c314で残る一致端点ごとの行頭prefix計数を、private `LineSpanEvaluation`一つへ移した。ADR0095 d7a136bと固定5条件のADR0082追記1dc3e55/独立期待JSONを実装前に固定。製品d368ae2、harness2bcaa30を統合したclean fdae17da073ecac7c6a63f734fcd959e15153fb8が実測source。行本文をLineViewへmoveした後に同期借用し、既存code_point_countで端点の差分を前進計数、後戻りは行頭へresetする。選択を先に、同じinstanceで既存非重複一致を投影する。旧span_ofを除去し、clip/absent/改行+1の意味は維持。+1は返り値だけで累積桁へ混入しない。renderer/UTF8/matcher/公開API/永続状態は変更しない。

規則 ARC-001/003/004/005/008/011/012、CPP-003/005/008/012/014/016、QLT-001/008/012/013/014、GIT-003/004・D41。waiver none。変更はapplication helper/line_view、直接unit selector/境界契約、既存perf targetの5入力とそのPython定義、対応文書だけ。schema変更なし。

### 対象検証と再利用

- `cmake -S . -B build/355-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug`と`cmake --build build/355-debug --target nib_tests --parallel 2`：正規toolchain/tidy/ASan/UBSanの初回124steps成功。`nib_tests.exe --frame-selection`56、`--vim-search-highlight`83、`--vim-search-incremental`141、`--vim-visual-block`1105、計1385checksが初回成功。clip/空行/CRLF/LF/末尾CR、原文と表示の桁、後方選択から前方の一致へのreset、preview/currentと矩形callerを直接確認。無関係な全回帰は実施しない。
- `python implementation/scoped-static.py`：変更8source/文書/waiver/実CMake File APIの規約違反0。`python eng/symbols.py --build-dir build/355-debug --require core application`：完成2libraries違反0。変更8C++のclang-format dry-run/Werrorと差分空白が成功。既存5関数のassert本文を逐語保持し、薄いbundleを既定から一度だけ呼ぶ監査が成功。旧44selectorは同じ入口を保持し、新frame-selectionで45へ増加した。
- `python -m unittest tests.conformance.test_compare_probes.ProbeComparisonTests.test_stage_six_frame_inputs`：新5固定入力の1test成功。old48監査は既存enum/name/registry/旧probe全文/main全文/Python入力prefixの一致。新5は正規DebugとRelease before/Release afterで各1sample+warmupの完全結果/独立checksum/metadata/marksを照合し全成功。旧48のruntimeを回し直したとはしない。
- #356初回Debugはdisplay_equals認知複雑度12>10を拒否。固定starts算術をprivate関数へ分けて2bcaa30、期待/区間/checksumを変更せず通常Debug増分10steps/Release113steps成功。旧48監査の初回はbodyのbraces込み60とinterior58の計数差で失敗し、監査だけを訂正。両初回logを保持し規則/flags/抑制は変えない。
- `pwsh -NoProfile -File eng/build-release.ps1 -Ref refactor/355-line-span-projection`：通常製品Release115steps成功。exe1448960bytes、SHA256 `ffeed056755897ddfa2e83a0178ec6d16a6163da0e552b4f7896e1f2868f1b41`。同じbuild/release-fdae17dへ`cmake --build ... --target nib_perf_probes --parallel 2`の追加10stepsだけで比較器を作った。製品library/configurationを共用し、別flagsの製品を作っていない。probe before SHA256 `7a8c7034e63a42a58670c9b9241d2abc58f3464d69604042bd49e1374ea7c5d2`、after `41e386b2f7669cd2d7f0e7b69e923b16229fcecc268f5bc8bf86aac985685131`。
- `python verify-355-boundaries.py --label before/after --executable <fixed exe>`と`--compare`：19場面の本文/ステータス0画素差、13刺激遷移は非0、正常終了/文書とprofile不変。通常跨行選択/各改行/逆向きVISUAL/矩形/検索preview/hlsearch切替/空行を確認し画像も目視。初回v1ではnative CtrlVとpost文字列の同期が混じって矩形の刺激が不成立。全初回19画像/scriptを保持し、v2でその入力同期だけ分け、前後19場面を確認した。待機時間を速度値に使わない。
- `python eng/protected-diff.py --base 163c31434777aa9f0817dcf6238e2301e8a8553b --head fdae17da073ecac7c6a63f734fcd959e15153fb8`：exit0、fixture1853→1853、metadata/deleted/changed/added0、protected files none。out/protected/fdae17d.json。旧44scopeの全checks実数は未測、新frame-selectionもこの保護比較では未測（直接56checksとは別記録）。全scope実数不変は主張しない。
- 独立製品reviewは借用寿命/旧式/後戻り/+1/既存assertと実ログを確認しP0/P1/P2なし。設計席の別作者harness reviewも固定期待/実測区間/初回修正を確認し阻害なし。`source-identity.json`でd368ae2→fdaeのsrc/unit/support/依存/flags不変、CMake差分はperf source追加だけ、前後harness全Git objects一致を保存。文書/push/review/mergeの工程変更では1385/GUI/正式速度を繰り返さない。

### 固定5条件の比較と採否

`python compare-355.py`が正規`eng/compare-probes.py`を一条件一度だけ呼ぶ。各frame()一回、20iterations×3ABBA、片側120sample/120対応組、process毎warmup1は区間外、timeout180秒を先行planへ固定。新5の各完全出力を固定算術で照合し、準備/検査/checksum/破棄は区間外。全5observed、欠測/外側retry/試料除去なし。

| 条件 | before中央値µs | after中央値µs | 対応比中央値 | 短縮/同値/遅い組（全120） |
| --- | ---: | ---: | ---: | ---: |
| dense ASCII 8192一致 | 112165 | 931.5 | 0.008294700 | 120/0/0 |
| dense混合 4096一致 | 77143 | 690.5 | 0.008950449 | 120/0/0 |
| sparse末尾 1一致 | 489.5 | 470.5 | 0.960045772 | 107/1/12 |
| 通常選択のみ | 241 | 235 | 0.970649766 | 91/2/27 |
| dense検索＋VISUAL | 111932 | 938.5 | 0.008396492 | 120/0/0 |

対応比の範囲は順に0.007720671〜0.011910845、0.008394531〜0.011328202、0.663841808〜1.392197125、0.660919540〜1.070247934、0.008002316〜0.010862597。dense3条件は全120組で短縮。疎/通常選択は中央値約4%/3%短縮だが遅い組も残り、小差を全入力/体感の保証にしない。描画や入力応答全体、RSS/capacityの改善量は未測。配列/共有cache/永続状態を増やさず、固定個数の借用位置だけで同じ原始を使う設計、完全結果一致と直接検証を合わせて採用する。

`python run-formal-355.py`は事前formal-355-plan.jsonの編集4条件を正規`eng/measure-speed.py --check --executable <fixed> --bench <name> --repetitions 5`で実行。空/短い反復/16MiB/長い日本語行がline_viewの共通frame生成と直接編集callerを覆う。起動/openにもframe共用があるが新しい持続状態はなく、別の起動/I/O/一覧処理を含む追加4本を繰り返す必要はない。検索/選択の固有費用は固定5/GUIで別確認した。

同じ指紋bc8a356f37c68491/実reference/25%許容/2ms床で4本とも終了0、各1bench判定/0退行/0計測不能。中央値msはsingle0.632、burst200 2.095、16MiBburst200 2.254、long-line5.764。全sample JSON/log/利用可能な最終marksを保存、全試行marksの保存とはしない。環境前後i9-10850K/RTX3090/120dpi/HP推奨電源、重いprocess0、他席のbuild/test/static/GUI/重いcopy/hash停止。基準値や許容を変更していない。

### 収載と残る作業

恒久先`D:/NeNeNib/evidence/speed-optimizations-20261010-wave3/snapshot-355/`へ464file/109,888,672bytesを元先SHA照合して収載。manifest SHA256 `ca6a47f3c00c986a23a511fb16596955199e199c56ffba2aa90157b421b1516a`。全原出力/初回失敗/script/レビュー/前後probe/製品とDebug exe/2枝source/実build設定を保持。旧製品GUI baselineはsnapshot-353に保持。後続のprotected/最終文書/CI/main/削除前監査は`acceptance-355/`へ追補する。

技術受理時点ではCI/main反映と2作業木の整理が残る。枝/commitを保持し、main一致、絶対Dパス、未保存/未追跡/ignored/唯一成果物/稼働参照/リンクを確認してから削除する。今後の候補はrendererのUTF16端点反復と場所検索の連結であり、今回の採用へ混ぜない。

### #355 / #356のmain反映と整理（11:15 JST）

[PR #357](https://github.com/hideyukiMORI/nene-nib/pull/357)は[必須CI run38016069217](https://github.com/hideyukiMORI/nene-nib/actions/runs/38016069217)成功後、11:12:58 JSTにsquash merge。main1aff4150d9c0a9b794d01cf3020b8cc8f946936fへff同期し、受理HEAD1d1fa01dfac8bc429a35572a7ad643b14bae481bと全tree一致、本体cleanを確認。#355/356はclosed。製品/検証内容はfdae17dから不変で、工程理由の再試験をしていない。

acceptance-355へPR/CI/最終main source/後続protectedと文書/統合監査を追補。355/356の2worktreeは絶対Dパス、clean/未追跡無し、links/稼働参照無しを監査。ignored357/371件中out37/55件は全archive hash一致。固定source/製品/probe/Debug exe/build設定を保持し、生成cacheを分類して2worktreeを削除、枝/commit保持を確認した。不要となった一時出力297file、archive結果JSON、依頼書2本も全て元先SHA/稼働参照/linksを確認して削除。恒久snapshot-355とacceptance-355が正本で、以前の作業パスは存在しない。

文書生成script初回はPython encoding名utf8-sigの誤りで本文変更前に停止し、utf-8-sigへ訂正した。統合監査初回のADR0082 pathはclosed-probesという不在名で空diffだったため、初期JSONを保存し、正しいscoped-probesを両commitに存在確認した上で一致を確認した。いずれも製品・試料を変更/再実行していない。全監査/整理JSONとscriptはacceptance-355へ保持。

## 5-di — 場所照合は文字列を連結せず既知の区間を借りる（Issue #358・ADR 0096）

main1aff415で`CommandChoice::listed_score`が名前不一致のたびに作っていたdetail＋区切り＋labelの所有stringを除いた。先行ADR0096 e352780、実行前の2scopeへの限定058af42、初回製品08116c3、初回の代償と固定区間数の追試設計fddb89aを経て、最終製品333086cf2f441f592b7cc0ff015080129f78e5fcを技術受理する。規則ARC-001/003/004/005/008/012、CPP-003/005/008/012/016、QLT-001/008/012/013/014、GIT-003/004・D41。waiver none。

完結したUTF8文字列を1/3個のstack arrayで同期借用し、`std::span<const std::string_view, Count>`を受ける一つのprivate template scorerへ渡す。区間数だけを型で保持し、特殊化/Count分岐/別の意味経路は作らない。scalar走査は既存関数の本文を維持し、区間内fromを写してglobal begin/endへ戻す。byte長/gap/初gap4倍/ASCII空白/大小同一視、名前優先、場所罰点、同点元順は不変。公開候補の防御copy/所有、subset再利用条件、harness53、renderer/IME、schema/fixture/基準/allowlist/抑制を変更しない。

### 直接検証と初回記録

- 初回は`cmake -S . -B build/358-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug`、`cmake --build build/358-debug --target nib_tests --parallel 2`の正規Debug124steps成功。`nib_tests.exe --command-palette`495、`--operations`516、計1011checksが初回成功、stderr空。Unicode3区間横断/byte gap/初gap4倍の独立期待3assertだけ追加し、除けば旧test全文と同一。既存scalar本文も同一。Ex候補の共通scorerはcommand-palette内で確認し、scorerを通らないex-settingsは実行前に除いた。
- 第2候補は同じDebugの増分3stepsと同じ2scope1011checksが初回成功。指定の2template/2span型/2callと整形以外のsource tokenは初回と一致、全test/public/CMake/flags/harness不変。型と製品sourceが変わったため実行結果は第2候補を使う。元の試験を工程変更だけで反復したものではない。
- 両候補とも通常clang-cl19.1.5/C++23/MT/W4/WX/tidy/ASan/UBSan/no-recover。変更sourceの`clang-format --dry-run --Werror`、canonical source/document/waiver checks、`git diff --check`、完成coreのsymbols1library/0違反が成功。初回protectedは1853fixture差分0/protected files none、45selectorsの全実数はexe無しで未測。無関係な全回帰/coverage/全gateは実行しない。
- `pwsh -NoProfile -File eng/build-release.ps1 -Ref refactor/358-palette-segments`を各clean sourceで初回実行し、両115steps成功。同じRelease buildへ`cmake --build ... --target nib_perf_probes --parallel 2`を追加10stepsだけ実行し成功。初回製品SHA `d7709837add2894e2a6196bade05d20af476c8b2b80a3ab90915bc80e59054df`、最終製品SHA `bdb24ff17b458fbf7863516de434cdaf2855e8cc5e634b5a927c9c9462f5f6ab`。最終1448960bytes、probe SHA `4fd1724e05f682c2f4e38b05bd9e33fdba353a2a95f6976d40beb582c8e3d13b`/1343488bytes。
- 旧probeはsnapshot-355のfdae17d/SHA `41e386b2f7669cd2d7f0e7b69e923b16229fcecc268f5bc8bf86aac985685131`を再利用。source-identityで各pathの両commitへの存在を確かめ、performance全tree/flags/referenceを同一、製品差分をCommandChoice.cppだけと確認。各版の正しさsmoke3はcanonical metadata/全5000位置または50行/input/caret/scopeと独立checksumを照合し全成功。smokeの時間を性能採否に使わない。
- `python verify-358-palette.py --label before/after --executable <fixed>`と`--compare`で、各候補17場面の本文/statusは旧版と0画素差、16刺激遷移は非0。名前/場所/名前優先/ASCII/跨separator補助平面文字/空白/不一致/Ex候補/操作キー・読み・F1を確認。正常終了、9文書/設定不変。WM_CHARはUTF16単位に分けて補助平面文字を送り、製品driverを変えない。第2候補は同じ絶対文書path/内容/操作/設定/DPI/寸法の旧版画像をSHA同一で再利用し、候補側だけ実行。親が絵文字の1件とF1ほぞん2件を目視した。
- 独立設計/初回製品/第2候補差分reviewはいずれもP0/P1/P2なし。実装者と別席がscalar/座標/score/所有/旧assertと実logを確認。第2候補は初回意味reviewを再利用し、型と新1011結果を追加確認。成功結果は関連source/test/依存/環境が不変なら文書/push/review/mergeで再実行しない。

読取時の推測filename不在、親の準備script内で実行前に訂正したharness path、作者manifest準備のJS構文失敗2件はjournal/read-failures/tool-failuresに残した。製品compile/runtime失敗や計測の再試行とは区別する。両候補の製品build/test・GUI・固定比較は初回成功、試料の除去や期待の緩和はない。

### 固定3比較と代償

各候補は事前planの同じ3条件を`python compare-358.py`から正規`eng/compare-probes.py`で一度ずつ実行。20iterations×3ABBA/片側120sample・120対応組、processごとのwarmup1は区間外、timeout180秒。名前/場所はlisted_positions一回、末尾絞込はqx→qxyのinserted一回。準備/全結果確認/checksum/破棄は区間外。全observed、欠測/外側retry/試料除去なし。

| 候補・条件 | before中央値µs | after中央値µs | 対応比中央値 | 短縮/同値/遅い組（全120） |
| --- | ---: | ---: | ---: | ---: |
| 初回・名前5000 | 332 | 363.5 | 1.089419127 | 14/0/106 |
| 初回・場所5000 | 1348 | 963 | 0.710424595 | 119/0/1 |
| 初回・末尾絞込5000→50 | 38 | 40 | 1.054054054 | 7/2/111 |
| 固定Count・名前5000 | 347 | 325.5 | 0.928876560 | 94/0/26 |
| 固定Count・場所5000 | 1348.5 | 893 | 0.657614200 | 120/0/0 |
| 固定Count・末尾絞込5000→50 | 37 | 39 | 1.054054054 | 6/2/112 |

初回の比範囲は名前0.771167048〜1.994428969、場所0.573700306〜1.002259036、絞込0.487804878〜1.605263158。最終は順に0.531147541〜1.710227273、0.348124244〜0.881057269、0.661016949〜2.461538462。全値は各comparisons/JSONに残し、初回と最終という異なる実験の中央値から直接対応比を作らない。

初回は場所利益があっても名前が約9%遅いため採用を保留し、既知の1/3という個数を実行時に消さず型で保つ別設計を先行した。最終は場所が全120組で短縮し中央値差約0.456ms、名前も今回の対応中央値で約7%短縮。一方、末尾絞込は約2µs/5.4%増加が112組に残る。この代償を含めて固定Count案を採用する。名前には遅い26組もあり、小差を全入力の保証にしない。全条件改善/悪化なしとは記載しない。

### 正式速度と受理

各候補の`python run-formal-358.py`は正規`eng/measure-speed.py --check --executable <fixed> --bench key-to-frame-palette-5000 --repetitions 5`を一度実行。変更scorerと直接の入力→frame callerを覆う。本文編集/起動/IO/rendererの実装は不変なので無関係な他7本は追加しない。実指紋bc8a356f37c68491、reference SHA `ce1f2bc26887bebf0f29c5d683422f82586dc912658d1190dd5cf6ec92a4ef43`、許容25%/床2msは不変。

初回は中央値1.984ms/min1.915/max2.208、最終は1.852ms/min1.752/max1.999、各5有効試料・1bench判定/0退行/0計測不能/exit0。最終試料は1.999/1.852/1.752/1.864/1.803ms。各5枚のPNGは同じSHAで、親が暖機fと1/5000を目視。all-sample JSONと利用可能な最終marksを保存し、5試行それぞれのmarksを保持したとはしない。前後i9-10850K/RTX3090/120dpi/HP推奨、重いprocess0、全席build/test/static/GUI/大きいcopy/hashを停止。

hideの今回の全権委任のもと、設計席の実機確認・独立レビュー・固定比較/正式速度を根拠に技術受理。恒久証跡は`D:/NeNeNib/evidence/speed-optimizations-20261010-wave3/snapshot-358/`、初回を含む全raw/両候補source/製品/probe/Debug/画面/レビュー5441file/129075666bytesを元先SHA照合して収載した。manifest SHA `dd89f5c9294fe9a0c45058c4985eca4bd874fd639ab00a1378948ea62b335129`。最終文書/PR/CI/main/削除監査はacceptance-358へ追補する。

残る限界は完結UTF8の私有1/3区間に限定した借用、固定入力外の性能、RSS/実確保回数、候補作成・到着全体。任意byte断片のdecoderではない。rendererのUTF16前方走査は次の調査だけで、今回の採用に混ぜない。CI/main反映後、未保存/未追跡/ignored/唯一成果物/稼働参照/リンク/絶対Dパス/取込を監査して作業木を整理し、branch/commitを保持する。

## 5-dj — 描画のcolumn→UTF16は表示行の端点を順に進む（Issue #360・ADR 0097）

main6ab76d2から先行設計e3764cf、製品ca4ad398d6ddd0c9665b3a9a8a97c2722863af93を採用。規則はADR0097のARC/CPP/QLT各ID、GIT-003/004・D41。私有`LineUtf16Evaluation`を一つ置き、Rendererだけが検証済みDisplayLineを同期借用する。byte/column/UTF16 unitsを同じmove_toで進め、新たに進んだ区間だけ正本next_code_point/utf16_lengthで数える。後退はresetし、0列は1へ、終端超過は実終端へ畳む。

旧byte_of_column/utf16_offsetを削除し、選択/検索/current/caret/置換/IMEのcolumn入力を同じ経路へ統一。検索と置換の順序端点列は一つの局所変換器を共有する。IMEのbyte挿入位置とUTF16 baseは同じ元表示行から得る。既存byte→UTF16/逆変換/表示写像の関数本文は逐語一致。所有copy/decoder/cache/公開試験口を追加せず、layout/字形/fallback/描画順/clip/色は変更しない。CMakeは新cppのui target登録だけ。schema/fixture/警告/flags/基準/許容/抑制不変、waiver none。

### 直接検証・再利用と初回失敗

作業根は`D:/NeNeNib/worktrees/360-renderer-positions`、原記録は`D:/NeNeNib/outputs/20261010-renderer-positions`。下記相対script名はこの原記録配下を指す。恒久化後は末尾のsnapshot配下へ同じ構造で保存される。

- `cmake -S <WT> -B <WT>/build -G Ninja -DCMAKE_BUILD_TYPE=Debug`と`cmake --build <WT>/build --target NeNeNib -j 2`は初回116steps成功。正規clang-cl19.1.5/C++23/MT/WX/tidy/ASan/UBSan/no-recoverを実compile commandsで確認。変更4sourceのclang-format、`python -X utf8 <WT>/out/360/focused-conformance.py`（source4/waivers/File API target graph）、`git diff --check`は0違反。source524の指紋を保存した。
- 対象は実Rendererの描画境界なのでnib_windowや無関係な全unit/coverageを実行しない。試験専用public/friendも足さない。`python -X utf8 eng/protected-diff.py --base 6ab76d2 --head ca4ad39`は1853fixturesの変更/追加/削除0、protected files none。45selectorsの全実数は未測。私有型へGUIから渡せない0列/範囲外はコードレビューで確認し、runtime全入力済みとしない。
- `pwsh -NoProfile -File eng/build-release.ps1 -Ref ca4ad398d6ddd0c9665b3a9a8a97c2722863af93`は初回116steps、141.574秒で成功。通常Release1448960bytes/SHA `c866a25c222637fd1f7ed41b68e3ae504dea0ca9603fd9e0fbcf7ffd14f88a65`。Debug17030656bytes/SHA `9d9445da9963b0c5949cda2f7e93b912fe11db17baa20969c3631098ca549d8e`。
- 比較旧版はsnapshot-358の333086c/通常Release/SHA `bdb24ff17b458fbf7863516de434cdaf2855e8cc5e634b5a927c9c9462f5f6ab`。333086c→6ab76d2の製品/test/CMake/eng/flagsは一致。measurement-identity.json（SHA `d35f87c6e2de1fe7e83a9e8540d47d67d60a05ba230240a3153e8e00937645e2`）とpost-measurement-auditで前後のsource5/harness9/canonical7/exe3のSHA不変を確認。文書/push/review/mergeではこの成功を再利用し、工程変更だけの再試験をしない。
- `python verify-360-boundaries.py --label before/debug/after --executable <fixed>`、`verify-360-ime.py`、`verify-360-ime-both-sides.py`を各版で実行し、`--compare`で前後を照合。GUI26と実IME4+4、計34場面の本文/statusは全0画素差。GUI20刺激はすべて差あり。通常/順逆VISUAL/矩形/zero-length/current/Tab/BMP/補助平面/結合文字/制御置換、空行/行末bar/block、入力欄を含む。親は結合block/混在denseと実IMEcomposition/Space画像を目視した。
- 実日本語IMEは既存SendInput経路でNIHONGO→Space→Enter。日/補助文字prefixの挿入位置と、制御置換が前後にある別条件を固定。underlineは0→207→0と0→204→0、Space accent282/285が前後同じ。全文書/設定不変、確定後の未保存破棄・元IME open0へ復帰・正常終了、Debug診断なし。任意IME候補の同値や書記素の完全性は主張しない。
- `python compare-360.py --smoke <label> --before <fixed>`の各版4条件smokeは入力対応と画像復帰の道具確認として成功（各argvはrun-gui/recordに収載）。build並行のsmoke時間を性能評価へ使わない。初回beforeの外部harnessは準備EnterのWM_CHARを数え忘れ、8実mark対7期待で失敗。他3条件はprocess-wide DPI初期化の反復で製品起動前に失敗。原script/rawを保持し、prep4/DPI初回だけへ訂正したbefore-v2を別folderで確認。製品の失敗/試料除外/成功までの再計測ではない。
- 独立設計/道具/製品/受理レビューを保存。製品P0/P1/P2なし。道具P2の暖機frame自動確認不足は独立`validate-360-marks.py`で補い、before/debug/after各8暖機、正式48trialの96暖機と960計測inputすべて次input前frame到達を確認。計測列の到達と値を原marksから照合した。

実装者のsource-evidence初回は親所有ADR indexの許可一覧漏れで失敗し、訂正script/rawを別保存。推測path不在、rgのliteral wildcard、過大な読取出力もjournalに残す。製品build/実GUI/固定比較/正式4の初回失敗はなく、結果を成功へ寄せる閾値変更もない。

### 固定の製品応答比較

`python compare-360.py --before <333086c固定通常Release> --after <ca4ad39固定通常Release>`を事前planで一度実行。4条件それぞれABBA×3、12process×20交互n/N、120対応組。各processは準備4input、暖機nN2input、測定20inputの計26mark。0.4秒間隔。既存`input_received`から次input前の最初の`frame_presented`までを採る。入力/controller/Renderer/Present呼出しまでを含み、GPU完了や変換kernelだけの値ではない。起動/準備/最初の検索を測定値へ含めない。

全48process成功、960計測sample/96暖機を保持。各条件の全3画像を最初のAへ照合し33比較、4条件計132比較は全差0。準備→暖機nは変化あり、nN後は準備と一致。本文/設定/絶対path/DPI120/client1280×800/機械/電源を固定。前後環境はi9-10850K/RTX3090/HP推奨電源（GUID48684d4a-8524-4093-8a63-ea7132b79c1c）、対象build/GUIは停止しheavies空。環境snapshotから全区間の任意外部活動不在へは一般化しない。全試料・遅い組・原marks・rawを保持、欠測/試料除去/外側retryなし。

| 条件（CRLF終端） | before中央値µs | after中央値µs | 対応比中央値 | 比の最小〜最大 | 短縮/同値/遅い（120組） |
| --- | ---: | ---: | ---: | ---: | ---: |
| `a `×4096 | 171614.5 | 18483 | 0.107151734 | 0.097882830〜0.135728519 | 120/0/0 |
| `日a🖋 `×2048 | 164741.5 | 14316.5 | 0.086843481 | 0.074385014〜0.110529236 | 120/0/0 |
| `a\x01 `×1024 | 101766 | 64808 | 0.630091959 | 0.587961356〜0.690603197 | 120/0/0 |
| `a `×32 | 646 | 637 | 0.990460076 | 0.637500000〜1.807812500 | 70/0/50 |

密な3条件の短縮を採用理由にする。通常行の中央値差は9µsと小さく、50組は遅い。全入力/体感改善とは言わない。置換条件はまだ約64.8msを要する。RSS/実確保回数、他機械/フォント/DPI、任意入力は未測。

### 既存基準での正式速度

`python run-formal-360.py`から正規`python eng/measure-speed.py --check --executable <ca4ad39通常Release> --bench <下記> --repetitions 5`を事前順で一度実行。今回body/caretを通る4条件だけを選び、起動/open/paletteは変更関数の直接リスクを増やさないため再実行しない。指紋bc8a356f37c68491の実機既存基準、25%許容/2ms床の実判定、reference SHA `ce1f2bc26887bebf0f29c5d683422f82586dc912658d1190dd5cf6ec92a4ef43`は不変。参考記録だけの成功ではない。

| bench | 5試行のms（順番どおり） | 中央値ms | 判定 |
| --- | --- | ---: | --- |
| key-to-frame-single | 0.636, 0.621, 0.632, 0.647, 0.589 | 0.632 | 0退行/0計測不能 |
| key-to-frame-burst-200 | 2.302, 2.173, 2.129, 2.223, 2.220 | 2.220 | 0退行/0計測不能 |
| key-to-frame-burst-200-16mib | 2.216, 2.331, 2.346, 2.946, 1.847 | 2.331 | 0退行/0計測不能 |
| key-to-frame-single-long-line | 5.960, 5.954, 5.908, 6.115, 5.975 | 5.960 | 0退行/0計測不能 |

各benchの全5summaryとlog、固定source/exe/SHAを保存。正規道具は最終trialのmarksだけを保持するため、全5trialのmarksがあるとは記さない。snapshotに重複して残る以前のbench JSONは対象benchを選び、追加試料として数えない。

### 収載と後続

恒久先`D:/NeNeNib/evidence/speed-optimizations-20261010-wave3/snapshot-360`に原記録/初回失敗/全GUI/marks/両build設定/DebugとRelease製品/source/レビューを元先SHA照合して収載した。842files/52860599bytes、manifest SHA `7c2bbcbd525c9887e83e8be70541e89fa9830e61a25ed33764348507ca383952`。旧baselineはsnapshot-358に保持。文書/Git/CI/main/整理の後続証跡はacceptance-360へ追補する。再生成できるobject/libraryだけを除外する。

#358はPR #359で11:57:20 JSTにmain6ab76d2へ統合、本体同期/受理tree同一/恒久収載と監査を終えた。追加358worktreeと不要な一時出力は削除済み、branch/commit保持。ここで再実行しない。

#360のCI/main/必要反映後にDの作業木と不要出力の絶対path/取込/未保存/未追跡/ignored/唯一成果/稼働参照/linksを確認して整理する。公開契約/schema変更なし、waiver none。hideの停止指示まで継続する依頼に従い、次の独立候補は置換色tintの描画経路を調べる。現時点では未設計/未起票/未実装/未計測で速度利益を主張しない。

## 5-dk — 着色文字も既存の字形経路で描く（Issue #362・ADR 0098）

**技術受理**。R3の残りを専用条件で比較し、本文の混在長行に大きな利益を確認した。製品の変更は `Direct2DRenderer.cpp` の `tint_runs` 内の一呼出しを既存 `draw_body_text(text, area, NONE)` へ置換し、不要originを削除する二編集のみ。元からあるbrush、HitTest範囲/順、clip push/pop、paint順、保持/失効、未登録または!glyphs_readyのDrawTextLayout fallbackを保つ。取得成功の空glyph列は正常な空描画でありfallback条件とはしない。一覧入力欄は従来どおり本文保持に未登録で、正規fallbackを通る。

先行ADR `30394c9`、製品 `36ebb6154935ad1722f7007b3e6a64e872424aa6`、文書訂正 `9218124` / `3b00a4d`。通常Releaseは9218124、Debugは36ebb615の同一製品source。基点main `3b114d6` と前版ca4ad39の製品/test/eng/flagsは一致。測定時HEAD `3b00a4d4a06bc935dc07a2f1d65f162fd7caf123` はclean。実装build前後は524source指紋と各参照object/実flagsを照合し、Renderer全文への厳密二編集と他の全関数不変を確認した。測定前後auditは実装source1・道具15・3exeの19path、HEADclean、identityを照合した。identity SHA256 `9cc9a4430351750c76f7ada82ee5ff7dcd83c82d2f300e3da831420c83cfd192`。

規則: ARC-001/002/004/007/008/011、CPP-003/008/011/012/016、QLT-001/002/004/008/012/013/014、CNF-001/002/003/004/009/012、D41。waivers **none**。公開API/source/target/独自描画器/decoder/cacheを追加せず、schema・fixture・警告・抑制・基準・許容を変更しない。

### 固定した製品応答比較

i9-10850K / RTX 3090、電源HP推奨（GUID48684d4a-8524-4093-8a63-ea7132b79c1c）、window DPI120、同じ絶対文書path/設定/system/Cascadia Code13.5/guide on/1280×800。通常Release前後の `--measure` と正規window_driverを使用。ASCII `a\x01 `×1024、混在 `日a🖋\u200b `×1024、短行 `a\x01 `×2、すべて末尾CRLF。検索 `/a<CR>` 後に最初のaへgg0/gg0l/gg0で固定し、暖機nNの後、交互nN20入力をABBA×3。一条件120対応組、全36process/720計測input。間隔はASCII/短行0.4秒、混在3秒。起動/準備/検索生成は区間外、input_receivedから次input前の最初のframe_presentedまで。Present復帰でありGPU完了やtint単体時間ではない。

| 条件 | 前版中央値µs | 後版中央値µs | 対応比中央値 | 対応比min–max | 短縮/同/遅延（120組） |
| --- | ---: | ---: | ---: | --- | --- |
| replaced-ascii-1024 | 64760 | 64106 | 1.001570153658 | 0.899039897589–1.118709999169 | 56/0/64 |
| replaced-mixed-1024 | 1.79317e+06 | 20619.5 | 0.011477568240 | 0.010769417527–0.014784208061 | 120/0/0 |
| replaced-short-2 | 601 | 605.5 | 1.013963963964 | 0.620689655172–1.792968750000 | 51/1/68 |

混在は120組全て短縮、対応比中央値約0.01148を確認したため採用する。ASCIIは対応比約1.00157で横ばい。短行は601→605.5µs（4.5µs増）、対応比約1.01396で68組遅く、無退行や全入力高速化とは記さない。中央値同士の比と対応比中央値は別値である。全36trialは初回exit0、各trialの暖機可視変化とn/N後復帰0差、case内の99画像比較も0差。独立validatorは暖機72と測定720の全792入力に次input前frameがあり、計測値と原区間が一致することを確認した。測定前後のCPU/GPU/powerPlanは同一、他のbuild/test/GUI/重処理は停止。計測中の作業は小さな読取/記録と未実装の次案調査だけ。

### 直接境界と実行記録

以下の`<WT>`は測定時 `D:/NeNeNib/worktrees/362-tint-glyphs`、`<OUT>`は `D:/NeNeNib/outputs/20261010-tint-glyphs`。整理後は後述の恒久先に同じ相対配置で原記録を保持する。成功済み結果を工程・担当・文書commitだけの理由で再実行しない。

| 対象と退行リスク | 実行コマンド/記録 | 結果 |
| --- | --- | --- |
| 固定tool/外部flag、単一sourceのformatと規約 | `eng/toolchain.ps1`、`clang-format --dry-run --Werror --style=file:<WT>/.clang-format <WT>/src/ui/win32/Direct2DRenderer.cpp`、`python -X utf8 <WT>/out/362/focused-conformance.py` | 初回exit0、0違反。waiver none |
| 呼出し型/警告/tidy/実製品sanitizer | `cmake -S <WT> -B <WT>/build -G Ninja -DCMAKE_BUILD_TYPE=Debug`、`cmake --build <WT>/build --target NeNeNib -j 2` | 初回116段成功、正規C++23/clang-cl19.1.5/ASan/UBSan/no-recover/tidy。NeNeNib targetと必要依存のみ |
| 二編集限定と他source/試験/flags不変 | `python -X utf8 <WT>/out/362/source-evidence.py`、`finalize.py`、`git diff --check` | 初回exit0。implementation/report.md/manifest.json/rawに全command/実flags/指紋 |
| 通常Release条件 | `pwsh -NoProfile -File eng/build-release.ps1 -Ref 9218124` | 初回116段/139.631秒、exit0。out/release/9218124.json |
| 実Rendererの色・clip・字体・重なり | `python <OUT>/verify-362-tints.py --label before --executable <前版>`、`run-gui.py --label debug/after --executable <対象版>`、各scriptの`--compare` | 3style×9scene=27の前後本文/status完全0画素差。18刺激遷移は全て可視変化あり。Debug実起動/正常終了 |
| IMEの本文保持と入力欄fallback | `verify-362-ime-body.py`、`verify-362-ime-inputs.py`をbefore、同run-gui.pyでdebug/after、各`--compare` | 本文4＋palette6場面も0画素差。実SendInputのother色0→204→0と0→83→0、本文Space accent285。0x411、open status0へ復元。合成IMEなし |
| 道具の入力対応と短行/長行の可視変化 | `compare-362.py --smoke before/debug/after --before <対象版>`、`validate-362-marks.py <成功3trialのfolder列> --expect-trials 3 --output <warmup.json>` | 全3caseずつ成功。旧版ASCII/短行v2成功を再利用し、混在だけ修正版v3を別folderで確認。速度採否には使わない |
| 固定比較と全入力の独立監査 | `compare-362.py --before <ca4ad39 Release> --after <9218124 Release>`、`validate-362-marks.py <OUT>/comparison --expect-trials 36 --output <OUT>/warmup-comparison.json` | exit0、36trial/360対応組/720計測/72暖機、99画像比較0差 |
| 同じ製品/入力/環境の証明 | `prepare-measurements.py`、`environment.ps1 -Label before/after`、`post-measurement-audit.py` | exit0。19pathのSHAとHEADclean、正式再利用原記録の不変、前後snapshotで重処理なし（測定中は作業lock）。JSONに正しいHP推奨を保持 |
| 独立レビュー | review/design.md、product.md、harness.md、acceptance.md | 厳密二編集、other節到達、全試料/暖機、正式再利用、短行の増加を含め確認。阻害所見は採用前に解消 |

GUIはASCII制御文字/BMP/補助平面/結合/Tab/双方向/複数置換、bar/normal/検索/選択/line/block、狭幅右clip、異なる字体/size/themeを含む。本文IMEは差込前後に置換文字、一覧IMEは160字のprefixによる左への横溢れを含む。全対象processは正常終了、文書/設定不変、本文IMEのdirtyは確認画面でdiscard。親は画像も読んで到達を確認した。

### 再利用・失敗保持・限界

正式single/burst200/burst200-16MiB/single-long-lineは#360の成功を再利用し、#362で再実行したとはしない。これらは制御置換のないASCII/日本語/空文書でIMEも作らず、変更tintへ非到達。draw_body_text/collector/幾何/他Renderer関数/core/application/tests/eng/flags/基準は不変。保存済み前版exe、正式record、script/referenceのSHAを照合し独立レビューで経路を確認した。既存WICのNONE+caller clip契約もdraw/collector/testが不変なので再利用し、実Renderer到達は新しい37場面で別途確認。全scope/全unit/全速度8条件を新実行しない。

初回smokeは混在のframeが0.4秒を超えて入力が合流、短行は検索開始点からの折返し通知で復帰不一致。v2の混在gg0は日を指しており最初のaではなかった。初回/v2 script/plan/rawを残し、間隔3秒・混在gg0l・準備input7/8/7を比較開始前に固定した。ASCII/短行v2の成功を再利用。速度試料の選別や合格までの再測定はない。継承planの旧source/GUI metadataも元を保存して訂正した。

初回IME道具は検索欄がIMEを受けると誤認したが、既存ImeStance::closedとcomposition_ignoredでother色0の未到達となった。原script/rawを残し、実到達する本文/paletteへADRと検証範囲を訂正した。これは製品退行ではない。ADR名/ログpath/メタデータpathの推測読取失敗、無いprocess名の監視、出力truncate/端末文字化けもjournal/rawへ記録。製品build/採用用比較の失敗を隠したものではない。

実Rendererの資源失敗注入、RSS/実確保数、任意装飾・全フォント・DPI・IME・全入力の同値、全般的な描画速度は未測。画面外clip省略やCPU samplerは次案の読取調査だけで、製品変更/実行/速度効果を主張しない。

### 恒久収載と後続

全source/通常DebugとRelease/実flags/FileAPI/GUIとIME画像/原marks/全試料/初回道具失敗/独立reviewを `D:/NeNeNib/evidence/speed-optimizations-20261010-wave3/snapshot-362` へ元先SHA照合して収載した。740files/27924676bytes、manifest SHA `125de7a682328a341a24698d135927314a1df088645a64f7fe71d7179d31c3f1`。前版はsnapshot-360、後版はsnapshot-362/files/worktrees/362-tint-glyphs/build/release-9218124/NeNeNib.exe（SHA `ce0277729b358d2d66f2dda65d592b62e3a159eecb7dbe3e9b85a744d0a88f77`、1449984bytes）、DebugはSHA `791642571062a2af35c1c4cce2fb1264a77812961c0c3af3743fb7225365255c`、17022976bytes。CI/main/追加文書とcleanupはacceptance-362へ後続保存。統合・唯一成果・未保存/未追跡/ignored・稼働参照・links・絶対D pathを監査して追加worktree/不要出力を削除し、branch/commitは保持する。

## 5-dl — 完全画面外の着色clip省略は実験不採用（Issue #364・ADR0099）

設計固定は2509493、実験製品は0c91c990446e70e90c62e05b0894d5ccbfaa206d、測定HEADは5bf99af17baa01370f4de86f7a5baee6a6692c91。tint_runs内でtarget幅widthを一度取得し、`run.left > width || run.left + run.width < 0` のrunを省く二挿入を試した。今回の固定全画素条件を満たさず、ASCII固定比較も欠測となったため不採用。二挿入を戻し、製品/src/tests/eng/CMake/flagsをmain221cbbcと同一へ復元して文書だけ収載する。混在で観測した短縮はmain適用済みの利益ではない。

規則はARC-001/002/004/007/008/011、CPP-003/008/011/012/016、QLT-001/002/004/008/010/012/013/014、CNF-001/002/003/004/009/012、D41。公開API/設定schema/fixture/基準/許容/抑制/allowlist変更なし、waiver none。

以下のOUTは実験時 `D:/NeNeNib/outputs/20261010-tint-clips`、WTは `D:/NeNeNib/worktrees/364-tint-clips`。恒久先は `D:/NeNeNib/evidence/speed-optimizations-20261010-wave3/snapshot-364` のfiles以下で元D相対pathを保つ。元path削除後も元raw metadataは書き換えない。通常旧版はsnapshot-362のfiles/worktrees/362-tint-glyphs/build/release-9218124/NeNeNib.exe（SHA ce0277729b358d2d66f2dda65d592b62e3a159eecb7dbe3e9b85a744d0a88f77）。新版通常Release0c91c99はSHA209a38fb976946745859d0b6854d8a11bf73dbc472d439a68a5316c548e8550d、通常DebugはSHAa741d8b653ce787a7be25d76ed499223d3e1e51bce746c370e894aed4cf954a1。

### 対象を限定した検証と原失敗

- 変更sourceのclang-format/source_checks/waiver_checks/whitespace、通常Debug configure/build `--target NeNeNib -j 2` は初回exit0。正規tidy/ASan/UBSan/no-recover、116段。strict二挿入と全他source/tests/eng/flagsの9gitobject一致、524sourceのbuild前後SHA一致を確認。`pwsh -NoProfile -File eng/build-release.ps1 -Ref 0c91c99` は通常Release初回exit0。command/実flags/元stdoutはOUT/implementation/{report.md,manifest.json,raw}、WT/out/364、OUT/release-0c91c99.log、WT/out/release/0c91c99.json。
- `python OUT/run-before.py` は新規edge49幅640〜688・一覧IME6・smoke3を初回exit0。`python OUT/run-gui.py --label debug/after --executable <各通常exe>` は本文27/本文IME4/一覧IME6/edge49とsmoke3を実行して全command正常終了した。これは比較結果の全成功を意味しない。`python OUT/verify-label.py --label debug` は成功、`--label after` は一覧比較でexit1。原validation-after.json/ime-palette/comparison.jsonはfailed/falseを保持。
- 本文27＋本文IME4のbefore31画像は#362 afterから再利用。同文書bytes/設定/字体/テーマ/DPI/刺激/道具/比較領域を照合し、絶対folder差が本文/statusへ表示されないことをsourceとbasenameから確認した。一覧emptyは候補locationに絶対folderが出るため旧画像を再利用せず新共通pathで採った。before metadata不変。compare-reused.pyのDebug/after31比較は差0、edge前後49は差0。最終列の固定muted色は49中16幅で到達/33幅で非到達。内部端点等値と完全左外側への実到達は未測。
- Release一覧03-other-compositionは6画素が一channel一段だけ異なった。他5場面は差0。6点はclient(1006,199)/(999,204)/(1024,204)/(1006,253)/(997,270)/(996,272)。本文入力欄の底185より下にあり、固定一回の所有者診断で全点が別process explorer.exeのApplicationFrameWindowだった。診断と元失敗Release PNGはSHA08a7b0d94e7b920127aaecc13313fbd8804fbef1e9bbee8b92d03a08dbeb12b2で完全同一。旧版/Debug PNGは別の同一SHA32c9aa2b0e75410174e5656f257630d70513a3dc06d45e9fe7c1a046a6096cc0。外部overlay表示領域の差という判断を#365へ分離したが、OSの具体的生成原因・同旧exeの揺れ・隠れたtarget画素は未実証。
- 原全画素条件はfalseのまま。mask/閾値/成功までの再撮影/元失敗の置換なし。独立review/ime-difference.mdの事後条件限定を許せるとした助言は、review/disposition-policy.mdで撤回された。AGENTSとQLT-010に従い、結果後に比較対象を狭めて今回を採用しない。QLT-012による別Issue分離と、採用条件未達を区別する。
- 本文IMEのother色0→204→0・Space accent285、一覧other0→83→0、open0復帰/文書破棄/設定不変を確認。正常Debugは86場面を正常実行し、再利用31比較は差0、一覧03の原PNGも旧版同一、edge末端着色は16有/33無だった。Debug全86の全画素比較を実施したとはしない。Releaseは85場面差0＋1場面6px差であり、「86場面同値」「全gate成功」としない。

### 一回の固定比較

i9-10850K/RTX3090/HP推奨、DPI120、1280×800、Cascadia Code13.5、system/guide on。ASCIIは`a\x01 `×1024、混在は`日a🖋\u200b `×1024、短行はASCII×2、CRLF。検索/a<CR>と先頭anchorを固定、暖機nN後の20n/Nを0.4秒間隔、ABBA×3で各120対応組を計画した。#362の混在3秒条件と直接比較しない。全36予定を一度だけ実施し、再試行/試料選別なし。

`python OUT/compare-364.py --before <9218124.exe> --after <0c91c99.exe>` は終了1。ASCII08-A（第9試行の旧版）がWM_CLOSE後15秒待っても終了せず、TimeoutExpired。raw marksは未出力、canonical driver.stopでそのowned processだけを停止した。ready/final画像はdirty dotと先頭余分文字様、最終Vim表示消失を示すが出所は未特定。ASCII固定120組は成立せずunmeasured。残り予定を記録し、成功試料だけでASCII改善率を補わない。

| 条件 | 前後中央値µs | 対応比中央値 | 短縮/同/遅延 | 状態 |
| --- | --- | --- | --- | --- |
| replaced-ascii-1024 | 算出しない | 算出しない | 集計しない | 旧版08-Aの失敗で固定比較不成立 |
| replaced-mixed-1024 | 20802.5→7054.0 | 0.338704158088 | 120/0/0 | 固定120組 |
| replaced-short-2 | 603.0→606.0 | 1.010831974533 | 53/2/65 | 固定120組 |

全36試行中35成功、1失敗。validatorは35試行の原marksを照合したが、必要36に足りずexit1/failed-or-unmeasured。欠測を除いて全比較成功とはしない。audit-rejected-experiment.pyはexit0/auditStatus completeで、製品受理のpassedではない。

`python OUT/validate-364-marks.py OUT/comparison --expect-trials 36 --output OUT/warmup-comparison.json` の結果と、成立した試行数は下記追記に記録する。成功用post-measurement-audit.pyは元のまま保存し、既知の未達を成功扱いに変更/実行しない。別audit-rejected-experiment.pyは元失敗が残ることと凍結source/exe/harness/GUI証拠/正式再利用/HEADの不変性だけを監査する。凍結environment.ps1の正常終了固定文をafterへ誤用せず、説明を訂正した別environment-rejected.ps1を使う。前後環境はsnapshotであり連続監視ではない。

正式single/burst200/burst200-16MiB/single-long-lineの#360成功4本は再実行していない。入力がtintへ到達せず、tint外のRendererとcore/application/adapters/tests/eng/flags/基準が同一であるsource証拠に基づく再利用。最終変更は製品全体を受理済みmain221cbbcへ戻す文書差分なので、元成功の再利用条件をcontentで確認し、製品試験を工程理由で繰り返さない。

初回prepareのGUI false assert、外部所有診断、read path/encoding/glob誤り、edge-inkの説明誤記と元script、比較09旧版の失敗、全画像/試料/marks/初期と最終reviewを保存。edge-ink集計RGB/16対33は不変で、説明だけBuiltinThemeの固定色へ訂正。詳細はjournalと各原stdout/JSON。任意文字/字体/DPI/IME/資源失敗、RSS/実確保回数、GPU完了時間は未測。

恒久snapshot-364へ全元先SHA照合して841files/28392986bytesを保存。manifest SHA `c54c1c9acb33f15ff5c8a330efda9cb6f8c3f3aedde58bc3e3d27673120de1fd`。独立review/acceptance.mdは不採用と記録の整合を確認。後続の文書/CI/main/復元proof/整理はacceptance-364。元候補commit/branchと全rawを残し、監査後にDの追加作業木・不要出力を削除する。

## 5-dm — 画面外clipの独立再評価は撮影不成立と入力外乱で保留（Issue #372・ADR0101）

hideの再開指示により旧#364を別系列で再評価した。設計commit685ab5d/df7c278、製品はbefore9218124/after0c91c99の保存済み通常Release、二挿入だけであることを全文照合。main25ff09cはbeforeのsrc/tests/eng/flagsと同一。今回のrepository変更はADR・報告・引き継ぎ等の文書だけで、候補の製品コードを新作業木へ適用していない。処分は保留であり、製品不良や利益不足とは判断しない。旧ADR0099/失敗/不採用は不変。

規則QLT-001/007/010/012/013/014、ARC-001/004/007/008/011/012、CNF-006、GIT-001〜004、D41。schema/API/基準/許容/mask/抑制変更なし、waiver none。以下OUTは実験時D:/NeNeNib/outputs/20261010-tint-reevaluation、恒久先はD:/NeNeNib/evidence/speed-optimizations-20261010-wave3/snapshot-372/files/outputs/20261010-tint-reevaluation。後続文書/CI/main/監査整理はacceptance-372。元rawのpathを改変しない。

### 選んだ検証・再利用と結果

| コマンド / 証拠 | 直接の退行・判定対象 | 初回結果 |
| --- | --- | --- |
| `python OUT/provenance.py` / provenance.json | 保存済みexe/正規build実flags/source、二挿入、mainとbefore、旧証拠の同一性 | exit0。before SHA ce0277729b358d2d66f2dda65d592b62e3a159eecb7dbe3e9b85a744d0a88f77、after209a38fb976946745859d0b6854d8a11bf73dbc472d439a68a5316c548e8550d、Debug a741d8b653ce787a7be25d76ed499223d3e1e51bce746c370e894aed4cf954a1 |
| `python OUT/test-trial.py` | session入力/dirty/foreground/IME/予定marks/次input前frameの拒否、QPC観測境界 | 初稿2tests成功。最終inputを後検査frameで救済する反例追加で2tests成功。QPC上界反例追加で3tests成功。変更した検査入力にだけ再実行 |
| `python OUT/run-harness-proof.py smoke` | 既存Exコピー/全本文/ready画素/予定mark/観測前frame完了の実到達 | exit0。旧exe ASCII、body3コピー一致、ready旧画像差0、warm変化/final復帰、正常終了。67.664/68.346msはcorrectness用で利益へ算入しない |
| `python OUT/run-harness-proof.py dirty` | 旧失敗と同じ予定外oを測定前に拒否すること | exit0。ordinaryへ合成o、dirtyをcopy/測定前に拒否。failed原record/PNGを保持し、既知合成入力の専用窓だけ停止 |
| `pwsh -NoProfile -File OUT/environment.ps1 -Label before/after` | hardware/電源/競合処理・必要環境 | 各exit0、i9-10850K/RTX3090/HP推奨、同OS起動/driver、競合対象なし。snapshotであり連続監視ではない。各GUIはDPI120/1280×800/Cascadia Code13.5/system/guide on |
| `python OUT/ime-comparison.py --label A1 --executable <before>` | 候補背後を含む全製品画素、保存画像上のIME到達 | exit1、04target到達不成立。03の候補窓を列挙できず全画素取得も不成立。A2/B未実行 |
| `python OUT/run-series.py freeze` / plan.json | 全6script/製品/ABBA順/全条件の固定 | exit0、plan SHA b8225e401c53b7bece90c0c3ff96181de28835e3477f3e66610273f879dffb69 |
| `python OUT/run-series.py run` | ASCII/混在/短行、20nN入力、ABBA×3各120組 | exit1、ASCII00-Aでsession入力変化を検出。1attempt/有効0、残35未取得、全条件有効対応0。改善率算出なし |
| `python OUT/audit-held.py` / held-audit.json | 凍結指紋と製品不変・原失敗保持・保留判断 | exit0/auditStatus complete、productAccepted false。採用の成功判定ではない |

全本文の:w別pathコピーはretain_documentを使い、原文書名/未保存状態を変更しない。計測入力はPostMessage、ReturnのKEYDOWN/CHAR2markも全数へ含める。製品bindはwindow作成前なのでstart直後QPCをorigin上界とし、frame相対µsの切捨てを+1とceilで包んで3観測開始より厳密に前と要求した。独立レビューの監視重なり・dirty窓破棄・別captureからのIME判定の3阻害を修正、修正前scriptはinspector-before-reviewに保存。修正後静的阻害なしだったが、実機で撮影網羅性が否定され評価を更新した。

正式4編集条件の#360成功と旧GUI成功は同じexe/source/tests/tools/flags/環境の記録を照合。tint非到達・他の描画経路不変という従前の再利用根拠も保存したが、今回新しい必須条件を満たした代用にはしない。新build/全unit/全gate/正式8条件/旧86場面は再実行していない。最後は文書だけの差分なので製品テストを追加しない。

### IMEの誤判定と新しい6画素の根拠

03-productには候補面が明瞭に写るのにaboveBefore/aboveForCapture/aboveAfterCaptureは[]、moves=[]。空配列のall検査と既存5点のassert_uncoveredでは候補を検出しなかった。raw/product同じbitmapのinput差0も全製品画素の露出証明にはならない。原capture status passedを書き換えず、held-auditでvisual evidenceにより無効と明記する。候補移動/復元は行われていない。窓chain外/visible・cloakフィルタ/合成関係のどれが原因かはフィルタ前記録がなく未特定。04はIME色83→0・accent42→27、PNGからも日本語composition/候補が消えておりtarget未到達。製品差ではない。#365に残す。

今回旧exeの03 PNGはSHA08a7b0d94e7b920127aaecc13313fbd8804fbef1e9bbee8b92d03a08dbeb12b2で旧候補版03と完全同一。旧exeの前回03（SHA32c9aa2b0e75410174e5656f257630d70513a3dc06d45e9fe7c1a046a6096cc0）との差は既知の同6座標だけ。**同じ旧exeでも6画素差が出た**ことが新しく分かった。入力欄と製品の着色コードへ差を帰属しない。OS内部の生成原因は未確定であり、この証拠でも隠れた製品画素の同値を救済しない。

### 性能系列の中止と未測

ASCII00-Aは開始〜暖機後のLastInput tick36908687が測定後36953703へ変化。title保存済み、foreground true、IME0、DPI/寸法は維持。入力元は未取得なのでhide/他process/OS/マウス/鍵のどれかは断定しない。最終copy/正常終了/marks flushへ進まなかったためraw marksなし。保存済み状態のowned processだけ終了、unexpected dirty窓を捨てたものではない。before2全本文copy/ready/warm/failure画像/予定238mark台本/全checkpointは保存。

条件は緩めず無効trialを差し替えず、欠測を旧試料で埋めない。残る35trialとIME A2/Bは未取得。今回の性能利益・短行費用・全製品同値・候補移動/復元は未測。入力監視の未知は#373へ分離し、依存しない#367へ進む。#367の7未保存ファイルは再開時に停止snapshotと一致、preserved-367へ追加保全済み。製品採用なしなので基準を#362のまま保つ。

全実験/初稿/ツール読取失敗/差分レビュー/保留判断はjournalとreview-planに保存。computer-use native pipe unavailable2回は入力なし、別のUI実行成功とは数えない。rawの推測path/glob失敗は実在pathの再発見で訂正、正式比較の失敗を再試行してはいない。恒久収載/取込・未保存・ignored・唯一成果・稼働参照・link・D絶対pathの監査後に#372の作業木と不要OUTだけ整理し、branch/commit/原証拠と未完了#367を保持する。

## 5-dn — 表示行一覧の既存範囲を一度予約する（Issue #367・ADR0100）

停止時の5modified/2untrackedをSHA照合・保全して再開。#372はPR374/main decd4f1で保留記録を統合、恒久収載・監査整理済み。#367は同mainの文書を同期し、先行ADR0100のdispatch設計を0d05b7aへ固定した。製品はbefore ffa57afからafter e7fd0c7へ、EditorController::visible_linesのvector直後に非空rangeのreserveを一度足す四行だけ。値・順序・所有・first/last・loop・line_view・検索・選択は保持する。

規則ARC-001/002/003/004/007/008/011、CPP-003/004/007/008/011/012/016、QLT-001/002/004/007/008/010/012/013/014、CNF-006、GIT-001〜004。公開API/schema/基準/許容/flags/抑制の変更なし、waiver none。OUTは実験時`D:/NeNeNib/outputs/20261010-frame-rows`、Rは`OUT/resume-1911`。恒久収載先はwave3の`snapshot-367/files/outputs/20261010-frame-rows`、統合・監査整理は`acceptance-367`に記録する。原raw内の旧pathは書き換えない。

### 一経路の計測器と対象検証

旧53enum/4要素登録/処理本体を保持し、新4条件だけ別の閉じたFrameRowsWorkloadとした。型付きProbeDispatchRowを一つの57行表へ載せ、ProbeSelectionのvariant訪問も同表を選ぶ。各enumの網羅switch、constexprのindex/名前/関数/size検査、型付きfactory/runnerが閉集合を強制する。選択は区間外。CPP-012の60行/nesting3を緩めず、各型を同名headerへ置きCPP-011も守った。

| コマンド / 原証拠 | 直接確認する退行 | 結果 |
| --- | --- | --- |
| `python -m unittest tests.conformance.test_compare_probes.ProbeComparisonTests.test_stage_six_frame_inputs tests.conformance.test_compare_probes.ProbeComparisonTests.test_frame_rows_inputs_and_registry tests.conformance.test_compare_probes.ProbeComparisonTests.test_frame_rows_invalid_names` | 新4入力/FNV/registry・旧53末尾境界・不正名 | 停止前の初回3tests成功を再利用。Python2ファイルは保全稿とbytes一致。原証拠implementation/raw/stage1-tooltests.* |
| `python R/source-proof-harness.py` / nesting-fixの訂正後proof | 旧53登録/enum/本体/網羅switch、製品不変、CMakeは登録だけ | exit0。constexpr付与以外の旧switch保持、旧関数block SHA242a520c2a1f5aa072923081c0bac8ccaab2306efab020f92a688aa1db266c35 |
| `pwsh -NoProfile -File OUT/implementation/build.ps1 -Phase before-release -Configuration Release -BuildDir build/probes-before-release` | 正規compiler/tidyと追加dispatch | 初回exit1、workload_ofのlambdaがnesting4。関数抽出で修正、ffa57afのPhase before-release-nestingでexit0。初回raw保持、規約変更なし |
| `python R/dispatch-proof.py` | 網羅性と型一致がcompilerで拒否されること | 正例exit0。旧case欠落/new case欠落/未知variant alternative/型違いfactoryの4負例各exit1・想定diagnostic、総合exit0。正規compile_commandsからD専用copyへsource/Fo/Fdだけ変更 |
| `python OUT/implementation/source-proof.py product ffa57af` | 製品変更が指定四行だけ、同じharness | exit0。他srcとtests/eng/CMake/tidy/formatは同一 |
| `cmake --build build/367-debug --target NeNeNib nib_tests nib_perf_probes -j 2`（build.ps1のDebug configure後） / `-Phase after-release -Configuration Release -BuildDir build/probes-after-release` | 変更sourceと直接callerを正規設定でbuild | 各初回exit0。Debug3targets、Releaseはprobe。ASan/UBSan/norecoverの実flags確認、全source/commit/clean/tools/flagsは各pre/post JSON |
| `pwsh -NoProfile -File OUT/implementation/verify.ps1 -Stage debug` | frame/scroll/selection/所有と新4probe oracle | 初回exit0、`nib_tests --application`16009checks、4条件各1iteration成功。frame-selectionを重複実行しない |
| `pwsh -NoProfile -File OUT/implementation/verify.ps1 -Stage self` | 同一exe/metadata/marks/新入力の接続 | 初回exit0、4条件各iterations1/blocks1。性能利益には使わない |
| `python OUT/implementation/scoped-static.py product build/367-debug` / `python eng/symbols.py --build-dir build/367-debug --require application core` | 追加source登録/依存graph、予約が禁止OS依存を導入しないこと | 各exit0、source/graph0違反、2libraries/0違反、waiver none。coreはapplicationの依存解決対象 |
| `python R/run-comparison.py` | 64frame生成破棄の利益/空短文費用 | 初回exit0、4条件48process/1920marks、各120組。下表。欠測/補完/除外/再試行なし |
| `pwsh -NoProfile -File eng/build-release.ps1 -Ref e7fd0c7932ee45c7f1ed5ded4389b509dfce47e6` | 通常製品とprobe以外のbuild | 初回exit0、configure1.791s/build266.313s、1450496bytes。out/release/e7fd0c7.json |
| `python R/gui-rows.py` → `python R/complete-gui-empty.py` → `python R/continue-gui-rows.py` | 空/2行/多行scroll/EOF/selectionの全文と全client画素 | 初回固定待ち失敗を保持し同じ保存の完了を別観測、未実施場面のみ続行。前後7場面すべて0px、全14PNG目視。下記に限界 |
| `python R/formal-speed.py --check --executable <WT>/build/release-e7fd0c7/NeNeNib.exe` / `python R/audit-formal.py` | 共通frame callerの正式速度と有効標本/基準 | 初回各exit0、8指標各5/5、0退行/0計測不能。原30trial保持。下表 |
| `python eng/protected-diff.py --base decd4f1 --head e7fd0c7` | 既存fixture/基準/許可/規約設定の差分 | exit0、fixtures1853→1853、metadata/deleted/changed/added各0、protected files changed none。out/protected/e7fd0c7.json。exe/buildを指定せず45scopes全未測、件数同一や全test成功は主張しない |

before probe SHA `bb33dd4e26e01a9313a37f76b2592e62ad50e61937b36cbe6fc037bbd25eaf16`、after `5d139269216a61028766f54df75d0256d7b0da8a4b0176bf35d2eabe62019c6b`、通常製品Release `bb2bc96e92c9509ed5d70b3e25f933288c3b048c55d4dc13728b64121555ac4b`。Debugの3exeと691sourceのSHA、実flagsはimplementationのcapture JSON。独立レビューで全185compile commandはcommit/build先以外一致、CMakeCacheはbuild先だけ、Ninja残差は構成入力一覧だけと確認した。comparison前後のfingerprintbc8a356f37c68491、i9-10850K/RTX3090/120dpi/HP推奨は一致。snapshotであり連続監視ではない。

### 固定比較の利益と費用

| 条件 | before / after中央値µs | 対応比中央値 | 対応差中央値µs | 短縮組/120 | cycle比中央値 |
| --- | ---: | ---: | ---: | ---: | --- |
| 空・1行 | 44 / 43 | 0.9772727273 | -1 | 97 | 費用条件内 |
| 2行 | 69 / 59 | 0.8550724638 | -10 | 119 | 費用条件内 |
| 30行 | 520 / 361 | 0.6928231821 | -160 | 120 | .6920105385 / .6875006934 / .6951587765 |
| 120行 | 2244 / 1409 | 0.6322791141 | -822 | 120 | .6178293503 / .6090123959 / .6476575320 |

全条件20iterations・ABBA3・各120組。空/2行は事前の差≤50µsかつ比≤1.10、30/120行は対応比/全cycle<1かつ短縮≥90組を満たす。比は対応比の中央値で、表の二中央値を割った値とは別。測定するのは64回の生成・破棄と固定checksumの合計。UI応答の31%/37%改善や純1frame時間、実確保回数/RSSは主張しない。各oracle/metadata/全試料/原marks/集計はfixed-comparisonに保持する。

### 直接GUIと正式速度

通常製品のbeforeはsnapshot-362のrelease-9218124（受理済みsrcはmain decd4f1と同じ）、afterは上記e7fd0c7。同じ空/2/120行混在CRLF、同じ設定/1280×800で、empty先頭、short先頭/EOF、full先頭/中間/EOF/visual選択を比較した。before7とafter空の撮影後、after空の最終:w別pathコピーは.75秒後に見つからずFileNotFoundError。原failed record/PNG/scriptを保存した。

同じcopyは後の観測で存在。PID/HWND/実exe/入力tick/保存title/foreground/寸法/原文書/設定を確認し、追加save/再撮影なしで0bytes全文を確認して正常終了した。非同期PostMessage後の原子配置完了待ち不足と整合するがmtimeだけで所要時間を断定しない。未実施after short/fullだけは一度送信後のdestination出現待ちを加えて取得。すべての全文初終copyと正常終了を確認し、7scene0px・全14PNG目視で文字/行番号/選択/caret/状態表示が同一。元失敗を一括成功へ書き換えない。2560点検査だけを全面遮蔽の証明にせず、#365のIME未解決とは分けた。

正式8指標は6刺激groupで測った。EditorWindow::start_rendering/paintがframe→visible_linesへ到達するため、空起動、空の単/連続入力、大容量初回/編集、一覧paint、長行payloadの各境界が直接影響する。window-shownは同じ起動に付随し追加刺激なし。旧tint非到達による再利用は不可。全gate/無関係な86場面は実行していない。

| 正式指標 | 中央値ms | 有効/予定 |
| --- | ---: | ---: |
| startup-first-frame | 197.8746 | 5/5 |
| startup-window-shown | 32.2822 | 5/5 |
| key-to-frame-single | 0.618 | 5/5 |
| key-to-frame-burst-200 | 2.106 | 5/5 |
| open-large-file-16mib | 212.0635 | 5/5 |
| key-to-frame-burst-200-16mib | 2.247 | 5/5 |
| key-to-frame-palette-5000 | 2.071 | 5/5 |
| key-to-frame-single-long-line | 5.860 | 5/5 |

正式結果はformal-speed/canonical-output/2026-10-10T10-51-31Z.json。既存reference/toleranceを変更せず8項目すべて合格、missing0。起動/長行には基準中央値を上回る値もあり、全指標の高速化とはしない。wrapperは未変更measure-speed.mainへ委譲し、OUTPUT先と上書き前raw保存だけ変更。30trialの連番/bytes/SHAを照合。paletteの5PNGは全同SHA0d2f023dea73a5b14798f44c356b144de91a918b7e0130170a61b29b71a1da98で、親が暖機f/1・5000候補/遮蔽なしをすべて目視した。

### 採否・全失敗・残る限界

固定局所条件、通常製品の同値、正式速度を満たしたため技術受理。独立読取レビューは設計/差分/負例/全48process・1920marksを監査しP0/P1/P2なし。最終レビューも7組PNG/本文/原失敗と別完了観測、正式30rawから8指標の全samples/中央値/range、コピーexe/道具識別を再確認し阻害なし。R/acceptance-review.mdに記録。GUI比較JSONのvisualInspectionPending:trueは撮影直後の原記録として保持し、完了済みvisual-qa.mdを関連付ける。通常PR/必須CI/main同期/恒久収載/監査整理へ進む。

初回before buildのnesting違反、英語件名のhook拒否（commit未作成）、ADR索引merge競合と両方保持、初回GUI固定待ち失敗、誤ったpath/glob/読取範囲と訂正をjournal/rawへ保存。成功した検証は関連source/test/依存/環境が不変なのでpush/文書/mergeのために反復しない。文書には限定CNF-006/空白だけを追加する。任意viewport/資源枯渇/全字体/DPIの網羅、RSS/実capacity、GPU完了時間は未測。#365/#373は別の未解決として残す。
