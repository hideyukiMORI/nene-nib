# ゲート発火の証明 — NeNe Nib

> Status: 記録 / 最終実測 2026-09-15（Issue #1・Phase 2。製品モジュールは無く、検査基盤だけが対象）
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

**復帰の確認**: 2026-09-15。P1〜P4・P8・P13〜P21 は `eng/prove-gates.py` が各反例の直後に元へ戻して build / configure / clang-format / symbols を再実行し、
終了コード 0 を確かめた（25 反例）。P5〜P7・P9〜P12・P22・P23 は正例テストが同じ suite にある（83 テスト）。最後にフルゲート全体が終了コード 0 で
`NeNe Nib full gate passed` を出した。

**除外側の確認**: 例外区画について「禁止が効いていること」と「唯一の窓口が通ること」の両方を見る。

| 区画 | 適用しない禁止 | 呼んでいる禁止 API | 結果 |
| --- | --- | --- | --- |
| `src/adapters/win32` | 決定性・OS import・スレッド | （製品モジュールが無い。字句検査の正例は tests/conformance で `src/adapters/win32/` の `<thread>` と `time(0)` が通ることを確認） | シンボル検査の実ライブラリは Phase 3 で実測 |

🔴 `eng/symbols.py --build-dir build` は現在 `0 libraries checked, 0 violation(s)` で通る。**中核の静的ライブラリが無いので何も守っていない。**
ARC-003 / ARC-007 / CPP-013 が planned のままなのはこのため。P1・P17〜P19 は単一オブジェクトへの部分証明である。

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
| PR 必須 | 未設定 | |
| 必須 check | | |
| strict up-to-date | 未設定 | |
| force push / ブランチ削除の禁止 | 未設定 | |
| squash のみ | リポジトリ設定 `allow_squash_merge` のみ true・`delete_branch_on_merge` true（`gh api repos/hideyukiMORI/nene-nib` で読み戻し） | 2026-09-15 |

🔴 **設定していないものを「必須になっている」と書かない。** 設定したら `gh api` で読み戻して記録する。

---

## 5. 環境依存の確認（QLT-013）

<!-- 表示・実機・実 GPU・oracle を伴う確認は、単体テストとは別にここに環境と手順を書く -->

### 5-a. Phase 0 の環境依存の実測（Issue #1・2026-09-15）

環境: 上記。4 モニタ（120 / 144 / 168 DPI）の端末だが、Phase 0 は窓を作っていない。

- D1: WARP の D3D11 device で Direct2D / DirectWrite / DXGI（flip model・waitable swap chain・composition）/ DirectComposition / DWM を呼び、描画して Present するまで終了 0。実 GPU では測っていない
- V2: `C:\Program Files\Vim\vim91\vim.exe`（9.1・2024-01-02）を `-u NONE -i NONE -N -n -es -S probe.vim` で 2 回起動し、同じ出力を得た。CI に Vim は無い
- MD1: md4c release-0.5.2 を GitHub から clone して測った。ネットワークが無ければ未実測になる設計
