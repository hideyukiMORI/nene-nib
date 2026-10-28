# ADR 0166 — 変更のある行の印・indent guides・yank の光りは、見えている行だけから作る frame の値

- 状態: 受理（施主決定 D67・FR-025・2026-10-28。実装は 3 席（印 → guides → 光り）・受理は D41 の形）
- 日付: 2026-10-28
- Issue: #1002（変更の印）・#1005（indent guides）・#1006（yank の光り）・計画 `docs/plans/2026-10-28-editing-features-roadmap.md` §1 の 4・7・8
- 規則: FR-025、ARC-001 / ARC-003 / ARC-004 / ARC-011、CPP-002 / CPP-005 / CPP-011、QLT-014、ADR 0008（色のトークン）/ 0037（当たりの面）/ 0040（制御文字の桁の表）/ 0052（undo）/ 0121（改行の索引）/ 0137（タイマーの意図は使う人の操作ではない）/ 0152（編集で位置をずらす `EditAdjust`）/ 0156（見えている行の走り）

## 文脈

gitsigns の印・indent-blankline の線・highlight-yank の光りは Neovim の設定に最も多く入る見た目の 3 つ。Nib の frame は見えている行の `LineView` の列で、当たりの面・構文の区間・置き換えの tint を同じ道で描く。D67 の速さの条件: 既定オンの機能は 1 打鍵の +1% まで・全文を走らない・frame を増やさない。

## 決定

### 変更のある行の印（#1002・既定オン）

1. **基準は最後に保存した本文。** git は読まない（後続・LSP L0 の子プロセスの土台の後）。文書の値 `ChangedLines`（行番号の集合 ＋ 「削除の位置」の行番号の集合）を `DocumentState` に 1 つ。保存で空になる（`●` の未保存の印と同じ時点・保存の `revision()` を覚える）。
2. **増分で持つ・全文の差分は取らない。** `replace`（編集の 1 か所）で `Edit` の `at` `removed` `inserted` から「変わった行の範囲」と「行の数の増減」を求め、`ChangedLines` を更新する（後ろの行番号は増減でずらす・`EditAdjust` の規則と同じ 1 本・ADR 0152）。undo / redo は同じ道（`Edit` を逆に当てる）。費用は 1 編集に O(編集の行数 ＋ 集合の大きさの対数)。保存の時点の本文と同じ内容に戻っても印は残る（限界・gitsigns と違う・差分を取らないため）。
3. **見せ方。** 行番号の列（gutter）の左端に 2 DIP の縦の帯: 変更・追加の行は `accent`、削除があった位置（次の行の上端）は `muted` の短い横線。`LineView` に `ChangeMark{none, changed, deleted_above}` を 1 欄。色のトークンは足さない。16 MiB 超の文書も同じ（集合は編集の数に比例・本文の大きさに依らない）。

### indent guides（#1005・既定オフ・設定 v4）

4. **線は見えている行の先頭の空白だけから。** 行の先頭の空白（Tab は `vim_tab_stop` の 8・空白は 1）の升の数を `display_line` の桁の表から数え（ADR 0040・1 行 O(先頭の空白の長さ)）、`shiftwidth`（8・ADR 0015 の決定 11）ごとに 1 DIP の縦線を `panel_border` で描く（`DrawLine`・layout を作らない）。空行は前後の行の深さの小さい方に合わせる（indent-blankline と同じ）。`:set indentguides` / `noindentguides`。オフで分岐なし。

### yank の光り（#1006・既定オン）

5. **タイマーの意図 `HighlightTick`。** `y` の効果を写した直後、yank した範囲（`VimStep` が添える `yanked` の範囲・行単位は行全体・矩形は升）を `EditorState.flash`（`OffsetRange` の列 ＋ 期限の券）に置き、ui が 150 ms の 1 回のタイマー（`SetTimer`・`AutosaveTick` と同じ合図の形）で `HighlightTick` を送って消す。`keeps_tab_walk` と `begin_intent` に「使う人の操作ではない」と書く。frame は光る 1 回と消える 1 回の 2 回だけ。次の編集・移動で先に消える。
6. **見せ方。** `LineView.matches` と同じ面（`search` の色）に載せる（新しい欄も色も足さない）。検索の当たりと重なれば検索が勝つ（当たりの面は 1 つ）。

### 共通

7. **契約。** `--change-marks`（編集の種類 × 位置 × 行の増減・undo / redo の往復・保存で空・CRLF・性質の試験: ランダムな編集の列で `ChangedLines` が「保存の時点から触った行の集合」の素朴な模型と一致）・`--indent-guides`（Tab と空白・空行・shiftwidth・巨大な行は無し）・`--yank-flash`（`y` の種類 3 つ・tick で消える・移動で先に消える・検索の当たりと重なる）。ui: `nib_window_tests` の画素 3 件。
8. **費用。** 変更の印は 1 編集 O(log n)・frame は見えている行の集合の照合 O(見えている行 × log n)。guides は見えている行の先頭だけ。光りは frame 2 回。既定オンの 2 つ（印・光り）は空の文書の 1 打鍵の A/B（+1% の線）を PR 本文に書く。任意のベンチ `change-marks-burst-200`（200 打鍵で集合が育つ）。
9. **限界。** git の差分（後続）・印から跳ぶ鍵（`]c` `[c`・後続）・保存の時点の本文に戻っても印が残る・indent guides の `listchars` 相当の設定・光りの時間の設定。

## 強制

- 全文の差分を取らない（増分）: **active**（性質の試験 ＋ 契約）。
- 8 本の速さと +1% の線: **active**（QLT-014・PR 本文の A/B）。
- 色のトークンを足さない: **active**（ADR 0008 のレビュー事項）。

## 結果

得られるもの: 保存してから触った行が一目で分かる。字下げの段が見える。`y` した範囲が確かめられる。
失うもの・残る穴: 印は「触った行」で「差分」ではない（戻しても残る）。git の印は後続。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| 保存の本文との LCS の差分を打鍵ごとに | 16 MiB で止まる（D67） |
| 光りを専用の色のトークンで | ADR 0008 のトークンを増やさない。`search` の面で足りる |
| 光りを frame のたびに時刻で判定 | 時計は adapters だけ。タイマーの意図 1 回で消す |
