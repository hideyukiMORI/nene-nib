#include "VimStepDetail.hpp"

#include "Utf8.hpp"
#include "VimBlockLine.hpp"
#include "VimBlockRange.hpp"
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
using detail::block_of;
using detail::block_operated;
using detail::cancelled;
using detail::cased_block;
using detail::cased_text;
using detail::count_of;
using detail::failed;
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
// （Issue #112 で実測）。矩形より手前で終わる行には何も書かない。
[[nodiscard]] std::vector<VimBlockEdit>
replaced_block_edits(const TextBuffer &text, const VimBlockRange &block, char32_t target)
{
    const std::size_t left = block.left.value;
    const std::size_t right = block_right(block);
    std::vector<VimBlockEdit> edits;
    for (std::size_t index = 0; index < block.lines.size(); ++index)
    {
        const VimBlockLine &line = block.lines.at(index);
        const std::size_t columns =
            virtual_width(text.line_text(LineNumber{block.first.value + index}));
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
// 矩形の `~` `u` `U`（ADR 0035 の追補 #959・ADR 0015 の決定 10）。行ごとに `r` と同じ本文
// （端で切れた文字も含む）の ASCII の英字だけを変える。切れた文字は Tab か全角で大小が無い
// ので、空白に割らずそのまま残る（実測）。矩形に届かない行と幅 0 の行は飛ばす。
[[nodiscard]] std::vector<VimBlockEdit>
cased_block_edits(const TextBuffer &text, const VimBlockRange &block, VimOperator operation)
{
    std::vector<VimBlockEdit> edits;
    for (const VimBlockLine &line : block.lines)
    {
        if (is_empty(line.range))
        {
            continue;
        }
        edits.push_back(VimBlockEdit{
            line.range, cased_text(text.text_range(line.range.begin, line.range.end), operation)});
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
// 落ちればその Tab の左側ぶんを空白にする（固定 Vim も Tab だけを割る・実測）。
[[nodiscard]] std::size_t put_lead(const TextBuffer &text, LineNumber line, std::size_t column)
{
    const std::string content = text.line_text(line);
    const std::size_t columns = virtual_width(content);
    if (column > columns)
    {
        return column - single_step - columns;
    }
    const std::size_t byte = byte_at_column(content, VirtualColumn{column});
    const std::size_t begins = column_of(content, byte).value;
    if (begins < column && code_point_at(content, Offset{byte}) == tab_character)
    {
        return column - begins;
    }
    return 0;
}

// 貼る 1 行ぶんの本文。回数ぶん並べ、`padded` 個のうしろだけを幅まで空白で埋める。貼り先の
// 行に本文が続かないときは最後の 1 つを埋めない（固定 Vim の block_prep と同じ・実測）。
[[nodiscard]] std::string put_block_body(const std::string &line, std::size_t width,
                                         std::size_t count, std::size_t padded)
{
    const std::size_t filled = virtual_width(line);
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
[[nodiscard]] VimBlockEdit put_block_line(const TextBuffer &text, LineNumber line,
                                          std::size_t column, const std::string &body)
{
    const std::string content = text.line_text(line);
    const Offset start = text.line_start(line);
    const std::size_t lead = put_lead(text, line, column);
    std::string utf8(lead, ' ');
    utf8 += body;
    if (column > virtual_width(content))
    {
        return VimBlockEdit{OffsetRange{text.line_end(line), text.line_end(line)}, std::move(utf8)};
    }
    const std::size_t byte = byte_at_column(content, VirtualColumn{column});
    if (lead == 0)
    {
        return VimBlockEdit{OffsetRange{Offset{start.value + byte}, Offset{start.value + byte}},
                            std::move(utf8)};
    }
    utf8.append(column_end_of(content, byte).value - column + single_step, ' ');
    return VimBlockEdit{
        OffsetRange{Offset{start.value + byte},
                    Offset{start.value + next_code_point(content, Offset{byte}).value}},
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
[[nodiscard]] std::size_t put_padded(const TextBuffer &text, LineNumber line, std::size_t column,
                                     std::size_t count)
{
    return column <= virtual_width(text.line_text(line)) ? count : count - single_step;
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
    for (std::size_t index = 0; index < lines.size(); ++index)
    {
        const std::size_t number = first + index;
        if (number > last)
        {
            rest.push_back(put_block_body(lines.at(index), width, count, count - single_step));
            continue;
        }
        const LineNumber line{number};
        edits.push_back(put_block_line(
            text, line, column,
            put_block_body(lines.at(index), width, count, put_padded(text, line, column, count))));
    }
    const Offset at{edits.front().range.begin.value + put_lead(text, LineNumber{first}, column)};
    if (!rest.empty())
    {
        edits.push_back(appended_block_lines(text, column, rest));
    }
    return VimInsertBlock{std::move(edits), at};
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
[[nodiscard]] VimStep put_step(const VimState &state, const TextBuffer &text, Offset caret,
                               VimPutSide side)
{
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
                       VimInsertBlock{std::vector<VimBlockEdit>{}, caret}};
    }
    if (source.text.empty())
    {
        // 空の本文は何も貼らず、キャレットも動かさない（Vim は yanklen が 0 なら列を進めない）。
        return VimStep{vim_resting_from(state, state.unnamed_register), VimNoEffect{}};
    }
    if (source.kind == VimRegisterKind::block)
    {
        return VimStep{vim_resting_from(state, state.unnamed_register),
                       put_block(state, text, caret, side)};
    }
    auto body = repeated(source.text, count_of(state.count));
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
