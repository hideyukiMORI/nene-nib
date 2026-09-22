# ADR 0037 — 検索の当たりは見えている行だけを照合器で数え、行ごとの列として描く（`hlsearch` は既定オン）

- 状態: 受理（2026-09-22・`v:hlsearch` と `searchcount()` の実測で決定 1〜8 を確認し、補足を足した）
- 日付: 2026-09-22
- Issue: #123
- 影響する規則: FR-003 / ARC-001 / ARC-004 / ARC-007 / ARC-011 / CPP-002 / CPP-004 / CPP-006 / CPP-011 / CPP-012 / QLT-001 / QLT-008 / QLT-012 / QLT-014

## 文脈

hide の要望（2026-09-22 の実機確認）: 検索したら全一致の背景が変わり、キャレットのある一致だけ別の色になってほしい（Vim の `hlsearch` と `CurSearch`）。ADR 0032 は `hlsearch` / `incsearch` を「Vim の既定がオフなので描画は変えない」として範囲外にした。engine は `last_search`（パターンと向き）と照合器（`VimPattern` / `vim_search`）を持つが、描画は一致を知らない。

見た目の正本 `docs/design/2026-09-15-editing-look.md` は 09-15 の時点で「検索の当たりは `search`（淡い橙 35%・ライト 30%）の面。現在の当たりは `accent` の 1 DIP の枠」と決めており、`Palette` には `search` トークンが既にある（ADR 0017 の派生規則: accent → 白へ 50%・不透明度 89 / 77）。実装が無かっただけで、新しいトークンは要らない。2026-09-22 に設計リナがキャンバス（ダーク / ライト・案 A 枠 / 案 B 面）で見比べ、案 A を推し、hide は「既定オン・色はリナの推しで」と決めた（施主決定 D17）。案 B（現在の当たりを `accent` の面で塗る）は、NORMAL のキャレットのブロック（`accent` の面）が一致の先頭の字に載って溶けるので採らない。

描画は行ごとの `LineView`（本文と `SelectionSpan` 1 つ）を renderer が描く。矩形 VISUAL（ADR 0035）で「行ごとの範囲の列」を application が作る形が既にある。Vim ソースは読まず、help（`:help 'hlsearch'` / `:help :nohlsearch` / `:help hl-CurSearch`）と実測（`v:hlsearch`）を根拠にする。

## 決定

**検索の当たりは application が `EditorFrame` を作るときに、見えている行だけを既存の照合器で数えて行ごとの列として `LineView` に持たせ、renderer は選択と同じ経路で `search` の面を塗り、キャレットのある一致だけ `accent` の 1 DIP の枠を描く。強調のオン・オフは engine の `VimState` が閉じた 3 値で持ち、既定はオン。**

1. **状態**: `VimState` に `highlight`（閉じた enum `VimSearchHighlight { on, off, suspended }`・既定 `on`・施主決定 D17）。`:set hlsearch` → `on`、`:set nohlsearch` → `off`、`:nohlsearch`（`:noh`）→ `on` のときだけ `suspended`。検索の鍵（`/ ? n N * #` の確定・見つからなくても）は `suspended` を `on` に戻す（Vim の `:help :nohlsearch` と同じ。`off` は戻さない）。設定の永続化（C2）には載せない（範囲外）。
2. **Ex**: `ExResult` に `std::optional<VimSearchHighlight>` を足し、`:set hlsearch` / `:set nohlsearch` / `:nohlsearch` / `:noh` が返す。controller は既存の Ex の写しの中で `VimState` へ置く（Ex の経路は 1 本のまま。`set` の補完候補に `hlsearch` / `nohlsearch` を足す）。通常モードでは `:` が無いので触れない。
3. **一致の計算**: core の純関数 `vim_line_matches(line_text, pattern) -> std::vector<OffsetRange>`（行内・0 桁目から重ならない列・ADR 0032 の決定 4 の追記と同じ走査）を照合器から公開し、`vim_search` もそれを使う（走査の規則を 2 か所に書かない）。application は `EditorFrame` を作るときに、`highlight == on` かつ `last_search` があり Vim モードのときだけ、**見えている行だけ**に対して呼ぶ。パターンは `VimPattern::parse` の結果をフレームごとに 1 回だけ作る（解析に失敗する `last_search` は無い。あれば強調しない）。
4. **行ごとの列**: `LineView` に `matches`（`SelectionSpan` の列・桁は既存の `span_of` 1 本で作る）と `current_match`（`std::optional<SelectionSpan>`）を足す。現在の一致は「キャレットを含む一致」（`begin <= caret < end`・長さ 0 なら `begin == caret`）で、無ければ空。`SelectionSpan` の型は選択と共用する（新しい型を増やさない）。
5. **描画**: renderer は本文の前に `matches` を `palette.search` で塗り（既存の選択の塗りと同じ 1 本の経路）、その上に選択を塗る（重なる所は選択が優先）。`current_match` は `palette.accent` の 1 DIP の枠を面の内側に描く（キャレットのブロックは今までどおり最後）。行をまたぐ一致は無い（照合は行内）。ui/win32 に色のリテラルを書かない。
6. **追従**: フレームは本文から毎回作るので、編集・スクロール・テーマ切替に自動で追従する。`incsearch`（入力中の強調）は範囲外。通常モード（Vim でない）では強調しない。
7. **速さ**: 見えている行 × パターンの照合だけ（本文全体は走らない）。1 打鍵の予算（0.9 ms）に対して、見えている 60 行程度の照合は十分小さい見込み。`measure-speed.py --check` を 1 回と、`last_search` を立てた状態の 1 打鍵を対象 unit で 1 回だけ時間を記録する（基準値には足さない）。
8. **oracle**: 強調そのものは Vim の報告に出ないので fixture にできない。`:noh` と `:set (no)hlsearch` と検索の後の `v:hlsearch` の遷移を probe で実測して決定 1 を確かめ、engine の契約（`--vim-search-highlight`）で守る。

## 強制

- 3 値の写し漏れ・`ExResult` の写し漏れ: **active**（`switch` の網羅性・CPP-002）
- 走査の規則が 1 か所であること・見えている行だけを数えること: **active**（対象 unit・`vim_search` が `vim_line_matches` を使う契約）
- 桁が `span_of` 1 本で作られること（全角・Tab・CRLF で選択と同じ桁）: **active**（application の unit）
- 色がトークンから来ること: **active**（既存の字句検査・ui/win32 の色リテラル禁止）
- 期待値が本物の Vim の答えであること: **不能**（強調は Vim の報告に無い。`v:hlsearch` の遷移だけ実測）

## 結果

得られるもの: 検索の手応え（全一致と現在の一致）が 9 テーマで既存の `search` / `accent` トークンから出る。走査の規則と桁の計算は既存の 1 本を共用し、engine は純関数のまま。
失うもの・残る穴: `incsearch` は後続。`hlsearch` の設定は永続化しない（起動時は常にオン）。長い行に短いパターン（`/a`）を掛けると見えている行の一致が多くなるが、行内の走査は行の長さに比例するだけ。

## 補足（実測のあとで・2026-09-22）

**決定 1 は固定 Vim 9.1 の `v:hlsearch` で 29 ケース測って一致した**（`out/issue123-oracle/probe.py`
25 件・`probe3.py` 4 件。証拠は同じ場所の `probe*.json` / `probe.txt`。`out/` は追跡外なので作業機にだけある）。
検索の鍵は `/ ? n N * #` のどれでも、見つからなくても（E486）・直前のパターンが無くても（E35）
`suspended` を `on` へ戻す。移動と編集は戻さない。`:set nohlsearch` の `off` は検索では戻らず、
`:set hlsearch` だけが戻す。`:set hlsearch` は `suspended` も同時に解く。Vim ソースは読まず、
help（`:help 'hlsearch'` / `:help :nohlsearch`）と実測だけを根拠にした。

実測で足りなかった点を 3 つ足す。

1. **固定 Vim は旗を 2 つ（`'hlsearch'` と `no_hlsearch`）持ち、本実装は 3 値に畳んでいる。**
   畳んだぶん「`off` のときの `:nohlsearch`」だけが表せないので、`requested_highlight(current,
   requested)` の 1 か所で `off` ＋ `suspended` → `off` に折る（`VimSearchHighlight.hpp`）。
   これが無いと `:set nohlsearch` → `:noh` → 検索で強調が復活して実測と食い違う。
2. **長さ 0 の一致は数えるが塗らない。** `searchcount()` で測ると `abc` の `/a*` は 3、`abc\ndef`
   の `/^` は 2 で、走査は長さ 0 の一致も 1 つと数えて 1 文字進む（`vim_line_matches` はそのまま
   返す）。面が無いので `span_of` が `absent` を返し、application がそこで落とす。決定 4 の
   「長さ 0 なら `begin == caret`」は結果として使われない。
3. **`aaaa` の `/aa` は 2 つである**（依頼書の例は 1 つとあったが、`searchcount()` の実測は 2）。
   走査は一致の終わりから数え直すので `ababa` の `/aba` だけが 1 つになる。

**決定 7 の測り方を直した。** `tests/unit/NibTests.cpp` は冒頭で「時間は測らない（`<chrono>` は
ARC-007 でここに書けない）」と定めているので、対象 unit には時計を入れない。代わりに core の
純関数だけを呼ぶ使い捨ての計測（scratchpad・リポジトリには置かない）で、**見えている 60 行 ×
1 行 2 一致の走査が 1 フレームあたり 0.067〜0.073 ms**（Release・実機）であることを 1 回だけ
記録した。1 打鍵の予算 0.9 ms に対しておよそ 8 % で、基準値には足していない。

**決定 2 の言い回し。** Ex の答えの文言は `hlsearch=on` / `hlsearch=off` / `hlsearch=suspended`
の 3 つで、`colorscheme=` / `fontsize=` と同じ形にそろえた（Vim は `:noh` に何も言わないが、
`DisplayText` は空にできない）。補完の候補に足したのは `set hlsearch` と `set nohlsearch` の 2 つ
だけで、裸の `nohlsearch` は足していない（10 文字で `colorscheme` より短く、Ctrl+P を開いた
ときの先頭の候補を奪ってしまうため。`:nohlsearch` / `:noh` は打てば通る）。

**モードの出入り。** `vim_resting_from` が `last_search` と同じように強調の値を持ち越すので、
通常モードへ出て Vim へ戻っても `suspended` は残る。永続化しないので再起動では `on` に戻る。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| Vim の既定（`nohlsearch`）に合わせてオフ | 施主決定 D17。見えないと検索の手応えが無い。`:noh` と `:set nohlsearch` で消せる |
| 現在の一致を `accent` の面で塗る（案 B） | NORMAL のキャレットのブロックと溶けて位置が消える。キャンバスで見比べて枠を選んだ |
| 新しいトークン `current_search` を足す | `accent` の枠で足りる。9 テーマぶんの値を増やさない（ADR 0017） |
| 本文全体の一致を engine が持つ | 16 MiB で 1 打鍵ごとに走査が要る。見えている行だけで足りる |
| 強調のオン・オフを `EditorSettings`（永続化）に載せる | C2 の schema が変わる。Vim も `hlsearch` は起動時の既定に戻る |
| renderer が本文から一致を探す | renderer は写すだけ（ARC-011）。桁の計算が `span_of` と 2 本になる |
