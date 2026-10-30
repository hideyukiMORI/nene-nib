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

1. **面の行数は窓の client の高さから導く。** E4 の追補 1 の「本文の `visible_lines` − 1」をやめる。上下の分割（ADR 0148）では操作中の枠の `visible_lines` が本文の半分の高さになり、窓の全高に出る面の下半分が空いていた。窓は `VisibleLines.explorer_lines` に「面の高さ（本文の帯の全高）に入る行数（ルート名の行を含む）」を載せ、数えるのは core の `explorer_panel_lines(client, dpi, font_size)` の 1 本（面が出ているか・分割に依らない）。application は `explorer_page()` の 1 か所で「それ − 1（ルート名の行）・最小 1」を導く。`explorer_lines` が 0（寸法が届く前）のときだけ本文の見えている行数を面の行数とみなす。契約は `--explorer` の `verify_panel_rows_from_window`、core は `--explorer-layout` の `verify_panel_lines`、画素は `nib_window_tests --explorer` の `verify_split_rows`（上下の分割でも面の最後の行に名前がある）。ホイールの 1 画面の設定（`WHEEL_PAGESCROLL`）の送り量は本文の行数のまま（後続）。
2. **行の文字組みの保持は鍵の hash で O(1) に引く。** `PaletteRowLayouts` の鍵の形（書式・役・DPI・欄の幅と高さ・文字列）はそのままで、鍵を 1 本のバイト列に畳んで `unordered_map` で引き、最後に引いた順の `list` で上限を越えた末尾（最も前に使ったもの）を捨てる。エクスプローラーの上限 512 は不変。同じ型の Ctrl+P の面とタブの題名の実体も同じ引き方になる（捨てる順の契約 `verify_capacity` は不変）。
3. **費用は `nib_perf_probes` の workload で前後を測る。** `explorer-typing-shown-1000-64`（面を出して 1,000 件を読んだ後に 64 打鍵・各打鍵で frame）と対の `explorer-typing-hidden-64`。任意のベンチは足さず（D60 と同じく 8 本と混ぜない）、`eng/compare-probes.py` の名前の表にも載せない。probe は application までなので、決定 2 の ui の保持の引き方はこの probe の区間に入らない。
