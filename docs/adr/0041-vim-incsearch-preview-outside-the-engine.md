# ADR 0041 — `incsearch` は入力中の当たりを engine の外の preview として持ち、確定は今までどおり 1 つの鍵で届く

- 状態: 受理（設計席 2026-09-23・Issue #148。既定オンは設計席の案で hide 未確認・確認後に施主決定 D18）
- 日付: 2026-09-23
- Issue: #148
- 影響する規則: FR-003 / ARC-001 / ARC-010 / CPP-002 / CPP-005 / QLT-001 / QLT-012
- 前提: [ADR 0032](0032-vim-search-as-input-line-and-one-key.md)（検索は入力行から入り確定は 1 つの鍵）・[ADR 0037](0037-search-highlight-as-visible-line-spans.md)（当たりは見えている行の列・`hlsearch` 既定オン D17）・[ADR 0023](0023-command-palette-and-shared-input-session.md)（入力 session の共用）・[ADR 0022](0022-ex-command-line-and-settings-evaluation.md)（`:set` の評価）

## 文脈

`/` `?` の入力中は本文もキャレットも動かず（契約 `verify_vim_search_input`）、`EditorController::search_pattern` は確定済みの `VimState::last_search` だけを読む。Vim の `'incsearch'` は「入力中のパターンで当たりを見せ、見えるように画面を動かし、`hlsearch` が on なら全一致も光る。不一致・無効なら何も見せない。Esc で元の位置に戻り、確定は Enter だけ」（`:help 'incsearch'`）。oracle は `normal!` で鍵を流し切った最終状態しか観測できないので、fixture では守れない。

## 決定

**入力中の当たりは application の `EditorState` が持つ preview（engine の外）で、engine のキャレットと `last_search` は確定まで動かない。preview の位置は確定の検索と同じ core の関数 1 本で求める。既定はオン。**

1. **検索の 1 本化（core）**: `vim_step` の検索の鍵が使う「パターン・方向・回数・開始位置から次の一致を求める」処理を `src/core/VimSearch.*` の純関数 `vim_find_match(const TextBuffer&, const VimPattern&, const VimMatchRequest&) -> std::expected<VimSearchHit, VimSearchNoticeKind>` に切り出し、`vim_step` と preview の両方がそれを呼ぶ（ARC-001）。`VimMatchRequest { TextPosition from; VimSearchDirection direction; std::size_t count; }`（引数は 4 つまでの `readability-function-size` の閾値に合わせて開始位置・向き・回数を束ねる・`src/core/VimMatchRequest.hpp`）。`VimSearchHit` は既存の型の `Offset caret` を `TextPosition position` に置き換える（`{ TextPosition position; bool wrapped; }`・1 ファイル 1 型・`src/core/VimSearchHit.hpp`）。1 回ぶんの検索だった公開の `vim_search` は `VimSearch.cpp` の中に閉じ、公開の入口は `vim_find_match` 1 本にする。折り返し・E486（`pattern_not_found`）の判定は変えない。
2. **preview の型（application）**: `struct SearchPreview { core::TextPosition match; ScrollState origin; }`（`src/application/SearchPreview.hpp`）。`origin` は入力行を開いたときのスクロール位置。`EditorState` に `std::optional<SearchPreview> search_preview` を足す。
3. **更新の経路**: `SearchLine` が開いているとき、`accept(CommandText)` / `accept(EditCommand)`（1 文字・BS）の後に controller が `update_search_preview()` を 1 か所で呼ぶ。手順: `VimState::incsearch` が off か本文が空か入力が空なら preview は `origin` だけを残して `match` 無し（→ scroll を `origin` に戻す）。入力を `VimPattern::parse` して失敗なら同じく無し。成功なら `vim_find_match(text, pattern, VimMatchRequest{caret, direction, 1})` で一致なら `match` を置いて `follow_position(match)`（`follow_caret` と同じ計算で、見える範囲に入れる）、不一致なら無しで `origin` に戻す。**報せ（E486 など）は出さない**（Vim と同じ）。
4. **`search_pattern()` の読み替え**: `SearchLine` が開いていて入力が空でなく `incsearch` が on なら、その入力を `VimPattern::parse` して返す（失敗なら `std::nullopt`）。そうでなければ今までどおり `last_search`（`highlight == on` のとき）。見えている行の全一致は ADR 0037 のまま `vim_line_matches` で塗り、`current_match` は preview の `match` を含む一致（キャレットではなく preview）。
5. **Esc**: `accept(CancelCommand)` が preview を消し、scroll を `origin` に戻す。キャレットと `last_search` は元から動いていない。**Enter**: `submit(SearchLine)` が preview を消してから今までどおり engine の 1 つの鍵として届ける（engine が同じ `vim_find_match` で同じ位置に動く。scroll は `follow_caret`）。
6. **設定**: `VimState::incsearch`（bool・既定 true）。`:set incsearch` / `:set noincsearch` を `ExResult` の既知オプション表に足す（`hlsearch` と同じ経路）。`hlsearch` と同じく永続化しない。
7. **Ctrl-G / Ctrl-T**（次・前の当たりへ）は後続。`:s` `:g` への適用も後続。
8. **renderer**: 変更しない。preview は `LineView.matches` / `current_match` に写るので既存の描画（`search` の面・`accent` の枠）で見える。入力中は本文のキャレットを描かない今の形のまま。

## 強制

- 契約（`nib_tests` の scope `--vim-search-incremental`）: **active**。`/be` の入力後に preview が最初の当たりで `matches` に見えている行の全一致が出る / Esc で preview が消え scroll が `origin` に戻りキャレット不変 / Enter の後のキャレット = preview の位置 / 無効（`\(`）と不一致で preview 無し・報せ無し・scroll は `origin` / `?` の後ろ向きと折り返し / `:set noincsearch` で preview 無し / 本文とキャレットは入力中に不変（既存の契約を残す）。
- 検索の経路が 1 本であること: **planned**（レビュー事項。`vim_step` の検索の鍵と `update_search_preview` の両方が `vim_find_match` を呼ぶ）。
- fixture: 増減なし。`eng/protected-diff.py --base main --allow --vim-search-incremental` で確かめる。

## 結果

得られるもの: 打ちながら当たりが見え、Esc で戻る。engine は変わらないので fixture 141 件と契約は不変。
失うもの・残る穴: preview は見えている行の照合と本文全体の検索を 1 打鍵ごとに行う（16 MiB では 1 打鍵の速さに効く可能性。速さのゲートの `keystroke` は通常の入力なので測らない。大きな本文での体感は hide の手元）。Ctrl-G / Ctrl-T が無い。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| engine のキャレットを入力中に動かす | ADR 0032 決定 1（入力行は本文のモードと独立）と `verify_vim_search_input` を壊す。Esc の巻き戻しが engine の状態に入る |
| preview 専用の検索を application に書く | 確定と preview で一致の位置がずれ得る。1 本化（ARC-001） |
| 既定オフ（Vim の既定） | `hlsearch` の D17 と同じ理由でオン。hide 未確認なら ADR の状態で明示 |
| Ctrl-G / Ctrl-T を同時に入れる | S 級に切る。preview の型と経路が入ってからの 1 命題 |
