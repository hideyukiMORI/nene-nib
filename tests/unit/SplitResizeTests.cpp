// scope `--split-resize` の単体テスト（ADR 0148 の 2026-11-22 の追補・#1941）。境界の比と最小の純関
// 数（pane_layouts・split_extent・split_share_at・sized_share・split_divider_hit）と、ドラッグの意
// 図 ResizeSplit・鍵の意図 ResizePane・Vim の `Ctrl-w > < + - = _ |`・Ex の `:resize` の契約。
#include "ClosePane.hpp"
#include "DevicePixels.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "FontSize.hpp"
#include "LayoutRect.hpp"
#include "PaneDimension.hpp"
#include "PaneFrame.hpp"
#include "PaneLayouts.hpp"
#include "PaneResize.hpp"
#include "PaneSide.hpp"
#include "PaneSizing.hpp"
#include "PaneSplit.hpp"
#include "ResizePane.hpp"
#include "ResizeSplit.hpp"
#include "Scopes.hpp"
#include "ScrollLines.hpp"
#include "SplitExtent.hpp"
#include "SplitLayout.hpp"
#include "SplitPane.hpp"
#include "SplitShare.hpp"
#include "SwitchPane.hpp"
#include "TestSupport.hpp"
#include "TextViewGeometry.hpp"
#include "VimKey.hpp"
#include "VimSpecialKey.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using app::EditorFrame;
using core::PaneDimension;
using core::PaneResize;
using core::PaneSide;
using core::PaneSizing;
using core::PaneSplit;
using core::SplitLayout;
using core::SplitShare;

// 帯を端から端まで覆い、境界は 1 DIP ちょうど（等分でないときは余りを枠が取る）。
void check_covered(const core::PaneLayouts &panes, std::int32_t width, std::uint32_t dpi)
{
    const auto line = std::max(core::to_pixels(1, dpi), 1);
    expect(panes.second.has_value() && panes.divider.has_value(), "two panes and a divider");
    if (!panes.second.has_value() || !panes.divider.has_value())
    {
        return;
    }
    const auto &divider = panes.divider.value();
    expect(panes.first.band.left == 0 && panes.second.value().band.right == width &&
               divider.left == panes.first.band.right &&
               divider.right == panes.second.value().band.left && core::width_of(divider) == line,
           "an uneven vertical split covers the band and the divider is one DIP");
}

// 比のとおりに分け、最小で止まる（1 つの窓の幅と DPI）。
void check_uneven_vertical(std::int32_t width, std::uint32_t dpi)
{
    const auto font = core::default_font_size();
    const core::LayoutRect client{0, 0, width, 700};
    const auto extent = core::split_extent(client, dpi, font, PaneSplit{SplitLayout::vertical});
    if (!extent.has_value())
    {
        expect(false, "a vertical split has an extent");
        return;
    }
    expect(extent.value().dimension == PaneDimension::width && extent.value().minimum > 0 &&
               extent.value().available + std::max(core::to_pixels(1, dpi), 1) == width,
           "the extent of a vertical split is the band width without the divider");
    const auto quarter =
        core::pane_layouts(client, dpi, font, PaneSplit{SplitLayout::vertical, SplitShare{1, 4}});
    check_covered(quarter, width, dpi);
    expect(core::width_of(quarter.first.band) == extent.value().available / 4,
           "a quarter share gives the first pane a quarter of the band");
    const auto narrow =
        core::pane_layouts(client, dpi, font, PaneSplit{SplitLayout::vertical, SplitShare{0, 1}});
    const auto wide =
        core::pane_layouts(client, dpi, font, PaneSplit{SplitLayout::vertical, SplitShare{1, 1}});
    expect(core::width_of(narrow.first.band) == extent.value().minimum && wide.second.has_value() &&
               core::width_of(wide.second.value_or(wide.first).band) == extent.value().minimum,
           "neither pane gets narrower than the minimum");
    expect(core::width_of(narrow.first.content) > 0 &&
               narrow.first.content.left < narrow.first.band.right,
           "the minimum keeps the gutter and some text area");
}

// 帯が狭すぎれば等分に戻る。
void verify_uneven_vertical()
{
    for (const auto dpi : {96U, 144U})
    {
        for (const auto width : {1001, 1366})
        {
            check_uneven_vertical(width, dpi);
        }
    }
    const auto font = core::default_font_size();
    const core::LayoutRect tiny{0, 0, 120, 700};
    const auto even = core::pane_layouts(tiny, 96U, font, PaneSplit{SplitLayout::vertical});
    const auto squeezed =
        core::pane_layouts(tiny, 96U, font, PaneSplit{SplitLayout::vertical, SplitShare{1, 5}});
    expect(squeezed.first.band == even.first.band,
           "a band too narrow for two minimums splits evenly");
}
void verify_uneven_horizontal()
{
    const auto font = core::default_font_size();
    const core::LayoutRect client{0, 0, 900, 801};
    const auto extent = core::split_extent(client, 96U, font, PaneSplit{SplitLayout::horizontal});
    expect(extent.has_value() && extent.value().dimension == PaneDimension::height,
           "the extent of a horizontal split is the band height");
    if (!extent.has_value())
    {
        return;
    }
    const auto panes =
        core::pane_layouts(client, 96U, font, PaneSplit{SplitLayout::horizontal, SplitShare{2, 3}});
    expect(panes.second.has_value() &&
               core::height_of(panes.first.band) == extent.value().available * 2 / 3 &&
               panes.first.band.bottom + 1 == panes.second.value().band.top &&
               core::width_of(panes.first.band) == core::width_of(panes.second.value().band),
           "a horizontal share splits the height and keeps the width");
    const auto low =
        core::pane_layouts(client, 96U, font, PaneSplit{SplitLayout::horizontal, SplitShare{0, 1}});
    expect(low.first.visible_lines == 2,
           "the minimum height of a pane shows two lines at the default size");
}

// 境界の位置 → 比 → 配置が往復し、窓の幅が変わっても比を保つ。
void verify_share_round_trip()
{
    const auto font = core::default_font_size();
    const core::LayoutRect client{0, 0, 1000, 700};
    const auto split = PaneSplit{SplitLayout::vertical};
    const auto found = core::split_extent(client, 96U, font, split);
    if (!found.has_value())
    {
        expect(false, "a vertical split has an extent");
        return;
    }
    const auto extent = found.value();
    const auto share = core::split_share_at(extent, extent.origin + 300);
    expect(core::width_of(
               core::pane_layouts(client, 96U, font, PaneSplit{split.layout, share}).first.band) ==
               300,
           "the divider lands where it was dragged");
    const auto wider = core::pane_layouts(core::LayoutRect{0, 0, 2000, 700}, 96U, font,
                                          PaneSplit{split.layout, share});
    expect(core::width_of(wider.first.band) >= 598 && core::width_of(wider.first.band) <= 601,
           "a wider window keeps the share");
    expect(core::width_of(
               core::pane_layouts(client, 96U, font,
                                  PaneSplit{split.layout, core::split_share_at(extent, -500)})
                   .first.band) == extent.minimum,
           "dragging past the edge stops at the minimum");
    expect(core::split_first_length(extent, SplitShare{}) == extent.available / 2,
           "the even share is half the band");
    expect(!core::split_extent(client, 96U, font, PaneSplit{}).has_value(),
           "one pane has no extent");
}

// 鍵の大きさの変え方（Vim の実測: 操作中の枠が増減し、もう 1 つの枠が残りを取る）。
void verify_sized_share()
{
    const core::SplitExtent extent{PaneDimension::width, 0, 1000, 500, 100};
    const auto first_after = [&extent](PaneSide side, PaneSizing sizing)
    {
        const auto share = core::sized_share(extent, side, sizing, 10);
        return share.has_value() ? core::split_first_length(extent, share.value()) : -1;
    };
    expect(first_after(PaneSide::first, PaneSizing{PaneResize::grow, PaneDimension::width, 5}) ==
                   550 &&
               first_after(PaneSide::second,
                           PaneSizing{PaneResize::grow, PaneDimension::width, 5}) == 450,
           "grow widens the operated pane on either side");
    expect(first_after(PaneSide::first, PaneSizing{PaneResize::shrink, PaneDimension::width, 1}) ==
                   490 &&
               first_after(PaneSide::second,
                           PaneSizing{PaneResize::set, PaneDimension::width, 20}) == 800,
           "shrink and set count cells of the operated pane");
    expect(
        first_after(PaneSide::first, PaneSizing{PaneResize::set, PaneDimension::width, 0}) == 100 &&
            first_after(PaneSide::first, PaneSizing{PaneResize::grow, PaneDimension::width, 200}) ==
                900 &&
            first_after(PaneSide::second,
                        PaneSizing{PaneResize::maximize, PaneDimension::width, 1}) == 100,
        "set 0, a long grow and maximize stop at the minimum of a pane");
    expect(first_after(PaneSide::first, PaneSizing{PaneResize::even, PaneDimension::height, 1}) ==
               500,
           "even returns to half whatever the dimension");
    expect(!core::sized_share(extent, PaneSide::first,
                              PaneSizing{PaneResize::grow, PaneDimension::height, 1}, 10)
                .has_value(),
           "a height command on a vertical split does nothing");
}

void verify_divider_hit()
{
    const auto panes =
        core::pane_layouts(core::LayoutRect{0, 0, 1000, 700}, 96U, core::default_font_size(),
                           PaneSplit{SplitLayout::vertical, SplitShare{1, 4}});
    if (!panes.divider.has_value())
    {
        expect(false, "two panes have a divider");
        return;
    }
    const auto divider = panes.divider.value();
    const auto middle = (divider.top + divider.bottom) / 2;
    expect(core::split_divider_hit(panes, 96U, divider.left, middle) &&
               core::split_divider_hit(panes, 96U, divider.left - 3, middle) &&
               core::split_divider_hit(panes, 96U, divider.right + 2, middle),
           "the divider catches the pointer a few DIPs on either side");
    expect(!core::split_divider_hit(panes, 96U, divider.left - 4, middle) &&
               !core::split_divider_hit(panes, 96U, divider.left, divider.top - 1),
           "the pointer away from the divider is not on it");
    const auto one = core::pane_layouts(core::LayoutRect{0, 0, 1000, 700}, 96U,
                                        core::default_font_size(), PaneSplit{});
    expect(!core::split_divider_hit(one, 96U, 500, middle), "one pane has no divider");
}

// ---------------------------------------------------------------- 意図と frame

// 製品の body_view と同じく、操作中の枠の寸法と境界の寸法ともう 1 つの枠の寸法を送る。
app::VisibleLines view_of(std::size_t lines, std::size_t other_lines)
{
    app::VisibleLines view{lines, core::TextViewGeometry{480, 20, 96, 10.0F}};
    view.split = core::SplitExtent{PaneDimension::width, 0, 1000, 500, 100};
    view.other_geometry = core::TextViewGeometry{480, 20, 96, 10.0F};
    view.other_lines = other_lines;
    return view;
}

[[nodiscard]] const app::PaneFrame *pane_of(const EditorFrame &frame)
{
    return frame.other.has_value() ? std::get_if<app::PaneFrame>(&frame.other.value()) : nullptr;
}

void verify_resize_split_intent()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::InsertText{"l1\nl2\nl3\nl4\nl5\nl6"});
    expect(controller.apply_frame(app::ResizeSplit{SplitShare{1, 4}}).share == SplitShare{},
           "ResizeSplit on one pane does nothing");
    applied(controller, app::SplitPane{SplitLayout::vertical});
    const EditorFrame moved = controller.apply_frame(app::ResizeSplit{SplitShare{1, 4}});
    expect(moved.share == SplitShare{1, 4} && controller.pane_split().share == SplitShare{1, 4},
           "ResizeSplit moves the divider of two panes");
    expect(controller.apply_frame(app::SwitchPane{}).share == SplitShare{1, 4},
           "moving to the other pane keeps the divider where it is");
    applied(controller, app::ClosePane{});
    expect(controller.apply_frame(app::SplitPane{SplitLayout::horizontal}).share == SplitShare{},
           "closing the pane and splitting again starts even");
}

// もう 1 つの枠は自分の行数で行を作る（等分でなければ操作中の枠と違う）。
void verify_other_pane_size()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, view_of(6, 6));
    applied(controller, app::InsertText{"l1\nl2\nl3\nl4\nl5\nl6"});
    applied(controller, app::SplitPane{SplitLayout::vertical});
    applied(controller, view_of(6, 2));
    const EditorFrame frame = controller.apply_frame(app::ScrollLines{0});
    expect(pane_of(frame) != nullptr && pane_of(frame)->lines.size() == 2 &&
               frame.lines.size() == 6,
           "the other pane lays out its own number of lines");
}

[[nodiscard]] std::int32_t first_of(const app::EditorController &controller)
{
    return core::split_first_length(core::SplitExtent{PaneDimension::width, 0, 1000, 500, 100},
                                    controller.pane_split().share);
}

EditorFrame window_keys(app::EditorController &controller, std::string_view count,
                        std::string_view second)
{
    if (!count.empty())
    {
        static_cast<void>(controller.press_vim_keys(vim_keys_of(count)));
    }
    static_cast<void>(controller.press_vim_key(core::VimKey{core::VimSpecialKey::control_w}));
    return controller.press_vim_keys(vim_keys_of(second));
}

void verify_vim_resize_keys()
{
    Editing editing;
    open_vim_document(editing, "one\ntwo");
    auto &controller = editing.controller();
    applied(controller, view_of(10, 10));
    static_cast<void>(window_keys(controller, "", ">"));
    expect(controller.pane_split().share == SplitShare{}, "Ctrl-w > on one pane does nothing");
    static_cast<void>(window_keys(controller, "", "v"));
    applied(controller, view_of(10, 10));
    static_cast<void>(window_keys(controller, "3", ">"));
    expect(first_of(controller) == 530, "3 Ctrl-w > widens the operated pane by three columns");
    static_cast<void>(window_keys(controller, "", "<"));
    expect(first_of(controller) == 520, "Ctrl-w < narrows it by one column");
    static_cast<void>(window_keys(controller, "", "+"));
    static_cast<void>(window_keys(controller, "", "-"));
    expect(first_of(controller) == 520, "the height keys do nothing on a vertical split");
    static_cast<void>(window_keys(controller, "", "l"));
    static_cast<void>(window_keys(controller, "2", ">"));
    expect(first_of(controller) == 500, "Ctrl-w > in the right pane moves the divider left");
    static_cast<void>(window_keys(controller, "", "|"));
    expect(first_of(controller) == 100, "Ctrl-w | widens the operated pane to the most");
    static_cast<void>(window_keys(controller, "30", "|"));
    expect(first_of(controller) == 700, "30 Ctrl-w | sets the operated pane to thirty columns");
    static_cast<void>(window_keys(controller, "", "="));
    expect(first_of(controller) == 500, "Ctrl-w = splits evenly again");
}

void verify_ex_resize()
{
    Editing editing;
    open_vim_document(editing, "one\ntwo");
    auto &controller = editing.controller();
    static_cast<void>(window_keys(controller, "", "v"));
    applied(controller, view_of(10, 10));
    static_cast<void>(run_ex(controller, "vertical resize 20"));
    expect(first_of(controller) == 200, ":vertical resize 20 sets the width");
    static_cast<void>(run_ex(controller, "vert res +3"));
    expect(first_of(controller) == 230, ":vert res +3 widens by three");
    static_cast<void>(run_ex(controller, "vertical resize -5"));
    expect(first_of(controller) == 180, ":vertical resize -5 narrows by five");
    static_cast<void>(run_ex(controller, "resize 5"));
    expect(first_of(controller) == 180, ":resize changes nothing on a vertical split");
    static_cast<void>(run_ex(controller, "vertical resize"));
    expect(first_of(controller) == 900, ":vertical resize without a number widens to the most");
    const EditorFrame refused = run_ex(controller, "3resize");
    expect(refused.command_message.has_value() && first_of(controller) == 900,
           "a range before :resize is refused");
}

// 通常モードの操作（F1 の一覧）は ResizePane を送る。
void verify_resize_pane_intent()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::SplitPane{SplitLayout::vertical});
    applied(controller, view_of(10, 10));
    applied(controller, app::ResizePane{PaneSizing{PaneResize::grow, PaneDimension::width, 4}});
    expect(first_of(controller) == 540, "ResizePane grows the operated pane");
    applied(controller, app::ResizePane{PaneSizing{PaneResize::even, PaneDimension::width, 1}});
    expect(first_of(controller) == 500, "ResizePane even splits evenly again");
}
} // namespace

void verify_split_resize_contracts()
{
    verify_uneven_vertical();
    verify_uneven_horizontal();
    verify_share_round_trip();
    verify_sized_share();
    verify_divider_hit();
    verify_resize_split_intent();
    verify_other_pane_size();
    verify_vim_resize_keys();
    verify_ex_resize();
    verify_resize_pane_intent();
}
} // namespace nenenib::tests
