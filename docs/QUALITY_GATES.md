# 品質ゲート — NeNe Nib

> Status: normative（規範）/ 2026-09-15 初版
> 本書は「いま何が機械で守られているか」の**正本**である。
> 規範の本文は [ARCHITECTURE_CONSTITUTION.md](ARCHITECTURE_CONSTITUTION.md) と
> [CODING_RULES.md](CODING_RULES.md)、機械側の実体はこの文書に対応する。

`pwsh -NoProfile -File ./eng/check.ps1` がローカルと CI の**唯一の完了定義**である。
個別の道具は診断のために単独で回してよいが、その成功は完了の代わりにならない。

---

## 1. ゲートの整合性規則（QLT-0xx）

### QLT-001 — ゲートは一つ

ローカルの検証と CI は**同じ 1 つのコマンド**（`pwsh -NoProfile -File ./eng/check.ps1`）を呼ぶ。CI のワークフローに品質判断のロジックを書かない。
第 2 の完了定義を文書化しない。道具の版は 1 か所（eng/tool-versions.json）にだけ書く。

- 機械強制: **planned**（CI が `pwsh -NoProfile -File ./eng/check.ps1` を呼ぶだけであること）

### QLT-002 — 警告は失敗する

コンパイラの警告・整形差分・静的解析の指摘はすべて失敗にする。重大度の引き下げは禁止。

- 機械強制: **active**（clang-cl `/WX`、clang-tidy WarningsAsErrors、clang-format --Werror。`eng/prove-gates.py` が毎回のゲートで実ツールの反例と復帰を確かめる）

### QLT-003 — baseline を作らない

lint・静的解析・アーキテクチャ・依存・テスト・**速さの基準値**のいずれについても baseline を持たない。既存の違反を「無かったこと」にする経路を作らない。
**唯一の例外機構は期限付きの狭い waiver である。**

- 機械強制: **planned** → CNF-005

### QLT-004 — 整形は検査であって修復ではない

整形は設定から決定的に決まる。CI がソースを書き換えて成功させることはしない。整形の正本は 1 つ（設定ファイルは置かないか 1 つだけ）。

- 機械強制: **active**（eng/check.ps1 の clang-format --dry-run --Werror。反例と復帰は `eng/prove-gates.py`）

### QLT-005 — ローカルと CI は同一

CI で必要なゲートは、すべてローカルで実行できる。検査を足したくなったら、まずローカルのゲートに足す。

- 機械強制: **planned**

### QLT-006 — アーキテクチャは実行可能

モジュール境界・import 規律・状態の経路は機械が検査する。散文だけのアーキテクチャ規則は、**「不能」と明記されない限り未完成**として扱う。

- 機械強制: **planned**

### QLT-007 — カスタムゲートには negative proof が要る

自作の検査は、「わざと違反させた入力で、意図した規則によって落ちること」と「正しい入力では通ること」の両方を示すまで信用しない。
証拠は [quality/gate-proofs.md](quality/gate-proofs.md) に記録する。ゲートを変えたら証拠も同じ変更で更新する。

🔴 **設定が在ることは、検査が効いていることの証明ではない。** 前例で 2 度、設定だけあって検査が効いていない状態が証明手順で発覚した。
Nib でも clang-tidy の `bugprone-unchecked-optional-access` が MSVC STL の `operator*` を見ないことを Phase 0 で実測した（T3-tidy-unchecked-optional-star-hole）。

- 機械強制: **planned**（検査器自身の正例・反例テストをゲートに結線する）

### QLT-008 — 振る舞いの変更はテストを伴う

振る舞いを変えたら、いちばん狭い安定した境界にテストを足す。不具合の修正は、可能なら**先に落ちる回帰テスト**を書いてから直す。
Vim の振る舞いは oracle から生成した fixture が期待値である（ADR 0005）。

- 機械強制: **planned**（レビュー事項。カバレッジ下限＝QLT-009 が部分的に代替する）

### QLT-009 — カバレッジは下げられない

中核の分岐カバレッジ下限を 90% とする。閾値は上げてよいが下げてはならない。下げるには ADR が要る。
**置いていない層は「置いていない」と書く。**

- 機械強制: **planned**（`eng/coverage.py` / `eng/coverage-policy.json`。clang-cl の C++ で `llvm-cov` が動くことは実測した（V1）が、測る中核がまだ無い。Phase 3 で結線）

### QLT-010 — ゲートの弱体化はアーキテクチャ変更

重大度・除外・閾値・モジュール境界・必須 CI ジョブ・速さの許容退行を変えるには明示的な根拠が要る。MUST 規則を弱める変更には ADR か waiver が要る。
**無関係な作業を通すために検査を切ることは禁止。**

- 機械強制: **不能**（判断そのものが対象。PR の手続きで担保する）

### QLT-011 — 依存は再現可能

道具の版は `eng/tool-versions.json` にだけ書き、`eng/toolchain.ps1` が実際の版と照合して違えば落とす。取り込む依存は tag と SHA-256 で固定する。

- 機械強制: **planned**（道具の版は照合する。Windows SDK 全体の固定と取り込み依存の照合は未完了）

### QLT-012 — 検証の頻度は変更に従う

反復中は最も狭い検査を使う。**フルゲートは PR の Draft → Ready の時点で必ず通す。**
CI の起動条件は `ready_for_review`（＋非 draft の `synchronize` / `edited`）。draft の間にフルゲートを回さない。
head が動いたら Draft に戻して再度 Ready にする。古い成功 SHA・スキップされたジョブ・狭い検査は、通ったフルゲートの代わりにならない。

- 機械強制: **planned**（ruleset の必須 check と strict up-to-date）

### QLT-013 — 環境依存の主張は正直に名付ける

単体テストは表示サーバ・実機・実 GPU・実 OS 資源・実物の Vim を必要としない。単体テストが通ったことを「実機で動く」の証拠として扱わない。
表示・DPI・IME・実 GPU の遅延・oracle の再生成を伴う確認は [quality/gate-proofs.md](quality/gate-proofs.md) に環境と手順を書いて別に記録する。

- 機械強制: **planned**

### QLT-014 — 速さの退行はゲートが落とす

ベンチ 3 本（起動→最初の描画 / キー→画面 / 1 GB を開く）を `eng/` の計測スクリプトで測り、`eng/perf-reference.json` の基準値からの
退行でゲートを落とす（[ADR 0006](adr/0006-speed-gate-simd-and-table-driven-dispatch.md)）。許容退行を広げる（基準値を下げる）には ADR が要る。
CI では同じ CI 機での相対退行だけを見て、絶対値の目標は施主の実機で記録する。

- 機械強制: **planned**（ベンチも基準値もまだ無い。Phase 3 の縦切りで最初の値を測ってから結線する）

---

## 2. 規約検査の規則（CNF-0xx）— `eng/conformance.py`

汎用の lint が見ないもの、つまり**このリポジトリ固有の規約**を検査する。依存ゼロで書き、ゲートから常に呼ぶ。
各規則には正例・反例の単体テストを付け、それもゲートに結線する（QLT-007）。

### CNF-001 — 禁止された総称名

`class` / `struct` / `enum` / `using` の名前とモジュール（ディレクトリ）名を字句検査する。CamelCase と snake_case の両方の語尾を見る。マクロで生成した名前は未対応。

- 対応する規則: CPP-010
- 機械強制: **planned**（`eng/conformance.py`）

### CNF-002 — 1 ファイル 1 主要宣言

名前空間直下の型定義の数と、主要型名とファイル名の一致を字句検査する。テンプレート等を含む主要宣言の完全な分類は未完了。

- 対応する規則: CPP-011
- 機械強制: **planned**（`eng/conformance.py`）

### CNF-003 — 抑制と waiver の結び付け

`#pragma clang diagnostic` / `#pragma warning` / `_Pragma` / `__pragma` / `NOLINT` 系のファイル単位の抑制を拒否。行単位（`NOLINTNEXTLINE`）には直前行の有効な waiver と検査名を要求する。

- 対応する規則: CPP-015
- 機械強制: **planned**（`eng/conformance.py`）

### CNF-004 — waiver 台帳の整合

命名・必須メタデータ・索引・Scope・期限を検査。期限は UTC 日付で、Expires 当日まで有効。解除条件本文の妥当性はレビュー。

- 対応する規則: CPP-015
- 機械強制: **planned**（`eng/conformance.py`）

### CNF-005 — baseline とゲート無力化の禁止

禁止された設定ファイル名（`baseline` / `suppressions`）と既知の無力化オプション（`/WX-`・`/W0-3`・`/wd`・`-Wno-error`・`-w`・`ignoreFailures`・`continue-on-error` 等）を検査。
名指しの `-Wno-<診断名>`（`-Wno-switch-default` / `-Wno-language-extension-token`）は無力化ではなく規則の選択なので対象外。任意のスクリプトによる迂回を網羅したとは扱わない。

- 対応する規則: QLT-003
- 機械強制: **planned**（`eng/conformance.py`）

### CNF-006 — 文書整合

規則 ID の重複定義・未定義参照・マトリクス行と本文状態・active の証明行・相対リンク・未置換の雛形値を検査する。文言の真偽そのものは機械では判定しない。

- 対応する規則: QLT-007
- 機械強制: **active**（`eng/conformance.py`。正例・反例は `tests/conformance` がゲートで回る）

### CNF-007 — 検査設定の読込

`eng/config-bindings.json` の設定と参照先を照合する。文字列参照だけでは実行を証明しないため、整形・lint・CMake・リンカ検査は実ツールの反例も実行する（`eng/prove-gates.py`）。

- 対応する規則: QLT-007
- 機械強制: **planned**（`eng/conformance.py`）

### CNF-008 — TODO と FIXME に Issue 番号

製品ソースと eng 内のコードにあるタスクマーカーは同じ行の Issue 番号を必須とする。検査器の正例・反例 fixture は対象外。

- 対応する規則: QLT-008
- 機械強制: **active**（`eng/conformance.py`。正例・反例は `tests/conformance` がゲートで回る）

### CNF-009 — 並行性と SIMD のヘッダの置き場

`src/adapters/win32` 以外の `src/` で並行性ヘッダ（`thread` `mutex` `atomic` `future` `condition_variable` `semaphore` `latch` `barrier` `stop_token` `process.h` `synchapi.h`）を
include したら CPP-013。`src/core/simd/` 以外の `src/` で SIMD ヘッダ（`immintrin.h` `intrin.h` `xmmintrin.h` 等）を include したら CPP-018。
リンカに見えない `std::atomic`（TH2）と、コンパイラの target feature 検査（S1）の置き場の側を字句で補う。ヘッダ名だけを見るので、
Interlocked 系の組み込みや `volatile` は見ない。tests/ は対象外。

- 対応する規則: CPP-013 / CPP-018
- 機械強制: **active**（`eng/conformance.py`。正例・反例は `tests/conformance` がゲートで回る。ヘッダ名の字句だけを見る）

🔴 **検出語は検査器のソースに直書きしない**（検査器が自分自身を違反として報告する。前例: xi-tools 初版で 7 件の自己検出）。
🔴 **テストソースは検査対象から外す**（テストは意図的な違反を書く場所）。

---

## 3. 強制マトリクス

規範の各規則が、いまどの層で守られているか。**この表が実装と食い違ったら merge を止める。**
CNF-006 が「本文に定義があるのにここに行が無い」を拒否する。

| 規則 | 状態 | 機械強制の実体 |
| --- | --- | --- |
| ARC-001 | planned | レビュー事項（CNF-001 が温床を減らす） |
| ARC-002 | planned | eng/targets.cmake（configure）＋ eng/conformance.py --build-dir。製品ターゲットが揃ってから |
| ARC-003 | planned | eng/symbols.py（llvm-nm ＋ eng/symbol-allowlist.json）＋ 字句検査。中核のライブラリが生まれてから |
| ARC-004 | planned | |
| ARC-005 | planned | clang-tidy avoid-non-const-global-variables（可変グローバルのみ） |
| ARC-006 | planned | |
| ARC-007 | planned | eng/symbols.py の nondeterministic 分類（src/adapters/win32 だけ対象外）＋ 字句検査 |
| ARC-008 | planned | レビュー事項 |
| ARC-009 | planned | |
| ARC-010 | planned | `[[nodiscard]]`＋`-Wunused-result` |
| ARC-011 | planned | |
| ARC-012 | 不能 | QLT-010 の手続き |
| CPP-001 | planned | |
| CPP-002 | planned | clang-cl `-Wswitch-enum` / `-Wcovered-switch-default` |
| CPP-003 | planned | `-Wold-style-cast`・clang-tidy pro-type-const-cast・misc-non-private-member-variables |
| CPP-004 | planned | `-Wuninitialized`・`-Wzero-as-null-pointer-constant`・clang-tidy NullDereference / unchecked-optional-access |
| CPP-005 | planned | `[[nodiscard]]`＋`-Wunused-result` |
| CPP-006 | planned | |
| CPP-007 | planned | 非公開コンストラクタ。`malloc` は許可シンボル外 |
| CPP-008 | planned | |
| CPP-009 | planned | clang-tidy pro-type-reinterpret-cast・CNF-003 |
| CPP-010 | planned | CNF-001 |
| CPP-011 | planned | CNF-002 |
| CPP-012 | planned | clang-tidy readability-* |
| CPP-013 | planned | eng/symbols.py の concurrency 分類 ＋ CNF-009 |
| CPP-014 | planned | eng/symbols.py（setlocale / locale） |
| CPP-015 | planned | CNF-003 ＋ CNF-004 |
| CPP-016 | planned | `-Werror=vla-cxx-extension`・clang-tidy NewDeleteLeaks / use-after-move・ASan / UBSan |
| CPP-017 | planned | platformLibraries（GDI を結べない） |
| CPP-018 | planned | clang-cl の target feature 検査 ＋ CNF-009 |
| GIT-001 | planned | PR テンプレート＋CI（`Closes #N`） |
| GIT-002 | planned | ruleset＋CI（head ブランチ名） |
| GIT-003 | planned | `.githooks/commit-msg`＋CI（全コミットと PR タイトル） |
| GIT-004 | planned | PR テンプレート＋ruleset（squash-only） |
| QLT-001 | planned | |
| QLT-002 | active | clang-cl・clang-tidy・clang-format と実ツール反例（eng/prove-gates.py） |
| QLT-003 | planned | CNF-005 |
| QLT-004 | active | clang-format --dry-run --Werror |
| QLT-005 | planned | |
| QLT-006 | planned | |
| QLT-007 | planned | |
| QLT-008 | planned | レビュー事項 |
| QLT-009 | planned | eng/coverage.py（未結線） |
| QLT-010 | 不能 | PR の手続き |
| QLT-011 | planned | eng/toolchain.ps1 |
| QLT-012 | planned | |
| QLT-013 | planned | |
| QLT-014 | planned | eng/perf-reference.json（未作成） |
| CNF-001 | planned | eng/conformance.py / tests/conformance |
| CNF-002 | planned | eng/conformance.py / tests/conformance |
| CNF-003 | planned | eng/conformance.py / tests/conformance |
| CNF-004 | planned | eng/conformance.py / tests/conformance |
| CNF-005 | planned | eng/conformance.py / tests/conformance |
| CNF-006 | active | eng/conformance.py / tests/conformance |
| CNF-007 | planned | eng/conformance.py / tests/conformance |
| CNF-008 | active | eng/conformance.py / tests/conformance |
| CNF-009 | active | eng/conformance.py / tests/conformance |

---

## 4. `pwsh -NoProfile -File ./eng/check.ps1` に入っている層

| 層 | 目的 | 実体 |
| --- | --- | --- |
| コンパイル | 型安全・網羅性・警告ゼロ | clang-cl（C++23）・`eng/targets.cmake` の警告集合 |
| 整形 | 文字列としての正本 | clang-format / .clang-format |
| API 禁止 | 時刻・乱数・環境・OS import・並行性・宣言外の外部シンボル | `eng/symbols.py`（`llvm-nm` ＋ `eng/symbol-allowlist.json`。アーカイブ内と宣言済み依存で解決してから照合。ライブラリが 0 個の間は何も守っていない）。字句検査は補助 |
| 静的解析 | null 逆参照・未初期化・new/delete・optional・キャスト・可変グローバル・複雑度 | clang-tidy / .clang-tidy |
| アーキテクチャ | 宣言グラフ・実ターゲット・ソース所有 | targets.cmake / architecture.json / CMake File API |
| 規約検査 | NeNe Nib 固有 | `eng/conformance.py` |
| 検査自身のテスト | 規約検査・シンボル検査・カバレッジ判定・実ツールの正例・反例 | unittest / eng/prove-gates.py |
| 単体テスト | C++23 基盤のスモーク（製品の単体テストは Phase 3 から） | CTest / tests/build（ASan / UBSan 付き・`-fno-sanitize-recover=all`） |
| カバレッジ | 中核の検証密度 | 未結線（QLT-009 planned） |
| 速さ | 3 本のベンチの退行 | 未結線（QLT-014 planned） |
| 依存 | 道具の版と実行時依存 0 | tool-versions.json / architecture.json / `/MT`（R1） |

---

## 5. マージゲート

`main` は次を必須とする。

- Pull Request 経由であること
- `pwsh -NoProfile -File ./eng/check.ps1` が成功していること（CI の必須 check）
- 生成物のドリフトが無いこと（作業ツリーが汚れないこと）
- 期限切れ waiver が無いこと
- 未解決のレビュー指摘が無いこと
- squash merge のみ

`main` への直接 push・force push・ブランチ削除は禁止する。

🔴 **リポジトリ設定（ruleset）を適用したら、その事実を [quality/gate-proofs.md](quality/gate-proofs.md) に記録する。**
**設定していないものを「必須になっている」と書かない。**
