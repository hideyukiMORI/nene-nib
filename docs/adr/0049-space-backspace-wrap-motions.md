# ADR 0049 — `<Space>` `<BS>` は行をまたぐ `l` `h` で、オペレータ待ちと VISUAL では行末の位置に一度止まる

- 状態: 受理（設計席 2026-09-23・Issue #200。hide 未確認・引き継ぎ 09-23「6 回目の区切り時点」の次の順の 2 番目。決定 2・3 は #200 の実装の実測で設計席が補正）
- 日付: 2026-09-23
- Issue: #200
- 影響する規則: FR-003 / ARC-001 / CPP-002 / QLT-001 / QLT-012 / CNF-010 / CNF-011
- 前提: [ADR 0012](0012-vim-engine-first-slice-and-oracle-fixtures.md)（fixture は oracle）・[ADR 0018](0018-vim-visual-selection-as-range.md)（VISUAL）・[ADR 0048](0048-named-registers-as-one-text-table.md)（決定 9 の `<NL> <CR> + -` と決定 7 の特殊鍵の本文）・[ADR 0042](0042-unit-tests-split-by-scope.md)（決定 6・鍵の表は `VimKeyTable`・#198）

## 文脈

`:help <Space>`: `[count]l` と同じ。`:help whichwrap` の既定 `b,s` で `<BS>` と `<Space>` だけが行をまたぐ（`h` `l` はまたがない）。Nib は `<Space>` を未知の鍵として `refused`、NORMAL の `<BS>` は `cancelled` にしていて、`d<Space>` `d<BS>` は効かず、fixture の記法に `<Space>` と矢印が無い（#193 の工程 4 で見つけた穴）。

本物の Vim 9.1 の実測（Sonnet の probe `probe-space.md`・oracle と同じ設定）:

- NORMAL の裸の `<Space>` は行末から 1 打鍵で次の行の先頭へ、`<BS>` は行頭から前の行の末尾へ。文書の端では失敗（ビープ）。回数つきは 1 歩ずつ。
- `d<Space>` を行末で打つと**またがず**最後の文字を消す。`d<BS>` を行頭で打つと**改行だけ**を消して行を繋ぐ（レジスタは `\n`・`v`）。`3d<Space>` を `ab` の行頭で打つと 1 行目が**行単位**で消える（レジスタ `ab\n`・`V`）。
- VISUAL の `<Space>` は行末で 1 打鍵ぶん「足踏み」してから次の行へ、`<BS>` も対称。
- `execute "normal! " . keys` は先頭の空白を食う（`:help :normal`「{commands} は空白で始められない・`1 ` と書く」）。oracle は先頭が空白の `keys` を黙って違う期待値にする。

これらは Vim の `nv_right` / `nv_left` の 1 つの規則で説明がつく: **オペレータ待ちか VISUAL のときだけ、行末から右へ 1 歩進むと改行の上（行末の位置・Vim の NUL）に止まり、次の 1 歩で次の行へ移る**（`<BS>` は行頭から左へ 1 歩で前の行の行末の位置、次の 1 歩で最後の文字）。裸の NORMAL は行末の位置に止まれないので 1 歩で次の行へ行く。`d<Space>` は行末の位置までの exclusive 範囲＝最後の文字 1 つ、`d<BS>` は前の行の行末の位置から行頭までの範囲＝改行 1 つ、`3d<Space>` は 3 歩目で次の行の 0 桁目に着き、`:help exclusive` の「exclusive で終点が 0 桁目なら前の行の末尾までの inclusive・さらに始点が最初の非空白以前なら行単位」で行単位になる。VISUAL の「足踏み」は行末の位置に止まっているだけで、Esc が caret を最後の文字へ戻すので動いていないように見える。

## 決定

**`<Space>` `<BS>` は NORMAL / VISUAL / 矩形 VISUAL の移動で、`l` `h` に「行をまたぐ」を足したもの。オペレータ待ちと VISUAL では行末の位置に一度止まる。範囲の形は既存の exclusive の規則に任せ、fixture が Vim と同じ結果を守る。**

1. **鍵と motion**: `VimKeyTable` の表に `U' '` → `VimAction::space_right`、`VimSpecialKey::backspace`（NORMAL / VISUAL の `normal_special` / `visual_special`）→ `VimAction::space_left` を足し、`VimMotion::wrap_right` / `wrap_left` を足す（exclusive・文字単位）。NORMAL の `<BS>` は `cancelled` ではなく `acted()` を通る（オペレータ待ちの `d<BS>` も同じ口）。INSERT の `<Space>` `<BS>` は今までどおり文字と削除。
2. **1 歩の規則**: 右へ 1 歩は「行末の文字の上でなければ 1 文字右」。行末の文字の上なら、裸の NORMAL は次の行の 0 桁目（次の行が無ければ `not_moved` の失敗）、オペレータ待ち・VISUAL は改行の位置（行末の位置・その行の `\n` のオフセット）に止まり、既に改行の位置なら次の行の 0 桁目。左へ 1 歩は対称（0 桁目から、裸の NORMAL と **`y` のオペレータ待ち**は前の行の最後の文字、**`d` `c` のオペレータ待ち**と VISUAL は前の行の改行の位置、改行の位置からは最後の文字。1 行目の 0 桁目は失敗）。`y<BS>` が止まらないのは Vim の `nv_left` の「消す改行を含めるときだけ NUL に置く」特例（`OP_DELETE` / `OP_CHANGE` だけ）で、#200 の実装で実測して補正した（`ab\ncd` の `jy<BS>` は `b`）。止まる・渡るは閉じた enum `VimLineEndStop` で渡す。空行は 0 桁目と改行の位置が同じで 1 歩で通る。回数は 1 歩ずつ繰り返し、途中で失敗したら全体が失敗（`j` `k` と同じ・Vim も `n == count1` のときだけビープ）。
3. **範囲**: オペレータの範囲は exclusive の文字単位で、`:help exclusive` の 2 規則（終点が 0 桁目なら前の行の末尾までの inclusive・さらに始点が最初の非空白以前なら行単位）を `VimMotionRange` を作る 1 か所で適用する。**例外**: `d<BS>` `c<BS>` が文字のある行の改行の位置へ渡ったときは言い換えを通さない（Vim の `CA_NO_ADJ_OP_END`。通すと行頭の `d<BS>` が空の範囲になり「改行 1 つを消す」が出ない・`j2d<BS>` は `c\n`）。渡った先が空行なら通して行単位（`\ncd` の `jd<BS>` は `V`）。#200 の実装で実測して補正した。VISUAL では caret が改行の位置に来られる（`$` と同じ扱い・選択は改行を含む）。
4. **`.` と録画**: `d<Space>` は普通の変更として `.` に記録される（鍵は `VimCharacter{U' '}`）。マクロの本文は `' '` と `<80>kb`（ADR 0048 決定 7）。
5. **fixture の記法**: `eng/vim-oracle.py` の `KEY_NAMES` と `tests/unit/VimTestSupport.hpp` の `vim_key_names` に `<Space>` `<Left>` `<Right>` `<Up>` `<Down>` を足す（`<NL>` と同じ 2 か所・ARC-012 の既知の複製）。`keys` の生の空白は今までどおり文字。**先頭が空白（生でも `<Space>` でも）の `keys` は oracle が `1` を前置して `:normal!` に渡す**（`:help :normal` の書き方・回数 1 は無害・2 桁目の数字と繋がることは先頭が空白なので無い）。`records_a_macro`（#190）は空白と矢印を「コマンドでない鍵」として扱う（既存どおり）。
6. **fixture**: `space-` の接頭辞で 25 件程度: 行の途中・行末の越境・行頭の `<BS>`・文書の端の失敗・回数・空行・`d<Space>`（途中・行末）・`d<BS>`（途中・行頭）・`3d<Space>`（行単位）・`c<Space>` `y<Space>`・VISUAL の `v<Space>…` の足踏みと越境・`v<BS>`・矩形 VISUAL の `<C-v><Space>`・`d<Space>` の後の `.`・先頭が `<Space>` の keys（決定 5）・矢印の fixture（NORMAL の `<Left>` `<Down>`・INSERT の中の `<Left>`・`d<Right>`）。
7. **後続**: `whichwrap` の設定（`h` `l` `<` `>` `[` `]`）は無い（既定 `b,s` 固定）。`selection=exclusive` は無い。

## 強制

- fixture（決定 6）: **active**（CTest・CNF-010 / 011）。
- 先頭が空白の `keys` の `1` の前置: **active**（`eng/vim-oracle.py`・conformance の反例 1 つ: 前置無しで生成すると期待値が違うことを示す fixture は書かない。`probe_script` の出力に `1` が付くことを test で見る）。
- 閉じた enum の写し漏れ（`VimAction` / `VimMotion` の追加）: **active**（`switch` の網羅性・CPP-002・`every_action_has_one_row`）。

## 結果

得られるもの: `<Space>` `<BS>` の移動と `d c y` との組み合わせ・VISUAL の越境・fixture の記法の矢印と `<Space>`・oracle の先頭の空白の穴が閉じる。
失うもの・残る穴: `whichwrap` は固定。矩形 VISUAL の越境は Vim の実測が 1 桁ぶんだけ（fixture で固める）。exclusive の 2 規則が無ければ本 Issue で足すので `VimMotionRange` に触る。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| `<Space>` を `l` の別名にして行をまたがない | Vim の既定 `whichwrap=b,s` と違う。またぐのが Vim の日常の手 |
| VISUAL の「足踏み」を NORMAL / VISUAL の別の分岐で書く | 1 つの規則（行末の位置に止まれるか）で 3 つの実測が全部出る。分岐を増やすと `d<BS>` の「改行だけ消す」が説明できない |
| 先頭が空白の `keys` を oracle が拒む | 書ける fixture が減り、`1` の前置は Vim の help が示す正規の書き方 |
