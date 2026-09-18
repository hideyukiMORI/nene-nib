# ADR 0018 — VISUAL は `VimMode` の 2 値で、engine は `Selection` を読み、選択を範囲に変える純関数 1 つでオペレータの経路に流す

- 状態: 受理
- 日付: 2026-09-19
- Issue: #53
- 影響する規則: ARC-001 / ARC-004 / ARC-007 / CPP-002 / CPP-003 / CPP-006 / CPP-011 / CPP-012 / QLT-009 / QLT-013

## 文脈

ADR 0012 は VISUAL を「`v` の範囲の規則（inclusive）と `Ctrl-v` の全角・タブが難所」として先送りし、ADR 0015（#43）で `vim_step` は「範囲を決める」（`motion_range` → `VimMotionRange{range, linewise}`）と「効果にする」（`removed` / `changed` / `yanked`）に分かれた。
選択は `EditorState` が `Selection{anchor, caret}` として所有し（ADR 0009 の決定 4・ARC-004）、通常モードの選択は `[anchor, caret)` の排他で、描画は `span_of` が `selection_range` から作る。
Vim の VISUAL は anchor と caret の**両端の文字を含む**（`v`）か**行まるごと**（`V`）で、キャレットは EOL の上にも置ける（`v$`）。制約: `VimState` は本文・キャレット・履歴を持たない（ADR 0012 の決定 1）。

## 決定

**VISUAL は `VimMode` に `visual` / `visual_line` の 2 値を足して表す。engine の入口は `Selection` を読み、効果 `VimSelect{Selection}` を 1 つ足す。選択 → 範囲は純関数 `vim_visual_range` の 1 本で、表示・Ctrl+C・`d x y c` が同じ範囲を使う。**

1. **`VimMode { normal, insert, visual, visual_line }`**。`mode_label` は `VISUAL` / `VISUAL LINE`、`CaretShape` はブロック、`follow_ime`（ADR 0014）は NORMAL と同じく IME を切る。閉じた enum に 2 値足すので、`switch` の写し先が足りない所はコンパイルが落ちる（CPP-002）
2. **`vim_step(const VimState&, const TextBuffer&, const Selection&, VimKey)`**。NORMAL / INSERT では anchor == caret で、engine は caret だけを読む。VISUAL では anchor を範囲の片端として読む。anchor を `VimState` に持たせない（選択の所有は `EditorState`・ARC-004）
3. **`VimSelect{Selection}`** を `VimEffect` に足す（9 → 10）。`v` `V` で入る（anchor = caret）・VISUAL の移動（anchor を保って caret を動かす）・`o`（入れ替え）が返す。controller は `EditorState` の選択をそのまま置き換える。VISUAL から出る（Esc・同じ鍵）は `VimMoveTo{caret}` で畳む（既存の `collapse`）
4. **`vim_visual_range(const TextBuffer&, const Selection&, VimMode) → VimMotionRange`**（core・純関数）: `visual` は小さいほうの位置から大きいほうの位置の**次の code point まで**（大きいほうが EOL の上なら改行を含む）、`visual_line` は小さいほうの行の行頭から大きいほうの行の終端まで（`linewise`）。NORMAL / INSERT で呼ぶと空の範囲（`switch` で網羅）
5. **表示と操作は同じ範囲**: controller の `span_of`（選択の描画）と Ctrl+C / Ctrl+X は VISUAL の間 `vim_visual_range` を使い、通常モードは `selection_range` のまま。`d x y c` は `vim_visual_range` の結果を #43 の `removed` / `changed` / `yanked` に流す（範囲 → 効果の経路は 1 本・ARC-001）。レジスタの種類は `visual` → `characters`・`visual_line` → `lines`。`c` は削除して INSERT（undo の単位は ADR 0015 の決定 5 のまま）
6. **キャレットの寄せ**: `settle_vim_caret` は VISUAL では寄せない（EOL の上に置ける）。VISUAL から NORMAL へ戻るときに寄せる（既存の経路）
7. **VISUAL で効く鍵**: 移動 `h j k l 0 $ w b e ^`・Home / End・矢印・回数・`o`・`d x y c`・Esc・`v` / `V`（同じ鍵で出る・違う鍵で種類を切り替える）。**効かない鍵は何もしない**（`u U ~ > < J p r I A gv` はこの縦切りでは無い。Vim の `u` は VISUAL では小文字化なので undo に流さない）。**`v` `V` の前の回数は Vim と同じく選ぶ量になる**（`3v` は 3 文字・`3V` は 3 行。`:help v` / `:help V`。2026-09-19 の改訂: 草稿は「回数を捨てる」だったが oracle と `:help` で誤りと分かった）
8. **やらないこと**: `Ctrl-v`（矩形。`virtcol` が要る）・マウスのドラッグで VISUAL（FR-003。anchor が `EditorState` にあるので、ドラッグが選択を置いて `visual` に切り替える形で足せる）・画面の高さが要る移動（PgUp / PgDn・`Ctrl-d/u`・`H M L`。engine に見えている行数を渡す形と、oracle の `-es` で `lines` が効くかの実測が先）

## 強制

- fixture の再生（ADR 0005 / 0012）— **active**（`nib_unit`。VISUAL の fixture を 61 件足して 195 → 256 件。fixture は NORMAL で終わる規約のまま）
- CPP-002: `VimMode` の `switch` に `default` を書かない・`std::visit` の写し先を欠かさない — **active**
- 表示の範囲と操作の範囲が同じ — 単体テスト（`vim_visual_range` の 4 形と、VISUAL の `EditorFrame` の選択スパンが `vim_visual_range` と一致すること）
- ARC-007 / CPP-013: core から時刻・OS のシンボルが出ない — **active**

## 結果

得られるもの: `v` `V` で選んで `d x y c`。ドラッグで VISUAL（FR-003）は選択を置いてモードを切り替えるだけで足せる。`Ctrl-v` は `vim_visual_range` に矩形の形を 1 つ足す位置が決まった。
失うもの: engine の入口の型が変わる（`Offset` → `Selection`）ので、#43 までの呼び出し（controller とテストの再生）は全部書き換える。`p` `~` `>` `<` `J` は VISUAL で効かない（何もしない）。
正直に: `v$` の改行の扱い・`vo` のあとのキャレット・行単位で `j` `k` の欲しい列・空行の `v` は oracle の結果に合わせる（実装の前に規則を書き切らない）。VISUAL の終わりの位置（`'<` `'>`）は fixture では見ない（`d` `y` の結果で範囲を確かめる）。

### oracle が決めたこと（Issue #53 の実測・2026-09-19）

fixture 61 件を Vim 9.1 に測らせて、規則は実装の前ではなく答えのほうから決まった。

1. **VISUAL のキャレットは行の内容の終わり（Vim が NUL を置く桁）に載る。** `v$d` は改行まで消して次の行と繋がり（`"abc\ndef"` → `"def"`・レジスタは `"abc\n"` の文字単位）、`vl` も行末に届く（`"abc\ndef"` に `llvld` で `"abdef"`）。Vim の `coladvance` の `one_more` が `VIsual_active` で立つのと同じで、決定 6 のとおり寄せるのは NORMAL へ戻るときだけ。空行の `v` は改行 1 つを選ぶ
2. **`op_delete` の「奇妙な Vi の振る舞い」は VISUAL には掛からない**（Vim の条件が `!oap->is_VIsual`）。`"  abc\n   "` の `vjd` は行単位にならず `"  abc\n "` を文字単位で消す。`d` ＋移動の経路（`whole_lines_for_delete`）とは別の入口を用意した
3. **決定 7 の「回数は VISUAL に入る前のものを捨てる」は Vim と違う。** `:help v` は「前の選択が無ければ `[count]` 文字を選ぶ。カーソルを右へ N 個ぶん動かすのと同じで、`'selection'` が `"exclusive"` でなければ 1 つ少ない」、`:help V` は同じく「`[count]` 行を選ぶ」と書いており、実測も `3v` が 3 文字・`3V` が 3 行だった。**実装は Vim に合わせて、入った直後に `count - 1` だけ `v` は右へ・`V` は下へ動かす**（ADR 0005 の「本物の Vim が正」）。決定 7 の括弧は同日に改訂した（上）
4. `V` の中の `$` `0` はキャレットだけ動いて範囲は行単位のまま・`Vy` のあとのキャレットは最初の行の同じ桁（最初の非空白ではない）・`vy` のあとは範囲の先頭、はどれも草稿どおりだった

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| `VimState` に anchor を持つ | 選択の正本が 2 つになる（ARC-004）。`EditorState` の `Selection` がそのまま anchor を持てる |
| `VimMode::visual` 1 値 ＋ `VimState::visual_kind` | 2 つの状態が矛盾しうる（`visual_kind` が `normal` で残る）。enum の 2 値なら `switch` の網羅で機械が守る |
| VISUAL の移動を `VimMoveTo` にして controller が「次のモードが VISUAL なら anchor を保つ」 | anchoring がモードから暗黙に決まり、`o`（入れ替え）には別の効果が要る。`VimSelect` 1 つで移動・入れ替え・入るを同じ形にする |
| 選択を排他 `[anchor, caret+1)` に置き換えて `selection_range` をそのまま使う | 描くキャレットが 1 つ右にずれる。位置は本当の位置のまま持ち、範囲だけを関数で決める |
| `x` を VISUAL では `d` の別名として表に書かず、`d` に写す | 表の行として `x` → `remove_character` を VISUAL では `d` と同じ動作に引くほうが、表 1 つで NORMAL と VISUAL の違いが読める |

## 関連

ADR 0009（選択と undo）・ADR 0012（engine の形）・ADR 0014（IME と Vim のモード）・ADR 0015（範囲 → 効果・レジスタの種類）・採用案 `docs/design/2026-09-15-editing-look.md`（D15・選択の描き方）。
