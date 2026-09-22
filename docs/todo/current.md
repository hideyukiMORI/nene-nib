# いまのタスク — NeNe Nib

> GitHub Issue が正。ここは要約であり、Markdown のチェックリストをタスク状態として扱わない。
> 更新は実測でだけ行う。検証は差分から選び、関連入力が不変の成功結果を再利用する（QLT-001 / QLT-012・[ADR 0021](../adr/0021-diff-scoped-verification-and-result-reuse.md)）。

2026-09-20: hide 指示の Issue #61 で全件・最終 HEAD ごとの再実行規定を置換した。過去の引き継ぎにある全件指示より新方針を優先する。設定機能 #60 も新しい方針で必要な検証だけを選ぶ。

## 現在の Issue

**直近の道具の直しは [Issue #106](https://github.com/hideyukiMORI/nene-nib/issues/106)（cp932 の端末で検査が落ちる）**。`python eng/test-conformance.py` と `eng/validate-git.ps1` が cp932 の端末でだけ落ちていた既存の失敗を閉じた。子プロセスとの入出力の符号化を固定する場所は **python 側 1 か所**（`tests/conformance/test_verification_policy.py` の `run_tool`・`encoding="utf-8"` ＋ `errors="replace"` ＋ 子の `PYTHONUTF8=1`）と **pwsh 側 1 か所**（`eng/validate-git.ps1` の前置きの `[Console]::OutputEncoding` と `$OutputEncoding`）だけ。検査の閾値・違反文言・規則・`eng/git-conventions.py` は変えていない。4 通りの端末（cp932 / UTF-8 × `PYTHONUTF8` 有無）で **157 tests すべて成功**、日本語 subject の 11 commit に対する `validate-git.ps1` が cp932 で終了 0（変更前は GIT-003）。C++・fixture・CMake に触れないので build / unit / symbols / 速さ / 全件は未実行。詳細は gate-proofs 5-ai、統合状態は GitHub が正。

**直近の実装は [Issue #112](https://github.com/hideyukiMORI/nene-nib/issues/112)（矩形 VISUAL `Ctrl-v`）**。FR-003 の T2 に残っていた VISUAL の 3 つめを入れた（[ADR 0035](../adr/0035-vim-visual-block-as-column-ranges.md) 受理）。選択の正本は anchor / caret のままで、それを「仮想桁の矩形」と読む行ごとの範囲を core の純関数 `vim_block_range` 1 本が決め、描画の `SelectionSpan`・Ctrl+C / Ctrl+X・`d x y r`・`.` の大きさ・矩形レジスタの貼付がすべてその 1 本を通る。矩形の編集は行ごとの置き換えを運ぶ 3 つの効果（`VimRemoveBlock` / `VimReplaceBlock` / `VimInsertBlock`）で、controller が 1 つの `Edit` に畳んで写すので undo は矩形 1 つで 1 単位になる。`VimRegisterKind::block` と `VimRegister.width` で矩形レジスタの幅を持ち、短い行へ貼るときの空白の埋め・行の追加・回数が固定 Vim と一致する。窓は Vim の NORMAL / VISUAL でだけ Ctrl+V を矩形の鍵に写し、通常モードと Vim INSERT は OS の貼付のまま（決定 9・hide に見てほしい判断）。追加 144 fixture・計 1320 件、`--vim-visual-block` 1204 checks、共有経路（1593 / 265 / 208 / 2232 / 424 / 460）、unit 全体 13076 checks、build/tidy/ASan/UBSan・symbols 2 libs 0・conformance 0・format 成功。**速さは矩形の走査が行数に比例するので明示実行し、0 regression**（1 打鍵 0.925 ms / 16 MiB 265.646 ms・基準値は変えていない）。画面確認は未実施。詳細は gate-proofs 5-af、統合状態は GitHub が正。

実測 228 ケースで ADR 0035 の決定を 6 点直した（端に掛かった Tab / 全角は丸ごと外して空白に置き換わる・取る空白は矩形の内側で残す空白は外側・`$` の印はレジスタに残らず幅 1 つに畳まれる・`r` は文字の数ではなく桁の数だけ書く・貼付の右の空白は貼り先に本文が続くときだけ・`.` の再生は選択の角ではなく記録した幅から矩形を決める）。**合わせていないのは 6 点**（`.` の直前の `$` が桁の記録にも漏れる・VISUAL の `u` `U` が VISUAL から抜ける・`.` のあとの `u` の単位・貼る桁が全角の途中に落ちたとき・矩形レジスタの Tab の幅の数え方・行が矩形より手前で終わる 3 件が 1 回の `:normal!` で再現しないこと）。`c I A C > < J ~ gv`・VISUAL の中の `p`・`virtualedit`・マウスのドラッグでの矩形は範囲外のまま。

**直近の実装は [Issue #111](https://github.com/hideyukiMORI/nene-nib/issues/111)（後ろ向きの VISUAL の引用符の対）**。ADR 0031 が「残る穴」として残していた「後ろ向きの選択で `i" a"` がどの対を選ぶか」を、固定 Vim 9.1 の **645 ケース**で閉じた（[ADR 0031](../adr/0031-vim-text-objects-as-one-range-function.md) に「補足（2026-09-22・Issue #111）」・決定 4 と決定 6 を直した）。穴の正体は「奇数」でも「隙間」でもなく、**非空の選択では対の選び方そのものが選択の向きで変わる**こと（caret の手前の引用符の行頭からの番号が奇数＝ caret が対の外なら、前向きは後ろの対・後ろ向きは手前の対へ渡る）。caret が引用符の上にあるときは自分の引用符を端にせず、anchor は「選択が引用符を含む」「anchor の隣が引用符」なら置き換えた範囲の端へ動かずその場に残る。行をまたぐ選択は取消。`vim_text_object_range` は 1 本のままで、引用符の枝だけを `quote_outcome` に分けた。追加 39 fixture・計 1176 件、`--vim-text-objects` 2232 checks（1912 から）、`--vim-dot` 1593（同数）、unit 全体 11873 checks、build/tidy/ASan/UBSan・symbols 2 libs 0・conformance 0・format 成功。速さ・画面確認は未実施（engine の意味論だけの差分）。詳細は gate-proofs 5-ag、統合状態は GitHub が正。

**直近の実装は [Issue #108](https://github.com/hideyukiMORI/nene-nib/issues/108)（Vim の桁を仮想桁で数える経路を閉じる）**。表示幅の表と仮想桁の計算を core の純関数に 1 本ずつ置き、Vim の桁の規則を使う経路（欲しい列・VISUAL の `.` の桁）だけがそれを通るようにした（[ADR 0034](../adr/0034-vim-virtual-column-one-table.md) 受理）。`VimWantedColumn.column` と `VimCharacterExtent.column` は `VirtualColumn`（`tabstop=8`・`ambiwidth=single` 固定）になり、着地は `offset_at_virtual_column` 1 本を通る。本文の位置・テキストオブジェクト・`f t`・`r`・検索・描画の桁は `Column`（code point）のまま。ADR 0033 が残した「Tab と幅の混ざった本文では固定 Vim と答えが違う」穴はこれで閉じた。追加 47 fixture・計 1137 件、`--vim-virtual-column` 424 checks、欲しい列を使う経路（208 / 989 / 15 / 1593）、unit 全体 11553 checks、build/tidy/ASan/UBSan・symbols 2 libs 0・conformance 0・format 成功。**速さは 1 打鍵に行頭からの走査が増えるので明示実行し、0 regression**（1 打鍵 0.936 ms / 16 MiB 270.950 ms・基準値は変えていない）。画面確認は未実施。詳細は gate-proofs 5-ae、統合状態は GitHub が正。

実測で ADR 0034 の決定を 4 点直した（`U+007F` は 2 桁でなく 1 桁・Vim が `<200b>` と描く書式用文字は 6 桁で `DisplayWidth` に 4 つめの値が要る・Tab の上だけキャレットは最後の桁に描かれるので `caret_virtual_column` が 3 本目の関数として要る・逆引きの端は「最後の文字」ではなく「行の内容の終わり」）。表の出典も Unicode の版ではなく固定 Vim 9.1 の `strdisplaywidth()` の全 code point 実測（491 行）にした。`Ctrl-v` の矩形はこの仮想桁の上に載る後続。

**直近の refactor は [Issue #92](https://github.com/hideyukiMORI/nene-nib/issues/92)（INSERT の入力記録を捨てる割り込みを engine の 1 本の経路に）**。外からの割り込み（クリック・Ctrl+Z / Ctrl+Y・全選択・engine を通らない編集）で何を捨てるかは engine の純関数 `vim_interrupted` が決め、controller は「割り込まれた」ことを伝えて履歴の単位を切るだけになった（controller に `VimState` のメンバーへの代入は残っていない・ARC-001 / ARC-004）。捨てる範囲は入力行の取消（`vim_cancelled_input`・ADR 0032 の決定 1）と同じなので共通の `input_discarded` 1 か所に書き、取消はそれに「欲しい列を捨てる」と「モードを休止へ戻す」を足したものになった。ADR は書いていない（ADR 0030 の「結果」の 1 文だけを更新）。振る舞いは変えていないので fixture も足していない。`--vim-open-line-external` 46 / `--vim-open-line-recovery` 430 / `--vim-dot` 1593 / `--vim-search` 1375 / `--vim-character-search` 621 checks と unit 全体 10833 checks が production の変更の前後で同数、契約を足したあとは 54 / 573 / 10841 checks（+8 はこの契約ぶん）。`ctest` 4 件・build/tidy/ASan/UBSan・symbols 2 libs 0・conformance 0・format すべて成功。詳細は gate-proofs 5-ac、統合単位は draft [PR #109](https://github.com/hideyukiMORI/nene-nib/pull/109)、統合状態は GitHub が正。

**直近の実装は [Issue #91](https://github.com/hideyukiMORI/nene-nib/issues/91)（VISUALで行った変更の `.`）**。VISUAL で完了した変更は「範囲の大きさ」と「VISUAL を終えた鍵の列」を記録し、`.` はキャレットから同じ大きさを選び直してから同じ `accept` 経路へ再生する（[ADR 0033](../adr/0033-vim-visual-dot-repeat-by-extent.md) 受理）。`VimRepeatRecord` に `optional<VimVisualExtent>`、`VimReplay` に `optional<VimVisualExtent> reselect` を足しただけで、効果は 1 鍵 1 つのまま。選び直しは core の純関数 `vim_visual_reselect` 1 本。ADR 0030 の決定 5（VISUAL の変更は記録を消す）はこの ADR が置換した。追加 78 fixture・計 1055 件、`--vim-dot` 1593 checks、共有 6 scope（変更前と同数）、unit 全体 10833 checks、`nib_unit` 2.28 s、build/tidy/symbols/conformance/format 成功。画面確認は未実施。詳細は gate-proofs 5-ab、統合状態は GitHub が正。

実測 138 ケースで ADR 0033 の決定を 2 つ直した（`N.` は VISUAL の記録では回数を使わない・桁は 1 行なら個数/複数行なら最終行の絶対桁/`$` は「行末まで」のまま）。**合わせていないのは 1 点**で、固定 Vim は桁を仮想桁で数えるが、この engine の `Column` は code point で `wanted_column` から `H M L` まで一貫している。仮想桁は表示幅の表と tabstop（＝ `Ctrl-v` と同じ前提）が要るので別 Issue に送り、Tab と幅の混ざった本文の fixture は採らず `--vim-dot` の対象 unit が engine の答えを固定している。

**直近のテスト整備は [Issue #97](https://github.com/hideyukiMORI/nene-nib/issues/97)（scope専用の契約を既定の単体実行へ）**。`--vim-dot` / `--vim-text-objects` の契約が selector 指定時にしか走っていなかったので、契約部分を fixture と分けて `verify_vim_scope_contracts` の表にまとめ、既定実行（CTest の `nib_unit`）へ載せた。既定は 8733 → 8823 checks（+90）・1.45 → 1.56 s、selector は 909 / 1623 checks のまま。テストの内容・閾値・fixture は変えていない。詳細はgate-proofs 5-y。

統合済み: #87 は [PR #90](https://github.com/hideyukiMORI/nene-nib/pull/90)、#93 は [PR #96](https://github.com/hideyukiMORI/nene-nib/pull/96)、#97 は [PR #102](https://github.com/hideyukiMORI/nene-nib/pull/102)、#88 / #94 は PR #89 / #95 で main `8f2c363` へ。#100（検索・ADR 0032 受理）は [PR #103](https://github.com/hideyukiMORI/nene-nib/pull/103) で main `b07c574` へ統合済み（fixture 977 件）。進行中は [Issue #98](https://github.com/hideyukiMORI/nene-nib/issues/98)（`fixtures.json` の整形・`chore/98-fixtures-json-format`・draft [PR #105](https://github.com/hideyukiMORI/nene-nib/pull/105)）と [Issue #91](https://github.com/hideyukiMORI/nene-nib/issues/91)（VISUAL の `.`・別 worktree で並行）。

**直近の道具の整えは [Issue #98](https://github.com/hideyukiMORI/nene-nib/issues/98)（`fixtures.json` の整形を 1 つに固定）**。`tests/vim/fixtures.json` は 3 通りの形（indent 2 ＋ 空の `"settings": []` が 295 件・1 行 1 件が 668 件・1 行に全部詰めた `viewport-follow-*` が 14 件）で混ざっていた。整形を決める関数を `eng/vim-oracle.py` に 1 つ置き（`canonical_fixtures_json`）、書き戻す `--format` / `--regenerate` と検査する CNF-011（`eng/conformance.py`）が同じ 1 か所を呼ぶ（ARC-001）。**既存の 1 行 1 件の 668 行はこの関数の出力とバイト単位で一致した**ので、追記者が手で書いてきた形をそのまま正準形にしている。整形は `--format` の 1 回だけで、**0 measured / 977 reused**・Vim 不要・変わったのは JSON のバイト列（108687 → 96756 bytes・2459 → 979 行）と生成物の SHA 行 1 行だけ。期待値・入力・`VimFixtures.hpp` の 977 行は 1 つも変えていない。CNF-011 は正例 1 と反例 15 通りを `tests/conformance` が回し、実リポジトリでも 2 通りの反例で終了 1 を確認した。既定の `nib_tests` は 10149 checks で #100 と同数、`nib_unit` 2.13 s。詳細は gate-proofs 5-aa。

**直近の実装は [Issue #100](https://github.com/hideyukiMORI/nene-nib/issues/100)（Vimの検索 `/ ? n N * #`）**。Ex と同じ入力行（`CommandInput` の 3 つめの値 `SearchLine`・見え方は `InputLineView` 1 つに畳んで描画を 1 本に）から入り、Enter の確定は `VimKey` の `VimSearchPattern` 1 鍵として engine に届く（[ADR 0032](../adr/0032-vim-search-as-input-line-and-one-key.md) 受理）。照合器 `VimPattern` は `magic` の部分集合（リテラル・`.`・`*`・`^`・`$`・`[...]`・`\<` `\>`・エスケープ・`\d \w \s` とその大文字）だけを受け、未対応の構文は閉じた失敗で拒否する（`std::regex` と `<locale>` は使わない）。`n` / `N` は `last_search`（失敗した検索も覚える）、`*` / `#` はキャレットの語を `\<…\>` にして同じ経路を通り、範囲の端だけは元のキャレット。オペレータは既存の exclusive の規則 1 か所を共用し、`.` は検索を鍵 1 つとして再生する。報せ（E486 / E35 / E348 / 折り返し / 未対応構文）は Ex と同じ左ステータスに出て次の入力で消える。追加 135 fixture・計 977 件、`--vim-search` 1375 checks、共有境界（`.`・r・f/t・gg）と Ex・設定一覧、unit 全体 10149 checks、build/tidy/symbols/conformance/format 成功。画面確認は未実施。成果物は `build/issue100/NeNeNib.exe`、詳細は gate-proofs 5-z、統合状態は GitHub が正。

実測 218 ケースで食い違ったのは 1 点だけで、期待値ではなく ADR の補足を直した（`\<あいう\>` はひらがな → 漢字の境界で `あいう漢字` に一致する）。48 を超えた `VimAction` の分岐が関数長 60 行に収まらなくなったので、CPP-012 のとおり「動作 → 大分類」を `constexpr` の表にし、NORMAL と VISUAL は `VimActionGroup` を網羅する switch で写した（閾値は触っていない。表の行の欠落と重複は `static_assert` 2 つが落とす）。`:s` `:g`・検索履歴・offset・`\v` `\c` `\(` `\|` `\{`・`ignorecase` / `smartcase`・`hlsearch` / `incsearch` の描画は後続。

**直近の実装は [Issue #93](https://github.com/hideyukiMORI/nene-nib/issues/93)（Vimのテキストオブジェクト）**。オペレータ保留中とVISUALの `i` / `a` を排他的な次キー待ちにし、新しい純関数 `vim_text_object_range` 1本が `iw aw iW aW i" a" i' a' i` a` i( a( i{ a{ i[ a[ i< a<`（`b` / `B` と閉じ括弧の鍵も別名）の範囲を決めて、d/c/y と VISUAL の選択へ同じ範囲を渡す（[ADR 0031](../adr/0031-vim-text-objects-as-one-range-function.md) 受理）。`.` は鍵の列なので追加の記録なしに `diw.` `ci"x<Esc>.` が再生される。追加192fixture・計842件、`--vim-text-objects` 1623 checks、待ちを共有する `.`・r・f/t・g と VISUAL yank、unit全体8733 checks、build/tidy/symbols/conformance/format成功。画面確認は未実施。詳細はgate-proofs 5-w、統合状態はGitHubが正。

実測364ケースでADR 0031の決定4点を直した（VISUALは行単位にならず改行まで届く・括弧は中に居なくても前の塊を使う・`i(` の2つの寄せは独立・`y` のキャレットは範囲の先頭の桁）。合わせていないのは、回数が尽きた `d9iw` でVimがキャレットを動かすことと、塊の外から数えた `2i(` が内側へ入ることの2点（fixtureに採っていない）。`it at` / `is as` / `ip ap`・`Ctrl-v`・`> < gu gU`・VISUALの後ろ向きの選択を伸ばす規則は後続。

**直近の実装は [Issue #87](https://github.com/hideyukiMORI/nene-nib/issues/87)（Vimの`.`）**。NORMALの直前の変更を鍵の列として記録し、同じ `accept(VimKeyPress)` の経路へ再生する（[ADR 0030](../adr/0030-vim-dot-repeat-as-key-replay.md) 受理）。回数付き `.` は記録の回数を置き換えて次の `.` にも残り、INSERTを伴う命令はEscまでを1つの変更として確定する。追加99fixture・計650件、`--vim-dot` 909 checks、共有境界（o/Oの回数反復・r・f/t・g待ち）とunit全体7181 checks、build/tidy/symbols/conformance/format成功。画面確認は未実施。成果物は `build/issue87/NeNeNib.exe`、詳細はgate-proofs 5-v、統合状態はGitHubが正。

取消の扱いは測り直しで決定3と一致することが分かった。Vimのビープが `:normal!` の残りの鍵を捨てるため最初の測定が誤っていたもので、鍵を区切って測ると取消になった命令は自分の鍵を捨てるだけで直前の変更を変えない。engineは変更していない。VISUALで行った変更の `.` だけがVimと違い（何もしない・決定5）、[Issue #91](https://github.com/hideyukiMORI/nene-nib/issues/91)へ送った。回数付き `i a I A` が回数を捨てていた穴は、`N.` の要件のためADR 0028の入力記録で塞ぎ（ADR 0028に補足）、controllerが入力記録を直接消す二重経路は[Issue #92](https://github.com/hideyukiMORI/nene-nib/issues/92)に分けた。

**直近の実装は [Issue #84](https://github.com/hideyukiMORI/nene-nib/issues/84)（Vimのr）**。NORMALの回数指定、文字/行単位VISUALの置換、Unicode/Tab、NORMALのEnter、取消、CRLF、undo/redoを接続した。追加39fixture・計551件、対象460 checks・build/tidy/symbols/conformance/format成功。画面確認はnative pipe接続エラーで未実施。成果物は `build/issue84/NeNeNib.exe`、詳細はgate-proofs 5-u、統合状態はGitHubが正。

統合単位は [PR #86](https://github.com/hideyukiMORI/nene-nib/pull/86)。

VISUAL r<Enter>はliteral CRを挿入するため、oracleと文書模型の既存問題を [Issue #85](https://github.com/hideyukiMORI/nene-nib/issues/85)へ分離した。今回は選択を維持して入力待ちを解除する。制御文字の引用/置換・Ctrl-e/yの隣行参照も未対応。

**前回の修正は [Issue #81](https://github.com/hideyukiMORI/nene-nib/issues/81)（行単位VISUAL yankの戻り位置）**。下向き/単一行は範囲先頭の列1、上向きの複数行は現在位置へ戻す。追加23fixture・計512件、対象265 checks・build/tidy/symbols/conformance/format成功。実機操作ツールの接続エラーで今回の画面確認は未実施。修正版は `build/issue81/NeNeNib.exe`、旧版の起動窓は保持。詳細はgate-proofs 5-t、統合状態はGitHubが正。

統合単位は [PR #83](https://github.com/hideyukiMORI/nene-nib/pull/83)。

**前回の修正は [Issue #77](https://github.com/hideyukiMORI/nene-nib/issues/77)（VISUAL移行時の希望列）**。開始/種類切替/回数の希望列を補正し、対象テストとnativeを確認。採用18fixture・計489件。行単位VISUAL yankの既存問題はIssue #81へ分離。検証と再利用はgate-proofs 5-s、統合状態はGitHubが正。

統合単位は [PR #82](https://github.com/hideyukiMORI/nene-nib/pull/82)。9月21日未明の続行分も9月20日の作業開始日にまとめて記録した。

**前回の実装・限定検証完了は [Issue #79](https://github.com/hideyukiMORI/nene-nib/issues/79)（Vimのo/O）**。hideの#76実機確認と続行指示により、main `8f48f49`から `feat/79-vim-open-lines` で実装した。ADR 0028、追加oracle40件・計471件、初回unit551中549成功と修正/追加対象430・46成功、native3件、build/tidy/symbols/conformance成功。統合状態はGitHubを正とする。

成果の統合単位は [PR #80](https://github.com/hideyukiMORI/nene-nib/pull/80)。独立レビューは未解決指摘なし。関連実装が不変のため、push/review/mergeではgate-proofs 5-rの成功結果を再利用する。

直前の完了はIssue #76 / PR #78（gg/Gと指定行移動）。main `8f48f49`へ統合し、hideが実機確認済み。既存VISUAL移行時の希望列解除はIssue #77へ分離している。

直前の完了はIssue #72（Vimの行内文字検索と反復）。2026-09-20夜のhideの続行指示で再開し、`feat/72-vim-character-search`で既存移動・範囲へ接続した。[ADR 0026](../adr/0026-vim-character-search-and-scoped-oracle.md)、追加oracle60件（旧329件は維持）、単体621 checks、native7シナリオ、build/tidy/symbols/conformanceが成功。PR #75でmainへ統合済み。

直前の完了はIssue #70（C4b・利用者テーマの選択/保存/復元）。[ADR 0025](../adr/0025-user-theme-catalog-and-selection.md)、対象unit88、Ex153、palette130、設定codec23、catalog301 checksとnative120 DPIが成功し、[PR #71](https://github.com/hideyukiMORI/nene-nib/pull/71)で製品commit `9a493fa`へ統合。停止記録はPR #74、再開時のmainは `b6d19ff`。
Issue #68 / PR #69（C4a）、#66 / PR #67（C3b）、#64 / PR #65（C3a）、#60 / PR #63（C2）、#58 / PR #59（Vim画面移動）、#61 / PR #62（差分検証・成功再利用）はmainへ統合済み。

## 段階

| 段階 | 状態 |
| --- | --- |
| Phase 0 言語の実測 | ✅ 2026-09-15（114 記録・`docs/quality/phase0-results.json`） |
| Phase 1 文書とゲートの足場 | ✅ 2026-09-15（Issue #1・PR #2） |
| Phase 2 negative proof | ✅ 2026-09-15（26 反例・`docs/quality/gate-proofs.md`） |
| Phase 3 縦切り | 🔲 進行中。#3 窓（ADR 0007）✅ → #5 見た目（ADR 0008）✅ → #7 編集（ADR 0009）✅ → #11 ファイル（ADR 0010）✅ → #16 速さ（ADR 0011）✅ → #19 起動の内訳 ✅ → #22 Vim の最初の縦切り（ADR 0012）✅ → #24 窓を先に見せる（ADR 0013）✅ → #28 IME（ADR 0014）✅ → #31 タブの帯（D16）✅ → #30 計測器の揺れ ✅ → #36 欠測の言い方 ✅ → #44 生成物の SHA（CNF-010）✅ → #43 Vim の 2 本目（ADR 0015）✅ → #47 CI の速さの基準値（ADR 0016）✅ → #52 カラーテーマ C1（ADR 0017）✅ → #53 VISUAL（ADR 0018）✅ → **#58 画面移動 ✅ → #60 C2 ✅ → #64 C3a ✅ → #66 C3b ✅ → #68 C4a ✅ → #70 C4b ✅ → #72 行内文字検索 ✅（ADR 0026）→ #76 指定行移動 ✅（ADR 0027）→ #79 開行と反復 ✅（ADR 0028）** |
| Phase 4 公開 | 🔲 |

## 実装したもの（Issue #87まで）

Vimの`.`: NORMALの直前の変更を回数1つと鍵の列で記録し、`.` / `N.` で同じ経路へ再生する。`x d c y`系・`D C`・`r`・`p P`・`i a I A o O` ＋ 入力 ＋ `<Esc>`、`f/t/;`・`gg/G` を含む待ちも対象。移動・yank・undo/redo・Ex開始・取消（範囲が作れない・回数が入らない・検索が外れる・Escなど12経路）は記録を変えない。`.` 1回はundo 1単位で、回数付き `i a I A` も入力を繰り返す。

VISUALの`.`: VISUALで完了した変更（`d x c r`）は範囲の大きさ（文字単位は行数と「行末まで/桁」・行単位は行数）を記録し、`.` はキャレットから同じ大きさを選び直してから鍵を再生する。行や桁が足りなければ畳むが記録は縮まず、`N.` は大きさを変えない。桁は code point 単位で、Tab と幅の混ざった本文だけ固定 Vim（仮想桁）と食い違う。

Vimのr: NORMALの回数分/文字・行単位VISUALの選択範囲を1回のreplaceで置換する。元の行境界と無名レジスタを保ち、1操作ずつundo/redoできる。普通のUnicode文字とTab、NORMALのEnterが対象。次キー待ちは検索/gと排他的。

行単位VISUAL yank: 下向き・単一行は列1、上向きの複数行は現在列へ戻す。NORMALと文字単位のyankは維持。

VISUALの希望列: v/Vの開始・種類切替・同じキーでの終了は保持し、回数付きの横選択は実際に移動した到達列へ更新する。

Vimの開行: NORMALの `o/O`、回数付き入力のEsc時反復、BS/Enter/UTF-8、CRLF/LF、既存INSERT/IME/undo/redo。移動・元本文への削除・外部編集で古い反復を解除する。VISUALではo/Oとも端点交換。

Vimの指定行移動: `gg` / `G`、絶対行番号、明示回数と未指定の区別、d/c/yの両端を含む行単位範囲、VISUAL端点。g待ちと文字検索待ちは排他的。取消と無効後続はキーを消費し、モード・選択・欲しい列・検索記憶を維持する。

Vimの行内文字検索: `f/F/t/T`と`;`/`,`、回数、d/c/y、VISUAL端点、UTF-8、行境界、未発見・取消・検索記憶、undo。未設定registerと空yank済みを区別する。oracleは対象prefixの指定と既存期待行の検証付き再利用に対応した。NORMAL/VISUALでIMEを閉じる仕様は継続する。

C4b: `%LOCALAPPDATA%/NeNeNib/themes`の起動時カタログ（最大128件）。Ex/Tab/Ctrl+Pから選択、保存と復元、名前付き診断、壊れた保存設定の保護。ファイル変更は再起動で反映。

C4a: ThemeDocumentによる動的な名前/出典の所有、`.v1.theme`の厳密な読込と色/コントラスト検証。既存設定とkey=value解析を共通化。選択はC4bで接続済み。

C3b: Ctrl+Pの共通設定一覧。テーマ名の部分列照合、上下/Tab/ホイール選択、クリック/Enter実行、フォント値の補完入力。本文・選択・Vim状態を保ち、入力中はIMEを閉じ終了後に復元する。

C3a: NORMALの `:` から設定用Ex入力。Tab/Shift+Tab補完、単行貼付、取消、テーマ/pt/フォントの即時反映と保存。結果は左ステータスへ表示。

C2: 設定の保存・復元、8〜40 ptの本文拡縮（Ctrl+`+` / `-` / `0` とCtrl+wheel）。本文・行番号だけを拡縮し、テーマ・フォント名・サイズを版付きで保存する。

枠なし窓（Snap と影は OS のまま）・Mica のタイトルバーにタブ 1 本と窓の操作・piece table の本文（複数行・スクロール・選択・Ctrl+C/X/V・Ctrl+Z/Y・クリックでキャレット）・
ステータスバーの「通常 | Vim」トグル・OS のライト／ダーク（茄子色 D11・橙 D12）・ファイルの開閉と保存（Ctrl+O / Ctrl+S / Ctrl+Shift+S・起動引数・UTF-8 / BOM / Shift_JIS・CRLF / LF・一時ファイルからの置換・未保存の印と確認）・速さのゲート（Release の exe で 4 本のベンチ・基準値の鍵 5 つ・実機の基準値・起動の内訳・窓を先に見せる起動（約 35 ms）・`WM_PAINT` で 1 フレームに 1 回の描画）・Vim（NORMAL / INSERT・`h j k l 0 $ w b e ^ gg G` と Home / End・回数（オペレータ側と移動側の掛け算）・`x`・`d c y` ＋移動・`dd cc yy`・`D C Y`・`p P`（文字単位と行単位・回数）・無名レジスタは種類つきで本文は LF・`i a I A`・`o O`・Esc・`u` / Ctrl-r・INSERT 1 回が undo 1 単位・VISUAL `v V`（回数・`o`・`d x y c`・表示と Ctrl+C と操作が同じ範囲）。画面移動 `H M L`・Ctrl-d/u/f/b・PgUp/PgDn、window-local な半画面量、Vim の自動追従。本物の Vim 9.1 の oracle が生成した fixture 551 件を CTest が再生）・IME（IMM32・変換中の文字列は本文の外の `Composition` に持って renderer がキャレットの行に差し込んで描く・確定は 1 意図で通常モードは undo 1 単位・Vim INSERT は打鍵として engine へ・Vim NORMAL では IME を切り INSERT で戻す・候補窓はキャレットの直下）・タブの帯は不透明の `title_bar`（D16・アクティブなタブは本文色）・速さのゲートは刺激が届かなかった試行を欠測にし、計測不能を退行と別の終了コード 2 で言う。性能検証は必要な変更で選んで実行する。一致する指紋の基準値で判定し、通常のCIでは測り直さない（ADR 0016 / 0021）。生成物 `VimFixtures.hpp` と `fixtures.json` の一致は CNF-010 が守る。core にテーマの模型（`Theme` / `SyntaxPalette` / `derive_ui`・組み込み 9 テーマ・コントラスト比 4.5 以上を tests が要求。ADR 0017）があり、設定ファイルから指定テーマまたはOS追従を選べる。NORMALのExからも切替可能。Ctrl+Pの設定一覧はC3bで接続済み。

## 動かないもの

64 MiB 超のファイル・文字コードと改行の手動切り替え・IME の再変換と TSF 固有の機能・
VISUAL の `p u ~ > < J I A gv` と `X D C Y`（この縦切りでは何もしない）/ ドラッグで VISUAL / 矩形の `c I A C > < J ~` と VISUAL の中の `p` / `virtualedit` /
autoindent / 仮想桁（Tab と全角の表示幅・`.` の VISUAL の桁と `Ctrl-v` が同じ前提） / 名前つきレジスタ / `J s S R` / rの制御文字・Ctrl-e/y / 検索の `:s` `:g`・履歴・offset・`\v` `\c` `\(` `\|` `\{`・`ignorecase` / `incsearch`（`hlsearch` は #123 で入れた） / 一般Ex（`:w` / `:q`、範囲、パイプ、履歴）・
複数タブ・Ctrl+Pのファイル/フォルダ/ブックマーク/履歴統合・折り返し・横スクロール・ドラッグ選択。

## 次の 1 手

[Issue #123](https://github.com/hideyukiMORI/nene-nib/issues/123)（検索の当たりの強調・`hlsearch` 既定オン）は実装・限定検証・文書まで済み、draft [PR #124](https://github.com/hideyukiMORI/nene-nib/pull/124)（`feat/123-search-highlight` は ADR 0037 の `4c0414c` の上）。ADR 0037 を受理し、`VimState` の閉じた 3 値と `vim_line_matches` の 1 本、`LineView` の `matches` / `current_match` で入れた。fixture は増減なし（強調は Vim の報告に出ないので契約 74 checks が正本）。速さのゲートは `startup-window-shown` が 46.782 ms で落ちたが、同じ機械の変更前の exe も 38.741 ms（最大 62.021 ms）で上限 43.666 ms に近く、測る区間が本件の差分より手前なので機械の揺らぎと見ている。静かな機械での 1 回の測り直しが要る。実機の画面確認は未実施。

[Issue #91](https://github.com/hideyukiMORI/nene-nib/issues/91)（VISUAL の `.`）は実装・限定検証・文書まで済み、draft [PR #107](https://github.com/hideyukiMORI/nene-nib/pull/107)。Ready・必須 check・merge は設計リナが行う。#98 は main `a44f380` へ統合済みで、#91 はその上に rebase してある。
#92（割り込みの 1 本化）は main `9e41f79` へ統合済み。[Issue #99](https://github.com/hideyukiMORI/nene-nib/issues/99)（テキストオブジェクトの残る 3 点）は実装・限定検証・文書まで済み、draft [PR #110](https://github.com/hideyukiMORI/nene-nib/pull/110)（`feat/99-vim-text-object-residuals` は `9e41f79` の上に rebase 済み・fixture 1055 → 1090 件）。ADR 0031 に補足を足し、回数が尽きたときのキャレットと後ろ向き VISUAL の伸ばし方を固定 Vim に合わせた（塊の外からの回数は実装が既に正しかった）。後ろ向きの選択で引用符のどの対を選ぶかだけが穴として残り、Issue は未起票。その後は 仮想桁（`virtcol`）＋ `Ctrl-v` → #85 を焦点 Issue ごとに進める（順は設計リナの案・hide 未確認）。仮想桁の Issue は未起票。
`eng/test-conformance.py` の `test_verification_policy.py` 6 件は、この機械の端末符号化（cp932）で pwsh / subprocess の出力が読めないことによる**既存の失敗**で、未変更の `b07c574` でも同じ 5 件が落ちる（残り 1 件は `validate-git.ps1` が `git show` の UTF-8 を pwsh の入力符号化で受ける問題で、main の commit `8f2c363` でも同じく落ちる）。#98 が原因ではないので別 Issue へ切る。
[Issue #85](https://github.com/hideyukiMORI/nene-nib/issues/85)（literal CR）は実装・限定検証・文書まで済み、draft [PR #120](https://github.com/hideyukiMORI/nene-nib/pull/120)（`feat/85-literal-cr-line-model` は #106 の `b1be094` の上に rebase 済み・fixture 1320 → 1339 件）。ADR 0036 を受理し、改行の形を `TextBuffer` に 1 つ持たせて `lf` の本文の `\r` を文字として扱う。既存の `-crlf` の fixture 10 件は Vim が `unix` と読んでいた（CRLF の fixture ではなかった）ので engine の契約 `verify_vim_crlf_documents` へ移した。VISUAL の `r<Enter>` は fixture で守られ、矩形 VISUAL の `<CR>` と制御文字の描画（`^M`）は後続。
今回の結果と未確認は [日報](../reports/2026-09-22.md) / [引き継ぎ](../handoffs/2026-09-22.md)。変更に関係する検証だけを行い、成功結果を再利用する。
