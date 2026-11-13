# ADR 0012 — Vim エンジンは core の純関数 `vim_step` で、鍵と効果は閉じた和型、再現度は oracle が生成した fixture の再生で守る

- 状態: 受理
- 日付: 2026-09-16
- Issue: #22
- 影響する規則: ARC-001 / ARC-003 / ARC-004 / ARC-007 / ARC-011 / CPP-002 / CPP-006 / CPP-011 / CPP-012 / QLT-008 / QLT-009 / QLT-013

## 文脈

ADR 0005 は「Vim は自前実装・本物の Vim との差分テストで再現度を担保」と決めたが、エンジンの形・鍵の表現・fixture の形式は決めておらず、fixture は 0 本、強制は planned のまま。
編集の縦切り（ADR 0009）で本文は `TextBuffer`（piece table）、意図は閉じた `std::variant`、undo は `EditHistory` と `EditBoundary`、キャレットの移動は純関数 `moved_caret` になった。Vim モードへのトグル（`EditMode::vim`）は在るが、
鍵は 1 つも効かず、`ModeLabel` は NORMAL 固定、`CancelSelection` は vim では何もしない。
Phase 0 の V2 で、Vim 9.1 を `-u NONE -i NONE -N -n -es -S probe.vim` で走らせると本文・カーソル・レジスタが決定的に取れることは実測済み（`docs/quality/phase0-results.json`）。

## 決定

**Vim の状態は core の値型 `VimState`、エンジンは純関数 `vim_step(VimState, TextBuffer, Offset, VimKey) → {VimState, VimEffect}`。鍵 `VimKey` と効果 `VimEffect` は閉じた `std::variant`、NORMAL の鍵 → 動作は `constexpr` の表。
再現度は `eng/vim-oracle.py` が本物の Vim から生成した `tests/vim/VimFixtures.hpp` を単体テストが再生することで守る。**

1. **`VimState`（core・値型）**: `VimMode { normal, insert }`・回数の前置（`std::optional<Count>`。0 は無い）・保留中のオペレータ（`std::optional<VimOperator>`。この縦切りは `remove` の 1 つ）・
   欲しい列（Vim の `curswant`。`j` `k` が短い行を通っても戻る。`$` は「行末」を貼り付ける）・無名レジスタ（`x` `d` が書く。`p` は次の縦切り）。本文・キャレット・履歴は持たない（所有は `EditorState`・ARC-004）
2. **`vim_step` は純関数**（ARC-007）。時刻・OS・スレッドを持たず、`eng/symbols.py` が core から出るシンボルで見る。`TextBuffer` と現在の `Offset` を読み、次の `VimState` と 1 つの `VimEffect` を返す。
   鍵のあと NORMAL なら controller がキャレットを文字の上へ寄せる（`vim_resting_caret`）。クリック（`PlaceCaret`）と Ctrl+矢印（`MoveCaret`）のあとも同じに寄せる（Vim も行末より右のクリックは最後の文字に置く）。
   語の移動 `w` `b` は Vim の `fwd_word` / `bck_word` と同じ「文字の種類」で切る別の純関数（`VimWordMotion`）。通常モードの `moved_caret`（空白で切る）は変えない
3. **`VimEffect` は閉じた `std::variant`**（8 つ）: `VimNoEffect` / `VimMoveTo{Offset}` / `VimRemoveRange{OffsetRange}` / `VimRemoveLines{OffsetRange}` / `VimInsertString{std::string}` / `VimNewLine` / `VimUndo` / `VimRedo`。
   `VimRemoveLines` は行単位の削除（`dd` `dj` `dk`）で、範囲は文字単位と同じ形でもキャレットの行き先が「上がってきた行の最初の非空白」と違い、無名レジスタには「行＋改行」が入る。controller に「行だったか」を推測させない。
   `VimNewLine` は「改行」とだけ言い、形（CRLF / LF）は `EditorState::line_ending()` が決める（ARC-009。`"
"` の挿入を controller が置き換える形は改行の正典を 2 つにする）。`EditorController::accept(const VimKeyPress&)` が `std::visit` で既存の
   `move_caret_to` / `replace` / `undo_edit` / `redo_edit` に写す。写し先が増えたらコンパイルが落ちる（CPP-002 / ADR 0009 の決定 9）
4. **`VimKey` は閉じた `std::variant<Character, Special>`**（`Character{char32_t}`・`Special { escape, enter, backspace, arrow_left, arrow_right, arrow_up, arrow_down, control_r }`）。窓は Vim モードのとき `WM_CHAR` / `WM_KEYDOWN` を
   `VimKey` に写して `VimKeyPress{VimKey}` を出す（`EditorIntent` に 1 つ足す）。通常モードの経路と、両モード共通の Ctrl+S / Ctrl+O / Ctrl+Shift+S は変えない
5. **NORMAL の鍵 → 動作は `constexpr` の表**（CPP-012 / ADR 0006・T8: 60 分岐の `switch` は関数長で落ちる）。範囲は移動 `h j k l 0 $ w b`・回数・`x`・`d`＋移動・`dd`・`i a I A`・`u`・Ctrl-r・Esc。
   NORMAL のキャレットは文字の上（行末を越えない・空行だけ行頭）。INSERT から Esc で 1 つ左（Vim と同じ）
6. **undo の区切り**: `i` `a` `I` `A` で INSERT に入るとき `EditHistory::sealed()`、Esc で出るときも `sealed()`。INSERT の中の文字・Enter・Backspace は 1 つの単位（`coalesce`）。通常モードの区切り方（ADR 0009 の決定 3）は変えない
7. **fixture は `tests/vim/fixtures.json` 1 本**（`name` / `text` / `keys` / `settings`）。`keys` は `ihello<Esc>3jx` の書き方（`<Esc>` `<CR>` `<BS>` `<C-r>`）。
   `eng/vim-oracle.py --regenerate` が `C:\Program Files\Vim\vim91\vim.exe` を V2 と同じ引数で走らせ、本文（`getline(1,'$')`）・カーソル（`line('.')`, `col('.')`。`col` はバイトなので UTF-8 の `Offset` と一致する）・無名レジスタ（`getreg('"')`）を
   **生成物 `tests/vim/VimFixtures.hpp`**（`constexpr` の配列）に書く。単体テストは header を再生するだけで Vim を要らない。生成物はリポジトリに入れ、再生成は Vim のある機械だけ（ADR 0005）。Vim の版は `eng/tool-versions.json` の `"vim"`
8. **oracle の設定は fixture の入力の一部**。既定は `set nocompatible` と `set backspace=indent,eol,start`。後者は `defaults.vim` が入れる値で、これが無い `-u NONE` の Vim は INSERT の Backspace が挿入開始位置より前を消せず、
   利用者が触る「既定の Vim」と違う。`defaults.vim` そのものは読まない（`syntax on` や `filetype` は本文の結果に関係なく、読み込む範囲が版で変わる）。
   `set encoding=utf-8` は足していない: この Vim 9.1（Windows 版）は `-u NONE` でも `&encoding` が `utf-8` だと実測した（gate-proofs 5-g）。oracle は `probe.vim` の先頭で `call cursor(1, 1)` する（Ex モードの開始位置は先頭ではない）
9. **fixture の本文は LF だけ**。Vim は CRLF を `fileformat=dos` と判別して CR を落とすので oracle に流せない。CRLF の本文（`$` `x` `dd` が CR を残さない）は手書きの単体テスト 1 本で守る
10. **見た目は採用案のまま**（D15）: `mode_label(EditMode, VimMode)` が NORMAL / INSERT、`CaretView::shape` が NORMAL でブロック・INSERT でバー。トグルで Vim に入ると NORMAL（回数・オペレータは空）で選択を畳み、通常に戻ると保留は捨てる。（追補 2026-10-22・#785: 窓の寸法が届いた後のトグルは画面を動かさない。キャレットが見えている範囲の外なら、外れた側の端の画面行へ今の横の位置のまま入れてから行末を寄せ、追従は頼まない（`EditorController::caret_in_view`・位置は上下の移動と同じ `location` `shifted` `caret_at`）。通常に戻るときは `clamp_scroll` だけ。Vim で最後の行を先頭まで上げてから通常へ戻ると、通常の送れる端（`filled_viewport`）まで `clamp_scroll` が戻す（通常モードの規則・直さない）。）
11. **Vim モードの窓の鍵**: `WM_CHAR` は `VimCharacter`、Esc / Enter / Backspace / 矢印は `VimSpecialKey` の表、Tab は INSERT の `	` として文字の鍵、Ctrl+R は `control_r`。Ctrl+Z / Ctrl+Y は Vim では送らず `u` / Ctrl-r に譲る。Ctrl+S / Ctrl+O / Ctrl+Shift+S・Ctrl+A/C/X/V・マウス・ホイールは両モード共通。Home / End / PgUp / PgDn は Vim モードではまだ効かない（次の縦切り）

## 強制

- ADR 0005 の「fixture の再生と差分は単体テスト（CTest）に載せ、QLT-009 の分岐カバレッジの対象にする」— **active**（この Issue で。`nib_unit` が `VimFixtures.hpp` の全項目を再生する）
- oracle の実行（`--regenerate` を 2 回して生成物が一致）— QLT-013 の環境依存の確認として `docs/quality/gate-proofs.md` 5 節に記録（CI に Vim は無い）
- ARC-007 / CPP-013: `src/core` から時刻・OS のシンボルが出ない — **active**（既存の `eng/symbols.py`）
- CPP-002: `VimMode` / `Special` / `VimOperator` の `switch` に `default` を書かない・`std::visit` の写し先を欠かさない — **active**（既存の clang-tidy とコンパイル）
- fixture の再生が無名レジスタと Vim のモードを突き合わせるため、`EditorController` に読み出し専用の `vim_state()` が 1 つある（状態を変える口は足していない）
- 「生成物 `VimFixtures.hpp` が `fixtures.json` と一致しているか」— **active**（2026-09-18・Issue #44）。`eng/vim-oracle.py` が生成物の先頭に `fixtures.json` の SHA-256 と本数を 1 行で書き、
  `eng/conformance.py` の CNF-010 がその行と実際の `fixtures.json` のバイト列・配列の長さを突き合わせる。Vim を要らないので CI でも落ちる。突き合わせるのは「生成物がこの json から作られたか」だけで、期待値が本物の Vim の答えかは Vim のある機械での `--regenerate`（QLT-013）が見る

## 結果

得られるもの: Vim の鍵が効く最初の状態。T2 の範囲（`c y p > <`・テキストオブジェクト・VISUAL・`.`・レジスタ・検索）は表の行・`VimEffect` の選択肢・fixture を足す形で増える。再現度が「人の記憶」ではなく oracle との一致で決まる。
失うもの: `VimEffect` を 1 つずつ返すので、1 つの鍵で複数の効果（例: `cw` の削除＋INSERT）は次の縦切りで `VimEffect` を列にするか合成の効果を足す判断が要る（この縦切りの範囲では 1 つで足りる）。
正直に（Issue #22 の実測で分かった oracle と実装の限界）:

- `col('.')` はバイトなので多バイト文字でも `Offset` と一致するが、`j` `k` の欲しい列は code point で数えていて Vim の `virtcol`（全角・タブの表示幅）とは別。全角やタブを含む行をまたぐ `j` `k` は Vim とずれうる。矩形（`Ctrl-v`）の縦切りで `virtcol` に直す
- **`:normal!` の 1 回の実行はまるごと 1 つの undo の単位**になる（`xxu` は oracle では `hello` に戻り、対話の Vim では `ello`）。undo の fixture は「1 回の変更 → `u`」に限り、INSERT の出入りが単位を閉じることは手書きの単体テストで守る。INSERT 中の Backspace は `EditHistory` の coalesce が削除を伴う編集をつなげないので単位を割る（`ia<BS>b<Esc>u` は Vim と一致しない。次の Vim の Issue）
- `:normal!` は失敗した鍵のあとの鍵を捨てることがある（`hx` は `h` が行頭で失敗して `x` が効かない）。規則が読み切れないので fixture では失敗する鍵を列の最後にだけ置く。エンジンは「失敗した鍵は何もしない・次の鍵は効く」
- **回数の掛け算**（`2d3w` は Vim では 6 語）は `VimState` の `count` が 1 つなので 23 語になる。Issue の範囲（`3j` `2w` `2dw`）は一致。オペレータ側の回数は次の縦切りで足す
- 語の種類の表は Vim の `utf_class_tab` のうちラテン補助・一般句読点・CJK 記号・ひらがな・カタカナ・漢字・全角記号だけで、それ以外の非 ASCII は「語の文字」に落ちる。ハングル・絵文字は未測
- oracle は Vim 9.1（2024-01-02）に依存し、版を上げると fixture の差分として現れる。CI に Vim は無いので、CI が見られるのは「生成物がいまの `fixtures.json` から作られたか」（CNF-010）までで、期待値が本物の Vim の答えかは Vim のある機械でしか測れない

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| エンジンが `TextBuffer` を所有して編集後の本文を返す | 本文の所有者が 2 つになる（ARC-004）。効果を返して controller が既存の 1 本の経路で適用するほうが undo・保存の印・スクロール追従がそのまま効く |
| fixture を JSON のまま C++ のテストが読む | JSON パーサを test に持ち込む（依存を足す）。生成した `constexpr` の header は依存 0 で、`git diff` で期待値の変化も見える |
| `defaults.vim` を oracle に読ませる | 版で内容が変わり、`syntax on` など本文の結果に無関係な副作用が入る。本文の結果に効く 1 行だけを設定として明示する |
| 鍵を `std::string` の Vim 記法で受け取る | 開いた集合になり網羅性が守れない（CPP-002）。fixture の記法は oracle の道具と test の中だけで使い、製品の型は閉じた和型 |
| VISUAL まで最初の縦切りに含める | 選択の描き方は在るが、`v` の範囲の規則（inclusive）と `Ctrl-v` の全角・タブが難所。エンジンの形が固まってから fixture を足す |

## 追記（2026-10-21・Issue #767）

- 移動の表に `%`（`VimMotion::matching_pair`）を 1 行足した。回数なしは対応する括弧（inclusive の文字単位・'matchpairs' の既定 `(:),{:},[:]` を固定・引用符の内側を飛ばす Vim の既定）、`{N}%` は本文の N % の行（行単位・100 超は失敗）。マーク `'x` `` `x `` は `VimMotion::mark_line`（行単位）と `mark_exact`（exclusive）の 2 行で、名前は鍵を写す前に engine が解く。マークの所有と編集のずれの直し方は [ADR 0152](0152-vim-marks-are-document-values-adjusted-by-edits.md)。

## 追記（2026-10-29・Issue #1040）

INSERT の `Ctrl-w` `Ctrl-u` `Ctrl-r {name}` を足した。値は oracle（`backspace=indent,eol,start` は `DEFAULT_SETTINGS` が `-u NONE` の上に置く・`autoindent` は無い）の fixture `insert-control-` 43 件で守る。

- **始めの位置**: `VimState.insert_start`（Vim の `Insstart_orig`・行と行頭からのバイト数）。INSERT の最初の鍵の前に `vim_step` の入口（`insert_started`）がキャレットから置き、`vim_resting_from` と `input_discarded` が消す。同じ行の手前を消しても動かず（`A<BS>xy<C-w>` は `y` だけを消す）、動くのは 2 つだけ。始めの行を前の行とつないだときはつないだ点へ（`jI<C-w>x<C-w>` は `x` だけ）。矢印・Home などで動いた後の最初の移動でない鍵では、キャレットの桁が始めの桁以下なら（行は比べない）キャレットへ、右なら動かない（Vim の `stop_arrow` と `update_Insstart_orig`・`Axy<Left><Left><Left>z<C-w>` は `z` だけ・`Axy<Left>z<Right>…` や `jAxy<Up>z<C-u>` は動かない側）。
- **`Ctrl-w` `Ctrl-u`**: キャレットが始めの行で始めの桁より右ならそこ、ほかは行頭まで消す（少なくとも 1 文字）。`Ctrl-w` は手前の空白を消してから `b` と同じ表 `vim_character_class` の同じ種類の連なりで止まる。行頭では前の行と 1 鍵ぶんつなぐだけ（Backspace と同じ `joined_back`）。Issue の見込みの「2 段目は字下げの後」は `autoindent` のときだけで、既定の Vim では 2 段目が行頭（実測）。`.` の記録には鍵のまま残る。回数つきの入力（`3A…`）の反復は下の #1093 の追記（近似をやめ、鍵の列を流し直す）。
- **`Ctrl-r {name}`**: `Ctrl-r` は次の 1 鍵を待つ `VimPrefix::insert_register`。名前 → 選択は `register_selection_of`、読みは `register_read` の 1 本で、本文は `VimInsertAt` の 1 回（undo は INSERT の 1 単位）。名前にならない鍵・空のレジスタは何も入れず INSERT を続ける。`.` の記録は鍵 2 つではなく入れた本文の文字（Vim の redo も本文・`"ayiwA<C-r>a<Esc>j0"ayiw.` は `bazfoo`）。`"+` `"*` は名前の鍵の直前に controller が写しを置く既存の道（`vim_reads_clipboard`）。本文は文字として入れ、制御文字を鍵として解かない（Vim は打鍵として解く・差は後続）。`Ctrl-r Ctrl-r` `Ctrl-r Ctrl-o` `Ctrl-r Ctrl-p` と読み取り専用のレジスタ `.` `:` `/` `%` は後続。

## 追記（2026-10-29・Issue #1093）

#1040 の「回数つきの入力は記録の全部に届いた消し方なら反復をやめ、記録の中で止まれば記録を短くする」近似をやめ、Vim の規則に替えた。値は oracle の fixture `insert-count-control-` 27 件（Issue の 5 件・fuzz seed 32〜40 の 6 件・Backspace の 5 件・undo 3 件ほか）で守る。

- **反復は打鍵の列を流し直す 1 本**: Vim の `edit()` は Esc で回数が残れば `start_redo_ins()` で redo の打鍵（`Ctrl-w` `Ctrl-u` `<BS>` を含む）をそのまま流す。回数の記録 `VimInsertRepeat` を本文から鍵の列 `keys` に替え（o / O は改行の鍵 1 つから始まる・`Ctrl-r {name}` は入れた本文の文字）、Esc（`left_insert`）は残りの周で列を本文の写しの上で 1 鍵ずつ打った鍵と同じ `insert_character` / `insert_special` に通す。2 周目からの `Ctrl-u` は 1 周目が入れた本文を消し、行頭なら前の行とつなぐ（`backspace=eol`・`10G2A<C-u>` は 2 行目が改行ごと消える）。2 周目の `Ctrl-w` は 1 周目の語を消す（`3i <C-w>  .(` は空白 6）。始めの位置 `insert_start` は周をまたいで持ち越す（Vim の `Insstart_orig` も同じ INSERT のまま・つないだらつないだ点へ動くのも同じ）。`backspace` は oracle と同じ固定値で、設定にしない。
- **効果は 1 つ**: 流し直しで消した手前の端から Esc のキャレットまでを、流し直した本文（LF で数えて controller が文書の改行へ直す）に置き換える `VimReplaceRange` を 1 つ返す。`VimReplaceRange` に履歴の境界 `boundary` を足し（`r` などの置換は今までどおり `separate`）、この置き換えは `absorb`。消す鍵を含まない列は、流し直しても本文の繰り返しと同じなので今までどおり `repeated` で 1 回に作る（`VimInsertAt`）。
- **INSERT の undo 1 単位を保つ**: 置き換えが単位の頭より前（前の行との改行）から単位が入れた本文の中までに及ぶので、`absorbed`（ADR 0015 の決定 5）の「挿入開始より前を消す削除」を「挿入開始より前から、入れた本文の中までの置き換え」に広げた（削除だけはその特別な形）。`j2i<BS><BS>x<Esc>u` と `jw2i<C-u><Esc>u` は 1 回の `u` で戻る（実測）。
- **何も消せない消す鍵は記録しない**: 本文の先頭（1 行目の行頭）の `<BS>` `Ctrl-w` `Ctrl-u` は Vim の `ins_bs` がビープで返り redo に足さないので、回数の鍵の列にも `.` の記録にも載せない（`3i<C-u>x` は `xxx`・`i<C-w>(日本foo<Esc>1.` は `(日本fo(日本fooo`）。
- **矩形の回数**: 矩形の `I` `A` `c` `C`（ADR 0035 の追補 #1042）の回数の周に消す鍵が混ざるときも、1 行目で打鍵を流し直し、周を終えた 1 行目の伸びたぶんをほかの行へ写す（Vim の `op_insert`・`w<C-v>jj2Ixy<C-w>z` は各行 `z`）。流し直しを終えたキャレットが 1 行目に無い流し直し（前の行とつないだまま・改行で次の行へ出た）は写さない（Vim の `op_insert` のカーソルの行の問い・fixture `insert-count-control-block-` 5 件）。#1107 で、この問いを置き換えの頭の行と置き換えた本文の改行の数から決める形に直した（つないでから改行で 1 行目の番号へ戻った列は写す・`j0<C-v>j2Iz<C-u><C-u><CR>y` は 2 行目と 3 行目の頭に `y`・1 行目の長さは本文を写さずに数え、写すのは伸びたぶんだけ（矩形の入口とほかの行の写しは行を読むので、Esc 全体は行の長さに比例したまま）・LF で数えたキャレットを文書の位置へ直すのは core の `document_caret_after` の 1 本で controller と共用・fixture `insert-count-join-` 9 件）。

## 追記（2026-11-03・Issue #1173）

#1107 の独立レビューの後続 4 件を oracle で測った（fixture `insert-count-join-2-` 16 件）。決定は変えない。
- **写しを読む所は行の中のバイトの桁**: 矩形の INSERT の覚え `VimBlockInsert` の読み始めを本文の位置 `at` から行頭からのバイトの桁 `at_byte` に替えた（Vim の `textcol`）。前の行を消してから改行で 1 行目の番号へ戻った列（`j0<C-v>jI<BS><BS><CR>x`・回数なしでも）は、1 行目の頭が前へずれても同じ桁から読む。`c` `C` の行の問いは Vim と同じ結果で外さない（改行で 1 行目を離れた `w<C-v>j2Cxy<BS><CR>z` も Vim は写さない）。入口の問い（流し直しの前のキャレット）は、1 周目が 1 行目を離れ流し直しの後に戻る列でも 1 行目が伸びず、写すかどうかの違いが本文に出ない（`j<C-v>j$2A<BS><C-u><CR>`）ので動かさない。流し直しの後に 1 行目へ戻るには、1 周目が最後の改行の後に打った本文が 1 行目の頭（入る前のキャレットより前）より短い必要があり、そのとき戻った 1 行目は入る前より伸びないので、写す本文はどちらの置き場所でも空になる（fixture は矩形の入口 3 件・ふつうの回数 INSERT 2 件）。
- **打った本文だけの繰り返しのキャレット**: 矩形の Esc に届く `VimInsertAt` の本文は改行を含まない（改行を含む `I` `A` は 1 行目を離れて写さず、VISUAL の `c` `C` は回数を流し直さない・fixture `insert-count-join-2-change-block-` で実測）ので、LF で数えたキャレットのまま文書の位置で、直さない（CRLF の契約 `verify_vim_block_count_insert_at_line_endings` 3 列）。Esc の費用（巨大な 1 行で行の全体を読む所）は数字を Issue に残し、直すのは別 Issue。

## 追記（2026-11-04・Issue #1234）

回数つきの INSERT の 2 周目からの `Ctrl-w` `Ctrl-u` が、始めの桁の文字の途中で止まっていた（生成器 5 の fuzz-7-0241・`12 ?` の `?` の手前で `4i<C-w>ＡＢ<Tab>  <Esc>`）。値は oracle の fixture `insert-count-ctrl-w-` 18 件（消せる場合・本文の先頭で消せない場合・回数 1 と 4・`<BS>` と `<C-u>` の同じ形・始めの桁が分解形の結合文字 U+0301 の頭に来る形 2 件）で守る。

- **`backspace` の既定**: Issue の見込みの「`backspace=` が空で始めより前を消せない」は当たらない。oracle は `-u NONE` の上に `DEFAULT_SETTINGS` の `set backspace=indent,eol,start` を置く（#1040 の追記のとおり）ので、1 周目の `Ctrl-w` は始めより前の `12` を消す。Nib は `backspace` を設定にせず、この固定値のまま（`:set backspace=` は後続にしない）。消せないのは本文の先頭（1 行目の行頭）だけで、そこは #1093 の「何も消せない消す鍵は記録しない」が受け持つ（ビープで返り redo に足さない・fixture `insert-count-ctrl-w-buffer-start` は `Ａ ` が 4 回）。
- **止まるのは歩いて始めの桁を踏んだときだけ**: Vim の `ins_bs`（`edit.c`）の `BACKSPACE_WORD` / `BACKSPACE_LINE` の do-while は `dec_cursor()` で 1 文字ずつ戻り、続ける条件は `curwin->w_cursor.col > mincol && (can_bs(BS_NOSTOP) || lnum != Insstart_orig.lnum || col != Insstart_orig.col)`（`nostop` は既定に無い）。桁はバイトで、比べるのは等しいかだけ。`Insstart_orig` は `edit()` の主の loop の頭で毎鍵 `Insstart` に戻る（`update_Insstart_orig`）ので、`ins_bs` の末尾の「始めより前を消したら `Insstart_orig.col` を寄せる」は次の鍵に残らず、始めの桁は 1 周目に手前を消しても動かない（#1040 の追記と同じ）。回数の残りは Esc（`ins_esc`）の `start_redo_ins()` が redo の打鍵を流し直すので、2 周目の本文の上では始めの桁（`12 ?` の 2）が 1 周目に入れた全角（3 バイト）の途中に来る。そこは `dec_cursor` が踏まないので、Vim は語の境か行頭まで戻る（`2l2i<C-w>ＡＢ` は `ＡＢ ?`・`2l4i<C-u>Ａ` は `Ａ ?`）。
- **Nib の形**: 左端を決める `erase_floor`（`VimStepInsert.cpp`）は、始めの桁がキャレットより左にあり、かつその桁が今の本文で 1 文字の頭（`is_boundary` で code point の頭・`vim_character_continues` で幅 0 の続きでない・ADR 0053）のときだけそこで止め、ほかは行頭まで（`ab?` の `2l2i<C-w>xéy` は 2 周目の始めの桁が U+0301 の頭で、Vim の `dec_cursor` は `é` を 1 文字として踏まないので行頭まで消して `xéy?`）。読むのは始めの桁の前後の数バイトだけで、行を写さない（1 行 16 MiB の行・#1040）。始めの桁が文字の頭に来る形（`ab ?` の `3l2i<C-w>Ａx` は 2 周目が `x` だけを消して `ＡＡx?`）は前のまま。`<BS>` は始めの桁で止まらない（`BACKSPACE_CHAR` は 1 文字で抜ける）ので変えない。

## 関連

ADR 0005（自前 Vim・oracle）・ADR 0006（表駆動）・ADR 0009（piece table・意図の和型・undo の区切り）・採用案 `docs/design/2026-09-15-editing-look.md`（D15）。

## 追記（2026-11-12・Issue #1517・ADR 0182）

INSERT の `Ctrl-o` の undo の区切りは決定 6（INSERT の出入りが区切り）のままで、打った鍵では Vim の対話と同じ 3 単位（`Ctrl-o` の前・命令・始め直した後）になる。始め直しは鍵ではなく controller が流す 1 歩（`InsertRestartRequest` → `vim_restarted_insert`）で、NORMAL → INSERT の出入りとして同じ区切りを通る。fixture の再生（press_vim_keys）の鍵は :normal! と同じに打った鍵ではない（出どころ mapped）ので、`<C-o>c` 系の Esc の後は INSERT を始め直す（ADR 0182 の決定 4）。

## 追記（2026-11-13・Issue #1626）— fixture の settings は再生も流す

oracle は fixture の `settings` を `-u NONE` の上に置いて測るが、生成物 `VimFixtures.hpp` に載って再生が読むのは viewport つきの `set scrolloff=N` だけで、ほかの行は黙って落ちていた（`set ignorecase smartcase` の 1 件は設定が効かなくても答えが同じで、何も試していなかった）。決定 7 を次のとおり補う。

- **settings は行の最後の欄**: `VimFixture.settings`（改行でつないだ行）。書くのは oracle の書き出し（`fixture_row` / `settings_field`・測定の領域の外）で、値は fixture の入力から作るので、`--only` の再利用の行もそのまま正しい。測定のコードは変えていない（`measurement_sources_match` が真）。
- **再生は窓の後・鍵の前に `:` の行として打つ**: `apply_vim_fixture_settings`（`tests/unit/VimTestSupport.cpp`）の 1 本を `verify_vim_fixture`・窓の道（`VimFixtureWindowPathTests`）・fuzz の再生器 `nib_vim_replay`（欄 `settings=`）が呼ぶ。余白の `set scrolloff=N` も同じ道で、`VimViewportFixture.scrolloff`（#1239）は無くした（道を 2 本にしない）。折り返す窓の fixture も余白を同じ欄の `set scrolloff=N` で書く（`eng/vim-wrap-oracle.py`）。
- **Nib が断る行は落とす**: 打つ前に同じ評価 `evaluate_ex` が受けるかを確かめ、受けなければ試験が落ちる（再生器は `mismatch <name> setting`）。Nib に `:set` の口が無く固定値と同じ行だけを、oracle の表 `NIB_FIXED_SETTINGS`（いまは `set nrformats=bin,hex` の 1 行）が生成物に載せない。表に無い行は全部流す。
- **移し替え**: 既存の 1,087 行（余白の 1,086 行と smartcase の 1 行）は Vim を使わずに書き出しの形だけを移し、24 件を新しい書き出しで測り直して生成物の差が 0 であることを確かめた。折り返す窓の 402 件は共有 oracle の digest（CNF-010）のため測り直し、差は余白の 10 行の形と digest の行だけ。
- **守り**: fixture `fixture-settings-` 8 件（`ignorecase`・`smartcase`（`*` には効かない）・`nowrapscan`・2 行の設定）を足した。設定を流さない変異で、うち 7 件と下の 1 件が落ちる（残る 1 件 `smartcase-upper-keeps-case` は大文字のパターンで、設定が効いても効かなくても答えが同じ。`ignorecase` だけが効いて `smartcase` が落ちる形を落とす）。同じ変異で余白の fixture 187 件のうち 131 件、折り返す窓の 10 件のうち 6 件も落ちる。`nowrapscan` の失敗する検索は `-es` の Vim が終了コード 1 を返すので、viewport つき（`--not-a-term`）で測る。`pattern-escape-search-tab-smartcase` は本文と鍵を設定が効く形（`x\tA\ta` で `/\ta<CR>x`）に直して測り直した。`hlsearch` は fixture の欄（本文・キャレット・レジスタ・窓）に現れないので、この形では試せない。
