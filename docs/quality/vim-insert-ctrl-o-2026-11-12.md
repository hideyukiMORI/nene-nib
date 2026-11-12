# INSERT の `Ctrl-o` の Vim 9.1 の実測（2026-11-12・#1517）

ADR 0182 の根拠。下ごしらえ G2 の実測（133 形・3 つの測り方）と Vim 9.1.0004 の source の行を写した記録。
測った木はまとめ枝 24（`c75b193c`）、Vim は `C:\Program Files\Vim\vim91\vim.exe`（9.1 パッチ 1-4・oracle と同じ）、
probe は `D:/NeNeNib/scripts/20261112/1517/probe.py`（cases*.json・table.py・リポジトリの外）。
fixture（接頭辞 `insert-ctrl-o-`）は oracle（下の N の形）がこの表と同じ形を測り直したもので、期待値は
`tests/vim/VimFixtures.hpp` が正本である。打った鍵の側（下の F の形）は契約 `--vim-insert-normal` が守る。

## 0. 測り方（3 つの形）

| 記号 | 形 | 意味 |
|---|---|---|
| N | `-es` で `execute "normal! …"` 1 本 | **oracle と同じ形**。鍵は「打っていない」（KeyTyped 偽）・`u_sync` は入らない（1 本の中は 1 単位） |
| F | `-es` で `call feedkeys("…", "xt")` | **打った鍵と同じ**（`t`）。`gotchars` → `may_sync_undo` が NORMAL の鍵ごとに単位を切る＝対話の Vim の undo。`q` の録画も効く |
| T | `--not-a-term -s keys.bin`（scriptin） | 打鍵として流すが `may_sync_undo` は scriptin では切らない（getchar.c:1376）。最後に Esc を足して報告するので「INSERT に戻ったか」は見えない |

各行の記録: 本文・キャレット（line,col）・無名レジスタ（種類:本文）・`'^` `'[` `']` `'.`・`undotree().seq_cur/seq_last`・`&modified`・
途中の値 `m`（`<C-o>:let g:m=…<CR>` で測ったもの: `mode(1)`・`col('.')`・`'^` `'.`）・`".`（直前の挿入）。
INSERT に残ったかは鍵の後ろに `Z<Esc>` を足して見た（INSERT なら `Z` が入る・batch 3 の `z-*`）。

## 1. Vim の規則（実測と source）

### 1.1 入口: `<C-o>` は「Esc の半分」（edit.c:860 `case Ctrl_O` → `ins_ctrl_o` edit.c:3864 → `goto doESCkey` → `ins_esc` edit.c:3597）

- `ins_ctrl_o` は `restart_edit='I'` と `ins_at_eol = (カーソルが行の内容の終わり NUL にいるか)` を置く。`count = 0` にしてから `ins_esc` へ行くので、**`3i` の回数の反復は捨てる**（`3iX<C-o>lY<Esc>` → `XaYlpha…`・`3iX<C-o><Esc>` → X は 1 つ）。
- `ins_esc` は普通の Esc と同じに: 何か打っていれば（`!arrow_used`）redo に ESC を足して **`.` の記録を確定**（edit.c:3621-3625）・`stop_insert`・**`'^` を置く**（edit.c:3669・`<C-o>` の時点の位置: `AX<C-o>` で `'^`=[1,18]）・IME を切る（`im_set_active(FALSE)`）・State を NORMAL へ。`".` もここで確定（`<C-o>:` の中で `getreg('.')`=`X`・`'.`=[1,7]）。
- **左へ 1 つ戻すのは行の内容の終わり（NUL の上）にいるときだけ**（edit.c:3677-3690 `restart_edit == NUL || (gchar_cursor() == NUL && !VIsual_active)`）。`wiX<C-o>:let g:m=col('.')` は 8（戻らない）、`AX<C-o>…` は 17（X の上へ戻る）、空の行は 1。全角・結合文字は文字ごと戻る（`aé` の A → col 2）。
- ステータスの表示は `-- (insert) --`（screen.c:4087-4089 `restart_edit == 'I'`）、`<C-o>v` 中は `-- (insert) VISUAL --`。`mode(1)` は `niI`（T の形で実測）。

### 1.2 NORMAL の命令 1 つ（normal.c:1040-1058 `normal_cmd` の終わり）

- 命令が終わったと見なす条件: `oap->op_type == OP_NOP && restart_edit != 0 && !VIsual_active && old_mapped_len == 0 && !(retval & CA_COMMAND_BUSY) && stuff_empty() && oap->regname == 0`。満たせば `edit(restart_edit, FALSE, 1)` で INSERT を始め直す。
  → **回数・オペレータ待ち・`"x` の待ちの間は戻らない**（`<C-o>3l` `<C-o>dw` `<C-o>"ayy` `<C-o>2"aye` は命令 1 つとして効く）。
  → **VISUAL の間は戻らない。VISUAL が終わった命令の後で戻る**（`<C-o>vlld` `<C-o>vlly` `<C-o>Vd` `<C-o>vll<Esc>` はどれも INSERT へ戻る）。
- **NORMAL の Esc は `restart_edit` を消さない**（nv_esc normal.c:6851-6857: 消すのは Ctrl-C だけ）。よって `<C-o><Esc>` `<C-o>d<Esc>` `<C-o>"a<Esc>` `<C-o>3<Esc>` は **どれも INSERT へ戻る**（後の `Y` が入る）。INSERT を抜けるには Esc がもう 1 つ要る。
- `<C-o><C-o>` の 2 つ目は NORMAL の `Ctrl-O`（ジャンプの戻り・#1041）。`<C-o>l<C-o>l` は 2 回とも普通に効く。
- `<C-o>u` は `<C-o>` の前に打った INSERT の分を戻す（u_undo は先に u_sync する undo.c:2204・3 形とも同じ）。
- `<C-o>.` は **INSERT を始める前の変更**を繰り返す（`ResetRedobuff` が前の記録を old_redobuff に残す getchar.c:479・`nv_dot` は `restart_edit != 0 && !arrow_used` なら old を読む normal.c:6337-6340）。`xiab<C-o>.cd` → `x` をもう 1 回。再開の後まだ何も打っていない（arrow_used）なら今の記録を読む。
- `<C-o>@a` は特別: `do_execreg` が `restart_edit` を `i` として typeahead の**後ろ**に積み、`restart_edit` を消す（register.c:716-736 `put_reedit_in_typebuf`）。マクロの鍵は全部 NORMAL で流れ、最後に普通の `i` が打たれる（a=`ll` → `XalYpha`・a=`iZ<Esc>` → `XYZalpha`）。
- `<C-o>:set so=2<CR>` `<C-o>/gam<CR>` は命令 1 つ（入力行を閉じてから戻る）。`<C-o>zz` `<C-o><C-e>` も命令 1 つ（-es では窓が無いので表示は viewport の fixture で測る）。

### 1.3 INSERT を始め直す（edit.c:123 `edit()` の再開の枝 edit.c:341-376）

- `did_restart_edit` が立つ・**redo を Reset しない**（edit.c:245 `restart_edit == 0` のときだけ `ResetRedobuff`）・`arrow_used = TRUE`（edit.c:350）＝「矢印で区切った後」と同じ。最初に打った鍵で `stop_arrow`（edit.c:2378-2405）が `ResetRedobuff` と `"1i"` を積む。
  → `iX<C-o>lY<Esc>` の後の `.` は **`Y` だけ**入れる（`1iY<Esc>`）。`3.` は `YYY`。Nib の #1137 の `restarted_insert()`（`1i` から取り直す）と同じ仕組み。
  → 再開の後に何も打たずに Esc すれば記録は `<C-o>` の時点のまま（`xiX<C-o>l<Esc>w.` は `X`・`3iX<C-o>l<Esc>w.` は **`XXX`**: 記録は `3iX<Esc>` で、その場の反復は捨てたのに記録には回数が残る）。何も打っていない `i<C-o>l<Esc>` の記録は空の `i<Esc>`（`.` はキャレットを 1 つ左へ戻すだけ）。`i<C-o>x<Esc>w.` は `x` を繰り返す（NORMAL の `x` が記録を置き換える）。
- `Insstart` は再開の位置で取り直す（edit.c:228-238）。`<C-u>` `<C-w>` の止まりは再開の位置（`wiabc<C-o>hd<C-u>` は `d` だけ消す）。再開の後に何も打たずに `<C-u>` すると行頭まで消す（`wiabc<C-o>h<C-u>` → `cbeta gamma`）。`<BS>` は `backspace=start` なので再開の位置を越えて消す。
- **行末の位置の規則**（edit.c:355-375）: 「`<C-o>` の前に行の内容の終わり（`ins_at_eol`）にいて同じ行（`o_lnum`）にまだいる」か「`curswant` > いまの仮想桁（`^O$` `^O80|`・`j` `k` で短い行へ来たとき）」で、かつカーソルが行の最後の文字の上なら、その文字の後ろへ 1 つ進める。
  例: `AX<C-o>lY` → `l` は失敗するが `XY`（F）・`AX<C-o>hY` → `gammYaX`（進めない）・`iX<C-o>$Y` → 行末に `Y`・`5|iX<C-o>jY`（2 行目 `ab`）→ `abY`・`AX<C-o>jY` → `abY`。
- `'^` は再開した INSERT を抜けた Esc の位置（普通の Esc と同じ）。`gi` は `'^` へ（`<C-o>gi` は `<C-o>` が置いた `'^` へ）。

### 1.4 INSERT へ入る命令（`i a A o O c s C S cc` と `<C-o>v…c`）

- `i a A o O` は `invoke_edit`（normal.c:7075-7103）が `restart_edit` を消してから入る（`.` や `r` の再生の中だけ残す）→ Esc 1 つで NORMAL。**N と F で同じ**。
- **`c` `s` `C` `S` `cc` と VISUAL の `c` は KeyTyped で分かれる**（ops.c:4114-4143 `case OP_CHANGE`: `if (p_im || !KeyTyped) restart_edit_save = restart_edit; else 0`）。
  - 打った鍵（F・対話）: `c` の INSERT の Esc で NORMAL（`iX<C-o>cwY<Esc>Z<Esc>` → `XY beta gamma`）。
  - oracle（N・`:normal!`）とマクロ（`@a` は N でも F でも）: `c` の Esc の後に **もう一度 INSERT を始め直す**（`XZY beta gamma`・`e-dot-cw` の N は `w.` が文字として入る）。
- `<C-o>.` が挿入を再生するとき（`iQ<Esc>wiX<C-o>.Y`）は再生の後に戻る（`invoke_edit` の `!stuff_empty()`）。

### 1.5 undo の単位

- 対話（F）: `<C-o>` の前の INSERT・NORMAL の命令・再開した INSERT がそれぞれ 1 単位（NORMAL で打った鍵を読むときに `may_sync_undo` getchar.c:1376）。
  `iab<C-o>xcd<Esc>` は 3 単位: `u` → `ablpha`（1,3）・`uu` → `abalpha`（1,3）・`uuu` → 元。`iab<C-o>lcd<Esc>` は 2 単位: `u` → `abalpha`（1,4）。`wiab<C-o>xcd<Esc>u` → `alpha abeta gamma`（1,9）。
- oracle（N）: 1 本の `:normal!` は 1 単位に畳まれる（`u` は全部を戻す）。**`<C-o>` を挟む fixture に `u` を入れても対話の単位は測れない**（`<C-o>u` だけは 3 形が一致する）。

### 1.6 失敗した命令

- F・T（対話）: ビープして INSERT へ戻り、後の鍵は続く（`AX<C-o>lY` → `XY`・`iX<C-o>fzY` → `XYalpha`・`i<C-o>hY` → `Yalpha`）。
- N（oracle）: `:normal!` は失敗で残りの鍵を捨てる（beep_flush）。捨てた後も `restart_edit` は残るので INSERT を始め直し、鍵が尽きた `:normal!` の Esc（getchar.c:3452）で抜ける（行末の規則の後に左へ 1 つ）。Nib の `press_vim_keys`（EditorControllerVim.cpp:373-398・失敗の後に INSERT なら Esc）と同じ形になる。

### 1.7 マクロ（`q`）

- 録画は打った鍵を録る: `qaiX<C-o>lY<Esc>q` の `@a` は `iX\x0flY\x1b`（F）。再生は 3 形とも打ったときと同じ本文。
- `<C-o>q` で録画を止めると、末尾の `<C-o>` を録画から外す（getchar.c:176 `get_recorded`: `restart_edit != 0 && p[len-1] == Ctrl_O`）。`qaiX<C-o>qY<Esc>` の `@a` は `iX`。
- `<C-o>qa` で録画を始められる（その後の INSERT の鍵から録る）。oracle の `:normal!` では録画は動かない。

### 1.8 oracle（N）と対話（F）が違う形のまとめ

| 種類 | 例 | N（oracle） | F（対話） |
|---|---|---|---|
| `c` 系の後 | `iX<C-o>cwY<Esc>Z<Esc>` | `XZY beta gamma`（INSERT へもう一度） | `XY beta gamma` |
| 失敗の後 | `AX<C-o>lY<Esc>` | `…gammaX`（残りを捨てる） | `…gammaXY` |
| undo | `iab<C-o>xcd<Esc>u` | 全部戻る | `ablpha`（`cd` だけ） |
| `/pat<Esc>` | `iX<C-o>/gam<Esc>Y<Esc>` | 検索が効く（`:normal` の Esc は入力行を実行） | 取消して `XY…`（T） |
| redo の取り消し | `xuiab<C-o><C-r>cd<Esc>` | `<C-r>` 失敗で `ab` だけ | `abcd…` |
| 録画 | `q…` | 動かない | 動く |

## 5. fixture で測れる形・測れない形

| 測れる（oracle `:normal!` と Nib の打った鍵が一致するはず） | 測れない・注意 |
|---|---|
| 移動 `l h 0 $ w b gg G j k fx 3l`・行末（`A…<C-o>h/l/0/$/j`）・空の行・空の文書・全角・結合文字 | undo の単位（`:normal!` は 1 単位・`<C-o>u` だけは一致） |
| オペレータ `dw dd D x p P yy "ayy 2"aye ~ >> J rz 3x 3o` | `q` の録画（`:normal!` では動かない・`register` 欄の再生だけ） |
| `<C-o><Esc>` `<C-o>d<Esc>` `<C-o>"a<Esc>` `<C-o>3<Esc>`・`<C-o>l<C-o>l`・`<C-o><C-o>`（ジャンプの列は oracle の `clearjumps` の形） | `<C-o>c` 系の対話の振る舞い（oracle は KeyTyped 偽の側・決定 1 次第） |
| `<C-o>i a A o O 3o gi`・`<C-o>vlld` `vlly` `Vd` `vll<Esc>` | 失敗の後に続く打鍵（oracle は捨てる・Nib の `press_vim_keys` も捨てるので形は一致する。対話の「続く」は単体テスト） |
| `.`（`iX<C-o>lY<Esc>w.` `3.`・`3iX<C-o>l<Esc>w.`・`i<C-o>x<Esc>w.`・`<C-o>.`） | `<C-o>/pat<Esc>`（`:normal` の Esc は入力行を実行する・使わない） |
| `@a` の再生（`register` 欄）・`<C-o>@a` | `<C-o>zz` `<C-o><C-e>` は viewport つきの fixture で |
| `` `^ `` `gi` で `'^`・`"change": true` で変更の印 | `mode(1)` と `(insert)` の表示・IME・キャレットの形（単体テスト） |
| `<C-o>/gam<CR>`・`<C-o>:set so=2<CR>`（Ex を fixture に入れられるかは今の再生の形に従う） | `<C-o>q` の録画の中身（F の形で実測済み・単体テスト） |
| `<C-u>` `<C-w>` `<BS>` の止まり（再開の位置） | |

## 6. 実測の表（全件）

N = oracle の形、F = feedkeys（打った鍵）、T = scriptin。T は N と同じなら省いた。`≠N` は本文・キャレット・レジスタ・マーク・`".` のどれかが N と違う行。
seq は `seq_cur/seq_last`（N と T は 1 本の中が 1 単位、F が対話の単位）。途中の値は `<C-o>:let g:m=…<CR>` で測った値（`-` は測っていない）。

| 名前 | 本文 | 鍵 | 形 | 結果の本文 | キャレット | 無名 | `'^` `'[` `']` `'.` | seq_cur/last・modified | 途中の値 | `".` |
|---|---|---|---|---|---|---|---|---|---|---|
| m-l | `alpha` | `liX<C-o>lY<Esc>` | N | `aXlYpha` | 1,4 | :'' | ^1,5 [1,4 ]1,5 .1,4 | 1/1 mod=1 | - | 'Y' |
| m-l | `alpha` | `liX<C-o>lY<Esc>` | F | `aXlYpha` | 1,4 | :'' | ^1,5 [1,4 ]1,5 .1,4 | 2/2 mod=1 | - | 'Y' |
| m-l2 | `alpha beta gamma` | `iX<C-o>lY<Esc>` | N | `XaYlpha beta gamma` | 1,3 | :'' | ^1,4 [1,3 ]1,4 .1,3 | 1/1 mod=1 | - | 'Y' |
| m-l2 | `alpha beta gamma` | `iX<C-o>lY<Esc>` | F | `XaYlpha beta gamma` | 1,3 | :'' | ^1,4 [1,3 ]1,4 .1,3 | 2/2 mod=1 | - | 'Y' |
| m-0 | `alpha beta gamma` | `wwiX<C-o>0Y<Esc>` | N | `Yalpha beta Xgamma` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'Y' |
| m-0 | `alpha beta gamma` | `wwiX<C-o>0Y<Esc>` | F | `Yalpha beta Xgamma` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 2/2 mod=1 | - | 'Y' |
| m-$ | `alpha beta gamma` | `iX<C-o>$Y<Esc>` | N | `Xalpha beta gammaY` | 1,18 | :'' | ^1,19 [1,18 ]1,19 .1,18 | 1/1 mod=1 | - | 'Y' |
| m-$ | `alpha beta gamma` | `iX<C-o>$Y<Esc>` | F | `Xalpha beta gammaY` | 1,18 | :'' | ^1,19 [1,18 ]1,19 .1,18 | 2/2 mod=1 | - | 'Y' |
| m-$-A | `alpha beta gamma` | `AX<C-o>$Y<Esc>` | N | `alpha beta gammaXY` | 1,18 | :'' | ^1,19 [1,18 ]1,19 .1,18 | 1/1 mod=1 | - | 'Y' |
| m-$-A | `alpha beta gamma` | `AX<C-o>$Y<Esc>` | F | `alpha beta gammaXY` | 1,18 | :'' | ^1,19 [1,18 ]1,19 .1,18 | 2/2 mod=1 | - | 'Y' |
| m-eol-h | `alpha beta gamma` | `AX<C-o>hY<Esc>` | N | `alpha beta gammYaX` | 1,16 | :'' | ^1,17 [1,16 ]1,17 .1,16 | 1/1 mod=1 | - | 'Y' |
| m-eol-h | `alpha beta gamma` | `AX<C-o>hY<Esc>` | F | `alpha beta gammYaX` | 1,16 | :'' | ^1,17 [1,16 ]1,17 .1,16 | 2/2 mod=1 | - | 'Y' |
| m-eol-l | `alpha beta gamma` | `AX<C-o>lY<Esc>` | N | `alpha beta gammaX` | 1,17 | :'' | ^1,18 [1,17 ]1,18 .1,17 | 1/1 mod=1 | - | 'X' |
| m-eol-l | `alpha beta gamma` | `AX<C-o>lY<Esc>` | F **≠N** | `alpha beta gammaXY` | 1,18 | :'' | ^1,19 [1,18 ]1,19 .1,18 | 2/2 mod=1 | - | 'Y' |
| m-eol-l | `alpha beta gamma` | `AX<C-o>lY<Esc>` | T **≠N** | `alpha beta gammaXY` | 1,18 | :'' | ^1,19 [1,18 ]1,19 .1,18 | 1/1 mod=1 | - | 'Y' |
| m-eol-0 | `alpha beta gamma` | `AX<C-o>0Y<Esc>` | N | `Yalpha beta gammaX` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'Y' |
| m-eol-0 | `alpha beta gamma` | `AX<C-o>0Y<Esc>` | F | `Yalpha beta gammaX` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 2/2 mod=1 | - | 'Y' |
| m-w | `alpha beta gamma` | `iX<C-o>wY<Esc>` | N | `Xalpha Ybeta gamma` | 1,8 | :'' | ^1,9 [1,8 ]1,9 .1,8 | 1/1 mod=1 | - | 'Y' |
| m-w | `alpha beta gamma` | `iX<C-o>wY<Esc>` | F | `Xalpha Ybeta gamma` | 1,8 | :'' | ^1,9 [1,8 ]1,9 .1,8 | 2/2 mod=1 | - | 'Y' |
| m-b | `alpha beta gamma` | `wwiX<C-o>bY<Esc>` | N | `alpha beta YXgamma` | 1,12 | :'' | ^1,13 [1,12 ]1,13 .1,12 | 1/1 mod=1 | - | 'Y' |
| m-b | `alpha beta gamma` | `wwiX<C-o>bY<Esc>` | F | `alpha beta YXgamma` | 1,12 | :'' | ^1,13 [1,12 ]1,13 .1,12 | 2/2 mod=1 | - | 'Y' |
| m-gg | `alpha beta\nsecond line here\nthird` | `jjiX<C-o>ggY<Esc>` | N | `Yalpha beta\nsecond line here\nXthird` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'Y' |
| m-gg | `alpha beta\nsecond line here\nthird` | `jjiX<C-o>ggY<Esc>` | F | `Yalpha beta\nsecond line here\nXthird` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 2/2 mod=1 | - | 'Y' |
| m-G | `alpha beta\nsecond line here\nthird` | `iX<C-o>GY<Esc>` | N | `Xalpha beta\nsecond line here\nYthird` | 3,1 | :'' | ^3,2 [3,1 ]3,2 .3,1 | 1/1 mod=1 | - | 'Y' |
| m-G | `alpha beta\nsecond line here\nthird` | `iX<C-o>GY<Esc>` | F | `Xalpha beta\nsecond line here\nYthird` | 3,1 | :'' | ^3,2 [3,1 ]3,2 .3,1 | 2/2 mod=1 | - | 'Y' |
| m-j | `abcdefgh\nab\nabcdefgh` | `5\|iX<C-o>jY<Esc>` | N | `abcdXefgh\nabY\nabcdefgh` | 2,3 | :'' | ^2,4 [2,3 ]2,4 .2,3 | 1/1 mod=1 | - | 'Y' |
| m-j | `abcdefgh\nab\nabcdefgh` | `5\|iX<C-o>jY<Esc>` | F | `abcdXefgh\nabY\nabcdefgh` | 2,3 | :'' | ^2,4 [2,3 ]2,4 .2,3 | 2/2 mod=1 | - | 'Y' |
| m-jj | `abcdefgh\nab\nabcdefgh` | `5\|iX<C-o>j<C-o>jY<Esc>` | N | `abcdXefgh\nab\nabcdeYfgh` | 3,6 | :'' | ^3,7 [3,6 ]3,7 .3,6 | 1/1 mod=1 | - | 'Y' |
| m-jj | `abcdefgh\nab\nabcdefgh` | `5\|iX<C-o>j<C-o>jY<Esc>` | F | `abcdXefgh\nab\nabcdeYfgh` | 3,6 | :'' | ^3,7 [3,6 ]3,7 .3,6 | 2/2 mod=1 | - | 'Y' |
| m-k | `abcdefgh\nab\nabcdefgh` | `G5\|iX<C-o>kY<Esc>` | N | `abcdefgh\nabY\nabcdXefgh` | 2,3 | :'' | ^2,4 [2,3 ]2,4 .2,3 | 1/1 mod=1 | - | 'Y' |
| m-k | `abcdefgh\nab\nabcdefgh` | `G5\|iX<C-o>kY<Esc>` | F | `abcdefgh\nabY\nabcdXefgh` | 2,3 | :'' | ^2,4 [2,3 ]2,4 .2,3 | 2/2 mod=1 | - | 'Y' |
| m-A-j | `abcdefgh\nab\nabcdefgh` | `AX<C-o>jY<Esc>` | N | `abcdefghX\nabY\nabcdefgh` | 2,3 | :'' | ^2,4 [2,3 ]2,4 .2,3 | 1/1 mod=1 | - | 'Y' |
| m-A-j | `abcdefgh\nab\nabcdefgh` | `AX<C-o>jY<Esc>` | F | `abcdefghX\nabY\nabcdefgh` | 2,3 | :'' | ^2,4 [2,3 ]2,4 .2,3 | 2/2 mod=1 | - | 'Y' |
| m-fx | `alpha beta gamma` | `iX<C-o>fbY<Esc>` | N | `Xalpha Ybeta gamma` | 1,8 | :'' | ^1,9 [1,8 ]1,9 .1,8 | 1/1 mod=1 | - | 'Y' |
| m-fx | `alpha beta gamma` | `iX<C-o>fbY<Esc>` | F | `Xalpha Ybeta gamma` | 1,8 | :'' | ^1,9 [1,8 ]1,9 .1,8 | 2/2 mod=1 | - | 'Y' |
| m-3l | `alpha beta gamma` | `iX<C-o>3lY<Esc>` | N | `XalpYha beta gamma` | 1,5 | :'' | ^1,6 [1,5 ]1,6 .1,5 | 1/1 mod=1 | - | 'Y' |
| m-3l | `alpha beta gamma` | `iX<C-o>3lY<Esc>` | F | `XalpYha beta gamma` | 1,5 | :'' | ^1,6 [1,5 ]1,6 .1,5 | 2/2 mod=1 | - | 'Y' |
| m-h0 | `alpha beta gamma` | `iX<C-o>hY<Esc>` | N | `YXalpha beta gamma` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'Y' |
| m-h0 | `alpha beta gamma` | `iX<C-o>hY<Esc>` | F | `YXalpha beta gamma` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 2/2 mod=1 | - | 'Y' |
| m-h0b | `alpha beta gamma` | `i<C-o>hY<Esc>` | N | `alpha beta gamma` | 1,1 | :'' | ^1,1 [1,1 ]1,1 .0,0 | 0/0 mod=0 | - | '' |
| m-h0b | `alpha beta gamma` | `i<C-o>hY<Esc>` | F **≠N** | `Yalpha beta gamma` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'Y' |
| m-h0b | `alpha beta gamma` | `i<C-o>hY<Esc>` | T **≠N** | `Yalpha beta gamma` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'Y' |
| m-fail-f | `alpha beta gamma` | `iX<C-o>fzY<Esc>` | N | `Xalpha beta gamma` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'X' |
| m-fail-f | `alpha beta gamma` | `iX<C-o>fzY<Esc>` | F **≠N** | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| m-fail-f | `alpha beta gamma` | `iX<C-o>fzY<Esc>` | T **≠N** | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| o-dw | `alpha beta gamma` | `iX<C-o>dwY<Esc>` | N | `XYbeta gamma` | 1,2 | v:'alpha ' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| o-dw | `alpha beta gamma` | `iX<C-o>dwY<Esc>` | F | `XYbeta gamma` | 1,2 | v:'alpha ' | ^1,3 [1,2 ]1,3 .1,2 | 3/3 mod=1 | - | 'Y' |
| o-dd | `alpha beta\nsecond line here\nthird` | `jiX<C-o>ddY<Esc>` | N | `alpha beta\nYthird` | 2,1 | V:'Xsecond line here\n' | ^2,2 [2,1 ]2,2 .2,1 | 1/1 mod=1 | - | 'Y' |
| o-dd | `alpha beta\nsecond line here\nthird` | `jiX<C-o>ddY<Esc>` | F | `alpha beta\nYthird` | 2,1 | V:'Xsecond line here\n' | ^2,2 [2,1 ]2,2 .2,1 | 3/3 mod=1 | - | 'Y' |
| o-D | `alpha beta gamma` | `wiX<C-o>DY<Esc>` | N | `alpha XY` | 1,8 | v:'beta gamma' | ^1,9 [1,8 ]1,9 .1,8 | 1/1 mod=1 | - | 'Y' |
| o-D | `alpha beta gamma` | `wiX<C-o>DY<Esc>` | F | `alpha XY` | 1,8 | v:'beta gamma' | ^1,9 [1,8 ]1,9 .1,8 | 3/3 mod=1 | - | 'Y' |
| o-x | `alpha beta gamma` | `wiX<C-o>xY<Esc>` | N | `alpha XYeta gamma` | 1,8 | v:'b' | ^1,9 [1,8 ]1,9 .1,8 | 1/1 mod=1 | - | 'Y' |
| o-x | `alpha beta gamma` | `wiX<C-o>xY<Esc>` | F | `alpha XYeta gamma` | 1,8 | v:'b' | ^1,9 [1,8 ]1,9 .1,8 | 3/3 mod=1 | - | 'Y' |
| o-x-eol | `alpha beta gamma` | `AX<C-o>xY<Esc>` | N | `alpha beta gammaY` | 1,17 | v:'X' | ^1,18 [1,17 ]1,18 .1,17 | 1/1 mod=1 | - | 'Y' |
| o-x-eol | `alpha beta gamma` | `AX<C-o>xY<Esc>` | F | `alpha beta gammaY` | 1,17 | v:'X' | ^1,18 [1,17 ]1,18 .1,17 | 3/3 mod=1 | - | 'Y' |
| o-p | `alpha beta gamma` | `yiwwiX<C-o>pY<Esc>` | N | `alpha XbalphaYeta gamma` | 1,14 | v:'alpha' | ^1,15 [1,14 ]1,15 .1,14 | 1/1 mod=1 | - | 'Y' |
| o-p | `alpha beta gamma` | `yiwwiX<C-o>pY<Esc>` | F | `alpha XbalphaYeta gamma` | 1,14 | v:'alpha' | ^1,15 [1,14 ]1,15 .1,14 | 3/3 mod=1 | - | 'Y' |
| o-P | `alpha beta gamma` | `yiwwiX<C-o>PY<Esc>` | N | `alpha XalphaYbeta gamma` | 1,13 | v:'alpha' | ^1,14 [1,13 ]1,14 .1,13 | 1/1 mod=1 | - | 'Y' |
| o-P | `alpha beta gamma` | `yiwwiX<C-o>PY<Esc>` | F | `alpha XalphaYbeta gamma` | 1,13 | v:'alpha' | ^1,14 [1,13 ]1,14 .1,13 | 3/3 mod=1 | - | 'Y' |
| o-yy | `alpha beta\nsecond line here\nthird` | `iX<C-o>yyY<Esc>` | N | `XYalpha beta\nsecond line here\nthird` | 1,2 | V:'Xalpha beta\n' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| o-yy | `alpha beta\nsecond line here\nthird` | `iX<C-o>yyY<Esc>` | F | `XYalpha beta\nsecond line here\nthird` | 1,2 | V:'Xalpha beta\n' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| o-ayy | `alpha beta\nsecond line here\nthird` | `iX<C-o>"ayyY<Esc>` | N | `XYalpha beta\nsecond line here\nthird` | 1,2 | V:'Xalpha beta\n' a='Xalpha beta\n' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| o-ayy | `alpha beta\nsecond line here\nthird` | `iX<C-o>"ayyY<Esc>` | F | `XYalpha beta\nsecond line here\nthird` | 1,2 | V:'Xalpha beta\n' a='Xalpha beta\n' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| o-~ | `alpha beta gamma` | `iX<C-o>~Y<Esc>` | N | `XAYlpha beta gamma` | 1,3 | :'' | ^1,4 [1,3 ]1,4 .1,3 | 1/1 mod=1 | - | 'Y' |
| o-~ | `alpha beta gamma` | `iX<C-o>~Y<Esc>` | F | `XAYlpha beta gamma` | 1,3 | :'' | ^1,4 [1,3 ]1,4 .1,3 | 3/3 mod=1 | - | 'Y' |
| o->> | `alpha beta gamma` | `iX<C-o>>>Y<Esc>` | N | `	YXalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| o->> | `alpha beta gamma` | `iX<C-o>>>Y<Esc>` | F | `	YXalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 3/3 mod=1 | - | 'Y' |
| o-J | `alpha beta\nsecond line here\nthird` | `iX<C-o>JY<Esc>` | N | `Xalpha betaY second line here\nthird` | 1,12 | :'' | ^1,13 [1,12 ]1,13 .1,12 | 1/1 mod=1 | - | 'Y' |
| o-J | `alpha beta\nsecond line here\nthird` | `iX<C-o>JY<Esc>` | F | `Xalpha betaY second line here\nthird` | 1,12 | :'' | ^1,13 [1,12 ]1,13 .1,12 | 3/3 mod=1 | - | 'Y' |
| o-rz | `alpha beta gamma` | `iX<C-o>rzY<Esc>` | N | `XYzlpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| o-rz | `alpha beta gamma` | `iX<C-o>rzY<Esc>` | F | `XYzlpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 3/3 mod=1 | - | 'Y' |
| o-u | `alpha beta gamma` | `iab<C-o>ucd<Esc>` | N | `cdalpha beta gamma` | 1,2 | :'' | ^1,3 [1,1 ]1,3 .1,1 | 2/2 mod=1 | - | 'cd' |
| o-u | `alpha beta gamma` | `iab<C-o>ucd<Esc>` | F | `cdalpha beta gamma` | 1,2 | :'' | ^1,3 [1,1 ]1,3 .1,1 | 2/2 mod=1 | - | 'cd' |
| o-u2 | `alpha beta gamma` | `xiab<C-o>ucd<Esc>` | N | `cdalpha beta gamma` | 1,2 | v:'a' | ^1,3 [1,1 ]1,3 .1,1 | 2/2 mod=1 | - | 'cd' |
| o-u2 | `alpha beta gamma` | `xiab<C-o>ucd<Esc>` | F **≠N** | `cdlpha beta gamma` | 1,2 | v:'a' | ^1,3 [1,1 ]1,3 .1,1 | 3/3 mod=1 | - | 'cd' |
| o-cr | `alpha beta gamma` | `xuiab<C-o><C-r>cd<Esc>` | N | `abalpha beta gamma` | 1,2 | v:'a' | ^1,3 [1,1 ]1,3 .1,1 | 2/2 mod=1 | - | 'ab' |
| o-cr | `alpha beta gamma` | `xuiab<C-o><C-r>cd<Esc>` | F **≠N** | `abcdalpha beta gamma` | 1,4 | v:'a' | ^1,5 [1,3 ]1,5 .1,3 | 3/3 mod=1 | - | 'cd' |
| o-cr | `alpha beta gamma` | `xuiab<C-o><C-r>cd<Esc>` | T **≠N** | `abcdalpha beta gamma` | 1,4 | v:'a' | ^1,5 [1,3 ]1,5 .1,3 | 2/2 mod=1 | - | 'cd' |
| o-dot | `alpha beta gamma` | `xiab<C-o>.cd<Esc>` | N | `abcdpha beta gamma` | 1,4 | v:'l' | ^1,5 [1,3 ]1,5 .1,3 | 1/1 mod=1 | - | 'cd' |
| o-dot | `alpha beta gamma` | `xiab<C-o>.cd<Esc>` | F | `abcdpha beta gamma` | 1,4 | v:'l' | ^1,5 [1,3 ]1,5 .1,3 | 4/4 mod=1 | - | 'cd' |
| r-i | `alpha beta gamma` | `iX<C-o>iY<Esc>` | N | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| r-i | `alpha beta gamma` | `iX<C-o>iY<Esc>` | F | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| r-i-esc2 | `alpha beta gamma` | `iX<C-o>iY<Esc>Z<Esc>` | N | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| r-i-esc2 | `alpha beta gamma` | `iX<C-o>iY<Esc>Z<Esc>` | F | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| r-a | `alpha beta gamma` | `iX<C-o>aY<Esc>` | N | `XaYlpha beta gamma` | 1,3 | :'' | ^1,4 [1,3 ]1,4 .1,3 | 1/1 mod=1 | - | 'Y' |
| r-a | `alpha beta gamma` | `iX<C-o>aY<Esc>` | F | `XaYlpha beta gamma` | 1,3 | :'' | ^1,4 [1,3 ]1,4 .1,3 | 2/2 mod=1 | - | 'Y' |
| r-A | `alpha beta gamma` | `iX<C-o>AY<Esc>` | N | `Xalpha beta gammaY` | 1,18 | :'' | ^1,19 [1,18 ]1,19 .1,18 | 1/1 mod=1 | - | 'Y' |
| r-A | `alpha beta gamma` | `iX<C-o>AY<Esc>` | F | `Xalpha beta gammaY` | 1,18 | :'' | ^1,19 [1,18 ]1,19 .1,18 | 2/2 mod=1 | - | 'Y' |
| r-o | `alpha beta\nsecond line here\nthird` | `iX<C-o>oY<Esc>` | N | `Xalpha beta\nY\nsecond line here\nthird` | 2,1 | :'' | ^2,2 [2,1 ]2,2 .2,1 | 1/1 mod=1 | - | 'Y' |
| r-o | `alpha beta\nsecond line here\nthird` | `iX<C-o>oY<Esc>` | F | `Xalpha beta\nY\nsecond line here\nthird` | 2,1 | :'' | ^2,2 [2,1 ]2,2 .2,1 | 2/2 mod=1 | - | 'Y' |
| r-O | `alpha beta\nsecond line here\nthird` | `jiX<C-o>OY<Esc>` | N | `alpha beta\nY\nXsecond line here\nthird` | 2,1 | :'' | ^2,2 [2,1 ]2,2 .2,1 | 1/1 mod=1 | - | 'Y' |
| r-O | `alpha beta\nsecond line here\nthird` | `jiX<C-o>OY<Esc>` | F | `alpha beta\nY\nXsecond line here\nthird` | 2,1 | :'' | ^2,2 [2,1 ]2,2 .2,1 | 2/2 mod=1 | - | 'Y' |
| r-cw | `alpha beta gamma` | `iX<C-o>cwY<Esc>` | N | `XY beta gamma` | 1,1 | v:'alpha' | ^1,2 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| r-cw | `alpha beta gamma` | `iX<C-o>cwY<Esc>` | F **≠N** | `XY beta gamma` | 1,2 | v:'alpha' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| r-cw | `alpha beta gamma` | `iX<C-o>cwY<Esc>` | T **≠N** | `XY beta gamma` | 1,2 | v:'alpha' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| r-s | `alpha beta gamma` | `iX<C-o>sY<Esc>` | N | `XYlpha beta gamma` | 1,1 | v:'a' | ^1,2 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| r-s | `alpha beta gamma` | `iX<C-o>sY<Esc>` | F **≠N** | `XYlpha beta gamma` | 1,2 | v:'a' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| r-s | `alpha beta gamma` | `iX<C-o>sY<Esc>` | T **≠N** | `XYlpha beta gamma` | 1,2 | v:'a' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| r-v-d | `alpha beta gamma` | `iX<C-o>vlldY<Esc>` | N | `XYha beta gamma` | 1,2 | v:'alp' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| r-v-d | `alpha beta gamma` | `iX<C-o>vlldY<Esc>` | F | `XYha beta gamma` | 1,2 | v:'alp' | ^1,3 [1,2 ]1,3 .1,2 | 3/3 mod=1 | - | 'Y' |
| r-v-esc | `alpha beta gamma` | `iX<C-o>vll<Esc>Y<Esc>` | N | `XalYpha beta gamma` | 1,4 | :'' | ^1,5 [1,4 ]1,5 .1,4 | 1/1 mod=1 | - | 'Y' |
| r-v-esc | `alpha beta gamma` | `iX<C-o>vll<Esc>Y<Esc>` | F | `XalYpha beta gamma` | 1,4 | :'' | ^1,5 [1,4 ]1,5 .1,4 | 2/2 mod=1 | - | 'Y' |
| r-v-y | `alpha beta gamma` | `iX<C-o>vllyY<Esc>` | N | `XYalpha beta gamma` | 1,2 | v:'alp' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| r-v-y | `alpha beta gamma` | `iX<C-o>vllyY<Esc>` | F | `XYalpha beta gamma` | 1,2 | v:'alp' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| r-V-d | `alpha beta\nsecond line here\nthird` | `iX<C-o>VdY<Esc>` | N | `Ysecond line here\nthird` | 1,1 | V:'Xalpha beta\n' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'Y' |
| r-V-d | `alpha beta\nsecond line here\nthird` | `iX<C-o>VdY<Esc>` | F | `Ysecond line here\nthird` | 1,1 | V:'Xalpha beta\n' | ^1,2 [1,1 ]1,2 .1,1 | 3/3 mod=1 | - | 'Y' |
| r-set | `alpha beta gamma` | `iX<C-o>:set so=2<CR>Y<Esc>` | N | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| r-set | `alpha beta gamma` | `iX<C-o>:set so=2<CR>Y<Esc>` | F | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| r-search | `alpha beta gamma` | `iX<C-o>/gam<CR>Y<Esc>` | N | `Xalpha beta Ygamma` | 1,13 | :'' | ^1,14 [1,13 ]1,14 .1,13 | 1/1 mod=1 | - | 'Y' |
| r-search | `alpha beta gamma` | `iX<C-o>/gam<CR>Y<Esc>` | F | `Xalpha beta Ygamma` | 1,13 | :'' | ^1,14 [1,13 ]1,14 .1,13 | 2/2 mod=1 | - | 'Y' |
| r-search-esc | `alpha beta gamma` | `iX<C-o>/gam<Esc>Y<Esc>` | N | `Xalpha beta Ygamma` | 1,13 | :'' | ^1,14 [1,13 ]1,14 .1,13 | 1/1 mod=1 | - | 'Y' |
| r-search-esc | `alpha beta gamma` | `iX<C-o>/gam<Esc>Y<Esc>` | F **≠N** | `Xalpha beta gamma` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'X' |
| r-search-esc | `alpha beta gamma` | `iX<C-o>/gam<Esc>Y<Esc>` | T **≠N** | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| r-zz | `alpha beta\nsecond line here\nthird` | `jiX<C-o>zzY<Esc>` | N | `alpha beta\nXYsecond line here\nthird` | 2,2 | :'' | ^2,3 [2,2 ]2,3 .2,2 | 1/1 mod=1 | - | 'Y' |
| r-zz | `alpha beta\nsecond line here\nthird` | `jiX<C-o>zzY<Esc>` | F | `alpha beta\nXYsecond line here\nthird` | 2,2 | :'' | ^2,3 [2,2 ]2,3 .2,2 | 2/2 mod=1 | - | 'Y' |
| r-ce | `alpha beta\nsecond line here\nthird` | `jiX<C-o><C-e>Y<Esc>` | N | `alpha beta\nXsecond line here\ntYhird` | 3,2 | :'' | ^3,3 [3,2 ]3,3 .3,2 | 1/1 mod=1 | - | 'Y' |
| r-ce | `alpha beta\nsecond line here\nthird` | `jiX<C-o><C-e>Y<Esc>` | F | `alpha beta\nXsecond line here\ntYhird` | 3,2 | :'' | ^3,3 [3,2 ]3,3 .3,2 | 2/2 mod=1 | - | 'Y' |
| r-ce | `alpha beta\nsecond line here\nthird` | `jiX<C-o><C-e>Y<Esc>` | T **≠N** | `alpha beta\nXYsecond line here\nthird` | 2,2 | :'' | ^2,3 [2,2 ]2,3 .2,2 | 1/1 mod=1 | - | 'Y' |
| s-esc | `alpha beta gamma` | `wiX<C-o><Esc>Y<Esc>` | N | `alpha XYbeta gamma` | 1,8 | :'' | ^1,9 [1,8 ]1,9 .1,8 | 1/1 mod=1 | - | 'Y' |
| s-esc | `alpha beta gamma` | `wiX<C-o><Esc>Y<Esc>` | F | `alpha XYbeta gamma` | 1,8 | :'' | ^1,9 [1,8 ]1,9 .1,8 | 2/2 mod=1 | - | 'Y' |
| s-coco | `alpha beta gamma` | `wiX<C-o><C-o>Y<Esc>` | N | `Yalpha Xbeta gamma` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'Y' |
| s-coco | `alpha beta gamma` | `wiX<C-o><C-o>Y<Esc>` | F | `Yalpha Xbeta gamma` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 2/2 mod=1 | - | 'Y' |
| s-twice | `alpha beta gamma` | `iX<C-o>l<C-o>lY<Esc>` | N | `XalYpha beta gamma` | 1,4 | :'' | ^1,5 [1,4 ]1,5 .1,4 | 1/1 mod=1 | - | 'Y' |
| s-twice | `alpha beta gamma` | `iX<C-o>l<C-o>lY<Esc>` | F | `XalYpha beta gamma` | 1,4 | :'' | ^1,5 [1,4 ]1,5 .1,4 | 2/2 mod=1 | - | 'Y' |
| s-eol-twice | `alpha beta gamma` | `AX<C-o>h<C-o>hY<Esc>` | N | `alpha beta gamYmaX` | 1,15 | :'' | ^1,16 [1,15 ]1,16 .1,15 | 1/1 mod=1 | - | 'Y' |
| s-eol-twice | `alpha beta gamma` | `AX<C-o>h<C-o>hY<Esc>` | F | `alpha beta gamYmaX` | 1,15 | :'' | ^1,16 [1,15 ]1,16 .1,15 | 2/2 mod=1 | - | 'Y' |
| s-empty-line | `alpha beta\nsecond line here\nthird\n\nlast` | `GkiX<C-o>kY<Esc>` | N | `alpha beta\nsecond line here\ntYhird\nX\nlast` | 3,2 | :'' | ^3,3 [3,2 ]3,3 .3,2 | 1/1 mod=1 | - | 'Y' |
| s-empty-line | `alpha beta\nsecond line here\nthird\n\nlast` | `GkiX<C-o>kY<Esc>` | F | `alpha beta\nsecond line here\ntYhird\nX\nlast` | 3,2 | :'' | ^3,3 [3,2 ]3,3 .3,2 | 2/2 mod=1 | - | 'Y' |
| s-empty-line2 | `\nab` | `i<C-o>jY<Esc>` | N | `\nYab` | 2,1 | :'' | ^2,2 [2,1 ]2,2 .2,1 | 1/1 mod=1 | - | 'Y' |
| s-empty-line2 | `\nab` | `i<C-o>jY<Esc>` | F | `\nYab` | 2,1 | :'' | ^2,2 [2,1 ]2,2 .2,1 | 1/1 mod=1 | - | 'Y' |
| s-empty-doc | `` | `iX<C-o>0Y<Esc>` | N | `YX` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'Y' |
| s-empty-doc | `` | `iX<C-o>0Y<Esc>` | F | `YX` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 2/2 mod=1 | - | 'Y' |
| s-empty-doc2 | `` | `i<C-o>xY<Esc>` | N | `Y` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'Y' |
| s-empty-doc2 | `` | `i<C-o>xY<Esc>` | F | `Y` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'Y' |
| s-wide | `あいう` | `AX<C-o>hY<Esc>` | N | `あいYうX` | 1,7 | :'' | ^1,8 [1,7 ]1,8 .1,7 | 1/1 mod=1 | - | 'Y' |
| s-wide | `あいう` | `AX<C-o>hY<Esc>` | F | `あいYうX` | 1,7 | :'' | ^1,8 [1,7 ]1,8 .1,7 | 2/2 mod=1 | - | 'Y' |
| s-wide2 | `あいう` | `liX<C-o>lY<Esc>` | N | `あXいYう` | 1,8 | :'' | ^1,9 [1,8 ]1,9 .1,8 | 1/1 mod=1 | - | 'Y' |
| s-wide2 | `あいう` | `liX<C-o>lY<Esc>` | F | `あXいYう` | 1,8 | :'' | ^1,9 [1,8 ]1,9 .1,8 | 2/2 mod=1 | - | 'Y' |
| s-comb | `éaé` | `iX<C-o>lY<Esc>` | N | `XéYaé` | 1,4 | :'' | ^1,5 [1,4 ]1,5 .1,4 | 1/1 mod=1 | - | 'Y' |
| s-comb | `éaé` | `iX<C-o>lY<Esc>` | F | `XéYaé` | 1,4 | :'' | ^1,5 [1,4 ]1,5 .1,4 | 2/2 mod=1 | - | 'Y' |
| s-op-pending-esc | `alpha beta gamma` | `iX<C-o>d<Esc>Y<Esc>` | N | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| s-op-pending-esc | `alpha beta gamma` | `iX<C-o>d<Esc>Y<Esc>` | F | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| s-reg-pending | `alpha beta gamma` | `iX<C-o>"a<Esc>Y<Esc>` | N | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| s-reg-pending | `alpha beta gamma` | `iX<C-o>"a<Esc>Y<Esc>` | F | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| s-count-esc | `alpha beta gamma` | `iX<C-o>3<Esc>Y<Esc>` | N | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| s-count-esc | `alpha beta gamma` | `iX<C-o>3<Esc>Y<Esc>` | F | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| s-end-no-esc | `alpha beta gamma` | `iX<C-o>l` | N | `Xalpha beta gamma` | 1,2 | :'' | ^1,3 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'X' |
| s-end-no-esc | `alpha beta gamma` | `iX<C-o>l` | F | `Xalpha beta gamma` | 1,2 | :'' | ^1,3 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'X' |
| s-end-no-esc2 | `alpha beta gamma` | `iX<C-o>lY` | N | `XaYlpha beta gamma` | 1,3 | :'' | ^1,4 [1,3 ]1,4 .1,3 | 1/1 mod=1 | - | 'Y' |
| s-end-no-esc2 | `alpha beta gamma` | `iX<C-o>lY` | F | `XaYlpha beta gamma` | 1,3 | :'' | ^1,4 [1,3 ]1,4 .1,3 | 2/2 mod=1 | - | 'Y' |
| c-3i | `alpha beta gamma` | `3iX<C-o>lY<Esc>` | N | `XaYlpha beta gamma` | 1,3 | :'' | ^1,4 [1,3 ]1,4 .1,3 | 1/1 mod=1 | - | 'Y' |
| c-3i | `alpha beta gamma` | `3iX<C-o>lY<Esc>` | F | `XaYlpha beta gamma` | 1,3 | :'' | ^1,4 [1,3 ]1,4 .1,3 | 2/2 mod=1 | - | 'Y' |
| c-3i-b | `alpha beta gamma` | `3iX<Esc>` | N | `XXXalpha beta gamma` | 1,3 | :'' | ^1,4 [1,1 ]1,4 .1,3 | 1/1 mod=1 | - | 'X' |
| c-3i-b | `alpha beta gamma` | `3iX<Esc>` | F | `XXXalpha beta gamma` | 1,3 | :'' | ^1,4 [1,1 ]1,4 .1,3 | 1/1 mod=1 | - | 'X' |
| c-3a | `alpha beta gamma` | `3aX<C-o>lY<Esc>` | N | `aXlYpha beta gamma` | 1,4 | :'' | ^1,5 [1,4 ]1,5 .1,4 | 1/1 mod=1 | - | 'Y' |
| c-3a | `alpha beta gamma` | `3aX<C-o>lY<Esc>` | F | `aXlYpha beta gamma` | 1,4 | :'' | ^1,5 [1,4 ]1,5 .1,4 | 2/2 mod=1 | - | 'Y' |
| c-3o | `alpha beta\nsecond line here\nthird` | `3oX<C-o>lY<Esc>` | N | `alpha beta\nX\nsecond line here\nthird` | 2,1 | :'' | ^2,2 [2,1 ]2,2 .2,1 | 1/1 mod=1 | - | 'X' |
| c-3o | `alpha beta\nsecond line here\nthird` | `3oX<C-o>lY<Esc>` | F **≠N** | `alpha beta\nXY\nsecond line here\nthird` | 2,2 | :'' | ^2,3 [2,2 ]2,3 .2,2 | 2/2 mod=1 | - | 'Y' |
| c-3o | `alpha beta\nsecond line here\nthird` | `3oX<C-o>lY<Esc>` | T **≠N** | `alpha beta\nXY\nsecond line here\nthird` | 2,2 | :'' | ^2,3 [2,2 ]2,3 .2,2 | 1/1 mod=1 | - | 'Y' |
| d-dot | `alpha beta gamma` | `iX<C-o>lY<Esc>w.` | N | `XaYlpha Ybeta gamma` | 1,9 | :'' | ^1,10 [1,9 ]1,10 .1,9 | 1/1 mod=1 | - | 'Y' |
| d-dot | `alpha beta gamma` | `iX<C-o>lY<Esc>w.` | F | `XaYlpha Ybeta gamma` | 1,9 | :'' | ^1,10 [1,9 ]1,10 .1,9 | 3/3 mod=1 | - | 'Y' |
| d-dot-eol | `alpha beta gamma` | `AX<C-o>hY<Esc>0.` | N | `Yalpha beta gammYaX` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'Y' |
| d-dot-eol | `alpha beta gamma` | `AX<C-o>hY<Esc>0.` | F | `Yalpha beta gammYaX` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 3/3 mod=1 | - | 'Y' |
| d-dot-nochange | `alpha beta gamma` | `iX<C-o>wY<Esc>0.` | N | `YXalpha Ybeta gamma` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'Y' |
| d-dot-nochange | `alpha beta gamma` | `iX<C-o>wY<Esc>0.` | F | `YXalpha Ybeta gamma` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 3/3 mod=1 | - | 'Y' |
| d-dot-x | `alpha beta gamma` | `iX<C-o>xY<Esc>w.` | N | `XYlpha Ybeta gamma` | 1,8 | v:'a' | ^1,9 [1,8 ]1,9 .1,8 | 1/1 mod=1 | - | 'Y' |
| d-dot-x | `alpha beta gamma` | `iX<C-o>xY<Esc>w.` | F | `XYlpha Ybeta gamma` | 1,8 | v:'a' | ^1,9 [1,8 ]1,9 .1,8 | 4/4 mod=1 | - | 'Y' |
| d-dot-x0 | `alpha beta gamma` | `i<C-o>xY<Esc>w.` | N | `Ylpha Ybeta gamma` | 1,7 | v:'a' | ^1,8 [1,7 ]1,8 .1,7 | 1/1 mod=1 | - | 'Y' |
| d-dot-x0 | `alpha beta gamma` | `i<C-o>xY<Esc>w.` | F | `Ylpha Ybeta gamma` | 1,7 | v:'a' | ^1,8 [1,7 ]1,8 .1,7 | 3/3 mod=1 | - | 'Y' |
| d-dot-only-x | `alpha beta gamma` | `i<C-o>x<Esc>w.` | N | `lpha eta gamma` | 1,6 | v:'b' | ^1,1 [1,6 ]1,6 .1,6 | 1/1 mod=1 | - | '' |
| d-dot-only-x | `alpha beta gamma` | `i<C-o>x<Esc>w.` | F | `lpha eta gamma` | 1,6 | v:'b' | ^1,1 [1,6 ]1,6 .1,6 | 2/2 mod=1 | - | '' |
| d-dot-dw | `alpha beta gamma` | `iX<C-o>dw<Esc>w.` | N | `Xbeta ` | 1,6 | v:'gamma' | ^1,2 [1,7 ]1,7 .1,7 | 1/1 mod=1 | - | 'X' |
| d-dot-dw | `alpha beta gamma` | `iX<C-o>dw<Esc>w.` | F | `Xbeta ` | 1,6 | v:'gamma' | ^1,2 [1,7 ]1,7 .1,7 | 3/3 mod=1 | - | 'X' |
| d-dot-3 | `alpha beta gamma` | `iX<C-o>lY<Esc>w3.` | N | `XaYlpha YYYbeta gamma` | 1,11 | :'' | ^1,12 [1,9 ]1,12 .1,11 | 1/1 mod=1 | - | 'Y' |
| d-dot-3 | `alpha beta gamma` | `iX<C-o>lY<Esc>w3.` | F | `XaYlpha YYYbeta gamma` | 1,11 | :'' | ^1,12 [1,9 ]1,12 .1,11 | 3/3 mod=1 | - | 'Y' |
| g-gi | `alpha beta\nsecond line here\nthird` | `iX<C-o>jY<Esc>ggwgiZ<Esc>` | N | `Xalpha beta\nsYZecond line here\nthird` | 2,3 | :'' | ^2,4 [2,3 ]2,4 .2,3 | 1/1 mod=1 | - | 'Z' |
| g-gi | `alpha beta\nsecond line here\nthird` | `iX<C-o>jY<Esc>ggwgiZ<Esc>` | F | `Xalpha beta\nsYZecond line here\nthird` | 2,3 | :'' | ^2,4 [2,3 ]2,4 .2,3 | 3/3 mod=1 | - | 'Z' |
| g-mark | `alpha beta\nsecond line here\nthird` | `liX<C-o>jY<Esc>gg`^` | N | `aXlpha beta\nseYcond line here\nthird` | 2,4 | :'' | ^2,4 [2,3 ]2,4 .2,3 | 1/1 mod=1 | - | 'Y' |
| g-mark | `alpha beta\nsecond line here\nthird` | `liX<C-o>jY<Esc>gg`^` | F | `aXlpha beta\nseYcond line here\nthird` | 2,4 | :'' | ^2,4 [2,3 ]2,4 .2,3 | 2/2 mod=1 | - | 'Y' |
| q-play | `alpha beta gamma` | `@a` | N | `XaYlpha beta gamma` | 1,3 | :'' a='iXlY' | ^1,4 [1,3 ]1,4 .1,3 | 1/1 mod=1 | - | 'Y' |
| q-play | `alpha beta gamma` | `@a` | F | `XaYlpha beta gamma` | 1,3 | :'' a='iXlY' | ^1,4 [1,3 ]1,4 .1,3 | 1/1 mod=1 | - | 'Y' |
| q-play-w | `alpha beta gamma` | `@aw@a` | N | `Xalpha Ybeta XgammYa` | 1,19 | :'' a='iXwY' | ^1,20 [1,19 ]1,20 .1,19 | 1/1 mod=1 | - | 'Y' |
| q-play-w | `alpha beta gamma` | `@aw@a` | F | `Xalpha Ybeta XgammYa` | 1,19 | :'' a='iXwY' | ^1,20 [1,19 ]1,20 .1,19 | 2/2 mod=1 | - | 'Y' |
| q-play-x | `alpha beta gamma` | `@a` | N | `XYlpha beta gamma` | 1,2 | v:'a' a='iXxY' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| q-play-x | `alpha beta gamma` | `@a` | F | `XYlpha beta gamma` | 1,2 | v:'a' a='iXxY' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| q-play-mid | `alpha beta gamma` | `iX<C-o>@aY<Esc>` | N | `XalYpha beta gamma` | 1,4 | :'' a='ll' | ^1,5 [1,4 ]1,5 .1,4 | 1/1 mod=1 | - | 'Y' |
| q-play-mid | `alpha beta gamma` | `iX<C-o>@aY<Esc>` | F | `XalYpha beta gamma` | 1,4 | :'' a='ll' | ^1,5 [1,4 ]1,5 .1,4 | 2/2 mod=1 | - | 'Y' |
| q-play-mid2 | `alpha beta gamma` | `iX<C-o>@aY<Esc>` | N | `XaYpha beta gamma` | 1,3 | v:'l' a='lx' | ^1,4 [1,3 ]1,4 .1,3 | 1/1 mod=1 | - | 'Y' |
| q-play-mid2 | `alpha beta gamma` | `iX<C-o>@aY<Esc>` | F | `XaYpha beta gamma` | 1,3 | v:'l' a='lx' | ^1,4 [1,3 ]1,4 .1,3 | 2/2 mod=1 | - | 'Y' |
| q-play-mid3 | `alpha beta gamma` | `iX<C-o>@aY<Esc>` | N | `XYZalpha beta gamma` | 1,2 | :'' a='iZ' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| q-play-mid3 | `alpha beta gamma` | `iX<C-o>@aY<Esc>` | F | `XYZalpha beta gamma` | 1,2 | :'' a='iZ' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| q-rec | `alpha beta gamma` | `qaiX<C-o>lY<Esc>qw@a` | N | `XaYlpha beta gamma` | 1,9 | :'' | ^1,4 [1,3 ]1,4 .1,3 | 1/1 mod=1 | - | 'Y' |
| q-rec | `alpha beta gamma` | `qaiX<C-o>lY<Esc>qw@a` | F **≠N** | `XaYlpha XbYeta gamma` | 1,11 | :'' a='iXlY' | ^1,12 [1,11 ]1,12 .1,11 | 3/3 mod=1 | - | 'Y' |
| q-rec | `alpha beta gamma` | `qaiX<C-o>lY<Esc>qw@a` | T **≠N** | `XaYlpha XbYeta gamma` | 1,11 | :'' a='iXlY' | ^1,12 [1,11 ]1,12 .1,11 | 1/1 mod=1 | - | 'Y' |
| q-rec-in | `alpha beta gamma` | `iX<C-o>qaY<Esc>` | N | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| q-rec-in | `alpha beta gamma` | `iX<C-o>qaY<Esc>` | F | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| q-rec-in2 | `alpha beta gamma` | `qaiX<C-o>qY<Esc>` | N | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| q-rec-in2 | `alpha beta gamma` | `qaiX<C-o>qY<Esc>` | F **≠N** | `XYalpha beta gamma` | 1,2 | :'' a='iX' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| q-rec-in2 | `alpha beta gamma` | `qaiX<C-o>qY<Esc>` | T **≠N** | `XYalpha beta gamma` | 1,2 | :'' a='iX' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| u-1 | `alpha beta gamma` | `iab<C-o>xcd<Esc>u` | N | `alpha beta gamma` | 1,1 | v:'a' | ^1,5 [1,1 ]1,1 .1,1 | 0/1 mod=0 | - | 'cd' |
| u-1 | `alpha beta gamma` | `iab<C-o>xcd<Esc>u` | F **≠N** | `ablpha beta gamma` | 1,3 | v:'a' | ^1,5 [1,1 ]1,1 .1,1 | 2/3 mod=1 | - | 'cd' |
| u-2 | `alpha beta gamma` | `iab<C-o>xcd<Esc>uu` | N | `alpha beta gamma` | 1,1 | v:'a' | ^1,5 [1,1 ]1,1 .1,1 | 0/1 mod=0 | - | 'cd' |
| u-2 | `alpha beta gamma` | `iab<C-o>xcd<Esc>uu` | F **≠N** | `abalpha beta gamma` | 1,3 | v:'a' | ^1,5 [1,1 ]1,1 .1,1 | 1/3 mod=1 | - | 'cd' |
| u-3 | `alpha beta gamma` | `iab<C-o>xcd<Esc>uuu` | N | `alpha beta gamma` | 1,1 | v:'a' | ^1,5 [1,1 ]1,1 .1,1 | 0/1 mod=0 | - | 'cd' |
| u-3 | `alpha beta gamma` | `iab<C-o>xcd<Esc>uuu` | F | `alpha beta gamma` | 1,1 | v:'a' | ^1,5 [1,1 ]1,1 .1,1 | 0/3 mod=0 | - | 'cd' |
| u-l1 | `alpha beta gamma` | `iab<C-o>lcd<Esc>u` | N | `alpha beta gamma` | 1,1 | :'' | ^1,6 [1,1 ]1,1 .1,1 | 0/1 mod=0 | - | 'cd' |
| u-l1 | `alpha beta gamma` | `iab<C-o>lcd<Esc>u` | F **≠N** | `abalpha beta gamma` | 1,4 | :'' | ^1,6 [1,1 ]1,1 .1,1 | 1/2 mod=1 | - | 'cd' |
| u-l2 | `alpha beta gamma` | `iab<C-o>lcd<Esc>uu` | N | `alpha beta gamma` | 1,1 | :'' | ^1,6 [1,1 ]1,1 .1,1 | 0/1 mod=0 | - | 'cd' |
| u-l2 | `alpha beta gamma` | `iab<C-o>lcd<Esc>uu` | F | `alpha beta gamma` | 1,1 | :'' | ^1,6 [1,1 ]1,1 .1,1 | 0/2 mod=0 | - | 'cd' |
| u-ab | `alpha beta gamma` | `iab<C-o><Esc><Esc>u` | N | `alpha beta gamma` | 1,1 | :'' | ^1,3 [1,1 ]1,1 .1,1 | 0/1 mod=0 | - | 'ab' |
| u-ab | `alpha beta gamma` | `iab<C-o><Esc><Esc>u` | F | `alpha beta gamma` | 1,1 | :'' | ^1,3 [1,1 ]1,1 .1,1 | 0/1 mod=0 | - | 'ab' |
| u-a-only | `alpha beta gamma` | `wiab<C-o>lcd<Esc>uu` | N | `alpha beta gamma` | 1,7 | :'' | ^1,12 [1,1 ]1,1 .1,1 | 0/1 mod=0 | - | 'cd' |
| u-a-only | `alpha beta gamma` | `wiab<C-o>lcd<Esc>uu` | F | `alpha beta gamma` | 1,7 | :'' | ^1,12 [1,1 ]1,1 .1,1 | 0/2 mod=0 | - | 'cd' |
| e-eol-esc | `alpha beta gamma` | `AX<C-o><Esc>` | N | `alpha beta gammaX` | 1,17 | :'' | ^1,18 [1,17 ]1,18 .1,17 | 1/1 mod=1 | - | 'X' |
| e-eol-esc | `alpha beta gamma` | `AX<C-o><Esc>` | F | `alpha beta gammaX` | 1,17 | :'' | ^1,18 [1,17 ]1,18 .1,17 | 1/1 mod=1 | - | 'X' |
| e-eol-esc-y | `alpha beta gamma` | `AX<C-o><Esc>Y<Esc>` | N | `alpha beta gammaXY` | 1,18 | :'' | ^1,19 [1,18 ]1,19 .1,18 | 1/1 mod=1 | - | 'Y' |
| e-eol-esc-y | `alpha beta gamma` | `AX<C-o><Esc>Y<Esc>` | F | `alpha beta gammaXY` | 1,18 | :'' | ^1,19 [1,18 ]1,19 .1,18 | 2/2 mod=1 | - | 'Y' |
| e-3i-esc | `alpha beta gamma` | `3iX<C-o><Esc>` | N | `Xalpha beta gamma` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'X' |
| e-3i-esc | `alpha beta gamma` | `3iX<C-o><Esc>` | F | `Xalpha beta gamma` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'X' |
| e-3i-i | `alpha beta gamma` | `3iX<C-o>iY<Esc>` | N | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| e-3i-i | `alpha beta gamma` | `3iX<C-o>iY<Esc>` | F | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| e-3i-dot | `alpha beta gamma` | `3iX<C-o>lY<Esc>w.` | N | `XaYlpha Ybeta gamma` | 1,9 | :'' | ^1,10 [1,9 ]1,10 .1,9 | 1/1 mod=1 | - | 'Y' |
| e-3i-dot | `alpha beta gamma` | `3iX<C-o>lY<Esc>w.` | F | `XaYlpha Ybeta gamma` | 1,9 | :'' | ^1,10 [1,9 ]1,10 .1,9 | 3/3 mod=1 | - | 'Y' |
| e-col-mid | `alpha beta gamma` | `wiX<C-o>:let g:m=col('.')<CR>Y<Esc>` | N | `alpha XYbeta gamma` | 1,8 | :'' | ^1,9 [1,8 ]1,9 .1,8 | 1/1 mod=1 | 8 | 'Y' |
| e-col-mid | `alpha beta gamma` | `wiX<C-o>:let g:m=col('.')<CR>Y<Esc>` | F | `alpha XYbeta gamma` | 1,8 | :'' | ^1,9 [1,8 ]1,9 .1,8 | 2/2 mod=1 | 8 | 'Y' |
| e-col-eol | `alpha beta gamma` | `AX<C-o>:let g:m=col('.')<CR>Y<Esc>` | N | `alpha beta gammaXY` | 1,18 | :'' | ^1,19 [1,18 ]1,19 .1,18 | 1/1 mod=1 | 17 | 'Y' |
| e-col-eol | `alpha beta gamma` | `AX<C-o>:let g:m=col('.')<CR>Y<Esc>` | F | `alpha beta gammaXY` | 1,18 | :'' | ^1,19 [1,18 ]1,19 .1,18 | 2/2 mod=1 | 17 | 'Y' |
| e-col-eol0 | `` | `i<C-o>:let g:m=col('.')<CR>Y<Esc>` | N | `Y` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | 1 | 'Y' |
| e-col-eol0 | `` | `i<C-o>:let g:m=col('.')<CR>Y<Esc>` | F | `Y` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | 1 | 'Y' |
| e-mark-mid | `alpha beta gamma` | `wiX<C-o>:let g:m=string(getpos("'^")[1:2])<CR>Y<Esc>` | N | `alpha XYbeta gamma` | 1,8 | :'' | ^1,9 [1,8 ]1,9 .1,8 | 1/1 mod=1 | [1, 8] | 'Y' |
| e-mark-mid | `alpha beta gamma` | `wiX<C-o>:let g:m=string(getpos("'^")[1:2])<CR>Y<Esc>` | F | `alpha XYbeta gamma` | 1,8 | :'' | ^1,9 [1,8 ]1,9 .1,8 | 2/2 mod=1 | [1, 8] | 'Y' |
| e-mark-eol | `alpha beta gamma` | `AX<C-o>:let g:m=string(getpos("'^")[1:2])<CR>Y<Esc>` | N | `alpha beta gammaXY` | 1,18 | :'' | ^1,19 [1,18 ]1,19 .1,18 | 1/1 mod=1 | [1, 18] | 'Y' |
| e-mark-eol | `alpha beta gamma` | `AX<C-o>:let g:m=string(getpos("'^")[1:2])<CR>Y<Esc>` | F | `alpha beta gammaXY` | 1,18 | :'' | ^1,19 [1,18 ]1,19 .1,18 | 2/2 mod=1 | [1, 18] | 'Y' |
| e-dotmark | `alpha beta gamma` | `wiX<C-o>:let g:m=string(getpos("'.")[1:2]).getreg('.')<CR>Y<Esc>` | N | `alpha XYbeta gamma` | 1,8 | :'' | ^1,9 [1,8 ]1,9 .1,8 | 1/1 mod=1 | [1, 7]X | 'Y' |
| e-dotmark | `alpha beta gamma` | `wiX<C-o>:let g:m=string(getpos("'.")[1:2]).getreg('.')<CR>Y<Esc>` | F | `alpha XYbeta gamma` | 1,8 | :'' | ^1,9 [1,8 ]1,9 .1,8 | 2/2 mod=1 | [1, 7]X | 'Y' |
| e-bs | `alpha beta gamma` | `wiabc<C-o>hd<BS><BS><BS>` | N | `alpha cbeta gamma` | 1,6 | :'' | ^1,7 [1,9 ]1,7 .1,7 | 1/1 mod=1 | - | 'd\x80kb\x80kb\x80kb' |
| e-bs | `alpha beta gamma` | `wiabc<C-o>hd<BS><BS><BS>` | F | `alpha cbeta gamma` | 1,6 | :'' | ^1,7 [1,9 ]1,7 .1,7 | 2/2 mod=1 | - | 'd\x80kb\x80kb\x80kb' |
| e-dd-last | `alpha beta gamma\nz` | `jAX<C-o>ddY<Esc>` | N | `Yalpha beta gamma` | 1,1 | V:'zX\n' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'Y' |
| e-dd-last | `alpha beta gamma\nz` | `jAX<C-o>ddY<Esc>` | F | `Yalpha beta gamma` | 1,1 | V:'zX\n' | ^1,2 [1,1 ]1,2 .1,1 | 3/3 mod=1 | - | 'Y' |
| e-o-count | `alpha beta gamma` | `iX<C-o>3oY<Esc>` | N | `Xalpha beta gamma\nY\nY\nY` | 4,1 | :'' | ^4,2 [2,1 ]4,2 .4,1 | 1/1 mod=1 | - | 'Y' |
| e-o-count | `alpha beta gamma` | `iX<C-o>3oY<Esc>` | F | `Xalpha beta gamma\nY\nY\nY` | 4,1 | :'' | ^4,2 [2,1 ]4,2 .4,1 | 2/2 mod=1 | - | 'Y' |
| e-x-count | `alpha beta gamma` | `iX<C-o>3xY<Esc>` | N | `XYha beta gamma` | 1,2 | v:'alp' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| e-x-count | `alpha beta gamma` | `iX<C-o>3xY<Esc>` | F | `XYha beta gamma` | 1,2 | v:'alp' | ^1,3 [1,2 ]1,3 .1,2 | 3/3 mod=1 | - | 'Y' |
| e-reg-count | `alpha beta gamma` | `iX<C-o>2"ayeY<Esc>` | N | `XYalpha beta gamma` | 1,2 | v:'alpha beta' a='alpha beta' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| e-reg-count | `alpha beta gamma` | `iX<C-o>2"ayeY<Esc>` | F | `XYalpha beta gamma` | 1,2 | v:'alpha beta' a='alpha beta' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| e-v-esc-eol | `alpha beta gamma` | `AX<C-o>vh<Esc>Y<Esc>` | N | `alpha beta gammYaX` | 1,16 | :'' | ^1,17 [1,16 ]1,17 .1,16 | 1/1 mod=1 | - | 'Y' |
| e-v-esc-eol | `alpha beta gamma` | `AX<C-o>vh<Esc>Y<Esc>` | F | `alpha beta gammYaX` | 1,16 | :'' | ^1,17 [1,16 ]1,17 .1,16 | 2/2 mod=1 | - | 'Y' |
| e-v-c | `alpha beta gamma` | `iX<C-o>vecY<Esc>` | N | `XY beta gamma` | 1,1 | v:'alpha' | ^1,2 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| e-v-c | `alpha beta gamma` | `iX<C-o>vecY<Esc>` | F **≠N** | `XY beta gamma` | 1,2 | v:'alpha' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| e-dot-o | `alpha beta gamma\nz` | `iX<C-o>oY<Esc>j.` | N | `Xalpha beta gamma\nY\nz\nY` | 4,1 | :'' | ^4,2 [4,1 ]4,2 .4,1 | 1/1 mod=1 | - | 'Y' |
| e-dot-o | `alpha beta gamma\nz` | `iX<C-o>oY<Esc>j.` | F | `Xalpha beta gamma\nY\nz\nY` | 4,1 | :'' | ^4,2 [4,1 ]4,2 .4,1 | 3/3 mod=1 | - | 'Y' |
| e-dot-cw | `alpha beta gamma` | `iX<C-o>cwY<Esc>w.` | N | `Xw.Y beta gamma` | 1,3 | v:'alpha' | ^1,4 [1,2 ]1,4 .1,2 | 1/1 mod=1 | - | 'w.' |
| e-dot-cw | `alpha beta gamma` | `iX<C-o>cwY<Esc>w.` | F **≠N** | `XY Y gamma` | 1,4 | v:'beta' | ^1,5 [1,4 ]1,5 .1,4 | 3/3 mod=1 | - | 'Y' |
| e-dot-A | `alpha beta gamma` | `iX<C-o>AY<Esc>0.` | N | `Xalpha beta gammaYY` | 1,19 | :'' | ^1,20 [1,19 ]1,20 .1,19 | 1/1 mod=1 | - | 'Y' |
| e-dot-A | `alpha beta gamma` | `iX<C-o>AY<Esc>0.` | F | `Xalpha beta gammaYY` | 1,19 | :'' | ^1,20 [1,19 ]1,20 .1,19 | 3/3 mod=1 | - | 'Y' |
| e-dot-nothing | `alpha beta gamma` | `xiX<C-o>l<Esc>w.` | N | `Xlpha Xbeta gamma` | 1,7 | v:'a' | ^1,8 [1,7 ]1,8 .1,7 | 1/1 mod=1 | - | 'X' |
| e-dot-nothing | `alpha beta gamma` | `xiX<C-o>l<Esc>w.` | F | `Xlpha Xbeta gamma` | 1,7 | v:'a' | ^1,8 [1,7 ]1,8 .1,7 | 3/3 mod=1 | - | 'X' |
| e-dot-after-nothing2 | `alpha beta gamma` | `xiX<C-o>l<Esc>` | N | `Xlpha beta gamma` | 1,2 | v:'a' | ^1,3 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'X' |
| e-dot-after-nothing2 | `alpha beta gamma` | `xiX<C-o>l<Esc>` | F | `Xlpha beta gamma` | 1,2 | v:'a' | ^1,3 [1,1 ]1,2 .1,1 | 2/2 mod=1 | - | 'X' |
| e-gi | `alpha beta gamma\nz` | `wiX<C-o>lY<Esc>Ggiq<Esc>` | N | `alpha XbYqeta gamma\nz` | 1,10 | :'' | ^1,11 [1,10 ]1,11 .1,10 | 1/1 mod=1 | - | 'q' |
| e-gi | `alpha beta gamma\nz` | `wiX<C-o>lY<Esc>Ggiq<Esc>` | F | `alpha XbYqeta gamma\nz` | 1,10 | :'' | ^1,11 [1,10 ]1,11 .1,10 | 3/3 mod=1 | - | 'q' |
| e-gi-nothing | `alpha beta gamma\nz` | `wiX<C-o>l<Esc>Ggiq<Esc>` | N | `alpha Xbqeta gamma\nz` | 1,9 | :'' | ^1,10 [1,9 ]1,10 .1,9 | 1/1 mod=1 | - | 'q' |
| e-gi-nothing | `alpha beta gamma\nz` | `wiX<C-o>l<Esc>Ggiq<Esc>` | F | `alpha Xbqeta gamma\nz` | 1,9 | :'' | ^1,10 [1,9 ]1,10 .1,9 | 2/2 mod=1 | - | 'q' |
| e-u-caret | `alpha beta gamma` | `wiab<C-o>xcd<Esc>u` | N | `alpha beta gamma` | 1,7 | v:'b' | ^1,11 [1,1 ]1,1 .1,1 | 0/1 mod=0 | - | 'cd' |
| e-u-caret | `alpha beta gamma` | `wiab<C-o>xcd<Esc>u` | F **≠N** | `alpha abeta gamma` | 1,9 | v:'b' | ^1,11 [1,1 ]1,1 .1,1 | 2/3 mod=1 | - | 'cd' |
| e-u-caret2 | `alpha beta gamma` | `wiab<C-o>xcd<Esc>uu` | N | `alpha beta gamma` | 1,7 | v:'b' | ^1,11 [1,1 ]1,1 .1,1 | 0/1 mod=0 | - | 'cd' |
| e-u-caret2 | `alpha beta gamma` | `wiab<C-o>xcd<Esc>uu` | F **≠N** | `alpha abbeta gamma` | 1,9 | v:'b' | ^1,11 [1,1 ]1,1 .1,1 | 1/3 mod=1 | - | 'cd' |
| e-u-caret3 | `alpha beta gamma` | `wiab<C-o>xcd<Esc>uuu` | N | `alpha beta gamma` | 1,7 | v:'b' | ^1,11 [1,1 ]1,1 .1,1 | 0/1 mod=0 | - | 'cd' |
| e-u-caret3 | `alpha beta gamma` | `wiab<C-o>xcd<Esc>uuu` | F | `alpha beta gamma` | 1,7 | v:'b' | ^1,11 [1,1 ]1,1 .1,1 | 0/3 mod=0 | - | 'cd' |
| e-wide-eol | `あいう` | `AX<C-o>:let g:m=col('.')<CR>Y<Esc>` | N | `あいうXY` | 1,11 | :'' | ^1,12 [1,11 ]1,12 .1,11 | 1/1 mod=1 | 10 | 'Y' |
| e-wide-eol | `あいう` | `AX<C-o>:let g:m=col('.')<CR>Y<Esc>` | F | `あいうXY` | 1,11 | :'' | ^1,12 [1,11 ]1,12 .1,11 | 2/2 mod=1 | 10 | 'Y' |
| e-wide-j | `あいう\nab` | `$i<C-o>jY<Esc>` | N | `あいう\nabY` | 2,3 | :'' | ^2,4 [2,3 ]2,4 .2,3 | 1/1 mod=1 | - | 'Y' |
| e-wide-j | `あいう\nab` | `$i<C-o>jY<Esc>` | F | `あいう\nabY` | 2,3 | :'' | ^2,4 [2,3 ]2,4 .2,3 | 1/1 mod=1 | - | 'Y' |
| e-comb-eol | `aé` | `A<C-o>hY<Esc>` | N | `Yaé` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'Y' |
| e-comb-eol | `aé` | `A<C-o>hY<Esc>` | F | `Yaé` | 1,1 | :'' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'Y' |
| e-comb-eol2 | `aé` | `A<C-o>:let g:m=col('.')<CR>Y<Esc>` | N | `aéY` | 1,5 | :'' | ^1,6 [1,5 ]1,6 .1,5 | 1/1 mod=1 | 2 | 'Y' |
| e-comb-eol2 | `aé` | `A<C-o>:let g:m=col('.')<CR>Y<Esc>` | F | `aéY` | 1,5 | :'' | ^1,6 [1,5 ]1,6 .1,5 | 1/1 mod=1 | 2 | 'Y' |
| e-rec | `alpha beta gamma` | `qaiX<C-o>lY<Esc>q` | N | `XaYlpha beta gamma` | 1,3 | :'' | ^1,4 [1,3 ]1,4 .1,3 | 1/1 mod=1 | - | 'Y' |
| e-rec | `alpha beta gamma` | `qaiX<C-o>lY<Esc>q` | F **≠N** | `XaYlpha beta gamma` | 1,3 | :'' a='iXlY' | ^1,4 [1,3 ]1,4 .1,3 | 2/2 mod=1 | - | 'Y' |
| e-rec-play | `alpha beta gamma` | `qaiX<C-o>lY<Esc>qw@a` | N | `XaYlpha beta gamma` | 1,9 | :'' | ^1,4 [1,3 ]1,4 .1,3 | 1/1 mod=1 | - | 'Y' |
| e-rec-play | `alpha beta gamma` | `qaiX<C-o>lY<Esc>qw@a` | F **≠N** | `XaYlpha XbYeta gamma` | 1,11 | :'' a='iXlY' | ^1,12 [1,11 ]1,12 .1,11 | 3/3 mod=1 | - | 'Y' |
| e-rec-play2 | `alpha beta gamma` | `qaiX<C-o>dwY<Esc>qu@a` | N | `alpha beta gamma` | 1,1 | v:'alpha ' | ^1,3 [1,1 ]1,1 .1,1 | 0/1 mod=0 | - | 'Y' |
| e-rec-play2 | `alpha beta gamma` | `qaiX<C-o>dwY<Esc>qu@a` | F **≠N** | `XXYgamma` | 1,3 | v:'beta ' a='iXdwY' | ^1,4 [1,3 ]1,4 .1,3 | 4/4 mod=1 | - | 'Y' |
| z-cw | `alpha beta gamma` | `iX<C-o>cwY<Esc>Z<Esc>` | N | `XZY beta gamma` | 1,2 | v:'alpha' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Z' |
| z-cw | `alpha beta gamma` | `iX<C-o>cwY<Esc>Z<Esc>` | F **≠N** | `XY beta gamma` | 1,2 | v:'alpha' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| z-s | `alpha beta gamma` | `iX<C-o>sY<Esc>Z<Esc>` | N | `XZYlpha beta gamma` | 1,2 | v:'a' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Z' |
| z-s | `alpha beta gamma` | `iX<C-o>sY<Esc>Z<Esc>` | F **≠N** | `XYlpha beta gamma` | 1,2 | v:'a' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| z-i | `alpha beta gamma` | `iX<C-o>iY<Esc>Z<Esc>` | N | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| z-i | `alpha beta gamma` | `iX<C-o>iY<Esc>Z<Esc>` | F | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| z-a | `alpha beta gamma` | `iX<C-o>aY<Esc>Z<Esc>` | N | `XaYlpha beta gamma` | 1,3 | :'' | ^1,4 [1,3 ]1,4 .1,3 | 1/1 mod=1 | - | 'Y' |
| z-a | `alpha beta gamma` | `iX<C-o>aY<Esc>Z<Esc>` | F | `XaYlpha beta gamma` | 1,3 | :'' | ^1,4 [1,3 ]1,4 .1,3 | 2/2 mod=1 | - | 'Y' |
| z-A | `alpha beta gamma` | `iX<C-o>AY<Esc>Z<Esc>` | N | `Xalpha beta gammaY` | 1,18 | :'' | ^1,19 [1,18 ]1,19 .1,18 | 1/1 mod=1 | - | 'Y' |
| z-A | `alpha beta gamma` | `iX<C-o>AY<Esc>Z<Esc>` | F | `Xalpha beta gammaY` | 1,18 | :'' | ^1,19 [1,18 ]1,19 .1,18 | 2/2 mod=1 | - | 'Y' |
| z-o | `alpha beta\nsecond line here\nthird` | `iX<C-o>oY<Esc>Z<Esc>` | N | `Xalpha beta\nY\nsecond line here\nthird` | 2,1 | :'' | ^2,2 [2,1 ]2,2 .2,1 | 1/1 mod=1 | - | 'Y' |
| z-o | `alpha beta\nsecond line here\nthird` | `iX<C-o>oY<Esc>Z<Esc>` | F | `Xalpha beta\nY\nsecond line here\nthird` | 2,1 | :'' | ^2,2 [2,1 ]2,2 .2,1 | 2/2 mod=1 | - | 'Y' |
| z-C | `alpha beta gamma` | `wiX<C-o>CY<Esc>Z<Esc>` | N | `alpha XZY` | 1,8 | v:'beta gamma' | ^1,9 [1,8 ]1,9 .1,8 | 1/1 mod=1 | - | 'Z' |
| z-C | `alpha beta gamma` | `wiX<C-o>CY<Esc>Z<Esc>` | F **≠N** | `alpha XY` | 1,8 | v:'beta gamma' | ^1,9 [1,8 ]1,9 .1,8 | 2/2 mod=1 | - | 'Y' |
| z-cc | `alpha beta\nsecond line here\nthird` | `iX<C-o>ccY<Esc>Z<Esc>` | N | `ZY\nsecond line here\nthird` | 1,1 | V:'Xalpha beta\n' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'Z' |
| z-cc | `alpha beta\nsecond line here\nthird` | `iX<C-o>ccY<Esc>Z<Esc>` | F **≠N** | `Y\nsecond line here\nthird` | 1,1 | V:'Xalpha beta\n' | ^1,2 [1,1 ]1,2 .1,1 | 2/2 mod=1 | - | 'Y' |
| z-S | `alpha beta\nsecond line here\nthird` | `iX<C-o>SY<Esc>Z<Esc>` | N | `ZY\nsecond line here\nthird` | 1,1 | V:'Xalpha beta\n' | ^1,2 [1,1 ]1,2 .1,1 | 1/1 mod=1 | - | 'Z' |
| z-S | `alpha beta\nsecond line here\nthird` | `iX<C-o>SY<Esc>Z<Esc>` | F **≠N** | `Y\nsecond line here\nthird` | 1,1 | V:'Xalpha beta\n' | ^1,2 [1,1 ]1,2 .1,1 | 2/2 mod=1 | - | 'Y' |
| z-vc | `alpha beta gamma` | `iX<C-o>vecY<Esc>Z<Esc>` | N | `XZY beta gamma` | 1,2 | v:'alpha' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Z' |
| z-vc | `alpha beta gamma` | `iX<C-o>vecY<Esc>Z<Esc>` | F **≠N** | `XY beta gamma` | 1,2 | v:'alpha' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| z-l | `alpha beta gamma` | `iX<C-o>lY<Esc>Z<Esc>` | N | `XaYlpha beta gamma` | 1,3 | :'' | ^1,4 [1,3 ]1,4 .1,3 | 1/1 mod=1 | - | 'Y' |
| z-l | `alpha beta gamma` | `iX<C-o>lY<Esc>Z<Esc>` | F | `XaYlpha beta gamma` | 1,3 | :'' | ^1,4 [1,3 ]1,4 .1,3 | 2/2 mod=1 | - | 'Y' |
| z-dot-i | `alpha beta gamma` | `xiX<C-o>.Y<Esc>Z<Esc>` | N | `XYpha beta gamma` | 1,2 | v:'l' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| z-dot-i | `alpha beta gamma` | `xiX<C-o>.Y<Esc>Z<Esc>` | F | `XYpha beta gamma` | 1,2 | v:'l' | ^1,3 [1,2 ]1,3 .1,2 | 4/4 mod=1 | - | 'Y' |
| z-dot-ins | `alpha beta gamma` | `iQ<Esc>wiX<C-o>.Y<Esc>Z<Esc>` | N | `Qalpha XYQbeta gamma` | 1,9 | :'' | ^1,10 [1,9 ]1,10 .1,9 | 1/1 mod=1 | - | 'Y' |
| z-dot-ins | `alpha beta gamma` | `iQ<Esc>wiX<C-o>.Y<Esc>Z<Esc>` | F | `Qalpha XYQbeta gamma` | 1,9 | :'' | ^1,10 [1,9 ]1,10 .1,9 | 4/4 mod=1 | - | 'Y' |
| z-gi | `alpha beta gamma` | `iX<C-o>giY<Esc>Z<Esc>` | N | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| z-gi | `alpha beta gamma` | `iX<C-o>giY<Esc>Z<Esc>` | F | `XYalpha beta gamma` | 1,2 | :'' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Y' |
| z-mac-cw | `alpha beta gamma` | `@aZ<Esc>` | N | `XZY beta gamma` | 1,2 | v:'alpha' a='iXcwY' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Z' |
| z-mac-cw | `alpha beta gamma` | `@aZ<Esc>` | F | `XZY beta gamma` | 1,2 | v:'alpha' a='iXcwY' | ^1,3 [1,2 ]1,3 .1,2 | 2/2 mod=1 | - | 'Z' |
| z-mac-i | `alpha beta gamma` | `@aZ<Esc>` | N | `XYalpha beta gamma` | 1,2 | :'' a='iXiY' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| z-mac-i | `alpha beta gamma` | `@aZ<Esc>` | F | `XYalpha beta gamma` | 1,2 | :'' a='iXiY' | ^1,3 [1,2 ]1,3 .1,2 | 1/1 mod=1 | - | 'Y' |
| e-3i-nothing-dot | `alpha beta gamma` | `3iX<C-o>l<Esc>w.` | N | `Xalpha XXXbeta gamma` | 1,10 | :'' | ^1,11 [1,8 ]1,11 .1,10 | 1/1 mod=1 | - | 'X' |
| e-3i-nothing-dot | `alpha beta gamma` | `3iX<C-o>l<Esc>w.` | F | `Xalpha XXXbeta gamma` | 1,10 | :'' | ^1,11 [1,8 ]1,11 .1,10 | 2/2 mod=1 | - | 'X' |
| e-3i-nothing-dot2 | `alpha beta gamma` | `3iX<C-o><Esc><Esc>w.` | N | `Xalpha XXXbeta gamma` | 1,10 | :'' | ^1,11 [1,8 ]1,11 .1,10 | 1/1 mod=1 | - | 'X' |
| e-3i-nothing-dot2 | `alpha beta gamma` | `3iX<C-o><Esc><Esc>w.` | F | `Xalpha XXXbeta gamma` | 1,10 | :'' | ^1,11 [1,8 ]1,11 .1,10 | 2/2 mod=1 | - | 'X' |
| e-i-nothing-dot | `alpha beta gamma` | `xi<C-o>l<Esc>w.` | N | `lpha beta gamma` | 1,5 | v:'a' | ^1,6 [1,6 ]1,6 .1,1 | 1/1 mod=1 | - | '' |
| e-i-nothing-dot | `alpha beta gamma` | `xi<C-o>l<Esc>w.` | F | `lpha beta gamma` | 1,5 | v:'a' | ^1,6 [1,6 ]1,6 .1,1 | 1/1 mod=1 | - | '' |
| e-bs-restart | `alpha beta gamma` | `wiabc<C-o>l<BS><Esc>` | N | `alpha abceta gamma` | 1,9 | :'' | ^1,10 [1,11 ]1,10 .1,10 | 1/1 mod=1 | - | '\x80kb' |
| e-bs-restart | `alpha beta gamma` | `wiabc<C-o>l<BS><Esc>` | F | `alpha abceta gamma` | 1,9 | :'' | ^1,10 [1,11 ]1,10 .1,10 | 2/2 mod=1 | - | '\x80kb' |
| e-cu | `alpha beta gamma` | `wiabc<C-o>hd<C-u><Esc>` | N | `alpha abcbeta gamma` | 1,8 | :'' | ^1,9 [1,9 ]1,9 .1,9 | 1/1 mod=1 | - | 'd' |
| e-cu | `alpha beta gamma` | `wiabc<C-o>hd<C-u><Esc>` | F | `alpha abcbeta gamma` | 1,8 | :'' | ^1,9 [1,9 ]1,9 .1,9 | 2/2 mod=1 | - | 'd' |
| e-cu2 | `alpha beta gamma` | `wAabc<C-o>0d<C-u><Esc>` | N | `alpha beta gammaabc` | 1,1 | :'' | ^1,1 [1,1 ]1,1 .1,1 | 1/1 mod=1 | - | 'd' |
| e-cu2 | `alpha beta gamma` | `wAabc<C-o>0d<C-u><Esc>` | F | `alpha beta gammaabc` | 1,1 | :'' | ^1,1 [1,1 ]1,1 .1,1 | 2/2 mod=1 | - | 'd' |
| e-cu3 | `alpha beta gamma` | `wiabc<C-o>ld<C-u><Esc>` | N | `alpha abcbeta gamma` | 1,10 | :'' | ^1,11 [1,11 ]1,11 .1,11 | 1/1 mod=1 | - | 'd' |
| e-cu3 | `alpha beta gamma` | `wiabc<C-o>ld<C-u><Esc>` | F | `alpha abcbeta gamma` | 1,10 | :'' | ^1,11 [1,11 ]1,11 .1,11 | 2/2 mod=1 | - | 'd' |
| e-cw | `alpha beta gamma` | `wiabc<C-o>hd<C-w><Esc>` | N | `alpha abcbeta gamma` | 1,8 | :'' | ^1,9 [1,9 ]1,9 .1,9 | 1/1 mod=1 | - | 'd' |
| e-cw | `alpha beta gamma` | `wiabc<C-o>hd<C-w><Esc>` | F | `alpha abcbeta gamma` | 1,8 | :'' | ^1,9 [1,9 ]1,9 .1,9 | 2/2 mod=1 | - | 'd' |
| e-cu-nomove | `alpha beta gamma` | `wiabc<C-o>:<CR>d<C-u><Esc>` | N | `alpha abcbeta gamma` | 1,9 | :'' | ^1,10 [1,10 ]1,10 .1,10 | 1/1 mod=1 | - | 'd' |
| e-cu-nomove | `alpha beta gamma` | `wiabc<C-o>:<CR>d<C-u><Esc>` | F | `alpha abcbeta gamma` | 1,9 | :'' | ^1,10 [1,10 ]1,10 .1,10 | 2/2 mod=1 | - | 'd' |
| e-cu-only | `alpha beta gamma` | `wiabc<C-o>h<C-u><Esc>` | N | `cbeta gamma` | 1,1 | :'' | ^1,1 [1,9 ]1,1 .1,1 | 1/1 mod=1 | - | '' |
| e-cu-only | `alpha beta gamma` | `wiabc<C-o>h<C-u><Esc>` | F | `cbeta gamma` | 1,1 | :'' | ^1,1 [1,9 ]1,1 .1,1 | 2/2 mod=1 | - | '' |
