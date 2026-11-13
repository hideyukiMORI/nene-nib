# ADR 0184 — 照合器の構文を段で広げる: 量は原子の写しに開き、選択からは命令の列と優先順の状態集合で照合する

- 状態: 受理（設計席の決定・2026-11-12・#1566 の下ごしらえ G4 の実測 184 形から・施主の指摘「`:s` が無いのは不思議」の続き）
- 日付: 2026-11-12
- Issue: #1566（親 #1007・`:s` の段 1 は #1559 / ADR 0168）
- 規則: ARC-001 / ARC-003 / ARC-012、CPP-002 / CPP-005 / CPP-007 / CPP-012、QLT-014、ADR 0032（照合器 `VimPattern`）/ 0037（hlsearch）/ 0041 / 0043（incsearch）/ 0093（`*` を含む列の状態集合）/ 0168（`:s`）/ 0174（通常モードの検索と置換）

## 文脈

`:s` の段 1（ADR 0168）が入っても、照合器が受けない構文があるので、毎日打つ置換が「Unsupported pattern」で断られる（本文は変わらない）: `:%s/ \+/ /g`・`:%s/foo\|bar/x/g`・`:s/foo\zsbar//`・`:%s/\n//`・`:%s/\v(foo|bar)+/x/g`・`:/pat/s/a/b/`・`:s/a/b/ | s/c/d/`。

今の照合器（`D:/NeNeNib/outputs/20261112/1566/groundwork.md` を `docs/quality/vim-pattern-syntax-2026-11-12.md` に写す）:

- 道は 2 本。`*` が無い列は後戻りしない `matched_sequence`、`*` を含む列は状態集合で進む `VimPatternEvaluation`（Pike VM の形・ADR 0093）。量は原子 1 つに付く `*` だけ。群は原子の列の外で、範囲は後から升の表で引く（`VimPatternCapture`・上限 `vim_capture_cell_limit`）。
- 「1 行の中だけ」の前提が 6 か所に埋まっている（`matched` の引数と `$`・`VimSearch` の行ごとの数え直し・`VimSearchWalk::line_at`・`SubstituteAll` の行ごとの 2 周・`hlsearch` の `shown_matches` / `grid_marks`・`FindState`）。
- 通常モードの Ctrl+F / Ctrl+H は `literal_pattern` が `\` を逃がすので、新しい構文は届かない。

Vim 9.1 の規則（実測 184 形）: 選択は最も左の開始が先で、同じ開始なら最初に当たる枝が勝つ。量は貪欲で `\{-}` は最短。`\zs` が複数なら最後が効く。`\n` は最後の行の行末にも当たるが何もつながない。番地 `/pat/` は次の行から探す。

## 決定

1. **段で入れる。どの段も「今の構文だけのパターンは今の 2 本の道のまま」を最初の条件にする。** 検索・`n N`・`* #`・incsearch・`hlsearch`・通常モードの検索と置換の当たり方と費用を 1 つも変えない（契約: 既存の fixture 全件と `--vim-pattern-property`・ASCII と literal の速道の probe）。
2. **段 1 — 原子 1 つに付く量**（`\+` `\=` `\?` `\{n}` `\{n,}` `\{,m}` `\{n,m}` と最短の `\{-}` `\{-n,m}`）: parse が今の原子の列に開く（`a\{2,4}` は `a a a* …` の形の写し・`\+` は `a a*`・`\=` は「0 か 1」の量を原子の量の欄に足す）。量の欄は閉じた形（最小・最大・貪欲か）で、`*` は {0, ∞, 貪欲} になる。照合は今の状態集合の道（ADR 0093）で、最短は状態の優先順を逆に入れる。開いた後の原子の数に上限（`vim_pattern_atom_limit`・1 か所の定数・候補 1,024）を置き、超えたら閉じた失敗 `too_complex`（E363 と同じ知らせ）。失敗の形は Vim に揃える: `\{` の中の不正は E554・先頭の量は E866・量の後ろの量は E871（今の `a**` を「`a*` と文字 `*`」に読む食い違いも E871 に揃える）。`^\+a` などの端の形は Vim の実測のとおり。
3. **段 2 — 選択 `\|`・群に付く量・`\%(\)`**: パターンを命令の列（文字・集まり・任意・分岐・跳び・錨・終わり）に組み、優先順の状態集合で進める（後戻りなし・O(本文 × 命令数)・ADR 0093 の一般化・Vim 9.1 の NFA と同じ作り）。**命令の列に組むのは、選択か群に付く量を含むパターンだけ**（決定 1）。命令の数に上限（`vim_pattern_instruction_limit`・候補 4,096）を置き、超えたら `too_complex`。時計は使わない。後戻りの歩きを一般化する形（指数の時間に戻る）は採らない。
4. **群の範囲は後から引く**（今の `VimPatternCapture` の升の表を（命令・位置）の升で歩く・上限は今の `vim_capture_cell_limit` の 1 本）。検索と `hlsearch` の道は群の費用を払わない。上限を超えたら群を空にせず置換全体を断る（ADR 0168 の追補の規則のまま）。
5. **段 3 — `\zs` `\ze`**: 当たりの始まりと終わりを置き直す 2 つの印を、状態ごとに 2 欄だけ持って運ぶ（検索の着地にも要るので後から引かない）。複数の `\zs` は最後が効く。群の値は動かさない。
6. **段 4 — `\v` `\V` `\M`**: parse の手前に字句の段を置き、magic の形へ写してから今の parse に渡す（途中の切り替えも字句の段で）。照合器は変えない。
7. **段 5 — 行またぎ（`\n` `\_s` `\_.` `\_[…]`）**: 照合器が「行の列」を借りて進む形にし、**改行を含みうるパターンだけ**がこの道を通る（決定 1）。`:s` は当たりが行をまたいだら行をつなぐ 1 つの編集にする（範囲は元の行番号で数える・Vim の `do_sub` のとおり・キャレットの桁は実装の席が source と実測で決めて ADR に書く）。`hlsearch` と incsearch は、見えている範囲より上へ探す行数に上限を置く（パターンが含みうる改行の数が有限ならその数・無限（`\_.*`）なら固定の行数・1 か所の定数）。巨大な 1 行と 1 GiB の文書で UI のスレッドを止めないこと（行の列の借り方は行を写さない）。改行を含む literal の速道は後続。
8. **段 6 — 検索の番地と `|`**: 番地 `/pat/` `?pat?` `\/` `\?` `\&` は範囲を読む 1 本（`parsed_ex_range`）に足す。`|` の後の命令は Ex の評価の 1 本（`evaluated_ex`）で扱い、命令ごとに「`|` を文字として読むか」を表に持つ（`:s` のパターンと置き換えの中は文字）。前の命令が失敗したら後ろを流さない（`e` フラグは流す）。照合器の外なので、ほかの段と独立に入れてよい。
9. **順は 1 → 2 → 3 → 4 → 6 → 5。** 使う頻度と大きさで決めた（行またぎは 6 か所の前提を外すので最後）。1 段 = 1 PR。
10. **通常モード（Ctrl+F / Ctrl+H）には届かせない**（`literal_pattern` のまま・D82）。

## 強制

- 段ごとに oracle の fixture（検索の着地・`:s` の本文・`d/pat`・失敗は「本文が変わらず続きの鍵が打ち切られる」）。
- 性質の試験 `--vim-pattern-property`: 量と選択を含む乱数のパターンを、素朴な後戻りの模型（試験の中）と照合して同じ当たりになること・上限の内で終わること。
- 契約: 決定 1（今の構文だけのパターンが命令の列の道を通らない）を、道の選び方の表で守る。上限（原子・命令・升）を試験から注入して `too_complex` を確かめる。
- `hlsearch` と incsearch の塗りの範囲・E 番号の文言・速さは unit と probe（`nib_perf_probes` の検索の項・記録）。
- 状態: 各段の PR が入るまで `planned`。

### 段 1 の強制（#1566・active・この ADR を足した PR）

- 量の欄は閉じた enum `VimPatternRepeat`（`once` `star` `lazy_star` `optional` `lazy_optional`）で、読んだ量は `VimPatternQuantity`（最小・最大・最短か）。parse が `\{n,m}` を「n 個の `once` ＋ 残りの `optional`（上限なしなら `star` 1 つ）」に開く（Vim 9.1 の `nfa_regpiece` と同じ写し）。上限 `vim_pattern_atom_limit`（1,024・`VimPattern.hpp` の 1 か所）は開くときだけ数え、量の無いパターンには掛けない（決定 1）。試験は `parse` の引数で小さな上限を注入する。
- 道は今の 2 本のまま。`once` だけの列（量の無い列と `a\{2}` のような固定の回数）は `matched_sequence`、ほかは `VimPatternEvaluation`（最短は「0 回の枝を先に展開し、消費を後に置く」）。群の範囲は `VimPatternCapture` が同じ優先順で歩く（升の表は原子の写しの上でそのまま成り立つ）。
- 道を選ぶのは `VimPattern::route()`（閉じた enum `VimPatternRoute`: `sequence` `states` `lazy_states`）の 1 本で、`matched` と `VimPatternEvaluation` が同じ答えを読む。決定 1 の表は `--vim-search` の `verify_vim_pattern_routes`（量の無い代表 16 個と量の 4 個の道と原子の数）。
- 量の付いた基底＋結合文字は 1 原子 `cluster` に畳む（`é\+` は `éé`）。量の無い `é` は今どおり literal の列。
- `\c` `\C` の直後の量（`a\c\+` `a\C\{2}` `a\c*` `^\c*` `a*\c*`）は E866（`misplaced`）。Vim 9.1 の `nfa_regconcat` は `\c` を項目の間で読み、次の `nfa_regatom` が量を「Misplaced」で断る。星はパターンの先頭（`\c*`）と `\(` の直後（`\(\c*\)`）だけ文字のまま（`skipchr_keepstart` が `at_start` と `prevchr` を残す）。`\ca\+` と `a\+\c` は受ける（2026-11-13 に oracle で実測・fixture `pattern-case-multi-` 10 件）。
- 失敗: `brace`（E554）・`misplaced`（E866・先頭・`\(` の直後・`\{0}` の後ろ・`\c` `\C` の直後）・`multi_after_multi`（E871・`a**` を含む）・`too_complex`（E363 の文言）。群に付く量は `quantifier` で断る（段 2・段 2 の強制で受けた）。幅の無い原子に付く量は最小が 1 以上なら 1 つ、0 なら消す（`^\+a` `\<\+a` `^\=a` は実測どおり）。
- oracle の fixture `pattern-quantifier-` 67 件（検索の着地・`n`・`d/`・`?` の `\?`・`:s` の範囲と群・失敗は viewport の形で「本文が変わらず続きの鍵が打ち切られる」）。性質の試験 `--vim-pattern-property` の `verify_quantifiers_follow_the_model`（量と群を含む乱数のパターンを試験の中の後戻りの模型と照合・800 組）。unit は `--vim-search` の `verify_vim_pattern_quantifier_*` と知らせの文言。
- 段 1 の残り: Vim は `\{` の最大が 500 を超えると後戻りの照合器へ移って受けるが、Nib は 1,024 原子を超える量を E363 で断る。E866 の文言は打った文字の代わりに `multi` と書く。`é*`（星だけのパターン）は今どおり結合文字だけに星が掛かり、`xééy` で `é`（1:4）に当たる。Vim 9.1 は 1 文字の全体に掛けて `éé`（1:7）に当たる（2026-11-12 に `matchstrpos` で実測）。今の構文だけのパターンなので決定 1 で変えず、直すなら別の Issue で決める。`\<*` `$*` の星（Vim は量）・上限 1,024 の量の長い行での費用・再生器の 16,384 字の行とあわせて #1588。

### 段 2 の強制（#1603・active・選択と群の量の PR）

- parse は段 1 と同じ原子の列に、原子の境に立つ構造の印 `VimPatternMark`（`open` `close` `branch`・閉じの印は群に付く量 `VimPatternQuantity` を持つ）を足す。印に選択か量の付いた閉じがあるパターンだけを `VimPatternCompiler` が命令の列（`VimPatternInstruction`・`atom` `split` `jump` `open` `close` `accept`）に組み、道は `VimPatternRoute::program`。ほかは印を捨て、原子の列は段 1 と同じ値なので道も費用も同じ（決定 1・`\%(…\)` だけのパターンも今の道）。道は構築で 1 回だけ決めて持つ（V-1573b の L-a）。
- 選択は先の枝を先の道にした split の連なり（Vim の NFA_OR）。群の量は Vim 9.1 の `nfa_regpiece` と同じ写し（`\+` は 1 回と `*`・`\{n,m}` は n 回と (m − n) 個の平らな `\=`・`\{n,}` は n 回と `*`・`\{0}` は群を消し番号は残す・最短は split の先と後を入れ替える）。
- 上限は `VimPatternLimits.hpp` の `vim_pattern_atom_limit`（1,024）と `vim_pattern_instruction_limit`（4,096・原子 1 つは命令が多くとも 3 つなので原子の上限の 4 倍）。超えたら `too_complex`（E363）。群の入れ子の深さは 64 まで（`vim_pattern_nesting_limit`・組み立ての再帰の守り・超えたら `too_complex`）。試験は `parse` の第 4 引数 `VimPatternLimits{.atoms, .instructions}` で小さな値を注入する。
- 照合は `VimPatternMachine`（Pike VM）。列は食べる命令を優先順に持ち、幅の無い遷移は明示の積みで辿る。始まりは当たりが見つかるまで位置ごとに最も低い優先で足し（最も左）、当たった候補より低い優先は捨てる（最初に当たる枝）。同じ位置での 2 度目は Vim 9.1 の `addstate` と同じ規則で、食べる原子・錨・群の開きは辿らず、split・jump・群の閉じ・当たりは辿り終えるまで辿り直す。この規則で空の周は 1 回だけ通り、`\(\|a\)\+` は `aa` で空の当たり（開始 0）になる（全部の命令で 2 度目を捨てる形は `aa` に当たり、Vim と違う・2026-11-13 に実測）。確保は作るときだけ。
- 群の範囲は `VimPatternTrace` が後から引く（決定 4）。一致の始まりから終わりまでの道を同じ優先順の DFS で 1 本探し、升（命令・位置）のうち食べる原子・錨・群の開きは入ったときに、ほかは着かなかったときに印を付ける。群の値は閉じを通るたびにその周の開きからの範囲（最後の周・最後の周で通らなかった内側の群は前の値のまま）。`\(a*\)\+` と `\(a*\)*` は `aaa` で `\1` が空（Vim の実測）。升の上限は今の `vim_capture_cell_limit` の 1 本（命令数 ×（一致の長さ + 1））。
- parse の規則（2026-11-13 に Vim 9.1 で実測・`out/1603-impl/measured.md` の 70 形）: `\|` の直後は `\(` の直後と同じ（`*` は文字・`^` は錨・量は E866）。`\%(` の直後の `^` は錨で `*` は E866。`$` は `\)` と `\|` の手前で錨。選択と `\%(` の端の後ろの結合文字は前の literal の続きにしない（番号のある群の端は今どおり透す）。群の閉じの直後の量（`*` を含む）は群に付き、その後ろの量は E871、`\{0}` の後ろの量は E866。
- 失敗の文言は Vim 9.1 の文: 閉じない `\(` は `group`（E54）・閉じない `\%(` は `unclosed_plain_group`（E53）・対の無い `\)` は `unmatched_close`（E55）・10 個目の `\(` は `too_many_groups`（E872）。`branch` は消した。`quantifier` は対の無い `\}` と、群の端を割る結合文字の量だけ。
- `longest_bytes` は命令の列に後ろへの跳びが無ければ命令の DAG の最長、あれば値なし。`literal_bytes` は命令の列の道では値なし（選択の literal の組の速道は後続のまま）。
- 決定 1 の表: `--vim-search` の `verify_vim_pattern_routes`（段 1 の 20 行・変えていない）と `verify_vim_pattern_program_routes`（選択と群の量は `program`・`\%(ab\)` と `\%(a*\)b` は今の道）。
- oracle の fixture は `pattern-alternation-` 41 件・`pattern-group-multi-` 28 件・`substitute-group-` 31 件（足した 100 件・直す前の main で 84 件が落ちる）。性質の試験 `--vim-pattern-property` の `verify_branches_follow_the_model`（選択と群（番号あり・なし）と量を含む乱数の木 1,200 個を、試験の中の木の後戻りの模型と照合・当たりと群 1〜9）。unit は `--vim-search` の `verify_vim_pattern_alternation_priority` `verify_vim_pattern_group_iterations` `verify_vim_pattern_program_failures` `verify_vim_pattern_program_bounds` と知らせの文言。probe は `nib_perf_probes` の `pattern-branch-star-miss-4096` と `pattern-group-bounded-miss-4096`（`compare-probes.py` の表には載せていない）。
- 段 2 の残り: パターンの中の後方参照（`\(a\)\1`）は Vim では通るが Nib は今どおり断る。Vim は `\{` の最大が 500 を超えると後戻りの照合器へ移って受けるが、Nib は命令が 4,096 を超える群の量を E363 で断る。選択の literal の組の速道は後続。

## 帰結

- `:s` と検索で、Vim の人が毎日打つ形が通る。
- 照合器の道が 3 本（後戻りなしの列・`*` と量の状態集合・命令の列）になる。道を選ぶのは parse の結果の 1 か所。

## 残る穴

- `\&`（連言）・後方参照 `\1` をパターンの中で使う形・`\@=` などの前後の確かめ・`\%[]`・`\%V` などは後続。命令の列の道では後方参照を受けない（閉じた失敗）。
- `foo\|bar` の literal の組を行の絞り込みに使う速道は後続。
