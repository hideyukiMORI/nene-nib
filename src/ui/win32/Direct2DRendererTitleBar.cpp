// タブの帯（自前のタイトルバー・ADR 0056）とタブの札（#1789）の描画。Direct2DRenderer.cpp の
// 長さの上限（CNF-014）のため翻訳単位を分けた（同じクラス・振る舞いは変えない）。
#include "Direct2DRenderer.hpp"
#include "Direct2DRendererDetail.hpp"

#include "EditorFrame.hpp"
#include "HoverTip.hpp"
#include "LayoutRect.hpp"
#include "PaletteRowKey.hpp"
#include "PaletteTextRole.hpp"
#include "RectCovers.hpp"
#include "RgbColor.hpp"
#include "TabTip.hpp"
#include "TabTipBox.hpp"
#include "TitleBarHit.hpp"
#include "TitleBarLayout.hpp"
#include "TitleBarTarget.hpp"
#include "UiTextSizes.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace nenenib::ui::win32
{
namespace
{
[[nodiscard]] D2D1_POINT_2F centre_of(const core::LayoutRect &area) noexcept
{
    return D2D1::Point2F(static_cast<float>(area.left + area.right) / 2.0F,
                         static_cast<float>(area.top + area.bottom) / 2.0F);
}
} // namespace

using detail::to_color;
using detail::to_rect;

bool Direct2DRenderer::create_tab_tip_format(const wchar_t *face)
{
    if (FAILED(make_format(face, ui_text_dips, DWRITE_FONT_WEIGHT_NORMAL, tab_tip_format_)))
    {
        return false;
    }
    return SUCCEEDED(tab_tip_format_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
}

void Direct2DRenderer::draw_cross(const core::LayoutRect &box, float half, float stroke)
{
    const auto centre = centre_of(box);
    context_->DrawLine(D2D1::Point2F(centre.x - half, centre.y - half),
                       D2D1::Point2F(centre.x + half, centre.y + half), brush_.Get(), stroke);
    context_->DrawLine(D2D1::Point2F(centre.x + half, centre.y - half),
                       D2D1::Point2F(centre.x - half, centre.y + half), brush_.Get(), stroke);
}

void Direct2DRenderer::draw_caption_glyphs(const core::TitleBarLayout &layout, core::RgbColor color)
{
    brush_->SetColor(to_color(color));
    const float stroke = scaled(1.0F);
    const float half = static_cast<float>(layout.glyph) / 2.0F;
    const auto minimize = centre_of(layout.minimize);
    context_->DrawLine(D2D1::Point2F(minimize.x - half, minimize.y),
                       D2D1::Point2F(minimize.x + half, minimize.y), brush_.Get(), stroke);
    const auto maximize = centre_of(layout.maximize);
    context_->DrawRectangle(
        D2D1::RectF(maximize.x - half, maximize.y - half, maximize.x + half, maximize.y + half),
        brush_.Get(), stroke);
    draw_cross(layout.close, half, stroke);
}

void Direct2DRenderer::draw_tab_face(const core::TitleBarLayout &layout,
                                     const core::LayoutRect &tab, core::RgbColor color)
{
    // 上の角だけ丸め、下は帯の下端と面一にする（docs/design/2026-09-15-look.md 第 2 節）。
    fill_rounded(tab, color, static_cast<float>(layout.corner_radius));
    fill(core::LayoutRect{tab.left, tab.bottom - layout.corner_radius, tab.right, tab.bottom},
         color);
}

void Direct2DRenderer::draw_button_face(const application::EditorFrame &frame,
                                        const core::TitleBarLayout &layout,
                                        const core::LayoutRect &box, core::TitleBarTarget target)
{
    // ×・「∨」・「＋」にマウスを載せているときだけ toggle の面（ADR 0056 の決定 12）。
    if (layout.hovered == target)
    {
        fill_rounded(box, frame.palette.toggle, static_cast<float>(layout.button_radius));
    }
}

void Direct2DRenderer::draw_tab(const application::EditorFrame &frame,
                                const core::TitleBarLayout &layout, std::size_t index)
{
    const auto tab = core::tab_rect(layout, index);
    const bool active = index == layout.active;
    const bool lit = active || core::tab_hovered(layout, index);
    if (active)
    {
        draw_tab_face(layout, tab, frame.palette.tab_active);
        fill(core::LayoutRect{tab.left, tab.bottom - layout.underline, tab.right, tab.bottom},
             frame.palette.accent);
    }
    if (!active && lit)
    {
        draw_tab_face(layout, tab, frame.palette.tab_hover);
    }
    // × はアクティブなタブとマウスを載せたタブにだけある（D21）。題名はその左端まで。
    const auto close = core::tab_close_rect(layout, index);
    const auto right = close.has_value() ? close.value().left : tab.right - layout.tab_padding;
    const core::LayoutRect title{tab.left + layout.tab_padding, tab.top, right, tab.bottom};
    // プレビューのタブの題名は斜体（ADR 0179 の決定 10）。色は普通のタブと同じ。
    const auto &view = frame.tabs.at(index);
    write_tab_title(view.title.text(),
                    view.transient ? tab_transient_format_.Get() : tab_format_.Get(), title,
                    lit ? frame.palette.text : frame.palette.muted);
    if (!close.has_value())
    {
        return;
    }
    draw_button_face(frame, layout, close.value(),
                     core::TitleBarTarget{core::TitleBarHit::tab_close, index});
    brush_->SetColor(to_color(frame.palette.muted));
    draw_cross(close.value(), static_cast<float>(layout.glyph) / 2.0F, scaled(1.0F));
}

void Direct2DRenderer::write_tab_title(std::string_view text, IDWriteTextFormat *format,
                                       const core::LayoutRect &title, core::RgbColor color)
{
    // 題名の文字組みは帯の保持から引く（#924）。書式に焼いた文字単位の省略記号ごと保持し、
    // DrawTextW と同じ画素（tests/ui の GutterLayoutTests）。
    const auto shown =
        tab_layouts_.obtain(PaletteRowKey{format, PaletteTextRole::plain, text, title, dpi_},
                            [&] { return text_layout(text, format, title); });
    if (!shown)
    {
        return;
    }
    brush_->SetColor(to_color(color));
    context_->PushAxisAlignedClip(to_rect(title), D2D1_ANTIALIAS_MODE_ALIASED);
    context_->DrawTextLayout(
        D2D1::Point2F(static_cast<float>(title.left), static_cast<float>(title.top)), shown.Get(),
        brush_.Get());
    context_->PopAxisAlignedClip();
}

void Direct2DRenderer::draw_tabs(const application::EditorFrame &frame,
                                 const core::TitleBarLayout &layout)
{
    // タブの領域の外（左へ送られた分と「∨」の下）は描かない（D20）。
    context_->PushAxisAlignedClip(to_rect(layout.viewport), D2D1_ANTIALIAS_MODE_ALIASED);
    const std::size_t count = std::min(frame.tabs.size(), layout.tab_count);
    for (std::size_t index = 0; index < count; ++index)
    {
        if (core::tab_visible(layout, index))
        {
            draw_tab(frame, layout, index);
        }
    }
    context_->PopAxisAlignedClip();
}

void Direct2DRenderer::draw_add_tab(const application::EditorFrame &frame,
                                    const core::TitleBarLayout &layout)
{
    draw_button_face(frame, layout, layout.add_tab,
                     core::TitleBarTarget{core::TitleBarHit::add_tab, 0});
    brush_->SetColor(to_color(frame.palette.muted));
    const auto plus = centre_of(layout.add_tab);
    const float half = static_cast<float>(layout.glyph) / 2.0F;
    const float stroke = scaled(1.0F);
    context_->DrawLine(D2D1::Point2F(plus.x - half, plus.y), D2D1::Point2F(plus.x + half, plus.y),
                       brush_.Get(), stroke);
    context_->DrawLine(D2D1::Point2F(plus.x, plus.y - half), D2D1::Point2F(plus.x, plus.y + half),
                       brush_.Get(), stroke);
}

void Direct2DRenderer::draw_tab_list(const application::EditorFrame &frame,
                                     const core::TitleBarLayout &layout)
{
    // 「∨」はあふれているときだけある（D20）。押したときの一覧は #240。
    draw_button_face(frame, layout, layout.tab_list,
                     core::TitleBarTarget{core::TitleBarHit::tab_list, 0});
    brush_->SetColor(to_color(frame.palette.muted));
    const auto centre = centre_of(layout.tab_list);
    const float half = static_cast<float>(layout.glyph) / 2.0F;
    const float quarter = half / 2.0F;
    const float stroke = scaled(1.0F);
    context_->DrawLine(D2D1::Point2F(centre.x - half, centre.y - quarter),
                       D2D1::Point2F(centre.x, centre.y + quarter), brush_.Get(), stroke);
    context_->DrawLine(D2D1::Point2F(centre.x, centre.y + quarter),
                       D2D1::Point2F(centre.x + half, centre.y - quarter), brush_.Get(), stroke);
}

void Direct2DRenderer::draw_title_bar(const application::EditorFrame &frame,
                                      const core::TitleBarLayout &layout)
{
    // 帯は D16 で不透明。DWM の Mica は掛けない（#1488）ので、帯の見た目はこの面だけで決まる。
    fill(layout.band, frame.palette.title_bar);
    draw_tabs(frame, layout);
    if (layout.overflowing)
    {
        draw_tab_list(frame, layout);
    }
    draw_add_tab(frame, layout);
    draw_caption_glyphs(layout, frame.palette.muted);
}

// 札の文はタブの経路から core が作り（無題は今の言語の「無題」）、幅の上限を超えれば真ん中を
// 「…」で切る（TabTipLayout の 1 本）。置き場は core::tab_tip_rect で、色はトークンだけ。
void Direct2DRenderer::draw_tab_tip(const application::EditorFrame &frame,
                                    const core::TitleBarLayout &layout)
{
    if (!frame.tab_tip.has_value() || frame.tab_tip.value() >= frame.tabs.size())
    {
        return;
    }
    const std::size_t tab = frame.tab_tip.value();
    const std::string text = core::tab_tip_text(frame.tabs.at(tab).path, frame.language);
    static_cast<void>(tab_tip_layout_.fitted(*dwrite_.Get(), tab_tip_format_.Get(), text,
                                             core::tab_tip_text_limit(layout.band.right, dpi_)));
    const auto paint = tab_tip_layout_.painted(layout, tab, frame.palette, dpi_);
    if (paint.has_value())
    {
        paint_tab_tip(*context_.Get(), brush_.Get(), paint.value());
    }
}

bool Direct2DRenderer::hover_tip_covers(std::int32_t x, std::int32_t y) const
{
    return hover_tip_box_.has_value() && rect_covers(hover_tip_box_.value(), x, y);
}

// hover の札（ADR 0190 の決定 6）。文は frame の値（12 行に切ってある）で、幅の上限で折り返す
// （HoverTipLayout の 1 本）。書式と描く 1 本（paint_tab_tip）はタブの札と同じで、置き場は
// core::hover_tip_rect、色はトークンだけ。
void Direct2DRenderer::draw_hover_tip(const application::EditorFrame &frame,
                                      const core::LayoutRect &area)
{
    hover_tip_box_.reset();
    if (!frame.hover.has_value())
    {
        return;
    }
    static_cast<void>(
        hover_tip_layout_.fitted(*dwrite_.Get(), tab_tip_format_.Get(), frame.hover.value(),
                                 core::hover_tip_text_limit(core::width_of(area), dpi_)));
    const core::LayoutRect caret{caret_rectangle_.left, caret_rectangle_.top,
                                 caret_rectangle_.right, caret_rectangle_.bottom};
    const auto paint = hover_tip_layout_.painted(caret, area, frame.palette, dpi_);
    if (paint.has_value())
    {
        hover_tip_box_ = paint.value().box;
        paint_tab_tip(*context_.Get(), brush_.Get(), paint.value());
    }
}
} // namespace nenenib::ui::win32
