// インデントガイドの純関数（ADR 0193 の決定 5・7・9・#1975）。
#include "IndentGuide.hpp"

#include "IndentGuidePaint.hpp"
#include "IndentGuideRun.hpp"
#include "IndentGuideStyle.hpp"
#include "IndentUnit.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "Palette.hpp"
#include "RainbowColors.hpp"
#include "RgbColor.hpp"
#include "SyntaxPalette.hpp"
#include "TextBuffer.hpp"
#include "ThemeDerivation.hpp"
#include "Utf8.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::core
{
namespace
{
// 字下げの空白を数える窓の初めの大きさ（行の全体は写さない・長ければ倍に広げる）。
constexpr std::size_t indent_window_bytes = 64;

[[nodiscard]] constexpr bool indent_blank(char unit) noexcept
{
    return unit == ' ' || unit == '\t';
}

// ---------------------------------------------------------------- 推し当て（決定 7）

// 票の数。添字 0〜2 は差 2・4・8（voted_steps の順）、3 は tab。
using IndentVotes = std::array<std::size_t, 4>;
constexpr std::size_t tab_votes = 3;

constexpr std::array<std::size_t, 3> voted_steps{2, 4, 8};
constexpr std::array<IndentUnit, 3> voted_units{IndentUnit::two, IndentUnit::four,
                                                IndentUnit::eight};
constexpr std::size_t least_votes = 3;

// 隣り合う空でない行の 1 組の票（previous と current は字下げの桁と先頭のバイト）。
void vote_pair(IndentVotes &votes, std::pair<std::size_t, char> previous,
               std::pair<std::size_t, char> current) noexcept
{
    if (previous.second == '\t' || current.second == '\t' ||
        (previous.second != ' ' && current.second != ' '))
    {
        return;
    }
    const std::size_t step = previous.first > current.first ? previous.first - current.first
                                                            : current.first - previous.first;
    for (std::size_t index = 0; index < voted_steps.size(); ++index)
    {
        votes.at(index) += voted_steps.at(index) == step ? 1U : 0U;
    }
}

// head の先頭の 1 行を外して返す（行の終わりの LF は含まない）。
[[nodiscard]] std::string_view taken_line(std::string_view &head) noexcept
{
    const std::size_t newline = head.find('\n');
    const std::string_view line = head.substr(0, newline);
    head = newline == std::string_view::npos ? std::string_view{} : head.substr(newline + 1);
    return line;
}

[[nodiscard]] IndentVotes counted_votes(std::string_view head) noexcept
{
    IndentVotes votes{};
    std::optional<std::pair<std::size_t, char>> previous;
    for (std::size_t lines = 0; !head.empty() && lines < indent_guess_lines; ++lines)
    {
        const std::string_view line = taken_line(head);
        if (blank_indent_line(line))
        {
            continue;
        }
        const std::pair<std::size_t, char> current{indent_cells(line), line.front()};
        votes.at(tab_votes) += current.second == '\t' ? 1U : 0U;
        if (previous.has_value())
        {
            vote_pair(votes, previous.value(), current);
        }
        previous = current;
    }
    return votes;
}

// ---------------------------------------------------------------- 虹の色（決定 5）

// sRGB の 1 成分（0〜1）を線形へ。x^2.4 は x^2 × x^0.4 で、x^0.4 は x^2 の 5 乗根（libm を
// core の外へ出さない・ARC-003）。根は上から Newton で近づける（凸なので単調に収束する）。
[[nodiscard]] constexpr double root_of(double value, int degree) noexcept
{
    if (value <= 0.0)
    {
        return 0.0;
    }
    constexpr int iterations = 64;
    double guess = std::max(value, 1.0);
    for (int step = 0; step < iterations; ++step)
    {
        double power = 1.0;
        for (int factor = 1; factor < degree; ++factor)
        {
            power *= guess;
        }
        guess = ((static_cast<double>(degree - 1) * guess) + (value / power)) /
                static_cast<double>(degree);
    }
    return guess;
}

[[nodiscard]] constexpr double linear_of(std::uint8_t channel) noexcept
{
    constexpr double full = 255.0;
    constexpr double knee = 0.04045;
    constexpr double slope = 12.92;
    constexpr double offset = 0.055;
    constexpr double scale = 1.055;
    constexpr int fifth = 5;
    const double value = static_cast<double>(channel) / full;
    if (value <= knee)
    {
        return value / slope;
    }
    const double base = (value + offset) / scale;
    return base * base * root_of(base * base, fifth);
}

// Lab の f(t)（CIE 1976）。
[[nodiscard]] constexpr double lab_f(double value) noexcept
{
    constexpr double epsilon = 216.0 / 24389.0;
    constexpr double kappa = 24389.0 / 27.0;
    constexpr double sixteen = 16.0;
    constexpr double hundred_sixteen = 116.0;
    constexpr int third = 3;
    return value > epsilon ? root_of(value, third) : ((kappa * value) + sixteen) / hundred_sixteen;
}

using Lab = std::array<double, 3>;

// D65 の白で正規化した XYZ から Lab へ（sRGB の行列）。
[[nodiscard]] constexpr Lab lab_of(RgbColor color) noexcept
{
    const double red = linear_of(color.red);
    const double green = linear_of(color.green);
    const double blue = linear_of(color.blue);
    const double x = ((0.4124 * red) + (0.3576 * green) + (0.1805 * blue)) / 0.95047;
    const double y = (0.2126 * red) + (0.7152 * green) + (0.0722 * blue);
    const double z = ((0.0193 * red) + (0.1192 * green) + (0.9505 * blue)) / 1.08883;
    const double fx = lab_f(x);
    const double fy = lab_f(y);
    const double fz = lab_f(z);
    return Lab{(116.0 * fy) - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz)};
}

// ΔE76 の 2 乗（根を取らずに閾値の 2 乗と比べる）。
[[nodiscard]] constexpr double distance_squared(const Lab &left, const Lab &right) noexcept
{
    double sum = 0.0;
    for (std::size_t axis = 0; axis < left.size(); ++axis)
    {
        const double delta = left.at(axis) - right.at(axis);
        sum += delta * delta;
    }
    return sum;
}

constexpr double apart_from_each_other = 20.0 * 20.0;
constexpr double apart_from_text = 15.0 * 15.0;
constexpr std::size_t rainbow_limit = 6;

// 候補の役割の順（決定 5）。
constexpr std::array<RgbColor SyntaxPalette::*, 9> rainbow_roles{
    &SyntaxPalette::keyword,  &SyntaxPalette::function, &SyntaxPalette::string,
    &SyntaxPalette::type,     &SyntaxPalette::number,   &SyntaxPalette::operators,
    &SyntaxPalette::constant, &SyntaxPalette::warning,  &SyntaxPalette::error};

// 候補 lab が取った色のどれとも離れているか。
[[nodiscard]] bool apart_from_picked(const Lab &lab, std::span<const Lab> picked) noexcept
{
    return std::ranges::all_of(picked, [&lab](const Lab &other)
                               { return distance_squared(lab, other) >= apart_from_each_other; });
}
} // namespace

std::size_t indent_cells(std::string_view line) noexcept
{
    std::size_t cells = 0;
    for (const char unit : line)
    {
        if (!indent_blank(unit))
        {
            break;
        }
        cells = unit == '\t' ? cells + vim_tab_stop - (cells % vim_tab_stop) : cells + 1;
    }
    return cells;
}

std::size_t indent_bytes(std::string_view line) noexcept
{
    const auto end = std::ranges::find_if_not(line, indent_blank);
    return static_cast<std::size_t>(end - line.begin());
}

bool blank_indent_line(std::string_view line) noexcept
{
    return std::ranges::all_of(line, [](char unit) { return indent_blank(unit) || unit == '\r'; });
}

std::optional<std::size_t> line_indent_cells(const TextBuffer &text, LineNumber line)
{
    const Offset start = text.line_start(line);
    const Offset end = text.line_end(line);
    std::size_t window = indent_window_bytes;
    while (true)
    {
        const std::size_t reach = std::min(window, end.value - start.value);
        const std::string piece = text.text_range(start, Offset{start.value + reach});
        const std::size_t blanks = indent_bytes(piece);
        if (blanks < piece.size())
        {
            const bool carriage = piece.at(blanks) == '\r' && blanks + 1 == piece.size() &&
                                  start.value + reach == end.value;
            return carriage ? std::nullopt : std::optional{indent_cells(piece)};
        }
        if (start.value + reach == end.value)
        {
            return std::nullopt;
        }
        window *= 2;
    }
}

std::optional<IndentUnit> guessed_indent_unit(std::string_view head) noexcept
{
    const IndentVotes votes = counted_votes(head);
    std::size_t spaces = 0;
    std::size_t best = 0;
    for (std::size_t index = 0; index < voted_steps.size(); ++index)
    {
        spaces += votes.at(index);
        best = votes.at(index) > votes.at(best) ? index : best;
    }
    const std::size_t tabs = votes.at(tab_votes);
    if (spaces + tabs < least_votes)
    {
        return std::nullopt;
    }
    if (tabs > spaces)
    {
        return IndentUnit::tab;
    }
    return voted_units.at(best);
}

std::optional<IndentUnit> guessed_indent_unit(const TextBuffer &text)
{
    const std::size_t reach = std::min(text.size_bytes(), indent_guess_bytes);
    const std::string head = text.text_range(Offset{0}, Offset{reach});
    return guessed_indent_unit(std::string_view(head).substr(0, whole_code_points(head)));
}

std::size_t indent_unit_cells(IndentUnit unit) noexcept
{
    switch (unit)
    {
    case IndentUnit::two:
        return 2;
    case IndentUnit::four:
        return 4;
    case IndentUnit::eight:
        return 8;
    case IndentUnit::tab:
        return vim_tab_stop;
    }
    std::unreachable();
}

std::optional<std::uint8_t> indent_guide_width_of(std::string_view text) noexcept
{
    if (text == "2" || text == "4" || text == "8")
    {
        return static_cast<std::uint8_t>(text.front() - '0');
    }
    return std::nullopt;
}

std::uint8_t indent_guide_width_stepped(std::uint8_t width, bool forward) noexcept
{
    constexpr std::uint8_t narrowest = 2;
    constexpr std::uint8_t widest = 8;
    if (forward)
    {
        return width >= widest ? widest : static_cast<std::uint8_t>(width * 2);
    }
    return width <= narrowest ? narrowest : static_cast<std::uint8_t>(width / 2);
}

std::size_t indent_guide_step(std::optional<IndentUnit> guessed, std::uint8_t width) noexcept
{
    return guessed.has_value() ? indent_unit_cells(guessed.value()) : std::size_t{width};
}

std::optional<std::uint8_t> indent_guide_colors_of(std::string_view text) noexcept
{
    if (text.size() != 1 || text.front() < '0' + indent_guide_colors_minimum ||
        text.front() > '0' + indent_guide_colors_maximum)
    {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>(text.front() - '0');
}

std::uint8_t indent_guide_colors_stepped(std::uint8_t count, bool forward) noexcept
{
    if (forward)
    {
        return count >= indent_guide_colors_maximum ? indent_guide_colors_maximum
                                                    : static_cast<std::uint8_t>(count + 1);
    }
    return count <= indent_guide_colors_minimum ? indent_guide_colors_minimum
                                                : static_cast<std::uint8_t>(count - 1);
}

std::string_view indent_guide_style_name(IndentGuideStyle style) noexcept
{
    switch (style)
    {
    case IndentGuideStyle::rainbow:
        return "rainbow";
    case IndentGuideStyle::plain:
        return "plain";
    }
    std::unreachable();
}

std::optional<IndentGuideStyle> indent_guide_style_named(std::string_view name) noexcept
{
    for (const IndentGuideStyle style : {IndentGuideStyle::rainbow, IndentGuideStyle::plain})
    {
        if (indent_guide_style_name(style) == name)
        {
            return style;
        }
    }
    return std::nullopt;
}

std::vector<std::size_t> continued_indents(std::span<const std::optional<std::size_t>> cells,
                                           std::optional<std::size_t> before,
                                           std::optional<std::size_t> after)
{
    // 下から上へ「下の最も近い空でない行」を、上から下へ「上の最も近い空でない行」を写す。
    std::vector<std::optional<std::size_t>> below(cells.size());
    std::optional<std::size_t> next = after;
    for (std::size_t index = cells.size(); index-- > 0;)
    {
        next = cells[index].has_value() ? cells[index] : next;
        below[index] = next;
    }
    std::vector<std::size_t> continued(cells.size(), 0);
    std::optional<std::size_t> previous = before;
    for (std::size_t index = 0; index < cells.size(); ++index)
    {
        const std::optional<std::size_t> &shown = cells[index];
        previous = shown.has_value() ? shown : previous;
        continued[index] = shown.value_or(std::max(previous.value_or(0), below[index].value_or(0)));
    }
    return continued;
}

std::optional<IndentGuideRun> active_indent_run(std::span<const std::size_t> levels,
                                                std::size_t caret) noexcept
{
    if (caret >= levels.size() || levels[caret] == 0)
    {
        return std::nullopt;
    }
    const std::size_t level = levels[caret];
    std::size_t begin = caret;
    while (begin > 0 && levels[begin - 1] >= level)
    {
        --begin;
    }
    std::size_t end = caret + 1;
    while (end < levels.size() && levels[end] >= level)
    {
        ++end;
    }
    return IndentGuideRun{begin, end, level - 1};
}

RainbowColors rainbow_colors(const SyntaxPalette &syntax, const Palette & /*palette*/,
                             std::size_t wanted) noexcept
{
    RainbowColors result{{}, 0};
    std::array<Lab, rainbow_limit> picked{};
    const Lab text = lab_of(syntax.foreground);
    for (const auto role : rainbow_roles)
    {
        const RgbColor color = syntax.*role;
        const Lab lab = lab_of(color);
        if (result.count == rainbow_limit || distance_squared(lab, text) < apart_from_text ||
            !apart_from_picked(lab, std::span<const Lab>(picked.data(), result.count)))
        {
            continue;
        }
        picked.at(result.count) = lab;
        result.colors.at(result.count) = color;
        ++result.count;
    }
    result.count = std::min(result.count, wanted);
    return result;
}

IndentGuidePaint indent_guide_paint(IndentGuideStyle style, std::size_t wanted,
                                    const SyntaxPalette &syntax, const Palette &palette) noexcept
{
    return IndentGuidePaint{style, rainbow_colors(syntax, palette, wanted), palette.panel_border,
                            palette.muted, palette.background};
}

RgbColor indent_guide_color(const IndentGuidePaint &paint, std::size_t level, bool strong) noexcept
{
    const bool plain = paint.style == IndentGuideStyle::plain || paint.rainbow.count == 0;
    if (plain)
    {
        return strong ? paint.plain_strong : paint.plain;
    }
    const RgbColor role = paint.rainbow.colors.at(level % paint.rainbow.count);
    return strong ? role : mix(role, paint.background, indent_guide_faded_percent);
}
} // namespace nenenib::core
