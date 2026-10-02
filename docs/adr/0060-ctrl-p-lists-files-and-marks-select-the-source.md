# ADR 0060 — Ctrl+P の一覧は 1 つの候補の列で、行頭の記号が出どころを絞る

- 状態: 受理（設計席 2026-09-30・Issue #258 / #259・**施主決定 D28〜D30**）
- 日付: 2026-09-30
- Issue: #258（統合の一覧の土台）・#259（履歴）
- 影響する規則: FR-006 / FR-016 / FR-010 / ARC-001 / ARC-003 / ARC-004 / ARC-007 / ARC-010 / CPP-002 / CPP-003 / CPP-005 / CPP-011 / CPP-012 / QLT-001 / QLT-012 / QLT-013 / QLT-014
- 前提（仕様の決定）: **D5・FR-006「ファイルアクセスは Ctrl+P 1 本に統合。開いているタブ・ブックマーク・履歴・同じフォルダを 1 つの窓でファジー検索。行頭の記号で絞り込む」**・FR-016「Ctrl+P の `:` 接頭辞からも同じ一覧」・D12（採用した見た目: 行は種別のアイコン・名前・場所・補足、検索欄の右に記号の案内）・D1（速さ）・D24（ファイルを指定した起動を軽いままにする）・D26（無くなったファイルは 1 行知らせる）。本 ADR はこれらを変えない。
- 前提（ADR）: [ADR 0023](0023-command-palette-and-shared-input-session.md)（Ctrl+P の面と入力の共有。決定 5 の「開く時は `:` を入力に置く」は本 ADR が置き換える）・[ADR 0057](0057-tab-destination-is-one-pure-function.md)（タブの一覧。決定 7 の「2 つ目の出どころ」「開いた直後の入力は空」は本 ADR が置き換える）・[ADR 0056](0056-tabs-park-inactive-documents-behind-the-active-one.md)（開く: 同じファイルのタブへ切り替える）・[ADR 0059](0059-session-remembers-tabs-and-loads-them-on-view.md)（port と adapter と保存の形の置き方・知らせの文言）・[ADR 0013](0013-startup-shows-the-window-before-the-device.md)（起動）

## 文脈

仕様は Ctrl+P の中身（開いているタブ・ブックマーク・履歴・同じフォルダ）と「行頭の記号で絞り込む」を決めているが、作る順・キーボードで打てる記号・履歴の決まりは決まっていなかった。2026-09-30 に設計席が選択肢と良い点・悪い点を説明し、施主が決めた。

| 決定 | 内容 |
| --- | --- |
| D28 | 作る順は、履歴 → 同じフォルダ → ブックマーク（Ctrl+D）→ Vim の `:e` `:b` `:ls`。1 本目で「開いているタブ＋最近開いたファイル」が Ctrl+P に出る |
| D29 | 絞り込みの記号は打てる記号にする: `*` ブックマーク・`@` 履歴・`/` 同じフォルダ・`#` 開いているタブ・`:` 設定。面の案内に絵（★ ◷）と鍵を並べて出す |
| D30 | 履歴から選んだファイルが無くなっていたら、ステータスバーに 1 行知らせて履歴から外す（D26 と同じ動き・窓は止めない） |

設計席が決めて施主に伝えたこと（異論なし）: 履歴は 100 件・何も打っていないときの並びは採用した画のとおり（タブ → ブックマーク → 履歴 → 同じフォルダ）・すでに開いているファイルは履歴に重ねて出さない・面の中の日本語入力は 1 本目の後の独立の Issue。

現物の調査（`out/probes/probe-ctrlp-2026-09-30.md`）で分かったこと:

- 面の出どころは、開く入口で決まる閉じた enum（`commands` / `tabs`）で、開いた後に切り替える仕組みは無い。Ctrl+P は入力に最初から `:` を入れて設定のコマンドだけを出す。
- 絞り込みは ASCII の大文字と小文字を区別しない部分列の照合 `match_score`（バイト単位）。候補は保持せず、1 つの意図で 2 回以上作り直す。今の規模（コマンド約 22・タブ 256 まで）では問題にならない。
- ブックマーク・履歴・フォルダの列挙の実装は無い。`FilePort` は `read` `write` `same_file` の 3 つ。`core::FilePath` に正規化と大文字小文字を無視した比較は無く、同じファイルかは `FilePort::same_file` が決める。
- ファイルを開く失敗はダイアログ（`MessageBoxW`）。前回のタブの復元の失敗は 1 行の知らせ（`unreached_message`）。
- core と application で `std::stable_sort` は ARC-003 のシンボルの検査に落ちる。`std::sort` と添字は通る。
- 面が開いている間、IME の変換も確定も捨てている。

## 決定

**Ctrl+P の面が持つ候補は、開くときに作る 1 つの列。入力の行頭の記号が、その列のどの出どころを見せるかを決める。記号の表は core の 1 つ。設定のコマンドは `:` の出どころ。履歴は閉じたファイルの新しい順で、別のファイル `history.v1` に覚える。**

1. **候補の列（core）**: `CommandPalette` は、開くときに受け取った候補の列を `std::shared_ptr<const std::vector<CommandChoice>>` で持つ（1 打鍵で写すのは参照だけ）。入口で固定の `CommandPaletteSource` は無くす。`CommandPalette::opened(候補の列, 最初の入力, 最初に選ぶ位置)` の 1 本で開く。
2. **出どころの記号の表（core・1 つ）**: 閉じた enum `PaletteScope`（`files` すべてのファイルの候補 / `tabs` / `history` / `commands`）と、`constexpr` の表 `palette_marks`（1 行は 記号の 1 文字・`PaletteScope`・案内に出す名前）。表に載せるのは実装した出どころだけ: #258 で `#` タブ と `:` 設定、#259 で `@` 履歴。後続の Issue が `/` 同じフォルダ と `*` ブックマーク を 1 行ずつ足す。
   - `palette_query_of(入力) → { PaletteScope scope; std::string_view query; }` が、入力の先頭の 1 文字を表で引く。表にあればその出どころと残りの文字、無ければ `files` と入力の全体。呼び出し元で先頭の文字を見ない。
   - 検索欄の案内の文字列も同じ表から作る（`palette_mark_hint()`）。記号を足すと案内も増える。
   - 表に無い記号（今の `@` `*` `/`）は、ただの検索の文字として扱う。
3. **候補の出どころの印（core）**: `CommandChoice` に `std::optional<PaletteOrigin> origin` を足す。`PaletteOrigin` は閉じた enum（`tab` / `history`。後続で `folder` `bookmark`）。Ex のコマンドの候補は印なし。出どころで絞るのは `origin` と `PaletteScope` を比べる 1 つの関数、行の右の補足の文言は `palette_origin_label(origin)`（「開いているタブ」「履歴」）。
4. **絞り込みと順（core の純関数 1 本）**: `listed_choices(候補の列, scope, query)`。今の `tab_list_choices` を置き換える。
   - `scope` に入る候補だけを残す（`files` は印のある候補の全部）。
   - `query` が空なら、列の順のまま。
   - 空でなければ、名前（`label`）に `match_score`。名前に当たらなければ「場所＋名前」（`detail` と `label`）に `match_score` して、固定の罰点を足す（名前で当たった候補が先）。どちらにも当たらなければ落とす。
   - 並べ替えは点の小さい順、同点は列の順。`std::stable_sort` は使わず、(点, 元の位置) を比べる（ARC-003）。
   - 照合はバイト単位のまま。日本語の名前の照合（コードポイントの境目）は、面の中の日本語入力の Issue で直す。
   - **後の変更（[ADR 0061](0061-the-palette-takes-ime-input-and-opens-with-it-off.md)・#264）**: 照合はコードポイントの境目で行う。
5. **`choices()`**: `palette_query_of(input)` の出どころが `commands` なら今の `palette_choices(input)`（先頭の `:` を剥がす・結果は今と同じ）、それ以外は `listed_choices`。`filled`（`set fontsize=` を補う）は候補の列を持ったまま入力を `:<コマンド>` にする。
6. **開く入口（application）**: 候補の列を作るのは controller の 1 本（`palette_entries()`）。Ctrl+P（`OpenCommandPalette`）は入力が空・選ぶのは先頭。「∨」と `:tabs`（`OpenTabList`）は同じ列で入力が `#`・選ぶのはアクティブなタブの行。開いている間にもう一度送ると閉じるのは今までどおり。
   - 列の順は、開いているタブ（帯の順・今の `tabnext N` の候補のまま）→ 履歴（新しい順・#259）。後続でブックマークはタブの後ろ、同じフォルダは最後（採用した画の順）。
   - 「∨」の一覧の入力欄に `#` が見えるようになる。`#` を消すと全部の候補になる。
7. **確定（application）**: `CommandChoiceKind` に `open` を足す（#259）。`open` の候補の `command` はファイルのパスの文字列で、`submit_palette` の `switch` が `open_listed(path)` へ写す（`default` なし・CPP-002）。タブの候補は今までどおり `execute` の `tabnext N`。
   - `open_listed` は `OpenDocument` と同じ 1 本の開く道を通る（同じファイルのタブがあれば切り替える → 何も書いていない無題に開く → 新しいタブ・ADR 0056 の決定 5）。違うのは失敗の告げ方だけ: ダイアログではなく 1 行の知らせ「開けませんでした: <名前>」（ADR 0059 の `unreached_message`）。開く道の本体を 1 つの関数に出し、`accept(OpenDocument)` と `open_listed` が結果の告げ方を決める。
   - 失敗が `not_found` のときは、その行を履歴から外す（D30）。ほかの失敗（大きすぎる・読めない・文字コード）は知らせるだけで、履歴に残す。
   - 後続の Vim の `:e <file>` は、Ex の評価がファイルの要求を返し、controller が同じ `open_listed` を呼ぶ。
8. **履歴（#259）**:
   - 値と port（application）: `FileHistory { std::vector<core::FilePath> files; }`（新しい順）。`HistoryPort` は `read() → std::expected<FileHistory, FileHistoryFailure>`（ファイルが無ければ空の履歴）と `write(const FileHistory &) → std::expected<void, FileHistoryFailure>`。`FileHistoryFailure` は閉じた enum。名前を `HistoryFailure` にしないのは、core に undo の端に使う同じ名前の型（`src/core/HistoryFailure.hpp`）があるため（#259 の工程 1 で include が取り違えられてビルドが落ちた）。1 型 1 ファイル。`EditorPorts` に `HistoryPort &history`。
   - 記録の純関数（application）: `history_recorded(履歴, パス, FilePort)` は、同じファイル（`FilePort::same_file`）を除いて先頭へ置き、100 件で切る。`history_forgotten(履歴, パス, FilePort)` は同じファイルを外す。
   - **書くのは閉じたときだけ**: パスのあるタブを閉じたとき（`CloseTab`）と、窓が閉じるとき（`EndSession`。`window_closed` は開いているパスのあるタブを使った順の古いほうから記録して、最後に見ていたタブが先頭になる。`last_tab_closed` は閉じたその 1 本）。開いている間は、そのファイルはタブの候補として一覧に出ている。起動と「開く」の道には、履歴の読み書きを足さない（D1・D24・ADR 0013）。
   - 書くたびに `read` → 記録 → `write`（ほかの窓が書いた分を失わない）。履歴を状態に持たない。結果の失敗は捨てる（履歴は無くても動く。窓を止めない）。
   - 読むのは面を開くとき（`palette_entries()`）。開いているタブと同じファイル（`same_file`）は履歴の候補から除く。読めなかったら履歴の候補なしで開く（知らせない）。
   - 履歴の候補は、名前がファイルの名前（`FilePath::file_name`）・場所がフォルダ（`tab_folder_for`）・`kind` は `open`・`origin` は `history`。
   - 保存の形（adapters に閉じる・ADR 0020 の決定 3）: `%LOCALAPPDATA%/NeNeNib/history.v1`（`beside_local_settings`）・UTF-8・`version=1` の次に 1 行 1 パス（新しい順）。上限は 1 MiB。版が違う・壊れている・絶対でないパスは全体を読まなかったことにする（次の記録で書き直される）。書きは `FilePort::write`。
   - 道具: `eng/window_driver.py` の `start` が起動の前に消すもの（`session.v1`）に `history.v1` を足す（本物の `LOCALAPPDATA` と同じ場所では消さない・ADR 0059 の決定 7 と同じ）。
9. **描画（core の配置と ui）**: 行は今の「名前・場所」に、右端の補足（`palette_origin_label`・muted）を足す。場所は補足の手前で切る。検索欄の右に記号の案内（`palette_mark_hint()`・muted）を、入力が空のときだけ出す。幅と位置は `palette_layout` の純関数が決める。色は今のトークンだけ。
   - 採用した画の「種別のアイコン」と履歴の「3 分前」は後続（アイコンは線の絵の描画が要る・時刻は port から入れる）。それまで種別は右端の補足の文字で分かる。
10. **1 打鍵の重さ**: 候補の列はタブ 256 本までと履歴 100 件。今の「必要なたびに作り直す」形のままにする。同じフォルダの Issue で、列挙の上限と候補の結果を持つ形を決める（数千件になるため）。
11. **縦切り（D28・どの後も main は動く状態）**:
    1. **#258 統合の一覧の土台**: 決定 1〜6 と 9。Ctrl+P は空で開いて開いているタブを出す。`#` と `:`。
    2. **#259 履歴**: 決定 7 と 8。`@`。
    3. 同じフォルダ（`/`・`FilePort` か専用の port にフォルダの列挙）。
    4. ブックマーク（`*`・Ctrl+D・FR-010 の前半）。
    5. Vim の `:e` `:b` `:ls`。
    - 1 本目の後の main: 設定のコマンドは Ctrl+P の後に `:` を打って出す（FR-016 のとおり）。NORMAL の `:` は今までどおり。
12. **範囲の外（後続）**: 面の中の日本語入力とコードポイントの境目の照合・行の種別のアイコン・「3 分前」の補足・履歴を消すコマンドと件数の設定・カーソルの位置を履歴に覚えること・タスクバーのジャンプリスト（FR-010 の後半）・Ctrl+Enter・Ctrl+P の面の実機の検査を `eng/verify-window.py` に足すこと。

## 強制

- 契約（#258 の分）: **active**（`tests/unit/CommandPaletteTests.cpp` と `tests/unit/TabsTests.cpp`。入口は scope `nib_tests --command-palette` と `--tabs` と既定の実行・CTest `nib_unit`）。
  - `verify_palette_marks`: 記号の表（`#` `:` と表に無い `@` `*` `/`・先頭でない `#`）から出どころと残りの文字を決めること・案内の文字列・補足の文言。
  - `verify_listed_choices`: 出どころごとの絞り込み・空の入力は列の順・名前で当たった候補が場所だけで当たった候補より先・同点は列の順・当たらない候補は落ちる。
  - `verify_palette_sources`: 空の入力で全部・`#` で渡した位置を選ぶ（範囲の外は先頭）・`#` を消すと全部・`:` の後ろが `choices_for`（今のコマンド一覧）と同じ結果・補った後も列が残ること。
  - `verify_palette_entries_controller`: Ctrl+P は入力が空で帯の順のタブ（印 `tab`）・「∨」の入口 `OpenTabList` は入力が `#` でアクティブのタブを選ぶ・Enter でタブが切り替わる・`:colo` の補完と `:colorscheme` の確定が今までどおり。
  - `verify_palette_notes_geometry` / `verify_palette_hint_view`: 行の右端の補足と検索欄の案内の配置（96 / 120 dpi・狭い欄で幅 0）と、案内は入力が空のときだけ出ること（決定 9）。
  - `TabsTests.cpp` の `verify_tab_list_rows` / `verify_tab_list_entries`: 一覧の入力が `#`・行に印 `tab`・`OpenTabList` と `:tabs` と Ctrl+P の空の入力が同じ列を開くこと・確定でタブが切り替わること。
- 契約（#259 の分）: **active**（`tests/unit/HistoryTests.cpp` と `tests/unit/CommandPaletteTests.cpp` と `tests/adapters/HistoryAdapterTests.cpp`。入口は scope `nib_tests --history` と `--command-palette` と既定の実行・CTest `nib_unit` と `nib_histories`）。
  - `verify_recorded_order` / `verify_recorded_same_file` / `verify_recorded_limit` / `verify_forgotten`: 記録は先頭へ置き既にあれば 1 つだけ前へ動かす・同じファイル（`same_file`）は記録した綴りで 1 つ・100 件で古いほうを切る・外すと同じファイルの綴りがすべて外れる。
  - `verify_scripted_round_trip`: 替え玉 `ScriptedHistory` の読み書き・書きの失敗・読みの失敗・回数。
  - `verify_close_records`: パスのあるタブを閉じると読み 1・書き 1 で先頭に入る（脇のタブもアクティブのタブも）・最後の 1 本・無題・範囲の外は書かない。
  - `verify_end_session_records`: `window_closed` は最後に見ていたタブが先頭・まだ読んでいないタブも読まずに入る・`last_tab_closed` はその 1 本・無題だけの窓は履歴を読み書きしない。
  - `verify_record_failures`: 書けなくてもタブ・知らせ・`last_failure` は不変で一覧の書きは 1 回・読めない履歴は閉じたファイルから書き直す。
  - `CommandPaletteTests.cpp` の `verify_palette_marks`: `@` が履歴を選ぶ・案内「# タブ　@ 履歴　: 設定」・補足「履歴」。
  - `verify_palette_history_rows`: タブの後ろに履歴を新しい順・開いているファイルは出ない・行の名前 / 場所 / `open` / `history`・`@` と `#` の絞り込み・読むのは面を開く 1 回・読めなければタブだけで知らせない。
  - `verify_palette_history_open`: 履歴の行は新しいタブで開き書かない・`not_found` は「開けませんでした: <名前>」の 1 行でダイアログなし・書き 1 回で外れる・`too_large` は知らせだけで残る。
  - `HistoryAdapterTests.cpp` の `verify_round_trips` / `verify_line_ends` / `verify_rejected` / `verify_limits` / `verify_adapter_round_trip` / `verify_adapter_failures`: `history.v1` の往復（LF・新しい順）・BOM と CRLF・壊れた入力（空・版・空行・相対・制御文字・壊れた UTF-8）の拒否・1 MiB と行数の上限・無いファイルは空の履歴・版違い / 壊れ / フォルダ / 場所なしの失敗。
- 閉じた和型の写し漏れ（#258）: `PaletteScope` `PaletteOrigin` は **active**（`default` の無い `switch`・CPP-002）。`PaletteScope` は `src/core/CommandChoice.cpp` の `in_scope`・`src/core/CommandPalette.cpp` の `CommandPalette::choices`・`src/application/CommandInput.cpp` の `completions_of`、`PaletteOrigin` は `src/core/CommandChoice.cpp` の `scope_of` と `palette_origin_label`。値を足すとコンパイルが落ちる。
- 閉じた和型の写し漏れ（#259）: **active**（`default` の無い `switch`・CPP-002）。`CommandChoiceKind` の `open` は `src/application/EditorController.cpp` の `EditorController::submit_palette`。`PaletteScope::history` は `src/core/CommandChoice.cpp` の `in_scope`・`src/core/CommandPalette.cpp` の `CommandPalette::choices`・`src/application/CommandInput.cpp` の `completions_of`、`PaletteOrigin::history` は `src/core/CommandChoice.cpp` の `scope_of` と `palette_origin_label`。値を足すとコンパイルが落ちる。
- core と application が OS とファイルに触れないこと: **active**（既存の `eng/symbols.py`）。
- 起動と「開く」の道に履歴の読み書きが無いこと: **active**（`tests/unit/HistoryTests.cpp` の `verify_quiet_paths`: ファイル引数つきの起動・前回のタブを戻す起動・`OpenDocument`（同じファイルへの切り替えを含む）・`SwitchTab`・`NewTab`・`SaveDocument`・打鍵で、替え玉の port の読みと書きがどちらも 0 回。履歴の行から開く道の書き 0 回は `verify_palette_history_open`）。速さの 6 本は設計席が測った（gate-proofs の #259 の節）。
- 実機の確認: **planned**（設計席が画で確かめる。機械の必須 check ではない）。#258 は設計席が 1 回限りの撮影で確かめた（gate-proofs の #258 の節）。#259 も設計席が 1 回限りの撮影で確かめた（gate-proofs の #259 の節）。
- fixture: **不能**（oracle の対象ではない）。既存の fixture は不変（`eng/protected-diff.py`）。

## 結果

得られるもの: Ctrl+P を開くとファイルの一覧が出て、名前を数文字打って Enter で戻れる。出どころを足すのは、表に 1 行・候補の列に 1 種類・`switch` が落ちた所を直す、の決まった形になる。設定のコマンドとタブの一覧と後続の出どころが、同じ面・同じ絞り込み・同じ確定の道を通る。
失うもの・残る穴: Ctrl+P で設定を変えるには `:` を 1 つ打つ（今は開いた時点で入っている）。「∨」の一覧の入力欄に `#` が見える。強制終了のときは、その回に閉じていないファイルは履歴に入らない。履歴に時刻は無い（「3 分前」は出ない）。名前が記号で始まるファイルは、先頭に空白を打ってから探す（照合は空白を無視する）。日本語の名前は、面の中の日本語入力の Issue まで矢印で選ぶ。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 出どころを開く入口で固定のまま、入口を増やす | 仕様は「1 つの窓・行頭の記号で絞り込む」（D5）。入口ごとに面と絞り込みが増える |
| 記号を `★` `◷` のまま | キーボードで打てない（D29） |
| ファイルの候補を Ex の `edit <path>` として確定する | パスの空白と 256 バイトの入力行の上限と Vim の `%` `#` の展開が絡む。Ex の評価を通す前に `:e` の引数の決まりが要る。開く道は controller の `open_listed` の 1 本にして、後続の `:e` がそこへ来る |
| `CommandChoice` の動作を `std::variant` にする | 候補は描画へ毎フレーム写す値で、今の `kind` と `command` の形に値を 1 つ足せば `switch` が落ちて写し漏れが分かる。和型への作り直しは差分が試験の全体に広がる |
| 履歴を開いたときに書く | 起動引数のファイルを開く道（窓を見せる前）にファイルの読み書きが入る（D24・ADR 0013）。開いている間はタブの候補として出ているので、使う人から見える一覧は同じ |
| 履歴を状態に持ち、窓を閉じるときに 1 回だけ書く | 窓を 2 つ開いていると、後から閉じたほうが先のほうの履歴を消す。閉じるたびに読んで足す形なら失わない |
| 履歴を `session.v1` か `settings.v1` に足す | 書く時機と中身が違う（一覧は窓を閉じるときに全部を書き換える・設定は変更のたびで外の変更を上書きしない）。版を上げると古いファイルが読めなくなる |
| タブの候補を最近使った順に並べる | 「∨」の一覧は帯の順（ADR 0057・D20）。1 つの列に 2 つの順を持たせない。最近使った順は Ctrl+Tab（D23） |
| 無くなったファイルをダイアログで知らせる | 施主が 1 行の知らせを選んだ（D30） |
