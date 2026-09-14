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
