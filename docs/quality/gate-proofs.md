# ゲート発火の証明 — NeNe Nib

> Status: 記録 / 最終実測 2026-09-15（Issue #1・Phase 2 と Issue #3・最初の縦切り。core / application の実ライブラリに対して ARC-002 / ARC-003 / ARC-007 / CPP-013 / QLT-009 / CNF-007 を結線した）
> 根拠となる規則: QLT-007（カスタムゲートには negative proof が要る）

**検査は「落ちること」を見るまで信用しない。** 各ゲートについて、最小の違反を仕込んだ状態で
意図した規則 ID によって失敗すること、そして元に戻すと `pwsh -NoProfile -File ./eng/check.ps1` が緑に戻ることを実測する。
ゲートを変えたら、この記録も同じ変更で更新する。

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
