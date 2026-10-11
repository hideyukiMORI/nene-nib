#include "VimScreenGeometry.hpp"

#include "VimScreenKind.hpp"
#include "VimScreenWindow.hpp"
#include "VimStep.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace nenenib::core
{
namespace
{
using Navigation = std::expected<VimDisplayNavigation, LineNumber>;

[[nodiscard]] std::size_t height_of(const VimEditorView &view) noexcept
{
    return std::max<std::size_t>(view.viewport.visible_lines, 1);
}

[[nodiscard]] std::expected<std::size_t, LineNumber> rows_of(const VimEditorView &view,
                                                             LineNumber line)
{
    if (line.value < 1 || line.value > view.text.line_count())
    {
        return std::numeric_limits<std::size_t>::max() / 4;
    }
    const auto found =
        std::ranges::lower_bound(view.layout.lines, line.value, {},
                                 [](const VimLineRows &value) { return value.line.value; });
    if (found == view.layout.lines.end() || found->line != line)
    {
        return std::unexpected(line);
    }
    return std::max<std::size_t>(found->rows, 1);
}

[[nodiscard]] std::expected<std::size_t, LineNumber> visible_rows(const VimEditorView &view,
                                                                  LineNumber line)
{
    const auto rows = rows_of(view, line);
    if (!rows)
    {
        return rows;
    }
    const auto skipped = line == view.viewport.first_visible ? view.layout.first_row : 0;
    return rows.value() - std::min(skipped, rows.value() - 1);
}

[[nodiscard]] std::expected<LineNumber, LineNumber>
middle_of(const VimEditorView &view, LineNumber first, std::size_t occupied)
{
    const auto half = (occupied + 1) / 2;
    std::size_t used = 0;
    for (auto line = first; line.value <= view.text.line_count(); ++line.value)
    {
        const auto rows = visible_rows(view, line);
        if (!rows)
        {
            return std::unexpected(rows.error());
        }
        used += rows.value();
        if (used >= half)
        {
            return LineNumber{line.value > first.value && used > height_of(view) ? line.value - 1
                                                                                 : line.value};
        }
    }
    return LineNumber{view.text.line_count()};
}

[[nodiscard]] std::expected<VimScreenWindow, LineNumber> window_of(const VimEditorView &view,
                                                                   LineNumber first)
{
    std::size_t occupied = 0;
    auto below = first;
    auto last = first;
    while (below.value <= view.text.line_count())
    {
        const auto rows = visible_rows(view, below);
        if (!rows)
        {
            return std::unexpected(rows.error());
        }
        if (rows.value() > height_of(view) - occupied)
        {
            occupied = height_of(view);
            break;
        }
        occupied += rows.value();
        last = below;
        ++below.value;
    }
    const auto middle = middle_of(view, first, occupied);
    if (!middle)
    {
        return std::unexpected(middle.error());
    }
    return VimScreenWindow{first, last, below, middle.value(), occupied};
}

[[nodiscard]] Navigation screen_target(const VimState &state, const VimEditorView &view,
                                       VimScreenKind kind)
{
    const auto window = window_of(view, view.viewport.first_visible);
    if (!window)
    {
        return std::unexpected(window.error());
    }
    const auto &area = window.value();
    const auto amount = std::min(vim_search_count(state) - 1, area.last.value - area.first.value);
    auto target = area.middle;
    if (kind == VimScreenKind::top)
    {
        target = LineNumber{area.first.value + amount};
    }
    if (kind == VimScreenKind::bottom)
    {
        target = LineNumber{area.last.value - amount};
    }
    return VimDisplayNavigation{area.first, target};
}

[[nodiscard]] bool at_bottom(const VimEditorView &view, const VimScreenWindow &area) noexcept
{
    return area.below.value > view.text.line_count() || area.first.value == view.text.line_count();
}

// 全体が見える行を単位に半画面分だけ送る。1行が大きくても最初の1行は送る。
[[nodiscard]] Navigation half_down(const VimEditorView &view, std::size_t amount)
{
    auto first = view.viewport.first_visible;
    auto caret = view.text.position_of(view.selection.caret).line;
    bool moved = false;
    while (amount > 0)
    {
        const auto area = window_of(view, first);
        if (!area)
        {
            return std::unexpected(area.error());
        }
        if (at_bottom(view, area.value()))
        {
            caret.value += std::min(amount, view.text.line_count() - caret.value);
            break;
        }
        const auto rows = visible_rows(view, first);
        if (!rows)
        {
            return std::unexpected(rows.error());
        }
        if (rows.value() > amount && moved)
        {
            break;
        }
        ++first.value;
        caret.value += static_cast<std::size_t>(caret.value < view.text.line_count());
        amount -= std::min(amount, rows.value());
        moved = true;
    }
    return VimDisplayNavigation{first, caret};
}

[[nodiscard]] Navigation half_up(const VimEditorView &view, std::size_t amount)
{
    auto first = view.viewport.first_visible;
    auto caret = view.text.position_of(view.selection.caret).line;
    bool moved = false;
    while (amount > 0 && first.value > 1)
    {
        const auto rows = rows_of(view, LineNumber{first.value - 1});
        if (!rows)
        {
            return std::unexpected(rows.error());
        }
        if (rows.value() > amount && moved)
        {
            return VimDisplayNavigation{first, caret};
        }
        --first.value;
        caret.value -= static_cast<std::size_t>(caret.value > 1);
        amount -= std::min(amount, rows.value());
        moved = true;
    }
    caret.value -= std::min(amount, caret.value - 1);
    return VimDisplayNavigation{first, caret};
}

[[nodiscard]] Navigation within_window(const VimEditorView &view, VimDisplayNavigation target)
{
    const auto area = window_of(view, target.first);
    if (!area)
    {
        return std::unexpected(area.error());
    }
    target.caret.value =
        std::clamp(target.caret.value, target.first.value, area.value().last.value);
    return target;
}

[[nodiscard]] Navigation half_target(const VimState &state, const VimEditorView &view,
                                     VimScreenKind kind)
{
    const auto fallback = std::max<std::size_t>(height_of(view) / 2, 1);
    const auto saved = state.scroll_lines.value_or(VimCount{fallback}).value;
    const auto amount = std::min(state.count.value_or(VimCount{saved}).value, height_of(view));
    auto target = kind == VimScreenKind::half_up ? half_up(view, amount) : half_down(view, amount);
    if (!target)
    {
        return target;
    }
    target = within_window(view, target.value());
    if (!target)
    {
        return target;
    }
    auto &result = target.value();
    result.preserve_column = result.caret == view.text.position_of(view.selection.caret).line;
    if (state.count.has_value() &&
        (result.first != view.viewport.first_visible || !result.preserve_column))
    {
        result.scroll_lines = amount;
    }
    return target;
}

[[nodiscard]] LineNumber adjacent_line(LineNumber line, bool forward) noexcept
{
    if (forward)
    {
        return LineNumber{line.value + 1};
    }
    return LineNumber{line.value > 0 ? line.value - 1 : 0};
}

[[nodiscard]] std::expected<LineNumber, LineNumber> overlap(const VimEditorView &view,
                                                            LineNumber start, bool forward)
{
    std::array<std::size_t, 4> sizes{};
    std::array<LineNumber, 4> lines{};
    auto line = start;
    for (std::size_t index = 0; index < sizes.size(); ++index)
    {
        const auto size = rows_of(view, line);
        if (!size)
        {
            return std::unexpected(size.error());
        }
        sizes.at(index) = size.value();
        lines.at(index) = line;
        line = adjacent_line(line, forward);
    }
    const auto minimum = height_of(view) > 2 ? height_of(view) - 2 : 0;
    if (sizes[0] + sizes[1] > minimum || sizes[1] + sizes[2] > minimum)
    {
        return start;
    }
    const bool one =
        sizes[3] + sizes[2] + sizes[1] > minimum || sizes[2] + sizes[1] + sizes[0] > minimum;
    return lines[one ? 1 : 2];
}

[[nodiscard]] Navigation page_down(const VimEditorView &view, LineNumber first)
{
    const auto area = window_of(view, first);
    if (!area)
    {
        return std::unexpected(area.error());
    }
    if (area.value().below.value > view.text.line_count())
    {
        return VimDisplayNavigation{LineNumber{view.text.line_count()},
                                    LineNumber{view.text.line_count()}};
    }
    const auto next = overlap(view, area.value().below, false);
    if (!next)
    {
        return std::unexpected(next.error());
    }
    const auto target =
        LineNumber{std::clamp(next.value().value, first.value + 1, view.text.line_count())};
    return VimDisplayNavigation{target, target};
}

[[nodiscard]] std::expected<LineNumber, LineNumber> page_above(const VimEditorView &view,
                                                               LineNumber bottom)
{
    std::size_t occupied = 0;
    while (occupied <= height_of(view) && bottom.value > 0)
    {
        --bottom.value;
        if (bottom.value == 0)
        {
            return LineNumber{1};
        }
        const auto rows = rows_of(view, bottom);
        if (!rows)
        {
            return std::unexpected(rows.error());
        }
        occupied += rows.value();
    }
    return LineNumber{bottom.value + 2};
}

[[nodiscard]] Navigation page_up(const VimEditorView &view, LineNumber first)
{
    const auto overlap_line = overlap(view, LineNumber{first.value - 1}, true);
    if (!overlap_line)
    {
        return std::unexpected(overlap_line.error());
    }
    const LineNumber caret{std::min(overlap_line.value().value, view.text.line_count())};
    const auto above = page_above(view, caret);
    if (!above)
    {
        return std::unexpected(above.error());
    }
    if (above.value().value >= first.value)
    {
        const auto area = window_of(view, LineNumber{first.value - 1});
        if (!area)
        {
            return std::unexpected(area.error());
        }
        return VimDisplayNavigation{area.value().first, area.value().last};
    }
    return within_window(view, VimDisplayNavigation{above.value(), caret});
}

[[nodiscard]] Navigation page_target(const VimState &state, const VimEditorView &view,
                                     VimScreenKind kind)
{
    VimDisplayNavigation result{view.viewport.first_visible,
                                view.text.position_of(view.selection.caret).line, true};
    const bool down = kind == VimScreenKind::page_down;
    const auto count = std::min(vim_search_count(state), view.text.line_count());
    for (std::size_t index = 0; index < count; ++index)
    {
        if (down ? result.first.value == view.text.line_count() : result.first.value == 1)
        {
            break;
        }
        const auto next = down ? page_down(view, result.first) : page_up(view, result.first);
        if (!next)
        {
            return std::unexpected(next.error());
        }
        result = next.value();
    }
    return result;
}

[[nodiscard]] std::optional<VimScreenKind> screen_kind(VimAction action)
{
    constexpr std::array mappings{std::pair{VimAction::move_screen_top, VimScreenKind::top},
                                  std::pair{VimAction::move_screen_middle, VimScreenKind::middle},
                                  std::pair{VimAction::move_screen_bottom, VimScreenKind::bottom},
                                  std::pair{VimAction::scroll_half_up, VimScreenKind::half_up},
                                  std::pair{VimAction::scroll_half_down, VimScreenKind::half_down},
                                  std::pair{VimAction::scroll_page_up, VimScreenKind::page_up},
                                  std::pair{VimAction::scroll_page_down, VimScreenKind::page_down}};
    const auto found = std::ranges::find(mappings, action, &decltype(mappings)::value_type::first);
    return found == mappings.end() ? std::nullopt : std::optional{found->second};
}

[[nodiscard]] Navigation resolved(const VimState &state, const VimEditorView &view,
                                  VimScreenKind kind)
{
    switch (kind)
    {
    case VimScreenKind::top:
    case VimScreenKind::middle:
    case VimScreenKind::bottom:
        return screen_target(state, view, kind);
    case VimScreenKind::half_up:
    case VimScreenKind::half_down:
        return half_target(state, view, kind);
    case VimScreenKind::page_up:
    case VimScreenKind::page_down:
        return page_target(state, view, kind);
    }
    std::unreachable();
}
} // namespace

std::expected<std::optional<VimDisplayNavigation>, LineNumber>
vim_display_navigation(const VimState &state, const VimEditorView &view, VimAction action)
{
    const auto kind = screen_kind(action);
    if (!view.layout.enabled || !kind.has_value())
    {
        return std::nullopt;
    }
    const auto result = resolved(state, view, kind.value());
    if (!result)
    {
        return std::unexpected(result.error());
    }
    return std::optional{result.value()};
}
} // namespace nenenib::core
