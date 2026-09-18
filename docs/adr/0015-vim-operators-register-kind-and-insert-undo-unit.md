# ADR 0015 — Vim のオペレータは自分の回数を持ち、レジスタは種類を持つ LF の本文、INSERT の編集は 1 つの `Edit` に吸収する

- 状態: 受理
- 日付: 2026-09-18
- Issue: #43
- 影響する規則: ARC-001 / ARC-004 / ARC-007 / ARC-009 / CPP-002 / CPP-003 / CPP-006 / CPP-011 / CPP-012 / QLT-009 / QLT-013

## 文脈

ADR 0012 で Vim のエンジンは core の純関数 `vim_step` になり、鍵と効果は閉じた和型、再現度は oracle の fixture の再生で守られている。範囲は移動と `d` だけで、ADR 0012 が「正直に」に残した限界が 3 つある:
回数の掛け算（`2d3w` が 23 語）、INSERT の中の undo の単位（`ia<BS>b<Esc>u` が Vim と一致しない。`EditHistory` の `coalesce` は削除を伴う編集をつなげない）、
無名レジスタが文字列だけで行単位か文字単位かを持たないこと（`p` の貼り方はレジスタの種類で決まる）。
T2 の「オペレータ `d c y p P` ＋回数＋移動」（SPECIFICATION 第 5 節）を、この 3 つを解いた上で足す。

## 決定

**保留中のオペレータは自分の回数を持つ値 `VimPendingOperator`、無名レジスタは種類つきの `VimRegister`（本文は LF だけ）、`p` `P` は効果 `VimPutString` の 1 つ、
INSERT にいる間の編集は `EditBoundary::absorb` で直前の `Edit` に吸収して 1 つの undo の単位にする。`c` は「削除の効果 ＋ 次の状態が INSERT」で、新しい効果を足さない。**

1. **`VimPendingOperator{VimOperator op; std::optional<VimCount> count}`**（core・公開 aggregate）。`VimState::pending` はこの型。`d` `c` `y` を押した時点の回数をオペレータが持ち、
   その後に積んだ回数（`VimState::count`）と**掛け算**して動作の回数にする（`2d3w` = 6 語、`2d3d` = 6 行、`3dd` = 3 行）。どちらも無ければ 1。`VimState::count` の意味（いま積んでいる回数）は変えない
2. **`VimOperator { remove, change, yank }`**。範囲の規則（inclusive / exclusive・行単位）は 3 つで同じ 1 本（`removed_by_motion` を「範囲を決める」と「効果にする」に分ける）。
   `change` の効果は `VimRemoveRange`（行単位 `cc` `cj` `ck` は行の内容だけを消して行を 1 本残す）で、次の `VimState` が INSERT。**モードは状態が持つので、`c` に合成の効果は要らない**。
   `cw` は Vim の特例（語の上では `ce`）を engine に書き、oracle の fixture が守る。`yank` は本文を変えず（`VimMoveTo`）、キャレットは範囲の先頭（`yb` `yk` で動く。oracle が決める）
3. **`VimRegister{std::string text; VimRegisterKind kind}`**（`VimRegisterKind { characters, lines }`）。`VimState::unnamed_register` はこの型。**本文は LF だけ**: Vim のレジスタは「行の列」で、改行の形（CRLF / LF）は文書の性質（ADR 0010）。
   `dd` `yy` は CRLF の文書でも LF で入れる（engine が `text_range` の結果から CR を落とす純関数を 1 つ持つ）。oracle は `getregtype('"')` も取り（`v` / `V`）、fixture の再生が種類も突き合わせる
4. **`VimPutString{Offset at; std::string utf8; Offset caret}`** を `VimEffect` に足す（8 → 9）。engine が「どこに」「何を（回数ぶん繰り返した LF の本文）」「貼ったあとキャレットをどこに」を決め、
   controller は `at` に `utf8` の LF を `newline_of(line_ending())` に直して入れて `caret` に置く。**改行の形を決めるのは `VimNewLine` と同じ controller の 1 か所**（ARC-009）。
   文字単位は文字の後ろ（`p`）/ 前（`P`）、行単位は下の行 / 上の行（最終行の下は行末に改行から）。キャレットの行き先は oracle が決める（文字単位 1 行・複数行・行単位で fixture）
5. **`EditBoundary::absorb`**（3 つ目）。`EditHistory::pushed` は `absorb` で来た編集が直前の `Edit` に隣接していれば **1 つの `Edit` に畳む**（純関数 `absorbed(previous, edit) → std::optional<Edit>`）:
   `inserted` の直後の挿入は `inserted` に足す、`inserted` の末尾を消す削除は `inserted` を縮める、挿入開始より前を消す削除は `at` を前へ動かして `removed` の先頭に足す。隣接しなければ新しい単位。
   通常モードの `coalesce`（連続した文字入力だけ・ADR 0009 の決定 3）は変えない。**Vim モードで INSERT にいる間の `VimInsertString` / `VimNewLine` / `VimRemoveRange`（Backspace・`c` の削除）は `absorb`**、
   NORMAL の編集（`x` `d` `p`）は `separate`。境界は controller が「効果を写すときの Vim のモード」で決める（効果に境界を持たせない）。矢印・Home / End で動くと単位は切れる（`move_caret_to` が閉じる。Vim と同じ）
6. **鍵の追加は表の行**（CPP-012）: 移動 `e` `^`、`D` `C` `Y`（`d$` `c$` `yy`）、`<Home>` `<End>`（`VimSpecialKey` に `home` `end`。NORMAL は `0` `$`、INSERT は行の中で動く）。
   ui/win32 の表と oracle の `KEY_NAMES`、テストの `vim_keys_of` にそれぞれ 1 行
7. **undo の単位の検査は手書きの単体テスト**。oracle は `:normal!` 1 回がまるごと 1 単位なので、単位の境界を見分けられない（ADR 0012「正直に」）。fixture は本文・キャレット・レジスタ（種類つき）だけを突き合わせる
8. **やらないこと（3 本目以降）**: VISUAL（`v V Ctrl-v`）、画面の高さが要る移動（PgUp / PgDn・`Ctrl-d/u`・`H M L`。engine に見えている行数を渡す形を決めてから）、テキストオブジェクト、`.`、名前つきレジスタ、`> <`、`J r s S X`

## 強制

- fixture の再生（ADR 0005 / 0012）— **active**（`nib_unit` が `VimFixtures.hpp` の全項目を再生し、レジスタの種類も突き合わせる）
- 生成物と `fixtures.json` の一致（ADR 0012 の planned）— Issue #44（SHA-256 の規約検査）で別に active にする
- CPP-002: `VimOperator` / `VimRegisterKind` / `EditBoundary` の `switch` に `default` を書かない・`std::visit` の写し先を欠かさない — **active**（コンパイルと clang-tidy）
- ARC-007 / CPP-013: core から時刻・OS のシンボルが出ない — **active**（`eng/symbols.py`）
- undo の単位（決定 5）— 単体テスト（CTest）。`ia<BS>b<Esc>u`・`cwfoo<Esc>u` は 1 単位、`ia<Left>b<Esc>u` は 2 単位

## 結果

得られるもの: `d c y p P` ＋回数＋移動が Vim と同じに動き、INSERT 1 回が undo 1 単位になる。VISUAL は「範囲を決める」の 1 本に選択を渡す形で足せる。
失うもの: `Edit` は 1 つの連続した置換なので、INSERT の中で矢印で動いてから打った文字は別の単位になる（Vim も同じ）。レジスタを LF に正規化するので、CR だけの改行（`fileformat=mac`）の文書は最初から扱っていない。
正直に: `cw` の特例・`p` のキャレット・`yk` の行き先は oracle の結果に合わせる（実装の前に規則を書き切らない）。`e` の語の切れ目は `w` と同じ表（ハングル・絵文字は未測のまま）。

2026-09-18 に oracle で実測して分かったこと（Issue #43 の実装は下に合わせてある。詳細と SHA は `docs/quality/gate-proofs.md` 5-g）:

- **`ia<Left>b<Esc>u` は oracle では `hello` になる**（この実装は決定 5 のとおり 2 単位で `ahello`）。これは `:normal!` の 1 回がまるごと 1 つの undo 単位になるという
  **oracle の限界**であって、対話の Vim の振る舞いではない。対話の Vim は `:help ins-special-special` のとおり矢印・Home / End で単位を切る
  （"The changes (inserted or deleted characters) before and after these keys can be undone separately"）＝決定 5 のまま。
  fixture には置けない（置けば oracle の限界のほうに落ちる）ので、単体テスト `verify_vim_insert_motion_breaks_the_unit` が「切れること」を守る
- 行単位のオペレータと `j` `k` の回数は、**最終行（最初の行）にいるときだけ**失敗し、そうでなければ本文の端で止まる（Vim の `cursor_down` / `cursor_up`）。`5dd` は 3 行の本文を全部消す
- 範囲の規則は 3 つで同じ 1 本（決定 2）だが、Vim には**オペレータで違う 2 つの後処理**がある: exclusive な移動の言い換え（`:help exclusive`。`d` `c` `y` 共通）と、
  複数行にまたがる文字単位の削除だけが行単位になる `op_delete` の規則（`c` と `y` には無い）。`dw` が空行を丸ごと消すのも `2D` が行単位になるのもこれで、レジスタの種類は `V` になる
- `$` は回数を取る（`2$` は 1 行下の行末）。`D` `C` はその `$` に回数を渡す＝決定 6 の「`D` は `d$`」は回数つきでも成り立つ

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| `VimEffect` を列（`std::vector<VimEffect>`）にして `c` を「削除 ＋ INSERT へ」の 2 つで返す | モードは `VimState` が既に持つ。列にすると controller が「途中で失敗したら」を考える経路が増える。1 鍵 1 効果のまま足りる |
| レジスタの本文を文書の改行のまま持つ | engine は `line_ending()` を知らない。最終行の `dd` が足す改行の形を engine が決められず、改行の正典が 2 つになる（ARC-009） |
| Backspace の undo 単位を `coalesce` の規則を緩めて解く | 通常モードの Backspace は separate（ADR 0009 の決定 3）。規則を混ぜると通常モードの単位が変わる。Vim の単位は「INSERT 1 回」で別の概念 |
| INSERT の単位を controller が「INSERT に入ったときの履歴の長さ」で覚えて Esc で畳む | 履歴の外に第 2 の状態を持つ。`EditHistory` の中で `Edit` 1 つに畳めば、undo / redo / 保存の印がそのまま効く |
| 回数の掛け算を `VimState::count` に積算する | `2d` のあとの `3` は新しい回数で、`23` ではない。オペレータが自分の回数を持つのが Vim の構造（`opcount` と `count`） |
| fixture の `keys` を `:normal!` の列にして undo の単位を oracle で見る | `:normal!` 1 回の中の境界は見えないままで、偶然の一致（両方 1 単位）と本当の一致を見分けられない |

## 関連

ADR 0005（自前 Vim・oracle）・ADR 0009（`Edit`・undo の区切り）・ADR 0010（改行の形は文書の性質）・ADR 0012（エンジンの形・oracle の限界）。
