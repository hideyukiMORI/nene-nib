# ADR 0156 — 構文の色は本文の桁の区間で、見えている行の 200 行前から走らせ、保持した字形 run を cluster で切って描く

- 状態: 提案（hide の試用の後に受理）
- 日付: 2026-10-22
- Issue: #815（段 1・core の字句器・決定 1〜6）・#816（段 2・application と ui・決定 7〜15）・親 #814
- 規則: ARC-001 / ARC-003 / ARC-011 / CPP-002 / CPP-003 / CPP-004 / CPP-005 / CPP-011 / CPP-012、ADR 0017（`SyntaxPalette`）/ 0040（置き換えの描き直し）/ 0073（保持した字形）/ 0095（桁の投影）/ 0114（本文の文字組みの共有）/ 0137（改訂番号）/ 0139（巨大な行の窓）/ 0148（split）/ 0154（文書の言語）
- 計画: `docs/plans/2026-10-21-syntax-and-lsp.md` §1.1〜1.4。調査: `D:/NeNeNib/outputs/20261022/survey-syntax-stage12.md`

## 文脈

文書の言語は文書の値になった（ADR 0154）。テーマは本文の 16 色 `SyntaxPalette` を持つが（ADR 0017）、本文はまだ `text` の 1 色で描いている。色分けは「どこを何色にするか（字句）」「どの行の状態から始めるか（行をまたぐコメントと raw string）」「どう描くか（保持した字形と共有の layout）」の 3 つを決める。

本文の layout は描画と位置解決で共有し、鍵は文字列と幾何だけ（ADR 0114）。layout に色（`SetDrawingEffect`）を載せると共有が割れ、字形の収集器は effect つきの run を棄却する（ADR 0073）。色は layout の外で、描くときだけ当てる。

## 決定

1. **役割は閉じた 10 値。** `core::SyntaxRole : uint8_t { comment, keyword, string, number, type, function, constant, operators, error, warning }`。warning は LSP の診断のために置き、字句器は出さない。
2. **区間は本文の桁。** `core::ColorSpan{ Column begin; Column end; SyntaxRole role; }`（公開 aggregate・比較は非メンバー・`[begin, end)`・`SelectionSpan` / `matches` と同じ code point の序数で行頭が `Column{1}`）。行の中で昇順・重ならない・隙間は `text` の色。
3. **行をまたぐ状態は値。** `core::LexKind { plain, block_comment, raw_string }` と `core::LexState{ kind; delimiter（最大 16 文字）; delimiter_length }`（既定 plain・比較は区切りの有効部分だけ）。言語ごとの状態を増やさない。
4. **純関数 2 本。** `lex_line(Language, const LexState &, std::string_view) noexcept -> LexLine{spans, out}` と、同じ状態機械で区間を作らない `lex_state(...) noexcept -> LexState`（遡り用）。`line` は改行を含まない 1 行で、CRLF の `\r` は呼び手が除く。字句器を持たない言語は spans が空で `out == in`。持つかは `has_lexer(Language)`（`switch`・`default` 無し）。
5. **C / C++ の分類は Vim の `c.vim` / `cpp.vim` に寄せる。** コメント・文字列（閉じない行は error）・raw string（行をまたぐ）・数字・予約語とプリプロセッサの行・組み込みの型・関数（識別子の直後の `(`）・定数（`true false nullptr NULL`）・演算子の連続（詳細は #815 の依頼書と `LexCpp.cpp`）。
6. **分岐は表。** 文字の種類 → 遷移は `constexpr` の表と小さな関数、予約語はソート済みの `std::array` を `binary_search`（CPP-002 / CPP-012）。
7. **区間は `LineView` の欄 1 つ。** `std::vector<core::ColorSpan> colors`（`matches` と同じ本文の桁）。字句器の無い言語・巨大な行の窓（`window.active`）・IME の差し込み行（`composed`）・格子の行では空。
8. **字句解析は frame を作るときに枠ごとに 1 回。** `visible_lines` が見えている最初の行 `first` の行頭の状態を求め（`syntax_begin`）、見えている行ごとに `lex_line` の区間を `colors` に置く（`colored_line`）。覚え（決定 9）が使えなければ `max(first − 200, 1)` の行頭を plain として `lex_state` で `first` まで進める（Vim の `c_minlines` の型）。遡りの行は `text_range(line_start, line_end)` で 1 行ずつ写す（CRLF の `\r` は `line_end` が外す）。見えている行は `LineView.text` をそのまま渡す。IME の差し込み行は区間を付けず `lex_state` で状態だけ進める。巨大な行は行の全体を読まず plain に戻す（遡りの道は `giant_line_index`、見えている道は `window.active` で判る）。
9. **覚えは controller が枠の側ごとに 1 つ。** `application::SyntaxStateCache{ AutosaveId tab; size_t revision; Language language; LineNumber line; LexState state; }`（`line` の行頭の状態・公開 aggregate）。`first_syntax_` / `second_syntax_` を `PaneSide` で引き、操作中の枠は `split().active_side`、もう 1 つの枠はその反対の側を使う（2 枠で別の文書でも取り合わない）。(tab, revision, language) が同じで `cache.line ≤ first` かつ `first − cache.line ≤ 4096` なら `cache.line` の状態から続け、それ以外は決定 8 の遡りで作り直す。frame の後に覚えを (`first` の行頭の状態) で置き換える。タブは `EditorState::autosave_ids()` の退避番号で名指しし（帯の位置・パスで名指ししない）、変わったかは `EditHistory::revision()` で比べる（ADR 0137）。`EditorState` には入れない。書くのは `frame()` と `add_other_view` だけ（`add_other_pane` / `add_other_view` は const を外した）。数える口 `EditorFrame.syntax_lexed`（操作中の枠で字句器へ通した行の数・遡りと見えている行）。
10. **色の対応は core の 1 本。** `core::syntax_color(const SyntaxPalette &, SyntaxRole) -> RgbColor`（`switch`・`default` 無し・役割と同じ名前の欄）。`EditorFrame.body`（`core::SyntaxPalette`・`theme.body`・`palette` の隣）。renderer に別の対応表を持たない。
11. **描画は保持した字形 run を cluster で切る。** renderer の `draw_line_text` は、`colors` が空なら今までどおり `text` の色で `draw_body_text` を 1 回、空でなければ区間を選択と同じ `range_of`（`displayed` が `^M` の展開を含めて描画用の行へ写し、`LineUtf16Evaluation` が UTF-16 にする）で `ColoredRun{DWRITE_TEXT_RANGE, D2D1_COLOR_F}` の列にして `BodyTextPainter::draw_colored_body_text(text, area, BodyColors{runs, plain}, paint)` を呼ぶ。字形が保持されている行は、run ごとに `glyph_pieces`（`GlyphPieces.hpp`）が区間の境の UTF-16 の位置を `clusters` / `text_position` で字形の添字に写して部分窓 `GlyphPiece{first, last, color}` に切り、`drawn_glyphs(run, first, last)` / `drawn_origin(run, origin, first)`（原点は前の送りを float で前から積む）で `DrawGlyphRun` する。区間の外は `text` の色。区間が run の境をまたぐと run ごとに切れる。色は呼び手の 1 本の brush の `SetColor` を替えるだけ（`paint`）。描く順は 検索の面 → 選択 → 構文の色の本文 → `draw_replaced`（muted）→ 今の当たりの枠 → キャレットで、置き換えの字は構文より上（replaced > 構文 > text）。
12. **保持できない行は切り抜きの道。** `glyphs_ready` でない行（収集の棄却）は、`text` の色で `DrawTextLayout` を 1 回描き、区間ごとに `tint_runs`（`HitTestTextRange` の矩形で切り抜いて layout を描き直す・置き換えと同じ道）を通す。区間の数 × 行の大きさに比例して遅いが正しい。
13. **見えている x の範囲だけ。** 部分窓は run の描く窓 `[run.first, run.last)` の中に収める（窓は収集器の `trim` が画面の横の範囲に狭めてある）ので、画面の左右の外の字形は描かない。
14. **plain の道の費用は 0。** 字句器を持たない言語では `syntax_begin` が nullopt を返し、`lex_line` / `lex_state` を呼ばず覚えにも触れず、`colors` は空のまま。`.txt` の既存 8 本のベンチの文書は今までどおり。
15. **色つきの速さは任意のベンチで記録だけ。** `eng/measure-speed.py` の `key-to-frame-single-cpp`（`OPTIONAL_BENCHES`・`--bench` で名指ししたときだけ・`--record` だけで基準値に載せない）。文書 `single-cpp-1000.cpp` は長い行のベンチと同じく決定的に作り SHA-256 で守る（5 つの C++ らしい行を番号つきで回す・1 行 100〜120 code points・CRLF・1000 行）。

## 1 打鍵の道の費用

字句器を持たない言語では、`visible_lines` に足したのは `has_lexer` の `switch` 1 回と、行ごとの `std::optional` の空の判定だけ。C / C++ では、覚えが続く限り frame ごとに見えている行の `lex_line`（#815 の probe の Release で 60 行 6,060 byte が中央値 30 µs・約 5 ns/byte）と、スクロールで進んだ行数の `lex_state`。作り直し（編集・タブの切り替え・上へ戻る・4,096 行を超える飛び）は 200 行の遡りが 1 回足される（同じ probe で 200 行 20,200 byte が中央値 99 µs）。描画は区間ごとに部分 run を描くので `DrawGlyphRun` の回数が区間の数に比例して増える。

## 限界（分かっていて今回は採らない）

- 見えている最初の行より 201 行以上前で開いたブロックコメント・raw string は、覚えが無いと見えない（上へ戻ったとき・編集の後・開いた直後の深い位置）。覚えから続く間（下へのスクロール）は届く。
- 巨大な行は状態を plain に戻す（行の中で開いたコメントは次の行へ渡らない）。
- 同じ退避番号で改訂番号を変えずに本文を丸ごと替える道は、空の無題のタブへ開く `with_opened` だけで、そのときの覚えは 1 行目（状態は常に plain）なので正しい。
- `:syntax on|off` は段 3。格子の行（`GridLineRows`）と IME の差し込み行の色は後続。C / C++ 以外の言語は後続。
- cluster の表が無い run と右から左の run は切らず、run 全体を最初に重なる区間の色で描く。合字の途中に区間の境があると、合字の字形は後ろの区間の色になる。
- 保持できない行（決定 12）は区間の数だけ layout を描き直すので遅い。
- `SetDrawingEffect` を使わない（共有の layout と収集器の棄却・ADR 0114 / 0073）。色を `BodyTextLayouts` の鍵に入れない。

## 却下した案

- **layout に `SetDrawingEffect` で色を載せる**: 描画と位置解決で共有する layout（ADR 0114）が色ごとに割れ、収集器は effect つきの run を棄却するので保持した字形の道（ADR 0073）を失う。
- **区間ごとに `HitTestTextRange` の切り抜きで描き直す（置き換えと同じ道）を常に使う**: 区間の数 × 行の大きさになり、1 行に区間が数十ある C++ の行で 1 打鍵ごとに重い。保持できない行だけに残す（決定 12）。
- **文書全体を開いたときに字句解析して行ごとの状態を持つ**: 16 MiB の文書で開く道の費用が増え、編集ごとに後ろの行の状態を直す道が要る。見えている行の手前 200 行と覚えで足りる（Vim と同じ型）。
- **覚えを `EditorState` に置く**: 値型で写しが多く、表示の都合の値が文書の状態に混ざる。
