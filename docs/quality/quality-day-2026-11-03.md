# 2026-11-03 — 10-30 の列の続き・表の PR の乗せ替え・fuzz 生成器 4 の seed 11〜20（記録）

設計席 Fable（Nibリナ）。hide の指示「日報と引き継ぎ書を読んで状況を把握して。実装の続きをお願い」（01:14）。hide「今日はほかの子も走ってるから計測は正確じゃないかも」→ 速さの計測（main のゲート 8 本・試用 exe の基準比べ）は今日は回さない。列・fuzz・乗せ替えは計測ではないので続ける。

## 0. 再開時の状態（01:14）

- 10-30 の夜に置いた列は #1162 → #1143 → #1163 → #1164 → #1167 → #1168 まで main（origin/main `1659da85`・10-31 00:54）。**#1169 で止まっていた**: `merge-pr.py` の出力が cp932 で件名の `ß` を書けず `Merge refused: 'cp932' codec can't encode character '\xdf'`。PR は乗せ替え・push・Ready 済み（HEAD `7afed999`・origin/main の上）。
- 直し: `merge-chain4.sh` / `build-probe-then-merge2.sh` の `python eng/merge-pr.py` と resolver の呼び出しを `python -X utf8` に（10-30 の教訓「heredoc の Python は `-X utf8`」と同じ根）。`eng/merge-pr.py` 自体は変えていない（道具の PR は別途・stdout を UTF-8 に `reconfigure` する 1 行で済む）。
- 本体の checkout は `21efae0b`・未 commit なし。worktree は 10-30 の分がそのまま（30 本余り・merged head と同じものは後で `--force` で外す）。

## 1. 列と席（随時・時刻はファイルの mtime と GitHub の createdAt から）

| # | 種類 | 状態 |
|---|---|---|
| #1169（#990 大小の写像の残り） | 列 | 01:16 `resume-1169-then-queue.sh` で merge-pr を `-X utf8` でやり直し（CI run 41479871936）→ **main `8a4c2d6f`**（01:42） |
| #1171 → #1172 → #1155 → #1175 → #1176 → #1179 → #1180 → #1184 | 列 | #1169 の後に `chain-queue4.sh` で自動（fixture の接頭辞: `word-class-` / `insert-count-join-` / `insert-arrow-` / `search-composing-start-`）。#1171 **main `875b5d11`**（02:28・1 本 45 分: 乗せ替えの手元ビルド＋CI）・#1172 **main `32809822`**（03:11） |
| #1152（N5a 全面プレビュー・表の PR の 1 本目） | 席 R1（Opus） | 乗せ替え: 衝突 0（前の基 `ff5f78e6` の後に入ったのは #1147 だけ）。**手元ビルドが関数長の門で落ちた**: `run_pane_operation` が 61 行（#1127 の 5 値と #1049 の 2 値が「何もしない case の列」に足され、この PR の `preview_full` で 61）。設計席の判断 **A**（case の中のコメント 1 行を関数の頭へ・60 行・振る舞いは同じ）。根本は **#1188** に起票（下）。手順 7 の先読み: #1184 の後に `ExCompletionProbe.cpp` の写しへ `preview full` `preview side` `preview off` を `set scrolloff=` の直後（添字 39〜41）に足す。**01:43 席の報告**: A の commit `b64b0469`（4 commit）・席が見つけた食い違い 1 つ（`run_sent_operation` の switch に `preview_full` が無く `-Wswitch` で落ちていた・前の乗せ替えで生じ tidy が先に落ちて見えなかった → 何もしない列に 1 行）・既定実行 2,332,769・`--preview-full` 81・`--operations` 1,605・probe 10 workload ok・conformance 0・symbols 0・format ok。設計席が `--force-with-lease` で push。merge は #1184 の後（写しに 3 行を足してから） |
| #1188（新規） | 設計 | `run_operation` の分岐を `constexpr` の表（操作 → メンバ関数ポインタ）にし、群の関数と「何もしない case の列」をなくす。網羅性は `static_assert`（`operation_texts.size()` と行の順）。CPP-012・ADR 0078 追補。依頼書 `brief-G4-1188-run-operation-table.md`。01:46 ごろ席 G4（Opus）を #1152 の枝の上（stacked・base `feat/1052-preview-full`）で出した → **PR #1191**（02:07・HEAD `180d3aa8`・`EditorWindowOperations.cpp` 706 → 590 行・群の関数 5 つを消し `runner_of`（関数内 `static constexpr std::array<OperationRunner, operation_texts.size()>`・行は enum の順・`static_assert` は大きさと `runners_in_order`）と `run_<operation>` 29 個・`run_operation` は 1 行・最長は `runner_of` の 45 行・既定実行 2,332,769・`--operations` 1,605・`--split` 275・probe 10 ok・conformance 0・symbols 0・format ok・ADR 0078 追補 4 項・CLAUDE.md §2 に 1 文）→ 独立レビュー（`review-1191`）: 止める 0・直してから 0・後続 4（常に真の `static_assert`・記録の行）→ 同じ席が直した（HEAD `0fbf14e8`）。**ui の差分は `nib_tests` と probe を通らない**ので、merge 前に `eng/verify-window.py` の操作の節を回したい（窓を最前面に出す → hide に確認・03:05「いま回していいよ」）→ **設計席が回した**（03:06〜03:20・PR の Debug の exe）: 既定の節・`--tabs` `--split` `--preview` `--preview-full` `--find` `--palette` `--explorer` の 8 本。既定の節の 1 回目だけ配色の面の `Enter` の look で落ちた（`Enter did not keep the chosen scheme`・面が閉じる前に読んだ形）→ #1152 の exe でも 1 回目は同じ所で落ち 2 回目 pass・main `ff5f78e6` の Release は 1 回で pass・本 PR の exe も 2 回目 pass → 本 PR の退行ではなく Debug と負荷（ほかの席が走っている）の揺れ。PR 本文に記録。**#1192 の後の列に置いた**（`after-1192-merge-1191.sh`・`rebase --onto` で base を main に） |
| #1149（INSERT の端の縦の矢印） | 席 I27（Opus） | 01:28 ごろ出した → **PR #1190**（01:57・HEAD `8002da7d`・`arrow_stays(view, key)` を判定の 1 本に・`<Up>` は最初の論理行・`<Down>` は最後・PgUp / PgDn は `page_stays`（`page_top` が先頭行と同じ・Vim の `onepage` の FAIL）・fixture `insert-arrow-vertical-` 14 件（修正前 9 件落ちた）・`--vim-view` 88・`--vim-open-lines` 706・既定実行 2,332,741・ADR 0028 追補・窓の大きさ不明 / ui が解いた移動 / 格子は従来どおり切る（残るリスク））→ 独立レビュー（`review-1190`）: 止める 0・直してから 2（動く側の契約の対・HEAD の exe での記録）・後続 5 → 同じ席が直した（HEAD `155aa2cc`・既定実行 2,332,745・`--vim-view` 89・`--vim-open-lines` 709）→ #1152 の後の列に置いた（02:12） |
| #1170 | 席 I28（Opus） | 02:03 ごろ出した → **PR #1192**（02:22・HEAD `22d74717`）。**依頼書の見込みが違った**: Vim は「置き換えた後の同じ行の同じバイトの桁を行末までに詰め、文字の途中なら文字の頭へ」（`check_cursor_col` / `mb_adjust_cursor`）で、「caret より前の body のバイト数で写す」ではない（`İİx` で `jVklu` → Vim は `x`）。席が実測に従った（正しい動き）。`cased_caret` の 1 本・fixture `case-rest-visual-` 12 件（行単位 9・`guj` 1・文字単位 1・矩形 1・修正前 5 件落ちた・文字単位と矩形は差なし）・CRLF の契約 1・`--vim-case` 313・既定実行 2,332,824・ADR 0015 決定 10 追補 → 独立レビュー（`review-1192`）: 止める 0・直してから 1（HEAD の exe での記録）・後続 5 → 同じ席が直した（HEAD `8884ccf4`・R3-2 の Ctrl-r は oracle で測って差なし）→ #1190 の後の列に置いた（02:38） |
| #1173 | 席 I29（Opus） | 03:16 に出した（#1172 が main に入った後） |
| #1189 | 席 I30（Opus） | 03:16 に出した（fuzz の新規・矩形の `A` と Tab の詰め物） |

## 2. fuzz 生成器 4 の seed 11〜20（引き継ぎの手順 4・main `1659da85` の再生器）

`D:\NeNeNib\outputs\20261103\vim-fuzz-gen4-seeds11-20\summary.md`。3,000 件: 拒否 205・一致 2,781・不一致 14（既知 6: #781 1・#985 5・新規 8）。

新規 8 件の仕分け:

- 6 件（`fuzz-11-0278` `14-0016` `15-0247` `16-0277` `20-0150` `20-0199`）: 矢印の後に何も打たずに Esc した INSERT の後の `.` → **#1137**（PR #1155 が列にある・`--known` に無い語（`i` `a` `A` の矢印つき）で出た）。
- 1 件（`fuzz-13-0076`）: `<C-v>0e` が国旗の 2 文字目（🇵）に止まらず `r` まで行く → **#985**（語の種類の表・PR #1171 が列にある。`--known 985` の語に `<C-v>` が無いので新規と出た）。
- 1 件（`fuzz-12-0041`）: 矩形の `A` で右端が Tab の途中・短い行の詰め物を Vim は `noexpandtab` の形（Tab）で作る → **#1189**（新規・低）。

残りの不一致率: 新規の種類は 3,000 件に 1 件（0.03%）。生成器 4 の seed 1〜20 で見つけた種類は全部 Issue か PR にある。

## 3. hide の試用（03:30 ごろ・`NeNeNib-main-ff5f78e6.exe`・10-30 の表の 16 項目のうち exe に入っている 12 項目）

hide「見た。良さそう」。見たもの: 1（Ctrl+F / F3 / Shift+F3）・3（Ctrl+G）・4（F1 の「検索」「行へ移動」）・5（Ctrl+ホイール）・6（案内の箱）・7（文字コードと改行の選択）・10（`gI`）・11（`:set so=3`）・12（`30G` の中央寄せ）・13 の前半（INSERT の `<Left>` `<Right>`）・14（`/e` と `é`）・16（DEL は 1 桁）。残り（2 置換・8 全面プレビュー・9 印刷・15 `G<C-o>`・13 の後半・今日の #1190 / #1192）は列が引けた後の main の Release で。

## 4. 区切り（03:30・hide「今日はそろそろ落とそうと思う」）

- main は `32809822`（#1169 #1171 #1172）。列と待ち受けは止めずに置いた（機械を落とすと止まる → 引き継ぎの再開の手順 1）。席 I29 / I30 は進行中（作業木に残る）。
- 文書 PR: 記録・日報・引き継ぎ・`current.md`・CLAUDE.md §6。
