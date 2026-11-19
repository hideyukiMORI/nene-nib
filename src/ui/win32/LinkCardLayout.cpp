#include "LinkCardLayout.hpp"

#include "DevicePixels.hpp"
#include "LayoutRect.hpp"
#include "LinkCard.hpp"
#include "Utf16.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <utility>

namespace nenenib::ui::win32
{
namespace
{
using TextLayout = Microsoft::WRL::ComPtr<IDWriteTextLayout>;

// 折り返さない layout の幅と、layout の高さの上限（行の数では切らない）。
constexpr float unbounded = 100'000.0F;

[[nodiscard]] float pixels(std::int32_t dips, std::uint32_t dpi) noexcept
{
    return static_cast<float>(core::to_pixels(dips, dpi));
}

// 幅 width で wrapping の折り返しをする layout（作れなければ null）。
[[nodiscard]] TextLayout made(IDWriteFactory &dwrite, IDWriteTextFormat *format,
                              std::string_view text,
                              std::pair<float, DWRITE_WORD_WRAPPING> wrapping)
{
    const auto wide = core::to_utf16(text);
    TextLayout layout;
    if (format == nullptr || !wide.has_value() ||
        FAILED(dwrite.CreateTextLayout(wide.value().c_str(),
                                       static_cast<UINT32>(wide.value().size()), format,
                                       std::max(wrapping.first, 1.0F), unbounded, &layout)) ||
        FAILED(layout->SetWordWrapping(wrapping.second)))
    {
        return nullptr;
    }
    return layout;
}

[[nodiscard]] DWRITE_TEXT_METRICS metrics_of(IDWriteTextLayout *layout) noexcept
{
    DWRITE_TEXT_METRICS metrics{};
    if (layout != nullptr)
    {
        static_cast<void>(layout->GetMetrics(&metrics));
    }
    return metrics;
}

[[nodiscard]] std::uint32_t units_of(std::string_view text) noexcept
{
    return static_cast<std::uint32_t>(core::utf16_length(text).value_or(text.size()));
}

// 宛先の太字の範囲（UTF-16）と全体の長さ。
[[nodiscard]] std::pair<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t>
emphasis_units(const LinkCardContent &content) noexcept
{
    const std::string_view text = content.target;
    const auto first = std::min(content.emphasis.first, text.size());
    const auto second = std::clamp(content.emphasis.second, first, text.size());
    return {{units_of(text.substr(0, first)), units_of(text.substr(0, second))}, units_of(text)};
}

// 6 つの文字組み（link_card_text の順）。どれか作れなければ nullopt。
[[nodiscard]] std::optional<std::array<TextLayout, 6>>
made_texts(IDWriteFactory &dwrite, const LinkCardFormats &formats, const LinkCardContent &content,
           std::pair<float, std::uint32_t> inner)
{
    const auto [width, dpi] = inner;
    const float target_width = width - (2.0F * pixels(core::link_card_target_padding_x_dips, dpi));
    std::array<TextLayout, 6> texts{
        made(dwrite, formats.title(), content.texts.title, {width, DWRITE_WORD_WRAPPING_WRAP}),
        made(dwrite, formats.target(), content.target,
             {target_width, DWRITE_WORD_WRAPPING_EMERGENCY_BREAK}),
        made(dwrite, formats.way(), content.texts.way, {width, DWRITE_WORD_WRAPPING_WRAP}),
        nullptr,
        made(dwrite, formats.button(), content.texts.secondary,
             {unbounded, DWRITE_WORD_WRAPPING_NO_WRAP}),
        made(dwrite, formats.button(), content.texts.primary,
             {unbounded, DWRITE_WORD_WRAPPING_NO_WRAP})};
    // 案内は ボタンの幅が決まってから作る（ここでは空）。
    for (const std::size_t index :
         {link_card_text::title, link_card_text::target, link_card_text::way,
          link_card_text::secondary, link_card_text::primary})
    {
        if (!texts.at(index))
        {
            return std::nullopt;
        }
    }
    const auto emphasis = emphasis_units(content).first;
    if (emphasis.second > emphasis.first)
    {
        static_cast<void>(texts.at(link_card_text::target)
                              ->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD,
                                              {emphasis.first, emphasis.second - emphasis.first}));
    }
    return texts;
}

// ボタンの幅（最小の幅か、文字の幅に左右の余白）。
[[nodiscard]] float button_width(IDWriteTextLayout *label, std::uint32_t dpi) noexcept
{
    return std::max(pixels(core::link_card_button_min_width_dips, dpi),
                    std::ceil(metrics_of(label).width) +
                        (2.0F * pixels(core::link_card_button_padding_dips, dpi)));
}

[[nodiscard]] float height_of(IDWriteTextLayout *layout) noexcept
{
    return std::ceil(metrics_of(layout).height);
}

// 札の高さ（余白・題・宛先の枠・開き方・ボタンの行）。
[[nodiscard]] float card_height(const std::array<TextLayout, 6> &texts, std::uint32_t dpi) noexcept
{
    const float row = std::max(pixels(core::link_card_button_height_dips, dpi),
                               height_of(texts.at(link_card_text::hint).Get()));
    return (2.0F * pixels(core::link_card_padding_y_dips, dpi)) +
           height_of(texts.at(link_card_text::title).Get()) +
           pixels(core::link_card_title_gap_dips, dpi) +
           height_of(texts.at(link_card_text::target).Get()) +
           (2.0F * pixels(core::link_card_target_padding_y_dips, dpi)) +
           pixels(core::link_card_way_gap_dips, dpi) +
           height_of(texts.at(link_card_text::way).Get()) +
           pixels(core::link_card_row_gap_dips, dpi) + row;
}

// 札の矩形 box の中の部品の位置を paint に書く（上から題・宛先の枠・開き方・ボタンの行）。
void arrange(LinkCardPaint &paint, std::uint32_t dpi)
{
    const float left = paint.box.left + pixels(core::link_card_padding_x_dips, dpi);
    const float right = paint.box.right - pixels(core::link_card_padding_x_dips, dpi);
    float top = paint.box.top + pixels(core::link_card_padding_y_dips, dpi);
    paint.origins.at(link_card_text::title) = D2D1::Point2F(left, top);
    top += height_of(paint.texts.at(link_card_text::title).Get()) +
           pixels(core::link_card_title_gap_dips, dpi);
    const float target_bottom = top + height_of(paint.texts.at(link_card_text::target).Get()) +
                                (2.0F * pixels(core::link_card_target_padding_y_dips, dpi));
    paint.target_box = D2D1::RectF(left, top, right, target_bottom);
    paint.origins.at(link_card_text::target) =
        D2D1::Point2F(left + pixels(core::link_card_target_padding_x_dips, dpi),
                      top + pixels(core::link_card_target_padding_y_dips, dpi));
    top = target_bottom + pixels(core::link_card_way_gap_dips, dpi);
    paint.origins.at(link_card_text::way) = D2D1::Point2F(left, top);
    top += height_of(paint.texts.at(link_card_text::way).Get()) +
           pixels(core::link_card_row_gap_dips, dpi);
    const float button = pixels(core::link_card_button_height_dips, dpi);
    const float row = std::max(button, height_of(paint.texts.at(link_card_text::hint).Get()));
    const float button_top = top + ((row - button) / 2.0F);
    const float primary_left =
        right - button_width(paint.texts.at(link_card_text::primary).Get(), dpi);
    paint.primary = D2D1::RectF(primary_left, button_top, right, button_top + button);
    const float secondary_right = primary_left - pixels(core::link_card_button_gap_dips, dpi);
    paint.secondary = D2D1::RectF(
        secondary_right - button_width(paint.texts.at(link_card_text::secondary).Get(), dpi),
        button_top, secondary_right, button_top + button);
    paint.origins.at(link_card_text::hint) = D2D1::Point2F(
        left, top + ((row - height_of(paint.texts.at(link_card_text::hint).Get())) / 2.0F));
}

// ボタンの文字をボタンの中央に置く。
void centre_labels(LinkCardPaint &paint) noexcept
{
    const auto centred = [](const D2D1_RECT_F &box, IDWriteTextLayout *label)
    {
        const auto metrics = metrics_of(label);
        return D2D1::Point2F(box.left + ((box.right - box.left - metrics.width) / 2.0F),
                             box.top + ((box.bottom - box.top - metrics.height) / 2.0F));
    };
    paint.origins.at(link_card_text::secondary) =
        centred(paint.secondary, paint.texts.at(link_card_text::secondary).Get());
    paint.origins.at(link_card_text::primary) =
        centred(paint.primary, paint.texts.at(link_card_text::primary).Get());
}

[[nodiscard]] bool inside(const D2D1_RECT_F &box, std::int32_t x, std::int32_t y) noexcept
{
    const auto px = static_cast<float>(x);
    const auto py = static_cast<float>(y);
    return px >= box.left && px < box.right && py >= box.top && py < box.bottom;
}

void stroke_round(ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush, D2D1_RECT_F box,
                  std::pair<float, float> shape)
{
    const float half = shape.second / 2.0F;
    const D2D1_RECT_F inner =
        D2D1::RectF(box.left + half, box.top + half, box.right - half, box.bottom - half);
    target.DrawRoundedRectangle(D2D1::RoundedRect(inner, shape.first, shape.first), &brush,
                                shape.second);
}

void draw_text(ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush, const LinkCardPaint &paint,
               std::size_t index)
{
    target.DrawTextLayout(paint.origins.at(index), paint.texts.at(index).Get(), &brush);
}

// 宛先の太字の外を muted で描く（drawing effect の brush）。brush を作れなければ ink のまま。
void mute_target(ID2D1RenderTarget &target, const LinkCardPaint &paint,
                 Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> &muted)
{
    IDWriteTextLayout *layout = paint.texts.at(link_card_text::target).Get();
    if (FAILED(target.CreateSolidColorBrush(paint.inks.muted, &muted)))
    {
        return;
    }
    const auto [first, second] = paint.emphasis;
    static_cast<void>(layout->SetDrawingEffect(muted.Get(), {0, first}));
    static_cast<void>(
        layout->SetDrawingEffect(muted.Get(), {second, paint.target_length - second}));
}
} // namespace

std::optional<LinkCardPaint> link_card_paint(IDWriteFactory &dwrite, const LinkCardFormats &formats,
                                             const LinkCardContent &content,
                                             const LinkCardPlace &place)
{
    const std::uint32_t dpi = place.dpi;
    const auto width = core::link_card_width(core::width_of(place.area), dpi);
    const float inner =
        static_cast<float>(width) - (2.0F * pixels(core::link_card_padding_x_dips, dpi));
    auto texts = made_texts(dwrite, formats, content, {inner, dpi});
    if (!texts.has_value())
    {
        return std::nullopt;
    }
    auto &all = texts.value();
    const float buttons = button_width(all.at(link_card_text::secondary).Get(), dpi) +
                          button_width(all.at(link_card_text::primary).Get(), dpi) +
                          (2.0F * pixels(core::link_card_button_gap_dips, dpi));
    all.at(link_card_text::hint) = made(dwrite, formats.hint(), content.texts.hint,
                                        {inner - buttons, DWRITE_WORD_WRAPPING_WRAP});
    if (!all.at(link_card_text::hint))
    {
        return std::nullopt;
    }
    const auto box = core::link_card_rect(
        place.link, place.area,
        {width, static_cast<std::int32_t>(std::ceil(card_height(all, dpi)))}, dpi);
    const auto [emphasis, length] = emphasis_units(content);
    LinkCardPaint paint{D2D1::RectF(static_cast<float>(box.left), static_cast<float>(box.top),
                                    static_cast<float>(box.right), static_cast<float>(box.bottom)),
                        {},
                        {},
                        {},
                        std::move(all),
                        {},
                        emphasis,
                        length,
                        pixels(core::link_card_radius_dips, dpi),
                        pixels(core::link_card_target_radius_dips, dpi),
                        pixels(core::link_card_button_radius_dips, dpi),
                        std::max(1.0F, pixels(1, dpi)),
                        content.inks};
    arrange(paint, dpi);
    centre_labels(paint);
    return paint;
}

void paint_link_card(ID2D1RenderTarget &target, ID2D1SolidColorBrush &brush,
                     const LinkCardPaint &paint)
{
    brush.SetColor(paint.inks.ground);
    target.FillRoundedRectangle(D2D1::RoundedRect(paint.box, paint.radius, paint.radius), &brush);
    brush.SetColor(paint.inks.edge);
    stroke_round(target, brush, paint.box, {paint.radius, paint.stroke});
    stroke_round(target, brush, paint.target_box, {paint.target_radius, paint.stroke});
    stroke_round(target, brush, paint.secondary, {paint.button_radius, paint.stroke});
    brush.SetColor(paint.inks.accent);
    target.FillRoundedRectangle(
        D2D1::RoundedRect(paint.primary, paint.button_radius, paint.button_radius), &brush);
    target.PushAxisAlignedClip(paint.box, D2D1_ANTIALIAS_MODE_ALIASED);
    brush.SetColor(paint.inks.title);
    draw_text(target, brush, paint, link_card_text::title);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> muted;
    mute_target(target, paint, muted);
    brush.SetColor(paint.inks.ink);
    draw_text(target, brush, paint, link_card_text::target);
    draw_text(target, brush, paint, link_card_text::secondary);
    brush.SetColor(paint.inks.muted);
    draw_text(target, brush, paint, link_card_text::way);
    draw_text(target, brush, paint, link_card_text::hint);
    brush.SetColor(paint.inks.on_accent);
    draw_text(target, brush, paint, link_card_text::primary);
    target.PopAxisAlignedClip();
}

LinkCardTarget link_card_target_of(const LinkCardPaint &paint, std::int32_t x,
                                   std::int32_t y) noexcept
{
    if (inside(paint.primary, x, y))
    {
        return LinkCardTarget::primary;
    }
    if (inside(paint.secondary, x, y))
    {
        return LinkCardTarget::secondary;
    }
    return inside(paint.box, x, y) ? LinkCardTarget::card : LinkCardTarget::outside;
}
} // namespace nenenib::ui::win32
