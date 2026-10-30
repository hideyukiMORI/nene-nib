// scope `--explorer-layout` の単体テスト（ADR 0162 の決定 2・9・ADR 0042 決定 2）。面の幅
// min(240 DIP, client の 40%)・縦は本文の帯・字下げ 12 DIP・余白 8 DIP・境界 1 DIP・印の升・DPI・
// 窓の最小・消えているとき・client の原点。
#include "BodyLayout.hpp"
#include "ExplorerLayout.hpp"
#include "FontSize.hpp"
#include "LayoutRect.hpp"
#include "PaneLayouts.hpp"
#include "PaneSide.hpp"
#include "Scopes.hpp"
#include "SplitLayout.hpp"
#include "TestSupport.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;
using core::LayoutRect;

// 面の矩形。消えていれば幅 -1 の印。
LayoutRect panel_of(const core::ExplorerLayout &layout)
{
    return layout.panel.value_or(LayoutRect{0, 0, -1, -1});
}

void verify_width_rule()
{
    const auto font = core::default_font_size();
    const LayoutRect client{0, 0, 1200, 800};
    const auto layout = core::explorer_layout(client, 96, font, true);
    const auto body = core::body_layout(1200, 800, 96, font);
    expect(layout.panel.has_value() &&
               panel_of(layout) == LayoutRect{0, body.band.top, 240, body.band.bottom},
           "a wide window gets a 240 DIP panel over the body band");
    expect(layout.body == LayoutRect{240, 0, 1200, 800},
           "the body is the client shrunk from the left by the panel");
    expect(layout.line_height == body.line_height, "rows are as tall as the body lines");
    const auto narrow = core::explorer_layout(LayoutRect{0, 0, 500, 400}, 96, font, true);
    expect(core::width_of(panel_of(narrow)) == 200 && narrow.body.left == 200,
           "a narrow window caps the panel at 40 percent");
    const auto minimum = core::explorer_layout(LayoutRect{0, 0, 360, 200}, 96, font, true);
    expect(core::width_of(panel_of(minimum)) == 144 && core::width_of(minimum.body) == 216,
           "the 360 DIP minimum window leaves 216 DIP for the body");
    const auto odd = core::explorer_layout(LayoutRect{0, 0, 333, 200}, 96, font, true);
    expect(core::width_of(panel_of(odd)) == 133, "40 percent is rounded down");
    const auto empty = core::explorer_layout(LayoutRect{0, 0, 0, 0}, 96, font, true);
    expect(core::width_of(panel_of(empty)) == 0 && core::width_of(empty.body) == 0,
           "an empty client gives an empty panel");
}

void verify_hidden_and_origin()
{
    const auto font = core::default_font_size();
    const LayoutRect client{10, 5, 1210, 805};
    const auto hidden = core::explorer_layout(client, 96, font, false);
    expect(!hidden.panel.has_value() && hidden.body == client,
           "a hidden panel leaves the client as it is");
    const auto shown = core::explorer_layout(client, 96, font, true);
    const auto body = core::body_layout(1200, 800, 96, font);
    expect(panel_of(shown) == LayoutRect{10, 5 + body.band.top, 250, 5 + body.band.bottom} &&
               shown.body == LayoutRect{250, 5, 1210, 805},
           "the panel and the body follow the client origin");
}

// 面の行数は帯の全高を行の高さで割った数（#1125）。面を消していても、分割の枠の高さでもない。
void verify_panel_lines()
{
    const auto font = core::default_font_size();
    const LayoutRect client{0, 0, 1200, 800};
    const auto body = core::body_layout(1200, 800, 96, font);
    const auto lines = core::explorer_panel_lines(client, 96, font);
    expect(lines == static_cast<std::size_t>(core::height_of(body.band) / body.line_height),
           "the panel's lines are the body band height over the line height");
    expect(lines >= body.visible_lines, "the panel is never shorter than the unsplit body");
    const auto split = core::active_pane_layout(
        core::pane_layouts(core::explorer_layout(client, 96, font, true).body, 96, font,
                           core::SplitLayout::horizontal),
        core::PaneSide::first);
    expect(split.visible_lines + 1 < lines, "a horizontal split does not halve the panel's lines");
    expect(core::explorer_panel_lines(LayoutRect{0, 0, 0, 0}, 96, font) == 0,
           "an empty client has no panel lines");
}

void verify_steps_and_dpi()
{
    const auto font = core::default_font_size();
    const auto plain = core::explorer_layout(LayoutRect{0, 0, 1200, 800}, 96, font, true);
    expect(plain.indent == 12 && plain.padding == 8 && plain.border == 1 && plain.marker == 18,
           "at 96 DPI one step is 12, padding 8, border 1 and the marker one 18 DIP em");
    const auto scaled = core::explorer_layout(LayoutRect{0, 0, 1000, 800}, 144, font, true);
    expect(core::width_of(panel_of(scaled)) == 360 && scaled.indent == 18 && scaled.padding == 12 &&
               scaled.border == 2 && scaled.marker == 27,
           "150 percent scales the panel and the steps");
    expect(scaled.line_height == core::body_layout(1000, 800, 144, font).line_height,
           "the row height follows the body at the same DPI");
    const auto capped = core::explorer_layout(LayoutRect{0, 0, 600, 800}, 144, font, true);
    expect(core::width_of(panel_of(capped)) == 240,
           "40 percent of the physical width wins over 240 DIP at 150 percent");
    const auto larger = core::FontSize::from_points(27.0F).value();
    const auto big = core::explorer_layout(LayoutRect{0, 0, 1200, 800}, 96, larger, true);
    expect(big.marker == 36 &&
               big.line_height == core::body_layout(1200, 800, 96, larger).line_height,
           "the marker cell and the row height follow the font size");
    expect(core::explorer_layout(LayoutRect{0, 0, 1200, 800}, 48, font, true).border == 1,
           "the border is at least one pixel");
}

// 本文の枠は面の幅だけ右へ縮めた client から pane_layouts の 1 本で求める（決定 2・ADR 0148）。
// 面が消えていれば今と同じ枠、出ていれば帯の左端が面の右端で縦は面と同じ。2 枠も同じ形。
void verify_body_panes()
{
    const auto font = core::default_font_size();
    const LayoutRect client{0, 0, 1200, 800};
    const auto hidden = core::explorer_layout(client, 96, font, false);
    const auto plain = core::pane_layouts(client, 96, font, core::SplitLayout::none);
    expect(core::pane_layouts(hidden.body, 96, font, core::SplitLayout::none).first.content ==
               plain.first.content,
           "a hidden explorer leaves the body pane as it was");
    const auto shown = core::explorer_layout(client, 96, font, true);
    const auto single = core::pane_layouts(shown.body, 96, font, core::SplitLayout::none).first;
    expect(single.band.left == panel_of(shown).right && single.band.right == 1200 &&
               single.band.top == panel_of(shown).top &&
               single.band.bottom == panel_of(shown).bottom,
           "the body band starts at the right edge of the panel");
    expect(single.gutter.left == single.band.left && single.content.left > single.gutter.right - 1,
           "the gutter and the text follow the narrowed band");
    expect(single.visible_lines == plain.first.visible_lines,
           "the narrowed body keeps the same number of lines");
    const auto split = core::pane_layouts(shown.body, 96, font, core::SplitLayout::vertical);
    expect(split.first.band.left == panel_of(shown).right && split.second.has_value() &&
               split.second.value().band.right == 1200,
           "two panes share the narrowed band");
}
} // namespace

void verify_explorer_layout_scope()
{
    verify_width_rule();
    verify_hidden_and_origin();
    verify_steps_and_dpi();
    verify_panel_lines();
    verify_body_panes();
}
} // namespace nenenib::tests
