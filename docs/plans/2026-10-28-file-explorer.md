# サイドのファイルエクスプローラーの計画（案）

2026-10-28 / hide の指示（外から・「サイドにファイルエクスプローラーの実装もしたい。ドキュメントルートを設定してそこからツリー状に展開する簡易なもので、ショートカットとコマンドで表示をトグル」）/ 設計席（Nibリナ）。
SPECIFICATION に無い新しい機能なので、始めるなら FR と D 番号が要る（§4）。見た目と鍵は hide の判断（§3 の案に推しを付けた）。

## 0. 要点

| 問い | 設計席の決め |
|---|---|
| 形 | 本文の左に幅 240 DIP の面（案 A）。ルートのフォルダからツリー。フォルダは開いたときに裏で読む（既存の `FolderPort` の worker を広げる） |
| 出す・消す | 鍵 Ctrl+Shift+E（通常と Vim の全モード・ブックマークの Ctrl+Shift+D と同じ形）と Ex `:set explorer` / `:set noexplorer` / `:set explorer?`。F1 の一覧にも「エクスプローラーを出す・消す」 |
| ルート | 設定に保存（`settings.v4` の `root`）。`:set root=<path>`・`:set root=.`（アクティブな文書のフォルダ）・`:set root?`。F1 の一覧の操作「ルートのフォルダを選ぶ」（フォルダを選ぶダイアログ・`FOS_PICKFOLDERS`）。未設定なら面に「`:set root=` でルートを決めてください」の 1 行 |
| 操作 | 面に焦点があるとき j / k / ↑ / ↓ で行を選び、l / → / Enter でフォルダを開き、h / ← で閉じる（閉じていれば親へ）、Enter でファイルを開いて本文に焦点が戻る。ダブルクリックも開く。シングルクリックは選ぶだけ（フォルダはクリックで開閉）。Esc で本文へ焦点だけ戻す（面は残る） |
| 保存するもの | 面の表示とルートだけ（設定）。開いているフォルダの状態と選択は保存しない |
| 大きさ | 1 フォルダ 2,000 件まで（超えたら末尾に「…」・`FolderProgress::truncated` と同じ） |
| 工程 | 5 段・ADR 1 本・4〜5 席日（推測）。設定の v4 は LSP（ADR 0160）と 1 回の版上げにまとめる |

## 1. 今ある土台

| 項目 | 現物 |
|---|---|
| フォルダを裏で読む | `FolderPort::list(FolderRequest{folder, ticket})` → `collect() -> FolderBatch{ticket, files, progress}`（ADR 0062・worker 1 本・1,024 件ずつ）。**ファイルだけ**でフォルダ（`FILE_ATTRIBUTE_DIRECTORY`）は返さない → 広げる |
| 面の色 | `Palette` の `panel` `panel_border` `current_line` `muted` `accent` `text`（ADR 0008 / 0150・色のトークンは足さない） |
| 本文の配置 | `core::pane_layouts(client, dpi, font_size, split)` が本文の枠（1 つか 2 つ）を決める 1 本（ADR 0148）。面はこの `client` を左から縮める形で足す |
| 鍵と操作 | `EditorOperation` の表（`operation_texts` `operation_bindings`・ADR 0078）。`OperationKey` に `e` は無い → 足す。Ctrl+Shift+E は通常・Vim とも空いている（Vim の Ctrl-E は shift 無し） |
| 設定 | `settings.v3`（`font_size` `font_family` `theme` `guide` `wrapping`）。v4 は LSP の `lsp` と同じ版上げで（ADR 0114 の移行の型・1 回で 2 つの機能の欄を足す） |
| 開く道 | `open_document` の 1 本（ADR 0056 の決定 5・ADR 0159 で port 化） |
| 焦点の切り替え | 面（Ctrl+P）は鍵を全部奪う既存の形（`CommandPalette`）。エクスプローラーは「面に焦点があるかどうか」を `EditorState` に 1 つ持ち、本文の鍵と分ける |

## 2. 形（ADR で確定する）

- **core**: `ExplorerTree`（純粋な値・ノードの列: 親・深さ・名前・フォルダか・開いているか・読み終えたか・`truncated`）。`visible_rows()`（開いているフォルダの子だけを深さ優先で並べた行の列）・`toggled(row)`・`selected(row)`・`loaded(node, entries)`（application が裏で読んだ子を置く）。並びはフォルダが先・名前は ASCII の大小を畳んだ順（OS の順には依らない・決定性）。`ExplorerLayout`（面の幅 240 DIP・行の高さは本文と同じ・字下げ 12 DIP × 深さ・▸ / ▾ の印は文字で描く・幅は `min(240, client の 40%)`・窓が 360 DIP の最小では本文を 120 DIP 残す）。Ex: `set explorer` / `set noexplorer` / `set explorer?` / `set root=…` / `set root=.` / `set root?`（`ExResult` の `settings` の欄に載る＝保存する値）。
- **application**: `EditorState` に `ExplorerState{ExplorerTree tree; std::optional<std::size_t> selected; bool focused;}`（窓に 1 つ）。意図 `ToggleExplorer`（出す・消す・焦点の規則は §0）・`ExplorerKey{key}`（j k h l Enter Esc 矢印）・`PointExplorer{row, click 種類}`（押した・離した・ダブル）・`ScrollExplorer{lines}`。フォルダを開くときに `FolderPort::list` を「子の一覧（フォルダとファイル）」で頼み、`WorkCompleted` → `collect` → `loaded`。ファイルを開くのは `open_document`（失敗は 1 行の知らせ・`open_listed` と同じ）。`verify_quiet_paths`: 打鍵・切り替え・保存で `FolderPort` を呼ばない（面を開いたときとフォルダを開いたときだけ）。
- **adapters**: `FolderRequest` に `FolderListing{files, entries}` の閉じた enum を足し、`entries` のときは `FolderBatch` に `std::vector<FolderEntry{FilePath path; bool folder;}>` を返す（`FindFirstFileExW` の属性で判定・`.` `..` を除く・隠し属性は返すが面では `muted`）。Ctrl+P の同じフォルダの候補は `files` のまま。
- **ui**: `pane_layouts` の `client` を面の幅だけ右へ縮め、面の矩形を renderer が描く（地は `panel`・境界 1 DIP は `panel_border`・選んだ行は `current_line`・焦点があるときの境界は `accent`・フォルダ名は `text`・ファイル名は `text`・隠しは `muted`・印 ▸ ▾ は `muted`・ルート名は面の先頭に `muted`）。マウスは `title_bar_released` と同じ「押した行と離した行が同じときだけ」。ホイールは本文と同じ `wheel_turned`。IME は面に焦点があるとき閉じる（Ctrl+P と同じ構え）。
- **描画の費用**: 面の行の layout は `PaletteRowLayouts` と同じ保持（#904）。面が無いときの打鍵の道は変えない。

## 3. 見た目の案（hide が選ぶ・/design の採用案は選んだ後に書く）

### 案 A（推し）: 左の面・幅 240 DIP・印は文字

```
┌──────────────────────────────────────────────────────────────┐
│ ● main.cpp   README.md   ＋                          ─ □ ×  │ タイトルバー（今のまま）
├───────────────┬──────────────────────────────────────────────┤
│ NeNeNib       │  1 #include "EditorWindow.hpp"               │ ← ルート名（muted）
│ ▾ docs        │  2                                           │
│   ▾ adr       │  3 namespace nenenib::ui::win32              │
│      0001-…md │  4 {                                         │
│      0002-…md │  5 ...                                       │
│   ▸ plans     │                                              │
│ ▸ src         │                                              │
│   CLAUDE.md   │                                              │
│ ▸ tests       │                                              │
│   …           │                                              │
├───────────────┴──────────────────────────────────────────────┤
│ 通常 | Vim  NORMAL                 行 3, 桁 1  UTF-8  LF  cpp │
└──────────────────────────────────────────────────────────────┘
```

- 面と本文の境界は 1 DIP（`panel_border`・面に焦点があるとき `accent`）。選んだ行は `current_line` で塗る。長い名前は末尾を「…」で切る（`tab_title_for` と同じ）。
- 良い点: 既存の色と layout の部品だけで作れる。VS Code / Neovim の netrw と同じ場所で迷わない。
- 悪い点: 窓が狭いと本文が狭い（360 DIP の最小では面を 40% に縮める）。

### 案 B: 右の面

同じ中身を右に置く。良い点: 行番号と本文の左端が動かない。悪い点: タブの帯の「∨」と窓の操作の下に来て目が往復する。

### 案 C: 浮く面（Ctrl+P の面と同じ形）

本文の上に浮く面で、ファイルを開くと消える。良い点: 本文の幅が変わらない。悪い点: 「ツリーを見ながら編集する」ができず、hide の「サイドに」から外れる。

### 鍵の案

| 案 | 出す・消す | 面の中 |
|---|---|---|
| **推し** | Ctrl+Shift+E（隠れている → 出して焦点・出ていて焦点あり → 消す・出ていて焦点なし → 焦点だけ） | j k h l Enter Esc と矢印 |
| 別案 | Ctrl+B | Vim NORMAL の Ctrl-B（1 画面上）と衝突。採らない |
| 別案 | F2 | 空いているが VS Code では名前の変更。採らない |

## 4. 施主に確かめる点

| # | 問い | 設計席の推し |
|---|---|---|
| X-1 | 初版の範囲に入れるか（FR-022・D66） | 入れる（設定でオフにできる・既定は**消えている**） |
| X-2 | 見た目は案 A / B / C | A |
| X-3 | 鍵 | Ctrl+Shift+E（上の規則） |
| X-4 | ルートの決め方 | `:set root=<path>` / `:set root=.` と F1 の「ルートのフォルダを選ぶ」。未設定の既定は無し（面に案内の 1 行）。起動の引数のフォルダはルートにしない（引数はファイルだけ・今のまま） |
| X-5 | クリックの意味 | シングルは選ぶ（フォルダは開閉）・ダブルと Enter で開く |
| X-6 | 出すもの | 全部のファイル（拡張子で隠さない・隠し属性は `muted`）。1 フォルダ 2,000 件まで |
| X-7 | 順 | 分割（#992）の後、巨大 段 2 と並行してよいか（触るファイルが違う: 面は core / ui / `EditorControllerPalette`・段 2 は `EditorControllerDocuments` と port） | 並行してよい |

## 5. 工程

| 段 | 中身 | 席日（推測） | ADR | 前提 |
|---|---|---|---|---|
| E1 | ADR（この計画の §2 を決定に）・/design の採用案（hide が案を選んだ後） | 設計席 0.5 | **1** | X-1〜X-6 |
| E2 | core: `ExplorerTree` `ExplorerLayout`・Ex の `set explorer` `set root=`・`EditorOperation` に 2 操作・`OperationKey::e`・設定 v4（LSP の `lsp` の欄と一緒に 1 回で） | 1〜1.5 | E1 | 分割（#992） |
| E3 | application: `ExplorerState`・意図 4 つ・`FolderPort` の `entries`・`WorkCompleted` → `loaded`・`open_document`・`verify_quiet_paths` | 1〜1.5 | E1 | E2 |
| E4 | ui: 面の配置と描画・マウス・鍵・焦点・IME の構え・`verify-window.py` の節 | 1.5 | E1 | E3 |
| E5 | 試用（hide）と直し | 0.5 + 試用 | — | E4 |

計 4〜5 席日。受理は D41 の形（独立レビュー・8 本の速さに退行なし・CI・試用）。

## 6. 残る不確かさ

- 面の幅のドラッグ・ルートの履歴（最近のルート）・名前の変更や削除などファイル操作・面の中の絞り込み（文字を打って探す）は初版の外（後続）。
- フォルダの監視（`ReadDirectoryChangesW`・外で変わったら更新）は初版の外。面を出し直すか、フォルダを閉じて開けば読み直す。
- 2 枠（split）のときの面の位置（左の枠のさらに左）。本文の枠は `pane_layouts` の `client` が縮むだけなので変わらない見込み。
