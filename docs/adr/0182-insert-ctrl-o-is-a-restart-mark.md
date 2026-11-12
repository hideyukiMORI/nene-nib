# ADR 0182 — INSERT の Ctrl-o は「INSERT を抜けて、NORMAL の命令 1 つの後に始め直す印」

- 状態: 受理（設計席の決定・2026-11-12・#1517 の下ごしらえ G2 の実測 133 形から）
- 日付: 2026-11-12
- Issue: #1517
- 規則: ARC-001 / ARC-004 / ARC-012、CPP-002 / CPP-005、ADR 0012（engine の効果）/ 0014（IME）/ 0030（`.`）/ 0046（マクロ）/ 0052（undo の戻り先）/ 0152（`'^`）/ 0176（map）/ 0181（変更の印）

## 文脈

Vim の INSERT の `Ctrl-o` は、NORMAL の命令を 1 つだけ実行して INSERT へ戻る（`i_CTRL-O`）。Nib は鍵として扱わず、後ろの鍵が文字として入る（`liX<C-o>lY<Esc>` が Vim `aXlYpha`・Nib `aXlYlpha`）。

Vim 9.1.0004 の作り（実測 133 形と source・`D:/NeNeNib/outputs/20261112/1517/groundwork.md` を `docs/quality/vim-insert-ctrl-o-2026-11-12.md` に写す）:

- `<C-o>` は Esc とほぼ同じに INSERT を抜ける（`.` の記録を確定・`'^` と `".` を置く）。違いは 2 つ: キャレットを左へ戻すのは行の内容の終わりにいるときだけ・回数つきの INSERT（`3i`）の反復は捨てる。そして「始め直す印」`restart_edit` を立てる（edit.c の `ins_ctrl_o` と `ins_esc`）。
- NORMAL の命令が 1 つ終わると、印があれば INSERT を始め直す（normal.c の `normal_cmd` の終わり）。回数・オペレータ待ち・`"x`・VISUAL の間は戻らない。NORMAL の Esc は印を消さない（`<C-o><Esc>` も `<C-o>d<Esc>` も INSERT へ戻る）。
- 始め直すときの位置（edit.c の 341〜376 行）: 行末にいて同じ行に残ったとき、または欲しい列が仮想桁より大きいときは、最後の文字の後ろへ進める（`AX<C-o>lY` → `XY`）。
- `<C-o>c` `s` `C` `S` `cc` と VISUAL の `c` の後の Esc: 打った鍵では NORMAL へ戻り、マクロと `:normal!` の中ではもう一度 INSERT へ入る（ops.c の `KeyTyped`）。

## 決定

1. **形は「始め直す印」（下ごしらえの案 B）。** `VimMode` に値を足さない。`VimState` に印 `insert_restart`（`VimInsertRestart`・1 欄）を足し、`<C-o>` は INSERT を抜ける今の道（Esc の道）を通って印を立てる。印は段（`VimInsertRestartPhase` の `waiting` 命令を待つ・`due` 命令が終わった・`restarted` 始め直した INSERT でまだ何も打っていない＝Vim の arrow_used）と、位置の規則の o_lnum・ins_at_eol、`<C-o>` の前の INSERT で打っていたか（`<C-o>.` の側）、`<C-o>` が再生の中の鍵だったかを持つ。NORMAL と VISUAL の鍵の道は変えない。印が立っている間のモードは NORMAL（IME は切れる・キャレットはブロック・Ctrl+V は矩形・`map` は NORMAL の表・ADR 0014 / 0176 のまま Vim と一致）。
2. **「命令が 1 つ終わった」を決めるのは `vim_step` の出口の 1 か所、始め直すのは engine の `vim_restarted_insert` の 1 本。** 出口（`insert_restart_noted`・Vim の normal_cmd の終わりの条件）は、命令が 1 つ終わった（回数・オペレータ待ち・レジスタの前置き・1 鍵の引数の待ち・VISUAL・入力行を開いた鍵の中でない）step で印を `due` にするだけで、INSERT へ入れない。controller は鍵の効果を写して NORMAL の寄せを済ませた後に、印が `due` なら `vim_restarted_insert` を呼ぶ。理由は 2 つ: 始め直す位置の規則（Vim の式のまま・edit.c の 355〜375 行。`<C-o>` の前に行の内容の終わりにいて同じ行にまだいるか、欲しい列が今の桁より右で、最後の文字の上ならその後ろへ）は命令の後のカーソルを読み、`x` `dd` `u` の後のカーソルは効果を写した controller が決めること。Vim は `.` の再生の鍵が残る間（stuff_empty() が偽）は始め直さず、その列を知るのは controller であること（マクロの鍵は typeahead なので待たない）。何を始め直すか（位置・`i` と同じ入口 `entered_insert`・`.` の記録の印）を決めるのは engine の 1 本（ARC-004）で、第 2 の INSERT の入口は作らない。controller が呼ぶ口も 1 本（`restart_vim_insert`）で、鍵の後と engine の外の命令の後（決定 8）の 2 つの合図がそこを通る。印の持ち越しは `vim_resting_from` の 1 行（`waiting` と `due` を持ち越し、`restarted` は持ち越さない）。`0` `^` `<Home>` は ins_at_eol を下ろす（Vim の nv_beginline と nv_home・`iX<C-o>0Y` は `YX`）。
3. **`<C-o>` で抜けるときの違いは Esc の道の引数。** 左へ戻すのは行の内容の終わりにいるときだけ・回数の反復を捨てる・左へ戻しても欲しい列は戻す前の桁のまま（Vim の ins_esc は restart_edit があると w_set_curswant を立てない）。`'^` と `.` の記録の確定は Esc と同じ 1 本（記録には `<C-o>` の代わりに Esc を足す）。
4. **`<C-o>c` 系の後の Esc は鍵の出どころで分ける。** 打った鍵は NORMAL へ戻り、再生（マクロ・`.`・fixture の再生）は INSERT を始め直す。Vim の `KeyTyped` と同じ分け方で、engine が借りている鍵の出どころ（`VimEditorView` の source）を読む。新しい出どころの値は足さない（fixture の再生 press_vim_keys は :normal! の鍵なので `mapped` を借りる）。`i a A o O gi` は印を消し、印を立てた後の `.` の再生の中だけ保つ（Vim の invoke_edit の !stuff_empty()）。VISUAL の `I` `A` はいつも保つ（op_insert）。
5. **undo は打った鍵で 3 単位**（`<C-o>` の前に打った分・命令・始め直した後に打った分。Vim の対話と同じ）。再生の中は今までどおり全体で 1 単位（ADR 0046）。空の命令（移動だけ）は単位を作らない（ADR 0181 の印が空）。
6. **`.` は Vim のとおり。** 始め直した後に打って Esc した変更は、始め直してから打った鍵の `1i` として記録する（#1137 の「INSERT の中の矢印の後」と同じ仕組み）。何も打たずに Esc したら `<C-o>` の時点の記録が回数ごと残る。`<C-o>.` は INSERT を始める前の変更を繰り返すので、INSERT へ入る前の `last_change` を 1 つ置いておく欄を足す（ADR 0030 の追補）。
7. **マクロ**: 打った `<C-o>` は録る（`vim_macro_typed` の 1 本）。`<C-o>q` で録画を止めたときは末尾の `<C-o>` を外す。`<C-o>@a` は Vim の put_reedit_in_typebuf のとおり印を消し、再生の全部の周の後に `i` を 1 つ流す（`VimReplay.insert_after`）。
8. **engine の外で終わる命令の後**: Ex の行（`<C-o>:set so=2<CR>`）と入力行の取消（Ex と検索）の後は、controller が決定 2 の同じ口で `vim_restarted_insert` を呼んで始め直す（検索の確定は鍵として engine に戻るので決定 2 の道）。クリックは「命令 1 つ」と数えて、クリックした位置で始め直す。タブの切り替え（`vim_switched_document`）は印を捨てて NORMAL（今の規則）。`vim_interrupted` を通る engine の外の編集と割り込み（Ctrl+Z / Ctrl+Y・全選択・キャレットの移動の意図・通常モードの道の置き換え＝貼り付けや IME の確定）は印を捨てて NORMAL。engine の効果を写す置き換えは命令そのものなので捨てない。
9. **表示**: 印が立っている間、ステータスバーのモードは `(insert)`（Vim の `-- (insert) --`）。文字は core の表（`mode_label` に引数を足す）。
10. **測り方**: oracle の測定コード（`records_a_macro`）は変えない（全件の再生成をしない）。fixture（接頭辞 `insert-ctrl-o-`）は oracle で測れる形だけ: 移動・オペレータ・Esc の待ち・2 回続け・行末・空の行と空の文書・全角と結合文字・`3o`・`<C-o>v…`・`.`・`@a` の再生（register 欄）・`` `^ `` と `gi`・`<C-u>` の止まり・`<C-o>c` 系の再生の側。打った鍵の側（決定 4 と 5・録画・表示・IME・`/pat<Esc>`）は単体の契約 `--vim-insert-normal` で守る。
11. **fuzz**: 語は次の版（生成器 7）に足す。今の `WORDS` には足さない。

## 強制

- fixture `insert-ctrl-o-`（oracle・直す前に落ちること）と契約 `--vim-insert-normal`（打った鍵の 3 単位の undo・`<C-o>c…<Esc>` が NORMAL・録画・`(insert)` の表示・IME の構え・Ex と検索の後の始め直し・クリック・タブの切り替え）。
- 網羅: 印を `VimState` の比較と `vim_resting_from` に書き忘れると落ちる契約（印を立てて 38 か所の呼び出しを通す形は取れないので、代表の鍵の族ごとに「印が残る / 消える」の表を契約にする）。
- 状態: `active`（#1517 の実装の PR から）。fixture 125 件は直す前に落ち（register の無い 117 件のうち 116 件・直す前の木の再生器 nib_vim_replay で確かめた。register の 8 件は再生器が受けない）、契約は既定実行と `--vim-insert-normal` に載る。

## 帰結

- INSERT のまま 1 命令だけ打てる。NORMAL と VISUAL の鍵の表・IME・map は変わらない。
- fixture の再生（press_vim_keys）の鍵は打った鍵ではなくなったので、そこで `q` を録っていた既存の契約 1 件（`verify_window_register_put`）は打った鍵の道（press_vim_key）へ移した（:normal! の中では Vim も録らない）。
- `VimState` に欄が 2 つ（印と、INSERT の前の `last_change`）増える。1 打鍵の道の費用は欄の写しだけ。

## 残る穴

- `<C-o>zz` `<C-o><C-e>` の窓（viewport の fixture が要る）・REPLACE（#768 の `R` が未実装）・`:normal!` の中の undo の区切りの細部。
- `map` の右辺の途中でも命令が終われば始め直す（Vim は old_mapped_len の間は始め直さない）。
- `<C-o>` で抜けた矩形の `I` `A` は残りの行へ写さない（Vim は op_insert が edit() の後に写す）。
- map の右辺・矩形の写し・`zz` `<C-e>` の窓の 3 つは #1544。NORMAL の `s` は Nib に無い（#1436）ので `<C-o>s` は測っていない。
