# ADR 0164 — matchparen はキャレットの下の括弧の相手を、見えている範囲（と前後 1 画面）で探して枠で見せる

- 状態: 受理（施主決定 D67・FR-024・2026-10-28。実装は 1 席・受理は D41 の形）
- 日付: 2026-10-28
- Issue: #999・計画 `docs/plans/2026-10-28-editing-features-roadmap.md` §1 の 1
- 規則: FR-024、ARC-001 / ARC-003 / ARC-004、CPP-002 / CPP-011、QLT-014、ADR 0037（当たりの枠 `current_match`）/ 0152（`%` の対応する括弧の探索）/ 0156（構文の色の区間）/ 0128（追従の遅延解決・frame を作る費用）

## 文脈

Vim は `matchparen` の plugin を既定で読み込み、キャレットの下（か直後）の括弧の相手を強調する。Nib には `%`（対応する括弧へ跳ぶ・ADR 0152）の探索と、キャレットを含む当たりの枠（`current_match`・ADR 0037）がある。D67 の速さの条件: 既定オンの機能は 1 打鍵の +1% まで・全文を走らない。

## 決定

1. **対は `matchpairs` の既定 `()` `[]` `{}`。** キャレットの下の文字が対の片方なら相手を探す。下に無ければ Vim と同じく探さない（Vim の `matchparen` はキャレットの下だけ。`%` が直後の括弧も探すのとは別）。INSERT ではキャレットの直前の文字も見る（Vim の `matchparen` の INSERT の規則）。
2. **探索は見えている範囲と前後 1 画面で打ち切る。** core の純関数 `matched_bracket(const TextBuffer &, Offset caret, OffsetRange window) -> std::optional<Offset>`（`%` の探索の関数を `window` で打ち切る形に広げ、2 本にしない）。`window` は見えている最初の行の 1 画面前から最後の行の 1 画面後まで（行の索引で求める・本文を全部走らない）。見つからなければ出さない（Vim の `matchparen` の `timeout` に相当）。巨大な行（格子表示）では探さない。
3. **文字列とコメントの中は除く（色のある言語だけ）。** キャレットの括弧が構文の区間（`LineView.colors` の `string` / `comment`）の中なら、相手も同じ種類の区間の中だけで探す（Vim の `matchparen` の `syn` の規則の簡易版）。区間の無い言語（plain）は除かない。構文の区間は frame を作る所にあるので、`matched_bracket` には「除く区間」を引数で渡す（純関数のまま）。
4. **見せ方は `current_match` と同じ枠。** `LineView` に `std::optional<core::SelectionSpan> bracket` を足し、renderer は `current_match` の枠（`accent`・1 DIP）と同じ道で描く（色のトークンは足さない）。キャレットの下の括弧自体は枠を描かない（相手だけ・Vim の `MatchParen` は両方だが、キャレットの枠と重なるので相手だけ）。選択（VISUAL）の上でも描く。
5. **費用。** frame を作るたびに `matched_bracket` を 1 回（キャレットの下が括弧のときだけ探索・そうでなければ 1 文字の比較で終わる）。探索は `window` の中の走査で、最悪でも 3 画面ぶん（約 180 行）。任意のベンチ `matchparen-single-key`（括弧の多い C++ の文書でキャレットを `(` に置いて 1 打鍵）を足し、空の文書の 1 打鍵の A/B（+1% の線）を PR 本文に書く。
6. **契約。** core: `--matchparen`（対 3 種・`window` の打ち切り・見つからない・文字列とコメントの除外・INSERT の直前・巨大な行）と性質（固定 seed のランダムな括弧の列で、`matched_bracket` の答えが素朴な全文の走査と `window` の中で一致・対になっていなければ値なし）。application: frame の `bracket` の写し。ui: `nib_window_tests` の枠の画素。
7. **限界。** `matchpairs` の設定（`<>` を足す等）・`%` の `#if` との連動・点滅や色の変更・2 枠のもう 1 つの枠。

## 強制

- 全文を走らない（`window` の打ち切り）: **active**（契約・性質の試験）。
- 8 本の速さと +1% の線: **active**（QLT-014・PR 本文の A/B）。

## 結果

得られるもの: Vim で「ある」と思われている括弧の強調。
失うもの・残る穴: 3 画面より遠い相手は見せない（Vim の `timeout` と同じ妥協）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| 全文を探す | 1 GB の文書で打鍵が止まる（D67） |
| キャレットの下と相手の両方を枠で | キャレットの枠と重なって見づらい |
| 別の色のトークン | ADR 0008 のトークンを増やさない。`accent` の枠で足りる |
