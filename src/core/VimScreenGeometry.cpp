#include "VimScreenGeometry.hpp"

#include "VimActionGroup.hpp"
#include "VimKeyTable.hpp"
#include "VimScreenKind.hpp"
#include "VimScreenRows.hpp"
#include "VimScreenWindow.hpp"
#include "VimScrollDirection.hpp"
#include "VimScrollUnit.hpp"
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
    return std::max<std::size_t>(vim_viewport_of(view).visible_lines, 1);
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
    const auto skipped =
        line == vim_viewport_of(view).first_visible ? vim_viewport_of(view).first_row : 0;
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
    const auto window = window_of(view, vim_viewport_of(view).first_visible);
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
    auto first = vim_viewport_of(view).first_visible;
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
    auto first = vim_viewport_of(view).first_visible;
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

// 半画面の量（回数・'scroll'・既定は半分）。画面の高さを越えない（ADR 0019 の決定 6）。
[[nodiscard]] std::size_t half_amount(const VimState &state, const VimEditorView &view) noexcept
{
    const auto fallback = std::max<std::size_t>(height_of(view) / 2, 1);
    const auto saved = state.scroll_lines.value_or(VimCount{fallback}).value;
    return std::min(state.count.value_or(VimCount{saved}).value, height_of(view));
}

[[nodiscard]] Navigation half_target(const VimState &state, const VimEditorView &view,
                                     VimScreenKind kind)
{
    const auto amount = half_amount(state, view);
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
        (result.first != vim_viewport_of(view).first_visible || !result.preserve_column))
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
    VimDisplayNavigation result{vim_viewport_of(view).first_visible,
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

// 画面行の単位の半画面と 1 画面（ADR 0144 の決定 5 とその追補）。1 画面は visible_lines
// 行で、回数は 画面の数。半画面は画面が送れない端でもキャレットが動く（Vim の CTRL-D）。
// 回数のある半画面は、動いたときだけ 'scroll' を置き換える（論理行の道と同じ）。
[[nodiscard]] std::expected<VimDisplayNavigation, VimLayoutNeed>
row_target(const VimState &state, const VimEditorView &view, VimScrollDirection direction,
           bool half)
{
    const auto pages = vim_search_count(state);
    const auto height = height_of(view);
    const auto whole = pages > std::numeric_limits<std::size_t>::max() / height
                           ? std::numeric_limits<std::size_t>::max()
                           : pages * height;
    const auto amount = half ? half_amount(state, view) : whole;
    auto target = half ? vim_row_half_navigation(state, view, direction, amount)
                       : vim_row_navigation(state, view, direction, amount);
    if (!target)
    {
        return target;
    }
    auto &result = target.value();
    const bool moved = result.first != vim_viewport_of(view).first_visible ||
                       result.first_row != vim_viewport_of(view).first_row ||
                       result.placed != std::optional{view.selection.caret};
    if (half && state.count.has_value() && moved)
    {
        result.scroll_lines = amount;
    }
    return target;
}

// H M L は格子でも論理行のまま（ADR 0144 の限界）。
[[nodiscard]] std::expected<VimDisplayNavigation, VimLayoutNeed>
lined(const VimState &state, const VimEditorView &view, VimScreenKind kind)
{
    const auto result = resolved(state, view, kind);
    if (!result)
    {
        return std::unexpected(VimLayoutNeed{result.error()});
    }
    return result.value();
}

[[nodiscard]] std::expected<VimDisplayNavigation, VimLayoutNeed>
rows_resolved(const VimState &state, const VimEditorView &view, VimScreenKind kind)
{
    switch (kind)
    {
    case VimScreenKind::top:
    case VimScreenKind::middle:
    case VimScreenKind::bottom:
        return lined(state, view, kind);
    case VimScreenKind::half_up:
        return row_target(state, view, VimScrollDirection::up, true);
    case VimScreenKind::half_down:
        return row_target(state, view, VimScrollDirection::down, true);
    case VimScreenKind::page_up:
        return row_target(state, view, VimScrollDirection::up, false);
    case VimScreenKind::page_down:
        return row_target(state, view, VimScrollDirection::down, false);
    }
    std::unreachable();
}

[[nodiscard]] std::expected<VimDisplayNavigation, VimLayoutNeed>
unit_resolved(const VimState &state, const VimEditorView &view, VimScreenKind kind)
{
    switch (view.layout.unit)
    {
    case VimScrollUnit::line:
        return lined(state, view, kind);
    case VimScrollUnit::row:
        return rows_resolved(state, view, kind);
    }
    std::unreachable();
}
// キャレットの行（格子では画面行）の上に置く画面行の数（ADR 0146 の決定 5）。
[[nodiscard]] std::size_t rows_above(VimScreenPlace place, std::size_t height) noexcept
{
    switch (place)
    {
    case VimScreenPlace::top:
        return 0;
    case VimScreenPlace::center:
        return (height - 1) / 2;
    case VimScreenPlace::bottom:
        return height - 1;
    }
    std::unreachable();
}

// 交互の足し方で足す行の画面行数。文書の末尾を越えた行は `~` の 1 画面行。
[[nodiscard]] std::expected<std::size_t, LineNumber> filler_rows(const VimEditorView &view,
                                                                 LineNumber line)
{
    if (line.value > view.text.line_count())
    {
        return 1;
    }
    return rows_of(view, line);
}

// 下端に置く（Vim の scroll_cursor_bot）。キャレットの行から上へ、画面に入る間だけ行を足す。
[[nodiscard]] std::expected<LineNumber, LineNumber> bottom_top(const VimEditorView &view,
                                                               LineNumber line, std::size_t used)
{
    const std::size_t height = height_of(view);
    LineNumber top = line;
    while (top.value > 1)
    {
        const auto rows = rows_of(view, LineNumber{top.value - 1});
        if (!rows)
        {
            return std::unexpected(rows.error());
        }
        if (rows.value() > height - used)
        {
            break;
        }
        used += rows.value();
        --top.value;
    }
    return top;
}

// 中央に置く（Vim の scroll_cursor_halfway）。下に足した画面行が上以下なら下の行を、上より多ければ
// 上の行を 1 つずつ足し、画面を越える行か文書の先頭で止める。added の 0 が上・1 が下の画面行数。
[[nodiscard]] std::expected<LineNumber, LineNumber> halfway_top(const VimEditorView &view,
                                                                LineNumber line, std::size_t used)
{
    const std::size_t height = height_of(view);
    LineNumber top = line;
    LineNumber next{line.value + 1};
    std::array<std::size_t, 2> added{};
    while (top.value > 1)
    {
        const bool down = added[1] <= added[0];
        const LineNumber adding = down ? next : LineNumber{top.value - 1};
        const auto rows = filler_rows(view, adding);
        if (!rows)
        {
            return std::unexpected(rows.error());
        }
        if (rows.value() > height - used)
        {
            break;
        }
        used += rows.value();
        added.at(static_cast<std::size_t>(down)) += rows.value();
        next.value += static_cast<std::size_t>(down);
        top = down ? top : adding;
    }
    return top;
}

// wrap on の text 形（論理行の単位・first_row は 0）。キャレットの行が画面より高ければ何も
// 変えない。
[[nodiscard]] std::expected<std::optional<VisualPosition>, VimLayoutNeed>
lined_top(const VimEditorView &view, LineNumber line, VimScreenPlace place)
{
    const auto rows = rows_of(view, line);
    if (!rows)
    {
        return std::unexpected(VimLayoutNeed{rows.error()});
    }
    if (rows.value() > height_of(view))
    {
        return std::nullopt;
    }
    auto top = std::expected<LineNumber, LineNumber>{line};
    switch (place)
    {
    case VimScreenPlace::top:
        break;
    case VimScreenPlace::center:
        top = halfway_top(view, line, rows.value());
        break;
    case VimScreenPlace::bottom:
        top = bottom_top(view, line, rows.value());
        break;
    }
    if (!top)
    {
        return std::unexpected(VimLayoutNeed{top.error()});
    }
    return std::optional{VisualPosition{top.value(), 0}};
}

// 横送りの鍵（ADR 0145 の決定 2）。画面の幅と今の横位置を表示領域から読む。
[[nodiscard]] bool scrolls_horizontally(VimAction action) noexcept
{
    return vim_group_for(action) == VimActionGroup::horizontal_scroll;
}
} // namespace

bool vim_reads_viewport(VimAction action) noexcept
{
    return screen_kind(action).has_value() || scrolls_horizontally(action) ||
           vim_group_for(action) == VimActionGroup::screen_alignment;
}

std::expected<std::optional<VimDisplayNavigation>, VimLayoutNeed>
vim_display_navigation(const VimState &state, const VimEditorView &view, VimAction action)
{
    const auto kind = screen_kind(action);
    if (!view.layout.enabled || !kind.has_value())
    {
        return std::nullopt;
    }
    const auto result = unit_resolved(state, view, kind.value());
    if (!result)
    {
        return std::unexpected(result.error());
    }
    return std::optional{result.value()};
}

std::expected<std::optional<VisualPosition>, VimLayoutNeed>
vim_aligned_top(const VimEditorView &view, Offset caret, VimScreenPlace place)
{
    const LineNumber line = view.text.position_of(caret).line;
    const std::size_t above = rows_above(place, height_of(view));
    if (!view.layout.enabled)
    {
        return std::optional{
            VisualPosition{LineNumber{line.value - std::min(line.value - 1, above)}, 0}};
    }
    switch (view.layout.unit)
    {
    case VimScrollUnit::line:
        return lined_top(view, line, place);
    case VimScrollUnit::row:
        break;
    }
    const auto top = vim_row_aligned_top(view, caret, above);
    if (!top)
    {
        return std::unexpected(top.error());
    }
    return std::optional{top.value()};
}
} // namespace nenenib::core
