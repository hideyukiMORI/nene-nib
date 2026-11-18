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
#include "CompletionPointer.hpp"
#include "CompletionRowView.hpp"
#include "CompletionSource.hpp"
#include "CompletionView.hpp"
#include "ContextMenuText.hpp"
#include "DirectWriteFactory.hpp"
#include "LayoutRect.hpp"
#include "Palette.hpp"
#include "PaletteRowLayouts.hpp"
#include "PointerClicks.hpp"
#include "SoftwareTarget.hpp"
#include "VimSpecialKey.hpp"
#include "VimSpecialKeys.hpp"
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

// #1845（ADR 0169 の決定 3）: Vim の INSERT の補完の鍵の振り分け。Ctrl+N は INSERT で常に engine の
// Ctrl-n、Ctrl+P・Ctrl+E・Ctrl+Y は補完の面が開いている間だけ engine の鍵で、閉じていれば今までの道
// （Ctrl+P はファイルの一覧）へ流す。INSERT の外・Ctrl 以外の修飾つき・ほかの鍵は写さない。
void verify_vim_completion_keys()
{
    using nenenib::core::VimSpecialKey;
    using nenenib::ui::win32::vim_completion_key;
    expect(vim_completion_key('N', true, true, false) == VimSpecialKey::control_n &&
               vim_completion_key('N', true, true, true) == VimSpecialKey::control_n,
           "Ctrl+N is the engine key Ctrl-n in the Vim INSERT mode, face open or closed");
    expect(!vim_completion_key('P', true, true, false).has_value() &&
               !vim_completion_key('E', true, true, false).has_value() &&
               !vim_completion_key('Y', true, true, false).has_value(),
           "with no face shown (closed, or only waiting for the server) Ctrl+P goes on to the file "
           "list and Ctrl+E / Ctrl+Y stay as they were");
    expect(vim_completion_key('P', true, true, true) == VimSpecialKey::control_p &&
               vim_completion_key('E', true, true, true) == VimSpecialKey::control_e &&
               vim_completion_key('Y', true, true, true) == VimSpecialKey::control_y,
           "with the face open Ctrl+P is the previous candidate and Ctrl+E / Ctrl+Y end the "
           "completion");
    expect(!vim_completion_key('N', true, false, true).has_value() &&
               !vim_completion_key('P', true, false, true).has_value(),
           "outside the Vim INSERT mode no completion key is sent");
    expect(!vim_completion_key('N', false, true, true).has_value() &&
               !vim_completion_key('Q', true, true, true).has_value(),
           "a key without Ctrl alone, and a key that is not a completion key, are not sent");
}

// #1844 の再確認 R1b・L1b・L3b: 面のマウスの守り。面の上で始まった押しの列の 2 回目・3 回目は本文へ
// 渡さない・窓の外へ出た / 捕まえが変わったら押した行の覚えを下ろす・面の上の右と中の押しは面が
// 受ける。
void verify_completion_pointer_guards()
{
    using nenenib::application::PointerClicks;
    using nenenib::ui::win32::completion_press_lost;
    using nenenib::ui::win32::completion_swallows_press;
    using nenenib::ui::win32::completion_takes_button;
    expect(completion_swallows_press(PointerClicks::double_, true) &&
               completion_swallows_press(PointerClicks::triple, true) &&
               !completion_swallows_press(PointerClicks::single, true) &&
               !completion_swallows_press(PointerClicks::double_, false),
           "the second and third press of a click that began on the face do not reach the text");
    expect(completion_press_lost(WM_MOUSELEAVE) && completion_press_lost(WM_CAPTURECHANGED) &&
               !completion_press_lost(WM_MOUSEMOVE) && !completion_press_lost(WM_LBUTTONUP),
           "leaving the window or losing the capture forgets the row pressed on the face");
    expect(completion_takes_button(true, true) && !completion_takes_button(true, false) &&
               !completion_takes_button(false, true),
           "a right or middle press on the visible face is taken by the face");
}
} // namespace

void verify_completion_paint()
{
    verify_completion_pointer_guards();
    verify_vim_completion_keys();
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
