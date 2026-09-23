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
| CNF-010 | `tests/vim/fixtures.json` の 1 文字を変える（fixture 名の末尾 `s` → `z`。長さは変えない）・生成物から SHA の行を消す・本数だけ違う行を書く | `python eng/conformance.py`（1 文字の実測）/ tests/conformance の fixture_digest_checks 正例・反例 | 2026-09-18: 1 文字変えると `CNF-010: tests/vim/VimFixtures.hpp: recorded digest 15758fd4… is not 3277c07b…; regenerate` で `Conformance: 1 violation(s)`・終了 1。戻すと `0 violation(s)`・終了 0。記述なし・本数不一致・正例（oracle の `header()` が書いた行）は `tests/conformance` の 5 件（Issue #44） |
| CNF-011 | `tests/vim/fixtures.json` を古い形に戻す（indent 2・空の `"settings": []` を足す・キー順を入れ替える・区切りの後に空白・`\u` エスケープ・1 行に全部・CRLF・末尾改行を消す）・未知のキー / キー不足 / 壊れた JSON / 不正な `viewport` | `python -X utf8 eng/conformance.py`（実リポジトリの実測）/ tests/conformance の fixture_format_checks 正例・反例 | 2026-09-22: 実物の 1 件に `"settings":[]` を足すと `CNF-011: tests/vim/fixtures.json: line 3: '  {"name":"h-with-a-count","text":"alpha","keys":"$3h","sett...' is not the canonical '  {"name":"h-with-a-count","text":"alpha","keys":"$3h"},'; run eng/vim-oracle.py --format` と CNF-010 の SHA 不一致で `Conformance: 2 violation(s)`・終了 1。末尾改行を消すと `no newline at the end of the file`（行の指摘は重ねない）で同じく終了 1。戻すと `0 violation(s)`・終了 0。反例 15 通りと正例（oracle の `canonical_fixtures_json` が書いたバイト列そのまま）は `tests/conformance` の 5 件（Issue #98） |
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
