// #1051（ADR 0078 の追補）: 本文の右クリックのメニューの画素。WIC の bitmap（SoftwareTarget）に
// ContextMenuPainter で描き、面の地が panel・区切り線が panel_border の 1 DIP・選んだ行に selection
// の面が敷かれて左右 4 DIP は地のまま・使えない行の名前は使える行と違う色（muted）で描かれる・鍵の
// 表示は行の右端にあり名前との間に墨が無い・行の文字組みの保持が描き直しで増えないことを、ダークと
// ライトで確かめる。配置は製品と同じ core::context_menu_layout。窓・device
// は作らない。タブのメニュー（#1790）も同じ部品で 8 行と 2 本の区切り線を描き、無題のタブの薄い行が
// 違う色で描かれることを確かめる。字の画素は字体 に依るので、Segoe UI と Consolas
// が無い機械では測らない（QLT-013）。
#include "Appearance.hpp"
#include "ContextMenu.hpp"
#include "ContextMenuKind.hpp"
#include "ContextMenuLayout.hpp"
#include "ContextMenuPainter.hpp"
#include "ContextMenuRequest.hpp"
#include "ContextMenuRow.hpp"
#include "ContextMenuText.hpp"
#include "ContextMenuView.hpp"
#include "DirectWriteFactory.hpp"
#include "EditMode.hpp"
#include "EditorOperation.hpp"
#include "LayoutRect.hpp"
#include "Palette.hpp"
#include "PaletteRowLayouts.hpp"
#include "SoftwareTarget.hpp"
#include "TabMenuSituation.hpp"
#include "WindowChecks.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace nenenib::tests::ui
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;
using nenenib::ui::win32::ContextMenuPainter;
using nenenib::ui::win32::ContextMenuText;
using nenenib::ui::win32::PaletteRowLayouts;
using Format = Microsoft::WRL::ComPtr<IDWriteTextFormat>;
using Pixels = SoftwareTarget::Pixels;

constexpr std::uint32_t dpi = 96;
constexpr std::size_t channels = 4;
constexpr std::int32_t menu_left = 40;
// 描く範囲は bitmap の外へ広げる（高さ 80 の bitmap に面の一部だけを写す）。
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

[[nodiscard]] bool same_pixel(const Pixels &left, const Pixels &right, std::int32_t x,
                              std::int32_t y)
{
    const auto at = index_of(x, y);
    return left.size() > at + 3 && right.size() > at + 3 && left.at(at) == right.at(at) &&
           left.at(at + 1) == right.at(at + 1) && left.at(at + 2) == right.at(at + 2);
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

// 矩形の中で 2 枚の絵の違う画素の数。
[[nodiscard]] std::size_t differing(const Pixels &left, const Pixels &right,
                                    const core::LayoutRect &area)
{
    std::size_t count = 0;
    for (auto y = std::max(area.top, 0); y < std::min(area.bottom, 80); ++y)
    {
        for (auto x = area.left; x < area.right; ++x)
        {
            count += same_pixel(left, right, x, y) ? 0U : 1U;
        }
    }
    return count;
}

[[nodiscard]] app::ContextMenuView sample_view(bool cut_enabled,
                                               std::optional<std::size_t> highlighted)
{
    const auto items = core::context_menu_items(
        core::ContextMenuSituation{core::ContextMenuMode::ordinary, false});
    app::ContextMenuView view{menu_left, 0, core::ContextMenuKind::body, {}, highlighted};
    const std::array<std::string, 7> names{"切り取り",   "コピー", "貼り付け", "削除",
                                           "すべて選択", "検索",   "置換"};
    for (std::size_t index = 0; index < items.size(); ++index)
    {
        view.rows.push_back(app::ContextMenuRow{
            items.at(index).operation, names.at(index),
            core::context_menu_key(items.at(index).operation, core::ContextMenuMode::ordinary),
            index == 0 ? cut_enabled : true, items.at(index).separated});
    }
    return view;
}

// タブのメニュー（#1790）。named が偽なら無題のタブ（パスの 3 つが薄い）。
[[nodiscard]] app::ContextMenuView tab_view(bool named)
{
    const auto items = core::tab_menu_items(core::TabMenuSituation{named, true, true, true});
    app::ContextMenuView view{menu_left, 0, core::ContextMenuKind::tab, {}, std::nullopt};
    const std::array<std::string, 8> names{"エクスプローラーで場所を開く",
                                           "パスをコピー",
                                           "ファイル名をコピー",
                                           "保存",
                                           "名前を付けて保存",
                                           "タブを閉じる",
                                           "ほかのタブを閉じる",
                                           "右のタブを閉じる"};
    for (std::size_t index = 0; index < items.size(); ++index)
    {
        view.rows.push_back(app::ContextMenuRow{
            items.at(index).operation, names.at(index),
            core::tab_menu_key(items.at(index).operation, core::EditMode::ordinary),
            items.at(index).enabled, items.at(index).separated});
    }
    return view;
}

// 面を top の高さに開いて描く。
class MenuStrip final
{
  public:
    MenuStrip(SoftwareTarget &target, const ContextMenuText &text, PaletteRowLayouts &layouts,
              const core::Palette &palette)
        : target_(target), text_(text), layouts_(layouts), palette_(palette)
    {
    }
    [[nodiscard]] core::ContextMenuLayout layout_of(const app::ContextMenuView &view,
                                                    std::int32_t top) const
    {
        const auto widths = measure(view);
        return core::context_menu_layout(core::ContextMenuRequest{
            menu_left, top, bounds, widths.first, widths.second, dpi, view.kind});
    }
    [[nodiscard]] Pixels operator()(const app::ContextMenuView &view, std::int32_t top) const
    {
        const auto layout = layout_of(view, top);
        return target_.painted(
            [&](ID2D1RenderTarget &surface, ID2D1SolidColorBrush *brush)
            {
                ContextMenuPainter painter(surface, *brush, text_, layouts_);
                painter.paint(view, layout, palette_);
            });
    }

  private:
    [[nodiscard]] std::pair<std::int32_t, std::int32_t>
    measure(const app::ContextMenuView &view) const
    {
        std::pair<std::int32_t, std::int32_t> widths{0, 0};
        static_cast<void>(target_.painted(
            [&](ID2D1RenderTarget &surface, ID2D1SolidColorBrush *brush)
            {
                const ContextMenuPainter painter(surface, *brush, text_, layouts_);
                const auto measured = painter.measure(view);
                widths = {measured.name, measured.key};
            }));
        return widths;
    }

    SoftwareTarget &target_;
    const ContextMenuText &text_;
    PaletteRowLayouts &layouts_;
    const core::Palette &palette_;
};

void verify_rows(const MenuStrip &strip, PaletteRowLayouts &layouts, const core::Palette &palette)
{
    const auto plain = sample_view(false, std::nullopt);
    const auto layout = strip.layout_of(plain, 0);
    const auto pixels = strip(plain, 0);
    const auto &first = layout.rows.at(0);
    const auto &second = layout.rows.at(1);
    expect(pixel_is(pixels, first.left + 2, second.top + 2, palette.panel),
           "the menu is filled with the panel color");
    const auto chosen = strip(sample_view(false, std::size_t{1}), 0);
    expect(!same_pixel(pixels, chosen, first.left + 8, second.top + 2) &&
               same_pixel(pixels, chosen, first.left + 2, second.top + 2),
           "the chosen row lays the selection face 4 DIP inside the row");
    const auto inside = core::context_menu_row_inside(layout, 0);
    const core::LayoutRect name_area{inside.left, first.top, inside.left + 40, first.bottom};
    const auto enabled = strip(sample_view(true, std::nullopt), 0);
    expect(inked(pixels, name_area, palette.panel) > 0 && differing(pixels, enabled, name_area) > 0,
           "a disabled name is drawn in another color than an enabled one");
    const core::LayoutRect key_area{inside.right - 40, first.top, inside.right, first.bottom};
    const core::LayoutRect gap{inside.left + 70, first.top, inside.right - 50, first.bottom};
    expect(inked(pixels, key_area, palette.panel) > 0 && inked(pixels, gap, palette.panel) == 0,
           "the key label sits at the right end with nothing between it and the name");
    const auto held = layouts.size();
    static_cast<void>(strip(plain, 0));
    expect(layouts.size() == held, "drawing the same menu again makes no new layouts");
}

void verify_separator(const MenuStrip &strip, const core::Palette &palette)
{
    const auto view = sample_view(true, std::nullopt);
    // 最初の区切り線（削除の行の下 4 DIP）が bitmap の y 28 に来る高さ。
    const auto probe = strip.layout_of(view, 0);
    const auto top = 28 - probe.separators.at(0).top;
    const auto layout = strip.layout_of(view, top);
    const auto pixels = strip(view, top);
    const auto line = layout.separators.at(0);
    expect(line.top == 28 && pixel_is(pixels, line.left + 4, line.top, palette.panel_border) &&
               pixel_is(pixels, line.left + 4, line.top - 2, palette.panel) &&
               pixel_is(pixels, line.left + 4, line.top + 2, palette.panel),
           "the separator is a 1 DIP panel border line with the panel above and below");
    expect(pixel_is(pixels, layout.panel.left - 2, line.top, core::RgbColor{0, 0, 0}),
           "nothing is drawn outside the menu (no shadow)");
}
// タブのメニューは同じ部品で 8 行と 2 本の区切り線を描き、無題のタブの薄い行は違う色（#1790）。
void verify_tab_menu(const MenuStrip &strip, const core::Palette &palette)
{
    const auto named = tab_view(true);
    const auto probe = strip.layout_of(named, 0);
    expect(probe.row_count == 8 && probe.separator_count == 2,
           "the tab menu lays out 8 rows and 2 separators");
    // 2 本目の区切り線（名前を付けて保存の下・タブを閉じるの上）が bitmap の y 28 に来る高さ。
    const auto top = 28 - probe.separators.at(1).top;
    const auto layout = strip.layout_of(named, top);
    const auto pixels = strip(named, top);
    const auto line = layout.separators.at(1);
    expect(line.top == 28 && pixel_is(pixels, line.left + 4, line.top, palette.panel_border) &&
               pixel_is(pixels, line.left + 4, line.top - 2, palette.panel),
           "the second tab menu separator is drawn above close");
    // 最後の行（右のタブを閉じる）が bitmap の y 25 から始まる高さ。
    const auto last_top = 25 - probe.rows.at(7).top;
    const auto last = strip(named, last_top);
    const auto inside = core::context_menu_row_inside(strip.layout_of(named, last_top), 7);
    const core::LayoutRect last_name{inside.left, inside.top, inside.left + 40, inside.bottom};
    expect(inked(last, last_name, palette.panel) > 0, "the eighth row of the tab menu is drawn");
    // 無題のタブの最初の行（エクスプローラーで場所を開く）は薄い。
    const auto first = core::context_menu_row_inside(probe, 0);
    const core::LayoutRect first_name{first.left, first.top, first.left + 40, first.bottom};
    const auto untitled = strip(tab_view(false), 0);
    expect(differing(strip(named, 0), untitled, first_name) > 0,
           "a greyed tab row is drawn in another color");
}
} // namespace

void verify_context_menu_paint()
{
    SoftwareTarget target;
    const auto factory = directwrite_factory();
    if (!target.ready() || factory == nullptr)
    {
        unmeasured("the context menu needs WIC and DirectWrite");
        return;
    }
    if (!installed(*factory.Get(), L"Segoe UI") || !installed(*factory.Get(), L"Consolas"))
    {
        unmeasured("the context menu pixels need Segoe UI and Consolas");
        return;
    }
    const Format name = format_of(*factory.Get(), L"Segoe UI", 13.0F);
    const Format key = format_of(*factory.Get(), L"Consolas", 12.0F);
    if (name == nullptr || key == nullptr)
    {
        unmeasured("the context menu formats could not be made");
        return;
    }
    const ContextMenuText text{factory.Get(), name.Get(), key.Get(), dpi};
    for (const auto appearance : {core::Appearance::dark, core::Appearance::light})
    {
        const auto palette = core::palette_for(appearance);
        PaletteRowLayouts layouts{ContextMenuPainter::retained};
        const MenuStrip strip(target, text, layouts, palette);
        verify_rows(strip, layouts, palette);
        verify_separator(strip, palette);
        verify_tab_menu(strip, palette);
    }
}
} // namespace nenenib::tests::ui
