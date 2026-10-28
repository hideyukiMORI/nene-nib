#include "VimStepDetail.hpp"

#include "VimCaret.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimKeyTable.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace nenenib::core
{
using detail::backward_characters;
using detail::caret_on_line;
using detail::combined_count;
using detail::count_of;
using detail::forward_characters;
using detail::line_above;
using detail::line_below;
using detail::line_of;
using detail::resolved_count;
using detail::resolved_motion_count;
using detail::rested_in;
using detail::single_step;
using detail::wanted_column_of;

namespace detail
{
[[nodiscard]] std::size_t count_of(const std::optional<VimCount> &count) noexcept
{
    return count.has_value() ? count.value().value : single_step;
}

// 明示された動作回数。オペレータ側と移動側のどちらも指定されたときは飽和する積にする。
// どちらも無ければ nullopt のまま保ち、G の既定値だけを motion 解決時に最終行へ写せるようにする。
[[nodiscard]] std::optional<VimCount> combined_count(const VimState &state) noexcept
{
    const auto pending = state.pending.has_value() ? state.pending.value().count : std::nullopt;
    if (!pending.has_value() && !state.count.has_value())
    {
        return std::nullopt;
    }
    const std::size_t operation = count_of(pending);
    const std::size_t motion = count_of(state.count);
    const std::size_t highest = std::numeric_limits<std::size_t>::max();
    return VimCount{operation > highest / motion ? highest : operation * motion};
}

[[nodiscard]] std::size_t resolved_count(const VimState &state) noexcept
{
    return count_of(combined_count(state));
}

[[nodiscard]] std::size_t resolved_motion_count(const VimState &state, const TextBuffer &text,
                                                VimMotion motion) noexcept
{
    const auto specified = combined_count(state);
    if (specified.has_value())
    {
        return specified.value().value;
    }
    // `%` の回数なしは 0（回数つきの `{N}%` と分ける・回数 0 は回数なしと同じ）。
    if (motion == VimMotion::matching_pair)
    {
        return 0;
    }
    return motion == VimMotion::document_last ? text.line_count() : single_step;
}

[[nodiscard]] LineNumber line_of(const TextBuffer &text, Offset caret)
{
    return text.position_of(caret).line;
}

// 行の中で count 文字ぶん先（1 文字は ADR 0053 の境）。
// 行の内容の終わり（Vim が NUL を置く桁）までは進める。
[[nodiscard]] Offset forward_characters(const TextBuffer &text, Offset caret, std::size_t count)
{
    const LineNumber line = line_of(text, caret);
    const Offset start = text.line_start(line);
    const std::string content = text.text_range(start, text.line_end(line));
    std::size_t index = caret.value - start.value;
    for (std::size_t step = 0; step < count && index < content.size(); ++step)
    {
        index = vim_character_end(content, Offset{index}).value;
    }
    return Offset{start.value + index};
}

[[nodiscard]] Offset backward_characters(const TextBuffer &text, Offset caret, std::size_t count)
{
    const LineNumber line = line_of(text, caret);
    const Offset start = text.line_start(line);
    const std::string content = text.text_range(start, text.line_end(line));
    std::size_t index = caret.value - start.value;
    for (std::size_t step = 0; step < count && index > 0; ++step)
    {
        index = vim_character_start(content, Offset{index}).value;
    }
    return Offset{start.value + index};
}

[[nodiscard]] VimWantedColumn wanted_column_of(const TextBuffer &text, const VimState &state,
                                               Offset caret)
{
    if (state.wanted_column.has_value())
    {
        return state.wanted_column.value();
    }
    return VimWantedColumn{VimColumnWish::at_column, caret_virtual_column(text, caret)};
}

// 動いた先のキャレットの置き所。NORMAL / INSERT は文字の上に寄せ、VISUAL は行の内容の終わり
// （Vim が NUL を置く桁）にも載せる（ADR 0018 の決定 6）。Vim の coladvance の one_more が
// VIsual_active で立つのと同じで、`v$d` が改行まで消し、`vl` が行末に届くのはこれ（Issue #53
// で実測）。
[[nodiscard]] Offset rested_in(const TextBuffer &text, Offset caret, VimMode mode)
{
    switch (mode)
    {
    case VimMode::visual:
    case VimMode::visual_line:
    case VimMode::visual_block:
        return caret;
    case VimMode::normal:
    case VimMode::insert:
        return vim_resting_caret(text, caret);
    }
    std::unreachable();
}

// 欲しい列（curswant）で別の行へ。$ が貼り付けた「行末」はその行の最後の文字になる。
[[nodiscard]] Offset caret_on_line(const TextBuffer &text, const VimWantedColumn &wanted,
                                   LineNumber line, VimMode mode)
{
    switch (wanted.wish)
    {
    case VimColumnWish::at_line_end:
        return rested_in(text, text.line_end(line), mode);
    case VimColumnWish::at_column:
        return rested_in(text, offset_at_virtual_column(text, line, wanted.column), mode);
    }
    std::unreachable();
}

[[nodiscard]] LineNumber line_below(const TextBuffer &text, LineNumber line, std::size_t count)
{
    const std::size_t current = std::min(line.value, text.line_count());
    const std::size_t step = std::min(count, text.line_count() - current);
    return LineNumber{current + step};
}

[[nodiscard]] LineNumber line_above(LineNumber line, std::size_t count)
{
    return LineNumber{line.value > count ? line.value - count : 1};
}
} // namespace detail
} // namespace nenenib::core
