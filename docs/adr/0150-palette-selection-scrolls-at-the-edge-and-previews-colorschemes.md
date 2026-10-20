# ADR 0150 — 面と補完の一覧は「選択が見えている範囲の中を動き、端を越えるときだけ送る」、配色の候補は保存せずに仮に当てて見せる

- 状態: 提案（設計席 2026-10-20・hide の要望を外からのチャットで受けた。merge は独立レビュー・CI の後、試用は hide）
- 日付: 2026-10-20
- Issue: #706（親）・#707（core と application）
- 施主決定: **D59**（hide 2026-10-20: 「`:` と Ctrl+P の補完の操作感を VS Code / Cursor / Neovim / zsh に寄せる」「colorscheme を選ぶとき、焦点の色に一時的になってプレビューで分かるように」「上で末尾に回り込んだ後に最下段に張り付いて一覧が送られるのを、よくある形に」）。SPECIFICATION.md の表に記入する。
- 規則: FR-010 / D28〜D32、ARC-001 / ARC-011 / CPP-002 / CPP-012、ADR 0020（設定の保存）/ 0022（`:` の補完）/ 0023（面の鍵）/ 0060〜0062（面の候補と frame）/ 0078（操作の表）
- 調査: `D:/NeNeNib/outputs/20261020/survey-palette-ux.md`（鍵の表・張り付きの原因 `PaletteLayout.cpp:66`・プレビューの置き場 `delivery()`・参考の動き）
- 実装: 決定 1〜4・6 の core と application は #707（ui の鍵の表・ホイールの写し・決定 5 は #706 の 2 本目）。#707 の時点の実装の範囲は各決定の「#707 の注」に書く。

## 文脈

一覧の送りの先頭は「選択を最下段に置く」式 1 本（`first = max(selected+1, visible) − visible`）で、先頭を状態として持たない。上で末尾へ回り込んだ後は上を押すたびに選択が最下段に戻り、一覧だけが送られる。renderer とクリックも frame の値を読まずに同じ式で計算し直している。`:` の Tab は候補の全文を順に写すだけで、共通の接頭辞の埋めが無い。面では PgUp / PgDn / Ctrl+N が効かず、Ctrl+P は面を閉じる。配色は `settings` に保存した値からだけ決まり、候補に焦点を当てても見た目は変わらない。VS Code の Quick Pick・Neovim の補完窓と Telescope・zsh の menu-select・fzf はどれも「選択は見えている範囲の中を動き、端を越えるときだけ一覧を送る」。

## 決定

1. **送りの先頭は core の `CommandPalette` が状態として持つ（`first_`）。** 選択を動かしたとき、選択が `[first, first + visible)` の中ならそのまま、上に出るなら `first = selected`、下に出るなら `first = selected − visible + 1`。回り込みは両端で続ける（先頭で上 → 末尾の候補が最下段に・末尾で下 → 先頭が最上段に）。絞り直し（文字の入力）で `selected = 0` `first = 0`。見えている行数 `visible` は面を開くときと frame のたびに application が渡す（`palette_row_limit` と窓の高さの小さい方・今と同じ値）。**renderer とクリックは frame の `first` を読み、計算し直さない**（`palette_first_visible` と `first_row_of` を消す）。`:` の補完の面（上限 6 行）も同じ規則（`CommandLine` の候補の巡回に `first` を持たせる）。
   - #707 の注: 規則は core の `list_window_first` の 1 本で、面（`CommandPalette`）と `:` の補完（`CommandLine`）が共有する。窓は件数の末尾を越えて空の行を残さない（窓が広がったときも末尾の候補が最下段）。窓の寸法は `VisibleLines` の `window_width` / `window_height` で application に届き、行数は core の `palette_window_rows` と `command_layout` から決める（`fit_command_rows` の 1 か所・意図と鍵の後）。寸法が届く前は上限（8 行・6 行）。frame は面の `CommandPaletteView.first` と入力行の `InputLineView.completion_first` に載る。
2. **面の鍵（通常モードと Vim で同じ・ADR 0023 決定 1 の追補）**: Up / Down と Ctrl+P / Ctrl+N は前・次（回り込む）。PgUp / PgDn は 1 画面（`visible` 行・端で止まる）。Home / End は入力欄のキャレット（今のまま）。Ctrl+Home / Ctrl+End は一覧の先頭・末尾。Tab / Shift+Tab は次・前（今のまま）。Enter は確定。Esc は取消。**Ctrl+P で面を閉じる今の動きはやめる**（閉じるのは Esc・zsh / Vim の Ctrl+P は「前の候補」）。ホイールは**一覧だけを送る**（選択は動かさず、見えなくなったら端に寄せる・VS Code と同じ）。
3. **`:` の補完（ADR 0022 決定 2 の追補）**: 最初の Tab は候補の**共通の接頭辞**まで入力欄を埋める（zsh の `menu-complete` の前段・Vim の `wildmode=longest`）。候補が 1 つなら全文を埋める（評価はしない・Enter が要る・今のまま）。共通の接頭辞が入力と同じ（これ以上埋まらない）なら候補を順に写す巡回に入る（今の Tab の動き）。Shift+Tab は巡回の逆。巡回の末尾の次は**打った元の文字に戻る**（Vim の `wildmenu` の一拍）。Up / Down は `:` の行では今までどおり何もしない（履歴は無い・ADR 0022 決定 2）。
4. **プレビューは「見えている設定」と「保存した設定」を分ける 1 か所で、状態を足さない。** `EditorController::delivery()` が frame と ui に渡す設定は `shown_settings()`: 面か `:` の行で焦点の候補が設定の命令（`colorscheme <名前>`・`set fontsize=<N>`）なら、その候補を純関数 `core::evaluate_ex` で評価した `settings`（保存しない）。それ以外は `state_.settings()`。焦点が動く・Esc・面を閉じる・文字を打つと自然に元に戻る。Enter で確定したときだけ今までどおり `SettingsPort` に保存する（ADR 0020）。**最初の段は `colorscheme` と `fontsize` だけ**。`guifont=` のプレビューは書式の作り直しと見えている本文の文字組みのやり直しが候補を動かすたびに起きるので、速さを測ってから次の段で決める（限界に書く）。
   - #707 の注: 実装したのは `colorscheme` だけ（評価した設定から配色 `theme` だけを保存した設定に重ねる）。`fontsize` は保留: application の位置解決と文字組みの問い（`VisualTextView`・`screen_rows_of` の `TextLayoutRequest`）と行高の幾何は保存した設定を読むので、見えている設定の字の大きさだけを替えると frame の行と ui の字体が食い違う。字の大きさのプレビューは、その読み手も見えている設定を読むかを決めてから入れる（設計席の判断待ち）。
5. **ui は毎回の配達で前後を比べる。** 字体・字の大きさ・明暗（DWM の `apply_backdrop`）の変化を 3 つの意図のときだけ見る今の形（`EditorWindow.cpp:1453`）をやめ、`deliver` の終わりで前回の `delivery` の該当の欄と比べて変わっていれば適用する（プレビューで配色が変わる道を 1 本にする）。
6. **契約**: core の `CommandPalette`（送りの表: 上で回り込んだ後に上を 3 回 → 選択が 3 つ上がり `first` は動かない・下端を越える → `first` が 1 進む・PgDn・Ctrl+Home）・`CommandLine`（共通の接頭辞・1 つ・巡回・元に戻る一拍）・application（`shown_settings` が焦点の候補の配色を返し Esc で保存した設定に戻る・Enter で保存・`verify_quiet_paths` は候補の移動で `SettingsPort` を呼ばない）・ui（`press_command_key` の表・ホイール）・`verify-window.py` の面の節（PNG で焦点の候補の配色が本文に出ること）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| プレビュー用の状態 `preview_theme` を `EditorState` に足す | 消し忘れの穴ができる。焦点の候補から導けば状態は要らない |
| ホイールで選択を動かす（今のまま） | VS Code / ブラウザの一覧と違い、長い一覧を眺められない |
| Ctrl+P で面を閉じる（今のまま） | zsh / Vim / Neovim の Ctrl+P は「前」。閉じるのは Esc の 1 つ |
| 送りの先頭を renderer が決める | 描画とクリックと application で 3 回計算して食い違う（ARC-001） |

## 限界

- `guifont=` のプレビューは次の段（速さを測ってから）。
- `fontsize` のプレビューは #707 では入れていない（決定 4 の #707 の注）。
- 面で Ctrl+P を押したときに閉じる動き（application の `OpenCommandPalette` の切り替え）は、ui の鍵の表が Ctrl+P を「前」に写すまで残る（#706 の 2 本目）。
- `:` の行の履歴（Up / Down）は無いまま。
- 面の一覧の `scrolloff`（端の手前で送る余白）は置かない（VS Code / Neovim と同じ 0）。
