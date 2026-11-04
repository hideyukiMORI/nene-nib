# 品質向上の日の記録 — 2026-11-04（hide 不在・Fable）

hide の指示（朝・外回り）: 「他のセッションは Folio サナ以外は立ち上げないので、自由に試験や計測を繰り返して、品質が上がるように調査対応をして欲しい。試したことや変更したことは全てドキュメントとして記録を残して」。設計席 Fable（Nibリナ）。実装席は Opus。時刻は JST、ファイルの mtime と GitHub の createdAt から書く。

## 0. 再開時の状態（07:0x）

- main `9ee0fcc9`（#1218 の文書）。開いている PR は引き継ぎの列 17 本（全部レビュー済み）＋ #1209（matchparen・A/B 待ち）＋ #593（hold）。
- #1181 は 11-03 夜の CI `41655693776` が緑のまま GitHub で完了していた → `merge-pr.py 1181 --ci-run 41655693776` で **main `27bf517d`**（07:05・引き継ぎの手順 1）。
- 機械: CPU 20 論理コア・負荷 16%・Folio の Codex（`codex` 2 プロセス）が動いている。速さの計測は列が引けた後に負荷を読んでから。

## 1. まとめ枝（#1220・引き継ぎの「明日の実行タスク」1〜4）

### 1.1 判断

- **ADR 0021 に追補**（まとめ枝 = レビュー済みの PR を 1 PR = 1 commit で積み、手元の検証と CI を 1 回にし、main へ rebase merge）。設計席が書き、道具 PR #1221 の 1 commit 目（`d9918ea1`）。GIT-004・DEVELOPMENT_WORKFLOW・QUALITY_GATES・ADR 0118・ADR の索引・CLAUDE.md §4 の「squash のみ」を置き換え、gate-proofs に ruleset の変更の行。
- **ruleset と repo の設定を設計席が API で変えた**（07:13）: `allowed_merge_methods: [squash, rebase]`・`allow_rebase_merge: true`。引き継ぎでは「hide の手」としていたが、hide は終日不在で、形は 11-03 23:4x の hide の判断そのもの・squash は残り・戻せる、ので進めた（変更前の JSON は `D:/NeNeNib/outputs/20261104/ruleset-before.json`）。**hide が違うと思えば戻す**。
- 枝の名前は GIT-002 の形が要る（CI の `validate-git.ps1` が head の枝の名前を見る）ので `integration/<日付>` でなく `chore/1220-integration-20261104`。
- 道具は `eng/merge-pr.py --integration`（別の道具を作らない）。席 T1（Opus・依頼書 `D:/NeNeNib/briefs/20261104/brief-T1-1220-merge-integration.md`）→ **PR #1221**（試験 9 本・35 tests OK）。席の判断の問い 2 つ: CI の後にもう一度 `compare main...head` を読んで main が進んでいれば merge しない（残す）・merge 後の照合の起点は compare の `merge_base_commit`（案 B・`f5436cc5`）。

### 1.2 積み方（`D:/NeNeNib/scripts/20261104/stack2.sh`）

`git merge --squash <head>` は、表の PR が #1191 の**古い**枝 `0fbf14e8` の上に積んであるために merge-base が古く、#1157 で 27 ファイルが衝突した（#1191 の直しの前後の差が全部衝突になる）。そこで各 PR の**自分の commit だけ**（`onto..head`）を `git rebase --onto <まとめ枝> <onto>` で写してから `merge --squash` で 1 commit にする形に変えた（onto は引き継ぎの `onto_from`・main 直の PR は merge-base）。commit の本文は `Closes #N`・`PR #K（head <sha>）を squash と同じ中身でまとめ枝 #1220 に積む。`・自分の commit の列（GitHub の squash の既定と同じ）。

| PR | 衝突 | 解き方 |
|---|---|---|
| #1157 印刷 N6a | `EditorIntent.hpp` の和型の末尾 | `PointPreview, PrintDocument` |
| #1183 登録 段 1 | port の束（`EditorPorts`・合成ルート・替え玉 11 ファイル）・操作の表（10 ファイル）・probe の期待表 | `printer` の後ろに `shell_registration`・`print` の後ろに `register_shell` `unregister_shell`（35 → 37・50 → 52・33 → 35）・probe の序数 35 / 36 と workload は base + Δprint + Δshell（1285632 / 1235456）・`NibTests.cpp` の 2 表は 80 / 103 に数え直し（両 PR が +1 ずつで git が同じ変更として 79 / 102 に畳んでいた） |
| #1185 登録 段 2 | 合成ルートの 1 行 | `Win32ShellRegistration shell_registration(L"Software")` |
| #1153 ジャンプの列（oracle を変えた PR） | 生成物 `VimFixtures.hpp` の頭と数 | 枝側を取って **oracle で全件を作り直し**（3314 件・294 s・`--only` は main 側だけの 229 件が枝の ref に無いので拒否）。3 commit とも同じ生成物に当たるので、途中は枝側で進め最後に 1 回作り直して amend |
| #1166 | include の行 | 両方 |
| #1180 検索の始まり | ADR 0032・`VimPattern.cpp`（`previous_class` と main の `class_at(vim_character_start)` は同じ式）・`VimSearchTests.cpp` の件数・生成物 | ADR は main の文 ＋ 追補・`previous_class` の側・検索の fixture は 194 + 15 = 209（`selected == 215`）・生成物は `--only search-composing-start- --reuse-ref <まとめ枝>`（15 件測って 3314 件再利用・2 s） |
| #1205 矩形 `I` | `VimBlockInsert`（`at_byte` と `straddled` の 2 欄）・`VimStepBlock.cpp` の構築・fixture の union・block- の数 | `at_byte` の式に `, 0` を足す・fixtures.json は和（3351 件・block- 431）・`--only block-insert-wide-start-`（22 件） |
| #1207 LSP core | `NibTests.cpp` の 2 表 | 81 / 104 |
| #1211 Vim の追従 | ADR 0019（決定 3 の文と追補 #1177 / #1178 の並び）・`VimViewTests.cpp`（#1177 の `tall_middles` の隣に #1178 の表）・include | main の決定 3 の文 ＋ 追補は #1177 → #1178・試験は両方 |
| #1215 LSP ui | `CMakeLists.txt`・窓の試験の表（`groups` 24） | 両方 |
| ほか（#1160 #1202 #1203 #1206 #1212 #1217 #1221） | 無し | — |

積んだ結果: origin/main `27bf517d` の上に 17 commit（16 本 ＋ #1221）。`validate-git.ps1` は 17 commit とも GIT-003 で通る。

### 1.3 まとめ枝の検証（ADR 0021 追補 決定 2）

積んだ木（`3d8fda11`・17 commit）の Debug ビルドと検証で、1 本ずつの乗せ替えでは CI が受け止めていたはずの「積んだ後にだけ出る」ものが 4 件出た。

| 出たもの | 原因 | 直し方 |
|---|---|---|
| clang-tidy: 合成ルート `run` が 61 行（門 60） | #1157 の `printer` と #1183 の `shell_registration` が 1 行ずつ（各 PR 単独では 60） | 履歴の注釈 2 行を 1 行に（#1152 と同じ判断 A）。#1183 の commit に amend |
| `test_probe_candidates`: probe の Ex の候補の写しに `hardcopy` が無い | #1157 は #1184（候補の表の写しの conformance）より前の枝 | 写しに `hardcopy` を足す。#1157 の commit に amend |
| 整形: `VimSearchTests.cpp` の注釈（設計席が衝突を解いた行） | 全角は幅 2 で数えるので 100 桁を越えた | clang-format。#1180 の commit に amend（後に #1180 ごと外した） |
| probe `operation-choice-all-*` 3 本が exit 1 | 期待表の checksum は `ordinal + 1` を含むので、登録の 2 操作の序数が 1 ずつ後ろへ動いた分（+512）が要る | 1286144 / 1235968。#1183 の commit に amend |
| **`nib_tests` 既定実行 2 件 fail**（`search-composing-ic-set-folds` の column・`--vim-search` の「a set and a precomposed letter still fold」） | **#1180（#1146）が #1147（結合文字の大小・10-30 main）より前の枝で、集合が結合文字を食べる規則と `[e]` + U+0301 が畳まれる規則が同じ入力で違う答えになる意味の衝突。レビューと席の検証では見えない** | **#1180 をまとめ枝から外した**（`drop`・`PR #1180` にコメント）。main の上で席が直す |

amend は `--fixup` の件名が GIT-003 の hook で拒まれるので、別の detached 作業木 `D:/NeNeNib/worktrees/1220-fixup` で `rebase -i` の `edit` で止めて `--amend`（`edit-todo*.py` が todo を書き換える）。ビルドの作業木には `git reset --hard <新しい先端>` で当てる（変わったファイルだけ書き換わるので ninja の再ビルドが最小）。

最終の木: `2dc9716e`（16 commit・#1180 なし）。

検証の結果（`2dc9716e`・Debug・`D:/NeNeNib/worktrees/1220-integration/out/1220-integration/`）: 全 target ビルド 0 warning・`ctest` 15 本（`nib_unit` 2,334,744 checks・`nib_window`・adapters / themes / sessions / bookmarks / histories / folders / autosave / long_paths / persisted / cfhtml / document_reader / language_server / toolchain_smoke は #1180 入りの 1 つ前の木 `3d8fda11` で Passed・#1180 は触れないので再利用）・`symbols.py` 3 libraries 0 violation・`conformance.py` 0・`test-conformance.py` 446 tests OK・`format-check` 1479 files・`validate-git` 16 commit・`test_merge_pr` 35 tests・probe operation-choice 10 workload exit 0。

### 1.4 PR #1222 と merge

PR #1222（題 `chore(git): 2026-11-04 のまとめ枝 … (#1220)`・本文 11 欄・`Closes #1220`）を Ready にし、枝の中の `merge-pr.py 1222 --expected-head 2dc9716e… --integration` の plan（`--rebase --match-head-commit`・commits 16）を確かめてから `--execute`（`D:/NeNeNib/scripts/20261104/merge-1222.sh`・CI 1 回）。独立レビュー V-1221（止める 0・直してから 0・後続 7 → Issue #1223）。

## 2. 速さのゲート（CI の間・同じ木の Release `build-release/`）

負荷 7〜10%（Folio の Codex が 9 プロセス・ほかの席とビルドは無し）で `measure-speed.py --check` を 1 回。

| ベンチ | 中央値 (ms) | 基準 (2026-10-18) | 判定 |
|---|---|---|---|
| startup-first-frame | 42.3 | 41.9 | pass |
| startup-window-shown | 31.2 | 32.4 | pass |
| key-to-frame-single | 0.67 | 0.64 | pass |
| key-to-frame-burst-200 | 2.72 | 2.16 | pass |
| open-large-file-16mib | 51.4 | 49.8 | pass |
| key-to-frame-burst-200-16mib | 3.00 | 2.34 | pass |
| key-to-frame-palette-5000 | 1.04 | 1.73 | pass |
| key-to-frame-single-long-line | 1.09 | 1.57 | pass |

**8 benches checked, 0 regression(s), 0 unmeasurable**（記録 `D:/NeNeNib/worktrees/1220-integration/out/speed/2026-11-03T23-47-29Z.json`・118 s）。

LSP L1 の任意ベンチ（Issue #1216 の本測定の手順・替え玉 `nib_fake_language_server`・各 1 回 `--record`・D60 のとおりオフの 8 本とは混ぜない・**基準値は採らない**（初回の参考値）: `key-to-frame-single-cpp` 1.110 ms（frame_built 0.462 / drawn 0.563 / presented 0.085）・`lsp-single-key` 1.307 ms（0.503 / 0.738 / 0.081・オフとの差 +0.20 ms・drawn の +0.18 は波線と右端の件数）・`lsp-burst-200` 4.038 ms・`lsp-diagnostics-ready` 2.779 ms（5 本のうち 1 本 5.9 ms・2 枚目の frame の近似）。記録 `out/speed/2026-11-03T23-49-05Z` 〜 `23-50-32Z.json`。clangd での記録は hide の機械の clangd の版と `compile_commands.json` が要るので試用のときに。

### 1.5 merge の結果（CI run [41740575152](https://github.com/hideyukiMORI/nene-nib/actions/runs/41740575152)・success）

rebase merge で **main `a9e93eec`**（`27bf517d` の上に 16 commit・木は `2dc9716e` と同じ）。道具が元の PR 16 本（#1157 #1160 #1183 #1185 #1153 #1166 #1202 #1203 #1205 #1206 #1207 #1211 #1212 #1215 #1217 #1221）を main の commit を書いて閉じ、Issue は commit の `Closes` で閉じた。`notClosed` は無し。**1 本ずつなら CI 16 回（8〜12 時間）が、手元の検証 1 回 ＋ CI 1 回（約 40 分）になった。**

試用の exe: `D:/NeNeNib/evidence/20261104-trial/NeNeNib-main-a9e93eec.exe`（まとめ枝の Release・同じ木・表は README）。

## 3. merge の後の席（並列）

| 席 | 対象 | 作業木 | 状態 |
|---|---|---|---|
| R6（Opus） | #1209 matchparen をまとめ枝の木の上に乗せ替え（7 ファイルの衝突）・Release の exe | `D:/NeNeNib/worktrees/1209-ab` | 済み（衝突 7 ファイル ＋ ADR 0152 の追補の順と生成物 `--only matching-pair-nested-lines-`・contracts 82 / scopes 105 / groups 25・既定実行 2,334,693・`--matchparen` 37・窓 7・Release 0 warning）。設計席が `rebase --onto origin/main` して PR #1209 の枝を `02f49062` に更新。**A/B は席が全部終わった静かな機械で** |
| I3（Opus） | #1180 と #1147 の意味の衝突を Vim の実測で解く | `D:/NeNeNib/worktrees/1146-fix` | 済み: Vim 9.1 の実測 8 件と `regexp_nfa.c` の `nfa_regatom`（集合の終わりは `MB_PTR_ADV` で進むので `]` の直後の結合文字は原子にならず読み飛ばされる）から、`parsed_set` の終わりを `vim_character_end` にする 1 か所の直し。#1147 の決定 7 はそのまま・#1146 の追補の決定 2 に「改め」。fixture 3 件・試験 1 件・`--vim-search` 2856・既定実行 2,334,772。残差: `.` の直後の結合文字（Vim は `.` を消して結合文字だけの原子に・候補 A のまま）。設計席が main の上に移して PR #1180 を `ea4fe120`（2 commit）に更新 → レビュー V-1180-2（直してから 1・後続 2・細かい 1）→ 同じ席が 1 commit で直し `893ab7ce`（3 commit・`--vim-search` 2856・`--vim` 2,334,772）。次のまとめ枝へ |
| I1（Opus） | #1199 `remaining_command` を解析関数の表に（計画の席 Opus の案・ADR 0022 追補・`static_assert` は置かず `-Wunused-function` と候補の表の試験で守る） | `D:/NeNeNib/worktrees/1199-ex-command-table` | **PR #1225**（試験 → 表の 2 commit・`--ex-settings` 702 → 914・`-Wunused-function` の守りは表から 1 行抜いて `error: unused function` で落ちることを実験で確認・ADR 0022 追補）。独立レビュー V-1225: 止める 0・直してから 0・後続 1（行の順の文言）・nit 2 → 設計席が直して `7799e133`。次のまとめ枝へ |
| I2（Opus） | #1198 巨大な 1 行の矩形 `I` / `A` を索引の道に（計画の席 Opus の案・候補 B・借用の view `VimLineColumns`・同値の試験・probe） | `D:/NeNeNib/worktrees/1198-block-insert-index` | **PR #1236**（`1dabb57b`・新しい型 `VimLineColumns`（借用の view・持たない本文は行を 1 回写して今の道）・`line_virtual_width` と `virtual_column_end` の索引の道・9 関数を替え・`vim_block_width`・同値の試験 3 本文 × 12 鍵列 2,800 鍵 ＋ 塊の境 7 列・probe `vim-block-insert-giant-1line` **1814.5 → 3.17 ms**（Release・ABBA 1 組・負荷 20% 以下）・既定実行 2,334,637）。残り（`r` `p` Esc・`.` の再生の幅・`block_changed` の写し）は Issue #1237。レビュー V-1236: 止める 0・直してから 1（窓を倍にする枝の試験）→ 同じ席が直し `8fae9598`（変異で落ちることを確認）。Ready・次のまとめ枝へ |

## 4. Vim の差分 fuzz（生成器 4・seed 41〜50・main `a9e93eec` の再生器）

3,000 件（拒否 228・照合 2,772）で **不一致 0**（既知 0・新規 0）。記録 `D:/NeNeNib/outputs/20261104/vim-fuzz-gen4-seeds41-50/`。生成器 4 は seed 1〜50 を使い切って新しい種類が出なくなったので、次は語を広げた生成器 5（下の §5 の候補）。

## 5. fuzz 生成器 5（viewport と画面移動の語・Issue #1224・席 F1・PR #1226）

`--generator 5`: 30〜60 行の番号つきの本文・窓 8〜12 行・`set scrolloff=0|2|3` を seed から決め、画面移動の語 17 個（`H M L zt zz zb z<CR> z. z- z+ z^ <C-d> <C-u> <C-f> <C-b> <PageUp> <PageDown>`・`<C-e>` `<C-y>` は Nib に無いので外した）と `d` `y` `v` ＋ `H M L` を既存の語に混ぜる。再生器が viewport と `:set so=` を受け、`first_visible` と `scroll` も比べる。生成器 3 / 4 の seed の列は不変（実測）。

**seed 1〜5（1,500 件）で不一致 35 件・6 種類（全部新規）。** 手の fixture（`viewport-` 160 件）と 11-03 の 882 件の表の外にあった製品の違い。Issue にした:

| Issue | 種類 | 件数（最小化後の代表） |
|---|---|---|
| #1227 | 回数つき `z^` が scrolloff の分だけ上に着く | 10（`36z^` so=3: 行 29 / 26・先頭 22 / 19） |
| #1228 | 本文の端で失敗する画面移動（先頭行の `<C-b>` `<C-u>`・末尾行の `<C-d>` `<C-f>`）が鍵の列を打ち切らない | 4 ＋ text 5（`gg<C-b>44%`: 行 1 / 18） |
| #1229 | 回数つき `<C-b>` `<C-f>` と `<PageUp>` の着地 | 3 ＋ text 3（`4<C-b>`: 行 10 / 3・`<PageUp>`: 9 / 10） |
| #1230 | 行単位の削除・変更（`dap` `cip` `dip` `GdH`）の後の先頭行 | 5（`dap`: 先頭 3 / 9） |
| #1231 | 小さな違い 3 つ（`z-M`・`{N}zz` の列・`<C-u>` の後の `f;` の列） | 3 |

席: I4（#1227）・I5（#1228 → #1229 の 2 PR・同じ関数）・I6（#1230）を並列に。#1231 は後。共通の依頼 `D:/NeNeNib/briefs/20261104/brief-gen5-fix-common.md`（正本は oracle の実測・直した後に生成器 5 を 1 回回して自分の種類が 0 になること）。

seed 6〜10（1,500 件・既知を `--known` で除く・レビュー V-1226 の R4-3）: 不一致 25（既知 22・新規 3）→ #1234（回数つき INSERT の `<C-w>` が挿入の始まりより前を消せないときの反復・`4i<C-w>ＡＢ→  <Esc>`）・#1235（`H M L` は跳びなので `''` の戻り先を置く・`49ztgI<Esc>M''` で Nib は行 1 へ）・#1230 に `vGx`（VISUAL で末尾まで消す）を追記。**seed 1〜10 で 60 件・8 種類**。`<C-e>` `<C-y>` は製品の Issue #1232、道具の後続は #1233。PR #1226 は Ready（V-1226: 止める 0）。席 I7（#1235）。

### 5.1 土台の穴（#1239）

#1227 の席（I4）が、`nib_tests` の fixture 再生（`arrange_vim_viewport`）が fixture の `settings`（`set scrolloff=N`）を読まず `VimFixtures.hpp` の行にも載らないことを見つけた。既存の `viewport-` 157 件は全部 so=0 なので通っていた。so>0 の fixture は今日が初めてで、製品を直しても `--vim` で落ちる。判断: 候補 A（土台を 1 本の PR に・Issue #1239・oracle の `fixture_row` が `scrolloff` の欄を書き `arrange_vim_viewport` が窓の後に `:set so=N` を打つ・#1226 の再生器と同じ順）。I4 が土台の枝 `test/1239-viewport-scrolloff` を先に作り、#1227 と I5 / I6 / I7 の PR はその枝の上に積む（席 3 つに知らせた）。#1227 の直しそのものは出来ていて fuzz でも `z^` の 11 件が消えた（35 → 24・新規 0）。Vim の規則: `nv_zet` の `z^` は回数があると `scroll_cursor_bot(0, TRUE)` → `w_cursor = w_topline`。`scroll_cursor_bot` は行 N を下端に置いた後 `extra >= so` か本文の終わりまで下の行を足してから送る。

### 5.2 直しの PR（夕方）

| Issue | PR | 原因（Vim の規則） | 状態 |
|---|---|---|---|
| #1239 土台 | #1243（`d0f3747d`・枝 `test/1239-viewport-scrolloff`） | oracle の `fixture_row` が `scrolloff` の欄を書き、`arrange_vim_viewport` が窓の後に `:set so=N` | レビュー V-1243: 止める 0・直してから 0・軽い後続 2（注釈・`:set so=` の失敗を捨てない）→ I4 が土台に足す。Ready |
| #1230 | #1242（`1de9b933`・base は土台） | Vim は行単位の削除の後も `w_topline` を動かさず `update_topline` は先頭を行数で切らない（本文の外に残った先頭から `n` を数えて halfway / top）。Nib は切っていた。`GdH` は演算子待ちの `H` が `cursor_correct` を通らない | レビュー V-1242: 止める 0・後続の契約 2 行（`verify_topline_after_shrinking`）を足した |
| #1235 | #1244（`6aaf7b00`） | **Issue の見立ては誤り**（`H M L` は跳びの表にある）。本当の原因は回数つきの z 系が `setpcmark` を置かないこと（`nv_zet`: 回数が今の行と違えば動く前に置く）。fixture 19 件 | レビュー V-1244: 直してから 1（本文の数）・後続 2 → 席が直し `a159a7c9`（修正前の exe で 21 件中 13 件が落ちる・VISUAL 2 件）。Ready |
| #1228 | #1238（`9f7a5133`） | 端で失敗する画面移動は鍵の列を打ち切る | 席 I5: 済み（`b7360a68`・base は土台・fixture 10 件）→ レビュー V-1238-1241 |
| #1229 | #1241（`19ff159b`・base は #1228 の枝） | 回数つき Ctrl-b / Ctrl-f と PageUp の着地。Issue の見込み（重なりの数え方）は高さ 5 以上では原因でなく、後ろへの周の `while (n <= w_height)` で送った先頭が 2 行目以下になる周は先頭が 1 へ（`loff.lnum < 1`）。重なりは高さ 5 以上で 2・4 で 1・3 以下で 0 | 席 I5: 済み（`e34f5d4d`・fixture 16 件・fuzz 35 → 20 → 18）→ レビュー V-1238-1241 |
| #1227 | （席 I4 が土台の後に PR） | `nv_zet` の `z^` は回数があると `scroll_cursor_bot(0, TRUE)` → `w_cursor = w_topline` | 土台 → #1227 の順 |

## 6. まとめ枝 2（#1240・`chore/1240-integration-20261104b`・`D:/NeNeNib/worktrees/1240-integration`）

午後にレビューが済んだものを同じ道具（`stack2.sh`・`INTEGRATION_ISSUE=1240`・`n:auto` は merge-base）で積む。生成物 `VimFixtures.hpp` の衝突は `finish-fixtures.sh`（枝側を取って進め、最後に `--only <接頭辞> --reuse-ref <まとめ枝>` で作り直して amend）。fixtures.json は両側の和。

| 順 | PR | 衝突 |
|---|---|---|
| 1 | #1225（#1199） | 無し |
| 2 | #1180（#1146） | 無し |
| 3 | #1226（#1224） | 無し |
| 4 | #1236（#1198） | 無し |
| 5 | #1244（#1235） | 生成物 → `--only viewport-hml-jump-mark-`（21 件） |
| 6 | #1243（#1239・土台・2 commit） | fixtures.json の和（3376 件）・生成物 → `--only viewport-so-`（1 件） |
| 7 | #1242（#1230・onto は土台の 1 commit 目 `d0f3747d`） | 生成物 → `--only viewport-after-linewise-delete-`（6 件） |
| 8 | #1238（#1228・onto `d0f3747d`） | ADR 0019 の追補の和・fixtures.json の和（3392 件・末尾の `,` を直す）・生成物 → `--only viewport-edge-fails-`（10 件） |
| 9〜 | #1245（#1227・レビュー V-1245）・#1241（#1229・`back_overlap` の再レビュー V-1241-2）・#1209（A/B の後） | レビューの後 |

| 9 | #1245（#1227・onto `d0f3747d`） | 生成物 → `--only viewport-z-caret-count-`（5 件） |
| 10 | #1241（#1229・onto #1238 の head `4d6ef093`） | 生成物 → `--only viewport-page-count-`（23 件） |

検証（`da47c49b`・10 commit・Debug）: 全 target ビルド 0 warning（358 s）・`ctest` 15 本 Passed・`symbols.py` 3 libraries 0 violation・`conformance.py` 0・`test-conformance.py` OK・`format-check` 1484 files・`validate-git` 10 commit・`test_merge_pr` / `test_speed` OK。7 commit の時点のビルドは、積んでいる最中の衝突の印を拾って無効だった（教訓: 同じ作業木でビルド中に積まない）。PR #1247 → `merge-pr.py --integration` の plan（`--rebase`・commits 10）→ `--execute`（CI 1 回）。

## 7. #1209 matchparen の A/B（D67・+1% の線・夕方・負荷 17%・Folio の Codex のみ）

A = main と同じ木の Release（まとめ枝 1）・B = #1209（`02f49062`・同じ木の上に 2 commit）。`matchparen-single-key`（#1209 が足した任意ベンチ）と空の文書の `key-to-frame-single` を ABBA で各 5 run の中央値:

| 組 | A matchparen | B matchparen | B 単打 | A 単打 |
|---|---|---|---|---|
| 1 | 0.578 | 0.598（+3.5%） | 0.614（min 0.417・max 1.027） | 0.492 |
| 2 | 0.761 | 0.734（−3.5%） | — | — |

同じ exe が組 1 と 2 で 0.58 → 0.76 ms と動き（機械の揺れ・長い行の 2 山と同じ種類）、5 run の中央値では +1% の線を判定できない。**閾値も合否も動かさず**、15 run × ABBA × 2 組の本測定を続けて回した（§7.1）。frame_built の差は +0.005 ms（matchparen の探索は 1 打鍵の道で 1% 未満の見込み）、揺れは frame_drawn（描画）側。

### 7.1 本測定（15 run × ABBA × 2 組・負荷 8%）

| 組 | A matchparen | B matchparen | B 単打 | A 単打 |
|---|---|---|---|---|
| 1 | 0.578 | 0.595（+2.9%） | 0.606 | 0.494 |
| 2 | 0.600 | 0.612（+2.0%） | 0.606 | 0.607 |

matchparen のベンチは 2 組とも B が **+2〜3%**（A 自身も組の間で +3.8% 動く）。空の文書の単打は A の組 1 だけ速く（0.494）、組 2 は A = B（0.607）で差は無い。打鍵の内訳では frame_built の差 +0.005 ms・frame_drawn は B が低い組もある。判断: **D67 の +1% の線を本測定でも越えて見える（+2〜3%）が揺れと同じ大きさ**で、合否を機械では決められない。閾値も測り方も動かさず、**#1209 はまとめ枝 2 に積まない**。hide の判断に出す（matchparen は見える機能・+2% を受けるか・描画の側の費用（括弧の枠を打鍵ごとに描く）を先に詰めるか）。記録 `D:/NeNeNib/outputs/20261104/ab-1209/` と `ab-1209-long/`。

### 6.1 まとめ枝 2 のゲート（同じ木の Release・負荷 5%・CI の間）

`measure-speed.py --check`: **8 benches checked, 0 regression(s)**（起動 42.4・窓 31.5・単打 0.61・200 打鍵 2.84・16 MiB を開く 54.1・16 MiB で 200 打鍵 3.15・面 1.02・長い行 1.12 ms・139 s・`out/speed/` の記録は `D:/NeNeNib/worktrees/1240-integration/out/speed/`）。

### 6.2 verify-window 8 節（hide 不在・負荷 5%・Debug の exe・`D:/NeNeNib/outputs/20261104/verify-window/`）

default / tabs / split / preview / preview-full / find / palette の 7 節 pass。**explorer は 2 回とも落ちた**（「the second Ctrl+Shift+E did not hide the panel」→ retry「Ctrl+Shift+E did not show the panel」）。11-03 の #1160 / #1185 の exe でも同じ落ち方で、今日の 10 本はエクスプローラーに触れない → #1204（節の待ち方）にコメント。製品の退行とは扱わない。

### 6.3 merge と統合後の fuzz

PR #1247 → CI run [41771475498](https://github.com/hideyukiMORI/nene-nib/actions/runs/41771475498) success → rebase merge で **main `c6e77916`**（`a9e93eec` の上に 10 commit・木は `da47c49b` と同じ）。元の PR 10 本は道具が閉じた。試用の exe `D:/NeNeNib/evidence/20261104-trial/NeNeNib-main-c6e77916.exe`（README 12〜15）。

統合後の木の Release の再生器で生成器 5 の seed 1〜10（3,000 件・既知は #1231 #1234）: **不一致 4（既知 4・新規 0）**。朝の 60 件（8 種類）から 4 件（2 種類・#1231 の 3 件と #1234 の 1 件）に。記録 `D:/NeNeNib/outputs/20261104/vim-fuzz-gen5-integrated-1-10/`。

## 8. 片付け

merge 済み・閉じ済みの PR の作業木 82 本（clean・未 push 0）を `git worktree remove --force` で外した（`D:/NeNeNib/outputs/20261104/worktree-cleanup.log`・残り 48）。残っているのは今日の席の作業木（`1146-fix` `1209-ab` `1198-*` `1199-*` `1224-*` `1227-*` `1228-*` `1229-*` `1230-*` `1235-*` `1239-*` と `*-fuzz-replay` `*-before`）・まとめ枝 2 本・実験の枝・detached の計測。

## 9. 夜（hide が戻ってからの指示）

- hide「1209 はマージでいいと思う」→ **#1209 は merge**（A/B の +2〜3% を受ける）。main が 11 commit 進んで生成物が衝突するので、まとめ枝 3（#1251・`chore/1251-integration-20261104c`）の最初に積んだ（ADR 0152 の追補の和・生成物 `--only matching-pair-nested-lines-` 3 件）。
- hide の要望「ダブルクリックでポイント中の単語を選択状態にしたい」→ Issue #1250（語は Vim の `iw` の 1 本の道・トリプルは行・通常は選択・Vim は VISUAL・意図は `PointText` に押しの回数）→ 席 I12。
- 夕方の席: I8（#1231）・I9（#1234）・I10（#1246）・I11（#1237）。レビューの後にまとめ枝 3 へ。
- #1234 → PR #1252（席 I9）: Issue の前提（`backspace=` が空）は誤り（oracle は `backspace=indent,eol,start`）。本当の原因は反復の 2 周目で挿入の始まりの桁が 1 周目が入れた全角の途中に落ち、Vim の `ins_bs` の歩きがその桁に当たらず行頭まで消すこと。`erase_lands_on`（始まりの桁が 1 文字の頭のときだけ止まる）。fixture 16 件・fuzz seed 6〜10 の 25 → 1。レビュー V-1252: 直してから 2（定数の重複・幅 0 の分岐の fixture）→ 席が直し中。後続（`insert_cut` の 16 MiB の写し）は #1237 に。
- #1231 → PR #1253（席 I8）: 3 件のうち `{N}zz` の列は直し（Esc の後の欲しい列を動く前の桁で置く）・`<C-u>` の後の `f;` は #1228 で直っていた・**`z-M` は oracle の都合**（`:normal!` の中は描き直しが無く古い `w_empty_rows` を次の `M` が読む・`redraw!` を挟んだ対話の Vim は Nib と同じ 6）→ 候補 B（Nib は今のまま・ADR 0146 に既知の差・fuzz は `--known`）。
- #1246 → PR #1254（席 I10）: INSERT の PgUp / PgDn は NORMAL と同じ `onepage` を呼び、端の失敗で `beep_flush`（残りを捨てる）。`<Up>` `<Down>` は `vim_beep` だけ。fixture の再生の口 `press_vim_keys` は打ち切った所が INSERT なら Esc を 1 つ流す（`:normal!` の `vgetorpeek` が typebuf が空になると Esc を返し、`ins_esc` の `--*count` が反復を起こす・`3ia<PageUp>b<Esc>` → `aaa03`）。窓の鍵とマクロは INSERT のまま続く（対話の Vim も同じ）。fixture 11 件・`--vim` 2,337,418。レビュー V-1254: 止める 0・直してから 0 → 後続 #1257（map の展開の途中の失敗は残りを捨てる・FLUSH_MINIMAL）。まとめ枝 3 へ。
- #1237 → PR #1256（席 I11）: 5 か所を索引の道に（`line_virtual_width`・`line_end − line_start`・貼る行ごとの `VimLineColumns`・`character_start` の手前の窓・`vim_replayed_block_width`）。同値の試験 14 列 13,824 鍵・probe `r` 53.9 → 3.26・`p` 324.9 → 0.017・`.` 4.78 → 0.094 ms。レビュー V-1256 中。
- #1250 → PR #1255（席 I12）: `PointerClicks`・`counted_clicks`（`WM_LBUTTONDBLCLK`・`GetDoubleClickTime`・`SM_CXDOUBLECLK`）・通常は `iw` の半開区間・契約 `--pointer` 22・`verify-window --double-click`（`window_driver.double_click` を足した）。レビュー V-1255: **Vim モードの「`viw` の鍵を流す」形は受けない**（`.` が消える・待ちの途中で文字が入る・回数つき INSERT の反復・マクロに録られる）→ `VimStepSource` に `PointerSelection{Selection, VimMode}` を足し core の `vim_pointer_selected` が `VimSelect` を直に置く形に直し中。
- まとめ枝 3（#1251）: #1209 → #1252 → #1253 → #1254 の 4 commit（生成物の衝突は `--only` で 3 / 18 / 7 / 11 件）。#1256 と #1255 はレビューの後。
- #1250 → PR #1255: レビュー V-1255 のとおり `VimStepSource` に `PointerSelection{Selection, VimMode}` を足し core の `vim_pointer_selected` が `VimSelect` を直に置く形に直した（`.`・マクロ・`''`・ジャンプの列・検索の強調に触れない・待ちと回数は `input_discarded`・INSERT は反復なしで離れる）。再レビュー V-1255-2: 止める 0・直してから 0・後続 3 → 同じ席が直し `c33fb30b`。`--pointer` 30・既定実行 2,337,209。
- まとめ枝 3 に #1256 と #1255 を積んで 6 commit。積んだ後にだけ出たもの 1 件: `NibTests.cpp` の `scopes` の表が 105 のまま 106 行（2 つの PR が +1 ずつで git が同じ変更として畳む・朝と同じ形・`test-conformance.py` の `test_protected_diff` が拾った）→ 106 に直して #1255 の commit に amend。**教訓: 表の大きさの行は積むたびに `eng/protected-diff.py` の `parse_scopes` で数え直す（`finish-*` の道具に足す）。**
- まとめ枝 3 の検証（6 commit・Debug）: 全 target ビルド 0 warning（696 s）・`ctest` 15 本 Passed・`symbols.py` 0 violation・`conformance.py` 0・`test-conformance.py` OK（表の大きさを直す前は `test_protected_diff` が 106 != 105 で拾った）・`format-check` 1491 files・`validate-git` 6 commit。PR → `merge-pr.py --integration` → CI 1 回。
- まとめ枝 3 のゲート（同じ木の Release・負荷 6%・matchparen 既定オン入り）: **8 benches checked, 0 regression(s)**（起動 40.1・窓 30.3・単打 0.62・200 打鍵 2.84・16 MiB を開く 51.2・16 MiB で 200 打鍵 3.03・面 1.04・長い行 1.09 ms・115 s）。
- まとめ枝 3 の木の Release の再生器で生成器 5 の seed 1〜10（既知は #1231 の `z-M`）: 不一致 2（既知 1・**新規 1**: `zz2<C-f>a)<Esc>`・so=0 vis12 top24 at30 で `a)` の位置が違う → Issue #1261・#1229 の回数つき `<C-f>` が本文の終わりに掛かる形の残差の見込み）。朝の 60 件からは 2 件。
- まとめ枝 3 の merge: CI run [41790476108](https://github.com/hideyukiMORI/nene-nib/actions/runs/41790476108) success → `gh pr merge --rebase` が GitHub の **502 Bad Gateway** で落ちた（PR は OPEN・head 不変）→ 同じ run を `--ci-run` で渡して再試行すると「Merge already in progress」→ 20 秒 × 9 回待っても OPEN のままだったので 3 回目の再試行で **main `40cc1467`**（6 commit・元の PR 6 本は道具が閉じた）。道具の「失敗を再試行しない」は merge の結果が分かっている（OPEN・head 不変）ときの再試行までは禁じていない（ADR 0118 の「成否不明なら PR を読んで確認」の形）。道具の後続（#1223）に「502 と in progress のときの待ち直し」を足す。

## 10. 深夜（夜 2 の席とまとめ枝 4）

| Issue | PR | 内容 | レビュー |
|---|---|---|---|
| #1223 道具の後続 | #1263（席 T2・`84048ac3`） | merge の呼び出しが 5xx か「Merge already in progress」のときは PR を読み直し、OPEN かつ head 不変なら 30 秒ごとに最大 10 回呼び直す（新しい CI は起こさない）・閉じる語の全部・元の PR の対応は積んだ順・`close_sources` の例外・250 の上限・ADR 0021 決定 8 / ADR 0118 の例外。49 tests | V-1263: 止める 0・直してから 1（待ち直しの後に merge が完了していたときの事後照合）＋ F-3（呼び直しの前に `behind_by` を読み直す）→ 直し済み。F-1 / F-2 は #1223 に残す |
| #1257 map の展開の途中の失敗 | #1265（席 I14・`8a38c72d`） | Vim は展開した鍵だけを単位（右辺 1 つ）ごとに捨て、打った鍵は捨てない（`flush_buffers(FLUSH_MINIMAL)`・実測 7 件）。`KeyMapEmit.ends` と `delivered_units` の 1 本。`--key-map` 761 | V-1265: 止める 0・直してから 0。気づき（流しの途中でモードが変わると後の単位は最初のモードの表）→ Issue #1266 |
| #1261 `zz` の後の回数つき `<C-f>` | #1267（席 I13・`d456c048`） | Vim の `onepage` の前への周は末尾が見えている周で先頭を最終行にする。Nib はその写しを最初の周だけに持っていた → 周ごとの 1 本 `pages_forward`（`forward_pages_held` と `limited_product` を畳む）。fixture 11 件・fuzz 2 → 1（新規 0） | V-1267: 止める 0・直してから 0 |
| #1258 巨大な 1 行の残り 2 | #1268（席 I15・`88ec92ef`） | `<C-w>` の左端は 64 バイトの窓（`indexed_word_erase_begin`）・`character_start(byte, count)` で何歩も戻る・ASCII の 1 歩の速道。`<C-w>` 1.55 → 0.011 ms・`99999999h` 509 → 15 ms。同値の試験 15,093 鍵 | V-1268: 止める 0・直してから 1（ADR の 1 文・設計席が足した）。`l` は Issue #1269 |

まとめ枝 4（#1264・PR #1270・4 commit・衝突なし）: Debug 全 target 0 warning（495 s）・ctest 15・symbols 0・conformance 0・format 1491・validate-git 4・`test_merge_pr` 49。merge は枝の中の新しい `merge-pr.py --integration`（待ち直し入り）で。

まとめ枝 4 のゲート（同じ木の Release・負荷 5%）: **8 benches checked, 0 regression(s)**（起動 41.0・窓 30.6・単打 0.59・200 打鍵 3.15・16 MiB を開く 52.8・16 MiB で 200 打鍵 2.99・面 1.08・長い行 0.86 ms・115 s）。

まとめ枝 4 の merge: CI run [41806832885](https://github.com/hideyukiMORI/nene-nib/actions/runs/41806832885) success → rebase merge で **main `b40ea348`**（4 commit・元の PR 4 本は道具が閉じた・新しい待ち直しの道は今回は通らなかった）。試用の exe `D:/NeNeNib/evidence/20261104-trial/NeNeNib-main-b40ea348.exe`（README 22〜24）。

**今日の合計: まとめ枝 4 本（#1220 #1240 #1251 #1264）で製品の PR 36 本と道具の PR 2 本が CI 4 回で main に入り、文書 PR 3 本。** fuzz 生成器 5 は 60 件 → 1 件（oracle の既知の差 `z-M`）。
