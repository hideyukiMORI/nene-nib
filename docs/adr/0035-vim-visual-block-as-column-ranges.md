# ADR 0035 — 矩形 VISUAL（`Ctrl-v`）は仮想桁の矩形を 1 本の範囲関数で決め、行ごとの範囲の列として編集する

- 状態: 受理（固定 Vim 9.1 の 228 ケースで確認し、決定 2・4・5・6・7 を直した。下の「補足」）
- 日付: 2026-09-22
- Issue: #112
- 影響する規則: FR-003 / ARC-001 / ARC-004 / ARC-007 / ARC-009 / CPP-002 / CPP-004 / CPP-006 / CPP-011 / CPP-012 / QLT-001 / QLT-008 / QLT-012 / QLT-013 / CNF-010

## 文脈

FR-003 の T2 に残る VISUAL の 3 つめ、矩形（`Ctrl-v`）を入れる。VISUAL は `VimMode` の値で種類を持ち（`visual` / `visual_line`・ADR 0018）、選択の正本は `EditorState` の `Selection`（anchor と caret の 2 つの `Offset`）で、`vim_visual_range` 1 本が「選択が覆う本文の範囲」を決めて描画・Ctrl+C/X・`d x y c` が同じ範囲を使う（ARC-001）。矩形は行ごとに別々の範囲になるので、1 つの `VimMotionRange` では表せない。桁は ADR 0034 の仮想桁で数える（Tab と全角の上で Vim と一致させるため。`$` は各行の行末まで）。

レジスタは `VimRegister{text, kind}` で種類は `characters` / `lines`（ADR 0015）。Vim の矩形レジスタは幅を持ち（`getregtype()` が `^V{幅}` を返す）、短い行へ貼るときは空白で埋める。

`.` は鍵の列と範囲の大きさで再生する（ADR 0030 / 0033）。矩形の変更は「行数 × 幅」の大きさを持つ。

窓の Ctrl+V はいま OS の貼付で、Vim の NORMAL / VISUAL でもそのまま貼付になっている。Vim では `Ctrl-v` が矩形の入口なので衝突する。Vim ソースは読まず、help（`:help blockwise-visual` / `:help v_b_$` / `:help v_o` / `:help v_O` / `:help v_b_r` / `:help blockwise-register`）と実測を根拠にする。

## 決定

**矩形は `VimMode::visual_block` として 3 つめの VISUAL にし、選択は anchor と caret のまま「仮想桁の矩形」と読む。行ごとの範囲は core の純関数 `vim_block_range` 1 本が決め、描画・Ctrl+C/X・`d x y r` と `.` の大きさがそれを使う。矩形の編集は「行ごとの範囲の列」を運ぶ効果で、controller が 1 つの undo 単位として写す。**

1. **モードと入口**: `VimMode` に `visual_block` を足す（switch の網羅性で写し漏れが落ちる）。NORMAL / `v` / `V` からの `Ctrl-v`（`VimSpecialKey::control_v`）で入り、`visual_block` での `v` / `V` は種類の切替、`Ctrl-v` と Esc は NORMAL へ戻る。INSERT の Ctrl+V は窓の OS 貼付のまま（Vim の literal 挿入は入れない）。Vim の NORMAL / VISUAL では Ctrl+V は矩形の鍵になり、OS 貼付は INSERT と通常モードだけ（決定 9）。
2. **矩形の幾何**: 左右の仮想桁は anchor と caret の `virtual_column` / `virtual_column_end`（ADR 0034）の小さいほうと大きいほう。`$`（`VimColumnWish::at_line_end`）が立っていれば右端は各行の内容の終わり。上下は anchor と caret の行。行ごとの範囲は `vim_block_range(text, selection, wish) -> VimBlockRange`（行ごとの `OffsetRange` の列と、左の仮想桁と幅）1 本が決め、左の桁より短い行は「行の内容の終わりの空範囲」、Tab や全角の途中に桁が掛かる文字は範囲に含める（Vim の既定・`virtualedit` 無し）。
3. **移動**: 既存の移動の表をそのまま使い、`j` `k` は欲しい列（仮想桁・ADR 0034）で着地する。`o` は対角の角へ、`O` は同じ行の反対の角へ（anchor と caret の桁だけ入れ替え）。
4. **編集**: `d` `x` は効果 `VimRemoveBlock{ranges}`（行ごとの範囲の列・上の行から）、`r` は `VimReplaceBlock{ranges, character}`、`y` は無名レジスタへ。controller は範囲の列を後ろの行から順に写し、履歴は前後で 1 回ずつ閉じて undo 1 単位にする。削除後のキャレットは矩形の左上（Vim の実測に従う）。`c` `I` `A` `C` `>` `<` `J` `~` `u` `U` `p`（VISUAL の中の貼付）は範囲外（次の縦切り）。
5. **レジスタ**: `VimRegisterKind` に `block` を足し、`VimRegister` に `width`（仮想桁の幅）を足す。本文は行を LF で結んだもの（末尾の改行なし）。`characters` / `lines` では `width` は使わない。~~`$` は「行末まで」の印~~ → **`$` の印は要らない**（補足 2）。
6. **矩形の貼付**: NORMAL の `p` / `P` は種類が `block` なら効果 `VimInsertBlock{at, lines, width}` で、キャレットの行から下へ 1 行ずつ同じ仮想桁に挿し、行が短ければ空白で埋め、行が足りなければ末尾に行を足す（Vim の `:help v_b_p` の既定）。回数は行ごとの繰り返し。貼付後のキャレットは Vim の実測に従う。
7. **`.`**: ADR 0033 の `VimVisualExtent` に `VimBlockExtent{lines, width | 行末まで}` を足し、再生は左上をキャレットにして同じ大きさの矩形を選び直す。鍵の列は `d x r` のまま。
8. **描画と OS の経路**: `EditorFrame` の各行の `SelectionSpan` は `visual_block` のとき `vim_block_range` の行ごとの範囲から作る（application の 1 か所。renderer は変えない）。Ctrl+C / Ctrl+X は矩形の本文を LF で結んで OS のクリップボードへ置き（改行は OS の形に直す既存の 1 か所）、Ctrl+X は `VimRemoveBlock` と同じ写しを通る。
9. **鍵の写し**: 窓の側で Ctrl+V を `VimSpecialKey::control_v` に写すのは、Vim モードの NORMAL / VISUAL のときだけ（既存の「窓の側で 1 か所だけ `VimKey` に写す」場所）。通常モードと Vim INSERT は OS 貼付のまま。
10. **oracle**: 鍵の記法に `<C-v>` を足す（`KEY_NAMES`）。`getregtype()` が返す `^V{幅}` を報告の `register_kind` にそのまま残し（`\x16` ＋ 10 進の幅）、fixture の生成側と unit の照合側で同じ文字列を比べる。

## 補足（2026-09-22・Issue #112 の実測）

実装の前に固定 Vim 9.1 を 228 ケース測った（`out/issue112-oracle/probe{,2,3,4,5,6}.py` の 109 / 39 / 25 / 21 / 20 / 14。`out/` は追跡外なので作業機にだけある）。Vim ソースは読んでいない。決定を 6 点直し、合わせられなかった差を 6 点残す。

**直した決定**

1. **決定 2 の「掛かる文字は範囲に含める」は半分だけ正しい。** 端が Tab や全角の途中に掛かると、Vim はその文字を**丸ごと外して掛かった桁ぶんの空白に置き換える**。しかも「取る空白」と「消したあとに残す空白」が逆で、取るのは矩形の**内側**の桁、残すのは**外側**の桁である（`	zz` の Tab（1〜8 桁）を 3〜9 桁の矩形で取ると `      z`＝内側 6 桁ぶんの空白、消すと `  z`＝外側 2 桁ぶんの空白）。`VimBlockLine` はこの 4 つ（`lead` / `tail` / `keep_lead` / `keep_tail`）を持つ。
2. **決定 5 の `$` の印は要らない。** `getregtype()` は `$` で取った矩形にも**有限の幅**を返す（`ab` / `cdef` / `gh` を `$` で取ると `^V4`）。Vim は取るときに「覆った行のうちいちばん長い行」へ幅を畳むので、レジスタに残る `$` は無い。`VimRegister.width` は `VimBlockWidth`（桁の数）1 つで足りる。
3. **決定 2 の `$` の右端は「行ごと」ではなく「いちばん長い行」で一度に決まる。** 結果は同じだが、幅が 1 つの数になるので矩形の形も `left` と `width` の 2 つで閉じる。
4. **決定 4 の `r` は文字の数ではなく桁の数だけ書く。** Tab 1 つを覆った矩形の `r` は Tab の桁数（6〜8 個）の文字になる（`ab<Tab>cd` の 3〜8 桁を `rZ` すると `abZZZZZZcd`）。
5. **決定 6 の貼付は、貼り先の行に本文が続くときだけ右を空白で埋める。** レジスタの行が幅より狭いときの埋めは矩形の形を保つためのもので、行末に貼るときは付かない。貼る桁が Tab の途中に落ちたら、その Tab だけを左右の空白に割る（ほかの文字は割らない）。
6. **決定 7 の再生は「選択の角」からではなく記録した幅から矩形を決める。** 角から読み直すと、短い行へ畳まれた角のぶんだけ矩形が痩せる（`cdef` / 空行 の上で 2 桁の記録を再生すると 1 桁になってしまう）。固定 Vim も `redo_VIsual_busy` のあいだは角を読み直さないので、`VimState` に `replayed_block` を 1 つ置いて同じ形にした。

**合わせていない差（fixture に採っていない）**

- **`.` の直前に `$` を打つと、固定 Vim は桁で記録した矩形も「行末まで」で再生する**（`curswant` が MAXCOL のまま再生に漏れる）。この engine は記録した大きさだけを使う。`.` の fixture は `$` を直前に打たないものだけを採った。
- **VISUAL の `u` / `U`（小文字化・大文字化）は範囲外だが、固定 Vim は鍵を食べて VISUAL から抜ける。** この engine は矩形のまま残る。
- **`.` のあとの `u`** は oracle（`:normal!` 1 回を 1 つの undo 単位にする）と単位が違う。矩形 1 つが 1 単位であることは `--vim-visual-block` の契約で測る。
- **貼る桁が Tab 以外の文字（全角）の途中に落ちたとき**、固定 Vim はその文字を貼った本文の後ろへ送る。この engine はその文字の先頭へ挿す。
- **矩形レジスタに Tab が入っているときの幅の数え方**が違う（固定 Vim は各文字を 0 桁目から数えるので Tab が常に 8 桁、この engine は行頭から順に数える）。
- **行が矩形より手前で終わる行の `y` `d` `r`** は 1 回の `:normal!` では再現しない（走査の失敗が残りの打鍵を捨てる・Issue #87）。fixture に採れないので `--vim-visual-block` の契約で測る。

## 強制

- モード・効果・レジスタの種類・大きさの和型の写し漏れ: **active**（`switch` / `std::visit` の網羅性・CPP-002）
- 矩形の幾何・編集・貼付・`.`・描画の範囲が 1 本の関数を通ること: **active**（対象 unit と oracle fixture・CNF-010。描画は application の unit）
- 期待値が本物の Vim の答えであること: **不能**（CI に Vim は無い）

## 結果

得られるもの: T2 の VISUAL が 3 種類そろい、矩形の `d x y r` と貼付が Tab / 全角の上で Vim と一致する（`block-` の fixture 144 件）。範囲の列を運ぶ効果は今後の `I A c` の土台になる。
失うもの・残る穴: Vim の NORMAL / VISUAL で Ctrl+V の OS 貼付が使えなくなる（`p` と INSERT の Ctrl+V で代替）。`I A c C > < J ~` と VISUAL の中の `p`、`virtualedit`、`gv` は後続。矩形の描画は既存の行ごとの `SelectionSpan` なので、短い行の右側（本文の無い所）は塗らない（Vim も既定では塗らない）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 選択の正本を「矩形専用の型」にして `EditorState` に持たせる | 選択の正本は anchor / caret の 1 つ（ADR 0009 / 0018）。矩形は読み方の違いで、型を分けると Ctrl+C / 描画 / `.` の経路が 2 本になる |
| 行ごとの削除を `VimRemoveRange` の列として複数の効果で返す | 1 鍵 1 効果を崩す（ADR 0030 の却下表と同じ）。範囲の列を 1 つの効果で運べば undo 単位も 1 か所で閉じる |
| 矩形の桁を code point で数える | Tab と全角で Vim と食い違う（Issue #91 で実測）。ADR 0034 が仮想桁を用意する |
| Vim モードでも Ctrl+V を OS 貼付のままにし、矩形は別の鍵にする | Vim と違う鍵は覚え直しになる。INSERT と通常モードの貼付は残るので不便は小さい |
| レジスタの幅を持たず貼付のたびに本文から計算する | `$` で取った矩形と短い行の埋め方が Vim と一致しない（`getregtype()` が幅を返すのは幅が本文だけでは決まらないから） |
