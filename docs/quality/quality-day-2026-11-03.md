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

## 5. 午後（13:08〜・hide「日報と引き継ぎ書を読んで対応の続きをして」・機械を落とした後の再開）

### 5.0 再開時の状態（13:08）

- 列は 03:46 に **#1175 の乗せ替えの衝突**（`tests/unit/LspFramesTests.cpp`・main の #1022 と #1175 が同じ所に検証の呼び出しを 1 行ずつ足した）で止まり、その後に機械が落ちて待ち受け（`after-*.sh` / `wait-pr-then-queue.sh`）も消えていた。main は `e52fbf24`（#1155 まで）。
- 席 I29（#1173）は未 commit の差分 4 ファイルで、I30（#1189）は commit 1 つ・未 push で止まっていた。

### 5.1 列の再開

- #1175 は設計席が作業木で両方の呼び出しを残す形で解いて origin/main に乗せ替え（`604e7a38`）、`nib_tests` 2,333,193 checks・conformance 0 を手元で確かめてから `restart-queue-1175.sh` で #1175 → #1176 → #1179 → #1180 → #1184 を起動し直し、待ち受け 4 本（#1152 / #1190 / #1192 / #1191）も起動し直した。#1175 **main `ae195146`**（14:02）・#1176 main `e46b…`（14:33）。
- `_common.md` に 2026-11-03 の追記: 最後の編集の後に HEAD の exe で流し直す・機械が落ちて止まった席の続きは作業木を新しく切らない。

### 5.2 席（続きと新規）

| # | 席 | 結果 |
|---|---|---|
| #1189（矩形の `A` と Tab の詰め物） | I31（I30 の続き・Opus） | **PR #1195**（13:13・HEAD `25b5e5c8`・conformance / symbols / format 0）→ 独立レビュー `review-1195`（止める 0・直してから 0・後続 5）→ 設計席の判断で後続のうち R4-1（Esc で 1 行目を丸ごと写す）・R3-1 / R6-1（制御文字も写すか）・R1-1（判定を 1 か所に）・R5-1（回数つきと `<Left>`）を merge 前に同じ席が直した（HEAD `9135c74d`・Vim は切れた `^V` と LF 文書の CR も 1 バイトごと写す → 条件を「切れた文字が 1 バイト」に広げ ADR 0035 追補を直す・判定は入口の `after_straddled` の 1 か所で `VimBlockInsert.carried` に覚え Esc は `at` の手前の 1 バイトだけ `text_range` で読む・fixture 12 → 17・既定実行 2,333,210・`--vim-visual-block` 3,040）。列は #1197 の後（席 R5 で乗せ替え・下） |
| #1173（回数つき INSERT の後続 2） | I32（I29 の続き・Opus） | **PR #1197**（HEAD `cbf3e976`）。R3-1: 写しの読み始めを文書の位置 `at` から行頭からのバイトの桁 `at_byte`（Vim の textcol）に（前の行を消してから改行で 1 行目へ戻ると位置がずれていた・`j0<C-v>jI<BS><BS><CR>x`）。R3-2: 流し直しの後に 1 行目へ戻る列は 1 行目が伸びないので入口の問いは動かさず fixture 5 件。R3-3: CRLF の契約 3 列（ずれ無し）。R4-1: **巨大な 1 行で矩形の `I` の入口が約 60 s**（Debug・16 MiB・ふつうの INSERT は読むだけと同じ 2.3 s）→ Issue #1173 のコメントと **Issue #1198**。fixture `insert-count-join-2-` 16 件・既定実行 2,333,227 → 独立レビュー `review-1197`（止める 0・直してから 0・後続 5）→ ADR の 1 句・契約の注釈・format の記録を同じ席が直した（HEAD `bdd40529`）。**乗せ替えで #1195 の `carried` の読みと衝突する**（`line_start(first) + at_byte` から 1 バイト戻す形が正しく、`Offset{at_byte − carried}` でも今の fixture は通ってしまう → #1195 を後に置き、席 R5 が別の行から始める fixture 1 件を足して乗せ替える・`brief-R5-1195-rebase.md`） |
| #1150（置換 N1b・表の PR） | R2（Opus） | #1191 の枝（`0fbf14e8`）の上に `rebase --onto`（`run_operation` に `case` を足していないので表への書き直しは不要・main の `literal_pattern` の向きの引数に追随する commit 1 つ・HEAD `7ef72ec8`・既定実行 2,332,802・`--find-replace` 104・probe 10 ok・conformance / symbols / format 0）。設計席が push・base を `refactor/1188-run-operation-table` に。待ち受け `after-1195-merge-1150.sh`（`rebase --onto origin/main 0fbf14e8` → `build-probe-then-merge2`） |
| #1181（全面プレビューの選択 N5b） | 設計席 | `rebase --onto 0fbf14e8 32161981`（衝突は `EditorIntent.hpp` の和型の末尾 1 か所・`PointPreview` を足す）→ HEAD `68bf40cb`・既定実行 2,332,788・`--preview-full` 100・conformance 0 → push・base を #1191 の枝に。待ち受け `after-1150-merge-1181.sh` |
| #1157 → #1160（印刷 N6a） | R3（Opus） | #1191 の枝の上に乗せ替え、`print` を `runner_of` の表の 1 行と `run_print()` に（#1160 はその中にダイアログ → Cancel → `send`）。**`remaining_command` が認知の複雑さの門（11 > 10）に当たった**（#1152 の `preview_command` と #1157 の `print_command` が並んだ）→ 候補 A（`document_command_or_print` に束ねる・+14 行）を採り、根本は **Issue #1199**（解析関数の `constexpr` の表・#1188 と同じ形・列の後）。HEAD `e86abf97`（3 commit）/ `fc146332`（2 commit）・`--print` 42 / 48・`nib_window_tests --print` 25・既定実行 2,332,871 / 2,332,877・probe 10 ok。手順 7: 写しに足すのは添字 39〜42（`preview` の 3 つと `hardcopy`）。待ち受け 1181 → 1157 → 1160 |
| #1183 → #1185（エクスプローラーへの登録 S4a） | R4（Opus） | 同じく #1191 の枝の上に。`register_shell` `unregister_shell` を表の 2 行と `run_register_shell()` / `run_unregister_shell()` に。probe の期待表の変更は #1185 から #1183 へ移った（PR 本文を直す）。HEAD `aa3eac32`（4 commit）/ `960ee58d`（2 commit）・`--shell-registration` 27・既定実行 2,332,911・adapters の試験 5 本 0 failures（本物のレジストリの木に触る `--shell-registration` と `--clipboard` は席では回さず CI で）。待ち受け 1160 → 1183 → 1185 |

列の全体（自動・`D:/NeNeNib/outputs/20261103/queue-plan-afternoon.md`）: #1179 → #1180 → #1184 → #1152 → #1190 → #1192 → #1191 → #1197 → [席 R5 で #1195] → #1150 → #1181 → #1157 → #1160 → #1183 → #1185 → #1153 → #1166。表の PR を main に移すときの表の行の衝突は設計席が解く。

### 5.3 fuzz 生成器 4 の seed 21〜30（引き継ぎの手順 5・main `e52fbf24` の再生器）

`D:/NeNeNib/outputs/20261103/vim-fuzz-gen4-seeds21-30/summary.md`。3,000 件: 拒否 186・一致 2,813・不一致 **1**（新規 1・既知 0）。`--known` は #985 と #1137 が main に入ったので外し、`781` `980` と `1189:<C-v>A` に（`--known` の語は生成器の表にある語だけ・矢印は語でないので #1149 は付けられない）。

- `fuzz-24-0020`: 矩形の `I` の左端が国旗の絵文字（🇯🇵 の片方・幅 2）の途中に掛かるとき、Nib の詰め物の空白が Vim より 1 つ多い → **Issue #1196**（低・#1195 の後に席）。

### 5.4 今日の午後に分かったこと

- 2 つの PR が同じ場所に 1 行ずつ足す衝突（検証の呼び出し・和型の末尾）は和で解けるので、resolver に「両側の和」の規則を足せば列が止まらない（道具の後日の直し）。
- 表の PR は、#1191 が main に入るのを待たずに **#1191 の枝の上に積めば**、席の乗せ替えを今日のうちに並列で進められる（最後に `rebase --onto origin/main 0fbf14e8` で main へ・squash の内容が同じなので衝突しない見込み）。6 本を 3 席で 2 時間。
- `remaining_command`（Ex の命令の解析の列）は `run_operation` と同じ「増えるたびに 4 行」の形で、認知の複雑さの門（10）に当たった。表にする（#1199）。
- fuzz の `--known` は生成器の表の語だけを受ける（`<Up>` は語でない）。
- heredoc の Python に本文を書く事故にまた当たった（`D:\N…` の `\N`）。本文は別ファイルに書いて Python で読む。

### 5.5 夕方〜夜（15:4x〜23:45・hide の決定 4 つ → verify-window → 「LSP L1 ＋ matchparen」→ 列の再開 → 23:40 に停止）

- **hide の決定（15:4x・1 つずつ平易に聞いた）**: (1) verify-window の 8 本は列が引けた後の main の exe で hide が席を離れるときに回す。(2) `merge-pr.py` の stdout を UTF-8 にする道具の PR は入れる（#1200 / PR #1202）。(3) `verify-window.py` の配色の面の Enter に「面が閉じるのを待つ」を足す道具の PR は入れる（#1201 / PR #1203）。(4) 試用は「今夜」→ のちに「明日以降でいいよ」に変更。
- **verify-window（15:48〜16:05・hide 離席・積んだ枝の Debug exe・`D:\NeNeNib\outputs\20261103\verify-afternoon\summary.log`）**: #1203 の script の既定 1 回 pass。#1160 / #1185 / #1150 / #1181 / #1191 の exe で 17 節。落ちたのは配色の面の Enter（既知・#1203 で直す）・Ctrl+S の HDD の揺れ（3 回目 pass）・explorer の節（`:set root=<長いパス>` を打ち終わる前に Ctrl+Shift+E → 道具の揺れ **#1204**）・IME の下線 1 回（再現せず）。**製品の退行は 0**。
- **fuzz 生成器 4 の seed 31〜40**（3,000 件）: 不一致 0・新規 0（`vim-fuzz-gen4-seeds31-40/summary.md`）。seed 21〜40 で新しい種類は #1196 の 1 件だけ。
- **#1198（巨大な 1 行で矩形の `I` / `A` の入口が約 60 s）の切り分け（席 P1・Opus）**: 行頭から歩く桁の計算が 23 回呼ばれ、仮想桁の索引（`VirtualColumnIndex`）を使えていない。直しは候補 B（索引の道）＋ A の一部。#1195 / #1197 が main に入った後（明日）。作業木 `1198-block-insert-probe` は tidy を外した未 commit の計時を持つ（捨てる）。
- **「LSP L1 ＋ matchparen」（hide「別の作業を進めることはできる？」→ 設計席の提案に「よし」・同時 3 席まで・列と衝突しない）**:

| # | 席 | 結果 |
|---|---|---|
| #995 L1a core（子 #1208） | L1a（Opus） | **PR #1207**（`ae4b634a`・`lsp_offset_of` の 1 本・`diagnostics_adjusted`・`diagnostic_spans`・本文の外の行は末尾へ切り詰める＝ADR 0161 の候補 B。依頼書に「捨てる」と書いた誤りを ADR のとおりに直させた。`Refs #995` だけでは GIT-001 で落ちるので子 Issue を立てて `Closes`）→ レビュー `review-1207` → 直し → 列の末尾 |
| #995 L1b application（子 #1210） | L1b（Opus・#1207 の枝の上） | **PR #1212**（`1e76cb8c` → 直し `34675886`）。`didChange` は `language_server_edit` の 1 本（replace / undo / redo が呼ぶ）で編集の前の本文で `lsp_position_of`、apply の終わりに文書ごと 1 通・`version` は意図の後の `revision()`。古い版の診断は送った版の本文 `SentRevision`（16 版）で写して後の編集でずらす。URI は `file:///C:/…`（完全一致）。無題は**送らない（候補 A・設計席の判断）**。レビュー `review-1212`: 止める 0・直してから 2（R1-1 脇の束の写しの 2 本目 → `EditorState::parked_bundle_at`・R5-1 別名保存の didClose の試験）＋ R3-1（差し引き 0 の意図で `version` が進まない → 最後に送った版を超えなければ送らない）・R4-3・R6-1。契約 `--language-server` 61 |
| #995 L1c ui（子 #1213） | L1c（Opus・#1212 の枝の上） | **PR #1215**（`2ca6f5a2` → 直し `1dcb26bb`）。波線は太さ 1・振幅 1・周期 4 DIP の三角波を画素の列ごとに塗る（`draw_diagnostic_wave` の 1 本・本文と格子）・色は `SyntaxPalette.error` / `.warning` / `muted`（トークンは足さない）・右端のいちばん右に「E2 W1」「+」（0 なら幅 0・`DisplayText` は空を持てないので 4 項目＋`diagnostics` の矩形）・キャレットの行の文は `command_message` が勝つ・showMessage は error / warning だけ・枠は 12（録画 9）。レビュー `review-1215`: 止める 0・直してから 1（R3-1 窓 360〜471 DIP で `:` の入力欄が最小幅を割る → 言語 → 件数の順で畳む）＋ トグルの取消・R3-3 録画中は文を出さない・R1-1。窓の試験 4,295 checks |
| #995 L1d ベンチ（子 #1216） | L1d（Opus・#1215 の枝の上） | **PR #1217**（`cdb368dd` → 直し `8938fe9c`）。替え玉に `--diagnostics <N>`・`lsp-single-key` / `lsp-burst-200` / `lsp-diagnostics-ready` は `OPTIONAL_BENCHES`・`--record` だけ・`profile-lsp`・ready は打鍵の後の 2 枚目の frame の近似（間隔の上限 `LSP_READY_GAP_MS = 100`・越えたら欠測）。1 回の値（混んだ機械・基準にしない）: 単打 1.08 / burst 3.93 / ready 2.50 ms。レビュー `review-1217`: 止める 0・直してから 1（R3-1）。本測定は静かな機械で設計席（Issue #1216 のコメント） |
| #999 matchparen（ADR 0164） | M1（Opus） | **PR #1209**（`71b06836`・`matched_bracket(text, bracket, window, skipped)` で `%` と 1 本）→ レビュー `review-1209`。既定オンなので merge 前に静かな機械で A/B（`matchparen-single-key` と空の文書の 1 打鍵・+1%・D67）。**列には入れていない** |
| #1178（Vim の追従を `update_topline` に揃える・ADR 0019 改訂） | I35 → I36（Opus） | I35 が 306 / 882 件の差で止まり、設計席が候補 A（`vim_topline_updated` / `vim_topline_kept`・Vim モードだけ）を Issue に書いて I36 → **PR #1211**（`2ef34be4`・576 → 882 件一致）→ レビュー `review-1211`（**止める 2・速さ**: 窓の外の行を論理行の固定幅で先読み）→ 直し（`12202718` `f1f29622`・「要求で」の形 `VimRowsNeeded`・`vim_topline_kept` を消す・窓の端の `j` 100 回 22.6 → 12.1 ms・替え玉）→ 再レビュー `review-2`（止める 0・直してから 1: `so=999` の跳びの歩き → (b) ADR 0019 決定 5 に残差 (e) と数字・省く形は **#1214**）→ `91d2921d`・列の末尾 |
| #1196（矩形 `I` の左端が幅 2 の文字の途中） | I34（Opus） | **PR #1205**（`17fc19d7`・レビュー `review-1205`・直し済み）→ 列（#1180 の後・onto_from は #1195 の古い HEAD） |
| #1177（高い行の `M` の中央） | — | **PR #1206**（レビュー `review-1206`・直し済み）→ 列（#1205 の後） |
| #1200 / #1201（道具） | T1（Opus） | **PR #1202 / #1203**（列の末尾・#1203 の窓での 1 回の確認は設計席が main の exe で） |

- **列（夜）**: #1191 main `b822680f`（20:08）→ #1197 main `1a829885`（21:40）→ 席 R5 が #1195 を乗せ替え（`5da82d0f`・`block_carried_text` は `line_start(first) + at_byte` から 1 バイト戻して読む・別の行から始める fixture `block-append-tab-pad-later-line`・誤った形で 1 件落ちることを確かめた）→ #1195 main `85f9a40f`（22:34）→ #1150 main `743cd598`（23:28）→ #1181（main の上に乗せ替え・Ready・CI 41655693776）。
- **列の止まり 2 回（どちらも道具の使い方）**: (1) `chain-queue4.sh` を outputs から起動したら `gh pr ready` が「not a git repository」で落ち merge-pr が「ready state differs」で拒否 → **root から起動する**。(2) `after-1195-merge-1150.sh` と `after-pr-merge-onto.sh` の 6 本が作業木を `/d/…` で持っていて pwsh の `Set-Location` で落ちた（`D:\d\…`）→ **`D:/` で書く**。6 本を kill して張り直した。
- **23:40 hide「いったん止めて。今日はマシンを落とす」** → 待ち受け 15 本・見張り・#1181 の merge-pr を kill（CI は GitHub で続く）。hide の提案「新しい枝にまとめて 1 回の CI で merge・リポジトリの決まりを緩めて実装速度を落とさない」→ 明日の最初の実行タスク（引き継ぎ）。

### 5.6 夜に分かったこと

- 席の依頼書に ADR と違うこと（「捨てる」）を書くと席はそのまま作る。依頼書を書く前に ADR の該当の決定を引用して照合する。
- 「`Refs #N` だけ」の commit は GIT-001 で落ちる。親 Issue の段ごとの PR は子 Issue を立てて `Closes`。
- 差し引き 0 の意図（マクロで打って消す）は `merge_replayed_edits` が履歴を意図の前に戻すので `revision()` が戻る。LSP の `version` は増えなければ送らない（R3-1）。自動退避の改訂番号と同じ性質。
- レビューで「止める」が速さの見積もりだけのとき（#1211）、席の直しは替え玉の文字組みの `made()` で本数を比べる形で足りたが、ms の判断は静かな機械の本測定まで保留。
- 列の 1 本 30〜45 分（CI）× 17 本は 1 日の実装より長い。hide の判断で決まりを緩める（まとめ枝）。
