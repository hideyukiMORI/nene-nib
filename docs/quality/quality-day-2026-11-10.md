# 品質向上の日の記録 — 2026-11-10（5 日目・hide 不在・Fable）

hide の指示（朝・外回り）: 「これから 1 日、外回りでデスクにいない。他のセッションのサナやリナを立ち上げないので、自由に試験や計測を繰り返して、品質が上がるように調査対応をして欲しい。必要なら web 検索やマルチエージェント・オーケストレーションを自由に使っていい。止めるまで続けて最善を尽くして。試したことや変更したこと等は全てドキュメントとして記録を残して」。設計席 Fable（Nibリナ）。実装席とレビュー席は Opus、棚卸しは Sonnet。時刻は JST。文書の Issue は #1287。

## 0. 再開時の状態（07:04）

- main `5f03f3a0`（文書だけが進んでいて、製品のコードは beta.8 の `b40ea348` と同じ）。開いている PR は #1273（レビュー済み・まとめ枝 5 に積んである）・#1275（直し済み・再レビュー待ち）・#1277（レビュー待ち）・#593（hold）。
- 機械: 負荷 6〜12%。`codex` のプロセスが 2 つあるが動いていない（hide「ほかの席は立ち上げない」）。
- 引き継ぎ 11-04 の「文書の棚卸」3 点（ADR 0032 の決定 7 の重複・CLAUDE.md §2 の `WORDS_5`・ADR 0021 追補の残り）は main で既に直っていた（確かめただけ）。
- 朝の速さのゲートは回さない: 製品のコードが 11-04 夜のゲート（beta.8・0 regression）から変わっていないので、成功結果を再利用する（QLT-012）。今日のゲートはまとめ枝 5 の木で回す。A/B の基準に使う main の Release は `build/release-5f03f3a/`（`eng/build-release.ps1 -Ref main`・227 s）。

## 1. 今日の列（07:10 の計画）

| 波 | 席 | 対象 | 形 |
|---|---|---|---|
| 1 | V-1275-2 | PR #1275（#1266 map の単位ごとのモード）の再レビュー | Opus・読むだけ |
| 1 | V-1277 | PR #1277（#1269 巨大な 1 行の `l`）のレビュー | Opus・読むだけ |
| 1 | I1 | #1232 Vim の `<C-e>` `<C-y>` | Opus・実装 |
| 1 | I2 | #1282 「保存しますか」の文言を自分で持つ | Opus・実装 |
| 1 | I3 | #1214 `so` が窓より大きいときの歩き | Opus・実装 |
| 1 | P1 | #1276 map の引き直しと失敗の後の鍵（Vim の実測） | Opus・調査 |
| 1 | S1 | 試験の棚卸し（性質の試験と fuzz の穴を探す） | Sonnet・読むだけ |
| 1 | 設計席 | fuzz 生成器 5 の seed 11〜20（main の再生器）・作業木の片付け・記録 | script |
| 2 | — | まとめ枝 5（#1273 #1275 #1277）→ 検証 → CI → merge → ゲート | 設計席 |
| 2 | — | 棚卸しから選んだ性質の試験・fuzz 生成器 6・ミニマップ #1017・起動画面 #1018 | 波 1 の結果で決める |

依頼書は `D:/NeNeNib/briefs/20261110/`（共通 `_today.md`）、出力は `D:/NeNeNib/outputs/20261110/`、スクリプトは `D:/NeNeNib/scripts/20261110/`。

## 2. 波 1 の結果

### 2.1 PR #1275 の再レビュー（V-1275-2・07:2x）

判定は **merge してよい**（必須 0）。M1（新しいモードが 1 鍵の引数待ちになったとき残りを表に通さない）は直っている。前のレビューの M1 の表（`nmap xab G`）は再現になっておらず、席が `nmap x0z G` に替えて Vim 9.1 を測り直したもの（`probe4.vim`・53 行目）が正しい再現だった。HEAD の exe で `--key-map` 779 checks。軽い残り L2（ADR 0176 追補 23 の `typed_step` の引数の 1 文・試験の注記の折れ）は設計席が直して push（`20162f57`・文言だけ・format 確認）。L1（入力行の形の試験が無い）は #1276 の後続に入れる。出力 `D:/NeNeNib/outputs/20261110/review-1275-2/review.md`。

### 2.2 試験の棚卸し（S1・Sonnet・07:2x）

出力 `D:/NeNeNib/outputs/20261110/test-inventory.md`（約 170 行）。性質の試験（固定 seed）は 23 ファイル・ui/win32 には 0 本。依頼書とのずれ: 描画は `src/ui/win32` にある（`src/render` は無い）・`Language` は plain を含めて 26 値・fuzz の生成器は CLI に 3・4・5。「性質の試験を足すと効きそうな所」10 件から 9 件を 4 席（T1〜T4・Opus）に分けて出した:

| 席 | 対象 |
|---|---|
| T1 | LSP の枠と受信の chunk の分割・置換 `SubstituteAll` の模型との一致 |
| T2 | 自動退避の台帳と本体の組の欠損・開く道の chunk の境 |
| T3 | controller の混ぜた意図の列（面・タブ・Ex・検索・エクスプローラー・合図） |
| T4 | UTF-8 / UTF-16 の全 code point の往復・`PrintPages`・テーマのファイル・shell の登録計画 |

字句器の走査量（6 番）は走査を数える口が要るので今日は出さない。破れを見つけた席は直さずに Issue と最小の再現を報告する約束。

### 2.3 #1276 map の Vim の実測（P1・Opus・probe）

probe と Vim の出力は `D:/NeNeNib/scripts/20261110/1276/` と `D:/NeNeNib/outputs/20261110/probe-1276/`（席は report.md を書く道具を止められたので、要旨は席の最終報告からここに写す。Nib の値の大半は実行でなくコードの読み）。

Vim 9.1 の規則（`getchar.c` / `normal.c`）:
- R1: 右辺は展開せず typebuf の先頭に差し込むだけで、各鍵は**読む時点**の `get_real_state()` で引き直される。
- R2: `noremap` は鍵ごとの印（`tb_noremap`）でモードが変わっても残る。
- R3: `maxmapdepth` は 1 文字を返すまでの中だけで数える（`nmap j 2j` は E223 にならない）。
- R4: 失敗のときは `beep_flush` が `FLUSH_MINIMAL` で展開した鍵（`tb_maplen`）だけ捨てる。右辺の残りは流れず、打った鍵は残る。
- R5（F2 の原因）: `f t F T r` だけは引数を読んだ後に `vpeekc()` で先読みし、typeahead に既にある次の鍵を今のモードで展開する。その後に `f` が失敗すると、その右辺ごと捨てる。
- R6 / R7: `'` `m` `<C-b>` の失敗は先読みしない。`:normal` と `t` なしの `feedkeys` は全部が展開した鍵の扱い。

Nib との差: F2 は 21 件中 8 件（1 鍵ずつ処理する Nib で実際に起きうるのは A1 `50Gxak`（Vim 50 / Nib 53）と A14（op 中の先読みが omap で引く）の 2 件・残りは typeahead に既にあることが前提）。F1 は 16 件中 11 件（右辺の途中でモードが変わる形）。

設計席の判断:
- **F2 は直さない（記録だけ）**。先読みは「次の鍵が既に打ってあるか」に依る振る舞いで、手で打つ速さでは後から打った形（Nib と同じ答え）になる。ADR 0176 に限界として書く。
- **F1 は直す**（候補 A: 右辺の 1 段の引き直しを「引いた時点」から「流す時点のモード」へ移す・決定 3 の「1 段」は保つ・追補 11 と 24 を改める）。#1275 が main に入ってから席に出す。
- map の差分 fuzz（表を固定 seed で作り、oracle は 1 鍵ずつ `feedkeys`）は生成器 6 の候補。

### 2.4 PR #1277 のレビュー（V-1277）

判定は **merge してよい**（必須 0・軽い指摘 3・後続 2）。窓の道 `character_end(byte, count)` は行を写す道と同じ答え（4 バイトの余白に入る 1 歩を捨てて読み直す・`whole_code_points` で窓の終わりを完結した code point で切る）。`--vim-visual-block` 3216 checks を HEAD の exe で確認。後続は Issue **#1292**: main に同じ形の潜在の穴が 2 か所（`VimHorizontalScroll.cpp` の `character_after`・`VimCaret.cpp` の `character_near`・窓の終わりで切れた 3 / 4 バイトの頭を decode して `size()` を 1〜2 バイト越えて読む・答えは変わらない）と、`whole_code_points` が 3 つになったこと（`Utf8.hpp` の 1 本へ）。出力 `D:/NeNeNib/outputs/20261110/review-1277/review.md`。

### 2.5 まとめ枝 5 を積んだ（07:4x）

`chore/1274-integration-20261105a` を origin/main（`5f03f3a0`）に乗せ直し、`stack2.sh`（今日の写し `D:/NeNeNib/scripts/20261110/stack2.sh`）で #1275（`20162f57`）と #1277（`190bbf67`）を積んだ。衝突なし。3 commit（#1233 `dacd26bc`・#1266 `191d4692`・#1269 `81337642`）。検証は `D:/NeNeNib/scripts/20261110/verify-integration.sh`（Debug 全 target・ctest・symbols・conformance・format・Release）。

### 2.6 作業木の片付け

merge 済みで未 push の commit が 0・追跡ファイルの変更が 0 の 31 本（まとめ枝 1〜4・11-04 の直しの枝・`*-before`・`beta5` `beta7` `beta8` ほか）を `git worktree remove --force` で外した（一覧と判定 `D:/NeNeNib/outputs/20261110/worktree-cleanup.log`）。残したもの: 未 push の commit か未 commit の変更があるもの（`1146-search-composing-start` `999-matchparen` `1198-block-insert-probe` `1227-z-caret-count` `1230-fuzz-replay` `1235-*` `1178-base` `792-before` `528-probe`）・古い実験の枝・今日使っているもの。

## 3. 長く使ったときの漏れの調査（設計席・`D:/NeNeNib/scripts/20261110/soak.py`）

これまで「同じ操作を何百回も繰り返してメモリと handle が増え続けないか」を調べた記録が無かったので、道具を書いて測った。Release の exe（main `5f03f3a0`・`build/release-5f03f3a/`）を道具の profile（`D:/NeNeNib/evidence/20261110-soak/profile`）で起動し、同じ操作の周を 30 回繰り返して、周ごとに private bytes・working set・handle 数・GDI / USER のオブジェクト数を読む。鍵は窓へ post するだけ（最初の Vim への切り替えだけ前面を取る）。

| 周の中身（1 周） | private bytes KiB（最初 → 1/3 → 2/3 → 最後） | handle | GDI | USER |
|---|---|---|---|---|
| 長い日本語行 1000 行で PgDn 150 + PgUp 150 | 65252 → 64968 → 64968 | 473 → 478 → 478 | 13 | 20 → 21 |
| F1 の面を開く・3 文字・Esc を 20 回 | 65944 → 65712 → 65552 | 473 → 478 → 478 | 13 | 20 → 21 |
| `:colorscheme` 12 種を順に | 65464 → 65688 → 65708 → 65668 | 508 → 488 | 13 | 21 → 23 |
| `:set fontsize=` 5 種を順に | 63916 → 64152 → 64060 → 64292 | 486 → 486 | 13 | 21 → 23 |
| `:tabnew` 20 + `:tabclose` 20 | 63128 → 63512 → 63476 → 63664 | 486 → 503 → 503 | 13 | 20 → 21 |
| `/測定` + `n` 200 + `N` 200 + `:noh` | 62088 → 62504 → 62352 → 62352 | 488 → 501 → 501 | 13 | 20 → 21 |
| `O` + `.` 50 + `u` 51 | 62284 → 63292 → 63224 → 63244 | 486 → 501 → 501 | 13 | 20 → 21 |
| `ggVGy` + `yyP` 5 + `u` 5（200 行） | 55424 → 56504 → 56336 → 56336 | 452 → 501 → 501 | 13 | 20 → 21 |
| 窓の大きさを 4 種に変える | 62520 → 62688 → 62908 → 63104 | 491 → 505 → 507 | 15 | 20 → 22 |
| `:preview` を 5 回開いて閉じる（Markdown 300 節） | 57324 → 57424 → 58064 → 58084 | 493 → 501 → 503 | 13 | 21 → 22 |
| `:syntax off` / `on` 10 回 + PgDn / PgUp 30（C++ 6000 行） | 58332 → 58320 → 58128 → 58128 | 483 → 500 → 500 | 13 | 20 → 21 |

**結果: 11 種類とも漏れは見えない。** private bytes は最初の数周で落ち着き、後ろの 2/3 で横ばい（差は数百 KiB 以内）。GDI は 13（窓の大きさの周だけ 15）で一定、handle は約 500 で一定。全周で exe は生きていて応答した。記録は `D:/NeNeNib/outputs/20261110/soak-main.log` `soak-main-2.json` `soak-theme.json`。

気づいたこと（漏れではない・後で静かな時に調べる）:
- **exe を止める（`TerminateProcess`）と終わるまで 3〜8 秒かかる周がある**（検索 7.7 s・打って戻す 7.8 s・配色 5.2 s、タブと構文は 0.2 s）。測った時は席のビルドと作業木の片付けで D（HDD）と CPU が混んでいた。普通に閉じたとき（`WM_CLOSE`）の終わりの時間は測っていない → §5 の静かな時間帯で測る。#424（D 側の profile で設定保存と終了待ちが長い）と同じ根かもしれない。
- `:colorscheme` 1 回が混んだ HDD で 0.2〜2 秒（設定の保存の書き込みを待つ）。これも #424 の範囲。

## 4. fuzz 生成器 5 の seed 11〜20（main・製品は beta.8 と同じ）

`D:/NeNeNib/scripts/20261110/fuzz-gen5-11-20.sh`（300 件 × 10・`--shrink`・既知は #1231 の `z-` `zb`・再生器はまとめ枝 4 の木の Release）。3000 件のうち不一致 7 件・全部新規・2 種類。出力 `D:/NeNeNib/outputs/20261110/vim-fuzz-gen5-main-11-20/`。

| 種類 | 件 | 最小の鍵 | Issue |
|---|---|---|---|
| オペレータの回数と `H` / `L` の回数の両方がある形 | 6 | `4y3H`・`4d3H`・`4y4L`・`"24y4L`・`"A4d4H` | #1293（席 I4） |
| 回数つきの `zt` で行を変えるときの列（Vim の `nv_zet` は `check_cursor_col()` でバイトの列を保つ・Nib は仮想桁で着く） | 1 | `2a<Tab><C-r>"<Esc>01.27zt` の後の列が Vim 2 / Nib 16 | #1294（席 I5） |

## 5. 波 1 の続き（08:00〜09:00）

- **まとめ枝 5 の検証**（`81337642`）: Debug 全 target 警告 0（機械が 9 席のビルドで混んでいて 1963 s）・ctest 15 / 15（581 s）・symbols 3 libraries 0 violation・conformance 0・test-conformance OK・format 通過・validate-git 3 commit・Release 全 target 警告 0。検証 script の誤り 2 つを直した（symbols.py は CMake File API の query と toolchain の PATH が要る・走っている bash の script を書き換えると途中で落ちる）。**PR #1299** → `merge-pr.py 1299 --integration --execute`（CI run 42721831346）。
- **#1282「保存しますか」**（席 I2 → PR #1295）: `TaskDialogIndirect` の custom button「保存する(Y)／保存しない(N)／キャンセル」（描画を失った後は 2 つ・Esc と × は効かない）。文言は core の `save_question_text` の 1 か所。comctl32 v6 は manifest の依存 ＋ delay-load で、起動の後 3 秒の module は前後とも 65 個（comctl32 は箱を出すときだけ読まれる）。道具の `dismiss_dialog` は TaskDialog に `TDM_CLICK_BUTTON` を送る。PNG `D:/NeNeNib/evidence/20261110-1282/dialog-head.png`（設計席が目で確認）。残る `MessageBoxW` は 7 か所（起動の失敗・自動退避の復元の問い・格子の問い・失敗の理由・設定の知らせ・UTF-8 で保存の問い・落としたファイルの失敗）で OS の文言のまま。
  - レビュー V-1295: **直してから merge**（必須 1）。描画を失った形で箱を出せなかったとき（`TaskDialogIndirect` の失敗 = cancel）が「保存しない」と同じ扱いになり、自動退避まで消える。設計席の裁定は候補 A（cancel は捨てた扱いにしない）。
  - 直し（席 I2b・`90fd6e5a`）: 答えの後の一歩を純関数 `save_step(SaveQuestion, SaveAnswer)` に切り、描画を失った後の cancel は「閉じるが捨てない」（`unsaved_discarded` を立てない＝自動退避を残す）。箱が出ている間の 2 つ目の閉じる確認は出さない。`--save-question` 20 checks・窓の試験の既定 4344 checks。設計席が差分を読んで受理。hide の実機の「OK／さらに詳しく／キャンセル」の原因は未特定のまま（文言を自分で持ったので、原因が OS の側でも出なくなる見込み・hide の試用で確かめる）。
- **#1214 `so` が窓より大きいときの歩き**（席 I3 → PR #1297）: `so` > 窓の高さ・先頭が 1 行目でない・キャレットが先頭以上・`n = top + so − cursor >= halfheight` の 4 つが揃えば `check_top_offset` の真偽によらず答えは `scroll_cursor_halfway(FALSE)`（偽でも `check_botline` の道で halfway へ行く）ので歩かない。契約: 1000 行（各 5 画面行）・窓 40 行・`so=999` の `500j` で layout 206 本 → 9 本。Python の写しとの比較 63,000 通り（高さ 1〜20 に広げて 180,000 通り）差 0。
  - レビュー V-1297: **直してから merge**（必須 1・製品コードの直しは不要）。Vim の実測 165 件は全部一致・証明も成り立つが、`so` が窓より大きい fixture が 1 件も無かった。
  - 直し（席 I3b・`f26ae0f2`）: fixture `viewport-so-over-height-` 12 件（Vim の値がレビューの表と 12 / 12 一致・3470 → 3482 件）。後続 Issue **#1300**（`so` が高さちょうど・折り返す行で先頭が Vim 10 / 写し 9・新しい道の外・前からある差）。
- **性質の試験 T1**（PR #1298・Issue #1291）: LSP の枠（正しい列 300・変異 300・ヘッダの変異 13 種 200・8 KiB の境・往復 600）と置換（literal 500・magic の部分集合 500・controller の Ctrl+H 60 と undo / redo）。製品の破れ 0。変異 2 本で落ちることを確認。既定実行 2,338,498 checks。
- **性質の試験 T2**（PR #1301・Issue #1290）: 自動退避の 2 ファイルの組（傷 8 種・16 件）・起動時の復元（40 件）・開く道の塊の境（7 族 84 本文・塊 1 / 2 / 3 バイトと多バイトの途中）。製品の破れ 0。変異 3 本で落ちることを確認。自動退避の組は本物のファイルを書くので HDD で遅い（16 件で 6.6 s・CTest の制限 120 s）→ 件数はこのまま（CI は SSD）。

## 6. まとめ枝 5 が main（09:1x）

`merge-pr.py 1299 --integration --execute`（CI run 42721831346）→ **main `a13db9be`**（3 commit: #1233 `d07715c5`・#1266 `9a0858e2`・#1269 `a13db9be`・元の PR #1273 #1275 #1277 は道具が閉じた・待ち直しは要らなかった）。速さの 8 本のゲートは席が引けた静かな時間帯に測る（§9）。

## 7. 波 1 の残りと波 2（09:00〜）

- **性質の試験 T3**（PR #1302・Issue #1288・controller に混ぜた意図の列）: 5 組 × 30〜70 歩を合図あり / なしで 2 回流し、毎意図の後の frame・キャレットと選択の境・モードと待ちの閉じた組・合図が見えるものを変えないこと・undo で開いた本文に戻ること。**製品の破れを 1 件見つけた → Issue #1296**: `WorkCompleted` を 1 つ挟むと通常モードの上下移動が狙う横位置が捨てられる（`keeps_wanted_x` に `WorkCompleted` だけが無い）。最小の再現: 本文 `abcdef\nx\nabcdef` で line_end → document_start → line_end → next_line → [WorkCompleted] → next_line が 3 行 2 桁に着く（合図なしは 3 行 7 桁）。同じフォルダの列挙が裏で終わる瞬間に ↓ を押していると列が飛ぶ、という形で使う人に見える。レビュー V-1302（Sonnet）: merge してよい・#1296 は製品の破れとして正しい・`MapTimeout` も同じ穴の疑い。→ 席 I6（#1296 の直しと「合図か」を並べている列の総点検・#1302 の枝の上に積む）。
- **性質の試験 T4**（PR #1303・Issue #1289）: UTF-8 / UTF-16 の全 code point の往復と乱れたバイト列 1500 件（模型は Unicode の表 3-7）・`PrintPages`・shell の登録計画 250 件（鍵が全部 Nib の木の中）・テーマ（組み込み 125 の往復・色を替えた 400 件・色の文字列 3000 件）。製品の破れ 0。レビュー V-1303（Sonnet）: merge してよい。
- レビュー V-1298（Sonnet）: merge してよい（軽い指摘: 桁あふれの後に数字でない文字が続く Content-Length は `body_too_long` になる・規則の文は `invalid_content_length` を想定 → 後で Issue）。
- レビュー V-1301（Sonnet）: **直してから merge**。塊の境の試験の UTF-8 の本文が 16 片中 3 片の不正なバイト列を含み、正しい UTF-8 の入力が 24 件中 2 件前後しか無い（多バイトの途中の境がほぼ不正の判定の比較になっている）→ 席 T2b。
- **#1232 `<C-e>` `<C-y>`**（席 I1 → PR #1304）: 窓を回数の行だけ送り、キャレットは窓の外へ出るときだけ動く。端でも失敗にしない・オペレータの後ろは拒否・`scrolloff` の内側へ寄せてその窓を後の追従で戻さない（`VimNavigate.keeps_top`）。fixture 44 件（`viewport-ctrl-e-` 20・`viewport-ctrl-y-` 16・折り返す窓 8）全部 Vim と一致。→ レビュー V-1304（`keeps_top` が第 2 の経路でないかを見る）。
- 新しい席: I4（#1293 オペレータの回数 × `H` `L`）・I5（#1294 回数つき `zt` の列）・I7（#1292 窓の外読みの残り 2 か所と `whole_code_points` の 1 本化）・I8（#1276 F1 map の右辺を流す時点のモードで引く）。

## 8. 波 2 の結果（09:15〜10:10）

- **T2b**（PR #1301 の直し・`080164ac`）: 正しい本文だけの族 5 つ（UTF-8・UTF-8 BOM・Shift_JIS・UTF-16 LE / BE）と壊れた欠片を混ぜた族 7 つに分けた（Shift_JIS と UTF-16 にも同じ偏りがあった）。96 入力のうち開けた 43・失敗 53・UTF-8 として開けた 16・境が多バイトの途中に来た読み 99 回・BOM の途中 27 回。件数の守りを `expect` で断言。変異（commit を 1 バイト少なく）で 96 入力すべて違反。→ 受理。
- **#1293 オペレータの回数 × `H` `L`**（席 I4 → PR #1306・`6c646bbe`）: Vim の `nv_scroll` は回数を窓の高さで切らない（`H` は `w_topline + count1 − 1` を最終行で切る・`L` は `w_botline − 1 − (count1 − 1)`・届かなければ 1 行目）。窓の内側へ寄せる `cursor_correct` はオペレータの無いときだけ。行の式は新しい `vim_screen_top_line` / `vim_screen_bottom_line` の 1 対で、折り返さない道と折り返す道の両方が呼ぶ。fixture `viewport-op-count-hl-` 21 件（修正前は 12 件落ちる）。fuzz seed 11〜20 でこの種類 6 → 0。レビュー V-1306: **merge してよい**（Vim の実測 31 件が全部一致・修正前は 7 件違う・折り返す窓 3 形も一致）。後続 Issue **#1307**（折り返す窓で先頭の行が窓より高いときの `L` `3L` `4H` が Vim 10 / Nib 9・前からある違い）。
- **#1276 F1 map の右辺**（席 I8 → PR #1308・`e4253283`）: core の `append_once`（引いた時点の 1 段の展開）をやめ、`KeyMapEmit` は生の右辺と引き直しの始まり `remaps` を持つ。純関数 `rhs_unit_step(map, optional<mode>, rest)` を controller が 1 鍵流すごとに呼ぶ（1 段だけ・待たない・モードが無いときは引かない）。`--key-map` 779 → 810。Vim の実測と一致するようになった件 B1 B3 B6 B7 B8 B9 B14 C1 C3 L1a。残る件: 2 段以上の連鎖（B4 B11 B16・決定 3）・`maxmapdepth`（B5）・`nmap j 2j` が Vim では止まらない形（B10h）・F2 の先読み（A1 A14）。レビュー V-1308: **直してから merge**（必須 1: 表に行があるとき、表に無い鍵の Pass の道で `remaps` の push が 1 回増える。ADR の「増えない」が事実と違う）→ 席 I8b（`consume` の前に Pass を判定して返す・コードで直す）。後続 Issue **#1311**（Vim は回数の途中の `0` を map で引かない・`no_zero_mapping`）と **#1312**（マクロの録画は打った鍵を録り再生のときに今の表で引く・ADR 0176 決定 7 は Vim と逆）。
- **#1232 の直し**（席 I1b・`9601ebd6`）: 再生を返した外側の鍵は、再生がそこで終わったときだけ中の最後の鍵の「先頭を保つ」印を引き継ぐ（`settle_vim_step`・`vim_top_kept_`）。fixture `viewport-ctrl-e-macro-` 5 件が Vim と一致。`eng/vim-wrap-oracle.py` は `vim --version` を UTF-8 で読む。再レビュー V-1304-2: M1 は直った・印が残って関係ない鍵の追従を飛ばす道は無い（失敗・入力行・タブ・モード・合図・マウス・大きさ・map・`.` を読んだ）。**M2**（最初の版からある差）: Vim の `VALID_TOPLINE` はキャレットの行か本文が変わるまで残るが、Nib は次の鍵で消す。30 行・`so=3`・窓 10 行・6 行目から `50<C-e>w` は Vim の先頭 30 / Nib 27（最終行より先まで送って `so>0` のときだけ）。設計席の裁定は (b): ADR 0019 に限界として書き（`fcf157ac`）、Issue #1310 に反例つきで残す。理由は、起きる形が狭いことと、「行か本文が変わるまで持つ印」は Vim の追従の写し（#1178）の形に手を入れる設計の仕事で、今日の残りの枠では受理の検証まで届かないこと。→ #1304 は受理。
- **まとめ枝 6**（Issue #1305・`D:/NeNeNib/worktrees/1305-integration`）: #1295 #1297 #1298 #1301 #1302 #1303 の 6 commit。試験の 4 本は `NibTests.cpp` の 2 表で衝突するので、両側の行を残して大きさを数え直す `resolve-nibtests.py` と `continue-stack.sh`（`D:/NeNeNib/scripts/20261110/`）で解いた（contracts 90・scopes 113）。検証: Debug 全 target 警告 0（813 s）・ctest 17 / 17（503 s）・symbols 0・conformance 0・test-conformance OK・format 通過。

## 9. 波 3（10:10〜）

- **まとめ枝 6** → PR #1314（`a17b9368`・6 commit・Release 全 target 警告 0）→ `merge-pr.py 1314 --integration --execute`（CI run 42733481898）。
- **#1296 合図と狙う横位置**（席 I6 → PR #1313・`66912b00`・#1302 の枝の上）: 「合図か」を `detail::is_signal` の 1 本（WorkCompleted・AutosaveTick・PreviewTick・LanguageServerTick・MapTimeout）に寄せ、`not_user_operation`（`keeps_tab_walk`・`begin_intent`・`keeps_key_map_pending`・全面プレビュー）と `keeps_wanted_x` が呼ぶ。漏れていたのは `keeps_wanted_x` だけ（WorkCompleted と MapTimeout）。合図がタブ・本文の改訂・キャレットを動かしたときは `apply` が写した後に狙いを捨てる（読み終えた文書を採ってタブが替わるとき、古い文書の桁を持ち込まない）。#1302 の性質 4 の絞りを外し、移動を 2〜4 個続ける生成を足した（元の生成器は MoveCaret が 5 組で 1 つしか出ていなかった）。レビュー V-1313: **merge してよい**（MapTimeout を入れても反例なし・`wanted_x_` を使うのは通常モードの `move_visual` だけ）。50 倍の走りが別の既存の破れを見つけた → Issue **#1309**（Vim で `$` の後に意図 DeleteText を送るとキャレットが結合文字の途中に止まる。窓は Vim モードでこの意図を送らないので使う人には見えない・席 I9）。
- **#1308 の直し**（席 I8b・`13733f5f`）: `key_map_step` の入口の `alone_step` が、待ちの無い 1 鍵の Pass / Pending を列を作らずに決める。表に行があって表に無い 1 鍵の割り当ては main 4 回 → 0 回（読み）。`--key-map` 814。限界の実測 2 件（P3 Vim 52 / Nib 49・P8b Vim 100 / Nib 48）を契約と ADR 0176 追補 28 に。→ 受理。
- **#1294**（席 I5 → PR #1315・`6ad48d8e`）: Issue の見込み（回数つき `zt` の桁）は外れ。回数つきの `zt` `zz` `zb` の桁は Nib も前から Vim と同じバイトの桁だった。違っていたのはその後の**行単位の `y`（`yy` `4yy` `Y` `y_` `yj`）のキャレット**: Vim の `nv_lineop` の `cursor_down(count1 − 1)` は回数が 1 でも `coladvance(w_curswant)` で欲しい列へ動き、`do_pending_operator` が動く前の位置と今の位置の早い方に置く。Nib は欲しい列から引き直していた（欲しい列とキャレットの桁がずれているときだけ違いが出る）。fixture `viewport-z-count-col-` 25 件（main では 5 件落ちる）。fuzz seed 11〜20 の残り 1 件が消えた。→ レビュー V-1315（欲しい列がずれるほかの状況を Vim で測る）。
- **#1312 の下調べ**（席 I11・probe・`D:/NeNeNib/scripts/20261110/1312/probe.vim`・出力 `D:/NeNeNib/outputs/20261110/probe-1312/out.txt`。席は report.md を書く道具を止められたので要旨をここに写す）:
  - Vim の規則（getchar.c の `gotchars`・register.c の `do_execreg`）: 録るのは**打った鍵だけ**（左辺は打った形のまま・右辺とレジスタの中身は録らない・止める `q` はそれを生んだ打鍵ごと消える）。`@a` は `REMAP_YES` で再生し**今の表で引く**。`:@a` と `.` は引かない。
  - 差: 26 件測って Nib に無い 3 件（`:normal` 系）を除き、同じ 6・差 17（録画の中身 5・再生が表を引かない 11・両方 1）。
  - Nib の今の形: 録画を積むのは engine の `macro_recorded` で map の入口より後ろ。`perform(VimReplay)` は表を通らず、`.` と `@` を区別できない。ADR 0176 の決定 3 の末尾・決定 7 の録画の部分・決定 21 が Vim と合わない。
  - 設計席の裁定は調査席の推しの **(B)**: 書き手は engine の 1 本のまま、右辺の鍵は「積まない」印（`VimKeySource` に `mapped`）を付け、表に当たった単位は打った左辺を同じ書き手で積む。`VimReplay` に `kind {repeat, macro}` を足し、macro だけを再生の中だけで持つ待ちの列で引く（タイマーなし・列が尽きたら即座に流す）。(A)（録画を入口の前へ移す）は入力行の文字と IME の確定が入口を通らず書き手が 2 本になるので採らない。実装は #1308 と #1311 が main に入ってから（同じファイル）。
- 新しい席: I9（#1309）・I10（#1311 回数の途中の `0`・#1308 の枝の上）。

## 10. まとめ枝 6・7 が main、まとめ枝 8（10:35〜12:00）

- **まとめ枝 6** → **main `ea3fde7a`**（10:35・CI run 42733481898・6 commit: #1282 `b1ba13f6`・#1214 `4d011e74`・#1291 `79213b10`・#1290 `4c9e739a`・#1288 `23c4698a`・#1289 `ea3fde7a`）。
- **まとめ枝 7**（Issue #1317・PR #1321）: #1313 #1308 #1304 #1306 の 4 commit。衝突の解き方を script にした（`D:/NeNeNib/scripts/20261110/resolve-conflicts.py`: `fixtures.json` は両側の和・生成物は枝側を取って後で oracle・`NibTests.cpp` は表の行を残して数え直し・ADR は両側の和。`continue-stack2.sh` が rebase を続けて fixture を測り直す）。試験の関数が隣り合って足された衝突（`KeyMapTests.cpp` `VimViewTests.cpp`）は手で両側を残した。#1304 は oracle の鍵の表を変えるので、枝の commit を `--reuse-ref` にして main 側だけの 12 件を測り、#1306 はまとめ枝を `--reuse-ref` にして 21 件を測った。検証: Debug 全 target 警告 0（725 s）・ctest 17 / 17・symbols 0・conformance 0・format・Release 警告 0。
  - **積んだ木の fuzz**（生成器 5・seed 1〜20・6000 件・Release の再生器）: 不一致 2 件 = 既知 1（#1231）＋ `fuzz-15-0158`（#1315 が直す件）。4 本を合わせたことによる新しい違いは 0。朝の main は seed 11〜20 だけで 7 件。出力 `D:/NeNeNib/outputs/20261110/vim-fuzz-gen5-integration7-1-20/`。
  - → **main `275a1535`**（11:52・CI run 42742198222・4 commit: #1296 `78833652`・#1276 `c7db7d5f`・#1232 `b850be18`・#1293 `275a1535`）。
- **#1292 窓の外読み**（席 I7 → PR #1320・`8d6d2781`）: 修正前に Debug（ASan）で落ちることを 5 か所で確かめた（Issue の 2 か所 `character_near` `character_after` と、grep で見つけた 3 か所: INSERT の `<Right>`・巨大な行の窓 `read_around`・格子の 8 バイトの覗き `walk`）。`Utf8.hpp` の `whole_code_points` の 1 本（最後の code point が完結していれば残す）に寄せ、3 つの写しを消した。`code_point_at` は切れた列でも text の外を読まない。横送りの押しと INSERT `<Right>` は `vim_character_after` にまとめた。性質の試験（乱数の本文 60 行・2 / 3 / 4 バイトと幅 0 の列を 64 バイトの窓の境にまたがせる）。レビュー V-1320: **merge してよい**（席が直した既存の試験 1 行は、試験が「日」の途中に Tab を入れて不正な UTF-8 を作っていた誤りで、製品の振る舞いの変更ではない）。後続 Issue **#1322**（Sentence と Tag の `next_in_line` と `leading_continuations` の写し）。`l` `h` と横送りの費用の前後は席が負荷で測れなかった → 静かな時間帯に設計席が probe で測る（前 `D:/NeNeNib/worktrees/1292-base/build-rel/nib_perf_probes.exe`・後 `D:/NeNeNib/worktrees/1292-window-overread/build/release-probe/nib_perf_probes.exe`）。
- **#1311 回数の途中の `0`**（席 I10 → PR #1318・`53c0bef9`・#1308 の上）: Vim の実測 23 件。回数の続きの `0`（`10j` `50G` `v10j` `d10j` `"a10j`・右辺の中の `10j`）は表を引かず、単独の `0`・`d0`・`2d0`・`5x0`・INSERT の `3i0` は引く。core の `vim_continues_count(state, key)`（engine の `counts_as_digit` を呼ぶだけ）と `vim_map_mode_of(state, front)` の 1 か所。`--key-map` 864。レビュー V-1318: **merge してよい**（追加の実測 18 件で本件が原因の違い 0）。見つかった engine の違い → Issue **#1319**（`5"a0` は Vim ではレジスタの後で回数を数え直す）。
- **#1315 の直し**（席 I5b・`77d7c9a0`）: `do_pending_operator` の「動く前の位置と着地の早い方」を `detail::operator_start` の 1 本にし、`yy` 系・移動の `y`・検索の `y` が通る。fixture `yank-line-caret-` 12 件。古い道が隠していた「オペレータの後ろの `H` `M` `L` の着地を余白へ寄せる」を直した（#1306 と同じ所）。まとめ枝 8 では衝突は注記 1 つだけで、コードは #1306 の形と合わさって 1 本になった。→ 再レビュー V-1315-2（退行 0 を前の 61 件で確かめる）。
- **#1309**（席 I9 → PR #1323・`bb229525`）: 全部の意図の出口で、Vim モードのときにキャレットを 1 文字の境へ寄せ直す `keep_vim_character()`。意図の表 23 行（修正前に外れていたのは 12 行）。レビュー V-1323: **直してから merge**（必須 2）。**INSERT の寄せは Vim と逆**: Vim 9.1 の INSERT の挿入点は結合文字の手前に残る（`\u0301a` に `iab` で `ab\u0301a`・`\t \u0301x` に `I` で col 3）のに、PR は文字の先頭へ戻すので窓の道では `iab` が `ba\u0301a` になる（fixture は入口を通らない道で再生するので落ちない）。費用は O(1)・NORMAL の出口の寄せは妥当・合図の後にキャレットは動かない。使う人が実際に届かせられるのは表の 1 行だけ（INSERT の孤立した結合文字の前での貼り付け）で、その行は修正前のほうが Vim と同じだった。→ 席 I9b（INSERT では寄せない・窓の道で打った本文を Vim と比べる契約）。後続: Vim モードの Delete キーが何も送らない（Vim の `<Del>` は NORMAL で `x`）・窓の道で fixture を再生する契約。
- **見つかった穴**: Vim の段落と文の移動 `{` `}` `(` `)` が Nib に無い → Issue **#1316**（V-1315 が `y{` を測って気づいた）。
- **まとめ枝 8**（Issue #1327・`D:/NeNeNib/worktrees/1327-integration`）: #1320 #1318 #1315 の 3 commit（fixture 3581 件・37 件を測り直し）。検証中。
- 新しい席: I12（#1312 録画は打った鍵・候補 B・#1318 の上）。

## 11. まとめ枝 8 が main、速さのゲート（12:00〜13:20）

- **まとめ枝 8**（Issue #1327・PR #1329・`7ae06829`）: 検証は Debug 全 target 警告 0・ctest 17 / 17（412 s）・symbols 0・conformance 0・Release 警告 0。**format が 1 回落ちた**: 設計席が #1306 と #1315 の同じ規則の注記を 1 つにまとめたときの行の長さ（`VimStepMotions.cpp` の注記 3 行）。注記を書き直して通した（1509 files）。注記だけの差なので、その前の木のビルドと試験の結果を再利用した（PR 本文に明記）。→ **main `d33361cf`**（13:05・3 commit: #1292 `c778662b`・#1311 `daad137f`・#1294 `d33361cf`）。
- 再レビュー V-1315-2: **merge してよい**（退行 0・前の 77 件のうち前に一致していた 67 件は全部一致のまま・追加 28 件も全部一致・`scrolloff=3` の H / M / L の 13 件も一致）。
- **#1312 録画は打った鍵**（席 I12 → PR #1328・#1318 の上）: `VimKeySource` を `typed` / `mapped` / `replayed` の 3 値にし、`== replayed` の 3 か所を `switch` の関数 2 つに替えた。録画の書き手は core の `vim_macro_typed` の 1 本。`VimReplay.kind`（`repeat` / `macro`）。macro の再生は再生の中だけの待ち `replay_pending_` で map の入口と同じ関数を通す（タイマーなし）。下調べの差 17 件のうち 16 件が Vim と一致（`--key-map` 1309）。既存の契約の期待値は 1 つも変えていない。表が空の editor と当たらない表の editor が一致する性質の試験 200 件。レビュー V-1328: **直してから merge**（必須 1: `nmap Z jq` で `qajZ` は Vim が `"a`=`jZ`・左辺が消えるのは右辺の最初の鍵で録画が止まったときだけ）。設計席の裁定は候補 A（Vim に合わせる）。直し（席 I12b・`e4333149`）: 左辺を `held_lhs_` に置き、右辺の最初の鍵を流した直後に録画がまだ立っていれば積む。契約 R21〜R28（Vim の実測）・再生の中の待ちの道を 103 回通ることを断言・`--key-map` 1526。再レビュー V-1328-2: **merge してよい**（疑わしい 8 つの道を Vim で実測して全部一致・`held_lhs_` が残る / 二重に積む / 積み漏らす道なし）。
  - Vim との違いで Nib のほうが望ましい形: `nmap x @a`・`"a`=`x` は Vim が E223 も出さずに止まらない（60 秒で打ち切り）。Nib は深さ 101 で E132 を出して止まる（ADR 0046 に 1 行）。
- **#1323 の直し**（席 I9b・`79e0bea3`）: レビューが読みで予告した退行を試験で再現（窓の道で `\u0301a` に `iab` → `ba\u0301a`・Vim は `ab\u0301a`、ほか 9 件）してから直した。`keep_vim_character(intent)` は NORMAL と VISUAL だけを寄せ、INSERT では何もしない。`VimKeyPress` の後は入力行が開いていなければ出口で引き直さない。新しい `VimWindowPathTests.cpp`: 窓の道の本文を Vim 9.1 の実測と比べる 10 件と、窓の道と `press_vim_key` の道で本文・キャレット・モードが同じになる性質（48 組）。後続 Issue **#1325**（Vim モードの Delete キーを engine の `<Del>` として流す）と **#1326**（fixture を窓の道でも再生する契約）。→ 受理。
- **試験の仕組みの穴 #1324**（席 I12 が見つけ、V-1328 が確認）: 名指しの scope `--vim-macro` が main で 2 件落ちる（#1246 の fixture `insert-page-edge-fails-page-up-in-a-macro` が scope の名前と件数の検査に当たる）。CTest が回すのは引数なしの `nib_tests` だけで、名指しの scope の自己検査は CI でも既定実行でも回らない。→ 席 T5（全 selector を 1 本ずつ回して落ちているものを洗い出し、強制を足す）。
- **まとめ枝 9**（Issue #1330・`D:/NeNeNib/worktrees/1330-integration`）: #1328（`ffeb9a4f`）と #1323（`8265e6fc`）。`EditorController::apply` の出口で #1313 の合図の後始末と #1323 の寄せが重なる衝突は、両方を残した（寄せが先・合図の比較が後）。検証中。

### 11.1 速さの 8 本のゲート（main `d33361cf`・まとめ枝 5〜8 の 16 commit の後）

席が T5 だけになった 13:05 に、T5 に一時停止を頼んで測った（`D:/NeNeNib/scripts/20261110/gate-main.sh`: ビルドの process が 0・負荷 12% 未満を 3 回続けて読んでから `eng/measure-speed.py --check --repetitions 5` を 1 回）。exe はまとめ枝 8 の木の Release（`D:/NeNeNib/worktrees/1327-integration/build-release/NeNeNib.exe`・main との違いは注記 3 行だけ）。

- **1 回目は無効**（13:07・値は 1 つも出ていない）: 本体 checkout が朝の `5f03f3a0` のままで `eng/window_driver.py` が古く、#1282 で TaskDialog になった「保存しますか」の箱に答えられずに `the confirmation did not take the answer` で落ちた。製品でなく道具の側の原因。本体 checkout を main に進めて測り直した。ログ `D:/NeNeNib/outputs/20261110/gate-main-d33361cf.log`。
- **2 回目**（13:08〜13:10・測る前の負荷 5% / 2% / 3%）: **8 benches checked・0 regression・0 unmeasurable**。ログ `gate-main-d33361cf-2.log`・記録 `out/speed/2026-11-10T04-10-18Z.json`。

| ベンチ | 中央値（ms） |
|---|---|
| startup-first-frame | 43.07 |
| startup-window-shown | 32.93 |
| key-to-frame-single | 0.606 |
| key-to-frame-burst-200 | 2.801 |
| open-large-file-16mib | 50.45 |
| key-to-frame-burst-200-16mib | 2.965 |
| key-to-frame-palette-5000 | 1.038 |
| key-to-frame-single-long-line | 1.123 |

### 11.2 #1320（窓の外読み）の probe の前後

`eng/compare-probes.py`（窓を出さない・ABBA）で、前 `a13db9be` / 後 `8d6d2781` の Release の `nib_perf_probes` を比べた（出力 `D:/NeNeNib/outputs/20261110/probe-1320/`）。後 / 前の中央値の比: `vim-count-l-giant-1line` 0.917・`vim-count-h-giant-1line` 0.999・`display-line-long` 1.000・`text-object-sentence-giant-1line-16384` 1.000・`text-object-tag-nested-4096` 0.945・`vim-insert-ctrl-w-giant-1line` 1.000。遅くなった所は無い。

## 12. まとめ枝 9 が main、波 4（13:20〜14:40）

- **まとめ枝 9**（Issue #1330・PR #1332・`d77a7602`）: 1 回目の検証は**ビルドで落ちた**。#1313（main）の合図の後始末と #1323 の寄せを両方残した `EditorController::apply` が 62 行になり、関数長の門（60 行）に当たった。門は変えず、ヘッダの注記と重複する 2 行の注記を外して 60 行に収め、全部を流し直した（Debug 全 target 警告 0・ctest 17 / 17・symbols 0・conformance 0・format・Release 警告 0）。→ **main `e7d926f4`**（14:38・CI run 42759861775・2 commit: #1312 `6e9dbec6`・#1309 `e7d926f4`）。
- **#1324 名指しの scope の穴**（席 T5 → PR #1331・`50c5d42c`）: 全 113 の selector を 1 本ずつ回した棚卸し（`D:/NeNeNib/outputs/20261110/named-scopes/`）で、落ちていたのは `--vim-macro` だけ（7 件: ほかの族の fixture 6 件が `macro` 欄を持っていて名前の接頭辞の検査に当たる 6 件と、件数 150 → 156 の 1 件）。製品の破れは無い。検査の側を直した（fixture は変えない）。穴は候補 (b) で塞いだ: 新しい selector `nib_tests --every-scope` が `scopes` の表を 1 プロセスで歩き（既定実行と名前も関数も同じ 55 本は飛ばして 58 本）、CTest の新しい項目 `nib_unit_scopes` が回す。費用: Release で既定実行 10.2 s・`--every-scope` 6.7 s（CI は +7〜15 秒）。Debug の手元の全件は混んだ機械で `nib_unit` 677 s・`nib_unit_scopes` 583 s（5〜10 分長くなる）。変異（fixture `char-search-f-first` を `dot-` に付け替える）で既定実行は通り `--every-scope` は 3 つの scope の件数で落ちる。ADR 0042 の追補と QUALITY_GATES.md §4 に active の 1 行。レビュー V-1331（Sonnet）: **merge してよい**（飛ばす 55 本は名前と関数ポインタの両方が一致したときだけで安全。軽い指摘: `--vim-macro` の検査が少し緩んだ・CTest の TIMEOUT を書いていない）。
- **#1307 折り返す窓で先頭の行が窓より高い**（席 I16 → PR #1333）: Vim の `comp_botline` は行の高さを `plines_correct_topline`（`w_height` で切る）で数えるので、先頭の行が窓より高くても「窓を埋める 1 行」と数え `w_botline` は先頭の次の行。Nib の `window_of` は `below = first` を返していた（+6 行の直し）。fixture `wrap-tall-first-` 17 件（直す前は 8 件落ちる）。レビュー V-1333: **merge してよい**（Vim の実測 約 30 件が全部一致・先頭の行が収まるときは値が 1 つも変わらない）。副産物で `so>0` の 2 つの形も直った（窓より高い行へ Ctrl-e / Ctrl-y した後のキャレット・最終行が窓より高い窓の `so=2` の `<C-f>` が `std::clamp(10, 11, 10)` の未定義動作の道に入っていた）。設計席が ADR 0019 にその 2 つと #1300 の残差を書いた（`0ac11b11`）。
  - **#1300 は別の根で、直さない（残差・Issue は開いたまま）**: Vim の `win_update`（drawscreen.c）は描いた後の `w_botline` が描く前と違うと `update_topline` をもう 1 回だけ呼ぶ（`normal! 10G` の直後の `line('w0')` は 9・`redraw` の後は 10）。やり直しは描くときだけ起きる（1 本の `normal!` の中の `10GH` は Vim でも 9 / 9）ので、「鍵ごとにもう 1 回回す」形では合わない。合わせるなら「frame のときだけもう 1 回」で、追従の設計（ADR 0019 / 0129）の変更になり、今は守る fixture も作れない。
  - 席 I16 は #1300 への調査のコメントを投稿しようとして権限の確認で止められ、設計席に代わりの投稿を頼んだ。設計席は代わりに投稿していない（原稿は `D:/NeNeNib/scripts/20261110/1307/comment-1300.md` に未投稿のまま・中身は上の 2 行）。
- **#1316 段落と文の移動 `{` `}` `(` `)`**（席 I13 → PR #1334・`390b8d7d`）: `{` `}` は Vim の `findpar`（境は空行だけ・空白だけの行は境でない）。`ip` `ap` の `current_par` は `linewhite` を使うので、Vim 自身が 2 つの判定を持つ（1 本にまとめない）。`(` `)` は `is` `as` と同じ `findsent(dir, 1)` を回数ぶん（#751 の塊の走査）。4 つとも排他的・文字単位・常に `"1` に書く（`use_reg_one`）・ジャンプ。fixture 60 件（main では 52 件落ちる）。巨大な 1 行の `)` は 23 µs（probe `sentence-motion-giant-1line-16384`）。関数長の門で `moved_by` `landing_of` `motion_range` の case を補助関数に分けた。席の自己申告: 自分が足したばかりの fixture を捨てるのに `git checkout --` を 1 回使った（約束の破り・失われたものは無い）。→ レビュー V-1334。
- **#1319 レジスタの後の回数**（席 I14 → PR #1335・`41105b5e`）: Vim の `nv_regname` は `"` の前の回数を `opcount` に覚え、次の命令は回数を 0 から読み直して掛ける（`5"a0` は回数 5 の行頭・`2"a3dd` は 6 行・`2"a3d2j` は 12 行）。新しい型 `VimRegisterCount`。fixture `count-register-` 21 件（修正前は 13 件落ちる）。`KeyMapTests` の Z11 を Vim の値 5 に改めた。→ レビュー V-1335。
- **まとめ枝 10**（Issue #1336・`D:/NeNeNib/worktrees/1336-integration`）: #1331（`ec266e4e`）と #1333（`b283f597`）を積んだ（衝突なし）。#1334 と #1335 のレビューを待って足す。

## 13. 閉じてから終わるまでの時間の調査（設計席・15:25〜15:40）

朝の soak（§3）で「exe を止めてから終わるまで 3〜8 秒かかる周がある」と見えたので測った（Release・main `e7d926f4` と同じ製品のまとめ枝 9 の木の exe・道具の profile は D の HDD・席のビルドが 2 つ動いている中）。

1. `D:/NeNeNib/scripts/20261110/exit-time.py`（起動 → 0.3 秒か 3 秒待つ → 閉じる → process が終わるまで・各 4 回・記録 `D:/NeNeNib/outputs/20261110/exit-time.json`）:

| 閉じ方 | 文書 | 終わるまで（秒） |
|---|---|---|
| `WM_CLOSE` | なし | 0.14〜0.80 |
| `WM_CLOSE` | 長い日本語行 1000 行 | 0.33〜1.64 |
| `TerminateProcess` | なし / あり | 0.000〜0.031 |

   強制終了は 16 ms で終わる。**朝の 3〜8 秒は再現しない**（作業木 31 本の削除と 9 席のビルドで D が混んでいた時の値で、製品の側の原因ではなかった）。

2. 普通の閉じ方の中身を「窓が消えるまで」と「process が終わるまで」に分けた（`IsWindowVisible` を 2 ms ごとに読む・各 5 回）: 窓が消えるまで 0.18 / 0.23 / 0.40 / 0.40 / 0.77 s（文書なし）・0.34 / 0.46 / 0.47 / 0.49 / 1.13 s（長い文書）。窓が消えてから process が終わるまでは約 25 ms。

**分かったこと**: 閉じる操作から窓が消えるまでの 0.2〜1.1 秒は、閉じるときの書き込み（一覧 `session.v2`・履歴 `history.v1`・自動退避の片付け・保存ごとに `FlushFileBuffers` と write-through の改名）を、窓が見えたまま UI スレッドで待っている時間。SSD の実ユーザーの profile では短いはずだが、遅い媒体では「× を押しても窓がしばらく残る」体験になる。#424（2026-10-16 の probe では閉じる 98〜376 ms）と同じ領域で、閉じるときに書くものが増えたぶん伸びている。

→ Issue **#1340**（先に窓を隠してから書き、最後に壊す。書き込みは worker に移さない・flush は外さない）・席 I17（先に内訳を測ってから直す）。

## 14. 波 5（14:40〜）

- **まとめ枝 10**（Issue #1336・PR #1339・`bb7aa417`）: #1331 #1333 #1335 の 3 commit。#1335 の ADR の和で、#1335 が書き換えた ADR 0176 の追補 31 が main の追補 32〜35 の後ろに重複して入ったのに気づいて 1 つに直した（**教訓: ADR の「両側の和」は、番号つきの追補の書き換えと追加が重なると重複を作る。和の後に同じ番号の行が 2 つ無いかを確かめる**）。検証: Debug 全 target 警告 0・**ctest 18 / 18**（801 s・新しい `nib_unit_scopes` が今日 main に入った scope を含めて全部の名指しの scope を回して通った）・symbols 0・conformance 0・format・Release 警告 0。fixture 3602 件・折り返す窓の fixture 332 件。→ `merge-pr.py 1339 --integration --execute`。
- レビュー V-1335: **merge してよい**（Vim の実測 約 40 形・`VimState.count` と `register_count` が片方だけ更新される道は無い）。後続 Issue **#1337**（レジスタと回数つきの `.`: `x"a3.` は Vim が 9 文字・Nib 3 文字。Vim の `nv_dot` は `regname` を消さず回数が 2 乗になる・前からある違い）。
- レビュー V-1334（`{` `}` `(` `)`）: **直してから merge**（Vim の実測 534 件のうち 521 件一致・必須 1）。`)` `(` の回数が本文の終わりを越えると Vim は失敗するのに Nib は終わりへ動く（`One. Two.` に `4)`・`d4)` は Vim は何もしないのに Nib は全文を消す）。同じ原因でループが回数ぶん回り切り、**`9999999)` が Debug の再生器で約 28 秒**（回数の上限では窓が固まる）。Nib は `[` を知らないので `[{` を打つと `{` が段落の移動として走る食い違いも。→ 席 I13b（Vim の `findsent` の「動かなかったら FAIL」を写す・回数ぶん回り切るループをほかの移動とテキストオブジェクトでも総点検・`[` `]` を「次の 1 鍵を取って失敗する未実装の 2 鍵の命令」に）。
- **#1325 Vim の `<Del>`**（席 I15 → PR #1338・`d2c96817`）: NORMAL は `x`・`d<Del>` は失敗・`"a<Del>` はレジスタの名前を捨てる・回数を打っている途中の `<Del>` は最後の桁を消す（`12<Del>x` は 1 文字・`3<Del>` は本文を変えない）・1 鍵を待つ状態の `<Del>` は失敗・VISUAL は `x`・INSERT は前方の Vim の 1 文字（結合文字ごと）を消し行末では次の行とつなぐ・`:` `/` `?` の行では行末で手前を消し空の行では閉じる。fixture `del-key-` 47 件。レビュー V-1338: **直してから merge**（Vim の実測 44 件のうち違い 5 件・全部同じ原因）。INSERT の `<Del>` で undo の単位が割れる（`ix<Del>y<Esc>u` は Vim が `abc`・Nib が `xabc`）。原因は `core::absorbed` が「入れた本文の終わりから後ろへ消す削除」を畳まないこと。→ 席 I15b（通常モードの Delete の undo の単位を変えない条件つき・Vim モードの Shift+Delete は送らない）。

## 15. まとめ枝 10 が main、まとめ枝 11（15:40〜16:10）

- **まとめ枝 10** → **main `d8f33693`**（15:56・3 commit: #1324 `432f31c7`・#1307 `19a846c8`・#1319 `d8f33693`）。**道具が merge の後で止まった**: merge は成功したが、その後の照合（`compare` API）が 60 秒で時間切れになり `merge outcome unknown: inspect main before any further action`・exit 2。設計席が main を確かめた（3 commit・まとめ枝 `bb7aa417` の木と差分 0・PR #1339 は MERGED・Issue 4 本は閉じた）うえで、開いたままだった元の PR 3 本（#1331 #1333 #1335）を手で閉じた。道具の後続は Issue **#1345**（merge の後の照合と閉じる段も待ち直す・続きを流す口）。
- **#1338 の直し**（席 I15b・`3614d303`）: `core::absorbed` に「入れた本文の終わりから後ろへ消す削除を畳む」枝を 1 つ。`absorb` を渡すのは Vim の INSERT だけ（通常モードの打鍵は `coalesce`・Delete は `separate`）なので通常モードの undo の単位は変わらない（経路の読みと契約 2 件）。fixture `del-key-undo-` 5 件・レビューの probe 44 件の違い 5 → 0。Vim モードの Shift+Delete は送らない。→ 受理。
- **#1334 の直し**（席 I13b・`f867e092`）: Vim の `findsent` の EQUAL_POS の枝を写した（動かなければ 1 文字進めてやり直し、進めず残りの回数があれば失敗）。fixture `sentence-motion-count-past-end-` 14 件。`[` `]` は「次の 1 鍵を取って失敗する」接頭辞 `VimPrefix::bracket`（`[{` `]}` `[(` `])` `[[` `]]` は何も動かさずに失敗）。**巨大な回数（999999999）で固まる形の総点検**（Debug の再生器・96 件・表 `D:/NeNeNib/outputs/20261110/i13b-huge-count/`）で、前から main にあった固まりが 3 群見つかった:

  | 形 | 扱い |
  |---|---|
  | `{N})` `{N}(` | この PR の新しい鍵。M1 で直した |
  | `d/v{N}is` `as` | 直した（端に着いた後は 2 周の繰り返しなので、残りを同じ偶奇の 1 か 2 に縮める） |
  | `{N}n` `N` `*` `#` | 直した（折り返しで最初の当たりへ戻ったら繰り返しの分を飛ばす） |
  | `{N}@a`（鍵の列を回数ぶん並べてから再生する・`300000@a` は 180 秒超）・`{N}p`（`999999999p` は約 4 GB の確保） | 直しが小さくないので Issue **#1341** → 席 I18 |

  ほかの鍵（`ip` `ap` `it` `at`・`w` `b` `e`・`f` `t`・`j` `k` `gj` `gk`・`H` `M` `L` `%` `G`・Ctrl-d / u / f / b / e / y・`x` `X` `.` `>>` `J` `~` `r`）は巨大な回数でも 30〜50 ms で返る。Release では `999999999` の `) ( } { is as n *` が `l` に比べて +0.11〜0.18 ms。
  - 再レビュー V-1334-2: **merge してよい**（退行 0）。`is` `as` の近道は本文 5 種 × 鍵 7 形 × N = 1〜12・41〜43 の 524 件と巨大な回数 70 件が全部 Vim と一致（周期は 1 か 2）。`n` `N` `*` `#` は 566 件中 512 件一致で、違う 54 件は直す前の再生器でも同じ 54 件（範囲が空になる `y{N}N` の種類と `c{N}n`・前からある違い）→ Issue **#1344**。副産物: Vim 9.1 自身が複数の文の本文で `G$vb3isd` に 30 秒返らない（Vim の側の問題）。
- **まとめ枝 11**（Issue #1343・`D:/NeNeNib/worktrees/1343-integration`）: #1338（`8b8a1ab3`）と #1334（`100e2e9a`）。#1338 は oracle の鍵の表に `<Del>` を足すので、枝の commit を `--reuse-ref` にして main 側だけの 21 件を測り、#1334 はまとめ枝を `--reuse-ref` にして 86 件を測った。fixture 3740 件。検証中。
- 新しい席: I17（#1340 閉じるとき先に窓を隠す）・I18（#1341 巨大な回数の `@` と `p`）。

## 16. まとめ枝 11 が main、ゲート 2 回目、波 6（16:10〜17:45）

- **まとめ枝 11**（Issue #1343・PR #1349・`a965b945`）: 検証の 1 回目で **新しい CTest `nib_unit_scopes` が `--vim-macro` の件数で落ちた**（#1338 がレジスタつきの fixture を 3 件足して 156 → 159・ほかの 17 本は通過・製品の破れではない）。今日入れたばかりの強制（#1324）が、積んだ木で最初の仕事をした。件数の検査と注記を 159 に直し、`nib_tests` を作り直して `ctest -R '^nib_unit'`（`nib_unit` 390 s・`nib_unit_scopes` 270 s）を流し直した。→ **main `c67cf493`**（17:33・CI run 42779281458・2 commit: #1325 `f0625f7a`・#1316 `c67cf493`）。
- **速さの 8 本のゲート 2 回目**（17:00・main `d8f33693` = まとめ枝 9・10 の後・exe はまとめ枝 10 の木の Release・測る前の負荷 1% / 7% / 2%）: **8 benches checked・0 regression**。起動 42.74・窓が見えるまで 33.17・1 打鍵 0.584・200 打鍵 2.824・16 MiB を開く 49.44・16 MiB で 200 打鍵 2.982・5000 件の面 1.037・長い行の 1 打鍵 1.011（ms）。ログ `D:/NeNeNib/outputs/20261110/gate-main-d8f33693.log`。
- **#1340 閉じるとき先に窓を隠す**（席 I17 → PR #1346）: 先に内訳を測った（Release・HDD の profile・一時の節目・commit していない）。時間を使うのは一覧 `session.v2` の書き込み（空の文書で中央値 565 ms・最大 12.4 秒）と履歴 `history.v1`（長い文書で中央値 133 ms）だけで、`DestroyWindow` は 11 ms・worker と device の解放は 15 ms。ui/win32 に純関数 `closing_steps(ClosingRoute)`。閉じる操作から窓が見えなくなるまで **中央値 567 ms → 2.6 ms**。
  - レビュー V-1346: **直してから merge**（必須 1）。**OS の終了の道（`WM_ENDSESSION`）では隠してはいけない**: 見える窓の無いアプリは終了を止められず 5 秒で自動で終わらされる（Microsoft Learn「Shutdown Changes for Windows Vista」）。隠すと、強行の終わりの退避の flush・一覧・履歴の書き込みに使える時間が縮む。
  - 直し（席 I17b・`f0d3236a`）: `WM_ENDSESSION` の道は「書く」の 1 段だけ。× で閉じる道は「**変換を取り消す** → 隠す → 書く → OS の終了を止める理由を外す → 壊す」。席が読みで見つけた穴: 隠すと `WM_KILLFOCUS` で IME が変換を確定し、`CommitText` が本文に入って、閉じる確認を通り抜けた後に未保存が生まれる（前は確定が書いた後に来て黙って消えていた）。列を段へ渡す口は `run_closing_steps` の 1 本で、替え玉で呼ばれた段を数える試験。後続 Issue **#1347**（前の Nib が一覧と履歴を書き終わるのを、新しい Nib が起動で上限つきで待つ）。
  - 再レビュー V-1346-2: **merge してよい**（確認の箱でキャンセルしたときは変換が消えない・保存済みの文書で変換中に閉じた結果は直す前と同じ）。試用で見る手順: 変換中に × で閉じ、次の起動で「前回の未保存の編集が残っています」が出ないこと。
- **#1341 巨大な回数の `@` と `p`**（席 I18 → PR #1348・`6e1effe1`）: Vim 9.1 の `nv_at` は回数ぶんレジスタを typebuf へ積みきってから流す（`100000@a` 0.78 秒・`1000000@a` 9 秒・`999999999@a` は中身が `j` でも 20 秒で終わらない）。`p` は確保できる限り実行する（`yl999999999p` は 1 GB の行を作る）。Nib は `VimReplay.rounds` を足し、engine は 1 周ぶんの鍵だけを返して controller が 1 周ずつ積む（`999999999@a` の `j` が 11.8 秒超 → 31〜38 ms）。`p` `P`・INSERT の回数の反復・`o` は編集の後の文書が 1 GiB を超えるなら確保の前に `too_large` で断る（10^9 を 45〜68 ms で断る）。fixture `huge-count-` 18 件。
  - レビュー V-1348: **直してから merge**（必須 1）。流れる順は小さな回数 59 件で前後が全部一致。**CRLF の文書では上限をすり抜ける**（判定は LF のバイト数・その後で `\n` を `\r\n` に写すので約 2 GiB の文書になる）。→ 席 I18b（改行の形の幅で数える・断ったときは Vim と同じ `E1240: Resulting text too long` の 1 行・VISUAL の `>` も）。
  - 席の手順上の自己申告: Vim の probe の時間切れのたびに `taskkill /F /IM vim.exe`（名前での一括の停止）を約 12 回呼んでいた（15:44〜15:58）。ほかの席の oracle を巻き込んだ可能性がある（その時間帯に設計席が測り直した fixture は無く、殺された oracle は値でなくエラーになる）。以後の共通の依頼書に「止めるのは自分の PID だけ」と書いた。
- **#1345 merge の道具**（席 I20 → PR #1350・`46a9ab92`）: merge の後の読みと「元の PR を閉じる」段を、時間切れと 5xx だけ #1223 と同じ表（30 秒・最大 10 回）で待ち直す。読めなければ `unfinished`（merged・mergeCommit・mergeBase・閉じた PR・閉じていない PR）と続きの命令を出す。続きの口 `--integration --resume-after-merge --merge-base <sha> [--execute]` は、PR が MERGED・mergeCommit が main にある・積んだ commit の列が数・順・件名・`PR #K（head <sha>）` の行・最後の sha まで一致、の全部が合うときだけ閉じる。試験 58 件（新規 9）。レビュー V-1350: **merge してよい**（閉じてはいけない PR を閉じる入力の列は 7 つの形を追って見つからない・旗なしの道は変わらない。軽い指摘: 待ち直しが入れ子になると全体の締め切りが無い）。
- **#1337 の下調べ**（席 I21・probe・`D:/NeNeNib/scripts/20261110/1337/`・出力 `D:/NeNeNib/outputs/20261110/probe-1337/`。席は report.md を書く道具を止められたので要旨を写す）: Vim 9.1 の `nv_dot` は `regname` を消さず、`normal_cmd` は `regname` が残る間 `opcount` を持ち越すので、`M"aN.` の回数 C = M × N に対して再生の回数が **C²** になる（`x"a3.` = 9・`x2"a3.` = 36）。`:help .` には書かれていない（文書と食い違う副作用）。`"a.` は再生した変更を a へ書く。Nib との差は 55 件中 39 件（回数 32 件・行き先 7 件: `.` の再生が選んだレジスタを捨てるので a が空のまま）。設計席の裁定は (A) Vim の実測をそのまま写す（oracle に fixture を測らせられるのは A だけ）。ただし 2 乗にする所は 1 つの関数に閉じ、ADR に「文書と食い違う Vim の振る舞いを実測のまま写した」と書く → 席 I23。**hide に確かめる項目**。
- **まとめ枝 12**（Issue #1352・`D:/NeNeNib/worktrees/1352-integration`）: #1346（`fae98724`）と #1350（`21da316d`）。窓の試験の表（`NibWindowTests.cpp` の `std::array` 27 → 28）と `CMakeLists.txt` の試験のソースの列の衝突は両側を残して数え直した。検証中。
- 新しい席: I19（#1326 全部の fixture を窓の道でも再生する契約）・I22（#1017 ミニマップ 段 1・既定オフ）・I23（#1337）・I24（#1344）・F6（fuzz 生成器 6: `<C-e>` `<C-y>`・`{ } ( )`・`<Del>`・レジスタと回数の語）。起動画面 #1018 は見た目の判断（hide と /design）が先に要るので出していない。

## 17. hide の決定（夕方）と、まとめ枝 12 が main、波 7（17:45〜19:20）

### 17.1 施主決定（hide が戻って答えた・番号は D75〜D77）

| 番号 | 項目 | 決定 |
|---|---|---|
| D75 | 失敗しない中身の巨大な回数のマクロ（`999999999@a` の中身が `x` など）は窓が固まる | **上限にする**（「エスケープじゃなくて上限でいいよ」）。いちばん外の再生で流す鍵の数に上限を置き、超えたら残りを捨てて 1 行知らせる。Esc の割り込みは作らない。値は Release で測って「最悪でも数秒」に収まる所に決め、値と知らせの文は実装の PR で hide に見せる（Issue #1351） |
| D76 | レジスタつきの `.` の回数（Vim は `x"a3.` で 9 文字 = 2 乗・`:help .` に無い振る舞い） | **Vim の実測どおり写す**（hide「移そうか」をそう読んだ）。2 乗にする所は `register_dot_count` の 1 か所に閉じる |
| D77 | 全面の Markdown プレビューの鍵 | **Ctrl+Shift+M の巡回**（閉じている → 側の枠 → 全面 → 閉じている・鍵を増やさない） |
| — | 起動画面 #1018 | 今日は出さない（見た目の判断が先・「おけ」） |
| — | 席が止められた #1300 へのコメント | 未投稿のまま（「おけ」＝了解と読んだ。中身は ADR 0019 と §12 に書いてある） |

### 17.2 まとめ枝 12 と 13

- **まとめ枝 12**（Issue #1352・PR #1358・`21da316d`）: #1346 #1350 の 2 commit。Debug 全 target 警告 0・ctest 18 / 18（1133 s）・symbols 0・conformance 0・道具の試験 58 件・format・Release 警告 0。道具自身を変える PR なので **main の道具で merge**。→ **main `b05d2c37`**（19:07・CI run 42791645759・2 commit: #1340 `baea22e8`・#1345 `b05d2c37`）。
- **まとめ枝 13**（Issue #1370・`D:/NeNeNib/worktrees/1370-integration`）: #1368（`0481bad2`）・#1367（`b7a87e1e`）・#1355（`dce22796`）。窓の試験の表の衝突は 2 回続いたので `resolve-window-tests.py` にした（29 群）。#1355 の scope の件数はレビューが出した乗せ替えの後の値（`--vim-macro` 164・`--vim-dot` 254）。fixture 3812 件。検証中。

### 17.3 波 6・7 の結果

- **#1337 レジスタつきの `.`**（席 I23 → PR #1355・`052dfb00`）: `register_prefixed` が記録した鍵の頭に `.` の前の `"{name}` を置く（記録の頭がもう `"` なら記録の名前が勝つ）。回数と名前の両方があれば `register_dot_count` で C²（上限 999999999）。下調べで違っていた 39 列が全部 Vim と一致。fixture `dot-register-count-` 43 件。レビュー V-1355: **乗せ替えの後に merge してよい**（Vim の実測 62 列のうち 60 列一致・不一致 2 列は道具の側）。
- **#1326 全部の fixture を窓の道でも再生**（席 I19 → PR #1356）: 鍵を `EditorWindowKeys.cpp` と同じ意図に写して 1 つずつ `apply` し、打った鍵の engine の道（`press_vim_key`）の本文・キャレット・レジスタ・窓・モードと比べる。**fixture 3,612 件 ＋ 折り返す窓 332 件を再生して食い違い 0・飛ばし 0**。既存の fixture だけでは #1323 のレビューが止めた退行の形（INSERT の寄せ）を入れても落ちなかった（挿入点が Vim の 1 文字の途中に立つ fixture が無かった）ので、Vim の実測の 10 行を fixture `window-path-1326-*` に足した。レビュー V-1356（Sonnet）: **直してから merge**（main に Vim の Delete キーが入って試験の「窓の振り分けの写し」が古くなりビルドが落ちる・固定の件数が fixture の増えで古くなる）→ 席 I26（新しい枝に写して乗せ替え・件数の断言を「絶対の数」から「関係」に・写しの `switch` は鍵の種類が増えたらコンパイルが落ちる形・既定実行に入る分は 40 件に 1 件）。
- **#1354 Ctrl+Shift+M の巡回（D77）**（席 I25 → PR #1368・`c0b5d96f`）: 純関数 `cycled_preview_placement(PreviewPlacement)`。決定 27（全面 → 側）は退いた。F1 の説明「Markdown のプレビューを右（下）の枠に出す・もう一度で全面・もう一度で閉じる」。ADR 0155 追補 43〜46 と仕様の D77 の行。レビュー V-1368（Sonnet）: **merge してよい**。実機の確かめ（窓を前面に出す道具）は席もレビューも回していない → 試用の手順に入れる。
- **#1344 検索つきオペレータの空の範囲**（席 I24 → PR #1367・`19015264`）: `search_operated` から「範囲が空なら何もしない」の早い返りを外し、空の範囲も既存の `performed` に流した（`d0` `c0` `y0` と同じ道・検索だけの規則は書かない）。Vim 9.1 は空の範囲を失敗にしない: `y` は空の文字単位をレジスタに書く・`c` は INSERT に入る・`d` は何も書かずに成功して `.` に残る。再レビューの検索の組の違い 42 → 0・巨大な回数の組 10 → 0。使う人に見えていた違い: `c{N}n` の後に打った文字が入らず手前の空白が消えていた。fixture `search-operator-empty-` 29 件。レビュー V-1367: **merge してよい**（追加の実測 24 件が全部一致・前の検索の組は修正前 523 → 565 件一致・退行 0）。後続 Issue **#1369**（`q` `"` `@` の後の `<Esc>` は Vim では取り消し）。
- **fuzz 生成器 6**（席 F6 → PR #1365・Issue #1353）: `<C-e>` `<C-y>`・`{ } ( )`（単独と `d y c v V`・回数が終わりを越える）・`<Del>`・レジスタと回数。本文の半分は文章（空行の段落・空白だけの行・`.PP` の行・全角の句点と閉じ括弧）。生成器 3〜5 の固定 seed の列は main の道具と完全に同じ（seed 1〜10 × 300 を比べた）。**seed 1〜10 の 3000 件: 一致 2913・既知 8・新規 79 件（6 種類）**（出力 `D:/NeNeNib/outputs/20261110/vim-fuzz-gen6-seeds1-10/`・仕分け `D:/NeNeNib/outputs/20261110/F6-gen6/kinds.md`）:

  | 種類 | 件数 | Issue |
  |---|---:|---|
  | `{ } ( )` と `ip` `ap` が nroff のマクロの行（`.PP` `.IP` `.SH`）を段落の境に数えない | 71 | #1359（席 I27） |
  | 回数つき `<C-f>` が終わりを越えるときの窓（`so>0`） | 3 | #1362（席 I28） |
  | 矩形の `c` / `>` の後の `gv` | 2 | #1361（席 I29） |
  | VISUAL の中で跳んで `>` `~` で閉じた後の `''` | 1 | #1360 |
  | 回数つきの INSERT で複数行を入れた後の窓 | 1 | #1363 |
  | 空白だけの行をまたぐ `d{N}<BS>` の種類 | 1 | #1364 |

  レビュー V-1365（Sonnet）: **直してから merge**（既知の違いに数える規則 `DEFAULT_KNOWN` が広い・閉じた #1231 の規則が残っている）→ 席 F6b（#1231 と #1337 の規則を外す・#1310 の規則は「最小化した件で `<C-e>` の後の語がちょうど 1 つ」に狭める・既知に数えた件は最小の鍵列を並べて出す）。
- **ミニマップ 段 1**（席 I22 → PR #1366・Issue #1357・親 #1017）: `features=minimap`（既定オフ・`:set minimap`）。core の純関数（1 行の塊・窓・帯）・application の `MinimapCache`・ui の `MinimapPicture`（bitmap・frame ごとは面の地 ＋ `DrawBitmap` 1 回 ＋ 見えている範囲の面）。絵 `D:/NeNeNib/evidence/20261110-1017/minimap-cpp.png`（設計席が目で確認: 右端に構文の色の塊の縮図と見えている範囲の面）。段 2 に残したもの: クリック・ドラッグ・ホイール・スクロールの帯の描き足し・枠ごと。
  - レビュー V-1366: **直してから merge**（必須 1: 構文の色の役割を enum の数値で引き、役割が増えるとコンパイルは通るのに描画の中で例外か色ずれ）。「オフで分岐なし」の形（メンバ関数ポインタと表引き）は、速さではほぼ差が無い見込み（1 打鍵あたり 1 µs 未満）だが、字面を守るためにオフでも配置の計算を毎 frame 余分にしていた。**設計席の判断: `if` の早い return に直す**（ほかの機能の前例と同じ形）。ADR 0171 決定 1 は「オフの費用を早い return の分岐 1 つに留める」と読み替える追補。+1% の線は測るまで「未測定」。→ 席 I22b。A/B は直しが届いてから設計席が測る。
- **#1348 の直し 2 回目へ**（I18b `371374cb` → 再レビュー V-1348-2: **直してから merge**・必須 2）: 前回の必須（CRLF）は直った。残っていた抜け: 矩形の `I` `A` の回数は 1 行目のぶんだけ確かめて全部の行へ写す（2,048 行 × 600,000 回で約 1.23 GB）・矩形の `p` が Tab を割ると見積もりが 1 行あたり最大 4 バイト小さい。→ 席 I18c（直しと、「回数ぶんの本文を作る所」の総点検の表）。

## 18. 波 7 の結果と、無効にした A/B（19:20〜20:45）

- **#1326 の乗せ替え**（席 I26 → PR #1372・古い #1356 は閉じた）: main の fixture 3,750 件 ＋ 折り返す窓 332 件（Vim の Delete キーを含む 57 件も）を窓の道で再生して食い違い 0・飛ばし 0。件数の断言は「関係」（選んだ fixture を全部再生・飛ばし 0・食い違い 0）に変えたので、ほかの PR が fixture を足しても古くならない。窓の振り分けの写しの `switch` は `VimSpecialKey` の全部の値を `default` なしで並べる。既定実行に入る分は 40 件に 1 件 ＋ `window-path-` の全部で 7.2 秒（前は約 26 秒）・全件は `nib_unit_scopes` が回す（224 秒）。#1323 の最初の形の変異で既定実行でも全件でも 5 件落ちる。席が止まった点（試験は層の決まりで窓の表 `VimSpecialKeys.hpp` を読めない）は候補 A（写しのまま）。→ 設計席が受理。
- **fuzz 生成器 6 の直し**（席 F6b・`fab8da4e`）: 既知の規則から #1231 と #1337 を外し、#1310 は「`--shrink` で縮んだ件で、語が `<C-e>` / `<C-y>` と後の 1 語の 2 語だけ・違う欄に `first_visible`」のときだけ。回し直して不一致は同じ 87 件・既知 8 / 新規 79 → **既知 3 / 新規 84**。既知に数えた件は規則ごとに最小の鍵列を並べて出す。→ 設計席が受理。
- **#1359 段落の境**（席 I27 → PR #1373）: Vim の `startPS(lnum, NUL, FALSE)` が真になるのは、空の行・行頭が改ページの行・行頭の `.` の後ろの 2 文字が既定の 'sections' か 'paragraphs' の名前に当たる行（`inmacro`）。1 本の述語 `vim_paragraph_start`（名前の表 21 行）を `{` `}`・`ip` `ap`・`(` `)` `is` `as` が呼ぶ。行頭の `{` は境に数えない（依頼書と Vim の実測が食い違い、席は実測の側に倒した）。fixture `paragraph-macro-` 24 件（main では 17 件落ちる）。**fuzz 生成器 6 の不一致 87 → 17**。レビュー V-1373: **merge してよい**（実測 52 件のうち 51 件一致・ふつうのファイルの誤爆 20 件は全部 Vim と同じ: CSS の `.pp {` や `.gitignore` の `.sh` は Vim でも境・`.PHONY:` や `. ./script.sh` は境でない）。ADR 0031 の生の改ページのバイトは設計席が `\f` の表記に直した（`854ca536`）。席が見つけた別の違い → Issue **#1374**（行を削除した直後の `<C-d>` の窓の先頭が 1 行違う）。
- **#1348 の直し 2 回目**（席 I18c・`d76faf7e`）: 矩形の `I` `A` `c` `C` は写す前に「1 行ぶん × 行数」で確かめる（`block_copy_fits`）。矩形の `p` の見積もりは `max(桁, 8) + 改行の幅` で、Tab を割る形・全角を割る形・行末より右に貼る形・行を足す形の 4 つで上界（「断らなかったなら、写した後の本文は上限以内」の契約・直しを外すと 8 件落ちる）。**総点検の表**: 上限を見る道 = `p` `P`（文字・行・矩形）・INSERT の回数・`o` `O`・矩形の `I` `A` `c` `C`・VISUAL と矩形の `>`（`.` と `@` も同じ道）。行や行数で切れる道 = `>>`・`r` の回数・`J`・`~` `g~`・`<C-a>`。残った穴 → Issue **#1376**（通常モードの「すべて置換」は上限を見ていない・定数倍まで増える道 3 つ）。→ 設計席が受理（レビュー 2 回の必須は全部直り、契約と総点検の表がそろった）。
- **ミニマップの直し**（席 I22b・`b203c502`）: 構文の色は `core::syntax_color(body, role)`（`default` の無い `switch`）から引く。オフの道は早い return（メンバ関数ポインタと表引きをやめた・renderer はオフのとき配置を計算しない）。同じ行数の編集は変わった行だけ書き換え・見えている範囲の面は論理行で・全面プレビューの間は出さない・面の上の押下は何もしない（契約）。絵 `minimap-wrapped-r2.png`（設計席が目で確認）。段 2 は Issue #1371。
- **#1362 回数つき `<C-f>` が終わりを越える**（席 I28 → PR #1377・`6af04280`）: Vim の `onepage` は FAIL の周の後は `cursor_correct` だけを呼び、その後の `update_topline` は `VALID_TOPLINE` が立っていれば何もしない。`VimNavigate` の `bool keeps_top` を閉じた enum `VimToplineSettle`（`margin_followed` / `updated` / `kept`）に替えた。fixture 20 件（main では 11 件落ちる）。レビュー V-1377: **merge してよい**（実測 26 件が全部一致・Ctrl-e / Ctrl-y の振る舞いは変わらない）。`z-2<C-f>` は製品の違いでなく oracle の側の差（`:normal!` の中では描き直しが無く、`z-` が置いた古い `w_botline` を次の頁移動が読む・`redraw!` を挟むと Vim も Nib と同じ値）。後続は #1310 に反例を足した。
- **#1361 矩形の後の `gv` とマーク**（席 I29 → PR #1378・`079ff9f0`）: 矩形の編集が 1 つの Edit に畳まれ、`marks_adjusted` に「行を消して足した」と読まれて、前の選択の端も**名前つきのマークも 1 行目に寄っていた**（`Gma<C-v>ggl>'a` が 1 行目に着く）。`marks_block_adjusted`（行ごとの置き換えを下の行から `marks_adjusted` に通す）。fixture 22 件（修正前は 21 件落ちる）。レビュー V-1378: **merge してよい**（実測 52 件で退行 0）。同じ根の残り（行数を変えない行単位・文字単位の書き換えの後のマーク・`u` の後）は、Vim が「編集の形でなく操作の種類」で `mark_adjust` を呼ぶかを決めているので、既にある Issue #793 の仕事。#793 に表と裁定を書いた（候補 B は `Vjp` で崩れるので採らない）。

### 18.1 無効にした測定: ミニマップの A/B（20:24〜20:32）

席が全部引けた 20:24 に、ミニマップをオフにした枝 `b203c502` と同じ基点 `d8f33693` の Release を「枝 → 基点 → 枝」の順に 8 本ずつ測った（測る直前の負荷 6〜9%・ビルドの process 0・3 回とも 0 regression）。ところが、**hide が 20:00 ごろから同じ機械でほかの席を動かしていた**と、測った後に分かった（「今他の立ち上げているから少し遅いかも」「20:00 くらいには立ち上げてた気がする」）。CLAUDE.md §2「速さを測る間は、同じ機械のほかの席とビルドを止める」の前提を満たしていないので、**この測定は無効**とし、値を受理の根拠にしない。

- 設計席は一度この値で ADR 0171 の強制の行を active に書き換えて push した（`c23a04cc`）。前提が崩れたと分かった時点で「未測定」に戻し、無効にした測定として ADR に残した（`df6ef4e0`）。
- 参考の値（中央値 ms・枝 / 基点 / 枝）: 1 打鍵 0.581 / 0.625 / 0.652・200 打鍵 2.213 / 2.821 / 2.148・長い行の 1 打鍵 1.101 / 1.100 / 1.075。200 打鍵は試行ごとに約 2.1 ms と約 2.8 ms の 2 つの山に分かれていた（基点の 5 本 2.89 / 2.89 / 2.82 / 2.16 / 2.33）。
- **教訓**: 負荷の数字とビルドの process だけでは、ほかの席が動いているかは分からない。hide が在席に戻った後は、測る前に「ほかの席は動いていないか」を聞く（CLAUDE.md §2 に書いてあるとおり）。
- ミニマップ #1366 は、静かな機械で 1 回だけ測り直すまでまとめ枝に積まない。まとめ枝 13 の後の 8 本のゲートも同じ。

## 19. まとめ枝 13 が main・試用の exe・hide の試用（20:39〜）

- **まとめ枝 13**（Issue #1370・PR #1375・`dce22796`）→ **main `14ad10ff`**（20:39・CI run 42804577023・3 commit: #1354 `d9a534ec`・#1344 `5906979d`・#1337 `14ad10ff`）。今日 main に入った新しい道具（merge の後の待ち直しつき）で merge した。ここまで 9 回のまとめ枝で 28 commit。
- **試用の exe**: `D:/NeNeNib/evidence/20261110-trial/NeNeNib-main-14ad10ff.exe`（まとめ枝 13 の木の Release・main との木の差分 0・SHA-256 `53570d57b32bbbd012c6339821181068840b22f5a96304445e5a221994606cdc`）と手順の表 `README.md`（通常モード 5・Vim 11・map 3 項目と、入っていないものの一覧）。
- **hide の試用**（20:45〜・設計席が `Start-Process` で 4 回起動）:

  | # | 項目 | 結果 |
  |---|---|---|
  | 1 | 「保存しますか」の箱が「保存する／保存しない／キャンセル」（#1282） | OK（「なってる」） |
  | 2 | 閉じると窓がすぐ消える（#1340） | OK（「押したらすぐ消える」） |
  | 3 | 変換中に × で閉じて、次の起動で復元を聞かれない（#1340） | OK（「聞かれない」） |
  | 4 | Markdown で Ctrl+Shift+M を 3 回（#1354・D77） | OK（「3 モード巡回する」） |

  2 は最初「ウィンドウの × は『変更が保存されていません』のポップアップが出る」との報告で、未保存の変更があるときの正しい動き（箱の × はキャンセル）だった。見てほしいのは未保存が無い状態（「保存しない」の直後）の窓の消え方だと伝え、OK。1 回目に閉じたとき箱が出なかったのは、本文を変えていなかったため（開いただけなら聞かずに閉じる）。
- **まとめ枝 14**（Issue #1379・`D:/NeNeNib/worktrees/1379-integration`）: #1365（`40d34191`）・#1372（`8e6b1129`）・#1373（`1e488096`）・#1377（`c6ac9bd7`）・#1378（`421fe36e`）・#1348（`a01d7260`）の 6 commit。#1373 は oracle の `literal` に `\f` を足すので、枝の commit を `--reuse-ref` にして main 側だけの 82 件（`search-operator-empty-` 29・`dot-register-count-` 43・`window-path-1326-` 10）を測った。#1348 は #1355（main）と同じ行（`.` の再生の `VimReplay` の構築）を触っていて、両方を残した（#1355 の `register_prefixed` と #1348 の `single_step`・`resolve-1348.py`）。`--vim-macro` の件数は 164 + 6 = 170。fixture 3906 件。検証中（hide のほかの席が動いているので時間がかかる・速さは測らない）。

## 20. 夜: まとめ枝 14 が main・hide の要望 5 つ・区切り（21:00〜23:45）

### 20.1 まとめ枝 14

- 検証（`verify-integration.sh`・`D:/NeNeNib/outputs/20261110/verify-1379.log`）: Debug 全 target 警告 0（551 s）・ctest 18 本 Passed（818 s）・symbols 0・conformance 0・test-conformance OK・format・validate-git（6 commit）・Release 警告 0。
- PR #1382 → **main `04a45f4c`**（CI run 42816233486・6 commit: #1365 `b1450441`・#1372 `e08d9e7d`・#1373 `99ca954c`・#1377・#1378・#1348）。元の PR 6 本は道具が閉じた。今日 10 回目のまとめ枝で、main は 34 commit 進んだ（`5f03f3a0` → `04a45f4c`）。fixture は 3906 件。
- **2 本目の試用の exe**: `D:/NeNeNib/evidence/20261110-trial/NeNeNib-main-04a45f4c.exe`（まとめ枝 14 の木の Release・main と木が同じ `31c5ed7c`・SHA-256 `40002479e255b535bf3b377c84406dd8d700e3894f49b9557b6172fc3cde1b2c`）。README に D 節（20〜23）を足した。hide は D 節をまだ試していない。

### 20.2 hide の要望と決定（夜・番号は設計席が振った）

| 番号 | hide の言葉 | 決め | Issue / PR |
|---|---|---|---|
| D78 | 「左のファイルエクスプローラなんだけど、ドラッグで幅を変更できるようにしたい」 | 境（左右 3 DIP）をドラッグ・最小 120 DIP・最大は client の 60% と本文 200 DIP 残しの小さい方・ダブルクリックで既定・`settings.v4` の鍵 `explorer_width`（離したときだけ書く・壊れた値は既定として読む = この 1 鍵だけ寛容） | #1380 / PR #1384 |
| D79 | 「MD ビューアーモードの時、画像を表示したい。NeNeFolio でもやったので参考になるかも」→ 推し 4 つに「全部推しで」 | ローカルのファイルだけ（http / https / data は取りに行かない）・PNG / JPEG / GIF（1 コマ目）/ BMP・縮めるだけ（配置の上限は復号した大きさ）・上限は Folio と同じ（16 MiB・64M 画素・長辺 2048・256 枚・保持 32 MiB）・裏のワーカーで読む・3 段 | #1381（親）・段 1 #1383 / PR #1385 |
| D80 | 「Cursor とかはダブルクリックじゃなくてシングルクリックで開くよね。その方がなれてる」 | ファイルはクリック 1 回で開き焦点は面に残す・ダブルクリック / Enter は焦点を本文へ・フォルダはクリックで開閉。プレビューのタブは入れない（試用で決める）。D66 の「クリックは選ぶだけ」を改める | #1388 / PR #1391 |
| D81 | 「/design つかってファイルエクスプローラのデザインを cursor や vscode と同等かそれ以上の見た目にしたい」→ モック 3 案 → 「推しの B」 | 案 B「色の点」: 面の地は `title_bar`（帯と L 字）・本文の左上の角を 8 DIP 丸める・UI の字体・行 22 DIP・丸い行の面・載せた行・幾何の山形とフォルダの絵・ファイルは構文の色のトークンの点・未保存は `accent` の丸。新しい色のトークンは足さない | #1390（席は未着手）・モック `D:/NeNeNib/evidence/20261110-explorer-look/explorer-look.html` |
| D82 | 「vim じゃないモードは windows の標準に合わせて」 | 通常モードのキャレット・選択・鍵は Windows の標準（メモ帳・ブラウザの入力欄）へ聞かずに寄せる。Vim のモードは oracle のまま | #1386 / PR #1392・#1393・#1394・#1395 |
| — | 「タグ切ってリリースできる区切りがあったらリリースしてほしい」 | tag と pre-release も設計席が出してよい（これまでは hide の手）。区切り = まとめ枝が main・速さのゲート・試用で止まる物が無い | beta.9（翌日） |

`/design` は plugin の `design:design-system`（extend）を使った。モックは色を `core::Palette` と `SyntaxPalette` の値のまま写した静的な HTML で、設計席は描画を目で確かめていない（hide がブラウザで見て選んだ）。

### 20.3 夜の席（実装 → 独立レビュー → 直し → 狭い再レビュー）

| PR | 中身 | レビュー | 状態（23:45） |
|---|---|---|---|
| #1384（#1380・D78） | 幅のドラッグ。core `ExplorerWidth` `explorer_panel_width` `explorer_width_clamped` `explorer_divider_hit`・意図 `ResizeExplorer`・`dispatch` のマウス 10 通を表 `pointer_messages` に | V-1384 受理（必須 0・推奨 6）→ 直し I31b（入口で [120, 9999] に収める・`delivery()` を作らない・ドラッグ中に面が閉じたら捨てる・試験 T-1〜T-5） | HEAD `419dfd96`・**hide の試用 OK**（枝の exe `NeNeNib-pr1384-419dfd96.exe`）・後続 #1387 |
| #1385（#1383・D79 段 1） | `MarkdownImage`・`resolve_image_reference`・`image_format_of`・`ImageLimits.hpp`・`image_placement`・`ImagePort`（2 口）・`ImagePixels`・口だけの `Win32ImageAdapter`。製品の振る舞いは不変 | V-1385「直してから」（M1 再解析のたびの頼み直し・M2 見えている画像が捨てられる）→ 直し I32b（錨に近い順に 1 枚ずつ・遠いものから捨てる）→ V-1385b 受理 | HEAD `2a59b62d`・後続 #1389。**段 2 の席に入れる**: 範囲の中の evicted も近ければ頼む（候補 B）・「要求ごとに結果を必ず 1 つ」の契約・試験 2 つ |
| #1391（#1388・D80） | クリック 1 回で開く。`explorer_clicked_`・`second_click_explorer`・`explorer_pointing_of` | V-1391 必須 1（2 打目が行を引き直して別の行に当たる）→ 直し I34b → V-1391b 受理 | HEAD `6ae8910c`（#1384 の上）。推奨 R-a（間にホイール）・R-b（開けない知らせが 2 打目で消える）は #1390 の席に |
| #1392（#1386・D82） | 端の行の ↑ ↓・PgUp / PgDn。`ShiftedPosition{at, stopped}`・`VisualTextView::reached`・`move_to_text_edge` | V-1392 受理（必須 0・Vim の呼び手 7 つは不変） → 直し I33b（試験の追加・もう端で画面が離れていたら送り返す） | HEAD `0888c0f8`（I33b: `move_to_text_edge` が `follow_caret()` を呼ぶ・`--edge-arrows` 44 → 65 checks・既定実行 2,349,137 pass） |

独立レビューが merge の前に止めた物: #1385 の 2 件（probe で再現）と #1391 の 1 件（読みで確定・直しの席が契約にした）。

### 20.4 通常モードと Windows の標準の棚卸し（席 S3・読むだけ）

`D:/NeNeNib/outputs/20261110/survey-normal-mode-windows-standard.md`（main `04a45f4c`）。違うか無い項目 28（高 8・中 7・低 13）。高: 端の行（#1386）・選択があるときの ← → ↑ ↓（#1393）・Ctrl+← → の語の境が空白だけ / Ctrl+Backspace / Ctrl+Delete が無い（#1394）・Ctrl+N が無い（#1395 に Shift+Insert・Ctrl+Insert・Shift+Delete・Ctrl+Shift+Z の redo と束ねた）・右クリックのメニュー（#1051）。表の「Windows の標準」の一部は席の知識で、実測ではない（#1394 は実装の前に実測させる）。

### 20.5 測らなかったもの・やらなかったもの

- **速さ**: まとめ枝 11 以降の main（`c67cf493`〜`04a45f4c`）の 8 本のゲートは**未測定**。ミニマップ #1366 の A/B も未測定のまま（20:24 の測定は無効・§18.1）。hide が 20 時ごろからほかの席（NeNeFolio）を動かしていて、その後は設計席の席がビルドを続けたので、静かな機械にならなかった。hide の指示「（最後の席が）終わったら今日は上がろう」で、計測は翌日に回した。
- まとめ枝 15（#1384 #1391 #1392 #1385）は積んでいない。beta.9 も出していない。
- 見た目の案 B（#1390）・画像の段 2・#1393〜#1395・D75 の上限（#1351）の席は出していない。
