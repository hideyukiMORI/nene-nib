// #1833（ADR 0169 の決定 4・ADR 0190 の決定 5）: 補完の面の画素。WIC の bitmap（SoftwareTarget）に
// CompletionPainter で描き、面の地が panel・選んだ行に current_line の面が敷かれて左右 4 DIP は地の
// まま・出どころの印と名前が行の左に、補足が行の右端にあってその間に墨が無い・面の外に何も描かない
// （影なし）・行の文字組みの保持が描き直しで増えないことを、ダークとライトで確かめる。配置は製品と
// 同じ core::completion_face_layout。窓・device は作らない。字の画素は字体に依るので、Segoe UI と
// Consolas が無い機械では測らない（QLT-013）。
#include "Appearance.hpp"
#include "CompletionFace.hpp"
#include "CompletionFaceLayout.hpp"
#include "CompletionFaceRequest.hpp"
#include "CompletionPainter.hpp"
#include "CompletionRowView.hpp"
#include "CompletionSource.hpp"
#include "CompletionView.hpp"
#include "ContextMenuText.hpp"
#include "DirectWriteFactory.hpp"
#include "LayoutRect.hpp"
#include "Palette.hpp"
#include "PaletteRowLayouts.hpp"
#include "SoftwareTarget.hpp"
#include "WindowChecks.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace nenenib::tests::ui
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;
using nenenib::ui::win32::CompletionPainter;
using nenenib::ui::win32::ContextMenuText;
using nenenib::ui::win32::PaletteRowLayouts;
using Format = Microsoft::WRL::ComPtr<IDWriteTextFormat>;
using Pixels = SoftwareTarget::Pixels;

constexpr std::uint32_t dpi = 96;
constexpr std::size_t channels = 4;
constexpr std::int32_t face_left = 40;
// キャレットは bitmap の上の外に置き、面の最初の 3 行が bitmap（高さ 80）に写る。
constexpr core::LayoutRect caret{face_left, -20, face_left + 2, -2};
constexpr core::LayoutRect bounds{0, -1000, 1300, 1000};

[[nodiscard]] Format format_of(IDWriteFactory2 &factory, const wchar_t *family, float size)
{
    Format format;
    if (FAILED(factory.CreateTextFormat(family, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size,
                                        L"", format.GetAddressOf())))
    {
        return nullptr;
    }
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    return format;
}

[[nodiscard]] std::size_t index_of(std::int32_t x, std::int32_t y) noexcept
{
    return ((static_cast<std::size_t>(y) * SoftwareTarget::width) + static_cast<std::size_t>(x)) *
           channels;
}

[[nodiscard]] bool pixel_is(const Pixels &pixels, std::int32_t x, std::int32_t y,
                            core::RgbColor color)
{
    const auto at = index_of(x, y);
    return pixels.size() > at + 3 && pixels.at(at) == color.blue &&
           pixels.at(at + 1) == color.green && pixels.at(at + 2) == color.red;
}

// 矩形の中で color と違う画素の数（墨の量）。
[[nodiscard]] std::size_t inked(const Pixels &pixels, const core::LayoutRect &area,
                                core::RgbColor color)
{
    std::size_t count = 0;
    for (auto y = std::max(area.top, 0); y < std::min(area.bottom, 80); ++y)
    {
        for (auto x = area.left; x < area.right; ++x)
        {
            count += pixel_is(pixels, x, y, color) ? 0U : 1U;
        }
    }
    return count;
}

[[nodiscard]] app::CompletionView sample_view(std::size_t selected)
{
    return app::CompletionView{
        7,
        {app::CompletionRowView{"main", "int ()", core::CompletionSource::language_server},
         app::CompletionRowView{"max", "", core::CompletionSource::buffer},
         app::CompletionRowView{"mix", "", core::CompletionSource::other_buffers}},
        selected,
        0,
        3,
        true};
}

[[nodiscard]] core::CompletionFaceLayout layout_of(const app::CompletionView &view)
{
    return core::completion_face_layout(core::CompletionFaceRequest{
        caret, face_left, bounds, view.rows.size(), view.selected, view.detailed, dpi});
}

void verify_face(SoftwareTarget &target, const ContextMenuText &text, const core::Palette &palette)
{
    PaletteRowLayouts layouts{CompletionPainter::retained};
    const auto paint = [&](const app::CompletionView &view)
    {
        return target.painted(
            [&](ID2D1RenderTarget &surface, ID2D1SolidColorBrush *brush)
            {
                CompletionPainter painter(surface, *brush, text, layouts);
                painter.paint(view, layout_of(view), palette);
            });
    };
    const auto view = sample_view(1);
    const auto layout = layout_of(view);
    const auto pixels = paint(view);
    const auto first = core::completion_face_row(layout, 0);
    const auto second = core::completion_face_row(layout, 1);
    expect(!layout.above && layout.rows == 3 && first.top >= 0 && second.bottom <= 80 &&
               pixel_is(pixels, first.left + 6, first.top + 2, palette.panel),
           "the completion face is filled with the panel color under the caret");
    expect(pixel_is(pixels, second.left + 8, second.top + 2, palette.current_line) &&
               pixel_is(pixels, second.left + 2, second.top + 2, palette.panel) &&
               pixel_is(pixels, first.left + 8, first.top + 2, palette.panel),
           "the chosen row lays the current line face 4 DIP inside the row");
    const auto mark = core::completion_face_mark(layout, 0);
    const auto label = core::completion_face_label(layout, 0);
    const auto detail = core::completion_face_detail(layout, 0);
    const core::LayoutRect name{label.left, label.top, label.left + 40, label.bottom};
    const core::LayoutRect gap{label.left + 60, label.top, detail.right - 60, label.bottom};
    const core::LayoutRect note{detail.right - 50, detail.top, detail.right, detail.bottom};
    expect(inked(pixels, mark, palette.panel) > 0 && inked(pixels, name, palette.panel) > 0,
           "a row draws the source mark and the candidate name on the left");
    expect(inked(pixels, note, palette.panel) > 0 && inked(pixels, gap, palette.panel) == 0,
           "the detail sits at the right end with nothing between it and the name");
    const auto plain = core::completion_face_detail(layout, 2);
    expect(inked(pixels, core::LayoutRect{plain.right - 50, plain.top, plain.right, plain.bottom},
                 palette.panel) == 0,
           "a row without a detail draws nothing in the detail column");
    expect(pixel_is(pixels, layout.panel.left - 2, first.top + 2, core::RgbColor{0, 0, 0}) &&
               pixel_is(pixels, layout.panel.right + 2, first.top + 2, core::RgbColor{0, 0, 0}),
           "nothing is drawn outside the face (no shadow)");
    const auto held = layouts.size();
    static_cast<void>(paint(view));
    expect(held > 0 && layouts.size() == held, "drawing the same face again makes no new layouts");
}
} // namespace

void verify_completion_paint()
{
    SoftwareTarget target;
    const auto factory = directwrite_factory();
    if (!target.ready() || factory == nullptr)
    {
        unmeasured("the completion face needs WIC and DirectWrite");
        return;
    }
    if (!installed(*factory.Get(), L"Segoe UI") || !installed(*factory.Get(), L"Consolas"))
    {
        unmeasured("the completion face pixels need Segoe UI and Consolas");
        return;
    }
    const Format name = format_of(*factory.Get(), L"Segoe UI", 13.0F);
    const Format key = format_of(*factory.Get(), L"Consolas", 12.0F);
    if (name == nullptr || key == nullptr)
    {
        unmeasured("the completion face formats could not be made");
        return;
    }
    const ContextMenuText text{factory.Get(), name.Get(), key.Get(), dpi};
    for (const auto appearance : {core::Appearance::dark, core::Appearance::light})
    {
        verify_face(target, text, core::palette_for(appearance));
    }
}
} // namespace nenenib::tests::ui
