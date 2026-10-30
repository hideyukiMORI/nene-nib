# ADR 0174 — 通常モードの検索・置換・行へ移動は、Ex の入力行と Vim の照合器を共用する

- 状態: 受理（施主決定 D71・2026-10-28 夜「メモ帳系で足りないものは全部入れる。Vim と被る鍵は Vim 優先で代替を用意」。実装は 1 席・受理は D41 の形）
- 日付: 2026-10-29
- Issue: #1046
- 規則: FR-002 / FR-031、ARC-001 / ARC-003 / ARC-010 / ARC-012、CPP-002 / CPP-005 / CPP-011 / CPP-012、QLT-014、ADR 0022 / 0023（入力行は `CommandInput` の 1 本）/ 0032（照合器 `VimPattern`・E486・折り返しの報せ）/ 0037（`hlsearch` の面）/ 0041 / 0043（incsearch と Ctrl-G / Ctrl-T）/ 0052（`Edit.restore`）/ 0078（操作の表）/ 0168（`:s` の族は 1 回の `Edit`）/ 0154（右端の項目）

## 文脈

通常モード（メモ帳の役・FR-002）には検索も置換も行へ移動も無い。検索は Vim の `/` だけで、Ctrl+F は何も起こさない。hide の点検（2026-10-28 夜）で見つかり、D71 で「全部入れる」と決まった。土台は揃っている: 入力行は Ex と検索で 1 本（`CommandInput`・`InputLinePrompt` は `ex` `palette` `search_forward` `search_backward`）、照合器は `VimPattern`（`magic` の部分集合・`literal_bytes`）、当たりの塗りは `hlsearch` の面、折り返しの報せ、操作の表（F1 の一覧と鍵）。

## 決定

1. **操作は 5 つ、鍵は通常モードだけの行。** `EditorOperation` に `find`（「検索」・Ctrl+F）・`find_next`（「次を検索」・F3）・`find_previous`（「前を検索」・Shift+F3）・`replace`（「置換」・Ctrl+H）・`go_to_line`（「行へ移動」・Ctrl+G）。`operation_bindings` の行は `OperationModes::ordinary`（Vim モードでは Ctrl+F は半画面送り・Ctrl+H は `<BS>`・Ctrl+G は Vim のまま・D71）。`OperationKey` に `f` `h` `g` `f3` を足す（Shift+F3 は `KeyChord{shift}`）。F1 の一覧（Ctrl+P の `?`）には両モードで載る。実行は `EditorWindow::run_operation` の 1 本（ADR 0078）。

2. **Vim モードの代替は入力行の前置き。** 一覧から `find` を選ぶと `/` の入力行（`search_forward`）、`replace` は `:` の入力行に `%s/` を前置き（確定は ADR 0168 の `:s`）、`go_to_line` は `:` の入力行を空で開く（`:N`・#847）。`find_next` / `find_previous` は `n` / `N` と同じ `SearchHop`。Vim モードに新しい状態を足さない。

3. **通常モードの入力は同じ入力行に 3 つのプロンプトを足す。** `InputLinePrompt` に `find`（「検索: 」）・`replace_with`（「置換後: 」）・`go_to_line`（「行: 」）。`CommandInput` は 1 本のまま（文字の編集・IME・履歴は Ex と共用・ADR 0061 の構えは `search_forward` と同じ）。置換は 2 段: Ctrl+H は `find` のプロンプトで開き（前回の検索語を置く）、Enter で `replace_with` へ進み、`replace_with` の Enter は**今の当たりを 1 件置き換えて次の当たりへ**、Ctrl+Enter は**全件を 1 回の `Edit`**、Esc は閉じる。`find` のプロンプトの Enter は次の当たりへ（入力行は閉じない・Esc で閉じる）。置換の 2 段の状態は `EditorState.find`（`FindState{std::string pattern; std::string replacement; FindPhase phase;}`・窓に 1 つ・保存しない）。

4. **照合は `VimPattern` の 1 本。通常モードの語は literal。** 通常モードの入力は `magic` を解釈せず、core の純関数 `literal_pattern(std::string_view) -> std::string`（`\` `/` `.` `*` `[` `~` `^` `$` を `\` で逃がす）で `VimPattern` の文字列にしてから同じ照合器に渡す。大小は `ignorecase` / `smartcase`（#1041）の旗を共用する。#1041 が先に入っていなければ大小を区別する（Vim の既定）。空の入力は何もしない。照合器を 2 本にしない（ARC-012）。追補（#1127 のレビュー R3-1）: 後ろ向き（Shift+F3・Ctrl+T）は区切りの `?` も逃がす。関数は `literal_pattern(text, direction)` の 1 本で、`*` `#` の逃がしも同じ関数を呼ぶ。

5. **最後の検索は Vim と同じ 1 つ。** 通常モードの検索語は、Vim の最後の検索（`"/`・`hlsearch` の面・`n` `N` の向き）に**同じ値として**置く（literal に逃がした形）。`hlsearch` が on なら当たりは同じ `search` の面で塗られ、キャレットを含む当たりは `accent` の枠（ADR 0037）。モードを切り替えても検索語は続く。`incsearch` は通常モードの `find` にも効く（入力中の当たりを preview・ADR 0041 の形をそのまま）。

6. **次・前は `SearchHop`。折り返しの報せも同じ。** F3 / Shift+F3 / `find` の Enter は既存の意図 `SearchHop{relative}` を通る（新しい意図を足さない）。見つからなければ E486 の文を 1 行、端を越えれば「search hit BOTTOM, continuing at TOP」の文をそのまま出す（ADR 0032 の文・訳さない）。

7. **置換は 1 件も全件も `Edit` 1 回。** 1 件: 今の当たり（キャレットを含む当たり・無ければ次の当たりへ先に動く）を `replacement` で置き換える `Edit` 1 つ（`restore` はそのときのキャレット・undo 1 単位）→ 次の当たりへ `SearchHop`。全件: core の純関数 `substituted_all(text, pattern, replacement) -> std::optional<Edit>`（当たりを前から順に置き換えた 1 回の `Edit`・当たりが無ければ値なし）。この関数は ADR 0168 の `:%s` の全件と同じ 1 本で、先に入った方が生み、後の方が使う（2 本にしない）。置換語に `\n` `&` `\1` は無い（literal・限界）。

8. **行へ移動は `:N` と同じ着地。** 入力は `行` か `行:桁`（10 進・全角は受けない）。行は `line_jump`（#847・ADR 0027 の `{N}G` と同じ切り詰め）で最後の行に切り詰め、桁は行の中の文字の境目に寄せる（`PlaceCaret` の寄せ・#971）。数でなければ 1 行知らせる（「行番号を入れてください」）。

9. **契約と道具。** application: `--find-replace`（Ctrl+F → 次・前・折り返しの報せ・E486・置換 1 件の `Edit` と undo 1 回・全件の `Edit` 1 回と undo 1 回・Esc で閉じる・`:N` と同じ着地・`行:桁`・Vim モードでは Ctrl+F / H / G が Vim のまま・一覧からの代替の前置き・`hlsearch` の面と `"/` が同じ値）。ui: `eng/verify-window.py --find`（Ctrl+F → 語 → Enter → F3 → Esc の節目）。速さ: 入力行が閉じていれば打鍵の道に分岐は増えない（`CommandInput` の有無の既存の 1 つ）。8 本に退行なし。

10. **限界（後続）。** 正規表現・単語単位・選択の中だけの置換・置換語の `\n` と `&`・大小の切り替えの欄・「置換して次へ」の鍵の案内の表示は後続。Vim モードの Ctrl+F / H / G を使う人が設定で通常モードの鍵に替えるのは `map`（ADR 0176）の後続。

## 強制

- 照合器が `VimPattern` の 1 本・最後の検索が 1 つ・置換が `Edit` 1 回: **planned**（契約 `--find-replace`・この PR で active）。
- 操作が表の行で実行が `run_operation` の 1 本: **active**（ADR 0078 の契約 `--operations`）。
- 8 本の速さに退行なし: **active**（QLT-014）。

## 結果

得られるもの: 通常モードでメモ帳と同じ鍵で検索・置換・行へ移動ができる。Vim モードでは同じ語が `n` `N` と `hlsearch` に続く。
失うもの・残る穴: 通常モードの検索は literal だけ。置換の 2 段は入力行 1 本の中で進むので、2 つの欄を同時には見られない（プロンプトに検索語を添えて見せる: 「置換後 (abc): 」）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| 検索と置換の専用の面（ダイアログ） | 面を増やす。入力行と照合器が既にあり、1 本で足りる（ARC-012） |
| 通常モードは別の照合器（`std::string::find`） | 照合器が 2 本になり `hlsearch` と `n` `N` に続かない |
| Vim モードでも Ctrl+F を検索に | D71（Vim 優先）。Ctrl+F は半画面送り |
| 置換の全件を 1 件ずつの `Edit` の列 | undo が件数ぶんになる。ADR 0168 と同じく 1 回の `Edit` |
