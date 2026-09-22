# NeNe Nib の編集中の見た目 — 採用案（2026-09-15）

> Status: 採用（施主 hide が 2026-09-15 に `:` の行を「案 A」で選んだ。残りは描いたとおり）。判断の記録は [ADR 0009](../adr/0009-editing-slice-piece-table-and-editing-states.md)。
> 案は Claude の `/design` で起こした。キャンバス: https://claude.ai/code/artifact/f42a77d4-30f3-4867-8303-77686e2cf53c
> 正本の画は `editing/Main.dc.html`（通常モード）・`editing/Normal.dc.html`・`editing/Visual.dc.html`・`editing/ExLineA.dc.html`（採用）・`editing/ExLineB.dc.html`（不採用）。`editing/build_edit_boards.py` が生成する（`../look/build_boards.py` の配色と部品を使う）。

## 1. 決めたこと

| 状態 | 見た目 |
| --- | --- |
| 通常モードのキャレット | 2 DIP のバー・`accent`。INSERT も同じ |
| Vim NORMAL のキャレット | 1 文字分のブロック・`accent` の面に `on_accent` の字。角丸 1 |
| 選択（通常の Shift+移動・Vim の VISUAL） | `selection`（橙 28%）の面。行をまたぐときは行末まで。VISUAL LINE は行全体 |
| 検索の当たり（`/`） | `search`（淡い橙 35%）の面。現在の当たり（キャレットを含む一致）は `accent` の 1 DIP の枠。ステータスに `/pattern` と `n / N`。**実装済み（#123・ADR 0037）**: 見えている行だけを数え、既定はオン（D17）で `:nohlsearch` / `:set nohlsearch` で消える |
| IME の変換中 | 注目文節は `accent` の 2 DIP の下線と淡い面、他の文節は `ime`（淡い紫）の下線と字色 |
| 現在行 | `current_line` の面（既存） |
| `:` のコマンドライン | **案 A**: ステータスバーの左側（トグルとモード）が `:` の入力に置き換わる。右の項目は残る。補完の候補は入力の上に小さな面（`panel`）で出る。Esc で戻る |

## 2. 追加のトークン

| トークン | ダーク | ライト | 用途 |
| --- | --- | --- | --- |
| `search` | 橙 35%（#F0A47A 系） | 橙 30% | 検索の当たり |
| `ime` | #D7C4E5 | #5E2750 | IME の変換中の下線と字色（注目文節以外） |

`core::Palette` に 2 トークンを足して 16 にする。ハイライトの 3 色は別。

## 3. 実装との対応（Issue #7 の編集の縦切り）

| 画 | 実装 |
| --- | --- |
| 通常モードの選択・バーのキャレット・複数行 | Issue #7 で実装 |
| IME の変換中 | IME の縦切り（FR-012）で。トークンだけ先に持つ |
| Vim NORMAL / VISUAL / 検索 / `:` の行 | Vim エンジンの縦切りで。トークンと寸法は先に持つ |
