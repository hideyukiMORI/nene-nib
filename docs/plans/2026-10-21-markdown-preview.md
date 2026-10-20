# Markdown プレビュー（FR-007）の実装計画

2026-10-21 / Issue #755 / hide の指示（2026-10-20「md プレビューの実装計画を詰めて」）/ 設計席（Nibリナ）。
調査は `D:\NeNeNib\outputs\20261021\survey-markdown.md`（調査席・main `d2d5b60` を読むだけ）。本文の数字で「推測」と書いたものは実測していない。

この計画は作業の切り分けと判断点の保存で、SPECIFICATION（D7 / FR-007）や受理済み ADR を置き換えない。見た目・所有・並行性の変更は各段の ADR で決める。[ロードマップ](2026-10-11-release-roadmap.md)の段階 5 と、[初版の範囲案](2026-10-12-release-scope-and-large-file-probe.md)の Markdown の行を具体化する。

## 0. 要点

| 問い | 設計席の決め | 根拠 |
| --- | --- | --- |
| md4c の取り込み | **vendoring**（`third_party/md4c/{md4c.c,md4c.h,LICENSE.md}`・3 ファイルの SHA-256 を規約検査が照合） | ADR 0003 の文言そのまま。`conformance.py` は git の木を読むので FetchContent の取得物は照合できない。`docs/PROJECT_LAYOUT.md` が `third_party/<name>` を先に決めている |
| parse の出力 | **平らな「ブロックの列＋装飾の区間」**（`MarkdownDocument{blocks}`・`PreviewBlock{kind, level, quote_depth, list_depth, marker, source_begin, text, spans}`） | 1 ブロック = 1 DirectWrite layout。見えている範囲の二分探索とスクロールの同期（`source_begin`）がそのまま書ける |
| frame | `EditorFrame.lines` は触らず、**別の `PreviewFrame`**（見えているブロックの範囲・最初のブロックと中の送り・古さの印・失敗） | `LineView` の選択・検索・IME の欄はプレビューで意味を持たない |
| 描画 | **プレビュー専用の保持 `PreviewTextLayouts`**（ui/win32・1 ブロック 1 layout・`DrawTextLayout`） | `BodyTextLayouts` は行高が一定（ADR 0114 決定 4）で見出しに合わない。range の書式つき layout は ADR 0073 の字形の収集に載らない |
| 置き場 | **split の other の枠**（受け身の表示の 2 つ目の種類 `PreviewView`） | ロードマップが「編集＋プレビュー」の 2 枠を名指し。ADR 0148 決定 2 / 8 / 11 の受け身の表示に種類を足すだけ。`DisplayForm` 案は打ちながら見られず、別窓案は窓の契約（ADR 0131〜0133）を 2 倍にする |
| 更新 | **worker で全文を parse・券で古い結果を捨てる**（ADR 0062 の形・`MarkdownPort{parse, collect}`） | 見えている範囲だけの parse は参照リンクの定義・フェンス・リストの遅延継続で意味が壊れる |
| 工程 | **6 段・ADR 2 本・約 8〜9.5 席日（推測）** | §4 |

## 1. 既存の決定との境界

| 決定 | 中身 | この計画での扱い |
| --- | --- | --- |
| D7 / FR-007 | WebView なし・md4c（MIT・C）・Direct2D の自前描画・CommonMark のサブセット・md4c の導入は別 Issue | そのまま。段 1 を別 Issue にする |
| ADR 0003 | `third_party/md4c` の別 target に `/W4 /WX /utf-8`（C17）だけ・ソースを書き換えない・tag `release-0.5.2` / commit `729e6b8` / 3 ファイルの SHA-256 を規約検査が照合・ADR・許可リスト・規約の表の 3 点 | そのまま。「厳格集合とサニタイザ以外は当てず」の読みは **厳格集合は当てない・サニタイザ（Debug の ASan / UBSan）は当てる** と設計席が読む。理由: 信用できない入力を読むパーサこそ検査の対象で、ソースを書き換えずに当てられ、Debug の exe は ASan の runtime を全体で結ぶ。段 1 の ADR に 1 行書く |
| 初版の範囲案 | 見出し・段落・強調・リンク・リスト・引用・区切り・コードの 8 つ。表・脚注・数式・図・画像の取得・HTML の解釈・プレビュー上の直接編集は後続 | そのまま。`MD_DIALECT_COMMONMARK`（flag 0）で 8 つは全部出る。表の flag は立てない |
| 構文色と Markdown プレビューは別の機能 | ソースの色分けをプレビューの完成扱いにしない | そのまま。ソースの色分けは [構文の色分けと LSP の計画](2026-10-21-syntax-and-lsp.md) |
| ADR 0148（split・PR #688 の中・試用待ち） | other は受け身の表示 `PaneView`。F6 / `Ctrl-w w` は隣の枠へ。`ClosePane` / `ScrollPane`。session には保存しない | プレビューは other の 2 つ目の種類。**プレビューの枠は操作中にならない**（F6 / `Ctrl-w w` / クリックは飛ばして 1 行知らせる）。閉じるは `ClosePane` を流用。スクロールはホイール（`ScrollPane`）。session に保存しない |
| ADR 0062 / CLAUDE.md §2 | worker 1 本・型のある port・完了は中身の無い合図 → `WorkCompleted` → `collect()`。`WorkCompleted` はタブの歩き・知らせ・変換・Vim の待ち・`.`・undo を動かさない | parse の完了も `WorkCompleted` で受ける（新しい意図を足さない）。parse の要求はタイマーの意図からだけ出す |
| 契約 `verify_quiet_paths` | 起動・開く・切り替え・保存・打鍵の道で port を呼ばない | `MarkdownPort` も同じ契約に載せる。プレビューを開いていない文書では呼ばない |
| ADR 0114 | `BodyTextLayouts` は本文の保持。選択 / 検索で共有 layout の書式を変えない | プレビューの layout は別の保持。`BodyTextLayouts` と `BodyTextPainter` に入れない |
| ADR 0117 決定 2 | `SyntaxPalette` の 10 役割は「後続の構文分類が同じ役割を選ぶ」 | プレビューの色もこの 10 役割から選ぶ（段 3 の採用案）。`Palette` にトークンを足さない |
| ADR 0140 | `DisplayForm{text, grid}` は文書ごとの値 | 触らない |

## 2. 施主に確かめる点（ADR の前に・CLAUDE.md §2）

| # | 問い | 設計席の推し | 決まると変わる所 |
| --- | --- | --- | --- |
| M-1 | 置き場は split の other の枠でよいか | はい（§0） | 段 4 の形。いいえなら (b) 別窓 か (c) `DisplayForm` を比較し直す |
| M-2 | 初版の構文は範囲案の 8 つで止め、表・打ち消し・タスクリストは後続でよいか | はい | 段 2 の型（`PreviewBlockKind` の値）と段 3 の描画 |
| M-3 | プレビューの上限の大きさと、超えたときの扱い | **段 2 の probe で測ってから決める**。仮の値は 8 MiB で、超えたら「プレビューは 8 MiB までです」と 1 行知らせて枠は空のまま | 段 5 の知らせ・契約 |
| M-4 | 更新の待ち（打鍵が止まってから何 ms で parse を出すか） | **200 ms**（ui のタイマー・`AutosaveTick` と同じ合図の形の意図 `PreviewTick`） | 段 5 のタイマー。短いほど worker を占める |
| M-5 | リンクは下線と色だけで開かないでよいか | はい（開くのは後続・`ShellExecute` は adapters・確認の UI が要る） | 段 3 の描画 |
| M-6 | 見出し・コード・リンク・引用の色を `SyntaxPalette` の役割に当てる採用案 | 段 3 で `/design` の案を 3 つ出して選んでもらう（仮: 見出し = keyword・コード = string・リンク = function・引用 = comment・水平線 = `panel_border`） | 段 3 の採用案 `docs/design/` |

## 3. 形（各段の ADR で確定する）

### 3.1 取り込み（段 1）

- `third_party/md4c/` に 3 ファイル。`CMakeLists.txt` を `LANGUAGES C CXX` にし、`eng/toolchain.ps1` が `CC=clang-cl` も固定する。
- `eng/targets.cmake` に `nenenib_third_party_target`（`/W4 /WX /utf-8`・`C_STANDARD 17`・`NENENIB_CXX_OPTIONS` は付けない・Debug のサニタイザは付ける）を 1 本。モジュールの検査（`architecture.json` に無いモジュールは FATAL）は共有する。
- `eng/architecture.json` にモジュール `third_party/md4c`（依存なし）を足し、core の依存に 1 本足す。ARC-002 の表と `docs/PROJECT_LAYOUT.md` の「現在は無い」を直す。
- clang-tidy は `CMAKE_CXX_CLANG_TIDY` だけなので C の target に掛からない。`md4c.h` を include するのは core の 1 ファイル（`MarkdownParse.cpp`）だけで、`SYSTEM` の include で受けて `HeaderFilterRegex '.*'` から外す（段 1 で実測して確かめる）。
- SHA-256 の照合は CNF の新規則（`eng/conformance-rules.json` に 3 ファイルの値）と `tests/conformance` の正例・反例。`eng/symbol-allowlist.json` に `third_party/md4c` のモジュールの欄（`malloc` `realloc` `free` `qsort` `memcpy` など md4c の未定義は md4c の lib のもので、core の許可は崩さない）。
- `THIRD_PARTY_NOTICES.txt`（リポジトリ直下・`eng/package-release.py` で ZIP に入る）に md4c の節（名前・tag・commit・URL・MIT 全文）。`eng/test-package-release.py` が中身を照合しているかを段 1 で確かめる。
- 今の LLVM（`eng/tool-versions.json`）で `/W4 /WX` 0 件を再測する（ADR 0003 の 0 件は Phase 0 の実測）。

### 3.2 core の parse（段 2）

- `[[nodiscard]] std::expected<MarkdownDocument, MarkdownFailure> parse_markdown(std::string_view, const StopPredicate&)`。純関数・例外なし（ARC-010 / CPP-005）。`MarkdownFailure` は閉じた enum（`parser_rejected`・`too_large`（`MD_SIZE` は `unsigned`）・`stopped`）。
- 打ち切り: md4c はコールバックの非 0 で止まる。core は「止めるか」を問う型のある述語を引数に取り、adapters の worker が自分の印（`std::atomic` は `src/adapters/win32` だけ・CNF-009）を読む lambda を渡す。新しい券が来たら前の parse を止める。
- `source_begin`: md4c はブロックの位置を渡さないので、そのブロックで最初に来た `text` の pointer − 入力の先頭から引く。文字の無い水平線は直前のブロックの終わりで補う。**`MD_TEXT_NORMAL` の pointer が常に入力を指すかは未確認**（段 2 の単体テストで確かめ、違えば行番号の数え直しに替える）。
- 入力は連続 1 本。`TextBuffer` は piece table なので、UI スレッドで `segments()` から連続の `std::string` を 1 回作り `shared_ptr<const std::string>` で worker に渡す（16 MiB で数 ms・推測）。
- 型は `MarkdownDocument` `PreviewBlock` `PreviewSpan` `PreviewBlockKind` `PreviewStyle` `MarkdownFailure` の 6 ファイル（CNF-002）。
- 単体テスト: CommonMark の例を数十（8 つの構文の範囲）。probe: `tests/performance` に **16 MiB の parse の費用**（md4c に公称値は無い。50〜300 ms と推測）。

### 3.3 描画（段 3）

| 要素 | 描き方 |
| --- | --- |
| 見出し | layout 全体に `SetFontSize`（本文の pt × 段ごとの倍率）＋`SetFontWeight(BOLD)` |
| 太字・斜体 | `SetFontWeight` / `SetFontStyle` を `DWRITE_TEXT_RANGE` で |
| 行内コード・コードブロック | 等幅（格子の書式と同じ字体・ADR 0140 決定 5 / D58）＋背景の矩形（`HitTestTextRange` → `FillRectangle`） |
| 引用 | 左の縦線・字下げ・comment の色 |
| 箇条書き | marker を文字に入れて字下げ |
| リンク | `SetUnderline`＋色。開かない（M-5） |
| 水平線 | `DrawLine` 1 本 |
| 画像 | `[画像: alt]` の文字（限界） |
| HTML ブロック | 生の文字のまま・色だけ変える（限界） |

- `PreviewTextLayouts`: 鍵はブロックの文字列＋spans＋幅＋DPI＋字体＋pt。見えているブロックだけ作り、前 frame と同じ鍵は再利用。高さは `GetMetrics().height` をブロックごとに覚えて上から足す。
- 見た目の採用案は `/design` で 3 案（`docs/design/2026-10-xx-markdown-preview-look.md`）。確認は `eng/verify-window.py --capture` の PNG を設計席が見る。段 3 は split 無しでも仮の口（固定の 1 文書を other の代わりに本窓へ描く検査用の経路）で撮れる。

### 3.4 置き場と更新（段 4・5）

- ADR 0148 の `other` を `PaneView` と `PreviewView` の閉じた和型にし、frame の `other` も `PaneFrame` と `PreviewFrame` の和型にする（`std::visit`）。
- 開く / 閉じる: `EditorOperation` に `TogglePreview` を 1 値（鍵は `operation_bindings` の表・F1 の一覧に載る・ADR 0078）。Ex は `:preview` を 1 語（後続でよい）。
- 追従: プレビューは操作中の文書に追従する。スクロールの同期は片方向（本文の `first_visible` の行頭 offset → `source_begin` の二分探索）。プレビューのホイールは `ScrollPane`。
- 更新: 打鍵 → `PreviewTick`（M-4）→ `MarkdownPort::parse(ticket, text)` → worker → 合図 → `WorkCompleted` → `collect()` → 券が今のものなら `MarkdownDocument` を差し替え。古い券は捨てる（ADR 0062 決定 15）。parse 中は前の結果を出したまま、ステータスには出さない（roadmap「古い解析結果を表示しない」は「新しい本文に古い結果を**確定として**出さない」の意味で、差し替わるまで前の絵を残す）。
- 検索: hlsearch の面はプレビューに塗らない（プレビューの文字列は本文と違う）。プレビュー内の検索は後続。
- IME: プレビューの枠は入力を受けない（窓の入力は 1 つ・ADR 0148 決定 10）。

## 4. 工程

| 段 | 中身 | 主な所 | 席日（推測） | ADR | 前提 |
| --- | --- | --- | --- | --- | --- |
| 1 | md4c の取り込み（§3.1）・`/W4 /WX` の再測・CNF の新規則・許可リスト・`THIRD_PARTY_NOTICES.txt`・`QUALITY_GATES.md` | eng・third_party・docs | 1〜1.5 | **A**（取り込みと parse の型・ADR 0003 の履行） | なし（今すぐ可） |
| 2 | `parse_markdown` と型・`source_begin`・打ち切り・単体テスト・**16 MiB の probe** | core・tests | 1.5〜2 | A に含める | 段 1 |
| 3 | `PreviewTextLayouts`・8 つの構文の描画・色の採用案（`/design`・M-6）・仮の口で撮影 | ui/win32・docs/design | 2 | **B**（置き場と更新）に入れる | 段 2・M-5・M-6 |
| 4 | other に `PreviewView`・`TogglePreview`・F6 / `Ctrl-w` の飛ばし・片方向の同期・契約 `--preview` | application・ui | 1.5〜2 | B | **split の 3 PR（#688 → #687 → #692）の main 統合**・M-1 |
| 5 | `MarkdownPort` と worker・券・`PreviewTick`・打ち切り・上限の知らせ・`verify_quiet_paths`・速さ（既存 8 本＋プレビュー更新までの時間を別に記録） | application・adapters | 1.5 | B | 段 4・M-3・M-4 |
| 6 | 受理（D41: 独立レビュー・速さのゲート・CI・hide の試用） | — | 0.5 | — | 段 5 |

計 6 段・ADR 2 本・約 8〜9.5 席日（推測）。ADR の番号は main の次の空き（0148 は split の branch が使う）。
**段 1〜3 は split を待たずに進められる**。段 4 以降は split の main 統合が前提。

Issue は段ごとに 1 本（親 Issue 1 本の下に 6 本）。段 1 は「md4c の導入」の Issue として ADR 0003 が予約している別 Issue に当たる。

## 5. 確かめ方

| 対象 | 方法 |
| --- | --- |
| 取り込み | `/W4 /WX` 0 件・CNF の SHA 照合の正例・反例・symbols の allowlist・`check.ps1 -Full`（外部コードを足すので全件の理由がある） |
| parse | 単体テスト（CommonMark の例・`source_begin`・打ち切り）・16 MiB の probe の値を記録 |
| 描画 | `verify-window.py --capture` の PNG（見出し・強調・コード・引用・リスト・リンク・水平線の 1 文書）・DPI 2 段 |
| 置き場 | 契約 `--preview`（開く・閉じる・F6 の飛ばし・同期・古い券の破棄）・`--background-work`（`WorkCompleted` が歩きを切らない） |
| 速さ | 既存 8 本に退行なし（単打・長い行・16 MiB 連打は Markdown の文書でも測る）・プレビュー更新までの時間を別に記録（ロードマップの表）。採用は D41 |

## 6. 残る不確かさ

- md4c の `MD_TEXT_NORMAL` の pointer が入力を指すか（段 2 で確かめる）。
- 16 MiB の parse の実時間（公称なし）。M-3 の上限はこの値で決める。
- DirectWrite で range ごとに字の大きさを変えたときの行高（段 3 で実測）。
- worker 1 本を数百 ms 占める間、自動退避とフォルダの列挙が待つ。打ち切りで短くするが、上限（M-3）が効く。
- `AddChunk` は末尾の chunk を伸ばすので、worker が piece を直接読むのは危ない → 連続の写しを UI スレッドで作る（段 2 の形で閉じる）。
