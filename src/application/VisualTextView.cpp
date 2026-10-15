#include "VisualTextView.hpp"

#include "ComposedLine.hpp"
#include "ScrollBounds.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace nenenib::application
{
namespace
{
[[nodiscard]] core::VisualPosition visual_first(const ScrollState &scroll) noexcept
{
    return core::VisualPosition{scroll.first_visible, scroll.first_row};
}

[[nodiscard]] std::size_t magnitude(std::int64_t amount) noexcept
{
    return amount < 0 ? static_cast<std::size_t>(-(amount + 1)) + 1
                      : static_cast<std::size_t>(amount);
}

[[nodiscard]] core::DisplayCaret shown_caret(const LineView &line, core::TextCaret caret,
                                             core::TextPosition composing)
{
    auto column = core::Column{
        core::display_position(
            line.display, caret.position.column.value > 0 ? caret.position.column.value - 1 : 0) +
        1};
    if (line.composed.has_value())
    {
        const auto &inserted = line.composed.value();
        if (caret.position == composing)
        {
            return core::DisplayCaret{inserted.cursor, core::CaretAffinity::leading};
        }
        if (column.value >= inserted.insertion_column.value)
        {
            column.value += inserted.inserted_columns;
        }
    }
    return core::DisplayCaret{column, caret.affinity};
}
// 見えている行数に足す保持の余り（画面の上下に半分かかる行と、キャレットの行）。
constexpr std::size_t shown_margin = 4;
} // namespace

VisualTextView::VisualTextView(const EditorState &state, TextLayoutPort &layouts,
                               core::TextViewGeometry geometry)
    : state_(state), layouts_(layouts), geometry_(geometry),
      caret_(state.text().position_of(state.selection().caret))
{
}

LineView VisualTextView::line(core::LineNumber number) const
{
    number.value = std::clamp<std::size_t>(number.value, 1, state_.text().line_count());
    auto text = state_.text().line_text(number);
    auto display = core::display_line(text);
    LineView result{number, std::move(text), std::move(display), core::no_selection_span(),
                    {},     std::nullopt};
    const auto &composition = state_.composition();
    if (number == caret_.line && composition.has_value() && !state_.command_input().has_value())
    {
        result.composed = composed_line(result.display, caret_.column, composition.value());
    }
    return result;
}

VisualTextView::SharedLine VisualTextView::shown(core::LineNumber number)
{
    number.value = std::clamp<std::size_t>(number.value, 1, state_.text().line_count());
    const auto found = std::ranges::find_if(shown_, [number](const SharedLine &value)
                                            { return value->number == number; });
    if (found != shown_.end())
    {
        return *found;
    }
    // 歩きが上限を超えて戻ってきたら作り直す（正しさは変わらない）。
    const auto limit = std::max<std::size_t>(state_.scroll().visible_lines, 1) + shown_margin;
    if (shown_.size() >= limit)
    {
        shown_.erase(shown_.begin());
    }
    shown_.push_back(std::make_shared<const LineView>(line(number)));
    return shown_.back();
}

TextLayoutRequest VisualTextView::request(const LineView &line) const
{
    return TextLayoutRequest{presented_text(line), state_.settings(), geometry_};
}

std::expected<std::size_t, TextLayoutFailure> VisualTextView::rows(core::LineNumber number)
{
    if (geometry_.width <= 0)
    {
        return 1;
    }
    const auto made = shown(number);
    return layouts_.rows(request(*made));
}

std::expected<core::TextLocation, TextLayoutFailure> VisualTextView::location(core::TextCaret caret)
{
    if (geometry_.width <= 0)
    {
        return core::TextLocation{0, static_cast<float>(caret.position.column.value - 1)};
    }
    const auto made = shown(caret.position.line);
    return layouts_.locate(request(*made), shown_caret(*made, caret, caret_));
}

std::expected<core::VisualPosition, TextLayoutFailure>
VisualTextView::position(core::TextCaret caret)
{
    const auto located = location(caret);
    if (!located)
    {
        return std::unexpected(located.error());
    }
    return core::VisualPosition{caret.position.line, located.value().row};
}

std::expected<core::TextCaret, TextLayoutFailure> VisualTextView::caret_at(core::VisualPosition at,
                                                                           float x)
{
    if (geometry_.width <= 0)
    {
        const core::TextPosition desired{
            at.line, core::Column{static_cast<std::size_t>(std::max(x, 0.0F)) + 1}};
        return core::TextCaret{state_.text().position_of(state_.text().offset_of(desired))};
    }
    const auto made = shown(at.line);
    const auto &shown_line = *made;
    const auto hit = layouts_.hit(request(shown_line), core::TextLocation{at.row, x});
    if (!hit)
    {
        return std::unexpected(hit.error());
    }
    const auto column = shown_line.composed.has_value()
                            ? uncomposed_column(shown_line.composed.value(), hit.value().column)
                            : hit.value().column;
    return core::TextCaret{
        core::TextPosition{
            shown_line.number,
            core::Column{core::source_column(shown_line.display, column.value - 1) + 1}},
        hit.value().affinity};
}

std::expected<core::VisualPosition, TextLayoutFailure> VisualTextView::last()
{
    const core::LineNumber number{state_.text().line_count()};
    const auto count = rows(number);
    if (!count)
    {
        return std::unexpected(count.error());
    }
    return core::VisualPosition{number, count.value() - 1};
}

std::expected<core::VisualPosition, TextLayoutFailure>
VisualTextView::forward(core::VisualPosition from, std::size_t amount)
{
    while (true)
    {
        const auto count = rows(from.line);
        if (!count)
        {
            return std::unexpected(count.error());
        }
        from.row = std::min(from.row, count.value() - 1);
        const auto available = count.value() - 1 - from.row;
        if (amount <= available)
        {
            return core::VisualPosition{from.line, from.row + amount};
        }
        if (from.line.value == state_.text().line_count())
        {
            return core::VisualPosition{from.line, count.value() - 1};
        }
        amount -= available + 1;
        ++from.line.value;
        from.row = 0;
    }
}

std::expected<core::VisualPosition, TextLayoutFailure>
VisualTextView::backward(core::VisualPosition from, std::size_t amount)
{
    while (true)
    {
        const auto count = rows(from.line);
        if (!count)
        {
            return std::unexpected(count.error());
        }
        from.row = std::min(from.row, count.value() - 1);
        if (amount <= from.row)
        {
            return core::VisualPosition{from.line, from.row - amount};
        }
        if (from.line.value == 1)
        {
            return core::VisualPosition{core::LineNumber{1}, 0};
        }
        amount -= from.row + 1;
        --from.line.value;
        from.row = std::numeric_limits<std::size_t>::max();
    }
}

std::expected<core::VisualPosition, TextLayoutFailure>
VisualTextView::shifted(core::VisualPosition from, std::int64_t amount)
{
    from.line.value = std::clamp<std::size_t>(from.line.value, 1, state_.text().line_count());
    const auto steps = magnitude(amount);
    // 制御文字の表示は最大6文字/byte。巨大な回数で全文書を歩かない。
    const auto &composed = state_.composition();
    const auto composition = composed.has_value() ? composed.value().utf8.size() : 0;
    const auto maximum = state_.text().size_bytes() * 6 + state_.text().line_count() + composition;
    if (steps >= maximum)
    {
        return amount < 0
                   ? std::expected<core::VisualPosition, TextLayoutFailure>{core::VisualPosition{
                         core::LineNumber{1}, 0}}
                   : last();
    }
    return amount < 0 ? backward(from, steps) : forward(from, steps);
}

std::expected<core::VisualPosition, TextLayoutFailure>
VisualTextView::constrained(core::VisualPosition first, core::ScrollExtent extent)
{
    const auto normalized = shifted(first, 0);
    if (!normalized)
    {
        return std::unexpected(normalized.error());
    }
    switch (extent)
    {
    case core::ScrollExtent::last_line:
        return normalized;
    case core::ScrollExtent::filled_viewport:
        break;
    }
    // 残る論理行だけで画面が埋まるなら、文書末尾の文字組みは要らない。
    if (state_.text().line_count() - normalized.value().line.value >=
        std::max<std::size_t>(state_.scroll().visible_lines, 1) - 1)
    {
        return normalized;
    }
    const auto end = last();
    if (!end)
    {
        return std::unexpected(end.error());
    }
    const auto highest =
        backward(end.value(), std::max<std::size_t>(state_.scroll().visible_lines, 1) - 1);
    if (!highest)
    {
        return std::unexpected(highest.error());
    }
    return highest.value() < normalized.value() ? highest : normalized;
}

std::expected<std::size_t, TextLayoutFailure>
VisualTextView::distance(core::VisualPosition first, core::VisualPosition last, std::size_t limit)
{
    if (last.line.value - first.line.value >= limit)
    {
        return limit;
    }
    std::size_t result = 0;
    while (first.line < last.line)
    {
        const auto count = rows(first.line);
        if (!count)
        {
            return std::unexpected(count.error());
        }
        const auto step = count.value() - std::min(first.row, count.value() - 1);
        if (step >= limit - result)
        {
            return limit;
        }
        result += step;
        ++first.line.value;
        first.row = 0;
    }
    return result + std::min(last.row - first.row, limit - result);
}

std::expected<core::VisualPosition, TextLayoutFailure>
VisualTextView::followed_first(core::VisualPosition caret, core::ScrollFollow follow)
{
    const auto normalized = shifted(visual_first(state_.scroll()), 0);
    if (!normalized)
    {
        return std::unexpected(normalized.error());
    }
    const auto first = normalized.value();
    const auto height = std::max<std::size_t>(state_.scroll().visible_lines, 1);
    const auto limit = height * 2 + 1;
    const bool above = caret < first;
    const auto delta = above ? distance(caret, first, limit) : distance(first, caret, limit);
    if (!delta)
    {
        return std::unexpected(delta.error());
    }
    const core::LineNumber virtual_first{limit + 1};
    const core::LineNumber virtual_caret{above ? virtual_first.value - delta.value()
                                               : virtual_first.value + delta.value()};
    const auto target = core::first_visible_for_caret(virtual_first, virtual_caret, height, follow);
    if (target == virtual_first)
    {
        return first;
    }
    return backward(caret, virtual_caret.value - target.value);
}

std::expected<ScrollState, TextLayoutFailure> VisualTextView::followed(core::TextCaret caret,
                                                                       core::ScrollFollow follow,
                                                                       core::ScrollExtent extent)
{
    const auto point = location(caret);
    if (!point)
    {
        return std::unexpected(point.error());
    }
    const auto requested =
        followed_first(core::VisualPosition{caret.position.line, point.value().row}, follow);
    const auto filled =
        constrained(visual_first(state_.scroll()), core::ScrollExtent::filled_viewport);
    if (!requested || !filled)
    {
        return std::unexpected(!requested ? requested.error() : filled.error());
    }
    const auto selected = filled.value() < visual_first(state_.scroll())
                              ? extent
                              : core::ScrollExtent::filled_viewport;
    const auto first = constrained(requested.value(), selected);
    if (!first)
    {
        return std::unexpected(first.error());
    }
    auto result = state_.scroll();
    result.first_visible = first.value().line;
    result.first_row = first.value().row;
    result.horizontal = 0;
    if (state_.settings().wrapping == core::LineWrapping::no_wrap && geometry_.width > 0)
    {
        const auto width = static_cast<float>(std::max(geometry_.width - 3, 1));
        const auto x = std::max(point.value().x, 0.0F);
        result.horizontal = std::clamp(state_.scroll().horizontal, std::max(x - width, 0.0F), x);
    }
    return result;
}
} // namespace nenenib::application
