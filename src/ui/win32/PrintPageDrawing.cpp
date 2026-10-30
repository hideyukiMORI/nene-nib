#include "PrintPageDrawing.hpp"

#include "BodyColors.hpp"
#include "BodyTextPainter.hpp"
#include "DevicePixels.hpp"
#include "GutterLabels.hpp"
#include "GutterMetrics.hpp"
#include "PrintPages.hpp"
#include "RenderFailure.hpp"
#include "RgbColor.hpp"
#include "TextLayoutRequest.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
namespace
{
using TextLayout = Microsoft::WRL::ComPtr<IDWriteTextLayout>;
using Painter = BodyTextPainter<ID2D1RenderTarget>;
constexpr float full_channel = 255.0F;
// ヘッダの題名とページ番号のあいだ（DIP）。
constexpr std::int32_t header_gap = 16;

[[nodiscard]] D2D1_COLOR_F to_color(core::RgbColor color) noexcept
{
    return D2D1::ColorF(static_cast<float>(color.red) / full_channel,
                        static_cast<float>(color.green) / full_channel,
                        static_cast<float>(color.blue) / full_channel, 1.0F);
}

[[nodiscard]] D2D1_RECT_F to_rect(const core::LayoutRect &area) noexcept
{
    return D2D1::RectF(static_cast<float>(area.left), static_cast<float>(area.top),
                       static_cast<float>(area.right), static_cast<float>(area.bottom));
}

// 本文と同じ書式の問い（幅は本文の欄・行の高さは本文の行・dpi 96）。題名とページ番号も同じ幅で
// 問うので、印刷の文字組みの書式は 1 回の印刷で 1 つのまま（ADR 0114 の鍵）。
[[nodiscard]] application::TextLayoutRequest request_of(const PrintPageInput &input,
                                                        std::string_view text)
{
    return application::TextLayoutRequest{
        text, input.settings,
        core::TextViewGeometry{std::max(core::width_of(input.body.content), 1),
                               input.body.line_height, core::reference_dpi}};
}

[[nodiscard]] std::int32_t layout_width(IDWriteTextLayout &layout)
{
    DWRITE_TEXT_METRICS metrics{};
    if (FAILED(layout.GetMetrics(&metrics)))
    {
        return 0;
    }
    return static_cast<std::int32_t>(std::ceil(metrics.widthIncludingTrailingWhitespace));
}

// 右寄せは字の幅を測って原点を置く（本文の書式は LEADING なので二重に寄らない）。
[[nodiscard]] bool draw_header(Painter &painter, ID2D1SolidColorBrush &brush,
                               const PrintPageInput &input)
{
    const auto label = core::print_page_label(input.number, input.count);
    const auto number = input.layouts.get(request_of(input, label));
    const auto title = input.layouts.get(request_of(input, input.title));
    if (!number || !title)
    {
        return false;
    }
    brush.SetColor(to_color(input.palette.ui.muted));
    const auto &header = input.header;
    const auto right = header.right - layout_width(*number.value().Get());
    painter.draw_body_text(number.value().Get(), {right, header.top, header.right, header.bottom},
                           D2D1_DRAW_TEXT_OPTIONS_CLIP);
    painter.draw_body_text(
        title.value().Get(),
        {header.left, header.top, std::max(right - header_gap, header.left), header.bottom},
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
    return true;
}

// 行番号は行の最初の画面行に右寄せで描く（画面の行番号の欄と同じ書式・余白・ADR 0177 の決定 5）。
[[nodiscard]] bool draw_number(ID2D1DeviceContext &context, ID2D1SolidColorBrush &brush,
                               const PrintPageInput &input, const PrintLine &line)
{
    const auto label = core::gutter_label(line.number, line.number, false);
    const auto wide = core::to_utf16(label.text);
    const auto padding = gutter_padding(input.settings.font_size, core::reference_dpi);
    const auto &gutter = input.body.gutter;
    const auto width = static_cast<float>(std::max(core::width_of(gutter) - padding, 1));
    TextLayout layout;
    if (!wide || FAILED(input.factory.CreateTextLayout(
                     wide.value().c_str(), static_cast<UINT32>(wide.value().size()), input.gutter,
                     width, static_cast<float>(input.body.line_height), &layout)))
    {
        return false;
    }
    const auto top =
        input.body.content.top +
        static_cast<std::int32_t>(line.first_row - input.page.first_row) * input.body.line_height;
    brush.SetColor(to_color(input.palette.ui.gutter));
    context.DrawTextLayout(D2D1::Point2F(static_cast<float>(gutter.left), static_cast<float>(top)),
                           layout.Get(), &brush, D2D1_DRAW_TEXT_OPTIONS_NONE);
    return true;
}

[[nodiscard]] bool draw_text(Painter &painter, ID2D1SolidColorBrush &brush,
                             const PrintPageInput &input, const PrintLine &line)
{
    const auto text = input.layouts.get(request_of(input, line.display));
    if (!text)
    {
        return false;
    }
    const auto &body = input.body;
    const auto shift =
        static_cast<std::int64_t>(line.first_row) - static_cast<std::int64_t>(input.page.first_row);
    const auto top = static_cast<std::int32_t>(body.content.top + shift * body.line_height);
    const core::LayoutRect area{body.content.left, top, body.content.right,
                                top + static_cast<std::int32_t>(line.rows) * body.line_height};
    const auto paint = [&brush](const D2D1_COLOR_F &color) { brush.SetColor(color); };
    painter.draw_colored_body_text(text.value().Get(), area,
                                   BodyColors{line.colors, to_color(input.palette.ui.text)}, paint);
    if (!line.replaced.empty())
    {
        brush.SetColor(to_color(input.palette.ui.muted));
        painter.draw_replaced(text.value().Get(), area, line.replaced);
    }
    return true;
}

// ページと交わる行を描く。ページをまたぐ行は本文の欄の切り抜きで上下を切る（行の layout は UNIFORM
// の行間なので画面行 r は r 行ぶん下）。行番号は行の最初の画面行がこのページにあるときだけ。
[[nodiscard]] bool draw_rows(ID2D1DeviceContext &context, Painter &painter,
                             ID2D1SolidColorBrush &brush, const PrintPageInput &input)
{
    bool drawn = true;
    for (const auto &line : print_page_lines(input.lines, input.page))
    {
        drawn = drawn && draw_text(painter, brush, input, line);
        drawn = drawn && (line.first_row < input.page.first_row || input.gutter == nullptr ||
                          draw_number(context, brush, input, line));
    }
    return drawn;
}
} // namespace

std::span<const PrintLine> print_page_lines(std::span<const PrintLine> lines,
                                            const core::PrintPage &page) noexcept
{
    const auto first = page.first_row;
    const auto last = first + page.row_count;
    const auto begin = std::ranges::partition_point(
        lines, [first](const PrintLine &line) { return line.first_row + line.rows <= first; });
    const auto end = std::ranges::partition_point(std::ranges::subrange(begin, lines.end()),
                                                  [last](const PrintLine &line)
                                                  { return line.first_row < last; });
    return {begin, end};
}

std::expected<void, application::PrintFailure> draw_print_page(ID2D1DeviceContext &context,
                                                               const PrintPageInput &input)
{
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    if (FAILED(context.CreateSolidColorBrush(to_color(input.palette.ui.text), &brush)))
    {
        return std::unexpected(application::PrintFailure::rendering_failed);
    }
    context.Clear(to_color(input.palette.ui.background));
    std::optional<RenderFailure> failure;
    Painter painter(context, brush.Get(), input.layouts, failure);
    const auto &body = input.body;
    const core::LayoutRect rows{body.gutter.left, body.content.top, body.content.right,
                                body.content.top + static_cast<std::int32_t>(input.page.row_count) *
                                                       body.line_height};
    input.layouts.begin_frame();
    const bool header = draw_header(painter, *brush.Get(), input);
    painter.set_rows(rows.top, rows.bottom);
    context.PushAxisAlignedClip(to_rect(rows), D2D1_ANTIALIAS_MODE_ALIASED);
    const bool drawn = draw_rows(context, painter, *brush.Get(), input);
    context.PopAxisAlignedClip();
    input.layouts.end_frame();
    if (!header || !drawn || failure.has_value())
    {
        return std::unexpected(application::PrintFailure::rendering_failed);
    }
    return {};
}
} // namespace nenenib::ui::win32
