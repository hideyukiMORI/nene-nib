# 照合器 `VimPattern` の未対応の構文の実測（2026-11-12・#1566・ADR 0184）

ADR 0184（照合器の構文を段で広げる）の根拠の表。下ごしらえ G4 の記録をリポジトリの外の作業の記録から写した（道具と生の結果はリポジトリの外にあり、ここにはこの文書だけを残す）。段 1（原子 1 つに付く量）は ADR 0184 の「段 1 の強制」の PR で入り、§2.1 の量の形は oracle の fixture `pattern-quantifier-` 67 件が守る。§1.2 の `quantifier` の行のうち `\{` `\+` `\=` `\?` は段 1 で受けるようになり、群に付く量と対の無い `\}` だけが `quantifier` のまま。§2.1 の「`^\+a` と `\<\+a` は Nib は断ってよい残差」は、段 1 で Vim の実測どおり受けるようにした（`^\=a` も）。

## 下ごしらえの記録

席 G4（Opus）・2026-11-12・読むだけ（製品のコードは変えていない・ビルドしていない）。
読んだ木: PR #1564 の作業木 `D:/NeNeNib/worktrees/1007-substitute`（HEAD `d886dc0f`）。
Vim の実測: `C:\Program Files\Vim\vim91\vim.exe -u NONE -i NONE -N -n -es`（`regexpengine=0`・既定の magic / wrapscan）。
道具 `D:/NeNeNib/scripts/20261112/1566/probe.py`・`cases.py`（176 形）・`cases_extra.py`（追加 8 形）。生の結果は `measured.json` / `measured.md` / `measured-extra.md`（この文書の末尾の表と同じ）。

---

## 1. 今の照合器の棚卸し

### 1.1 受ける構文（`VimPattern::parse`・`src/core/VimPattern.cpp`）

| 種類 | 形 | 原子 / 扱い |
|---|---|---|
| literal | 1 code point・`\` + 記号（`\.` `\*` `\[` `\/` `\\` `\^` `\$` など） | `literal`（`exact` は結合文字と、その直前の基底） |
| 任意 | `.` | `any`（1 文字の残りの結合文字まで食べる・#1146） |
| 集合 | `[...]` `[^...]`（区間 `a-z`・先頭の `]`）・`\d \D \w \W \s \S` | `set`（`bracketed` は `[...]` だけ・ic で畳むのはこれだけ） |
| 結合文字だけ | 基底の literal の直後でない結合文字（`.` の直後なら `.` を消す） | `composing`（#1506） |
| 錨 | `^`（パターンの先頭・`\(` の直後）・`$`（末尾・`\)` の直前） | `line_start` / `line_end` |
| 語の境 | `\<` `\>` | `word_start` / `word_end` |
| 量 | `*` だけ（直前の原子 1 つに付く・幅の無い原子と `*` の後ろでは文字） | 原子の `repeated` の bool |
| 大小 | `\c` `\C`（原子を足さない・`\c` が勝つ） | `ignore_case_` |
| 群 | `\(` `\)`（9 個・入れ子可） | 原子の列に入れず `VimPatternGroup{first, end}`（原子の添字）の列。照合は読まない |
| 区切り | 打った向きの区切りは offset で断る・`\/`（`?` なら `\?`）は文字 | — |

### 1.2 閉じた失敗で断る構文（`VimPatternFailure`）

| 失敗 | 形 |
|---|---|
| `group` | 閉じない `\(`・対の無い `\)`・10 個目 |
| `branch` | `\|` |
| `quantifier` | `\{` `\}` `\+` `\=` `\?`・群の後ろの `*`（`\(ab\)*`） |
| `magic` | `\v` `\V` `\m` `\M` |
| `escape` | `\%` `\_` `\@` `\&` `\z`（`\zs` `\ze` を含む）・英数字の `\`（`\n` `\t` `\r` `\a` `\1` など）・末尾の単独の `\` |
| `previous_substitute` | `~` |
| `offset` | 区切りの後ろ（`/foo/e`） |
| `invalid_utf8` | 壊れた UTF-8 |

**Vim と食い違う既存の 1 形（範囲外・記録）**: `a**` を Nib は「`a*` ＋ 文字 `*`」に読むが、Vim 9.1 は `/a**` も `:s/a**/` も **E871**（`Can't have a multi follow a multi`）で断る（`x-double-star-search` `x-double-star-sub`・fixture に `**` は 0 件）。`^*` の `*` が文字なのは Vim と同じ（`x-caret-star`）。量を足す段で「量の後ろの量は E871」に揃えるのが自然。

### 1.3 照合の作り

- **入口は `matched(line, from)` の 1 本**（1 行の `string_view` と開始バイト → 最も左の一致 `{begin, end}`）。2 つの道を原子列そのものから選ぶ（ADR 0093 の決定 2・3）。
  - **反復の無い列**（`*` が 1 つも無い）: 各開始位置で `matched_sequence` が原子を順に 1 回ずつ評価（後戻り無し・確保無し）。開始位置は Vim の 1 文字の頭だけ（`ends_a_character`）。O(行長 × 原子数)。
  - **反復を含む列**: `VimPatternEvaluation`（ADR 0093）。**これは既に優先順つきの状態列（Pike VM／Thompson の形）**で、状態は「次の原子の添字」、候補が持つのは開始バイト 1 つ。同じ位置・同じ原子の 2 本目は捨てる（`begins` の有無が訪問済みの印）。`*` は「1 文字食べて同じ原子へ戻る」を先に、「0 回で次へ」を後に並べる（貪欲）。一致を得たら低優先を捨て、高優先が尽きたら返す（leftmost-first）。結合文字の途中は「原子 + `skipping_`」の番号で列に残す（Vim の `add_off` と同じ）。仕事は 1 文字ごとに O(原子数 + 集合の区間数)、確保は原子数に比例。
- **群の範囲**は照合器の外で後から引く: `VimPattern::captured(line, match)` → `VimPatternCapture`。照合と同じ優先順の **明示の積みの DFS**、試して着かなかった（原子・位置）の升に 1 bit の印（RE2 の BitState と同じ形）。仕事・確保は升の数（原子数 + 1）×（一致の長さ + 1）に比例し、`vim_capture_cell_limit`（4 Mi 升）を超えたら値なし → `:s` 全体を `too_complex`（E363）で断る（ADR 0168 追補 13）。`\1`〜`\9` を読まない置き換えは歩かない。
- **速道**:
  - `literal_bytes()`: 魔法の無い literal だけの列（ic で畳む英字・非 ASCII を含めば無し）→ `VimSearchWalk` が piece の view の上の `find` / 先頭バイトの `find` で **候補の行へ飛ぶ**（#583）。当たりの位置は照合器が決める。
  - `longest_bytes()`: `*` が無ければ原子数 × 4 バイト。巨大な行の格子の `hlsearch` が窓の余裕に使う（無ければ `giant_line_half_width` = 4,096 バイト）。
  - ASCII のバイトは結合文字の表を引かない（`ends_a_character` の先頭の比較）。
- **「1 行の中だけ」が埋まっている所**（行またぎで触る所）:
  1. `VimPattern::matched` の引数（1 行の view）と `line_end` の錨（`at >= line.size()`）・`any` / `set` が改行を見ない前提（行に `\n` が無い）。
  2. `VimSearch.cpp` の `visit_matches` / `first_match` / `last_match`（行ごとに 0 桁目から数え直す・`at >= window.end` で打ち切る）と `searched_once` の結果が 1 つの `Offset`（終わりを持たない）。
  3. `VimSearchWalk::line_at`（1 行の view・piece をまたぐ行だけ `scratch_` に繋ぐ・CRLF は末尾の `\r` を外す）と `candidate_after` / `candidate_before`（literal の含まれる **行** へ飛ぶ）。
  4. `SubstituteAll.cpp` の `each_line_match` / `line_tallied` / `line_edited`（`line_text` を写して行ごとに 2 周・行ごとの `VimBlockEdit`）。
  5. `EditorController.cpp` の `shown_matches` / `grid_window_matches` / `grid_marks`（見えている 1 行 or 巨大な行の窓 + 前後 8 バイトの文脈）。
  6. `FindState.cpp` の `line_match`（1 行の `line_text` と `vim_line_matches`）。

### 1.4 照合を呼ぶ所と 1 回に見る範囲・費用

| 呼ぶ所 | 道 | 1 回に見る範囲 | 費用の形 |
|---|---|---|---|
| `/` `?` の Enter・`n` `N`・`*` `#`・`d/pat` など（`VimStepSearch.cpp` → `vim_find_match`） | `VimSearchWalk` で行を選び、行ごとに `first_match` / `last_match` | 起点の行 → 本文の端 → 折り返して起点の行まで（最悪は本文の全体・1 GiB 可）。巨大な回数は #1334 の周期で飛ばす | 鍵 1 つで同期（UI のスレッド）。literal なら候補の行へ飛ぶ。非 literal は全行を照合 |
| incsearch の preview（`EditorControllerVim.cpp` `previewed_match` → `found_match` → `vim_find_match`） | 同上 | 同上（入力の 1 打鍵ごと・回数ぶん） | 打鍵ごとに同期 |
| `hlsearch`（通常の行・`shown_matches` → `grid_window_matches` → `vim_window_matches`） | 見えている行ごと | 行の全体（`shown.text`）。巨大な行は表示の窓 + 前後 8 バイト | frame ごとに見えている行の数だけ（`BodyTextLayouts` とは別） |
| `hlsearch`（格子・`grid_marks`） | 同上 | 見えている画面行の範囲 ± `longest_bytes`（無ければ 4,096） | frame ごと |
| `:s` `:&` `&` `g&`（`VimStepSubstitute.cpp` → `substituted` / `SubstituteAll`） | 範囲の行ごとに `line_text` を写して 2 周（数える → 作る）・群は `captured` | 範囲の行の全部 | 同期。行 × 当たり。群は升の上限 |
| 通常モードの Ctrl+F / Ctrl+H（`EditorControllerDetail.cpp` `typed_pattern`・`EditorControllerCommand.cpp` `find_pattern`・`FindState.cpp`） | `literal_pattern` で `\ / . * [ ~ ^ $`（後ろ向きは `?` も）と先頭の結合文字を逃がしてから同じ照合器 | `current_find_match` は 1 行 + `vim_find_match`・すべて置換は `substituted_all` | **新しい構文は届かない**（`\` を逃がすので `\+` `\|` `\zs` `\v` は全部 literal の `\` になる）。magic で素の `+ = ? { } \| ( )` は今も文字 |
| テキストオブジェクト・文・段落の探索（#751 の塊の走査） | 照合器を使わない | — | 影響なし |

---

## 2. Vim 9.1 の実測の要点（表は §6）

### 2.1 量の指定

- `\+` `\=` `\?` `\{n}` `\{n,}` `\{,m}` `\{n,m}` `\{}`（= `*`）`\{,}`（= `*`）は**直前の原子の貪欲な繰り返し**。`a\{3,1}` は `\{1,3}` と同じ（逆順は入れ替え・`q-reversed`）。`a\{0}b` は `b`。
- `\{-}` `\{-n,}` `\{-,m}` `\{-n,m}` は**最短**。`a\{-}` は常に空、`a\{-1,}` は 1 文字ずつ、`.\{-}c` は最初の `c` まで（`q-dot-lazy` `q-lazy-ab` `q-x-lazy-y`）。`:s/a\{-}/X/g` は `XaXaXa`（空の当たりの規則は今の `*` と同じ）。
- 群に付く: `\(ab\)\+` は `ababab`。群の値は**最後の周**（`\(ab\(.\)\)\+` on `abcabd` → `\1=abd` `\2=d`）。空の周を回る `\(a*\)\+` は止まって空の当たり（`b` で `(0,0)`・`:s/\(a*\)\+/[\1]/` on `baa` → `[]baa`）。`\(a\|b\)\{2,3}` は `aba`。
- 集合・`.` に付く: `[0-9]\+` `.\+` `\s\+$` は普通どおり。全角 `あ\+` はバイト 3:15。**結合文字つきの 1 文字に量**: `é\+`（`e` + U+0301 + `\+`）は `x éé y` の `éé`（1:7・量は基底 + 結合文字の 1 原子に掛かる）。`e\+` on `x é e e y` は `ee`（4:6・`é` の基底の `e` には当たらない・#1138 と同じ）。`.\{2}` は `xé` `éy`（`.` は 1 文字の全体）。
- 失敗: `\{` の中の不正（`a\{` `a\{x}` `a\{1,2,3}` `a\{-`）は **E554**。先頭の `\+` `\{2}` は **E866**（`Misplaced +` / `Misplaced {`）。量の後ろの量（`a\+\+` `a*\+` `a\{1}\{2}` `a**`）は **E871**。`^\+a` と `\<\+a` は Vim では通る（錨に量・Nib は断ってよい残差）。
- 後戻りの爆発: `\(a*\)*b` と `\(a\|aa\)\+b` を `a` × 30 に掛けても Vim の NFA は即答（当たり無し）。
- 大きな回数: `a\{30}` は普通に当たる（Vim の NFA は `\{n,m}` を原子の写しに展開する）。

### 2.2 選択 `\|`

- **最も左の開始が先、同じ開始なら最初に当たる枝**（leftmost-first・Perl と同じで POSIX の最長ではない）: `a\|ab` on `abc` → `a`、`ab\|a` → `ab`、`a\|b` on `ba` → `b`（開始 0 が先）。
- 後ろが合わなければ次の枝へ戻る: `\(a\|ab\)c` on `abc` → `abc`。POSIX の試験 `\(a\|ab\)\(c\|bcd\)\(d*\)` on `abcd` → `[a|bcd|]`（leftmost-first の答え）。
- 空の枝: `\|a` は位置ごとに空の当たり（`(0,0)` …）、`a\|` は `a` があれば `a`、無ければ空。
- 錨: `^a\|b$` の `^` と `$` は**枝の端でも錨**（`cab` → `b`・`acb` → `a` と `b`）。`x\|^b` の `^` も錨。`a$\|$b` の 2 つ目の `$` は文字（`a$b` の `$b`）。
- 群の番号は枝をまたいで通し（`\(a\)\|\(b\)` → `[a][b]`・通らなかった群は空）。`\%(...\)`（番号の無い群）は普通に使われる（`b-noncapture`）。
- `:s/foo\|bar/X/g` → `X X baz`。`\<\(foo\|bar\)\>` → `X food X`。

### 2.3 `\zs` `\ze`

- 当たりの範囲（`n` の着地・`:s` の置き換え・`&`・`hlsearch` の塗り）は `\zs` から `\ze` まで。`foo\zsbar` → `bar`(3:6)、`foo\zebar` → `foo`(0:3)。`:s/foo\zsbar//` → `foo`、`:s/foo\zsbar/<&>/` → `foo<bar>`。
- **`\zs` が複数なら最後に通ったもの**（`a\zsb\zsc` → `c`・`\(a\zs\)*b` on `aab` → `b`）。枝の中の `\zs` はその枝が当たったときだけ（`a\zsb\|cd` on `cd ab` → `cd` と `b`）。
- 群の値は `\zs` に動かされない（`:s/\(a\zsb\)/[\1]/` on `xab` → `xa[ab]`・`\1=ab` で `&=b`）。
- `\ze` が `\zs` より前なら空（`a\zeb\zsc` → 空の当たり (2,2)）。
- `n` と回数は `\zs` の位置で数える（カーソル (1,1) から `2/a\zsb` は (1,5)・1 つ目の `a` は (1,1) だが `\zs` が (1,2) なので数える）。`d/foo\zsbar` は `\zs` の手前まで消す（`bar end`）。
- `:s/.\zs/-/g` → `a-b-c-`、`:s/a\ze/-/g` → `---`、`:s/\zs/X/g` → `XaXbXc`、`:s/foo\zsbar//gn` → 2 matches。

### 2.4 行またぎ

- `\n` は**行末**に当たる（最後の行の行末にも当たる・`n-eol-search` (1,4)(2,2)・`b\n` は最後の行で当たる）。`.` と `[^x]` は改行に当たらず、`\_.` `\_s` `\_[...]` と `[...\n]` は当たる（`n-dot-under` `n-under-neg-set` `n-set-with-newline`）。`a$\n^b` の `$` `^` は改行の前後で錨。
- 検索の着地は当たりの始まり。行末から始まる当たり（`/\nb`）は NORMAL では**行の最後の文字**に置かれる（`n-search-n` (1,2)）。そのため `d/\nb` を最後の文字の上で打つと範囲が空で何も消えない（`n-dw-across`）。当たりは 3 行にまたがれる（`a\nb\nc`）・重なりは次の当たりの始まりから（`a\na` on `a⏎a⏎a` → (1,1) と (2,1)）。
- `:s` の行またぎは**行をつなぐ編集**: `:%s/\n//` → `abc`・`:%s/,\n/, /` → `a, b, c`・`:%s/a\n/X\r/`・`:1s/\n/\r\r/`・`:%s/\n\n\+/\r/`（空行の圧縮）・`:%s/\n*b/X/` → `aX`。
- **最後の行の行末の `\n` は当たるが何もつながない**: `:$s/\n//` は本文を変えず失敗もしない・`:%s/\n//gn` は「3 matches on 3 lines」（3 行の本文）・`:%s/b\n/X/` は `a⏎X`・`:%s/\_s\+/ /g` は末尾に空白を足す（`a b c `）。
- **範囲は元の行番号で数える**: 当たりの始まりが範囲の元の行にある限り置き換える。つないだ後ろの本文の当たりも、その始まりが範囲の元の行なら置く（`%s/a\nb/X/g` on `a⏎ba⏎b⏎c` → `XX⏎c`・`2,3s/\n//` → `a⏎bcd`・`s/\n// 3` → `abcd`）。範囲の外の元の行から始まる当たりは `g` でも置かない（`1s/,\n/;/g` on `a,⏎b,⏎c` → `a;b,⏎c`）。
- **行末の直前で終わった当たりの直後の改行は試さない**（`[b\n]` / `\_[b]` を `a⏎b⏎c` に `:%s/…//g` → `a⏎c`・「3 substitutions on 2 lines」・検索の当たりの列も (2,2) を飛ばす）。今の Nib の行の走査（`from >= line.size()` で打ち切る）と同じ規則で、行またぎでも同じ所で打ち切れば合う。
- `:s` の後のキャレットの桁は行またぎで一様でない（`%s/\n//` は (1,3)・`1s/\n//` は (1,2)・`1,2s/\n//` は (1,3)・`s/\n// 3` は (1,1)・`%s/,\n/ /` は (1,1)）。規則は `ex_cmds.c` の `do_sub` の終わりを読んで決める（下ごしらえでは決めない・fixture が正本）。

### 2.5 `\v` `\V` `\M`

- Vim では全部通る。`:%s/\v(foo|bar)+/x/g` → `x x`、`\va+` `\v<foo>` `\va{2,}` `\vcolou?r` `\v(\w+) (\w+)`・途中の切り替え `a\vb+`・`\V` は `\` だけが魔法（`\Va.b` は literal・`\Va\.b` は `.` が任意）・`\M` は `*` が文字（`\Ma*` → `a*`）・`\v` の中の `\=` は文字 `=`。
- **入れ方**: parse の前に「字句 → magic の字句」の写しを 1 段置けば足りる（`\v` では `( ) | + = ? { @ % < >` が魔法で `\(` が文字・`\V` では `\` の後ろだけが魔法・`\M` では `*` `.` `[` `~` が文字）。途中で切り替わるので字句器は状態（今の magic の段）を持つ。パターンの文字列を別の文字列へ書き換えて parse し直す形は `\` の数え方（`smartcase` の `has_uppercase`・区切り `\/` `\?`）がずれるので避け、parse の項目を読む所（`parsed_next`）が「今の段で魔法か」を 1 か所で問う形がよい。

### 2.6 検索の番地と `|`

- `:/pat/s` は**カーソルの次の行から**前向き（今の行は見ない・`a-current-line-skip`）・wrapscan で折り返す（`a-wrap`）・無ければ E486。`:?pat?s` は前の行から後ろ向き。`:.,/b/s` `:/a/,/b/s`（`,` は両方ともカーソルの行から）・`:/b/;/c/s`（`;` は前の番地の行から）・`:/b/+1s`。`:\/s` は直前の検索・`:\?s` は後ろ向き・`:\&s` は直前の置換のパターン。番地の中の `\|` は選択（`/foo\|bar/`）。`:/c/` だけなら行へ移る。
- `|`: `:s/a/b/ | s/c/d/` は 2 つ流す・`:s/a/b/|`（空）は通る・`:s/a/b/g|s/c/d/g`・`:%s/a/b/|%s/b/c/`・`:s/a/X/ 2 | s/X/Y/`（回数の後）・`| normal! x`。**パターンと置き換えの中の `|` は文字**（`:s/a|c/X/` は `a|c` に当たる・`:s/a/b|c/` は `b|c` に置く）。前の命令が失敗したら後ろは流さない（E486）・`e` があれば流す。

---

## 3. 設計の候補

### 前提（どの候補でも同じ）

- **最初の条件「検索と `hlsearch` と incsearch の今の当たり方と費用を 1 つも変えない」** は、parse が作る値の形で守るのが確実: 今の構文だけのパターンは今の 2 つの道（`matched_sequence` と `VimPatternEvaluation`）へそのまま行き、新しい構文を含むパターンだけが新しい道へ行く。分けるのは「原子列そのものから決める」（ADR 0093 の決定 2 と同じ・設定や cache で選ばない）。今の fixture（`search-` `substitute-` ほか）と `--vim-pattern-property` と probe の `pattern-*` 7 本が、そのまま守り手になる。
- 群の範囲・`\zs` `\ze` を読む必要があるのは `:s`（群・`\zs` `\ze`）と検索の着地・`hlsearch`（`\zs` `\ze` だけ）。群を読まない呼び手に群の費用を払わせない。

### A: 今の原子列に「量」と「選択」を足す（後戻りの歩きを一般化）

- データ: `VimPatternAtom.repeated` を `{min, max, greedy}` に。選択は「枝の列」を原子列に埋める形が要る（群も枝も原子列の外の木）。
- 実際の問題: 今の速い道は後戻りではなく状態列（ADR 0093）。A を「後戻り」で書くと、`\(a\|aa\)\+b` や `\(a*\)*b` で指数になり、ADR 0093 が消した問題を戻す。守りは仕事量の上限しかなく、上限に当たった検索は「見つからない」か「断る」になって Vim と違う答えを返す（Vim の NFA は即答）。
- 群の範囲: `VimPatternCapture` の升の印は「（原子・位置）の失敗は道に依らない」ことに頼る。量 `{n,m}` を数で持つと升に「何周目か」が要り、升の数が max 倍になる。選択も枝ごとの升が要る。結局、原子列を「命令の列」に開くのと同じ形に寄っていく。
- 大きさ: 中（parse と後戻りの歩き）。ただし速さの保証が無いので probe と上限の試験が大きくなる。
- **評価: 量（原子 1 つに付くもの）だけなら成り立つが、選択と群の量では不利。**

### B: パターンを小さな命令の列（Thompson の NFA）に組み、状態の集合を優先順で進める（Pike VM）

- データ: `VimPatternProgram`（命令の列）。命令は「1 文字を食べる（原子 1 つ・今の `consumes` / `eaten` / `whole_character` をそのまま使う）」「錨（今の `anchored`）」「分岐 split(先, 後)（優先順つき）」「jmp」「印 save(k)（群の端 `2i` `2i+1`・`\zs` `\ze`）」「受理」。`\{n,m}` は原子（群）の写しに開く（Vim の NFA と同じ・上限を決める）。`\{-}` は split の優先を逆にするだけ。
- 照合: `VimPatternEvaluation` の一般化そのもの。状態 = 命令の番号（今は原子の番号）。同じ位置・同じ命令の 2 本目を捨てる規則・一致の後に低優先を捨てる規則・結合文字の途中の印（`skipping_`）はそのまま。O(行長 × 命令数)・後戻り無し・指数にならない。空の周（`\(a*\)\+`）は「同じ位置・同じ命令は 1 回」で自然に止まる。**Vim 9.1 の既定の照合器（NFA・実測の失敗文は全部 `(NFA regexp)`）と同じ作りなので、優先順と空の周の答えが Vim と揃いやすい。**
- `\zs` `\ze` と群: 2 つの形がある。
  - B1: 候補ごとに印の配列を持つ（Pike VM の本来の形）。`\zs` `\ze` だけなら印は 2 つで、検索の道でも安い。群の 20 個は `:s` だけが要る。
  - B2: 照合は「始まり・終わり」だけを出し、印は後から今の `VimPatternCapture`（BitState）を「命令・位置」の升に一般化して引く。失敗の升の印は「後方参照が無い限り道に依らない」ので量と選択を入れても成り立つ。上限は今の `vim_capture_cell_limit` の 1 本のまま（升 = 命令数 × 一致の長さ）。
  - 推し: **`\zs` `\ze` は B1（印 2 つ・検索でも要る）、群は B2（今の `captured` を命令の列で歩く・`\1`〜`\9` を読む `:s` だけ）**。こうすると「群を読まない呼び手は群の費用を払わない」（今の性質）が残る。
- 今の速道: 今の構文だけのパターンは今の道へ行く（前提）。新しい道でも `literal_bytes()` は「選択の無い・量の無い literal」のときだけ値を持つ（今と同じ）。選択の literal の組（`foo\|bar`）を行の絞り込みに使う拡張は後続（`VimSearchWalk` の候補が 1 本の literal を前提にしている）。`longest_bytes()` は命令の列から上限を出せる（選択は枝の最大・`\{n,m}` は m 倍・`\+` `*` は無し）。
- 大きさ: 中〜大（parse を木 → 命令の列へ・評価器の状態を命令へ・`VimPatternCapture` を命令へ）。ただし評価器と升の歩きの骨格は今のものを残せる。
- 試験の網: `--vim-pattern-property` の生成器に新しい断片（`\+` `\=` `\{n,m}` `\{-}` `\|` `\%(` `\zs` `\ze`）を足し、(1) 新しい道と、量を `*` と連結に開いた等価なパターン（`a\+` = `aa*`・`a\{2,3}` = `aaa\=`… は今の道では書けないので `a\{2}` = `aa` など今の道で書ける形だけ）を同じ行に掛けて一致を比べる（**今の道との差分の試験**）、(2) 群の範囲は当たりの中・入れ子の包含・`\zs` ≤ `\ze` の性質、(3) 仕事量の上限（命令数 × 行長）を数え口で確かめる（今は「数え口が無い」と試験の頭に書いてある）。Vim との一致は fixture（oracle）の領分。

### C: 段を切る

| 段 | 中身 | A か B か |
|---|---|---|
| 1 | 量 `\+` `\=` `\?` `\{n,m}` `\{-n,m}` を**原子 1 つ**（literal・`.`・集合・結合文字だけ・群は除く）に付ける。量の後ろの量は E871・先頭は E866・`\{` の不正は E554 に揃える（`a**` も） | A でも B でも小さい。A なら `repeated` を `{min,max,greedy}` に、評価器は「`max` までの写し」を持たずに「`min` 回の必須の写し + 残りの任意」へ parse で開けば、今の評価器の状態（原子の添字）だけで済む（`a\{2,3}` = `a a a\=`・`a\{-}` は 0 回の枝を先に並べる）。B の命令の列の最初の一歩としても同じ形 |
| 2 | 選択 `\|`・`\%(\)`・群に付く量 `\(ab\)\+` | **B**（原子の列のままでは書けない・`VimPatternGroup{first,end}` の「原子の添字の範囲」は選択と群の量で破れる） |
| 3 | `\zs` `\ze` | B1（印 2 つ）。段 2 の命令の列の save 命令の 2 つ |
| 4 | 行またぎ `\n` `\_x` `[\n]` | B の評価器に「行の供給」を足す（下） |
| 5 | `\v` `\V` `\M` `\m` | parse の手前の字句の段（§2.5） |
| 6 | 検索の番地と `\|` の後の命令 | 照合器の外（`ExRange` の `ExAddressBase` に `pattern` を足し、engine の `resolved_lines` が `vim_find_match` を呼ぶ・`ExSubstituteCommand.cpp` の `text.starts_with('\|')` の拒否を外して次の命令を返す） |

- 段 1 は A の形で書いても B の最初の段（量の写しに開いた原子列 = 分岐の無い命令の列）と同じ値になるので、手戻りが無い。

### 行またぎの形（段 4）

| 形 | 中身 | 良い点 | 悪い点 |
|---|---|---|---|
| **行の列を見る**（Vim の `reg_getline` と同じ） | 照合器は「行 i の view」を問う口（`VimSearchWalk::line_at` か `TextBuffer::line_text`）を借り、状態の集合が行末で `\n` を食べたら次の行の頭へ進む。`$` `^` は行の端 | CRLF は行の view が `\r` を外すので考えなくてよい（ADR 0036）・今の行の走査（候補の行・piece の view・折り返し）をそのまま使える・`\n` を含まないパターンは今の 1 行の道のまま | 当たりの位置が（行・桁）の組になり、`VimPatternMatch` と `searched_once` の答えを 2 点の範囲に広げる |
| 本文の窓（改行を含むバイト列）を見る | `text_range` で前後を写した窓に照合 | 照合器は 1 本の `string_view` のまま | CRLF の本文で `\r\n` を 1 つの改行に読む写しが要る・窓の大きさ（どこまで先を読むか）が決まらない（`\_.*` は本文の最後まで）・1 GiB で写しが大きい |

- **推し: 行の列を見る形**。照合器は「`\n` を含みうるか」（`\n` `\_x` `[...\n]` があるか）を parse で 1 回決めて値に持ち、含まないパターンは今の `matched(line, from)` のまま（費用を変えない）。
- `:s` と検索で要るもの:
  - `:s`: 当たりの範囲（行・桁）×2 → 「つなぐ編集」。§2.4 の範囲の規則（元の行番号・最後の行の行末は何もつながない・行末で終わった当たりの直後の改行は試さない）。`SubstituteAll` の 2 周（数える → 作る）を、行の `VimBlockEdit` から「行の範囲をまたぐ 1 つの置き換え」へ広げる（undo 1 単位は今と同じ）。上限（#1376 の 1 GiB）の数え方は同じ。
  - 検索 `/` `n` `N`: 当たりの始まりだけ（着地）。行末から始まる当たりは最後の文字へ寄せる（Vim の実測）。`VimSearchWalk` の行の選び方（literal の候補）は、`\n` を含む literal（`,\n`）では「行末が `,` の行」を探す形に変える必要があり、最初は literal の速道を使わない（全行を照合）でよい。
  - `hlsearch` と incsearch: 見えている行の塗りは「見えている範囲より上で始まって入ってくる当たり」を拾う必要がある（Vim は上の行へ戻って探し、`redrawtime` の時計で打ち切る）。core は時計を読めないので**行数かバイトの予算**で打ち切る（例: 見えている最初の行から上へ N 行まで）。設計席の決め所。
- **巨大な 1 行（16 MiB）と 1 GiB の本文で UI を止めない条件**:
  - 評価器が B なら 1 文字あたり O(命令数) で、指数にならない。止まりうるのは「本文の全体を見る」こと（今の非 literal の検索と同じ）と、命令数が大きいこと（`\{n,m}` の写し）。**命令数の上限**（例: 写しを開いた後の命令数 ≤ 数百〜数千・超えたら閉じた失敗 `too_complex`）を parse で決めるのが、時計を使わない唯一の守り。
  - 群の範囲（B2）は今の `vim_capture_cell_limit`（4 Mi 升）の 1 本に命令数を掛けた升で同じ守りになる（`\(.*\),\(.*\)` の 1 MiB 程度の一致まで・超えたら E363 で `:s` 全体を断る・今の追補 13 のまま）。
  - 行またぎの当たりの長さ: `\_.*` は本文の最後まで伸びる。検索は終わりを使わない（着地だけ）ので評価は線形で済むが、`:s` の置き換えと `hlsearch` の塗りは長さに比例。`:s` は今の 1 GiB の上限で、`hlsearch` は見えている範囲で切る（今の `vim_window_matches` と同じく窓の終わりで切る）。
  - 巨大な行の格子の `hlsearch` は `longest_bytes` が無いと 4,096 バイトの余裕で照合する（今と同じ）。量 `\{n,m}` は上限があるので `longest_bytes` を出せる。

### 比較の表

| | A | B | C（段 1 = A の形・段 2 以降 = B） |
|---|---|---|---|
| 今の当たり方と費用 | 変えない（道を分ける） | 変えない（道を分ける） | 変えない |
| 後戻りの爆発 | 上限が要る・上限で Vim と違う答え | 無い（O(n·m)） | 無い |
| 仕事量の上限 | 歩数の上限（新設） | 命令数の上限（parse）と今の升の上限 | 同左 |
| 群の範囲 | 升の表が量と選択で破れる | 今の BitState を命令で歩く（升の表が成り立つ） | 同左 |
| 選択・群の量 | 書きにくい | 自然 | 段 2 |
| `\zs` `\ze` | 別の印 | save 命令 | 段 3 |
| 行またぎ | 行の供給を足す | 行の供給を足す | 段 4 |
| Vim との揃い | 優先順を手で合わせる | Vim の NFA と同じ作り | 同左 |
| 大きさ | 中（ただし試験が大） | 中〜大 | 段ごとに小〜中 |

---

## 4. おすすめと段

**C（段を切る）で、段 2 から B（命令の列 + 優先順の状態集合 = ADR 0093 の一般化）にする。**

1. **段 1（量）**: 原子 1 つに付く `\+` `\=` `\?` `\{n,m}` `\{-n,m}`。parse で「必須の写し + 任意 / 星」に開き、評価は今の `VimPatternEvaluation` の状態（原子の添字）で足りる形にする（`repeated` を「0 回の枝が先か後か」も持つ値に）。写しの数に上限。E554 / E866 / E871 に揃える（`a**` を含む）。今の構文だけのパターンは値が同じなので道も費用も変わらない。`:%s/ \+/ /g` `:s/colou\=r/` `[0-9]\+` `.\{-}` が通る。
2. **段 2（選択と群の量）**: 原子列を命令の列（split / jmp / save）に組み直し、評価器の状態を命令の番号に、`VimPatternCapture` の升を（命令・位置）に。`\%(\)` も。`VimPatternGroup{first,end}` は save 命令に置き換わる。
3. **段 3（`\zs` `\ze`）**: save 命令 2 つ。検索の着地・`n` の数え・`hlsearch` の塗り・`:s` の範囲が `\zs`〜`\ze` を使う。
4. **段 4（行またぎ）**: 行の供給を借りる評価。`\n` を含みうるパターンだけ。`:s` のつなぐ編集と検索の着地を先に、`hlsearch` の上への探しは予算つきで。
5. **段 5（`\v` `\V` `\M` `\m`）**: 字句の段。段 1〜3 の後ならどれも写すだけ。
6. **段 6（番地と `|`）**: 照合器の外。段 1 と並べて先に入れてもよい（照合器に依らない）。

順の理由: hide の「`:s` が無いのは不思議」の流れで毎日打つ形は `\+`（段 1）・`\|`（段 2）・`\n`（段 4）・`\v`（段 5）。段 1 は今の作りの延長で小さく、段 2 の土台にもなる。

---

## 5. 設計席が決めるべき点

1. 段 2 で B に移るか（評価器の状態を命令の番号に一般化する ADR 0093 の追補か、新しい ADR か）。
2. 命令数（写しを開いた後）の上限の値と、超えたときの失敗（Vim の E363 に寄せた `too_complex` を検索でも使うか）。
3. 群の範囲を B1（候補ごとの印）にするか B2（今の BitState の一般化・`\1`〜`\9` を読む `:s` だけ）にするか。`\zs` `\ze` だけ B1 にする混ぜ方の可否。
4. 行またぎの `hlsearch` の上への探しの予算（行数・バイト）と、予算に当たったときに塗らないことを残差として書くか。
5. 行またぎの literal（`,\n`）に `VimSearchWalk` の速道を広げるか（最初は使わない・後続）。
6. `^\+a` `\<\+a`（Vim は通る）を Nib は断るか。`a**` を E871 に揃える（今の振る舞いを変える）か。
7. 行またぎの `:s` の後のキャレットの桁（§2.4・`do_sub` を読んで規則を決めるか、fixture に任せるか）。
8. 選択の literal の組（`foo\|bar`）を行の絞り込みに使う拡張の要否。
9. 段 6 の `|` の後の命令: 失敗で打ち切る・`e` で続ける（実測どおり）を Ex の 1 本（`|` を割る所）に置くか、`:s` だけにするか。

### fixture で測れる形と測れない形

- **測れる**（oracle の `:normal!` と `:s` の鍵・本文・キャレット・レジスタ・viewport）: 量・選択・`\zs` `\ze` の `n` の着地と `:s` の結果・`d/pat\zs`・行またぎの `:s` の本文とキャレット・`/a\nb` の着地・`\v` `\V` `\M`・番地 `:/pat/s` `:.,/pat/s` `:\/s`・`:s/a/b/ | s/c/d/`・`:s/…/gn` の後の本文（数は知らせなので本文と印だけ）。本文が変わらない失敗（E554 / E866 / E871）は「本文が変わらず、続きの鍵が打ち切られる」で測れる（打ち切りの形は #230 の規則）。
- **測れない**（unit と性質の試験で守る）: `hlsearch` と incsearch の塗りの範囲（`\zs` `\ze`・行またぎの塗り）・知らせの文言（E 番号は Vim の文と Nib の文が違う）・仕事量の上限と E363・後戻りの爆発の速さ（probe）・`:s///gn` の数の文言・Vim が通して Nib が断る残差（`^\+a`）。
- **注意**: fixture は 1 本の `:normal!` に流すので、`u` を含むなら変更が 1 つの鍵列で（CLAUDE.md の規則）。`|` を含む Ex は、`:normal!` が `|` を引数の一部として読む（`:h :normal`）ので鍵のまま流れる見込み（oracle の `KEY_TABLE` に `<Bar>` は無く、要らない見込み・fixture を 1 件測って確かめる）。

---

## 6. Vim 9.1 の実測の表（176 形 + 追加 8 形）

列: 「hits」は `search()` の `cW` → `W` で集めた当たり（行,桁）-（終わりの行,桁）・桁は 1 始まりのバイト・終わりは最後の文字の頭（空の当たりで終わりが (0,0) なのは `searchpos(…,'cenW')` の答えで、当たりは空）。「msp」は 1 行目への `matchstrpos` の [文字列@始まり:終わり]（0 始まりの半開区間・`\zs` `\ze` を反映）。「buf」は命令の後の本文（⏎ は行の切れ目）・「cur」はキャレット（行,桁）・「msg」は `execute()` の出力か `getpos('.')`・「ERR」は捕まえた例外。

| 名前 | 本文 | 入力 | 結果 |
|---|---|---|---|
| q-plus | `baaac a` | `a\+` | hits=(1,2)-(1,4) (1,7)-(1,7) ; msp=['aaa'@1:4] ['a'@6:7] |
| q-eq | `baaac` | `a\=` | hits=(1,1)-(1,2) (1,2)-(1,2) (1,3)-(1,3) (1,4)-(1,4) (1,5)-(0,0) ; msp=[''@0:0] ['a'@1:2] ['a'@2:3] ['a'@3:4] [''@4:4] [''@5:5] |
| q-qm | `baaac` | `a\?` | hits=(1,1)-(1,2) (1,2)-(1,2) (1,3)-(1,3) (1,4)-(1,4) (1,5)-(0,0) ; msp=[''@0:0] ['a'@1:2] ['a'@2:3] ['a'@3:4] [''@4:4] [''@5:5] |
| q-colour | `color colour colouur` | `colou\=r` | hits=(1,1)-(1,5) (1,7)-(1,12) ; msp=['color'@0:5] ['colour'@6:12] |
| q-n | `a aa aaa aaaa` | `a\{2}` | hits=(1,3)-(1,4) (1,6)-(1,7) (1,10)-(1,11) (1,12)-(1,13) ; msp=['aa'@2:4] ['aa'@5:7] ['aa'@9:11] ['aa'@11:13] |
| q-n- | `a aa aaa aaaa` | `a\{2,}` | hits=(1,3)-(1,4) (1,6)-(1,8) (1,10)-(1,13) ; msp=['aa'@2:4] ['aaa'@5:8] ['aaaa'@9:13] |
| q--m | `a aa aaaa` | `a\{,3}` | hits=(1,1)-(1,1) (1,2)-(1,4) (1,3)-(1,4) (1,5)-(1,8) (1,6)-(1,8) (1,9)-(1,9) ; msp=['a'@0:1] [''@1:1] ['aa'@2:4] [''@4:4] ['aaa'@5:8] ['a'@8:9] [''@9:9] |
| q-n-m | `a aa aaa aaaa` | `a\{2,3}` | hits=(1,3)-(1,4) (1,6)-(1,8) (1,10)-(1,12) ; msp=['aa'@2:4] ['aaa'@5:8] ['aaa'@9:12] |
| q-empty-braces | `baab` | `a\{}` | hits=(1,1)-(1,3) (1,2)-(1,3) (1,4)-(0,0) ; msp=[''@0:0] ['aa'@1:3] [''@3:3] [''@4:4] |
| q-lazy | `aaa` | `a\{-}` | hits=(1,1)-(1,2) (1,2)-(1,3) (1,3)-(0,0) ; msp=[''@0:0] [''@1:1] [''@2:2] [''@3:3] |
| q-lazy-1 | `aaa` | `a\{-1,}` | hits=(1,1)-(1,1) (1,2)-(1,2) (1,3)-(1,3) ; msp=['a'@0:1] ['a'@1:2] ['a'@2:3] |
| q-lazy--3 | `aaa` | `a\{-,3}` | hits=(1,1)-(1,2) (1,2)-(1,3) (1,3)-(0,0) ; msp=[''@0:0] [''@1:1] [''@2:2] [''@3:3] |
| q-lazy-2-3 | `aaaaa` | `a\{-2,3}` | hits=(1,1)-(1,2) (1,3)-(1,4) ; msp=['aa'@0:2] ['aa'@2:4] |
| q-reversed | `aaaa` | `a\{3,1}` | hits=(1,1)-(1,3) (1,4)-(1,4) ; msp=['aaa'@0:3] ['a'@3:4] |
| q-zero | `ab` | `a\{0}b` | hits=(1,2)-(1,2) ; msp=['b'@1:2] |
| q-group-plus | `xababab y` | `\(ab\)\+` | hits=(1,2)-(1,7) ; msp=['ababab'@1:7] |
| q-set-plus | `ab 123 4` | `[0-9]\+` | hits=(1,4)-(1,6) (1,8)-(1,8) ; msp=['123'@3:6] ['4'@7:8] |
| q-dot-plus | `abc` | `.\+` | hits=(1,1)-(1,3) ; msp=['abc'@0:3] |
| q-dot-lazy | `abcabc` | `.\{-}c` | hits=(1,1)-(1,3) (1,4)-(1,6) ; msp=['abc'@0:3] ['abc'@3:6] |
| q-x-lazy-y | `xaybyc y` | `x.\{-}y` | hits=(1,1)-(1,3) ; msp=['xay'@0:3] |
| q-greedy-ab | `aXbYb` | `a.*b` | hits=(1,1)-(1,5) ; msp=['aXbYb'@0:5] |
| q-lazy-ab | `aXbYb` | `a.\{-}b` | hits=(1,1)-(1,3) ; msp=['aXb'@0:3] |
| q-trailing-ws | `foo   ` | `\s\+$` | hits=(1,4)-(1,6) ; msp=['   '@3:6] |
| q-wide | `いああああう` | `あ\+` | hits=(1,4)-(1,13) ; msp=['ああああ'@3:15] |
| q-composed-plus | `xééy` | `é\+` | hits=(1,2)-(1,5) ; msp=['éé'@1:7] |
| q-composed-base | `xéeey` | `e\+` | hits=(1,5)-(1,6) ; msp=['ee'@4:6] |
| q-dot-composed | `xééy` | `.\{2}` | hits=(1,1)-(1,2) (1,5)-(1,8) ; msp=['xé'@0:4] ['éy'@4:8] |
| q-group-star-empty | `b` | `\(a*\)\+` | hits=(1,1)-(0,0) ; msp=[''@0:0] [''@1:1] |
| q-group-star-empty-sub | `baa` | `X s/\(a*\)\+/[\1]/` | buf=[]baa ; cur=1,1 |
| q-alt-group-n | `ababa` | `\(a\\|b\)\{2,3}` | hits=(1,1)-(1,3) (1,4)-(1,5) ; msp=['aba'@0:3] ['ba'@3:5] |
| q-sub-plus | `a   b  c` | `X s/ \+/ /g` | buf=a b c ; cur=1,1 |
| q-sub-eq | `colour color` | `X s/colou\=r/X/g` | buf=X X ; cur=1,1 |
| q-sub-empty-g | `a12b` | `X s/\d*/X/g` | buf=XaXb ; cur=1,1 |
| q-sub-lazy-g | `aaa` | `X s/a\{-1,}/X/g` | buf=XXX ; cur=1,1 |
| q-sub-lazy-empty-g | `aaa` | `X s/a\{-}/X/g` | buf=XaXaXa ; cur=1,1 |
| q-capture-last-iter | `abcabd` | `X s/\(ab\(.\)\)\+/[\1\|\2]/` | buf=[abd\|d] ; cur=1,1 |
| q-err-open | `a` | `X s/a\{/X/` | buf=a ; cur=1,1 ; ERR Vim(substitute):E554: Syntax error in \{...} |
| q-err-bad | `a` | `X s/a\{x}/X/` | buf=a ; cur=1,1 ; ERR Vim(substitute):E554: Syntax error in \{...} |
| q-err-three | `a` | `X s/a\{1,2,3}/X/` | buf=a ; cur=1,1 ; ERR Vim(substitute):E554: Syntax error in \{...} |
| q-err-start-plus | `+a` | `X s/\+a/X/` | buf=+a ; cur=1,1 ; ERR Vim(substitute):E866: (NFA regexp) Misplaced + |
| q-err-start-brace | `a` | `X s/\{2}a/X/` | buf=a ; cur=1,1 ; ERR Vim(substitute):E866: (NFA regexp) Misplaced { |
| q-err-double | `aa` | `X s/a\+\+/X/` | buf=aa ; cur=1,1 ; ERR Vim(substitute):E871: (NFA regexp) Can't have a multi follow a multi |
| q-err-star-plus | `aa` | `X s/a*\+/X/` | buf=aa ; cur=1,1 ; ERR Vim(substitute):E871: (NFA regexp) Can't have a multi follow a multi |
| q-double-star | `a*a` | `a**` | hits= ; msp= ; ERR Vim(let):E871: (NFA regexp) Can't have a multi follow a multi |
| q-err-after-anchor | `a` | `X s/^\+a/X/` | buf=X ; cur=1,1 |
| q-after-word-start | `ab` | `X s/\<\+a/X/` | buf=Xb ; cur=1,1 |
| q-backtrack-blowup | `aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa` | `\(a*\)*b` | hits= ; msp= |
| q-backtrack-blowup2 | `aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa` | `\(a\\|aa\)\+b` | hits= ; msp= |
| b-simple | `xbar foo` | `foo\\|bar` | hits=(1,2)-(1,4) (1,6)-(1,8) ; msp=['bar'@1:4] ['foo'@5:8] |
| b-group-tail | `barbaz foobaz` | `\(foo\\|bar\)baz` | hits=(1,1)-(1,6) (1,8)-(1,13) ; msp=['barbaz'@0:6] ['foobaz'@7:13] |
| b-empty-first | `ba` | `\\|a` | hits=(1,1)-(1,2) (1,2)-(0,0) ; msp=[''@0:0] [''@1:1] [''@2:2] |
| b-empty-last | `ba` | `a\\|` | hits=(1,1)-(1,2) (1,2)-(1,2) ; msp=[''@0:0] ['a'@1:2] [''@2:2] |
| b-first-wins | `abc` | `a\\|ab` | hits=(1,1)-(1,1) ; msp=['a'@0:1] |
| b-first-wins2 | `abc` | `ab\\|a` | hits=(1,1)-(1,2) ; msp=['ab'@0:2] |
| b-backtrack-into-branch | `abc` | `\(a\\|ab\)c` | hits=(1,1)-(1,3) ; msp=['abc'@0:3] |
| b-posix-test | `abcd` | `X s/\(a\\|ab\)\(c\\|bcd\)\(d*\)/[\1\|\2\|\3]/` | buf=[a\|bcd\|] ; cur=1,1 |
| b-anchors | `cab` | `^a\\|b$` | hits=(1,3)-(1,3) ; msp=['b'@2:3] |
| b-anchors2 | `acb` | `^a\\|b$` | hits=(1,1)-(1,1) (1,3)-(1,3) ; msp=['a'@0:1] ['b'@2:3] |
| b-star-group | `xababy` | `x\(a\\|b\)*y` | hits=(1,1)-(1,6) ; msp=['xababy'@0:6] |
| b-noncapture | `abba c` | `\%(a\\|b\)\+` | hits=(1,1)-(1,4) ; msp=['abba'@0:4] |
| b-sub-g | `foo bar baz` | `X s/foo\\|bar/X/g` | buf=X X baz ; cur=1,1 |
| b-three | `c b a` | `a\\|b\\|c` | hits=(1,1)-(1,1) (1,3)-(1,3) (1,5)-(1,5) ; msp=['c'@0:1] ['b'@2:3] ['a'@4:5] |
| b-nested | `ace bce de` | `\(\(a\\|b\)c\\|d\)e` | hits=(1,1)-(1,3) (1,5)-(1,7) (1,9)-(1,10) ; msp=['ace'@0:3] ['bce'@4:7] ['de'@8:10] |
| b-groups-unset | `ab` | `X s/\(a\)\\|\(b\)/[\1\2]/g` | buf=[a][b] ; cur=1,1 |
| b-leftmost-before-branch | `ba` | `a\\|b` | hits=(1,1)-(1,1) (1,2)-(1,2) ; msp=['b'@0:1] ['a'@1:2] |
| b-caret-mid | `a^b` | `x\\|^b\\|\^b` | hits=(1,2)-(1,3) ; msp=['^b'@1:3] |
| b-dollar-mid | `a$b` | `a$\\|$b` | hits=(1,2)-(1,3) ; msp=['$b'@1:3] |
| b-dollar-branch-end | `ab` | `a$\\|b$` | hits=(1,2)-(1,2) ; msp=['b'@1:2] |
| b-caret-after-branch | `ba` | `x\\|^b` | hits=(1,1)-(1,1) ; msp=['b'@0:1] |
| z-zs | `foobar` | `foo\zsbar` | hits=(1,4)-(1,6) ; msp=['bar'@3:6] |
| z-ze | `foobar` | `foo\zebar` | hits=(1,1)-(1,3) ; msp=['foo'@0:3] |
| z-both | `foobarbaz` | `foo\zsbar\zebaz` | hits=(1,4)-(1,6) ; msp=['bar'@3:6] |
| z-sub-zs | `foobar` | `X s/foo\zsbar//` | buf=foo ; cur=1,1 |
| z-sub-ze | `foobar` | `X s/foo\zebar/X/` | buf=Xbar ; cur=1,1 |
| z-n-landing | `foobar⏎xfoobar` | `exe "normal! /foo\\zsbar\<CR>n" ; let g:o.msg = string(getpos('.'))` | buf=foobar⏎xfoobar ; cur=2,5 ; msg=[0, 2, 5, 0] |
| z-multi-zs | `abc` | `a\zsb\zsc` | hits=(1,3)-(1,3) ; msp=['c'@2:3] |
| z-branch-zs | `cd ab` | `a\zsb\\|cd` | hits=(1,1)-(1,2) (1,5)-(1,5) ; msp=['cd'@0:2] ['b'@4:5] |
| z-sub-g | `abab` | `X s/a\zsb/X/g` | buf=aXaX ; cur=1,1 |
| z-sub-dot-zs-g | `abc` | `X s/.\zs/-/g` | buf=a-b-c- ; cur=1,1 |
| z-sub-ze-empty-g | `aaa` | `X s/a\ze/-/g` | buf=--- ; cur=1,1 |
| z-group-capture | `xab` | `X s/\(a\zsb\)/[\1]/` | buf=xa[ab] ; cur=1,1 |
| z-amp | `foobar` | `X s/foo\zsbar/<&>/` | buf=foo<bar> ; cur=1,1 |
| z-zs-in-star-group | `aab` | `\(a\zs\)*b` | hits=(1,3)-(1,3) ; msp=['b'@2:3] |
| z-ze-before-zs | `abc` | `a\zeb\zsc` | hits=(1,3)-(1,3) ; msp=[''@2:2] |
| z-count-n | `foobar foobar` | `X s/foo\zsbar//gn` | buf=foobar foobar ; cur=1,1 ; msg=2 matches on 1 line |
| z-dw | `foobar end` | `exe "normal! d/foo\\zsbar\<CR>"` | buf=bar end ; cur=1,1 |
| n-search | `xa⏎by` | `a\nb` | hits=(1,2)-(2,1) ; msp= |
| n-join-all | `a⏎b⏎c` | `X %s/\n//` | buf=abc ; cur=1,3 |
| n-join-all-g | `a⏎b⏎c` | `X %s/\n//g` | buf=abc ; cur=1,3 |
| n-comma | `a,⏎b,⏎c` | `X %s/,\n/, /` | buf=a, b, c ; cur=1,1 |
| n-last-line | `a⏎b` | `X $s/\n//` | buf=a⏎b ; cur=2,1 |
| n-last-line-all | `a⏎b` | `X %s/b\n/X/` | buf=a⏎X ; cur=2,1 |
| n-ws | `a ⏎  b⏎⏎c` | `X %s/\_s\+/ /g` | buf=a b c  ; cur=1,1 |
| n-dot-under | `a1⏎2z` | `a\_.*z` | hits=(1,1)-(2,2) ; msp= |
| n-set-under | `ab⏎ba c` | `\_[ab]\+` | hits=(1,1)-(2,2) (2,1)-(2,2) (2,5)-(2,5) ; msp=['ab'@0:2] |
| n-rest-range | `a⏎ba⏎b⏎c` | `X %s/a\nb/X/g` | buf=XX⏎c ; cur=1,1 |
| n-count-n | `a⏎b⏎c` | `X %s/\n//gn` | buf=a⏎b⏎c ; cur=1,1 ; msg=3 matches on 3 lines |
| n-three-lines | `a⏎⏎⏎b` | `X %s/a\n\n/X/` | buf=X⏎b ; cur=1,1 |
| n-three-lines-match | `xa⏎b⏎cy` | `a\nb\nc` | hits=(1,2)-(3,1) ; msp= |
| n-eol-search | `abc⏎d` | `\n` | hits=(1,4)-(1,4) (2,2)-(2,2) ; msp= |
| n-range-1 | `a⏎b⏎c⏎d` | `X 1s/\n//` | buf=ab⏎c⏎d ; cur=1,2 |
| n-range-2-3 | `a⏎b⏎c⏎d` | `X 2,3s/\n//` | buf=a⏎bcd ; cur=2,3 |
| n-count | `a⏎b⏎c⏎d` | `X s/\n// 3` | buf=abcd ; cur=1,1 |
| n-insert-break | `a⏎b` | `X %s/a\n/X\r/` | buf=X⏎b ; cur=2,1 |
| n-double | `a⏎b` | `X 1s/\n/\r\r/` | buf=a⏎⏎b ; cur=3,1 |
| n-search-n | `ab⏎b⏎ab` | `exe "normal! /\\nb\<CR>" ; let g:o.msg = string(getpos('.'))` | buf=ab⏎b⏎ab ; cur=1,2 ; msg=[0, 1, 2, 0] |
| n-search-N | `ab⏎b⏎ab` | `normal! G ; exe "normal! ?\\nb\<CR>" ; let g:o.msg = string(getpos('.'))` | buf=ab⏎b⏎ab ; cur=1,2 ; msg=[0, 1, 2, 0] |
| n-dollar-n | `a⏎b` | `a$\n^b` | hits=(1,1)-(2,1) ; msp= |
| n-not-in-dot | `a⏎b` | `a.b` | hits= ; msp= |
| n-neg-set | `a⏎b` | `a[^x]b` | hits= ; msp= |
| n-under-neg-set | `a⏎b` | `a\_[^x]b` | hits=(1,1)-(2,1) ; msp= |
| n-end-of-file | `a⏎b` | `b\n` | hits=(2,1)-(2,2) ; msp= |
| n-join-emptylines | `a⏎⏎⏎b` | `X %s/\n\n\+/\r/` | buf=a⏎b ; cur=2,1 |
| n-cursor-after | `a,⏎b,⏎c⏎d` | `X %s/,\n/ /` | buf=a b c⏎d ; cur=1,1 |
| n-zs-newline | `a⏎b` | `X %s/a\zs\n//` | buf=ab ; cur=1,1 |
| n-hits-overlap | `a⏎a⏎a` | `a\na` | hits=(1,1)-(2,1) (2,1)-(3,1) ; msp= |
| v-sub | `foobarfoo x` | `X %s/\v(foo\|bar)+/x/g` | buf=x x ; cur=1,1 |
| v-plus | `baaac` | `\va+` | hits=(1,2)-(1,4) ; msp=['aaa'@1:4] |
| v-V | `axb a.b` | `\Va.b` | hits=(1,5)-(1,7) ; msp=['a.b'@4:7] |
| v-M | `aa a*` | `\Ma*` | hits=(1,4)-(1,5) ; msp=['a*'@3:5] |
| v-word | `foo food` | `\v<foo>` | hits=(1,1)-(1,3) ; msp=['foo'@0:3] |
| v-brace | `a aa aaa` | `\va{2,}` | hits=(1,3)-(1,4) (1,6)-(1,8) ; msp=['aa'@2:4] ['aaa'@5:8] |
| v-eq | `color colour` | `\vcolou?r` | hits=(1,1)-(1,5) (1,7)-(1,12) ; msp=['color'@0:5] ['colour'@6:12] |
| v-switch-mid | `abbb` | `a\vb+` | hits=(1,1)-(1,4) ; msp=['abbb'@0:4] |
| v-V-escape | `a.b axb` | `\Va\.b` | hits=(1,1)-(1,3) (1,5)-(1,7) ; msp=['a.b'@0:3] ['axb'@4:7] |
| v-literal-eq | `a=b` | `\va\=b` | hits=(1,1)-(1,3) ; msp=['a=b'@0:3] |
| a-fwd | `xa⏎b⏎xa⏎xa` | `X /xa/s/a/X/` | buf=xa⏎b⏎xX⏎xa ; cur=3,1 |
| a-range | `a⏎a⏎b⏎a` | `X .,/b/s/a/X/` | buf=X⏎X⏎b⏎a ; cur=2,1 |
| a-two | `x⏎a⏎x⏎b⏎x` | `X /a/,/b/s/x/Y/` | buf=x⏎a⏎Y⏎b⏎x ; cur=3,1 |
| a-back | `xa⏎b⏎xa` | `normal! G ; X ?xa?s/a/X/` | buf=xX⏎b⏎xa ; cur=1,1 |
| a-semicolon | `a⏎b⏎c⏎b⏎c` | `X /b/;/c/s/$/!/` | buf=a⏎b!⏎c!⏎b⏎c ; cur=3,1 |
| a-offset | `a⏎b⏎c` | `X /b/+1s/$/!/` | buf=a⏎b⏎c! ; cur=3,1 |
| a-not-found | `a⏎b` | `X /zz/s/a/X/` | buf=a⏎b ; cur=1,1 ; ERR Vim(substitute):E486: Pattern not found: zz |
| a-wrap | `xa⏎b⏎c` | `normal! G ; X /xa/s/a/X/` | buf=xX⏎b⏎c ; cur=1,1 |
| a-slash-prev | `a⏎b⏎b` | `exe "normal! /b\<CR>" ; normal! gg ; X \/s/b/X/` | buf=a⏎X⏎b ; cur=2,1 |
| a-qm-prev | `b⏎a⏎b` | `exe "normal! /b\<CR>" ; normal! G ; X \?s/b/X/` | buf=X⏎a⏎b ; cur=1,1 |
| a-amp-prev | `b⏎a⏎b` | `X s/b/Y/ ; X \&s/b/X/` | buf=Y⏎a⏎X ; cur=3,1 |
| a-pattern-with-branch | `a⏎foo⏎bar` | `X /foo\\|bar/s/$/!/` | buf=a⏎foo!⏎bar ; cur=2,1 |
| a-alone | `a⏎b⏎c` | `X /c/ ; let g:o.msg = string(getpos('.'))` | buf=a⏎b⏎c ; cur=3,1 ; msg=[0, 3, 1, 0] |
| a-current-line-skip | `xa⏎xa` | `X /xa/s/a/X/` | buf=xa⏎xX ; cur=2,1 |
| p-two | `ac` | `X s/a/b/ \| s/c/d/` | buf=bd ; cur=1,1 |
| p-trailing | `ac` | `X s/a/b/\|` | buf=bc ; cur=1,1 |
| p-in-pattern | `a\|c` | `X s/a\|c/X/` | buf=X ; cur=1,1 |
| p-in-replacement | `ac` | `X s/a/b\|c/` | buf=b\|cc ; cur=1,1 |
| p-branch | `ac` | `X s/a\\|c/X/g` | buf=XX ; cur=1,1 |
| p-flags | `aacc` | `X s/a/b/g\|s/c/d/g` | buf=bbdd ; cur=1,1 |
| p-percent | `a⏎a` | `X %s/a/b/\|%s/b/c/` | buf=c⏎c ; cur=2,1 |
| p-normal | `abc` | `X s/a/X/ \| normal! x` | buf=bc ; cur=1,1 |
| p-after-count | `a⏎a⏎a` | `X s/a/X/ 2 \| s/X/Y/` | buf=X⏎Y⏎a ; cur=2,1 |
| p-error-first | `ab` | `X s/z/X/ \| s/b/Y/` | buf=ab ; cur=1,1 ; ERR Vim(substitute):E486: Pattern not found: z |
| p-error-first-e | `ab` | `X s/z/X/e \| s/b/Y/` | buf=aY ; cur=1,1 |
| f-open-group | `a` | `X s/\(a/X/` | buf=a ; cur=1,1 ; ERR Vim(substitute):E54: Unmatched \( |
| f-close-group | `a` | `X s/a\)/X/` | buf=a ; cur=1,1 ; ERR Vim(substitute):E55: Unmatched \) |
| f-v-open | `a` | `X s/\v(a/X/` | buf=a ; cur=1,1 ; ERR Vim(substitute):E54: Unmatched ( |
| f-at | `a` | `X s/a\@/X/` | buf=a ; cur=1,1 ; ERR Vim(substitute):E869: (NFA regexp) Unknown operator '\@ |
| f-lazy-open | `a` | `X s/a\{-/X/` | buf=a ; cur=1,1 ; ERR Vim(substitute):E554: Syntax error in \{...} |
| f-brace-comma | `aaa` | `a\{,}` | hits=(1,1)-(1,3) ; msp=['aaa'@0:3] [''@3:3] |
| f-zs-only | `abc` | `X s/\zs/X/g` | buf=XaXbXc ; cur=1,1 |
| f-nested-multi | `aa` | `X s/a\{1}\{2}/X/` | buf=aa ; cur=1,1 ; ERR Vim(substitute):E871: (NFA regexp) Can't have a multi follow a multi |
| f-zs-twice-group | `ab` | `X s/\(\zs\)\+a/X/` | buf=Xb ; cur=1,1 |
| f-branch-trailing-close | `a` | `X s/a\\|\)/X/` | buf=a ; cur=1,1 ; ERR Vim(substitute):E55: Unmatched \) |
| n-set-with-newline | `ab⏎ba c` | `[ab\n]\+` | hits=(1,1)-(2,2) (2,1)-(2,2) (2,5)-(2,5) ; msp=['ab'@0:2] |
| n-sub-set-newline | `a⏎b⏎c` | `X %s/[b\n]//g` | buf=a⏎c ; cur=2,1 ; msg=3 substitutions on 2 lines |
| n-g-two-matches-joined | `a,⏎b,⏎c` | `X 1s/,\n/;/g` | buf=a;b,⏎c ; cur=1,1 |
| n-search-count | `a⏎b⏎a⏎b` | `exe "normal! 2/a\\nb\<CR>" ; let g:o.msg = string(getpos('.'))` | buf=a⏎b⏎a⏎b ; cur=1,1 ; msg=[0, 1, 1, 0] |
| n-dw-across | `xa⏎by` | `normal! l ; exe "normal! d/\\nb\<CR>"` | buf=xa⏎by ; cur=1,2 |
| n-star-newline | `a⏎⏎⏎b` | `X %s/\n*b/X/` | buf=aX ; cur=1,1 |
| v-sub-trailing | `a  ⏎b ` | `X %s/\v\s+$//` | buf=a⏎b ; cur=2,1 |
| v-sub-swap | `foo bar` | `X s/\v(\w+) (\w+)/\2 \1/` | buf=bar foo ; cur=1,1 |
| b-word-branch | `foo food bar` | `X s/\<\(foo\\|bar\)\>/X/g` | buf=X food X ; cur=1,1 |
| z-n-count-zs | `ab ab ab` | `exe "normal! 2/a\\zsb\<CR>" ; let g:o.msg = string(getpos('.'))` | buf=ab ab ab ; cur=1,5 ; msg=[0, 1, 5, 0] |
| q-lazy-group-capture | `<a><b>` | `X s/<\(.\{-}\)>/[\1]/g` | buf=[a][b] ; cur=1,1 |
| q-plus-capture | `key   =  value` | `X s/\(\w\+\)\s*=\s*\(\w\+\)/\2=\1/` | buf=value=key ; cur=1,1 |

### 追加の 8 形（`cases_extra.py`）

| 名前 | 本文 | 入力 | 結果 |
|---|---|---|---|
| x-double-star-search | `xa*a` | `exe "normal! /a**\<CR>" ; let g:o.msg = string(getpos('.'))` | buf=xa*a ; cur=1,1 ; ERR Vim(normal):E871: (NFA regexp) Can't have a multi follow a multi |
| x-caret-star | `*a` | `^*` | hits=(1,1)-(1,1) ; msp=['*'@0:1] |
| x-double-star-sub | `a*a` | `X s/a**/X/` | buf=a*a ; cur=1,1 ; ERR Vim(substitute):E871: (NFA regexp) Can't have a multi follow a multi |
| 名前 | 本文 | 入力 | 結果 |
|---|---|---|---|
| x-under-set-sub | `a⏎b⏎c` | `X %s/\_[b]//g` | buf=a⏎c ; cur=2,1 ; msg=3 substitutions on 2 lines |
| x-set-nl-hits | `a⏎b⏎c` | `[b\n]` | hits=(1,2)-(1,2) (2,1)-(2,1) (3,2)-(3,2) ; msp= |
| x-nl-sub-g | `a⏎b⏎c` | `X %s/\n//g` | buf=abc ; cur=1,3 |
| x-join-cursor-2 | `a⏎b⏎c⏎d` | `X 1,2s/\n//` | buf=abc⏎d ; cur=1,3 |
| x-big-count | `aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa` | `a\{30}` | hits=(1,1)-(1,30) ; msp=['aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'@0:30] |
