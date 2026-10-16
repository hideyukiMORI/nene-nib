#include "VisualTextView.hpp"

#include "ComposedLine.hpp"
#include "DevicePixels.hpp"
#include "FontSize.hpp"
#include "GiantLine.hpp"
#include "ScrollBounds.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cstdint>
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
    // 巨大な行の窓では、本文の桁を窓の中の桁に写す（ADR 0139・試作）。
    const auto source = caret.position.column.value > 0 ? caret.position.column.value - 1 : 0;
    auto column = core::Column{
        core::display_position(line.display, source - std::min(source, line.window.column)) + 1};
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

// ADR 0139（試作）: 錨の桁をこの刻みに丸めた中心から窓を切る。錨は常に窓の端から刻み以上
// 内側にあり、刻みをまたぐまで窓の文字列は変わらない（決定 6 を状態なしで近似する）。
constexpr std::size_t giant_line_step = core::giant_line_half_width / 2;
// 平均の字幅の見積もり（em 比・Consolas の送り幅）。application は字幅を知らない（試作の穴）。
constexpr float average_advance_em = 0.55F;
constexpr std::size_t utf8_longest = 4;
constexpr std::size_t slice_margin = 64;

[[nodiscard]] float advance_of(const core::EditorSettings &settings,
                               core::TextViewGeometry geometry) noexcept
{
    return std::max(core::font_size_dips(settings.font_size) * static_cast<float>(geometry.dpi) /
                        static_cast<float>(core::reference_dpi) * average_advance_em,
                    1.0F);
}

[[nodiscard]] std::size_t walked_back(std::string_view text, std::size_t at, std::size_t steps)
{
    for (std::size_t step = 0; step < steps && at > 0; ++step)
    {
        at = core::previous_code_point(text, core::Offset{at}).value;
    }
    return at;
}

[[nodiscard]] float whole_pixels(float value) noexcept
{
    return static_cast<float>(static_cast<std::int64_t>(value));
}
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
    if (giant(number))
    {
        return giant_line(number);
    }
    auto text = state_.text().line_text(number);
    auto display = core::display_line(text);
    LineView result{number, std::move(text), std::move(display), core::no_selection_span(),
                    {},     std::nullopt};
    compose(result);
    return result;
}

void VisualTextView::compose(LineView &result) const
{
    const auto &composition = state_.composition();
    if (result.number == caret_.line && composition.has_value() &&
        !state_.command_input().has_value())
    {
        const auto shift = std::min(result.window.column, caret_.column.value - 1);
        result.composed = composed_line(result.display, core::Column{caret_.column.value - shift},
                                        composition.value());
    }
}

bool VisualTextView::giant(core::LineNumber number) const noexcept
{
    const auto &text = state_.text();
    return text.line_end(number).value - text.line_start(number).value > core::giant_line_threshold;
}

core::Offset VisualTextView::giant_anchor(core::LineNumber number, float advance) const
{
    if (caret_.line == number)
    {
        return state_.selection().caret;
    }
    const auto column =
        static_cast<std::size_t>(std::max(state_.scroll().horizontal, 0.0F) / advance);
    return state_.text().offset_of(core::TextPosition{number, core::Column{column + 1}});
}

// 巨大な行で窓の外の x を指されたら、見積もった字幅で桁を決める（Home / End・試作）。
std::optional<core::TextCaret> VisualTextView::outside_window(const LineView &line, float x) const
{
    if (!line.window.active)
    {
        return std::nullopt;
    }
    const auto advance = advance_of(state_.settings(), geometry_);
    // End は x に float の最大を渡す。桁は本文のバイト数で頭打ちにする（offset_of
    // が行末に寄せる）。
    const auto estimate =
        std::min(static_cast<double>(std::max(x, 0.0F)) / static_cast<double>(advance),
                 static_cast<double>(state_.text().size_bytes()));
    const auto first = static_cast<double>(line.window.column);
    const auto last = first + static_cast<double>(core::code_point_count(line.text));
    if (estimate >= first && estimate <= last)
    {
        return std::nullopt;
    }
    const auto column = estimate < first ? std::size_t{0} : static_cast<std::size_t>(estimate);
    const auto offset =
        state_.text().offset_of(core::TextPosition{line.number, core::Column{column + 1}});
    return core::TextCaret{state_.text().position_of(offset)};
}

LineView VisualTextView::giant_line(core::LineNumber number) const
{
    const auto &text = state_.text();
    const auto start = text.line_start(number).value;
    const auto end = text.line_end(number).value;
    const auto advance = advance_of(state_.settings(), geometry_);
    const auto anchor = giant_anchor(number, advance).value;
    const auto column = caret_.line == number
                            ? caret_.column.value - 1
                            : text.position_of(core::Offset{anchor}).column.value - 1;
    const auto center_column = column / giant_line_step * giant_line_step;
    const auto reach =
        (core::giant_line_half_width + giant_line_step) * utf8_longest + slice_margin;
    const auto first = anchor > start + reach ? anchor - reach : start;
    const auto slice =
        text.text_range(core::Offset{first}, core::Offset{std::min(anchor + reach, end)});
    const auto center = walked_back(slice, anchor - first, column - center_column);
    const auto range =
        core::giant_line_window(slice, core::Offset{center}, core::giant_line_half_width);
    const std::string_view whole(slice);
    const auto before =
        core::code_point_count(whole.substr(range.begin.value, center - range.begin.value));
    LineWindow window{true, first + range.begin.value - start, center_column - before, 0.0F};
    window.x = whole_pixels(static_cast<float>(window.column) * advance);
    std::string shown(whole.substr(range.begin.value, range.end.value - range.begin.value));
    auto display = core::display_line(shown);
    LineView result{number, std::move(shown), std::move(display), core::no_selection_span(),
                    {},     std::nullopt};
    result.window = window;
    compose(result);
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
    return TextLayoutRequest{presented_text(line), state_.settings(), geometry_, line.window};
}

std::expected<std::size_t, TextLayoutFailure> VisualTextView::rows(core::LineNumber number)
{
    if (geometry_.width <= 0 || giant(number))
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
    auto located = layouts_.locate(request(*made), shown_caret(*made, caret, caret_));
    if (located)
    {
        located.value().x += made->window.x;
    }
    return located;
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
    if (const auto outside = outside_window(shown_line, x); outside.has_value())
    {
        return outside.value();
    }
    const auto hit =
        layouts_.hit(request(shown_line), core::TextLocation{at.row, x - shown_line.window.x});
    if (!hit)
    {
        return std::unexpected(hit.error());
    }
    const auto column = shown_line.composed.has_value()
                            ? uncomposed_column(shown_line.composed.value(), hit.value().column)
                            : hit.value().column;
    return core::TextCaret{
        core::TextPosition{shown_line.number,
                           core::Column{core::source_column(shown_line.display, column.value - 1) +
                                        1 + shown_line.window.column}},
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
    // 巨大な行は折り返しの設定にかかわらず横に追従する（ADR 0139 の決定 2・試作）。
    if ((state_.settings().wrapping == core::LineWrapping::no_wrap || giant(caret.position.line)) &&
        geometry_.width > 0)
    {
        const auto width = static_cast<float>(std::max(geometry_.width - 3, 1));
        const auto x = std::max(point.value().x, 0.0F);
        result.horizontal = std::clamp(state_.scroll().horizontal, std::max(x - width, 0.0F), x);
    }
    return result;
}
} // namespace nenenib::application
