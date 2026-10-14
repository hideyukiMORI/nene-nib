# ADR 0129 — Vim の鍵は viewport を必要なときだけ求める

- 状態: 提案（設計席 2026-10-15。実装と性能の受理は #476 で）
- 日付: 2026-10-15
- Issue: #476
- 規則: ARC-001 / ARC-004 / ARC-012、CPP-002 / CPP-004 / CPP-005、QLT-001 / QLT-012 / QLT-014、ADR 0019 / 0114 / 0128

## 文脈

ADR 0128 でキャレット追従はスクロールを読む直前に 1 回だけ解決するようになり、通常モードの連打は 11.3 → 2.1 ms になった。しかし Vim の鍵は `EditorController::measured_vim_step` が `VimViewport`（先頭行・見えている行数）と `VimLayoutView.first_row` を鍵ごとに engine へ渡すので、鍵ごとに `settled_scroll()` が追従を解決し、キャレット行の DirectWrite layout（1 打鍵 40〜50 µs）を作る（ADR 0128 の決定 5）。

engine が viewport を読むのは画面行に関わる鍵（`H M L`・Ctrl-d/u/f/b・Ctrl-e/y・`z` 系）だけで、INSERT の文字や `h j k l` `w b` は読まない。engine には既に、必要になった行の画面行数を controller に後から求める `layout_request` の往復がある（ADR 0114）。

## 決定

1. **viewport は要求で渡す。** `VimEditorView.viewport` を `std::optional<VimViewport>` にし、`VimLayoutView.first_row` も同じ optional の中に畳む（先頭行・見えている行数・先頭行の画面行の 3 つを 1 つの値で）。engine が viewport を読む所は `view.viewport` が無ければ `VimStep.viewport_request = true` を立てて、効果を出さずに返す（`layout_request` と同じ形）。読む所は `VimStep.cpp` の 1 つの関数 `viewport_of(view)` に畳み、散らばった `view.viewport.` の直読みを無くす（ARC-012）。
2. **controller は要求に応えて同じ鍵をもう一度流す。** `measured_vim_step` は最初は viewport 無しで step し、`viewport_request` が立ったら `settled_scroll()` を読んで viewport を渡し、同じ鍵をもう一度 step する。`layout_request` の往復と同じループに統合し、往復の上限はこれまでの `layout_request` と同じ扱い（無限に回らない）。
3. **観測できる結果は変えない。** viewport を読む鍵は要求の往復の後に今日と同じ値で動くので、fixture 1853 件と Vim の scope は不変のまま通る。viewport を読まない鍵は追従を解決しないので、INSERT の連打は通常モードの連打と同じ水準になる。
4. **ADR 0128 の決定 5 は本 ADR で置き換える。**
5. **Vim の連打の bench は足さない（今回）。** 正式ゲートの 8 本に悪化が無いことを示し、Vim INSERT の連打の前後は実験ブランチの節目（`experiment/460-burst-breakdown` の形）で測って品質記録に残す。bench を足すかは hide に確かめてから。

## 却下した選択肢

- engine に controller の参照や callback を渡して viewport を遅延評価する: core が application を知る（ARC-003）。
- 鍵の種類を controller 側で見て viewport を渡すか決める: 「どの鍵が画面を読むか」の表が core と application の 2 か所になる（ARC-012）。

## 検証

- `--vim-*` の全 scope と `--application`（fixture 全件の再生を含む）。`H M L`・Ctrl-d/u/f/b・`zt zz zb`・Ctrl-e/y が要求の往復を通って同じ結果になる契約を engine の単体に 1 本。
- 正式ゲート 8 本に悪化が無いこと。Vim INSERT 200 打鍵の前後は節目で実測。

## 限界

要求の往復で、viewport を読む鍵は step が 2 回走る（1 回目は要求で途中まで）。画面行の移動は 1 打鍵ごとなので、費用は layout 1 回分より小さい。
