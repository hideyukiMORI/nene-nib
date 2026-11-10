#include "VimStepDetail.hpp"

#include "LineEnding.hpp"
#include "Utf8.hpp"
#include "VimBlockInsert.hpp"
#include "VimBlockLine.hpp"
#include "VimBlockRange.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimLineColumns.hpp"
#include "VimNumberedRule.hpp"
#include "VimPutSide.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::core
{
using detail::block_insert_edit;
using detail::block_of;
using detail::block_operated;
using detail::cancelled;
using detail::cased_block;
using detail::cased_text;
using detail::count_of;
using detail::count_room;
using detail::failed;
using detail::fits_repeated;
using detail::line_of;
using detail::put_characters;
using detail::put_lines;
using detail::put_step;
using detail::refused_register;
using detail::register_read;
using detail::registers_written;
using detail::repeated;
using detail::replaced_block;
using detail::restored_at;
using detail::single_step;
using detail::with_clipboard;

namespace
{
constexpr char32_t tab_character = U'\t';
} // namespace

namespace detail
{
// ---------------------------------------------------------------- 矩形 VISUAL（ADR 0035）

// 矩形が覆う形。`$` が立っているかどうかだけを欲しい列から借りる（決定 2）。
[[nodiscard]] VimBlockRange block_of(const VimState &state, const VimEditorView &view)
{
    if (state.replayed_block.has_value())
    {
        return vim_replayed_block_range(view.text, view.selection, state.replayed_block.value());
    }
    return vim_block_range_for(view.text, view.selection, state.wanted_column);
}
} // namespace detail

namespace
{
// 矩形の左上。取ったあとも消したあとも置き換えたあとも Vim はここへキャレットを置く（実測）。
// 左上が端で切れた Tab や全角なら、その文字のあった所＝埋め直した空白の先頭（ADR 0035 の追補
// #1067・Vim は oap->start へ戻す）。
[[nodiscard]] Offset block_caret(const VimBlockRange &block)
{
    return block.lines.front().range.begin;
}

[[nodiscard]] VimRegister block_register(const TextBuffer &text, const VimBlockRange &block)
{
    return VimRegister{vim_block_text(text, block), VimRegisterKind::block, block.width};
}

// 矩形の右の桁。幅は 1 以上なので左より手前には来ない。
[[nodiscard]] std::size_t block_right(const VimBlockRange &block) noexcept
{
    return block.left.value + block.width.columns - single_step;
}

// 矩形の 1 行が覆った桁の数を埋める本文（ADR 0035 の追補 #1067）。幅 2 の文字は桁の数の半分だけ
// 並べ、奇数なら矩形の右端に届いた行だけ空白 1 つで埋める（Vim の op_replace の `numc / 2` と
// `!bd.is_short`・短い行と `$` の矩形は足さない・実測）。ほかの文字は桁の数だけ並ぶ（Tab も
// 1 桁ずつ・実測）。
[[nodiscard]] std::string replaced_block_fill(std::size_t covered, char32_t target, bool reached)
{
    const bool wide = display_width(target) == DisplayWidth::wide;
    const std::size_t copies = wide ? covered / 2 : covered;
    std::string fill;
    for (std::size_t step = 0; step < copies; ++step)
    {
        append_utf8(fill, target);
    }
    fill.append(wide && reached ? covered % 2 : 0, ' ');
    return fill;
}

// r（決定 4）。行ごとに「矩形が覆った桁の数」を同じ文字で埋める＝ Tab の上では桁の数ぶん並ぶ
// （Issue #112 で実測）。矩形より手前で終わる行には何も書かない。行の幅は line_virtual_width
// （索引を持つ本文は行を写さない・#1237）。
[[nodiscard]] std::vector<VimBlockEdit>
replaced_block_edits(const TextBuffer &text, const VimBlockRange &block, char32_t target)
{
    const std::size_t left = block.left.value;
    const std::size_t right = block_right(block);
    std::vector<VimBlockEdit> edits;
    for (std::size_t index = 0; index < block.lines.size(); ++index)
    {
        const VimBlockLine &line = block.lines.at(index);
        const std::size_t columns = line_virtual_width(text, LineNumber{block.first.value + index});
        if (columns < left || is_empty(line.range))
        {
            continue;
        }
        std::string body(line.keep_lead, ' ');
        const std::size_t covered = std::min(right, columns) - left + single_step;
        // 行の最後の桁が矩形の右の桁に届いたか（桁は 1 から）。`$` の矩形はどの行も届かない行
        // （Vim の is_short・実測）。
        const bool reached = block.wish == VimColumnWish::at_column && columns >= right;
        body.append(replaced_block_fill(covered, target, reached));
        body.append(line.keep_tail, ' ');
        edits.push_back(VimBlockEdit{line.range, std::move(body)});
    }
    return edits;
}
} // namespace

namespace detail
{
[[nodiscard]] VimStep block_operated(const VimState &state, const VimEditorView &view,
                                     VimOperator operation)
{
    const VimBlockRange block = block_of(state, view);
    auto written = registers_written(state, block_register(view.text, block), operation,
                                     VimNumberedRule::by_extent);
    if (!written.has_value())
    {
        return refused_register(state);
    }
    auto [next, clipboard] = std::move(written).value();
    switch (operation)
    {
    case VimOperator::remove:
        return restored_at(
            with_clipboard(VimStep{std::move(next), vim_remove_block(block)}, std::move(clipboard)),
            block_caret(block));
    case VimOperator::yank:
        return with_clipboard(VimStep{std::move(next), VimMoveTo{block_caret(block)}},
                              std::move(clipboard));
    case VimOperator::change:
    // `c` は範囲外（決定 5）。大小文字の矩形は cased_block（追補 #959）、字下げの矩形は #770
    // （ADR 0015 の決定 11）。visual_selection_action が先に振り分ける。
    case VimOperator::toggle_case:
    case VimOperator::lower_case:
    case VimOperator::upper_case:
    case VimOperator::shift_right:
    case VimOperator::shift_left:
        break;
    }
    std::unreachable();
}

[[nodiscard]] VimStep replaced_block(const VimState &state, const VimEditorView &view,
                                     char32_t target)
{
    const VimBlockRange block = block_of(state, view);
    // キャレットも戻り先も矩形の左上（ADR 0052 の決定 10）。左上が切れた Tab や全角なら、その
    // 文字のあった所＝埋めた空白の先頭（Vim の op_replace は oap->start へ戻す・#1067 で実測）。
    return restored_at(VimStep{vim_resting_from(state, state.unnamed_register),
                               VimReplaceBlock{replaced_block_edits(view.text, block, target),
                                               block_caret(block)}},
                       block_caret(block));
}
} // namespace detail

namespace
{
// 矩形の `~` `u` `U`（ADR 0035 の追補 #959 / #1104・ADR 0015 の決定 10）。行ごとに矩形の
// 仮想桁に丸ごと入る本文（`inside`）だけの大小を変える。端に掛かった文字（左右の端の全角や
// Tab）は対象にせず、空白にも割らない（Vim の block_prep の is_del が偽の textlen・#1104 で
// 実測）。矩形に丸ごと入る文字が無い行は飛ばす。
[[nodiscard]] std::vector<VimBlockEdit>
cased_block_edits(const TextBuffer &text, const VimBlockRange &block, VimOperator operation)
{
    std::vector<VimBlockEdit> edits;
    for (const VimBlockLine &line : block.lines)
    {
        if (is_empty(line.inside))
        {
            continue;
        }
        edits.push_back(VimBlockEdit{
            line.inside,
            cased_text(text.text_range(line.inside.begin, line.inside.end), operation)});
    }
    return edits;
}
} // namespace

namespace detail
{
// キャレットも u の戻り先も矩形の左上（切れた Tab の上なら Tab の先頭・実測）。レジスタは書かない。
[[nodiscard]] VimStep cased_block(const VimState &state, const VimEditorView &view,
                                  VimOperator operation)
{
    const VimBlockRange block = block_of(state, view);
    return restored_at(VimStep{vim_resting_from(state, state.unnamed_register),
                               VimReplaceBlock{cased_block_edits(view.text, block, operation),
                                               block_caret(block)}},
                       block_caret(block));
}
} // namespace detail

namespace
{
// 桁が文字（幅 2 以上の文字・Tab・2 桁以上の制御文字など）の途中に落ちれば、その文字のうち桁より
// 前の桁の数。文字の頭ちょうどか、行が桁に届かなければ 0。`columns` は行の幅（呼ぶ側が 1 回引く）。
[[nodiscard]] std::size_t straddled_columns(const VimLineColumns &line, std::size_t columns,
                                            VirtualColumn column)
{
    return column.value <= columns ? column.value - line.column_of(line.byte_at(column)).value : 0;
}

// 矩形の INSERT が 1 行の `column` の桁に入れる本文の前の空白（ADR 0035 の追補 #1042）。桁が文字
// （Tab か全角）の途中に落ちればその文字の手前から桁までを空白で埋め、`A` が届かない行では行末から
// 桁までを埋める（固定 Vim の block_insert・実測）。
[[nodiscard]] std::size_t insert_lead(const VimLineColumns &line, VimBlockInsertSide side,
                                      VirtualColumn column)
{
    const std::size_t columns = line.width();
    const std::size_t inside = straddled_columns(line, columns, column);
    switch (side)
    {
    case VimBlockInsertSide::line_end:
        return 0;
    case VimBlockInsertSide::before:
        return inside;
    case VimBlockInsertSide::after:
        break;
    }
    return column.value <= columns ? inside : column.value - single_step - columns;
}

// `line` の行で INSERT の本文が入る所。桁が Tab の途中なら Tab ごと置き換える（Tab だけを割り、
// 全角は前を空白で埋めて右へ送る・実測）。
[[nodiscard]] OffsetRange insert_range(const VimLineColumns &line, const VimBlockInsert &insert)
{
    const VirtualColumn column = insert.column;
    const Offset start = line.start();
    const Offset end{start.value + line.size()};
    switch (insert.side)
    {
    case VimBlockInsertSide::line_end:
        return OffsetRange{end, end};
    case VimBlockInsertSide::before:
    case VimBlockInsertSide::after:
        break;
    }
    if (column.value > line.width())
    {
        return OffsetRange{end, end};
    }
    const std::size_t byte = line.byte_at(column);
    const Offset at{start.value + byte};
    const bool splits_tab =
        line.column_of(byte).value < column.value && line.code_point_at(byte) == tab_character;
    return OffsetRange{at, splits_tab ? Offset{start.value + line.next_code_point(byte)} : at};
}

// Tab を割ったときに本文の後ろへ残す空白（Tab の残りの桁）。
[[nodiscard]] std::size_t insert_tail(const VimLineColumns &line, const OffsetRange &range,
                                      VirtualColumn column)
{
    if (is_empty(range))
    {
        return 0;
    }
    return line.column_end_of(range.begin.value - line.start().value).value - column.value +
           single_step;
}

// `I` `c` `C` は矩形の左の桁に届かない行を飛ばす（`A` は埋める・実測）。
[[nodiscard]] bool skips_line(const VimLineColumns &line, VimBlockInsertSide side,
                              VirtualColumn column)
{
    switch (side)
    {
    case VimBlockInsertSide::before:
        return column.value > line.width() + single_step;
    case VimBlockInsertSide::after:
    case VimBlockInsertSide::line_end:
        break;
    }
    return false;
}
} // namespace

namespace detail
{
// 矩形の INSERT の本文を 1 行に入れる形（ADR 0035 の追補 #1042）。Esc が 1 行目の本文を残りの行へ
// 写すときと、`I` `A` の入口が 1 行目を埋めるとき（本文は空）に使う 1 本。写す桁と向きは `insert`
// から読む。飛ばす行は空。
[[nodiscard]] std::optional<VimBlockEdit> block_insert_edit(const TextBuffer &text, LineNumber line,
                                                            const VimBlockInsert &insert,
                                                            std::string_view body)
{
    const VimLineColumns columns(text, line);
    if (skips_line(columns, insert.side, insert.column))
    {
        return std::nullopt;
    }
    const OffsetRange range = insert_range(columns, insert);
    std::string utf8(insert_lead(columns, insert.side, insert.column), ' ');
    utf8 += body;
    utf8.append(insert_tail(columns, range, insert.column), ' ');
    return VimBlockEdit{range, std::move(utf8)};
}

// 矩形の `I` が Esc で残りの行へ写す本文（ADR 0035 の追補 #1196）。1 行目の左の端が文字の途中に
// 掛かっていたら（`straddled`）、打った本文をその文字の頭の桁から並べ、左の桁を含む Vim の 1 文字
// （切れればその文字ごと）から後ろだけを写す。左の桁に届かなければ空（固定 Vim の op_insert は
// 打った後の 1 行目を block_prep(is_del = TRUE) し直し、bd2.textcol から伸びたぶんを読む）。
[[nodiscard]] std::string block_copied_body(const VimBlockInsert &insert, std::string body)
{
    if (insert.straddled == 0)
    {
        return body;
    }
    const std::size_t edge = insert.column.value - single_step;
    std::size_t column = edge - insert.straddled;
    Offset at{0};
    while (at.value < body.size() && column < edge)
    {
        const std::size_t width = column_advance(code_point_at(body, at), column);
        if (column + width > edge)
        {
            break;
        }
        column += width;
        at = vim_character_end(body, at);
    }
    return body.substr(at.value);
}

} // namespace detail

namespace
{
// `$` で取った矩形かどうか（欲しい列が行末）。`A` は各行の内容の終わりへ写す（実測）。
[[nodiscard]] bool at_line_end(const VimState &state) noexcept
{
    return state.wanted_column.has_value() &&
           state.wanted_column.value().wish == VimColumnWish::at_line_end;
}

// INSERT へ入った状態。`I` `A` の回数は打った本文を回数ぶん入れてから写す（実測 `2Iab`）。
[[nodiscard]] VimState block_insert_entered(VimState next, std::size_t count,
                                            const VimBlockInsert &insert)
{
    next.mode = VimMode::insert;
    if (count > single_step)
    {
        next.insert_repeat = VimInsertRepeat{count - single_step, VimRecordedKeys::from({})};
    }
    next.block_insert = insert;
    return next;
}

// `C` の矩形。選んだ矩形を `$` で取り直す（固定 Vim の v_visop は curswant を MAXCOL にしてから
// `c` を流す）。`.` の再生では記録の大きさの左上から取り直す。
[[nodiscard]] VimBlockRange line_end_block_of(const VimState &state, const VimEditorView &view)
{
    if (state.replayed_block.has_value())
    {
        VimBlockExtent extent = state.replayed_block.value();
        extent.wish = VimColumnWish::at_line_end;
        return vim_replayed_block_range(view.text, view.selection, extent);
    }
    return vim_block_range(view.text, view.selection, VimColumnWish::at_line_end);
}

// `A` の 1 行目で右の端が文字（全角・Tab・制御文字）の途中に掛かれば、その文字の範囲。その文字の
// 後ろから打つ（空白で埋めず割らない・固定 Vim の op_insert はカーソルを矩形の本文の次へ進める・
// `.` の再生で実測）。残りの行は 写すときに桁で埋める（block_insert_edit）。掛からなければ空。
// 「右の端が文字の途中に掛かるか」を決めるのはここ 1 か所（Esc の写しは覚えた `carried` を読む）。
[[nodiscard]] std::optional<OffsetRange> after_straddled(const VimBlockInsert &insert,
                                                         const VimLineColumns &line)
{
    switch (insert.side)
    {
    case VimBlockInsertSide::before:
    case VimBlockInsertSide::line_end:
        return std::nullopt;
    case VimBlockInsertSide::after:
        break;
    }
    if (straddled_columns(line, line.width(), insert.column) == 0)
    {
        return std::nullopt;
    }
    const std::size_t byte = line.byte_at(insert.column);
    const std::size_t start = line.start().value;
    return OffsetRange{Offset{start + byte}, Offset{start + line.character_end(byte, single_step)}};
}

// 切れた文字のうち、Esc で残りの行へ写す頭のバイト数（ADR 0035 の追補 #1189）。固定 Vim の
// op_insert は打った後の 1 行目を block_prep し直し、端の文字が切れていれば（endspaces）textlen を
// 1 バイト戻してそこから写す本文を読む。1 バイトの文字（Tab・制御文字）ならその文字ごと写る
// （実測 `block-append-tab-pad-`）。全角は Vim が UTF-8 の途中のバイトを写して壊すので揃えず 0。
[[nodiscard]] std::size_t carried_bytes(const std::optional<OffsetRange> &straddled) noexcept
{
    if (!straddled.has_value())
    {
        return 0;
    }
    const OffsetRange range = straddled.value();
    return range.end.value - range.begin.value == single_step ? single_step : 0;
}

// `I` の 1 行目で左の端が文字の途中に掛かるときの、端より前の桁の数（ADR 0035 の追補 #1196）。
// 1 行目はその文字の頭から打つ（空白で埋めず Tab も割らない・固定 Vim は矩形の左上を
// coladvance で文字の頭に置く）。`A` は 0（右の端は after_straddled）。
[[nodiscard]] std::size_t before_straddled(const VimBlockInsert &insert, const VimLineColumns &line)
{
    switch (insert.side)
    {
    case VimBlockInsertSide::after:
    case VimBlockInsertSide::line_end:
        return 0;
    case VimBlockInsertSide::before:
        break;
    }
    return straddled_columns(line, line.width(), insert.column);
}

// 入る前の 1 行目の形（本文は空）。埋める空白か割った Tab があれば、打つ本文と同じ単位に入る。
[[nodiscard]] VimBlockEdit first_line_entry(const TextBuffer &text, const VimBlockInsert &insert,
                                            const VimLineColumns &line,
                                            const std::optional<OffsetRange> &straddled)
{
    if (straddled.has_value())
    {
        const Offset after = straddled.value().end;
        return VimBlockEdit{OffsetRange{after, after}, std::string{}};
    }
    if (insert.straddled > 0)
    {
        const Offset head{line.start().value + line.byte_at(insert.column)};
        return VimBlockEdit{OffsetRange{head, head}, std::string{}};
    }
    const Offset end = text.line_end(insert.first);
    return block_insert_edit(text, insert.first, insert, std::string_view{})
        .value_or(VimBlockEdit{OffsetRange{end, end}, std::string{}});
}

// `I` `A` が写す向き。`A` は `$` の矩形なら各行の内容の終わり。
[[nodiscard]] VimBlockInsertSide insert_side(const VimState &state, VimAction action) noexcept
{
    if (action != VimAction::insert_at_line_end)
    {
        return VimBlockInsertSide::before;
    }
    return at_line_end(state) ? VimBlockInsertSide::line_end : VimBlockInsertSide::after;
}

// 矩形の `c` `C` が 1 行目に打ち始める所（ADR 0035 の追補 #1108）。ふつうは消したあとの左の桁
// （切れた文字の外側の空白の後ろ）。ただし左の端が文字（Tab か全角）の途中に掛かって外側を空白で
// 残し、その後ろに何も残らないときは、残した最後の空白の手前から打つ（固定 Vim の op_delete が
// カーソルを `textcol + startspaces` へ置いて check_cursor_col が行末の 1 つ手前へ戻し、op_change
// の inc_cursor は矩形の左上の文字が手前なので戻さない・実測 `block-change-edge-issue-C`）。
[[nodiscard]] Offset change_entry(const TextBuffer &text, const VimBlockRange &block,
                                  const VimRemoveBlock &removal)
{
    const VimBlockLine &top = block.lines.front();
    const bool nothing_after =
        top.keep_tail == 0 && top.range.end.value == text.line_end(block.first).value;
    if (top.keep_lead > 0 && nothing_after)
    {
        return Offset{removal.caret.value - single_step};
    }
    return removal.caret;
}
} // namespace

namespace detail
{
// 矩形の `I` `A`（ADR 0035 の追補 #1042）。1 行目だけに打ち、Esc で残りの行へ写す。1 行目が桁に
// 届かない `A` や桁が Tab の途中なら、入る前に 1 行目を空白で埋める（固定 Vim の op_insert）。
// u の戻り先は打ち始める所（実測）。
[[nodiscard]] VimStep block_inserted(const VimState &state, const VimEditorView &view,
                                     VimAction action)
{
    const VimBlockRange block = block_of(state, view);
    const TextBuffer &text = view.text;
    const VimBlockInsertSide side = insert_side(state, action);
    VimBlockInsert insert{block.first,
                          block.lines.size(),
                          side,
                          side == VimBlockInsertSide::before
                              ? block.left
                              : VirtualColumn{block_right(block) + single_step},
                          0,
                          0,
                          block_caret(block),
                          0,
                          0};
    const VimLineColumns line(text, block.first);
    const std::optional<OffsetRange> straddled = after_straddled(insert, line);
    insert.carried = carried_bytes(straddled);
    insert.straddled = before_straddled(insert, line);
    VimBlockEdit entry = first_line_entry(text, insert, line, straddled);
    const Offset at{entry.range.begin.value +
                    (entry.utf8.empty() ? 0 : insert_lead(line, side, insert.column))};
    insert.at_byte = at.value - line.start().value;
    insert.line_bytes =
        line.size() - (entry.range.end.value - entry.range.begin.value) + entry.utf8.size();
    VimState next = block_insert_entered(vim_resting_from(state, state.unnamed_register),
                                         count_of(state.count), insert);
    if (entry.utf8.empty() && is_empty(entry.range))
    {
        return restored_at(VimStep{std::move(next), VimMoveTo{at}}, at);
    }
    const Offset begin = entry.range.begin;
    // 埋めた編集は打つ本文と同じ INSERT の単位に入る（controller が absorb で写す）。
    return restored_at(
        VimStep{std::move(next), VimReplaceBlock{std::vector<VimBlockEdit>{std::move(entry)}, at}},
        begin);
}

// 矩形の `c` `C`（ADR 0035 の追補 #1042）。`d` と同じく矩形を消してレジスタ（矩形の種類）へ書き、
// 左の桁から `I` と同じに入る。キャレットは Esc のふつうの位置、u の戻り先は矩形の左上（実測）。
[[nodiscard]] VimStep block_changed(const VimState &state, const VimEditorView &view,
                                    VimColumnWish wish)
{
    const VimBlockRange block =
        wish == VimColumnWish::at_line_end ? line_end_block_of(state, view) : block_of(state, view);
    auto written = registers_written(state, block_register(view.text, block), VimOperator::change,
                                     VimNumberedRule::by_extent);
    if (!written.has_value())
    {
        return refused_register(state);
    }
    auto [next, clipboard] = std::move(written).value();
    VimRemoveBlock removal = vim_remove_block(block);
    removal.caret = change_entry(view.text, block, removal);
    const VimBlockLine &top = block.lines.front();
    // 1 行目のバイト数は行を写さず行頭と行末の差で数える（#1237）。
    const std::size_t line_bytes =
        (view.text.line_end(block.first).value - view.text.line_start(block.first).value) -
        (top.range.end.value - top.range.begin.value) + top.keep_lead + top.keep_tail;
    VimState entered = block_insert_entered(
        std::move(next), single_step,
        VimBlockInsert{block.first, block.lines.size(), VimBlockInsertSide::before, block.left,
                       removal.caret.value - view.text.line_start(block.first).value, line_bytes,
                       std::nullopt, 0, 0});
    return restored_at(
        with_clipboard(VimStep{std::move(entered), std::move(removal)}, std::move(clipboard)),
        block_caret(block));
}
} // namespace detail

namespace
{
// 矩形レジスタの行（本文は LF で区切られ、末尾に改行は無い）。
[[nodiscard]] std::vector<std::string> block_register_lines(std::string_view body)
{
    std::vector<std::string> lines;
    std::size_t at = 0;
    while (true)
    {
        const std::size_t found = body.find('\n', at);
        const std::size_t end = found == std::string_view::npos ? body.size() : found;
        lines.emplace_back(body.substr(at, end - at));
        if (found == std::string_view::npos)
        {
            return lines;
        }
        at = found + single_step;
    }
}

// 貼り付ける桁（決定 6）。`p` は今いる文字の次の桁、`P` はその文字の桁。空行は 1 桁目（実測）。
[[nodiscard]] std::size_t put_column(const TextBuffer &text, Offset caret, VimPutSide side)
{
    const LineNumber line = line_of(text, caret);
    if (text.line_start(line).value >= text.line_end(line).value)
    {
        return single_step;
    }
    return side == VimPutSide::after ? virtual_column_end(text, caret).value + single_step
                                     : virtual_column(text, caret).value;
}

// 貼る本文が始まる桁までに入れる空白の数。行が桁より短ければ末尾まで埋め、桁が Tab の途中に
// 落ちればその Tab の左側ぶんを空白にする（固定 Vim も Tab だけを割る・実測）。桁は VimLineColumns
// に問う（索引を持つ本文は行を写さない・#1237）。
[[nodiscard]] std::size_t put_lead(const VimLineColumns &line, std::size_t column)
{
    const std::size_t columns = line.width();
    if (column > columns)
    {
        return column - single_step - columns;
    }
    const std::size_t byte = line.byte_at(VirtualColumn{column});
    const std::size_t begins = line.column_of(byte).value;
    if (begins < column && line.code_point_at(byte) == tab_character)
    {
        return column - begins;
    }
    return 0;
}

// Vim の do_put が 1 文字の先頭のバイトに数える桁（#1117）。Tab はここに来ない。固定 Vim 9.1 の
// oracle で測ったのは、全角（2）・幅 0 の結合文字 U+0301（1）・書式用文字 U+200B（6）・C1 制御
// 文字 U+0085（1。表示の `<85>` の 4 ではない）・4 バイトの絵文字 U+1F600（2）と ASCII の 1 桁の
// 文字（fixture `block-put-wide-`）。ASCII の制御文字（`^X` の 2）と、同じ分類のほかの文字が
// 測った代表と同じ値になることは Vim のソース（ptr2cells）の読みで、測っていない。
[[nodiscard]] std::size_t put_lead_cells(DisplayWidth width) noexcept
{
    switch (width)
    {
    case DisplayWidth::zero:
    case DisplayWidth::single:
    case DisplayWidth::hex:
        return 1;
    case DisplayWidth::wide:
        return 2;
    case DisplayWidth::unprintable:
        return 6;
    }
    std::unreachable();
}

// 右の埋めのために Vim の do_put が数えるレジスタの 1 行の幅（#1117）。Vim は桁を 0 に戻しながら
// 1 バイトずつ数えるので、Tab はどこにあっても tabstop ぶん（Tab の後ろの本文で実測）、先頭の
// バイトは `put_lead_cells`、2 バイト目からの続きのバイトは 1 つにつき 4 桁（不正なバイトの
// `<xx>`。2〜4 バイトの文字で実測）。表示の幅とは違う、埋めのためだけの数。
[[nodiscard]] std::size_t put_block_cells(std::string_view line) noexcept
{
    constexpr std::size_t continuation_cells = 4;
    std::size_t cells = 0;
    std::size_t byte = 0;
    while (byte < line.size())
    {
        const char32_t value = code_point_at(line, Offset{byte});
        const std::size_t next = next_code_point(line, Offset{byte}).value;
        cells += value == tab_character ? vim_tab_stop : put_lead_cells(display_width(value));
        cells += (next - byte - single_step) * continuation_cells;
        byte = next;
    }
    return cells;
}

// 貼る 1 行ぶんの本文。回数ぶん並べ、`padded` 個のうしろだけを幅まで空白で埋める。貼り先の
// 行に本文が続かないときは最後の 1 つを埋めない（固定 Vim の block_prep と同じ・実測）。埋める
// 数は `put_block_cells` で数える（Vim の do_put の spaces と同じ・#1117）。
[[nodiscard]] std::string put_block_body(const std::string &line, std::size_t width,
                                         std::size_t count, std::size_t padded)
{
    const std::size_t filled = put_block_cells(line);
    const std::size_t spaces = width > filled ? width - filled : 0;
    std::string body;
    for (std::size_t step = 0; step < count; ++step)
    {
        body += line;
        if (step < padded)
        {
            body.append(spaces, ' ');
        }
    }
    return body;
}

// 1 行ぶんの貼付。行が桁より短ければ手前を空白で埋め、桁が Tab の途中に落ちればその Tab を
// 空白に割って置き換える（固定 Vim も Tab だけを割る・実測）。
[[nodiscard]] VimBlockEdit put_block_line(const VimLineColumns &line, std::size_t column,
                                          const std::string &body)
{
    const Offset start = line.start();
    const std::size_t lead = put_lead(line, column);
    std::string utf8(lead, ' ');
    utf8 += body;
    if (column > line.width())
    {
        const Offset end{start.value + line.size()};
        return VimBlockEdit{OffsetRange{end, end}, std::move(utf8)};
    }
    const std::size_t byte = line.byte_at(VirtualColumn{column});
    if (lead == 0)
    {
        return VimBlockEdit{OffsetRange{Offset{start.value + byte}, Offset{start.value + byte}},
                            std::move(utf8)};
    }
    utf8.append(line.column_end_of(byte).value - column + single_step, ' ');
    return VimBlockEdit{
        OffsetRange{Offset{start.value + byte}, Offset{start.value + line.next_code_point(byte)}},
        std::move(utf8)};
}

// 行が足りないぶんは文書の末尾へ新しい行として足す（決定 6）。桁までは空白で埋める。
[[nodiscard]] VimBlockEdit appended_block_lines(const TextBuffer &text, std::size_t column,
                                                const std::vector<std::string> &rest)
{
    const Offset at = text.line_end(LineNumber{text.line_count()});
    std::string utf8;
    for (const std::string &body : rest)
    {
        utf8 += '\n';
        utf8.append(column - single_step, ' ');
        utf8 += body;
    }
    return VimBlockEdit{OffsetRange{at, at}, std::move(utf8)};
}

// 貼り先の行に本文が続くかどうか。続くときは矩形の形を保つために右も空白で埋める（実測）。
[[nodiscard]] std::size_t put_padded(const VimLineColumns &line, std::size_t column,
                                     std::size_t count)
{
    return column <= line.width() ? count : count - single_step;
}
} // namespace

namespace detail
{
// `p` `P` と `@` の読み元（ADR 0048 の決定 5・ADR 0050 の決定 5）。名前つきは表の 1 本（`"Ap`
// `@A` も同じ）、数字は `"0`〜`"9` の表の 1 本、`"-` は小削除、選択が無いか `""` なら無名、`"_`
// は空（空のレジスタと同じく何も貼らない）、`"+` `"*` は controller が置いた写し（ADR 0051 の
// 決定 5）。
[[nodiscard]] VimRegister register_read(const VimState &state,
                                        const std::optional<VimRegisterSelection> &chosen)
{
    if (!chosen.has_value())
    {
        return state.unnamed_register.value();
    }
    const VimRegisterSelection selection = chosen.value();
    switch (selection.target)
    {
    case VimRegisterTarget::named:
        return state.registers.registers
            .at(vim_register_index(static_cast<char32_t>(selection.name)).value_or(0))
            .value();
    case VimRegisterTarget::unnamed:
        return state.unnamed_register.value();
    case VimRegisterTarget::black_hole:
        return VimRegister{"", VimRegisterKind::uninitialized};
    case VimRegisterTarget::numbered:
        return state.numbered.registers
            .at(vim_numbered_index(static_cast<char32_t>(selection.name)).value_or(0))
            .value();
    case VimRegisterTarget::small_delete:
        return state.small_delete.value();
    case VimRegisterTarget::clipboard:
        return state.clipboard.value();
    }
    std::unreachable();
}
} // namespace detail

namespace
{
[[nodiscard]] VimInsertBlock put_block(const VimState &state, const TextBuffer &text, Offset caret,
                                       VimPutSide side)
{
    const VimRegister source = register_read(state, state.selected_register);
    const std::vector<std::string> lines = block_register_lines(source.text);
    const std::size_t width = source.width.has_value() ? source.width.value().columns : single_step;
    const std::size_t count = count_of(state.count);
    const std::size_t column = put_column(text, caret, side);
    const std::size_t first = line_of(text, caret).value;
    const std::size_t last = std::min(first + lines.size() - single_step, text.line_count());
    std::vector<VimBlockEdit> edits;
    std::vector<std::string> rest;
    // 1 行目の手前の空白（キャレットは貼った本文の頭）。行ごとの桁は 1 行に 1 つの VimLineColumns
    // に問う（#1237）。
    std::size_t first_lead = 0;
    for (std::size_t index = 0; index < lines.size(); ++index)
    {
        const std::size_t number = first + index;
        if (number > last)
        {
            rest.push_back(put_block_body(lines.at(index), width, count, count - single_step));
            continue;
        }
        const VimLineColumns line(text, LineNumber{number});
        if (index == 0)
        {
            first_lead = put_lead(line, column);
        }
        edits.push_back(put_block_line(
            line, column,
            put_block_body(lines.at(index), width, count, put_padded(line, column, count))));
    }
    const Offset at{edits.front().range.begin.value + first_lead};
    if (!rest.empty())
    {
        edits.push_back(appended_block_lines(text, column, rest));
    }
    return VimInsertBlock{std::move(edits), at, EditBoundary::separate};
}

// 矩形の `p` `P` の本文が文書の上限に収まるか（#1341）。1 行は回数ぶんの本文と幅までの空白で、
// 本文の外に足すのは多くとも max(桁, tabstop) と改行（文書の改行の形の幅）: 行末より右の桁は
// 手前を桁 − 1 個の空白で埋め、足す行は改行と桁 − 1 個の空白、桁の途中で割る Tab（1 バイト）は
// 多くとも tabstop 個の空白になる（PR #1348 の再レビュー M2）。Vim は行ごとにそのまま確保する
// （`<C-v>jy999999999p` は 2 GB の 2 行を作る・実測）。
[[nodiscard]] bool block_put_fits(const VimState &state, const VimEditorView &view, VimPutSide side)
{
    const TextBuffer &text = view.text;
    const Offset caret = view.selection.caret;
    const VimRegister source = register_read(state, state.selected_register);
    const std::size_t width = source.width.has_value() ? source.width.value().columns : single_step;
    const std::size_t count = count_of(state.count);
    const std::size_t lead = std::max(put_column(text, caret, side), vim_tab_stop) +
                             newline_of(text.line_ending()).size();
    std::size_t room = count_room(text, view.document_limit);
    for (const std::string &line : block_register_lines(source.text))
    {
        const std::size_t cells = put_block_cells(line);
        const std::size_t piece = line.size() + (width > cells ? width - cells : 0);
        if (room < lead || !fits_repeated(piece, count, room - lead))
        {
            return false;
        }
        room -= lead + piece * count;
    }
    return true;
}

// 貼れないレジスタ（E353）。未使用と空のクリップボードだけ（ADR 0051 の B6）。空の本文で書かれた
// レジスタ（空行の `C`・#960）は Vim の do_put の y_size が 1 なので拒まない（実測）。
[[nodiscard]] bool refuses_put(const VimState &state, const VimRegister &source)
{
    const bool clipboard = state.selected_register.has_value() &&
                           state.selected_register.value().target == VimRegisterTarget::clipboard;
    return source.kind == VimRegisterKind::uninitialized || (source.text.empty() && clipboard);
}
} // namespace

namespace detail
{
[[nodiscard]] VimStep put_step(const VimState &state, const VimEditorView &view, VimPutSide side)
{
    const TextBuffer &text = view.text;
    const Offset caret = view.selection.caret;
    const VimRegister source = register_read(state, state.selected_register);
    const bool black_hole = state.selected_register.has_value() &&
                            state.selected_register.value().target == VimRegisterTarget::black_hole;
    if (!black_hole && refuses_put(state, source))
    {
        return failed(cancelled(state), VimRepeatFailure::refused);
    }
    if (black_hole)
    {
        // `"_` からの貼り付けは拒まずに何もしない（Vim の do_put はブラックホールなら黙って返る）。
        // それでも直前の変更になり `.` は `"_p` を繰り返す（nv_put は do_put の前に redo を置く・
        // 実測）。記録に載る「何も入れない貼付」は行の無い矩形の貼付で表す（本文・履歴・キャレット
        // を変えない・#988・ADR 0050 の決定 5 の追補）。
        return VimStep{vim_resting_from(state, state.unnamed_register),
                       VimInsertBlock{std::vector<VimBlockEdit>{}, caret, EditBoundary::separate}};
    }
    if (source.kind == VimRegisterKind::block)
    {
        if (!block_put_fits(state, view, side))
        {
            return failed(cancelled(state), VimRepeatFailure::too_large);
        }
        // 本文が空の矩形（空の行で始まる矩形の `C` `c`）も 1 行・幅つきの矩形として貼り、幅の
        // ぶんを空白で埋める（Vim の do_put の MBLOCK は y_size 1 の空の行を幅まで埋める・#1158）。
        return VimStep{vim_resting_from(state, state.unnamed_register),
                       put_block(state, text, caret, side)};
    }
    if (source.text.empty())
    {
        // 空の本文は何も貼らず、キャレットも動かさない（Vim は yanklen が 0 なら列を進めない）。
        return VimStep{vim_resting_from(state, state.unnamed_register), VimNoEffect{}};
    }
    auto body = repeated(source.text, count_of(state.count), text, view.document_limit);
    if (!body.has_value())
    {
        return failed(cancelled(state), body.error());
    }
    VimState next = vim_resting_from(state, state.unnamed_register);
    switch (source.kind)
    {
    case VimRegisterKind::uninitialized:
        // An uninitialized register is refused above before put dispatch.
    case VimRegisterKind::block:
        // A block register is handled above, before the characterwise / linewise dispatch.
        std::unreachable();
    case VimRegisterKind::characters:
        return VimStep{std::move(next), put_characters(text, caret, std::move(body).value(), side)};
    case VimRegisterKind::lines:
        return VimStep{std::move(next), put_lines(text, caret, std::move(body).value(), side)};
    }
    std::unreachable();
}
} // namespace detail
} // namespace nenenib::core
