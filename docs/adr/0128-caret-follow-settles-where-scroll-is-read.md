# ADR 0128 — キャレット追従はスクロールを読む所で 1 回だけ解決する

- 状態: 提案（設計席 2026-10-15。実装と性能の受理は #474 の正式ゲートで）
- 日付: 2026-10-15
- Issue: #474（#460 の 16 MiB 連打も同じ原因の一部）
- 規則: ARC-001 / ARC-004 / ARC-005 / ARC-011 / ARC-012、CPP-002 / CPP-005、QLT-001 / QLT-010 / QLT-012 / QLT-014、ADR 0009 / 0011 / 0041 / 0084 / 0114

## 文脈

2026-10-15 の main `89c582d` の Release で 8 本の速さの基準を測ると、連打の 2 本だけが基準を超えた。

| bench | 2026-10-15 の中央値 | 基準（上限） |
| --- | ---: | ---: |
| key-to-frame-burst-200（空の文書） | 11.266 ms | 2.695 ms（3.369） |
| key-to-frame-burst-200-16mib | 14.661 ms | 7.411 ms（9.264） |

文書の大きさに依らない。窓なしの probe `controller-insert-200`（同じ 200 の `apply(InsertText)`）は 16 MiB の後でも 0.38 ms なので、1 打鍵あたり約 50 µs は本文の編集ではなく、`EditorController::replace` の末尾の `follow_caret()` → `VisualTextView::followed()` → `TextLayoutPort::locate()` にある。折り返し（ADR 0114）以後、キャレットを見える範囲に収めるために、打鍵ごとにキャレット行の DirectWrite layout（`CreateTextLayout` + `GetMetrics` + `HitTestTextPosition`）を作り直している。窓なしの probe は幅 0 なので layout を作らず、この費用を観測できなかった（実験ブランチ `experiment/460-burst-breakdown` に節目を足して実測: 1 打鍵の `apply` は中央値 65 µs、そのうち `make_layout` が毎打鍵 1 回・中央値 50 µs・200 打鍵で 10.0 ms。窓側は 1 打鍵 5 µs）。

まとめて届いた 200 打鍵は 1 枚の `WM_PAINT` に畳まれる（ADR 0011 / 0084）。しかしスクロールの追従は意図ごとに即時に計算され、199 回分は誰にも読まれずに捨てられる。

## 決定

1. **追従は「頼む」と「解決する」を分ける。** `follow_caret()` / `follow_position(p)` は追従したい位置を `pending_follow_` に置くだけで、layout を作らない。後から来た頼みは前の頼みを置き換える（最後に頼んだ位置だけが意味を持つ）。
2. **解決するのはスクロールを読む直前の 1 か所。** controller の中で `state_.scroll()` を読む所と `visual_view()` を作る所は、private の `settled_scroll()`（非 const・保留があれば `followed()` を計算して `state_` に写し、失敗は `layout_failure_` に置く）を通す。`state_.scroll()` を直接読まない（ARC-012: 読む道は 1 本）。
3. **`frame()` は解決してから作る。** `frame()` は非 const にし、先頭で解決する。`const EditorController` から `frame()` を読んでいた試験は非 const に直す（`delivery()` / `documents()` / `title_bar_input()` などスクロールを読まない口は const のまま）。
4. **解決はその時点の state で 1 回。** 解決はその時点の state（本文・選択・モード・見えている行数）で `followed()` を 1 回呼ぶ。スクロールを読む意図（画面行の移動・`ScrollLines`・`VisibleLines`・Vim の鍵・マウスの `PlaceCaret`・frame）は読む前に解決するので、それらが見る値は解決済みである。読まれないまま続いた複数の頼み（打鍵・undo/redo・IME の確定が 1 枚の WM_PAINT の前に続いたとき）は最後の 1 つだけが効く。通常モードの追従は今のスクロールからの最小の移動なので、即時の実装が途中の画面を経由して着いた位置とは違い得る（例: undo で 20 行目へ跳んでから次の undo で 5 行目へ戻ると、即時は 5 行目を先頭に、遅延は元の画面のまま）。どちらもキャレットは見えており、Vim の鍵は毎回解決するので fixture には出ない。
5. **Vim の鍵は毎回解決する（本 ADR の範囲外の費用）。** `measured_vim_step` は `VimViewport` を engine に渡すので、Vim の鍵はスクロールを読む意図に当たる。Vim INSERT の連打はこの ADR では速くならない。engine が viewport を `layout_request` と同じ要求の形で後から求める案は後続の Issue。
6. **基準値・許容・ゲートは変えない。** 連打の 2 本が既存の基準に戻ることを正式な QLT-014 で示す。`--adopt` はしない。
7. **文書を離れるときに保留は捨てる。問いの答えが頼み直す。** `pending_follow_` は controller に 1 つで文書ごとではない。巨大な行の問いを待つ間（`display_pending`・ADR 0140 の決定 2）は `settle_follow` が保留を消費しないので、そのまま文書を離れる（`leave_document` と `open_document` の `blank_untitled` の差し替え）と、次の文書の最初のスクロールの読みで前の文書の位置に追従してしまう。離れる所の 2 か所は `drop_pending_follow()` の 1 本を通り、問いの間なら保留を捨てる。失うものは無い: 同じ文書で問いに答える `SelectDisplay` が `clamp_scroll` と `follow_caret` を頼み直す。問いの無いときは今どおり離れる前に解決する。契約は `nib_tests --application` の `verify_follow_dropped_on_leaving_question`。

## 実装の形（決定の写し先）

- `EditorController` に `std::optional<core::TextPosition> pending_follow_` を足す。`follow_position(p)` は `pending_follow_ = p` だけ。
- private `void settle_follow()`: 保留を取り出して空にしてから、今日の `follow_position` の本体（`followed()` → `with_scroll` / `layout_failure_`）を 1 回。保留が無ければ何もしない。
- private `const ScrollState &settled_scroll()`: `settle_follow()` の後に `state_.scroll()` を返す。controller と `EditorView.cpp` の `state_.scroll()` の読みはすべてここを通す（残ってよいのは `settle_follow` の中だけ。`grep -n 'state_.scroll()' src/application/EditorController.cpp src/application/EditorView.cpp` で確かめる）。
- `visual_view()` は非 const にして先頭で `settle_follow()` を呼ぶ。`settle_follow` 自身は保留を空にしてから view を作るので再入しない。`visible_lines()` / `measured_vim_step()` など const のまま `visual_view()` を呼んでいた関数は非 const にする（呼び元はすべて非 const）。
- `frame()` は非 const。`const EditorController` から `frame()` を呼ぶ試験と probe は非 const に直す。`delivery()` `documents()` `title_bar_input()` `is_composing()` `command_palette_view()` はスクロールを読まないので const のまま。
- スクロールを書く所（`with_scroll`）は、同じ式で `settled_scroll()` を読んでから書く（`visible_lines` の写し）。読まずに書く所を作らない。

## 却下した選択肢

- 窓で連続する WM_CHAR をまとめて 1 つの `InsertText` にする: 1 打鍵 = 1 意図の原則（ADR 0009）と Vim の鍵の記録・再生の単位を変える。
- キャレット行の幅を文字数や字体の最大幅で見積もって layout を省く: 別の文字幅表を持たない（ADR 0114 の決定 3）。
- 状態遷移そのものを WM_PAINT まで遅らせる: ADR 0084 が却下済み。本 ADR が遅らせるのは追従の解決（`TextLayoutPort` の呼び出し）だけで、本文・選択・IME・保存・終了は即時のまま。ADR 0084 の文脈の「状態遷移とポート呼び出しを省略・延期しない」は、スクロール追従の解決に限って本 ADR で改める。
- `mutable` で const の `frame()` から解決する: CPP-003 が禁じる。

## 検証

- 対象: application の単体（スクロール・追従・incsearch の origin・VisibleLines・タブ切替）、Vim fixture（`H M L` と Ctrl-d/u/f/b を含む）、ui の窓なしの契約。`const EditorController` の試験の非 const 化は機械的。
- 速さ: 正式な `eng/measure-speed.py --check` を 8 本（連打 2 本が基準内に戻ること・ほかに悪化が無いこと）。窓なしの probe は幅 0 で追従を観測しないので、前後比較の主役にしない。

## 限界

解決は解決する時点の state（設定・モード・寸法）で行う。頼んだまま読まれないうちに wrap や字体の設定・モードが変わると、即時の実装なら古い設定で追従してから新しい設定で clamp していた所を、新しい設定で 1 回追従する。見えるのはどちらもキャレットを収めた画面で、新しい設定で収めるほうが正しい。
Vim の鍵の連打は決定 5 のとおり残る。1 打鍵の単打は layout を作る時点が `apply` から `frame` に移るだけで、速さは変わらない。
