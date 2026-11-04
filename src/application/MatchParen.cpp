#include "MatchParen.hpp"

#include "GiantLineWindow.hpp"
#include "Utf8.hpp"
#include "VimMatchingPair.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::application
{
std::vector<core::OffsetRange> color_span_bytes(std::string_view text,
                                                std::span<const core::ColorSpan> colors)
{
    std::vector<core::OffsetRange> bytes;
    bytes.reserve(colors.size());
    std::size_t byte = 0;
    std::size_t column = 1;
    // 桁は昇順なので、行の先頭から 1 回だけ前へ歩いて写す（区間ごとに行頭へ戻らない）。
    const auto byte_at = [&byte, &column, text](core::Column wanted)
    {
        for (; column < wanted.value && byte < text.size(); ++column)
        {
            byte = core::next_code_point(text, core::Offset{byte}).value;
        }
        return core::Offset{byte};
    };
    for (const auto &span : colors)
    {
        const auto begin = byte_at(span.begin);
        bytes.push_back(core::OffsetRange{begin, byte_at(span.end)});
    }
    return bytes;
}

namespace
{
using Ranges = std::vector<core::OffsetRange>;
using Roles = std::vector<core::SyntaxRole>;

constexpr std::string_view bracket_bytes{"()[]{}"};

[[nodiscard]] bool bracket_byte(char byte) noexcept
{
    return bracket_bytes.find(byte) != std::string_view::npos;
}

[[nodiscard]] bool excluded_role(core::SyntaxRole role) noexcept
{
    switch (role)
    {
    case core::SyntaxRole::comment:
    case core::SyntaxRole::string:
        return true;
    case core::SyntaxRole::keyword:
    case core::SyntaxRole::number:
    case core::SyntaxRole::type:
    case core::SyntaxRole::function:
    case core::SyntaxRole::constant:
    case core::SyntaxRole::operators:
    case core::SyntaxRole::error:
    case core::SyntaxRole::warning:
        return false;
    }
    std::unreachable();
}

// INSERT と通常モード（棒のキャレット）はキャレットの直前の文字も見る（決定 1）。
[[nodiscard]] bool looks_before(const EditorState &state) noexcept
{
    switch (state.mode())
    {
    case core::EditMode::ordinary:
        return true;
    case core::EditMode::vim:
        return state.vim().mode == core::VimMode::insert;
    }
    std::unreachable();
}

[[nodiscard]] std::vector<LineView>::iterator line_of(std::vector<LineView> &lines,
                                                      core::LineNumber number)
{
    return std::ranges::find_if(lines,
                                [number](const LineView &line) { return line.number == number; });
}

// キャレットの行の中の、探す括弧の本文の位置（下を先に、なければ直前）。
[[nodiscard]] std::optional<core::Offset> caret_bracket(const EditorState &state,
                                                        const LineView &line, core::Offset start)
{
    const std::size_t at = state.selection().caret.value - start.value;
    if (at < line.text.size() && bracket_byte(line.text.at(at)))
    {
        return core::Offset{start.value + at};
    }
    if (at > 0 && at <= line.text.size() && looks_before(state) &&
        bracket_byte(line.text.at(at - 1)))
    {
        return core::Offset{start.value + at - 1};
    }
    return std::nullopt;
}

// 見えている行の前後 screen 行まで、キャレットの行から外へ歩いて
// 巨大な行の手前で止めた窓（決定 2）。
[[nodiscard]] core::OffsetRange search_window(const core::TextBuffer &text, core::LineNumber caret,
                                              const std::vector<LineView> &lines,
                                              std::size_t screen)
{
    const std::size_t first = lines.front().number.value;
    const std::size_t low = first > screen ? first - screen : 1;
    const std::size_t high = std::min(lines.back().number.value + screen, text.line_count());
    std::size_t begin = caret.value;
    while (begin > low && giant_line_index(text, core::LineNumber{begin - 1}) == nullptr)
    {
        --begin;
    }
    std::size_t end = caret.value;
    while (end < high && giant_line_index(text, core::LineNumber{end + 1}) == nullptr)
    {
        ++end;
    }
    return core::OffsetRange{text.line_start(core::LineNumber{begin}),
                             text.line_terminator_end(core::LineNumber{end})};
}

// 1 行の string / comment の区間を本文の位置で足す。roles は同じ順の役割。
void add_excluded(const core::TextBuffer &text, const LineView &line, Ranges &ranges, Roles &roles)
{
    const auto start = text.line_start(line.number).value;
    const auto bytes = color_span_bytes(line.text, line.colors);
    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        if (excluded_role(line.colors.at(index).role))
        {
            ranges.push_back(core::OffsetRange{core::Offset{start + bytes.at(index).begin.value},
                                               core::Offset{start + bytes.at(index).end.value}});
            roles.push_back(line.colors.at(index).role);
        }
    }
}

// 見えている行の string / comment の区間（行の順・区間の順なので昇順で重ならない）。
void excluded_ranges(const core::TextBuffer &text, const std::vector<LineView> &lines,
                     Ranges &ranges, Roles &roles)
{
    for (const auto &line : lines)
    {
        add_excluded(text, line, ranges, roles);
    }
}

// 前の区間が行の終わりまでで、後ろの区間が次の行の頭からか（行をまたぐ 1 つの区間）。
[[nodiscard]] bool joined(const core::TextBuffer &text, const core::OffsetRange &before,
                          const core::OffsetRange &after)
{
    const auto line = text.position_of(before.begin).line;
    return line.value < text.line_count() && before.end == text.line_end(line) &&
           after.begin == text.line_start(core::LineNumber{line.value + 1});
}

// 区間 index と同じ役割で、行の終わりと次の行の頭でつながる区間をまとめた範囲。
[[nodiscard]] core::OffsetRange joined_region(const core::TextBuffer &text, const Ranges &ranges,
                                              const Roles &roles, std::size_t index)
{
    std::size_t first = index;
    while (first > 0 && roles.at(first - 1) == roles.at(index) &&
           joined(text, ranges.at(first - 1), ranges.at(first)))
    {
        --first;
    }
    std::size_t last = index;
    while (last + 1 < ranges.size() && roles.at(last + 1) == roles.at(index) &&
           joined(text, ranges.at(last), ranges.at(last + 1)))
    {
        ++last;
    }
    return core::OffsetRange{ranges.at(first).begin, ranges.at(last).end};
}

// 区間の外の括弧は区間の中を数えず、区間の中の括弧は同じ役割のつながった区間の中だけで探す。
[[nodiscard]] std::optional<core::Offset> partner_of(const core::TextBuffer &text,
                                                     const std::vector<LineView> &lines,
                                                     core::Offset bracket, core::OffsetRange window)
{
    Ranges ranges;
    Roles roles;
    excluded_ranges(text, lines, ranges, roles);
    const auto inside = std::ranges::find_if(
        ranges, [bracket](const core::OffsetRange &range)
        { return range.begin.value <= bracket.value && bracket.value < range.end.value; });
    if (inside == ranges.end())
    {
        return core::matched_bracket(text, bracket, window, ranges);
    }
    const auto region = joined_region(
        text, ranges, roles, static_cast<std::size_t>(std::distance(ranges.begin(), inside)));
    const core::OffsetRange narrowed{core::Offset{std::max(window.begin.value, region.begin.value)},
                                     core::Offset{std::min(window.end.value, region.end.value)}};
    return core::matched_bracket(text, bracket, narrowed, {});
}
} // namespace

void mark_bracket(const EditorState &state, std::size_t screen, std::vector<LineView> &lines)
{
    if (lines.empty() || state.composition().has_value())
    {
        return;
    }
    const core::TextBuffer &text = state.text();
    const auto caret_line = text.position_of(state.selection().caret).line;
    const auto shown = line_of(lines, caret_line);
    if (shown == lines.end() || shown->window.active)
    {
        return;
    }
    const auto bracket = caret_bracket(state, *shown, text.line_start(caret_line));
    if (!bracket.has_value())
    {
        return;
    }
    const auto window = search_window(text, caret_line, lines, std::max<std::size_t>(screen, 1));
    const auto partner = partner_of(text, lines, bracket.value(), window);
    if (!partner.has_value())
    {
        return;
    }
    const auto line = text.position_of(partner.value()).line;
    const auto found = line_of(lines, line);
    if (found == lines.end() || found->window.active)
    {
        return;
    }
    const auto at = partner.value().value - text.line_start(line).value;
    const core::Column column{1 +
                              core::code_point_count(std::string_view(found->text).substr(0, at))};
    found->bracket = core::SelectionSpan{core::SelectionPresence::present, column,
                                         core::Column{column.value + 1}};
}
} // namespace nenenib::application
