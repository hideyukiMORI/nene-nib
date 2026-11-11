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

## 追補（2026-11-11・#1376）全件の置換は文書の上限で確保の前に断る

決定 7 の `substituted_all` を `substituted_all(text, pattern, replacement, limit) -> std::expected<Edit, SubstituteFailure>` に改める。`SubstituteFailure` は閉じた enum の `no_match`（今までの「値なし」）と `too_large`（ARC-010 / CPP-005）。1 周目は当たりを数えるだけ（当たりの数・覆うバイトの和・最初と最後の当たりの行と端）で、置き換えた後の文書のバイト数「本文 − 当たりの和 + 当たりの数 × 置換語」が `limit` を超えるなら、置き換えた本文を確保する前に `too_large`（掛け算をせずに割り算で判定する・#1341 の `fits_repeated` と同じ形）。収まるなら 2 周目で、最初の当たりの行より前と最後の当たりの行より後の行を飛ばし、その間の行（最初の当たりの行から最後の当たりの行まで）を全部照合し直して置き換える（照合は行ごとの決定的な関数で本文は変わらないので、1 周目と同じ当たりになる）。当たりの位置の列は持たない（1 当たり 16 バイトで本文の 16 倍になり得た）。置換語は literal で改行の写しも無いので、CRLF の文書も本文が持つバイトのまま数える。

上限は製品では `maximum_document_bytes`（1 GiB・ADR 0159 の決定 7・#1341 と同じ値）を controller の `replace_all_matches` が渡し、試験だけが小さな値を渡して境を確かめる（`--find-replace` の `verify_substituted_all_limit`: ちょうどは通り 1 バイト超は断る・縮める置換も当たりの外が上限を超えれば断る・当たりが無ければ上限に依らず `no_match`。`--substitute-property` の模型は件の番号で上限を「模型の本文の 1 つ下・ちょうど・1 つ上・製品の上限」と回す）。断ったら本文と履歴は変えず、入力行は「置換後: 」のまま、`reject` が「置換の結果は 1 GiB までです」（印刷の「16 MiB までです」と同じ流儀）を出して 1 回鳴らす（入力行の確定の断り・ADR 0153 の決定 1）。置換語は入力行の上限（`DisplayText::maximum_bytes`・256 バイト）までなので、全件の置換は本文を多くとも 256 倍にする。契約 `--find-replace` の `verify_replace_all_too_large` は 64 個の `a` の行 2^16 本（4 MiB・当たり 2^22 個）を 256 バイトの語に置き換えて（2^30 + 2^16 バイト）製品の上限で断るのを確かめ、1 GiB は確保しない。

限界（#1376 の残り・ADR 0046 の #1341 の追補の続き）: 回数に依らず本文の定数倍まで増える Vim の道 3 つ（4 バイト文字での `r`・最大 4 倍、VISUAL の `g<C-a>`・約 20 倍、INSERT の流し直し・上限を 1 周ぶん越え得る）は上限を見ない。上限の近く（数百 MiB 以上）の文書でだけ起き、結果は多くとも上限の定数倍か上限と 1 周ぶんに留まる（倍率は #1341 の総点検の読みで、この追補では確かめていない）。判定を通すのは後続。通常モードの 1 件ずつの置換（`replace_current`）も上限を見ない。1 回で増えるのは多くとも置換語の 256 バイトなので、上限の近くの文書で 1 回あたり 256 バイトまで越え得るだけに留まる（INSERT の流し直しと同じ「上限を少し越え得る」族）。
