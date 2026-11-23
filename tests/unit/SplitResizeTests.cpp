// scope `--split-resize` の単体テスト（ADR 0148 の 2026-11-22 の追補・#1941）。境界の比と最小の純関
// 数（pane_layouts・split_extent・split_share_at・sized_share・split_divider_hit）と、ドラッグの意
// 図 ResizeSplit・鍵の意図 ResizePane・Vim の `Ctrl-w > < + - = _ |`・Ex の `:resize` の契約。
#include "CancelSelection.hpp"
#include "ClosePane.hpp"
#include "DevicePixels.hpp"
#include "DisplayLanguage.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "FontSize.hpp"
#include "LayoutRect.hpp"
#include "OpenDocument.hpp"
#include "PaneDimension.hpp"
#include "PaneFrame.hpp"
#include "PaneLayouts.hpp"
#include "PaneResize.hpp"
#include "PaneResizeStep.hpp"
#include "PaneSide.hpp"
#include "PaneSizing.hpp"
#include "PaneSplit.hpp"
#include "PreviewFrame.hpp"
#include "PreviewPlacement.hpp"
#include "ResizePane.hpp"
#include "ResizeSplit.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "ScrollLines.hpp"
#include "ScrollPane.hpp"
#include "SetPreviewPlacement.hpp"
#include "SettingsRows.hpp"
#include "SettingsValue.hpp"
#include "SplitExtent.hpp"
#include "SplitLayout.hpp"
#include "SplitPane.hpp"
#include "SplitShare.hpp"
#include "SwitchPane.hpp"
#include "TestSupport.hpp"
#include "TextViewGeometry.hpp"
#include "TogglePreview.hpp"
#include "VimKey.hpp"
#include "VimSpecialKey.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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

// on でも升を読まない `Ctrl-w = | _` は off と同じ（#1972 のレビュー L2）。
void verify_vim_fixed_resize_keys()
{
    const auto widest = [](bool follows)
    {
        Editing editing;
        open_vim_document(editing, "one\ntwo");
        auto &controller = editing.controller();
        static_cast<void>(window_keys(controller, "", "v"));
        applied(controller, view_of(10, 10));
        static_cast<void>(run_ex(controller, "set paneresizestep=7"));
        static_cast<void>(
            run_ex(controller, follows ? "set paneresizestepvim" : "set nopaneresizestepvim"));
        static_cast<void>(window_keys(controller, "", "_"));
        const bool unmoved = first_of(controller) == 500;
        static_cast<void>(window_keys(controller, "", "|"));
        const std::int32_t most = first_of(controller);
        static_cast<void>(window_keys(controller, "", "="));
        return std::pair{unmoved && first_of(controller) == 500, most};
    };
    const auto off = widest(false);
    const auto on = widest(true);
    expect(off.first && on.first && on.second == off.second && on.second != 500,
           "on: Ctrl-w _ | = move as they do off");
}

// `:set` の問い・`&`・`inv` と、F1 の操作が読む 1 回分（#1972 のレビュー L2・R2）。
void verify_pane_resize_step_ex()
{
    Editing editing;
    auto &controller = editing.controller();
    const auto message = [&controller](std::string_view command)
    {
        const EditorFrame frame = run_ex(controller, std::string(command));
        return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                                 : std::string{};
    };
    expect(message("set paneresizestep?") == "paneresizestep=5" &&
               message("set paneresizestep") == "paneresizestep=5" &&
               message("set paneresizestepvim?") == "paneresizestepvim=off",
           ":set paneresizestep? and paneresizestepvim? answer the values");
    static_cast<void>(run_ex(controller, "set invpaneresizestepvim"));
    expect(message("set paneresizestepvim?") == "paneresizestepvim=on",
           ":set invpaneresizestepvim flips the switch");
    static_cast<void>(run_ex(controller, "set paneresizestepvim&"));
    expect(message("set paneresizestepvim?") == "paneresizestepvim=off",
           ":set paneresizestepvim& restores the default");
    static_cast<void>(run_ex(controller, "set paneresizestep=7"));
    applied(controller, app::SplitPane{SplitLayout::vertical});
    applied(controller, view_of(10, 10));
    // ui の EditorWindow::resize_operated_pane と同じ形（升の数は controller の口から読む）。
    applied(controller, app::ResizePane{PaneSizing{PaneResize::grow, PaneDimension::width,
                                                   controller.pane_resize_step()}});
    expect(first_of(controller) == 570, "after :set paneresizestep=7 one Enlarge Pane moves 7");
}

// 回数なしの `Ctrl-w > < + -` は既定で本物の Vim と同じ 1 升、設定 `paneresizestepvim` が on なら
// 1 回分の設定の升。回数つきはどちらでも回数どおり（#1972・hide の決め 2026-11-23）。
void verify_vim_resize_step_setting()
{
    Editing editing;
    open_vim_document(editing, "one\ntwo");
    auto &controller = editing.controller();
    static_cast<void>(window_keys(controller, "", "v"));
    applied(controller, view_of(10, 10));
    static_cast<void>(run_ex(controller, "set paneresizestep=7"));
    static_cast<void>(window_keys(controller, "", ">"));
    expect(first_of(controller) == 510, "off: Ctrl-w > moves one cell like Vim");
    static_cast<void>(run_ex(controller, "set paneresizestepvim"));
    static_cast<void>(window_keys(controller, "", ">"));
    expect(first_of(controller) == 580, "on: Ctrl-w > moves the pane resize step");
    static_cast<void>(window_keys(controller, "", "<"));
    expect(first_of(controller) == 510, "on: Ctrl-w < moves the pane resize step back");
    static_cast<void>(window_keys(controller, "2", ">"));
    expect(first_of(controller) == 530, "on: a count still wins");
    static_cast<void>(run_ex(controller, "set nopaneresizestepvim"));
    static_cast<void>(window_keys(controller, "2", ">"));
    expect(first_of(controller) == 550, "off: a count still wins");
    static_cast<void>(run_ex(controller, "set paneresizestep=0"));
    static_cast<void>(run_ex(controller, "set paneresizestep=41"));
    expect(controller.pane_resize_step() == 7,
           ":set paneresizestep refuses a value outside 1 to 40");
}

// 1 回分の読み・送り・設定の画面の行（#1972）。既定は 5 升と off。
void verify_pane_resize_step_values()
{
    expect(core::pane_resize_step_of("1") == std::optional<std::uint8_t>{1} &&
               core::pane_resize_step_of("40") == std::optional<std::uint8_t>{40} &&
               !core::pane_resize_step_of("0").has_value() &&
               !core::pane_resize_step_of("41").has_value() &&
               !core::pane_resize_step_of("05").has_value() &&
               !core::pane_resize_step_of("").has_value() &&
               !core::pane_resize_step_of("5a").has_value() &&
               !core::pane_resize_step_of("-1").has_value() &&
               !core::pane_resize_step_of("5.5").has_value(),
           "the pane resize step reads 1 to 40 without a leading zero");
    expect(core::pane_resize_step_stepped(5, true) == 6 &&
               core::pane_resize_step_stepped(5, false) == 4 &&
               core::pane_resize_step_stepped(40, true) == 40 &&
               core::pane_resize_step_stepped(1, false) == 1,
           "the pane resize step moves by one and stops at the ends");
    expect(core::vim_uncounted_pane_cells(7, false) == 1 &&
               core::vim_uncounted_pane_cells(7, true) == 7,
           "uncounted Vim keys move one cell unless they follow the step");
    const auto defaults = core::default_editor_settings();
    expect(defaults.pane_resize_step == core::pane_resize_step_default &&
               !defaults.pane_resize_step_vim,
           "the defaults are five cells and off");
    const auto language = core::DisplayLanguage::english;
    const auto bigger = core::settings_value_stepped(
        defaults, core::SettingsValue::pane_resize_step, true, std::nullopt);
    const auto follows = core::settings_value_stepped(
        defaults, core::SettingsValue::pane_resize_step_vim, true, std::nullopt);
    expect(core::settings_value_text(bigger, core::SettingsValue::pane_resize_step, language) ==
                   "6" &&
               follows.pane_resize_step_vim &&
               !core::settings_value_stepped(follows, core::SettingsValue::pane_resize_step_vim,
                                             false, std::nullopt)
                    .pane_resize_step_vim,
           "the settings rows step the cells and flip the Vim switch");
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

// ---------------------------------------------------------------- 上下の分割（レビュー R4 の R3）

constexpr core::SplitExtent stacked_extent{PaneDimension::height, 0, 600, 300, 70};

// 上下の分割の寸法（帯の高さ 600・最小 70・行の高さ 20）。
app::VisibleLines stacked_view_of(std::size_t lines)
{
    app::VisibleLines view{lines, core::TextViewGeometry{480, 20, 96, 10.0F}};
    view.split = stacked_extent;
    view.other_geometry = core::TextViewGeometry{480, 20, 96, 10.0F};
    view.other_lines = lines;
    return view;
}

[[nodiscard]] std::int32_t top_of(const app::EditorController &controller)
{
    return core::split_first_length(stacked_extent, controller.pane_split().share);
}

void verify_stacked_resize_keys()
{
    Editing editing;
    open_vim_document(editing, "one\ntwo");
    auto &controller = editing.controller();
    static_cast<void>(window_keys(controller, "", "s"));
    applied(controller, stacked_view_of(10));
    static_cast<void>(window_keys(controller, "", "+"));
    expect(top_of(controller) == 320, "Ctrl-w + makes the upper pane one line taller");
    static_cast<void>(window_keys(controller, "3", "+"));
    expect(top_of(controller) == 380, "3 Ctrl-w + makes it three lines taller");
    static_cast<void>(window_keys(controller, "", "-"));
    static_cast<void>(window_keys(controller, "2", "-"));
    expect(top_of(controller) == 320, "Ctrl-w - and 2 Ctrl-w - make it shorter");
    static_cast<void>(window_keys(controller, "", ">"));
    static_cast<void>(window_keys(controller, "", "|"));
    expect(top_of(controller) == 320, "the width keys do nothing on a stacked split");
    static_cast<void>(window_keys(controller, "", "_"));
    expect(top_of(controller) == 530, "Ctrl-w _ makes the operated pane the tallest");
    static_cast<void>(window_keys(controller, "4", "_"));
    expect(top_of(controller) == 80, "4 Ctrl-w _ sets the operated pane to four lines");
    static_cast<void>(window_keys(controller, "", "="));
    static_cast<void>(window_keys(controller, "", "j"));
    static_cast<void>(window_keys(controller, "", "+"));
    expect(top_of(controller) == 280, "Ctrl-w + in the lower pane moves the divider up");
}

void verify_stacked_ex_resize()
{
    Editing editing;
    open_vim_document(editing, "one\ntwo");
    auto &controller = editing.controller();
    static_cast<void>(window_keys(controller, "", "s"));
    applied(controller, stacked_view_of(10));
    static_cast<void>(run_ex(controller, "resize 5"));
    expect(top_of(controller) == 100, ":resize 5 sets the height to five lines");
    static_cast<void>(run_ex(controller, "res +2"));
    expect(top_of(controller) == 140, ":res +2 makes it two lines taller");
    static_cast<void>(run_ex(controller, "resize -1"));
    expect(top_of(controller) == 120, ":resize -1 makes it one line shorter");
    static_cast<void>(run_ex(controller, "vertical resize 20"));
    expect(top_of(controller) == 120, ":vertical resize changes nothing on a stacked split");
    static_cast<void>(run_ex(controller, "resize"));
    expect(top_of(controller) == 530, ":resize without a number makes it the tallest");
    static_cast<void>(run_ex(controller, "resize 0"));
    expect(top_of(controller) == 70, ":resize 0 makes it the shortest");
}

// `Ctrl-w` の後ろの回数（Vim と同じ・前の回数と掛ける・実測 2026-11-22）。
void verify_counts_after_window_key()
{
    Editing editing;
    open_vim_document(editing, "one\ntwo");
    auto &controller = editing.controller();
    static_cast<void>(window_keys(controller, "", "v"));
    applied(controller, view_of(10, 10));
    static_cast<void>(window_keys(controller, "", "5>"));
    expect(first_of(controller) == 550, "Ctrl-w 5 > widens by five columns like 5 Ctrl-w >");
    static_cast<void>(window_keys(controller, "", "="));
    static_cast<void>(window_keys(controller, "2", "3>"));
    expect(first_of(controller) == 560, "2 Ctrl-w 3 > multiplies the two counts");
    static_cast<void>(window_keys(controller, "", "12|"));
    expect(first_of(controller) == 120, "Ctrl-w 12 | sets twelve columns");
    static_cast<void>(window_keys(controller, "", "10<"));
    expect(first_of(controller) == 100, "a zero after a digit is a digit");
    static_cast<void>(window_keys(controller, "", "="));
    static_cast<void>(window_keys(controller, "", "l"));
    static_cast<void>(window_keys(controller, "", "2>"));
    expect(first_of(controller) == 480, "Ctrl-w 2 > in the right pane widens the right pane");
}

// 大きさの鍵は本文を変えない（undo の単位を作らない・`.`
// に入らない）が、マクロでは再生される（録画は 打った鍵の道 vim_replay で流す）。
void verify_resize_keys_leave_history()
{
    Editing editing;
    open_vim_document(editing, "abcdef");
    auto &controller = editing.controller();
    static_cast<void>(window_keys(controller, "", "v"));
    applied(controller, view_of(10, 10));
    vim_normal(controller, "x");
    static_cast<void>(window_keys(controller, "3", ">"));
    vim_normal(controller, ".");
    expect(vim_body(controller.frame()) == "cdef" && first_of(controller) == 530,
           ". repeats the last change, not the size key");
    vim_normal(controller, "uu");
    expect(vim_body(controller.frame()) == "abcdef" && first_of(controller) == 530,
           "two u undo both changes because the size key left no undo unit");
    static_cast<void>(window_keys(controller, "", "="));
    vim_replay(controller, "qa2<C-w>>q");
    expect(first_of(controller) == 520, "a size key runs while a macro records");
    vim_replay(controller, "@a");

    vim_replay(controller, "@@");
    expect(first_of(controller) == 560, "@a and @@ replay the size key");
    vim_replay(controller, "qb<C-w>3<q@b");
    expect(first_of(controller) == 500, "a count after Ctrl-w is recorded and replayed");
    expect(vim_body(controller.frame()) == "abcdef", "the size keys leave the text alone");
}

// もう 1 つの枠の寸法を幅 other_width で送る（帯の長さ available・操作中の枠の first）。
app::VisibleLines other_view_of(std::int32_t available, std::int32_t first,
                                std::int32_t other_width)
{
    app::VisibleLines view{3, core::TextViewGeometry{480, 20, 96, 10.0F}};
    view.split = core::SplitExtent{PaneDimension::width, 0, available, first, 100};
    view.other_geometry = core::TextViewGeometry{other_width, 20, 96, 10.0F};
    view.other_lines = 3;
    return view;
}

// もう 1 つの枠の先頭の画面行（先頭の行の screen_row の符号を返した値）と先頭の行の番号。
[[nodiscard]] std::pair<std::size_t, std::int64_t> other_top(app::EditorController &controller)
{
    const EditorFrame frame = controller.apply_frame(app::ScrollLines{0});
    const auto *pane = pane_of(frame);
    if (pane == nullptr || pane->lines.empty())
    {
        return {0, -1};
    }
    return {pane->lines.front().number.value, -pane->lines.front().screen_row};
}

// 窓の大きさを変えてももう 1 つの枠の先頭の画面行は main のまま。境界を動かすと操作中の枠と同じ
// 規則（先頭の行を保って収める）で収め直す（レビュー R4 の R2）。
void verify_other_pane_rows()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, other_view_of(1000, 500, 100));
    applied(controller, app::InsertText{std::string(100, 'x') + "\nb\nc\nd\ne\nf\ng\nh"});
    applied(controller, app::SplitPane{SplitLayout::vertical});
    applied(controller, other_view_of(1000, 500, 100));
    applied(controller, app::ScrollPane{-100});
    applied(controller, app::ScrollPane{5});
    expect(other_top(controller) == std::pair<std::size_t, std::int64_t>{1, 5},
           "the other pane starts five rows into the long line");
    applied(controller, other_view_of(1200, 600, 120));
    expect(other_top(controller) == std::pair<std::size_t, std::int64_t>{1, 5},
           "resizing the window keeps the other pane's first row as on main");
    applied(controller, other_view_of(1200, 300, 480));
    const auto [line, row] = other_top(controller);
    expect(line == 1 && row >= 0 && row < 3,
           "moving the divider fits the other pane's first row into the wider line");
}

// もう 1 つの枠がプレビューでも大きさは変わり、境界を動かしてもプレビューに触れない。
void verify_preview_pane_resize()
{
    Editing editing;
    auto &controller = editing.controller();
    editing.files().hold(Bytes{std::string{"# title\n\ntext"}});
    applied(controller, app::VisibleLines{4});
    applied(controller, app::OpenDocument{sample_path()});
    applied(controller, app::TogglePreview{});
    applied(controller, view_of(4, 4));
    applied(controller, app::ResizePane{PaneSizing{PaneResize::grow, PaneDimension::width, 4}});
    expect(first_of(controller) == 540, "ResizePane grows the body beside a preview");
    const EditorFrame moved = controller.apply_frame(other_view_of(1000, 540, 450));
    expect(moved.other.has_value() &&
               std::holds_alternative<app::PreviewFrame>(moved.other.value()) &&
               moved.share == controller.pane_split().share,
           "moving the divider beside a preview keeps the preview pane");
}

// 横のプレビュー → 全面 → 戻ると、比は全面の前のまま（#1973）。本文の 2 枠でも同じ。全面から
// 出てもう一度側を開き直すと等分から。
void verify_share_through_full_preview()
{
    Editing editing;
    auto &controller = editing.controller();
    editing.files().hold(Bytes{std::string{"# title\n\ntext"}});
    applied(controller, app::VisibleLines{4});
    applied(controller, app::OpenDocument{sample_path()});
    applied(controller, app::TogglePreview{});
    applied(controller, app::ResizeSplit{SplitShare{1, 4}});
    applied(controller, app::SetPreviewPlacement{app::PreviewPlacement::full});
    expect(controller.frame().full_preview.has_value(), "the preview is full");
    const EditorFrame back = controller.apply_frame(app::CancelSelection{});
    expect(back.other.has_value() &&
               std::holds_alternative<app::PreviewFrame>(back.other.value()) &&
               back.share == SplitShare{1, 4} && controller.pane_split().share == SplitShare{1, 4},
           "side, full and side again keep the divider where it was");
    applied(controller, app::SetPreviewPlacement{app::PreviewPlacement::full});
    applied(controller, app::SetPreviewPlacement{app::PreviewPlacement::side});
    expect(controller.pane_split().share == SplitShare{1, 4},
           "leaving full for side by placement keeps the divider too");
    applied(controller, app::SetPreviewPlacement{app::PreviewPlacement::off});
    applied(controller, app::SetPreviewPlacement{app::PreviewPlacement::side});
    expect(controller.pane_split().share == SplitShare{},
           "closing the preview and opening it again starts even");
    // 側で比を動かす → 全面 → off → 全面（側のプレビューの無い全面）→
    // 側は等分（前の比を持ち越さない）。
    applied(controller, app::ResizeSplit{SplitShare{1, 4}});
    applied(controller, app::SetPreviewPlacement{app::PreviewPlacement::full});
    applied(controller, app::SetPreviewPlacement{app::PreviewPlacement::off});
    applied(controller, app::SetPreviewPlacement{app::PreviewPlacement::full});
    applied(controller, app::SetPreviewPlacement{app::PreviewPlacement::side});
    expect(controller.frame().other.has_value() && controller.pane_split().share == SplitShare{},
           "full entered without a side preview leaves for side evenly");

    Editing body;
    auto &split = body.controller();
    body.files().hold(Bytes{std::string{"# title\n\ntext"}});
    applied(split, app::VisibleLines{4});
    applied(split, app::OpenDocument{sample_path()});
    applied(split, app::SplitPane{SplitLayout::vertical});
    applied(split, app::ResizeSplit{SplitShare{3, 4}});
    applied(split, app::SetPreviewPlacement{app::PreviewPlacement::full});
    const EditorFrame returned = split.apply_frame(app::CancelSelection{});
    expect(returned.other.has_value() &&
               std::holds_alternative<app::PaneFrame>(returned.other.value()) &&
               returned.share == SplitShare{3, 4},
           "two body panes, full and back keep the divider where it was");
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
    verify_vim_resize_step_setting();
    verify_vim_fixed_resize_keys();
    verify_pane_resize_step_ex();
    verify_pane_resize_step_values();
    verify_ex_resize();
    verify_resize_pane_intent();
    verify_stacked_resize_keys();
    verify_stacked_ex_resize();
    verify_counts_after_window_key();
    verify_resize_keys_leave_history();
    verify_other_pane_rows();
    verify_preview_pane_resize();
    verify_share_through_full_preview();
}
} // namespace nenenib::tests
