# ADR 0129 — Vim の鍵は viewport を必要なときだけ求める

- 状態: 提案（設計席 2026-10-15。実装と性能の受理は #476 で）
- 日付: 2026-10-15
- Issue: #476
- 規則: ARC-001 / ARC-004 / ARC-012、CPP-002 / CPP-004 / CPP-005、QLT-001 / QLT-012 / QLT-014、ADR 0019 / 0114 / 0128

## 文脈

ADR 0128 でキャレット追従はスクロールを読む直前に 1 回だけ解決するようになり、通常モードの連打は 11.3 → 2.1 ms になった。しかし Vim の鍵は `EditorController::measured_vim_step` が `VimViewport`（先頭行・見えている行数）と `VimLayoutView.first_row` を鍵ごとに engine へ渡すので、鍵ごとに `settled_scroll()` が追従を解決し、キャレット行の DirectWrite layout（1 打鍵 40〜50 µs）を作る（ADR 0128 の決定 5）。

engine が viewport を読むのは画面行に関わる鍵（`H M L`・Ctrl-d/u/f/b・PgUp/PgDn。Ctrl-e/y と `zt zz zb` は engine にまだ無い）だけで、INSERT の文字や `h j k l` `w b` は読まない。engine には既に、必要になった行の画面行数を controller に後から求める `layout_request` の往復がある（ADR 0114）。

## 決定

1. **viewport は要求で渡す。** `VimEditorView.viewport` を `std::optional<VimViewport>` にし、`VimLayoutView.first_row` も同じ optional の中に畳む（先頭行・見えている行数・先頭行の画面行の 3 つを 1 つの値で）。engine が viewport を読む所は `view.viewport` が無ければ `VimStep.viewport_request = true` を立てて、効果を出さずに返す（`layout_request` と同じ形）。読む所は `VimStep.hpp` の公開関数 `vim_viewport_of(view)` の 1 本に畳み（`VimScreenGeometry.cpp` も同じ口）、散らばった `view.viewport.` の直読みを無くす（ARC-012）。viewport を読む鍵の表は `vim_reads_viewport` の 1 つで、`H M L`・Ctrl-d/u/f/b に横送り（ADR 0145）と縦の位置合わせ `screen_alignment`（`zt zz zb z<CR> z. z-`・ADR 0146）が加わる。
2. **controller は要求に応えて同じ鍵をもう一度流す。** `measured_vim_step` は最初は viewport 無しで step し、`viewport_request` が立ったら `settled_scroll()` を読んで viewport を渡し、同じ鍵をもう一度 step する。`layout_request` の往復と同じループに統合し、往復の上限はこれまでの `layout_request` と同じ扱い（無限に回らない）。（追補 2026-10-18・#665: 回数の上限は持たない。往復は毎回「新しい行の画面行数」「新しい (行, 段) の画面行」「viewport 1 回」のどれかを足し、同じ問いの立て直しと viewport の二重要求は即 `inconsistent` で止まる（ADR 0133 の追補）ので、新しい答えは「行数 + 画面行の総数 + 1」で尽きる。固定の上限は `100gj` や大きな窓の Ctrl-f の正しい往復を拒否する。）
3. **観測できる結果は変えない。** viewport を読む鍵は要求の往復の後に今日と同じ値で動くので、fixture 1853 件と Vim の scope は不変のまま通る。viewport を読まない鍵は追従を解決しないので、INSERT の連打は通常モードの連打と同じ水準になる。
4. **ADR 0128 の決定 5 は本 ADR で置き換える。**（追補 2026-10-22・#785: 通常 → Vim のトグルはキャレットを追わず、解決済みの viewport の中へキャレットを入れる。トグルは鍵ではないので engine の viewport の往復を通らない・ADR 0012 の決定 10 の追補。）
5. **Vim の連打の bench は足さない（今回）。** 正式ゲートの 8 本に悪化が無いことを示し、Vim INSERT の連打の前後は実験ブランチの節目（`experiment/460-burst-breakdown` の形）で測って品質記録に残す。bench を足すかは hide に確かめてから。

## 却下した選択肢

- engine に controller の参照や callback を渡して viewport を遅延評価する: core が application を知る（ARC-003）。
- 鍵の種類を controller 側で見て viewport を渡すか決める: 「どの鍵が画面を読むか」の表が core と application の 2 か所になる（ARC-012）。

## 検証

- `--vim-*` の全 scope と `--application`（fixture 全件の再生を含む）。`H M L`・Ctrl-d/u/f/b・`zt zz zb`・Ctrl-e/y が要求の往復を通って同じ結果になる契約を engine の単体に 1 本。
- 正式ゲート 8 本に悪化が無いこと。Vim INSERT 200 打鍵の前後は節目で実測。

## 限界

要求の往復で、viewport を読む鍵は step が 2 回走る（1 回目は要求で途中まで）。画面行の移動は 1 打鍵ごとなので、費用は layout 1 回分より小さい。

**追従の時期（独立レビュー F1）。** ADR 0128 の前は Vim の鍵ごとに追従を解決していた。本 ADR では viewport を読まない鍵（`j` `k` や INSERT の文字）は解決せず、次に viewport を読む鍵か `frame()` で 1 回解決する。打った鍵は 1 鍵ごとに WM_PAINT が来るので、人が打つ速さでは鍵ごとに解決される。差が出るのは、まとめて届いた鍵（自動リピート・貼り付けた鍵）と、マクロ `@a` と `.` の再生の中で、`15j10k` のように画面の外へ出て戻る列がある場合: 通常モードと同じく最小の移動で解決するので、途中の画面を経由した即時の結果と先頭行が違い得る（キャレットは見える）。本物の Vim は stuff バッファ（マクロ・`.`・`:normal!`）の実行中は `main_loop` の `update_topline()` を呼ばず、画面の先頭行を直さない（設計席の記憶・未実測）ので、再生の中で鍵ごとに解決しない本 ADR の形はそれに近い。fixture（`:normal!` の 1 本で流す）は `H M L` の前に viewport を求めて解決するので、この差は今日の 1853 件には出ない。再生の間だけ鍵ごとに解決する案は、長いマクロで鍵ごとに layout を作り直すことになるので採らない。hide が実機で違和感を見たときに戻す判断をする。
