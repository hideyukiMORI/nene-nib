# アーキテクチャ憲章 — NeNe Nib

> Status: normative（規範）/ 2026-09-15 初版
> 判断の根拠は [ADR 0001](adr/0001-strictness-is-mechanically-enforced.md)。
> 本書が沈黙している領域は [CODING_RULES.md](CODING_RULES.md) と公式の C++23 標準に従う。

---

## 0. この文書の読み方

**すべての規則は「機械強制」の状態を持つ。**

| 表記 | 意味 |
| --- | --- |
| **active** | 対応する検査を実行すると違反を機械が拒否する。実行対象と結果の再利用は QLT-001 / QLT-012 に従う |
| **planned** | 規範だが、まだ機械が見ていない。**この行に触れる変更は PR で明示的に自己レビューすること** |
| **不能** | 言語・道具の仕様上、機械では塞げない。塞げないことを明記して残す |
| **不採用** | 検討して採らなかった。理由を必ず併記する（再提案は同じ理由への反論から始める） |

🔴 **planned を active と書き換えないこと。** 未実装の強制を実装済みに見せるのは、
規約全体の信頼を壊す唯一の行為である。実装してから書き換える。
どの規則がいまどの状態かの正本は [QUALITY_GATES.md](QUALITY_GATES.md) の強制マトリクスであり、
文書整合検査が「本書に定義があるのにマトリクスに行が無い」状態を拒否する。

### 強制の実体は 5 層ある

| 層 | 実体 | 守るもの |
| --- | --- | --- |
| コンパイラ | clang-cl（C++23・`/WX`・`eng/targets.cmake` の警告集合）。非公開・網羅性・`old-style-cast`・VLA・target feature の無い SIMD | **不正な状態を「書けなく」する** |
| ビルドのモジュールグラフ | eng/architecture.json（`eng/targets.cmake` が宣言外の依存と OS ライブラリを拒否） | 層の依存方向 |
| API 単位の禁止 | `eng/symbols.py`（`llvm-nm` で中核の静的ライブラリの**未定義シンボル**を許可リストと照合。非決定入力＝ARC-007・並行性＝CPP-013・宣言外＝ARC-003）＋ `eng/conformance.py` の字句検査（リンカに見えない `std::atomic` と SIMD ヘッダの置き場） | **シンボル単位**で「呼べなくする」 |
| 静的解析 | clang-tidy（`.clang-tidy`。null 逆参照・未初期化・`new`/`delete`・optional・`const_cast`・`reinterpret_cast`・可変グローバル・複雑度） | 書けてしまうが書くべきでないこと |
| 規約検査 | `eng/conformance.py`（依存ゼロ） | **NeNe Nib として守るべきこと** |

道具が重なっても責務は重ねない。同じ規則を 2 か所で設定すると、片方を緩めても誰も気づかなくなる。

### C++23 (clang-cl) がどこまで届いたか

先行事例（NENE-PIXEL＝Kotlin / nene-recall＝Go / xi-tools＝Rust / NeNeCommander＝C# / NeNeClock＝Java）が
到達した点との比較。**本リポジトリの列は Phase 0 の実測結果（2026-09-15・114 記録）。再現手順は ADR 0003。**
ケース番号は [phase0-results.json](quality/phase0-results.json) の `id`。

| 規則 | Kotlin | Go | Rust | C# | Java | **C++23 (clang-cl)（本リポジトリの実測）** |
| --- | --- | --- | --- | --- | --- | --- |
| 不正状態を表現不能に・網羅性 | ある | 無い | ある | ある | ある | 不十分。`-Wswitch-enum` で分岐漏れ、`-Wcovered-switch-default` で網羅済みの `default` を拒否（M1-clang-*）。範囲外の enum 値はキャストで作れる（M1-clang-out-of-range-hole） |
| 公開状態は不変 | ある | 無い | ある | ある | ほぼある | 不十分。private への代入は拒否（M2-private-clang）。C スタイルキャストは `-Wold-style-cast`（M2-cstyle-cast-clang）。`const_cast` は clang-tidy が拒否（T4）。`mutable`（M2-mutable-hole）と公開 aggregate（T1-tidy-aggregate-hole）は通る |
| `null` の意味は一つ | 不十分 | 不十分 | ある | ある | 不十分 | 不十分。未初期化は拒否（M3-uninitialized-clang）。`nullptr` 逆参照は clang-analyzer が拒否（T2）。空の optional の `operator*` は通る（T3-*-star-hole）。値初期化 `Id{}` は通る（M3-value-init-hole） |
| 非公開コンストラクタ＋唯一のファクトリ | ある | 不十分 | ある | ある | ある | ある（M4-private-clang）。検証済み値のコピーは可能（M4-copy）。`malloc`＋キャストの偽造は通る（M4-forge-hole）ので許可シンボルから `malloc` を外す |
| 決定性（時刻・乱数・環境を読ませない） | planned | planned | ある | ある | ある | 言語では無い（M5-time-clang）。**リンカ段で補う**: `llvm-nm` の未定義シンボルに `_Xtime_get_ticks` / `_Query_perf_counter` / `_Random_device` / `__imp_GetTickCount` が現れる（L1 / L2） |
| 依存方向は物理的 | ある | ある | ある | ある | ある | 言語では無い（M6-win32-clang）。CMake の宣言グラフ＋File API の実グラフ照合と、`llvm-nm` の `__imp_*` / `__std_fs_*` 検査で補う |
| 抑制は例外であって道具ではない | waiver 台帳 | 理由必須 | forbid 層 | 全面禁止 | waiver 台帳 | 無い。`#pragma clang diagnostic ignored` で `-Werror` を抑制できる（M7-suppression-clang-hole）。waiver 台帳＋CNF-003 で補う |
| 並行性を一種類に固定できるか（Nib 固有） | — | — | — | — | UI スレッドのみ | 不十分。スレッド生成と同期原始はリンカに見える（TH1）。`std::atomic` は見えない（TH2）ので字句検査で補う |
| SIMD の許可範囲を機械で強制できるか（Nib 固有） | — | — | — | — | — | ある。target feature の無い関数の組み込み関数はコンパイルエラー（S1）。cl には無い（S3） |

🔴 **穴が残ることを書かずに厳格さを主張しない。** 塞げなかったものはここと第 1 節に「不能」として残す。

---

## 1. 憲章規則

### ARC-001 — 一つの意味に一つの正典経路

一つの意味には、正典となる型・所有者・振る舞いの経路が **ちょうど 1 つ**存在する。
並行するサービス、同義の別モデル、アダプタ固有の業務ロジック、同じ概念の別名を作らない。
**テキストの正本は 1 本**で、通常モードと Vim モードはキー割り当てとマウス規則の入れ替えだけで切り替わる（SPECIFICATION 第 2 節）。
置き換えが必要なら、呼び出し側の移行と旧経路の削除を**同じ変更で**行う。段階移行が要るときは期限付きの waiver に書く。

- 機械強制: **planned**（レビュー事項。「意味が同じか」は機械が判定できない）
- 補助: CNF-001（総称名の禁止）が「役割の分からない第 2 経路」の温床を減らす

### ARC-002 — 依存方向は物理的である

依存は [PROJECT_LAYOUT.md](PROJECT_LAYOUT.md) のグラフに従う。禁じた依存は「レビューで気をつけること」ではなく
**import できないこと**でなければならない。循環は許さない。

- 機械強制: **active** → `eng/targets.cmake`（configure で宣言外の依存と OS ライブラリを拒否）＋ `eng/conformance.py --build-dir`（File API の実グラフ・相対 include）。2026-09-15・Issue #3 で実ターゲット 6 つに対して結線。全ての include / マクロ迂回を塞ぐ証明は未完了

### ARC-003 — 中核はプラットフォームから独立している

中核（core / application）は Win32・DirectX・COM・ファイル・ネットワーク・現在時刻・既定ロケール・既定タイムゾーンを知らない。
プラットフォームは**型のあるポート**から入る。

🔑 標準ライブラリに同梱される枠組み（`std::filesystem`・`std::locale`・`<windows.h>` を含む SDK）は、依存を宣言しなくても include できてしまう。
その場合はビルドグラフでは塞げず、リンカ段のシンボル検査（`__std_fs_*` / `?_Init@locale@std@@…` / `__imp_*`）と字句検査層が塞ぐ。

- 機械強制: **active** → `eng/symbols.py --require core application`（`llvm-nm` ＋ `eng/symbol-allowlist.json`）＋ 字句検査。2026-09-15・Issue #3 で結線し、実ライブラリの反例で発火を確認

### ARC-004 — 状態には唯一の所有者がいる

| 状態 | 意味 | 所有者 | 変更経路 |
| --- | --- | --- | --- |
| テキスト正本 | 開いている各バッファの本文（piece table）と undo の履歴 | application（`EditorState`） | 利用者の意図（通常・Vim とも同じ編集操作の集合） |
| 編集モード | 通常 / Vim と、Vim の中のモード（normal / insert / visual / …） | application | トグルの意図と Vim エンジンの遷移 |
| カーソル・選択 | 本文上の anchor と caret | application（`EditorState::selection`） | 編集の意図と Vim エンジンの効果（ADR 0018） |
| Vim の保留・レジスタ・検索記憶・移動量の設定 | モード固有の入力状態、行内検索の対象と種別、半画面の明示行数 | application（`EditorState::vim`） | core の純関数の結果（ADR 0012 / 0019 / 0026） |
| 縦スクロール | 表示中の先頭行と表示行数 | application（`EditorState::scroll`） | スクロール・表示行数の意図と Vim の画面移動の効果（ADR 0019）。engine へ渡す view は借用で保存しない |
| ワーカーの結果（索引・ハイライト・md 変換） | 版番号付きの派生値 | application | ワーカー完了の意図。古い版は捨てる（ADR 0004） |
| 本文フォント・テーマ設定 | pt・フォント名・任意のテーマ（無しは OS 追従） | application（`EditorState::settings`） | `SettingsPort` の読込と、設定変更を保存した結果。UI は `EditorFrame` の値を写す（ADR 0020） |
| 履歴・ブックマーク | 保存される値 | application | 変更の意図と保存ポートの結果 |

同じ事実を 2 つの区分に独立して持たない。派生値は再計算するか、無効化を明示したキャッシュにする。

- 機械強制: **planned**

### ARC-005 — 可変性は隔離区画にのみ存在する

外から見える core / application の状態は不変とする。可変な状態を持ってよいのは次の区画だけである。

| 区画 | 何が可変か | なぜ必要か |
| --- | --- | --- |
| application の状態遷移 | 所有する現在状態を不変の次状態へ置き換える | 操作とワーカー完了の反映。公開値は不変 |
| src/ui/win32 | HWND・Direct2D / DirectWrite / DXGI の資源・IME の合成状態 | Win32 と GPU への反映。業務状態は複製しない |
| src/adapters/win32 | ファイル・設定・ジャンプリスト・ワーカーのスレッドとキュー | OS との入出力。取得値を検証済み値へ変換する |

隔離区画は、可変なコレクション・配列・ビルダを外へ返さず、検証済みの値型だけを受け取り、同じ入力に対して同じ結果を返す。
可変グローバル変数はどの区画にも置かない。

🔴 **区画を増やさない。** 必要になったら、この表と ADR を先に変える。区画が増えるたびに検査の除外が伸びるので、伸びること自体が警告になる。

- 機械強制: **planned** → 可変グローバルは clang-tidy `cppcoreguidelines-avoid-non-const-global-variables`（T6）。区画の意味はレビュー事項

### ARC-006 — 合成は明示的である

依存は合成ルート（`src/app` の `wWinMain`）で与える。サービスロケータ・リフレクションによる発見・クラスパス走査・
暗黙のシングルトン・可変なグローバル登録簿を使わない。DI フレームワークを入れるには ADR が要る。
SIMD の実装の選択（ADR 0006）とワーカーの起動（ADR 0004）も合成ルートが 1 回だけ行う。

合成ルートは**プロセスそのものの責務**も持つ。端末への出力とプロセスの終了コードを扱ってよいのは合成ルートだけである。
出してよいのは**起動できなかった理由 1 行**だけで、ログの手段として使わない。

- 機械強制: **planned**

### ARC-007 — 既定で決定的である

中核は、現在時刻・乱数・既定ロケール・既定タイムゾーン・環境変数・プロセス状態を**直接読まない**。
これらはポートか明示的な引数から入る。読んでよいのは **`src/adapters/win32` ただ 1 区画**であり、
そのことは「レビューの約束」ではなく、**その区画だけ禁止を適用しない**というビルド設定の差分として残す。

- 機械強制: **active** → `eng/symbols.py` の `nondeterministic` 分類（`_Xtime_get_ticks` / `_Query_perf_counter` / `_Random_device` / `getenv` / `__imp_GetTickCount` 等。L1 / L2）＋ 字句検査。2026-09-15・Issue #3。文字列・コメント・マクロ経由は字句では見ない
- 補足: テストソースにも同じ禁止を適用する。**テストが実時刻を読むことも決定性の破壊である。**

### ARC-008 — 境界は一度だけ検証する

外から来た値（保存済み設定・利用者入力・ファイルの bytes・IME の合成文字列）は、最初の自前の境界で検証し、その場で domain の型へ変換する。
検証前の生の値を中核へ流さない。UI と永続化で**違う規則の検証を二重に書かない**。

- 機械強制: **planned**（レビュー事項）
- 補助: CPP-007（唯一のファクトリ）が、検証を通らない値の生成を難しくする

### ARC-009 — 永続化の契約は版を持つ

保存形式（設定・履歴・ブックマーク・セッション）は明示的な版を持ち、移行規則を決める。読めない版は既定値へ黙って落とさず、型のある失敗として返す。
直列化 DTO は domain と分ける。開いているテキストファイルの改行と文字コードは読んだときの形を保ち、勝手に変えない（SPECIFICATION D8）。

- 機械強制: **planned**

### ARC-010 — 期待される結果は型で表す

期待される失敗・検証エラー・非互換・device lost は、閉じた結果型（`std::expected` / `enum class`）で返す。例外はプログラム上の欠陥・下位層の想定外に限る。
`nullptr`・boolean・文字列・汎用例外で複数の業務的な結果を表さない。

- 機械強制: **planned** → `[[nodiscard]]`＋`-Wunused-result`（C7）。失敗の意味分類はレビュー

### ARC-011 — UI は状態を描き、意図を発行する

UI 層は、application が作った表示値を Direct2D で描き、利用者の操作（キー・マウス・IME）を意図として渡すだけを行う。
整形の判断・状態遷移・永続化の呼び出しを UI に置かない。

- 機械強制: **planned**（`src/ui/win32` → adapters の依存は ARC-002 が拒否。意図と表示値の分離はレビュー事項）

### ARC-012 — アーキテクチャは利便性に優先する

道具の近道・ライブラリの便利機能・最適化が、状態の所有者・モジュールグラフ・契約の境界を迂回することを許さない。
**速さのための近道も同じ**で、SIMD・ワーカー・キャッシュは決められた区画の中でだけ行う（ADR 0004 / 0006）。
アーキテクチャが間違っているなら ADR で明示的に変える。ユーティリティを装った局所的な例外を作らない。

- 機械強制: **不能**（判断そのものが対象。QLT-010 の手続きで担保する）

---

## 2. 強制を置く順番

規則は、確実に効く**いちばん早い層**に置く。

1. 言語の型・可視性・網羅性
2. ビルドシステムのモジュールグラフ
3. コンパイラオプション（警告はエラー）
4. API 単位の禁止（リンカ段のシンボル）
5. 静的解析とアーキテクチャテスト
6. 規約検査（`eng/conformance.py`）
7. CI のマージゲート
8. まだ自動化できない判断だけをレビューへ

レビューだけの強制は一時的な状態であり、[QUALITY_GATES.md](QUALITY_GATES.md) に `planned` として残す。
