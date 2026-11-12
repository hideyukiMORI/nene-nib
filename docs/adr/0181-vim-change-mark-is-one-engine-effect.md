# ADR 0181 — Vim の鍵が「undo の単位を作ったか」と「本文を変えたと数えるか」は engine が効果に添える 1 つの印

- 状態: 受理（設計席の決定・2026-11-12・#1440 の下ごしらえ G1 の実測 80 形から）・実装 #1440
- 日付: 2026-11-12
- Issue: #1440（後続は #1374 / PR #1411 の再開・#1475・#793・#980）
- 規則: ARC-001 / ARC-004 / ARC-012、CPP-002 / CPP-005、ADR 0009（undo の単位）/ 0010（未保存の判定）/ 0012（engine の効果）/ 0052（`Edit.restore`）/ 0137（改訂番号）/ 0152（マーク）/ 0180（履歴の塊）

## 文脈

Vim の 1 鍵には独立した印が 2 つある（Vim 9.1.0004 の source と `-es -u NONE` の実測・表は [vim-change-marks-2026-11-12.md](../quality/vim-change-marks-2026-11-12.md)）。

- **U**: `u_save` を呼ぶ。undo の単位ができる。
- **C**: `changed_lines` / `changed_bytes` を呼ぶ。`b:changedtick` が進み、`'modified'`・`'.`・窓の先頭の決め直し（`VALID_TOPLINE`）が決まる。

実測した 80 形は「U ＋ C」「U だけ」「どちらも無し」のどれかで、「C だけ」は無かった。本文が結果として変わるかどうかは、どちらの印とも一致しない。

| 族 | 例 | U | C | 本文 |
|---|---|---|---|---|
| ふつうの編集 | `x` `dw` `ix<Esc>` | ○ | ○ | 変わる |
| 本文が同じでも C | 空の行・字下げの無い行の `>>` `<<`・同じ文字への `r`・同じ本文の VISUAL の `p`・`ix<BS><Esc>`・空の行の `cc<Esc>` | ○ | ○ | 同じ |
| U だけ | 英字の無い範囲・既にその大小の `~` `g~` `gu` `gU`・行頭の `X` `d0`・空の行の `x` `dl` `C`・失敗する `p`（空のレジスタ）と `<C-a>` `<C-x>`（数が無い） | ○ | × | 同じ |
| どちらも無し | 移動・yank・失敗する移動 | × | × | 同じ |

Nib の engine の効果は「本文を置き換える編集を返すか」しか持たないので、controller が改訂番号やバイト列から「Vim が変えたと数えるか」を推し量っていた。これは両向きに外れる（PR #1411 の 4 回のレビュー）。今ずれている形は 32（未保存の印と `'.` が 22 形・undo の単位が 16 形）。

## 決定

1. **engine は効果に印 `VimStep.change` を添える。** 型は閉じた `enum class VimChange { changed, saved_only }` の `std::optional`。
   - 空（`nullopt`）: Vim が `u_save` を呼ばない鍵（どちらも無し）。
   - `saved_only`: U だけ。
   - `changed`: U ＋ C。
   - 「C だけ」は Vim に無いので値を置かない。値が増えたら `switch` が落ちる（CPP-002）。
2. **規則は 1 本: 印は Vim が `u_save` を呼ぶときに限って立ち、`changed` は Vim が `changed_*` を呼ぶときに限る。** 本文が結果として同じかどうかでは決めない。engine は、本文を置き換える編集の効果を返すときは必ず印を立てる（`changed`）。逆に、U だけの形では本文を置き換える編集を返さない（今の「removed == inserted の編集を積む」大小の no-op をやめる）。
3. **本文を変えない単位は「空の単位」として履歴に積む。** `EditHistory` の 1 件は「編集（今の `Edit`）」か「空の単位」で、空の単位は本文に触れず、戻り先（`restore`）だけを持つ。`undo` `redo` は空の単位を 1 段として消費する（Vim と同じに、`u` が本文を変えない 1 段を戻す）。
   - 実装の注記（設計席が受理・2026-11-12）: 空の単位の後ろに INSERT の打鍵（absorb）が続けば、打鍵は空の単位に畳まれて 1 単位になる（空の行の `Cx<Esc>` は 1 回の `u` で戻る・Vim の op_change の u_save と edit() は 1 単位）。空の単位は前の単位に畳まれず、通常モードの打鍵（coalesce）は空の単位に畳まれない。0 バイトの文書の範囲が空の `d` は印なし・`c` は `saved_only`（Vim の `ML_EMPTY` の答え・#980 は決めない・fixture にしていない）。詳しくは下の実装の追補 1・4。
   - 印が立っていて編集の効果が無い鍵（U だけの形と、空の範囲の U ＋ C の形）で controller が積む。積む口は `EditorController::replace` と同じ 1 か所の並び（空 / 空の早帰りの所）で、engine の印からだけ積む。通常モード（Vim でないモード）は空の単位を作らない。
   - 空の単位は本文の置き換えを起こさないので、LSP の `didChange`・ミニマップ・構文の覚え・マークの行の直し・自動退避の本文には流れない（流す口を通らない）。
   - 本文が同じで範囲が空でない U ＋ C の形（同じ文字への `r` など）は、今までどおり removed == inserted の編集で積む（置き換えの口を通る。費用は今と同じ）。
4. **未保存の判定は「保存時点から今までに `changed` の単位があるか」。** `EditHistory` は各件に「本文を変えたと数える単位か」を持ち、`content_position()`（今の位置より前にある `changed` の件数）を返す。`Document.saved_position` と `save_state_of` は、履歴の位置の代わりにこの値で比べる（判定は今までどおり `save_state_of` の 1 か所・ADR 0010 の決定 7）。`saved_only` の単位は値を進めないので、未保存の印「● 」は付かない。`u` で保存時点の本文へ戻れば印は消える（今と同じ）。
   - 改訂番号 `revision()`（ADR 0137・自動退避の「前回から変わったか」）は今のまま全部の単位で進める。空の単位の後の退避は本文が同じで 1 回余分になるだけで、正しさは変わらない。
5. **`'.` を置くのは `changed` のときだけ。** 位置の決め方は今のまま（`vim_last_change_of`）で、畳んだ編集の頭になる違い（#1475・#793）はこの ADR では変えない。後で印に位置を添えるなら、この同じ欄を構造体に広げる。
6. **窓の先頭の決め直し（#1374 / PR #1411）は、この印を読む。** `keep_vim_window` は改訂番号とバイト列の比較をやめ、「印が `changed` か」と「キャレットの行が変わったか」で決める。Vim の `changed_common` が `VALID_TOPLINE` を落とすのは、変えた行がキャレットの行で、キャレットの桁が変えた桁以上のときだけ（`$x` `$D` は先頭を保つ・change.c）。この桁の条件は窓の規則なので印には入れず、#1411 の再開の席が controller の判定の 1 か所に Vim の式のまま写す（編集の位置とキャレットはどちらも controller が持つ値で、推し量らない）。
7. **失敗しながら U を作る形（空のレジスタの `p`・数の無い `<C-a>` `<C-x>`）も同じ印。** 失敗（`VimStep.failure`）と印 `saved_only` を同時に返す。再生の打ち切りは今のまま。
8. **`.` の記録・マクロの打ち切り・レジスタは変えない**（実測で全部一致している・`s` `S` の別の違いは別 Issue）。Nib の空の文書（0 バイト）を Vim の `ML_EMPTY` と数えるかは #980 のままで、この ADR では決めない。

## 強制

- 構造（active）: 本文を置き換える編集の効果（`vim_replaces_text`）を返す step の印は、`vim_step` の出口の 1 か所（`change_marked`）が `changed` に立てる。編集を返さない形の印（`saved_only` と範囲が空の `changed`）は効果を作る所が立てる（大小の `case_changed` `swapped_characters` `cased_block`・字下げの `shifted` `shifted_block`・`d` `c` の空の範囲の `empty_removal_noted`・空のレジスタと空の本文と `"_` の `p` の `put_step`・`<C-a>` `<C-x>` の `numbered` `visual_numbered`）。controller が空の単位を積む口は `push_empty_unit` の 1 か所（`stepped_vim` の効果の後の `perform_vim_effect`）。
- 契約（単体・active）: `--vim-change-mark`（`tests/unit/VimChangeMarkTests.cpp`・既定実行の契約に載る）。fixture 全件の鍵を engine に直に流し（最初の編集・失敗・文字組みの問いまで）、「本文を置き換える編集の効果を返した step は印が `changed`」「印が空の step は編集の効果を返さない」。engine の印の 14 形、未保存の印（`saved_only` の単位の後は保存済みのまま・空の範囲の `changed` の後は未保存・`u` で戻ると保存済み・`<C-r>` で未保存・保存の後の `saved_only` も保存済み・通常モードは空の単位を作らない）、空の行の `C` の後の打鍵が 1 単位になること。`--save-state` という scope は無いので、未保存の印の契約はこの scope に置いた。
- fixture（active）: oracle（`eng/vim-oracle.py`）は fixture の `change: true` だけで、鍵の前の `b:changedtick` と `undotree().seq_cur` を覚え、鍵の後の `&modified`・changedtick が進んだか（C）・seq_cur が進んだか（U・1 本の `:normal!` の中は 1 単位に畳まれるので真偽だけ）を測る。既存の fixture の json と生成物の行は 1 バイトも変えない（測定コードの commit を `--reuse-ref` にして `change-mark-` だけを測った・既存の 21 件の `change-mark-` と、`undo-` `shift-` `case-` `macro-` `replace-` `increment-` `viewport-` `register-` を新しい測定コードで測り直して生成物の差が 0）。再生（`verify_vim_fixture`）は未保存の印・`'.` の有無・`content_position() > 0`・履歴の位置 > 0 を比べる。接頭辞 `change-mark-` の新しい 91 件（印つき 86・`u` を含むキャレットの 5 件）と、書き直した `black-hole-put-` の 2 件（実装の追補 7）。表の 80 形のうち Nib に無い `R`・別の違いの `s` `S`・#980 に依る 0 バイトの文書の形・窓だけの形・`-es` で E353 の終了コードになる空のレジスタの `p` を除く。
- 再生器と fuzz（active・道具）: `nib_vim_replay` は行の最後の欄 `change=<m><c><u>` を読んで同じ 3 つを比べ（`changed` は `'.` の有無・Vim の changedtick は `u` でも進むので）、`eng/vim-fuzz.py --change` が oracle に印を測らせて欄を足す（既定は測らない）。
- 性質の試験（active）: `--edit-history-property` に空の単位の手書きの形と、空の単位を混ぜた固定 seed（1440〜1442）の列を素朴な模型（本文の位置は `[0, 位置)` の `changed` を数え直す）と照合する歩きを足した。
- planned: 群の間に undo の区切りを置く fixture の形（oracle の 1 本の `:normal!` は undo を区切れないので、鍵ごとの単位の数は測っていない。seq_cur の真偽と `u` の後のキャレットで代えた）。

## 帰結

- 未保存の印・undo の段・`'.`・窓の先頭の決め直しが、engine の 1 つの印から決まる。controller は推し量らない。
- `EditHistory` の 1 件に 1 バイトぶんの種類が増える（ADR 0180 の塊の形は変えない）。1 打鍵の道の費用は、印の 1 欄の写しだけ（#1440 の時点）。追補 #1374 の後は、engine が changed の鍵ごとに `position_of` を 1 回呼ぶ（鎖の外でも呼ぶ・索引の無い行では行頭からコードポイントを数える）。100 万文字の 1 行で INSERT の 2000 打鍵を Debug の再生器で 3 回ずつ流した比では、前の木と揺れの範囲だった（独立レビュー V-1543b の L3・正式な速さのゲートでは測っていない）。
- 通常モードの undo の単位は変わらない。

## 実装の追補（#1440・2026-11-12）

1. 空の単位の後ろに INSERT の打鍵（absorb）が続けば、打鍵は空の単位に畳まれて 1 単位になる（空の行の `Cx<Esc>` が 1 回の `u` で戻る・Vim の op_change の u_save と edit() は 1 単位）。畳んだ単位の印は、どちらかが `changed` なら `changed`。空の単位は前の単位に畳まれず、通常モードの打鍵（coalesce）は空の単位に畳まない。
2. 空の単位の `u` と `<C-r>` は本文に触れず、`'.` を単位の行に置く（Vim の u_undoredo は単位が保存した行を書き戻して changed_lines を呼ぶ・`~` を数字にした後の `ju` で `'.` が 1 行目になることを実測）。未保存の印は単位の印で決まる（`saved_only` の単位を戻しても保存済みのまま）。
3. `.` と `@` の再生が 2 つ以上の単位を積み、前後の本文が同じときは、今までどおり単位を残さない（`merge_replayed_edits` の `before.sealed()`）。Vim の再生は 1 単位なので `u` の段の数は違うが、ADR 0161 の決定 1（意図の中の編集が差し引き 0 なら版を進めず `didChange` を送らない）がこの据え置きに頼るので、この ADR では変えない（設計席の決定・残る穴・#1534）。単位が 1 つだけの再生（`~` を数字にした後の `.`）は空の単位がそのまま残る。
4. 範囲が空の `d` `c`（Vim の op_delete / op_change）: 0 バイトの文書は `d` が印なし・`c` が `saved_only`（Vim の `ML_EMPTY` の形・#980 は決めていないが、fixture の `text: ""` は `ML_EMPTY`）。それ以外の文書の行単位の `cc` は空の行でも `changed`（行単位の `d` の範囲は改行を含むので空にならない）。文字単位の空の範囲の `c` は `saved_only`、`d` は exclusive な移動（`x` `X` `d0` `dl` `dh` の行頭・`di(` の `()`）なら `saved_only`、inclusive な移動（`D` `d$` `diw` の空の行）なら印なし（Vim の op_delete は空の行の inclusive な `d` を u_save の前に返す）。VISUAL の範囲は空の行でも改行を覆うので空にならない（`vd` `vc` の空の行は U ＋ C・実測）。
5. 実測で足した形: 空の本文のレジスタの `p` は U だけ（Vim の do_put は先に u_save を呼ぶ）・空の行の `<C-a>` と数の無い `v<C-a>` は U だけ（op_addsub は先に u_save を呼ぶ）・矩形の大小の変わらない行は置き換えを返さない。
6. `VimStep.change` は `stepped_vim` の `settle_vim_step(step, before)` まで届く（決定 6 の #1411 の判定はこの PR に入れていない）。
7. **`"_p` は `saved_only`**（Vim の do_put は `"_` でも先に u_save を呼ぶ・U だけ・`-es` で実測・設計席の決定 案 A）。**既存の fixture を 2 件書き直した。** `black-hole-put-then-undo`（`x"_pu`）と `black-hole-put-dot-then-undo`（`x"_p.u`）は `u` を含むのに変更が 2 つ以上あり（CLAUDE.md の「`u` を含む fixture は変更が 1 つだけの鍵列で書く」に反する）、oracle の 1 本の `:normal!` が `x` と `"_p` を 1 単位に畳む答え（`u` が `x` まで戻す）に頼っていた。人が打つ Vim では `u` は `"_p` の空の単位だけを戻すので、印を付けると 2 件が落ちた。`black-hole-put-then-undo` は変更が `"_p` の 1 つだけの `yl"_pu`（`u` が空の単位を戻し、レジスタは `yl` の `a` のまま）にし、`black-hole-put-dot-then-undo` は `u` を使わない `black-hole-put-dot-keeps-register`（`yl"_p.`・`change: true`・`"_p` と `.` がレジスタを書かず、単位だけを作り未保存にしない）に書き直した。元の 2 件が守っていた「`"_p` はレジスタを書かない」「`.` で繰り返せる」は、この 2 件と既存の `black-hole-put-dot`（`x"_p.`）・`black-hole-put-unnamed-kept` が守る。fixture の json の差はこの 2 行だけ。
8. 大小の変わらない `~` `g~` `gu` `gU` と VISUAL の `~` `u` `U` は、本文を置き換えない空の置き換え（`VimReplaceRange` の空の範囲・着地の位置）を返す。`.` の記録は効果の種類で決まる（`changes_text`）ので、種類を保って記録を今のまま残す（`VimMoveTo` にすると `.` が大小の no-op を繰り返さなくなり、既存の fixture `empty-case-*` が落ちた）。

## 追補（#1374・2026-11-12）: changed の印は Vim が `changed_*` に渡した位置を持つ

決定 6 の窓の判定（`changed_common` の式）は「Vim が `changed_*` に渡した行と桁」を要る。controller が「最初の置き換えの始まり」から決めると、上端の行を編集しない矩形の操作（空の行・英字の無い行・もう大文字の行の `d` `x` `U` `~` `r` `>`・`$` の矩形を詰めた桁）で Vim より後ろの位置になった（PR #1543 の独立レビュー V-1543 の M1・11 形）。設計席の決定で、位置も engine が印に添える。

1. **型。** `VimStep.change` は `std::optional<VimChangeMark>`（`VimChangeMark.hpp`・`VimChange kind` と `std::optional<TextPosition> at`）。`changed` は必ず位置を持ち、`saved_only` は持たない（契約 `--vim-change-mark` が fixture 全件の step で確かめる）。位置は鍵の前の本文の座標（置き換えの始まりより前は鍵の後も同じ）。
2. **書く所。** `vim_step` の出口の `change_marked` が、changed で位置の無い印に既定の位置（`change_position`・効果ごとの多重定義で、効果が増えたらコンパイルが落ちる）を書く。矩形の `r` と矩形の INSERT の 1 行目の埋めは作る所が `change_placed` で書く。

   | 鍵の族 | Vim の `changed_*`（v9.1.0004） | 印の位置 |
   |---|---|---|
   | ふつうの削除・置き換え（`x` `d` `c` `r` `s` `J` `gJ`・大小・VISUAL） | `del_bytes` / `changed_bytes` / `changed_lines(start.lnum, start.col)`・`do_join` の (行, 元の行の長さ) | 置き換えの範囲の始まり |
   | 行単位のオペレータ・`>` `<`（文字単位と行単位） | `changed_lines(start.lnum, 0, ...)` | 範囲の始まり（行頭） |
   | INSERT の打鍵・Enter | `ins_char_bytes` の `changed_bytes(行, col)`・`open_line` | 鍵の前のカーソル |
   | `p` `P`（文字単位・行単位） | `changed_lines(cursor.lnum, col)`・`changed_lines(b_op_start.lnum, 0)`・1 行は `changed_cline_bef_curs` も | 貼る所（効果の置き換えの始まり） |
   | 矩形の `d` `x` `c`（`op_delete`） | `changed_lines(cursor.lnum, cursor.col)`（`check_cursor_col` の後） | 左上の行の行頭（カーソルの行ではカーソルの桁が必ずそれ以上なので同じ答え） |
   | 矩形の `~` `u` `U`（`op_tilde`）・`>` `<`（`op_shift`）・VISUAL の `<C-a>` `<C-x>`（`op_addsub`） | `changed_lines(start.lnum, 0, ...)` | 左上の行の行頭 |
   | 矩形の `r`（`op_replace`） | `changed_lines(start.lnum, start.col, ...)` | 矩形の左上（`change_placed`） |
   | 矩形の INSERT の 1 行目の埋め | `changed_bytes(行, 埋めの始まり)` | 埋めの始まり（`change_placed`） |
   | 矩形の `I` `A` `c` の Esc（`block_insert`・`op_change`） | `changed_lines(start.lnum + 1, 0, ...)`（1 行目は打鍵が済ませた） | 左上の次の行の行頭 |
   | 矩形の貼付（`do_put` の `MBLOCK`） | `changed_lines(lnum, 0, ...)`（呼ぶときのカーソルは貼った行の下） | 左上の行の行頭 |
   | 空の範囲の changed（空の行の `>>` `cc`・字下げの無い行の `<<`） | `changed_lines(start.lnum, 0)` / `changed_bytes(行, 0)` | 範囲の始まりか鍵の前のカーソル |
   | `.` とマクロの再生 | 再生の中の鍵ごとの印 | 再生の中の鍵がそれぞれ持つ（再生を返した外の鍵は窓の送りを頼む側で、判定を引き継がない） |

3. **`u` と `<C-r>`。** engine の効果は「戻せ」だけで印を立てない（`u_save` を呼ばない）。Vim の `u_undoredo` は `changed_lines(top + 1, 0, ...)` を呼ぶが、戻す行は履歴を持つ controller だけが知るので、controller が戻した編集の行の行頭を位置にする（ADR 0019 の追補 #1374 の 4）。
4. **controller。** 鍵の始めに印の位置を覚えるだけで、`replace` `apply_block` `push_empty_unit` は位置を覚えない（覚える所は `stepped_vim` の 1 か所と `u` / `<C-r>`）。
5. **強制の範囲（独立レビュー V-1543b の L2）。** 契約 `--vim-change-mark` が見るのは型の不変条件（changed は位置を持ち、saved_only は持たない）だけ。`change_marked` は位置の無い changed にいつも既定の位置を書くので、「Vim の式が既定と違う効果で `change_placed` を書き忘れた」は契約では捕まらない。矩形の位置を守るのは fixture `viewport-half-after-delete-block-` 12 件（窓の先頭で見る）だけ。
6. **矩形の削除の前提（V-1543b の L4）。** 表の「左上の行の行頭で同じ答え」は、鍵の後のキャレットが Vim の `changed_lines` を呼ぶ時点のカーソル（`check_cursor_col` で詰めた左上）と同じ行・桁にあることに立つ。`o` `O` で角を入れ替えた形・`$` の矩形・右下から選んだ形でも、鍵の後のキャレットは左上に来る（V-1543b の s6 / s7 の 16 形で Vim の 3 通りの流し方と一致）。

## 残る穴

- `'.` の位置（#1475・#793）・`ML_EMPTY`（#980）・`s` `S` の失敗の違い（G1 の表の注）。
- 本文が同じに戻る 2 単位以上の再生（`@a` の `ix<Esc>x`・`xP`）は単位を残さない（実装の追補 3・ADR 0161 の決定 1・#1534）。この形（`@a` = `xP`・数字の上の `@a` = `~l~`）では undo の単位だけでなく、未保存の印と `'.` も Vim と違う（Vim は modified=1・Nib は 0・独立レビュー V-1530 の実測）。本文は保存時点と同じなので、本文を失う向きの違いではない。
- 対話の Vim は鍵ごとに undo を区切り、`:normal!` と再生の中では区切らない。Nib の区切り（ADR 0009・0046）との細かい差は fixture の形に依るので、実装の席が測って限界に書く。
