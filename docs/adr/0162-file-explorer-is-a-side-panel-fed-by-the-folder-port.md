# ADR 0162 — ファイルエクスプローラーは本文の左の面で、ツリーは core の値、フォルダは `FolderPort` の worker が読む

- 状態: 受理（施主決定 D66・2026-10-28「全部推しでいい」。実装は E2〜E4 の席・受理は D41 の形）
- 日付: 2026-10-28
- Issue: #997（親）・計画 `docs/plans/2026-10-28-file-explorer.md`
- 規則: FR-022、ARC-001 / ARC-003 / ARC-004 / ARC-007 / ARC-009 / ARC-011、CPP-002 / CPP-005 / CPP-011 / CPP-012、QLT-014、ADR 0008（色のトークン）/ 0056（帯のマウスは押した要素と離した要素が同じとき）/ 0060（開く道は `open_document` の 1 本・一覧からは `open_listed`）/ 0062（`FolderPort`・worker 1 本・`WorkCompleted`）/ 0078（操作の表）/ 0114（設定の版）/ 0148（`pane_layouts`）/ 0150（面の行の layout の保持）/ 0160（設定 v4 は LSP と 1 回）

## 文脈

hide の指示（2026-10-28）: ルートを設定してそこからツリー状に展開する簡易なエクスプローラーを本文の横に置き、鍵とコマンドで出す・消す。土台は揃っている: フォルダを裏で読む port（ファイルだけ）・面の色のトークン・本文の枠の配置の 1 本・操作の表・設定の版の移行。

## 決定

1. **ツリーは core の純粋な値 `ExplorerTree`。** ノードの列（親・深さ・名前・フォルダか・隠し属性か・開いているか・読み終えたか・`truncated`）。`visible_rows()`（開いているフォルダの子だけを深さ優先で並べた行）・`toggled(row)`（フォルダの開閉・ファイルなら不変）・`loaded(node, entries)`（子を置く・並びはフォルダが先・名前は ASCII の大小を畳んだバイト順・OS の順に依らない）・`collapsed_to_parent(row)`（閉じていれば親の行へ）。1 フォルダ 2,000 件まで（超えた分は捨てて `truncated`・行の末尾に「…」）。ルートの変更でツリーは作り直す。

2. **配置は core の純関数 `explorer_layout`。** 面の幅は `min(240 DIP, client の幅の 40%)`、行の高さは本文と同じ、字下げは深さ × 12 DIP、印 ▸ / ▾ は文字で描く（字体に依らない U+25B8 / U+25BE・幅は `display_width` の表）。本文の枠は `pane_layouts` の `client` を面の幅だけ右へ縮めた矩形で求める（ADR 0148 の 1 本はそのまま・2 枠のときも同じ）。長い名前は末尾を「…」で切る（`tab_title_for` と同じ規則）。

3. **状態は窓に 1 つ。** `EditorState.explorer`（`ExplorerState{ExplorerTree tree; std::optional<std::size_t> selected; bool focused; std::size_t first_visible;}`）。表示（`explorer` on / off）とルート（`root`）は設定（v4・ADR 0160 の `lsp` と 1 回の版上げ・ADR 0114 の移行の型）。開いているフォルダと選択は保存しない。

4. **意図は 4 つ。** `ToggleExplorer`（D66 の規則: 隠れていれば出して焦点・出ていて焦点があれば消す・焦点が無ければ焦点だけ）・`ExplorerKey{key}`（j k h l Enter Esc と矢印。j / k / ↓ / ↑ は選ぶ・l / → / Enter はフォルダを開く・h / ← は閉じる（閉じていれば親へ）・Enter はファイルを開いて本文へ焦点を戻す・Esc は焦点だけ戻す）・`PointExplorer{row, ExplorerPointing}`（押した・離した・ダブル。押した行と離した行が同じときだけ効く・シングルは選ぶだけでフォルダは開閉・ダブルは開く・ADR 0056 の決定 9）・`ScrollExplorer{lines}`（`wheel_turned` の累積）。面に焦点があるとき本文の鍵は面へ行き、Vim の鍵の表は通らない（Ctrl+P の面と同じ奪い方）。IME は面に焦点があるとき閉じる（ADR 0061 の構え）。どの意図も `keeps_tab_walk` は偽（使う人の操作）。

5. **フォルダは `FolderPort` で裏に読む。** `FolderRequest` に閉じた enum `FolderListing{files, entries}` を足し、`entries` のときは `FolderBatch` に `std::vector<FolderEntry{FilePath path; bool folder; bool hidden;}>`（`.` `..` を除く・隠し属性は返す）。Ctrl+P の同じフォルダは `files` のまま。開くときに頼み、`WorkCompleted` → `collect` → `loaded`（券が今の要求と違えば捨てる・ADR 0062 の形）。読めないフォルダは行を残して「（読めません）」を 1 行知らせる。打鍵・切り替え・保存では頼まない（`verify_quiet_paths` の欄）。

6. **ファイルを開くのは `open_listed` の道。** 失敗は 1 行の知らせ（ADR 0060 の決定 7・履歴からは外さない）。開いたら本文へ焦点を戻し面は残す。

7. **操作と Ex。** `EditorOperation` に `toggle_explorer`（名前「エクスプローラーを出す・消す」・Ctrl+Shift+E・通常と Vim の全モード・`OperationKey::e` を足す）と `choose_explorer_root`（名前「ルートのフォルダを選ぶ」・鍵なし・F1 の一覧から・`FOS_PICKFOLDERS` のダイアログは ui の `FileDialog` に 1 本足す）。Ex は `set explorer` / `set noexplorer` / `set explorer?` と `set root=<path>` / `set root=.`（アクティブな文書のフォルダ・無題なら失敗）/ `set root?`（`ExResult.settings` の欄に載る＝保存する値・候補 5 行）。

8. **描画。** 地は `panel`・境界 1 DIP は `panel_border`（焦点があるとき `accent`）・選んだ行は `current_line`・名前は `text`・隠しと印とルート名は `muted`。色のトークンは足さない。行の text layout は `PaletteRowLayouts` と同じ保持（ADR 0150 の追補 #904）。面が消えているときの打鍵の道に分岐を足さない。

9. **契約。** core: `--explorer-tree`（開閉・並び・2,000 件・`truncated`・親へ・ルートの作り直し）と性質（固定 seed のランダムな木で `visible_rows` が深さ優先・開いていないフォルダの子が出ない・`toggled` の往復）・`--explorer-layout`（幅の規則・字下げ・2 枠）。application: `--explorer`（意図 4 つ・焦点の規則・`FolderPort` の `entries` の往復・古い券・`open_listed` の道・設定の保存・`verify_quiet_paths`）。ui: `nib_window_tests` の面の画素（WIC）と `eng/verify-window.py --explorer`（出す → フォルダを開く → ファイルを開く → 消す）。速さ: 8 本に退行なし（面が消えている既定）。

10. **限界（後続）。** 面の幅のドラッグ・フォルダの監視（`ReadDirectoryChangesW`）・ファイル操作（名前の変更・削除・新規）・面の中の絞り込み・最近のルートの履歴・起動の引数のフォルダをルートにすること。

## 強制

- 開く道が `open_listed` の 1 本・フォルダの読みが `FolderPort` の 1 本: **planned**（契約 `--explorer`・E3 の PR で active）。
- 面が消えているときの打鍵の道の不変と 8 本の速さ: **active**（QLT-014）。
- 色のトークンを足さない: **active**（ADR 0008 のレビュー事項）。

## 結果

得られるもの: ルートからのツリーでファイルを開ける。鍵 1 つで出し入れでき、消えていれば今と同じ。
失うもの・残る穴: 面を出すと本文が 240 DIP 狭い。外で変わったフォルダは開き直すまで古い。2,000 件超のフォルダは末尾が切れる。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| 右の面（案 B） | 帯の「∨」と窓の操作の下で目が往復する |
| 浮く面（案 C・Ctrl+P の形） | 開くと消えるので「ツリーを見ながら編集」ができない。hide の「サイドに」から外れる |
| Ctrl+B | Vim NORMAL の Ctrl-B（1 画面上）と衝突 |
| OS の順（`CompareStringEx`）で並べる | core が OS に触れない（ARC-003）。ASCII の大小を畳んだバイト順で決定的に |
| 起動時にルートを全部読む | 深い木で起動が遅れる。開いたフォルダだけ裏で読む |

## 追補（2026-10-29・PR #1070 の E3 の席が見つけた点・設計席の決定）

11. **フォルダの読みは 1 つずつ。** `FolderPort` は新しい要求で前の要求をやめる（ADR 0062 決定 6）ので、application は「port が今の仕事（自分の控えか Ctrl+P の面の券）を持たない間だけ、開いていて読んでいないフォルダを木の順に 1 つずつ頼む」。Ctrl+P の面が読み始めたら読みかけの控えは捨て、面の読みが終わった後に頼み直す。決定 5 の「node ごとに 1 つ」はこの形で読む。契約 `verify_one_listing_at_a_time` と `verify_palette_shares_port`。
12. **5 つ目の意図 `ExplorerRootChosen{folder}`。** 操作 `choose_explorer_root` のダイアログの答えを受ける口。使う人の操作（`keeps_tab_walk` は偽）。
13. **細部。** 押した時点で面に焦点・ダブルは開く（フォルダは開いたまま）・読めないフォルダは子の無い読み終えた扱い（開き直しても頼み直さない・ルートの作り直しで読む）・面の行数は本文の `VisibleLines`・`EditorFrame` に載せる行は `first_visible` から画面の行数ぶんだけ（全体の数は `total`）・押した行と離した行は節の経路で比べる（間に子が届いても取り違えない）・「…」の行は押しても離しても何もしない。
14. **`WorkCompleted` の中の知らせの例外。** 読めないフォルダの「（読めません）」は `WorkCompleted` の `collect` の中で置く 1 行で、ADR 0062 決定 7 の「`WorkCompleted` は知らせを動かさない」の唯一の例外（読めない結果は使う人の操作の結果として届く）。
15. **強制の更新。** 決定 9 の「開く道が `open_listed` の 1 本・フォルダの読みが `FolderPort` の 1 本」は PR #1070 の契約 `--explorer`（59 checks）と `verify_quiet_paths` の欄で **active**。
## 追補（2026-10-30・#997 の E4）E4 で足した口と凍結した値

1. **E4 で足した口（application）。** 設計席の答え（E4 の止まった点）に従い、ui の PR で 5 点を足した。
   - `EditorController::explorer_focused() const noexcept`（public）: 面に焦点があるか。窓は打鍵ごとにこれを読み（`command_palette_active()` と同じ安い読み・frame を作らない）、真なら操作の鍵（Ctrl+P・F1・Ctrl+Shift+E など表の鍵）のほかは `ExplorerKey` の表（j k h l Enter Esc 矢印）だけへ写す。表にない鍵と文字（WM_CHAR）は捨て、Vim の表と `map` の表を通さない。
   - IME の構えは `ime_stance_of(mode, vim_mode, input, explorer_focused)` の 1 本のまま。入力行が無く面に焦点がある間は `closed`（NORMAL と同じ）、焦点が外れればモードの規則へ戻る。入力行があれば入力行の規則が勝つ（ADR 0061）。
   - `explorer_view()` を public に（`command_palette_view()` と同じ・本文の行を作らない）。窓は押した点の面の行に `first_visible` を足して `PointExplorer.row` を送る。先頭の行（ルート名）と行の無い所は木の行の数（行の外）を送る。
   - **本文の押しは面の Esc と同じ。** 面に焦点がある間に本文の枠を押したら、窓は押したとき（離したときではない）に `ExplorerKey{esc}` を送ってから今までの押しを続ける。新しい意図は足さない。
   - 面の行数（`explorer_page`）は「本文の `visible_lines` − 1（先頭のルート名の行）・最小 1」を application の 1 か所で導く。本文の行の上の余白（12 DIP）は行の高さより小さいので、面の高さに入る行数はこれを下回らず、最後の行は切れない。窓が送る `VisibleLines` は今までどおり本文の値。
   - （独立レビュー R2-2・6 つ目）`composition_ignored()` は入力行の判定の後で `explorer_focused()` を見て真を返す。面に焦点がある間の IME の変換（`ComposeText`）と確定（`CommitText`）は本文へ入れない。
   - （独立レビュー R2-1）`CS_DBLCLKS` で 2 回目の押しが `*BUTTONDBLCLK` で届くので、窓は `single_press_of` で 1 回目と同じ押しへ写す（中ボタンの続けてのタブを閉じる・ADR 0056 の決定 9）。左は面の上だけダブルクリック。
2. **ダブルクリック。** 窓の class に `CS_DBLCLKS` を立て、面の上の `WM_LBUTTONDBLCLK` だけを `PointExplorer{double_click}` にする。面の外の `WM_LBUTTONDBLCLK` は `WM_LBUTTONDOWN` と同じ道（本文と帯の押しは不変）。
3. **本文の枠。** `pane_layouts` は client の原点（left / top）を使う（E2 では原点 0 に固定されていて、決定 2 の「縮めた client を渡す」が成り立たなかった）。原点 0 の呼び出しの結果は不変。
4. **凍結した値（採用案の「実装で凍結する値」）。** 面の左右の余白 8 DIP・印の升の幅は `explorer_layout` の `marker`（2 つの印の桁の多い方 × 字の大きさ）・「…」は書式に焼いた文字単位の省略記号（タブの題名と同じ）。面の名前の字は UI の字体（Segoe UI Variable Text → Segoe UI）を本文の大きさの比で拡げた大きさ。行の文字組みは `PaletteRowLayouts` の別の実体（上限 512）で保持し、書式の作り直しと面を消したときに捨てる。

## 追補（2026-10-30・#1125）面の行数は client の高さ・保持は名前で引く・面を出した 1 打鍵の費用

PR #1121 の独立レビュー（R6-2 / R4-1）の後続。設計席の判断（依頼書 E5）を記す。

1. **面の行数は窓の client の高さから導く。** E4 の追補 1 の「本文の `visible_lines` − 1」をやめる。上下の分割（ADR 0148）では操作中の枠の `visible_lines` が本文の半分の高さになり、窓の全高に出る面の下半分が空いていた。窓は `VisibleLines.explorer_lines` に「面の高さ（本文の帯の全高）に入る行数（ルート名の行を含む）」を載せ（`VisibleLines` を作るのは ui の `window_lines` の 1 本・`EditorWindow::body_view` が呼ぶ・試験は `nib_window_tests --explorer` の `verify_window_lines`）、数えるのは core の `explorer_panel_lines(client, dpi, font_size)` の 1 本（面が出ているか・分割に依らない）。application は `explorer_page()` の 1 か所で「それ − 1（ルート名の行）・最小 1」を導く。`explorer_lines` が 0（寸法が届く前）のときだけ本文の見えている行数を面の行数とみなす。契約は `--explorer` の `verify_panel_rows_from_window`、core は `--explorer-layout` の `verify_panel_lines`、画素は `nib_window_tests --explorer` の `verify_split_rows`（上下の分割でも面の最後の行に名前がある）。ホイールの 1 画面の設定（`WHEEL_PAGESCROLL`）の送り量は本文の行数のまま（後続）。
2. **行の文字組みの保持は鍵の hash で O(1) に引く。** `PaletteRowLayouts` の鍵の形（書式・役・DPI・欄の幅と高さ・文字列）はそのままで、鍵を 1 本のバイト列に畳んで `unordered_map` で引き、最後に引いた順の `list` で上限を越えた末尾（最も前に使ったもの）を捨てる。エクスプローラーの上限 512 は不変。同じ型の Ctrl+P の面とタブの題名の実体も同じ引き方になる（捨てる順の契約 `verify_capacity` は不変）。
3. **費用は `nib_perf_probes` の workload で前後を測る。** `explorer-typing-shown-1000-64`（面を出して 1,000 件を読んだ後に 64 打鍵・各打鍵で frame）と対の `explorer-typing-hidden-64`。任意のベンチは足さず（D60 と同じく 8 本と混ぜない）、`eng/compare-probes.py` の名前の表にも載せない。probe は application までなので、決定 2 の ui の保持の引き方はこの probe の区間に入らない。保持の探索の費用は計算量（線形探索 → O(1)）で担保し、ui_win32 に依る probe exe は足さず `nib_perf_probes` の依存の向きも変えない（ADR 0082 のまま）。面を出した 1 打鍵は probe の shown / hidden の対で見る（Debug の参考値で 1 打鍵あたり約 +0.3 ms）。

## 追補（2026-11-10・#1380・施主決定 D78）面の幅は境のドラッグで変えて覚える

施主の要望と決定 D78（2026-11-10）: 面と本文の境をドラッグして面の幅を変え、次の起動でも覚える。決定 10 の限界「面の幅のドラッグ」を解く。値は設計席の決め（Issue #1380）。

1. **当たりとカーソル。** 境は面の右端の左右 3 DIP（1 画素以上）・縦は面の帯。判定は core の `explorer_divider_hit(ExplorerLayout, dpi, x, y)` の 1 本。窓は面の行の当たりより先に引く（境は面の右端の行と重なる）。境に載ったかは `WM_MOUSEMOVE` の点で決め、窓の印 `over_explorer_divider_` に覚える（ここで controller の安い読み `explorer_width()` と `font_size()` を読む。面が消えていれば `explorer_width()` の 1 欄で抜け、`delivery()` も frame も作らない・#1380 の R-2）。`WM_SETCURSOR` は client の上（`HTCLIENT`）でその印だけを読んで `IDC_SIZEWE` を保つ（OS のカーソルの位置は読まない・ARC-007）。印は次の move まで前の値が残るので、鍵で面を閉じた直後やドラッグが最大・最小で止まった後に、カーソルが 1 回だけ前の形のまま見えることがある（見た目だけ。面が消えたときは印を消す）。本文・全面プレビューの選択のドラッグ・面の行・帯を押している間は、境を横切ってもカーソルを変えない（#1380 の R-3）。
2. **範囲。** 最小 120 DIP・最大は「client の幅の 60%（切り捨て）」と「本文に 200 DIP を残す幅」の小さい方。最大が最小より小さい窓では最大が勝ち、0 を下回らない。切り詰めは core の `explorer_panel_width(client の幅, dpi, ExplorerWidth)` の 1 本で、窓を狭めたら描く幅だけを切り詰めて覚えた値は変えない（広げれば戻る）。覚えた値が無ければ決定 2 の式 `min(240 DIP, client の 40%)` のまま（今までと同じ値）。
3. **入力の型。** `explorer_layout` の 4 つ目の引数を `bool shown` から `std::optional<core::ExplorerWidth>` に替えた（値なしは面が消えている・`ExplorerWidth.dips` の値なしは既定の式・tidy の引数の上限 4 のまま）。呼び手は窓の `explorer_frame`（controller の `explorer_width()`）と描画器（`ExplorerView.width`）と `explorer_panel_lines` の 3 か所。
4. **意図と状態。** 意図 `ResizeExplorer{ExplorerResizing, width_dips}`（`dragged` / `released` / `reset`）は使う人の操作の側（`keeps_tab_walk` と `begin_intent` の表に載せない・全面プレビューの間も通す）。窓は押した点と面の右端のずれを覚え、動かした点からずれを引いた右端を core の `explorer_dragged_width`（同じ範囲に切り詰めて DIP へ・設定として読める範囲に収める）で幅にして送る。controller は入口で `width_dips` を core の `explorer_width_clamped`（[120, 9999]・`explorer_dragged_width` と同じ 1 本）に収めてから状態と設定に置く（窓を経ない送り手でも書いた値を自分の読みで既定に戻さない・`INT32_MAX` や負の値で溢れない・#1380 の R-1）。ドラッグの間の幅は controller の状態（`explorer_dragged_`）だけが持ち、窓の安い読み `explorer_width()` と frame の `ExplorerView.width` が同じ値を返す。窓は幅を送るたびに本文の寸法（`VisibleLines`）を送り直す（窓の寸法が変わったときと同じ道）。
5. **書くのは 2 回だけ。** 設定 `explorer_width` へ書くのは、動かした後に離したとき（`released`）と境のダブルクリック（`reset`・既定の式へ戻す）だけで、`persist_settings` の 1 本（同じ値なら書かない）。動かしている間と、動かさずに離したときは書かない（押しただけで既定の式を数字に変えない）。離す前に捕まえを失ったら最後に送った幅を離したのと同じに送る。書けなければ 1 行知らせて覚えた幅へ戻る。面が消えていればどれも何もしない。**ドラッグの途中で面が閉じたら**（`ToggleExplorer`・`:set noexplorer` など、どの道でも）ドラッグは終わりで、ドラッグの幅は捨てて設定に書かない（覚えた値のまま・次に開けば覚えた幅）。controller は設定を置き換える 1 本（`persist_settings` → `sync_explorer`）で面が消えていればドラッグの幅を捨て、窓は面が消えた delivery（`follow_explorer`）で捕まえを放して印を消し、幅を送らない（`end_hidden_explorer_drag`）。開き直した後に離しても書かない（Issue #1380 の設計席の決め）。
6. **面が消えているときの打鍵の道は不変。** 幅は面が出ているときだけ読み（`explorer_layout` は `shown` が値なしなら幅を計算しない・`explorer_view` は面が消えていれば作らない）、打鍵の道の `delivery` に欄も分岐も足さない。
7. **契約。** core `--explorer-layout`（`verify_chosen_width` `verify_dragged_width` `verify_divider_hit` `verify_width_text`）・application `--explorer`（`verify_resize_writes_on_release` `verify_resize_remembered_and_hidden` `verify_resize_clamped` `verify_resize_settles_tab_walk` `verify_resize_hidden_while_dragging`・設定の port の書いた回数）・`--preview-full`（`verify_full_preview_resize`・全面の間も幅が変わる）・adapters の設定の codec（鍵の往復・空と壊れた値は既定・v1 / v2 / v3 に在れば拒否・境の 120 と 9999）・窓 `nib_window_tests --explorer`（`verify_chosen_width_pixels`・境は面の行と重なる）。窓の状態機械（押す・動かす・離す・捕まえを失う・ダブルクリック）の自動の試験は無く、実機のドラッグは施主の試用で確かめる（窓を前面に出す道具は回さない）。
8. **限界。** プレビューの枠と split の境のドラッグ（ADR 0148 の決定 1 のまま）・面を右に置く設定は対象外。ドラッグの間の境の強調（色）は足していない（境界は決定 8 の `panel_border` / `accent` のまま）。境の軽い調整の直後の押し直しがダブルクリックになる・窓が 2 つのときの書き込みの機会は後続（#1387・レビュー V-1384 の F-2・F-3）。
9. **ドラッグ中の Esc（#1413・#1387 の F-1・施主決定 2026-11-10 深夜「入れる」）。** 捕まえている間の Esc は押す前の幅へ戻してドラッグを終え、設定に書かない（Windows の分割バーの慣習）。窓は `WM_KEYDOWN` の入口で、境を押している間の `VK_ESCAPE` だけを `cancel_explorer_divider` で受けて捕まえを放し、動かしていれば意図 `ResizeExplorer{cancelled}` と本文の寸法を送る。その 1 回の Esc は食べる（本文・Vim・面の鍵へ流さない・`WM_CHAR` の 0x1B は制御文字として前から捨てる）。押していなければ Esc は今までどおり鍵の道へ。controller は `cancelled` でドラッグの幅（`explorer_dragged_`）を捨てるだけで、`width_dips` を読まず `persist_settings` を呼ばない（覚えた値か既定の式のまま）。後から来る離しはドラッグの幅が無いので書かない。取り消した Esc を押し続けた繰り返し（`lParam` の bit 30）は、押し直すまで食べる（`take_explorer_escape`）。IME の変換中の Esc は IMM32 が `VK_PROCESSKEY` に替えるのでこの道に当たらず、変換の取り消しが先に効いてドラッグは続く（2 回目の Esc で取り消す）。ドラッグ中に DPI が変わったとき（`WM_DPICHANGED`）も Esc と同じ `cancel_explorer_divider` で押す前の幅へ戻す（#1387 の F-4）。捕まえの終わり（離す・Esc）は `settle_explorer_divider` が押しで置いた帯の印 `left_pressed_` を消し、離した点か最後の点でカーソルを決め直す。捕まえを失ったときは帯の印と境のカーソルの印を消し、カーソルは置かない（境の離しは `release_title_bar` を通らないので、印が残ると hover が次のクリックまで止まり、左右の矢印が client 全体に残っていた・#1380 から・レビュー V-1421 の後続）。契約は application `--explorer` の `verify_resize_cancelled`。窓の状態機械の自動の試験は決定 7 と同じく無い。

## 追補（2026-11-10・#1388・施主の要望）クリック 1 回でファイルを開き、フォルダを開閉する

施主の要望（2026-11-10 夜）: 「Cursor とかはダブルクリックじゃなくてシングルクリックで開くよね。その方がなれてる」。決定 4 の `PointExplorer` の「シングルは選ぶだけ・ダブルは開く」と決定 6 の「開いたら本文へ焦点を戻す」を、VS Code / Cursor と同じ手触りに改める（値は設計席の決め・Issue #1388）。仕様 D66 の文言（シングルは選ぶだけ）は施主の要望で改まり、仕様の表の更新は設計席が行う（この PR は仕様を触らない）。

1. **1 回のクリック。** 押した行と離した行が同じ節のとき（ADR 0056 の決定 9 のまま）、ファイルの行は選んで `open_listed` の 1 本で開き（失敗は 1 行・履歴は触らない・既に開いていればそのタブへ切り替えて読まない）、**焦点は面に残す**（続けて ↑ ↓ で歩ける）。フォルダの行は選んで開閉する（今までどおり）。「…」の行は選ぶだけ・行の外と空の所は何もしない。↑ ↓ と j k の鍵は選ぶだけで開かない。
2. **ダブルクリックの状態機械。** OS は面の上の速い 2 回の押しを「押す → 離す → `WM_LBUTTONDBLCLK` → 離す」と送る。窓はそのまま `pressed` → `released` → `double_click` → `released` と送り（写しは ui の `explorer_pointing_of` の 1 本・押していない離しは送らない）、意味は controller が決める。controller は 1 回のクリックが効いた節の経路を `explorer_clicked_` に覚え（次の押しと 2 打目で消す）、2 打目（`double_click`）は:
   - 覚えた節があれば、2 打目は**画面の行を引き直さずにその節として扱う**（`second_click_explorer`）。`WM_LBUTTONDBLCLK` は 1 打目の押しから数 px の中でしか来ないので、2 打目は 1 打目と同じ節を指す。1 打目の開閉で先頭が切り詰められたり（末尾までスクロールした面でフォルダを畳む）、裏の列挙で上に行が足されたりして行が画面上でずれても、別の行を押さない（レビュー V-1391 の M-1）。ファイルは焦点を本文へ移し、開いたタブがあれば `open_listed` の 1 本でそのタブをアクティブにする（開いていれば切り替えるだけで**2 回読まない**・読み中なら揃えばアクティブになる・1 打目で開けなかったときは試し直さない）。フォルダは何もしない（**開いて閉じる、にしない**・見えていれば選ぶだけ）。
   - 覚えた節が無ければ（押した行と離した行が違った・境の押しの直後・間に使う人の操作が入った）、画面の行を引いて 1 回のクリックとして効かせ、ファイルなら焦点を本文へ移す（Enter と同じ・開いていれば切り替えるだけ）。
   - 2 打目の後の離しは押した印が無いので何もしない。3 打目の押しは新しいクリック（フォルダならもう一度開閉）。
   - **印を消す所。** 次の押しと 2 打目（`accept(PointExplorer)`）、面のマウス以外の使う人の操作（controller の入口 `apply` の 1 か所・`keeps_explorer_click`。鍵・タブの切り替え・本文や境の押し・問いの答えなど）、面を消す・ルートを替える（`sync_explorer`）。使う人の操作ではない意図（裏の仕事の合図 `WorkCompleted`・窓の寸法など・`not_user_operation`）は印を保つ（裏で行が届いても 2 打目は 1 打目と組む）。
3. **Enter は今までどおり。** ファイルは開いて焦点を本文へ・フォルダは開く（決定 4 のまま）。意図・`ExplorerPointing` の値・`keeps_tab_walk` と `begin_intent` の表は変えない（`PointExplorer` は使う人の操作のまま）。
4. **当たらないもの。** 境のドラッグとダブルクリック（#1380・D78）は面の行より先に引くので、この規則に入らない。面のホイールは不変。面に右クリックの道は無い。
5. **契約。** application `--explorer` の `verify_click_opens_file`（クリックで開く・焦点は面・↑ ↓ で歩ける・既に開いているタブへの切り替えは読まない・開けないファイルは 1 行で焦点は面・履歴は触らない）・`verify_double_click_reads_once`（OS の列で読みは 1 回・焦点は本文・開けないファイルも試すのは 1 回・1 打目が効かなかった 2 打目は Enter と同じ）・`verify_folder_double_click_toggles_once`（閉じたフォルダも開いたフォルダもダブルクリックで 1 回だけ開閉）・`verify_double_click_across_arrival`（1 打目の後に裏で上に子が届いても、1 打目のファイルの読みは 1 回・焦点は本文・ずれた先の行のファイルは読まない）・`verify_double_click_collapse_at_bottom`（末尾までスクロールした面で開いたフォルダをダブルクリックすると、そのフォルダが 1 回閉じるだけ・ほかの行の開閉とファイルの読みは 0）・`verify_double_click_after_tab_switch`（A をクリック → Ctrl+Tab で B → A の行で 2 打目 → アクティブは A・A の読みは 1 回）・`verify_double_click_after_question`（1 打目で巨大な行の問いが出て答えた後の 2 打目は読まず・もう一度聞かない）・`verify_enter_opens_file`（矢印で歩くだけでは開かない）。窓 `nib_window_tests --explorer` の `verify_click_messages`（OS の 4 通の列 → 送る 4 つの意図・押していない離しは送らない・ほかのボタンと動きは面の押しではない）。窓の実機のクリックは施主の試用で確かめる（窓を前面に出す道具は回さない）。
6. **やらないこと。** プレビューのタブ（クリックで開いたタブを次のクリックで置き換える形）は施主の判断の後で別の Issue。
7. **問いとの順。** 1 打目で開いたファイルが巨大な行の問い（ADR 0140 の決定 2）を出したとき、窓は同じ配達の中でモーダルに聞いて答えを意図 `SelectDisplay` で返す。2 打目はその後に来るので、答え（使う人の操作）が印を消し、2 打目は画面の行を引く 1 回のクリック（開いているので切り替えるだけ・読まない・もう一度聞かない）になる。問いの窓が出ている間の実機の押し（本窓は使えないので届かない、と読んでいる）は確かめていない（施主の試用・レビュー V-1391 の F-1）。
8. **限界。** 製品の読みは worker なので、1 打目の読みが裏で失敗したときも 2 打目は焦点を本文へ移す（読みの結果を待たない・失敗の 1 行は出る・本文は前の文書のまま）。害は小さいので直さない（レビュー V-1391 の F-2）。

## 追補（2026-11-10・#1390・施主決定 D81）見た目を採用案 B（帯とつながる面・構文の色の点・UI の字体）に改める

施主の要望と決定 D81（2026-11-10 夜）: 「cursor や vscode と同等かそれ以上の見た目にしたい」→ 案 A / B / C のモックから案 B。値の正本は採用案 [`docs/design/2026-11-10-explorer-look.md`](../design/2026-11-10-explorer-look.md)（モック `docs/design/2026-11-10-explorer-look.html`）。決定 2 の「行の高さは本文と同じ」と決定 8 の色の割り当てを改める。

1. **寸法は字の大きさに依らない。** 行 22 DIP・ルート名の行 30 DIP・升 16 DIP・行の丸い面は左右 6 DIP の内側で角 5 DIP・本文の左上の角 8 DIP・未保存の丸 6 DIP（DIP の固定値・DPI に比例）。`explorer_layout(client, dpi, shown)` と `explorer_panel_lines(client, dpi)` から字の大きさの引数を外した（縦の帯は `body_band` の 1 本・`body_layout` と共用）。面の行数は「ルート名の行が入れば 1 + 残り / 22 DIP」。application の `explorer_page()`（それ − 1）と窓の `window_lines` は不変の形のまま。
2. **行と部品の矩形は core の 1 本。** `explorer_line(layout, index)`（0 はルート名の行）・逆の `explorer_line_at(layout, x, y)`（窓の当たり・`EditorWindow::explorer_row_at` と載せた行）・`explorer_root_name`・`explorer_row_parts(layout, line, depth, unsaved)`（丸い面・山形の升・印の升・名前・未保存の丸の型 `ExplorerRowParts`）・`explorer_guide_x`（深さの縦の線）。描画と当たりが同じ値を読む。
3. **色。** 地は `title_bar`、境界は `panel_border`（焦点で `accent`）、選んだ行は `background` の丸い面に 1 DIP の枠（焦点で `accent`・焦点が無いときは `panel_border`。後者は設計席の決め（2026-11-11・V-1428 の F-1）・試用で直す: 背景が黒のテーマでは `title_bar` = `background` になり、枠が無いと焦点の無いときの選んだ行が見えない）、載せた行は `tab_hover`（選んだ行には重ねない）、縦の線は `panel_border`、山形とフォルダの絵は `muted`、名前は `text`（隠しは `muted`）、未保存は `accent`。ファイルの印は構文の色で、言語 → 役割の表は core の `explorer_mark_role` の 1 本（`language_of_path`・C / C++ のヘッダは `header_of_path`・表にない言語は値なしで `muted`）。新しいトークンは足さない。
4. **載せた行は application の状態。** 意図 `HoverExplorer{line}`（見えている行の列の位置・ルート名の行と面の外は値なし）は帯の `PointTitleBar` と同じく使う人の操作ではない（`not_user_operation` の 1 本・歩きを続け、知らせとダブルクリックの印を消さない）。通常モードの ↑ ↓ が狙う横位置（`keeps_wanted_x`）も、`PointTitleBar` と帯の幅の知らせ `TitleBarWidth` と並べて保つ（レビュー V-1428 の M-1 と F-3・D82。本文の寸法と DPI を運ぶ `VisibleLines`・外観の `RefreshAppearance`・ホイールの `ScrollTabs` `ScrollExplorer`・全面プレビューの `PointPreview` は捨てる側のまま）。画面の行で覚えるので、ホイールで行が送られるとポインタの下の別の行が載った行になる。見えている行の外から外へ動いただけなら描き直さない（L-3・値は画面の行のまま覚え、窓が送り直さない）。窓は WM_MOUSEMOVE で変わったときだけ送り（`explorer_hovered()` の安い読み）、WM_MOUSELEAVE で値なしを送る。境の上（左右の矢印のカーソル）と本文・全面プレビューの選択のドラッグの間は値なし（L-4）。面が消えたら `sync_explorer` が消す。
5. **アクティブと未保存の出どころ。** `ExplorerRowView` の `active`（今の文書と同じファイル）・`unsaved`（アクティブか脇の束のタブのどれかが未保存・判定は `save_state_of` とタブの表示値）・`mark`。同じファイルかは `open_tab_of` と同じ `FilePort::same_file`。写すのは見えている行だけ（費用は画面の行数）。
6. **字。** 名前はタブの題名と同じ UI の字体と大きさ（renderer の `create_explorer_formats`・本文の書式の作り直しでは作り直さない）。ルート名とアクティブな文書の行は semibold の書式。行の文字組みは `PaletteRowLayouts` の保持のまま（書式は鍵に入る）。山形とフォルダの絵は線（丸い端と継ぎ目の stroke style）で描き、文字の layout を作らない。stroke style と本文の角の 2 つの形（path geometry）は renderer が `ExplorerShapes` に保持し、描画先の factory と角の鍵（面の右端・上端・半径・境界の太さ）が変わったときだけ作り直す（幅のドラッグ・DPI）。書式の作り直し（`create_text_formats`）と面を消したときに捨てる（レビュー V-1428 の L-1）。
7. **レビュー V-1391b の 2 つ。** R-a: 面のホイール `ScrollExplorer` も `keeps_explorer_click` に入れ、1 打目と 2 打目の間のホイールで 2 打目と組む印を消さない（フォルダが開いて閉じる、にならない）。R-b: 2 打目（DBLCLK）と押した印の無い離しは知らせを消さない（`keeps_explorer_notice`・1 打目で開けなかった 1 行が残る）。押しは新しいクリックなので消す。「押した印の無い離し」は 2 打目の後の離しだけでなく、本文で押して面の上で離した場合も含む（知らせを残すだけで害は無い・レビュー V-1428 の L-5）。
8. **面が消えているときの道は不変。** 打鍵と描画の道に分岐を足さない（`explorer_view` は面が消えていれば値なしのまま・描画は面が無ければ何もしない）。マウスの動きは面が消えていれば controller の 1 欄を読むだけ。
9. **契約。** core `--explorer-layout`（`verify_steps_and_dpi` `verify_panel_lines` `verify_rows_and_parts` `verify_line_at` `verify_mark_roles`）・application `--explorer`（`verify_hover` `verify_pointer_keeps_wanted_column` `verify_row_marks` `verify_double_click_across_wheel` `verify_double_click_keeps_notice`）・ui `nib_window_tests --explorer`（WIC の描画先で地・境界・角・選んだ行と焦点の有無の枠・載せた行・印の色・未保存の丸・semibold の画素・線と角の形の保持）。実機の目視は施主の試用。
10. **やらないこと。** ファイルの種類ごとの絵のアイコン・git の状態の色・右クリックのメニュー・行のドラッグ・git の枝の名前。

## 追補（2026-11-11・#1450・施主の報告）ルートのフォルダ選びの箱に WSL のディストリを場所として足す

施主の報告（2026-11-11）: 決定 7 の `FOS_PICKFOLDERS` の箱の左の一覧に WSL の場所（`\\wsl.localhost\<distro>`）が出ず、アドレス欄に直打ちしないと選べない。Issue #1450 の候補 A（`IFileDialog::AddPlace`）を、箱を出さない実測（`D:/NeNeNib/outputs/20261111/wsl-root-probe.md`）で確かめてから採る。

1. **場所はディストリごと。** 入れ終わったディストリ（レジストリの `State` が 1）ごとに `\\wsl.localhost\<名前>` を登録の順で箱の左の一覧の下（`FDAP_BOTTOM`）に足す。どれを足すかは core の純関数 `wsl_places(span<const WslDistribution>)` の 1 本（名前が空・`.` `..`・`\ / : * ? " < > |`・制御文字を含むものと同じ名前の 2 つ目は外す）。`\\wsl.localhost` の 1 つ（Linux の名前空間・非ファイルシステム）は、`FOS_FORCEFILESYSTEM` の箱で歩けるかを `Show` を呼ばずに確かめられないので採らない。ディストリの item は `SFGAO_FILESYSTEM` を持ち、そのまま選べる（実測）。Docker が内部で使う `docker-desktop` と `docker-desktop-data` は名前で外す（core の `wsl_places` の表・ASCII の大小を無視した完全一致・前方一致にはしない）。同じ名前の 2 つ目も同じ比べ方で外し、場所は登録の順の先頭から最大 16 件（`wsl_place_limit`・超えた分は捨てて知らせない）。名前は 255 バイト（`wsl_name_limit`・経路の 1 節とレジストリの副鍵の名前の上限）までで、adapters はそれより大きい値を読まずに外す。
2. **一覧はレジストリから・箱を出すときだけ。** application の `WslDistributionPort`（`distributions()` → `expected<vector<core::WslDistribution>, WslListFailure>`）を adapters の `Win32WslDistributions` が `HKCU\Software\Microsoft\Windows\CurrentVersion\Lxss` の副鍵の `DistributionName` と `State` から読む（実測 0.13 ms・`wsl.exe -l -q` は process を起こして 272 ms）。鍵が無いのは失敗ではなく空の一覧。合成ルートが `TimingPort` と束ねた `WindowPorts` で ui の `EditorWindow` に渡し（引数は 4 つまで・`readability-function-size`）、ui は操作 `choose_explorer_root` の中でだけ読む。起動・開く・打鍵の道では読まない。
3. **失敗は黙って足さない。** 一覧が読めない・UTF-16 に戻せない・`SHCreateItemFromParsingName` か `AddPlace` が失敗した場所は外して箱を出す（知らせない）。箱を出す前に待つ形にしない。`SHCreateItemFromParsingName` は経路を解くだけで中身を列挙せず、WSL の VM が止まっていても起こさなかった（実測・無いディストリの名前でも S_OK になるので「あるか」はレジストリで決める）。process で最初の UNC の解決に約 65 ms（2 回目から 0.1〜0.3 ms・`Show` の中で同じ費用が出るかは未確認）。
4. **OS に触れる所。** レジストリは adapters（`advapi32` は adapters だけ・ARC-007）、`SHCreateItemFromParsingName` と `AddPlace` は ui の `FileDialog` の `choose_folder(owner, places)`（COM は `ComPtr`・CPP-017）。
5. **契約。** core: `nib_tests --wsl-places`（入れ終わったものだけ・登録の順・使えない名前・同じ名前・UTF-8 の名前・空）。adapters: `nib_adapter_tests --wsl`（試験の根 `Software\NeNeNib\test-<pid>\Lxss` に同じ形の鍵を書いて読む・鍵が無ければ空・名前の無い副鍵は外す・本当の `Lxss` は読まない）。箱の左の一覧に実際に出るか・名前とアイコン・クリックで VM が起きるかは箱を出さないと見えないので、施主の試用で確かめる（自動の試験は無い）。
6. **限界。** Windows 11 が箱の左の一覧に自分で「Linux」を出さない理由は確かめていない。古い Windows 10 で `\\wsl.localhost` が無い（`\\wsl$` だけの）機械は考えない。候補 B（最近のルート・フォルダを窓に落とす）と C（`:set root=` に Linux の書き方）は後続。

## 追補（2026-11-11・#1495・施主決定）ルートのフォルダを選ぶ鍵は Ctrl+Shift+R

決定 7 の「ルートのフォルダを選ぶのは鍵なしで F1 の一覧から」を改め、`choose_explorer_root` に **Ctrl+Shift+R**（通常 / Vim の両モード）を割り当てる。行は `operation_bindings` の 1 行（`OperationKey::r` を足した）で、F1 の一覧と起動画面の「フォルダを開く」の行（ADR 0172 の追補 27）は表から鍵を引く。窓は Vim の鍵の前に操作の表を引くので、Vim の NORMAL / INSERT / VISUAL と INSERT の `Ctrl-r` の待ちでも Ctrl+Shift+R は engine に流れず、Shift なしの Ctrl+R は表に行が無いので Vim の redo とレジスタの挿入のまま（Ctrl+Shift+E の `toggle_explorer` と同じ道）。入力行の間は入力行の鍵（`press_command_control_key`）が先に受け、Ctrl+Shift+E と同じく何もしない。契約: `nib_tests --operations` の表・`--vim-fixture-window-path` の Ctrl+R の素通し・`--explorer` の `verify_root_choice_keeps_vim`・`nib_window_tests --standard-keys` の `verify_folder_root_key`・`--start-screen`。
