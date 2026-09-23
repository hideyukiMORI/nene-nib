# ADR 0043 — incsearch の Ctrl-G / Ctrl-T は preview の検索の起点を当たりへ動かし、確定の鍵はその起点を運ぶ

- 状態: 受理（設計席 2026-09-23・Issue #168。引き継ぎの「次の順」の 3 番目。向きと回数は hide の指示「Vim 準拠」（2026-09-23・D18 の補足）を受けて Vim のソースで確認済み・Issue #172）
- 日付: 2026-09-23
- Issue: #168
- 影響する規則: FR-003 / ARC-001 / ARC-010 / ARC-011 / CPP-002 / CPP-005 / QLT-001 / QLT-012
- 前提: [ADR 0041](0041-vim-incsearch-preview-outside-the-engine.md)（preview は engine の外・決定 7「Ctrl-G / Ctrl-T は後続」）・[ADR 0032](0032-vim-search-as-input-line-and-one-key.md)（確定は 1 つの鍵）・[ADR 0037](0037-search-highlight-as-visible-line-spans.md)

## 文脈

`:help c_CTRL-G` / `c_CTRL-T`: 「`'incsearch'` が set で `/` `?` の入力中に今の当たりが見えているとき、CTRL-G は次の当たりへ、CTRL-T は前の当たりへ動く（search-offset は見ない）」。`:help 'incsearch'`: 「Esc で元の位置に戻る。当たりへ動くには Enter で検索を終える」。

本物の Vim で観測できるか（Sonnet の probe・scratchpad `probe-ctrlg.md`）: oracle の `-es` ＋ `:normal!` は typeahead を持つので incsearch が対話と同じには動かず、Ctrl-G 無しの `/be<Esc>` ですら元の位置に戻らなかった（help と食い違う）。Ctrl-T の 1 回目だけ動かない等の非対称も同じ穴の産物と見る。**したがって Ctrl-G / Ctrl-T は fixture にせず、ADR 0041 と同じく契約テストで守る。** 同じ probe で確かめられた事実 2 つは使う: (a) Ctrl-G の後にパターンを編集しても、確定の位置は「編集後のパターンを Ctrl-G で着いた位置から探した当たり」になる（hop は消えない）。(b) `[count]/pat` の回数は hop のたびに数え直す（`n` / `N` と同じ形）。

Nib の今の形: preview は `SearchPreview { optional<TextPosition> match; ScrollState origin; }` で、`update_search_preview()` が毎回 **キャレット**から `vim_find_match(text, pattern, {caret, direction, count})` を引く（ADR 0041 決定 3）。確定は engine の 1 つの鍵 `VimSearchPattern { pattern; direction; }` で、engine がキャレットから同じ関数で探す。Ctrl-G / Ctrl-T に当たる意図は application にも ui にも無い。

## 決定

**preview は「検索の起点」を持ち、Ctrl-G / Ctrl-T はその起点を今の当たりへ動かしてから検索方向の次／前を探す。確定の鍵は起点を運び、engine は起点があればそこから探す。engine の検索は `vim_find_match` 1 本のまま。**

1. **起点（application）**: `SearchPreview` に `core::TextPosition from` を足す（`{ optional<TextPosition> match; ScrollState origin; TextPosition from; }`）。`perform(VimOpenSearch)` で `from = キャレット`。`update_search_preview()` は毎回 `from` から `vim_find_match(text, pattern, {from, direction, count})` を引く（今は `caret` を渡している所を `from` に）。パターンの編集は `from` を変えない（文脈の (a)）。Esc は今までどおり `origin` へ戻して preview を消す（`from` も消える）。
2. **hop の意図**: application の意図の閉じた和型に `SearchHop { core::VimSearchDirection relative; }` を足す（`src/application/SearchHop.hpp`・1 ファイル 1 型・`EditorIntent` に 1 つ）。`accept(SearchHop)` は `SearchLine` が開いていて `incsearch` が on で `match` があるときだけ動く。**hop の向きは本文の順**（Ctrl-G は下へ・Ctrl-T は上へ）で、`/` `?` の検索方向に依らない（根拠は Vim のソース `src/ex_getln.c` の `may_adjust_incsearch_highlighting`（GitHub master・2026-09-23 取得）: `searchit(..., c == Ctrl_G ? FORWARD : BACKWARD, ..., count, ...)` で、向きは `firstc`（`/` `?`）に依らず、回数 `count` は hop にも渡る。Sonnet の probe の実測 `?be<C-G>` が下へ動いたことと一致。help の "next / previous match" を検索方向に相対と読む案は却下）。手順: 今の当たり `match` から本文の順の向きに `vim_find_match(text, pattern, {match, 向き, count})` を引いて新しい当たり `M'` を置き、**起点 `from` は「確定の検索（検索方向・同じ回数）が `M'` に着く位置」**、すなわち `M'` から検索方向の逆へ `count` 件戻った当たりにする（Ctrl-G を `/` で押したときは今の当たりに等しい）。`count` は `[count]/pat` の回数で、preview の最初の検索も hop も確定と同じ回数を使う（#148 の preview は回数を見ていなかった。`2/be` の preview は 2 件目・文脈の (b)・Vim の `may_do_incsearch_highlighting` も `count` を受ける）。**Vim と違う 1 点（意図的）**: Vim は逆向きの hop（`/` の Ctrl-T・`?` の Ctrl-G）の後の起点を「当たりの 1 文字手前／後ろ」に置くので、回数が 2 以上だと Enter が当たりを通り過ぎる（ソースの注釈は「nv_search が終わったとき当たりに戻すため」と意図を書いており、回数 2 以上はその意図から漏れている）。Nib は起点を「確定の検索が着く位置」と定義するので回数に依らず当たりに着く。回数 1 では同じ。回数は engine の `vim_search_count` を application が読む（core の公開関数 1 本・確定と同じ値）。折り返しは `vim_find_match` の規則のまま（報せは出さない・ADR 0041 決定 3）。`match` が無い・`incsearch` off・Ex や palette の入力行では何もしない（文字を挿入しない。Vim は `noincsearch` だと `^G` を文字として入れるが、Nib は入力行に制御文字を入れない・ADR 0032）。
3. **確定の鍵（core）**: `VimSearchPattern` に `std::optional<TextPosition> from` を足す。`vim_step` の検索の鍵は `from.value_or(caret)` を `VimMatchRequest::from` にする以外は変えない（回数も `last_search` も折り返しの報せも今までどおり）。`submit(SearchLine)` は preview の `from` を鍵に載せる（hop が無ければ `from == caret` で今までと同じ・fixture 1339 件は `from` が無い鍵のまま不変）。`n` / `N` / `.` は `last_search` を使い `from` を持たない。
4. **ui/win32**: 検索の入力行が開いているときの Ctrl+G / Ctrl+T を `press_command_control_key` で `SearchHop` に写す（Ex / palette では今までどおり素通り）。通常モードと INSERT の Ctrl+G / Ctrl+T は変えない（今は何もしない）。
5. **描画**: 変えない。`current_match` は preview の `match` を含む一致で、hop で動く。`hlsearch` の全一致は hop で変わらない。
6. **oracle・fixture**: 増減なし。`eng/vim-oracle.py` の記法にも `<C-g>` `<C-t>` を足さない（fixture にしないので要らない）。

## 強制

- 契約（scope `--vim-search-incremental`）: **active**。`/be` → Ctrl-G で `match` が下へ・scroll が追う / Ctrl-T で上へ（先頭なら末尾へ折り返し）/ `?be` の Ctrl-G も下へ・Ctrl-T は上へ / Ctrl-G・Ctrl-T どちらの後の Enter でもキャレット = preview の位置・`last_search` は今までどおり / Ctrl-G の後に BS で編集しても `from` が残る / Esc で origin に戻りキャレット不変 / `match` 無し・`noincsearch`・Ex の入力行では何もしない / `2/be` の Ctrl-G は起点から 2 件先。
- 検索の経路が 1 本: **planned**（レビュー事項。`vim_step`・`update_search_preview`・`accept(SearchHop)` の 3 か所が `vim_find_match` を呼ぶ）。
- fixture: 増減なし（`eng/protected-diff.py --base origin/main --allow --vim-search-incremental`）。

## 結果

得られるもの: 入力中に当たりを次々に見て、確定でそこへ着く。engine の鍵は 1 つのまま。
失うもの・残る穴: Ctrl-G / Ctrl-T は本物の Vim の対話でしか観測できないので、契約は help と Vim のソース（向き・回数）に依る。回数 2 以上の逆向きの hop の後の Enter は Vim の実装と違う（決定 2・意図的）。search-offset は Nib に無い。`:s` `:g` への適用は後続。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| hop の回数を数えて確定の鍵に回数として載せる | Ctrl-T の折り返し（全一致の数が要る）と回数の数え直しを表せない。起点を運ぶほうが Vim と同じ形 |
| hop の向きを検索方向に相対にする（`?` の Ctrl-G は上へ） | help の文面からの読みで、Vim のソース（`searchit` の向きは Ctrl-G が常に FORWARD）と probe の実測に一致しない |
| 確定時に application がキャレットを preview へ動かしてから鍵を送る | ADR 0032 決定 1（入力行は本文と独立）を破り undo の単位が割れる |
| Ctrl-G / Ctrl-T を fixture にする | oracle の `:normal!` は typeahead で incsearch を対話と同じに動かさない（`/be<Esc>` が元に戻らない実測） |
| `VimSpecialKey` に `control_g` / `control_t` を足す | engine の鍵ではなく入力行の編集鍵。engine の閉じた enum を広げない |
