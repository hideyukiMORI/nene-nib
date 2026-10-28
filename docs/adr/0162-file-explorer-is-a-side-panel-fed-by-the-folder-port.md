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
