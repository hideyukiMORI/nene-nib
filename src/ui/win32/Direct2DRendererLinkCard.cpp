// プレビューのリンクの札の描画と当たり（施主決定 D106・#1869 の段 L2）。Direct2DRenderer.cpp の
// 長さの上限（CNF-014）のため翻訳単位を分けた。置き場はこの frame で描いたプレビューのリンクの
// 矩形（PreviewPane::range_box）で、測れなければ領域の左上に置く（札は開いている）。
#include "Direct2DRenderer.hpp"
#include "Direct2DRendererDetail.hpp"

#include "DevicePixels.hpp"
#include "EditorFrame.hpp"
#include "LayoutRect.hpp"
#include "LinkCard.hpp"
#include "LinkCardContent.hpp"
#include "LinkCardInks.hpp"
#include "LinkCardLayout.hpp"
#include "LinkCardPlace.hpp"
#include "LinkCardTarget.hpp"
#include "LinkDestination.hpp"
#include "LinkKind.hpp"
#include "UiLocale.hpp"

#include <cstdint>
#include <optional>

namespace nenenib::ui::win32
{
using detail::to_color;

LinkCardTarget Direct2DRenderer::link_card_target_at(std::int32_t x, std::int32_t y) const
{
    return link_card_paint_.has_value() ? link_card_target_of(link_card_paint_.value(), x, y)
                                        : LinkCardTarget::outside;
}

// 書式は札を最初に描くときに作る（出していない間は持たない）。
bool Direct2DRenderer::link_card_formats_made()
{
    return link_card_formats_.made(
        *dwrite_.Get(),
        {family(L"Segoe UI Variable Text", L"Segoe UI"), family(L"Cascadia Mono", L"Consolas")},
        static_cast<float>(dpi_) / static_cast<float>(core::reference_dpi),
        ui_locale_name(formatted_language_));
}

void Direct2DRenderer::draw_link_card(const application::EditorFrame &frame,
                                      const core::LayoutRect &area)
{
    link_card_paint_.reset();
    if (!frame.link_card.has_value() || !link_card_formats_made())
    {
        return;
    }
    const auto &card = frame.link_card.value();
    const auto link = preview_.range_box(card.block, card.link.begin.value, card.link.end.value)
                          .value_or(core::LayoutRect{area.left, area.top, area.left, area.top});
    const bool refused = card.destination.kind == core::LinkKind::refused ||
                         card.destination.kind == core::LinkKind::heading;
    // 見えない字は印に置き換えて描く（写すのは元の綴り・#1876 のレビューの軽い 4）。
    const core::LinkDestination shown{card.destination.kind,
                                      core::link_card_shown_text(card.destination.text)};
    const LinkCardContent content{
        core::link_card_texts(card.destination.kind, frame.language), shown.text,
        core::link_card_emphasis(shown),
        LinkCardInks{to_color(frame.palette.panel), to_color(frame.palette.panel_border),
                     to_color(frame.palette.text), to_color(frame.body.comment),
                     to_color(refused ? frame.body.error : frame.palette.text),
                     to_color(frame.body.function), to_color(frame.palette.background)}};
    link_card_paint_ = link_card_paint(*dwrite_.Get(), link_card_formats_, content,
                                       LinkCardPlace{link, area, dpi_});
    if (link_card_paint_.has_value())
    {
        paint_link_card(*context_.Get(), *brush_.Get(), link_card_paint_.value());
    }
}
} // namespace nenenib::ui::win32
