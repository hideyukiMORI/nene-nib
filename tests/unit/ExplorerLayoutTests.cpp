// scope `--explorer-layout` の単体テスト（ADR 0162 の決定 2・9・ADR 0042 決定 2）。面の幅
// min(240 DIP, client の 40%)・縦は本文の帯・採用案 B の寸法（行 22 DIP・ルート名の行 30 DIP・升・
// 丸い面・縦の線・#1390）・DPI・窓の最小・消えているとき・client
// の原点・点の下の行・印の色の役割。使う人が決めた幅の切り詰め・ドラッグの幅・境の当たり・
// 設定の値の読み（追補 D78）。
#include "BodyLayout.hpp"
#include "ExplorerLayout.hpp"
#include "ExplorerMark.hpp"
#include "ExplorerWidth.hpp"
#include "FontSize.hpp"
#include "LanguageTable.hpp"
#include "LayoutRect.hpp"
#include "PaneLayouts.hpp"
#include "PaneSide.hpp"
#include "Scopes.hpp"
#include "SplitLayout.hpp"
#include "SyntaxRole.hpp"
#include "TestSupport.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

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
    const auto layout = core::explorer_layout(client, 96, core::ExplorerWidth{});
    const auto body = core::body_layout(1200, 800, 96, font);
    expect(layout.panel.has_value() &&
               panel_of(layout) == LayoutRect{0, body.band.top, 240, body.band.bottom},
           "a wide window gets a 240 DIP panel over the body band");
    expect(layout.body == LayoutRect{240, 0, 1200, 800},
           "the body is the client shrunk from the left by the panel");
    expect(layout.line_height == 22 && layout.root_height == 30,
           "rows are 22 DIP and the root line 30 DIP, not the body line height");
    const auto narrow =
        core::explorer_layout(LayoutRect{0, 0, 500, 400}, 96, core::ExplorerWidth{});
    expect(core::width_of(panel_of(narrow)) == 200 && narrow.body.left == 200,
           "a narrow window caps the panel at 40 percent");
    const auto minimum =
        core::explorer_layout(LayoutRect{0, 0, 360, 200}, 96, core::ExplorerWidth{});
    expect(core::width_of(panel_of(minimum)) == 144 && core::width_of(minimum.body) == 216,
           "the 360 DIP minimum window leaves 216 DIP for the body");
    const auto odd = core::explorer_layout(LayoutRect{0, 0, 333, 200}, 96, core::ExplorerWidth{});
    expect(core::width_of(panel_of(odd)) == 133, "40 percent is rounded down");
    const auto empty = core::explorer_layout(LayoutRect{0, 0, 0, 0}, 96, core::ExplorerWidth{});
    expect(core::width_of(panel_of(empty)) == 0 && core::width_of(empty.body) == 0,
           "an empty client gives an empty panel");
}

void verify_hidden_and_origin()
{
    const auto font = core::default_font_size();
    const LayoutRect client{10, 5, 1210, 805};
    const auto hidden = core::explorer_layout(client, 96, std::nullopt);
    expect(!hidden.panel.has_value() && hidden.body == client,
           "a hidden panel leaves the client as it is");
    const auto shown = core::explorer_layout(client, 96, core::ExplorerWidth{});
    const auto body = core::body_layout(1200, 800, 96, font);
    expect(panel_of(shown) == LayoutRect{10, 5 + body.band.top, 250, 5 + body.band.bottom} &&
               shown.body == LayoutRect{250, 5, 1210, 805},
           "the panel and the body follow the client origin");
}

// 面の行数はルート名の行（30 DIP）と、帯の残りの高さを行の高さ（22 DIP）で割った数（#1125・
// #1390）。面を消していても、分割の枠の高さでもなく、本文の字の大きさにも依らない。
void verify_panel_lines()
{
    const auto font = core::default_font_size();
    const LayoutRect client{0, 0, 1200, 800};
    const auto body = core::body_layout(1200, 800, 96, font);
    const auto lines = core::explorer_panel_lines(client, 96);
    expect(lines == 1 + static_cast<std::size_t>((core::height_of(body.band) - 30) / 22),
           "the panel's lines are the root line and the rest of the band over 22 DIP");
    const auto split = core::active_pane_layout(
        core::pane_layouts(core::explorer_layout(client, 96, core::ExplorerWidth{}).body, 96, font,
                           core::SplitLayout::horizontal),
        core::PaneSide::first);
    expect(split.visible_lines + 1 < lines, "a horizontal split does not halve the panel's lines");
    expect(core::explorer_panel_lines(LayoutRect{0, 0, 0, 0}, 96) == 0,
           "an empty client has no panel lines");
    // 帯の高さが root_height ちょうどならルート名の 1 行、22 DIP 足すごとに 1 行。
    const auto band = core::body_band(1200, 800, 96);
    const auto exact = [&](std::int32_t extra)
    {
        return core::explorer_panel_lines(
            LayoutRect{0, 0, 1200, 800 - core::height_of(band) + 30 + extra}, 96);
    };
    expect(exact(0) == 1 && exact(21) == 1 && exact(22) == 2 && exact(-1) == 0,
           "the root line comes first and every 22 DIP adds one row");
}

// 寸法は DIP の固定値で、DPI 100 / 150 / 200% に比例し（四捨五入）、字の大きさに依らない（#1390）。
void verify_steps_and_dpi()
{
    const auto plain =
        core::explorer_layout(LayoutRect{0, 0, 1200, 800}, 96, core::ExplorerWidth{});
    expect(plain.line_height == 22 && plain.root_height == 30 && plain.indent == 12 &&
               plain.cell == 16 && plain.inset == 6 && plain.padding == 4 &&
               plain.root_padding == 12 && plain.gap == 6 && plain.radius == 5 && plain.dot == 6 &&
               plain.dot_inset == 14 && plain.border == 1,
           "at 96 DPI the steps are the table's DIP values");
    const auto scaled =
        core::explorer_layout(LayoutRect{0, 0, 1000, 800}, 144, core::ExplorerWidth{});
    expect(core::width_of(panel_of(scaled)) == 360 && scaled.line_height == 33 &&
               scaled.root_height == 45 && scaled.indent == 18 && scaled.cell == 24 &&
               scaled.inset == 9 && scaled.padding == 6 && scaled.root_padding == 18 &&
               scaled.gap == 9 && scaled.radius == 8 && scaled.dot == 9 && scaled.dot_inset == 21 &&
               scaled.border == 2,
           "150 percent scales the panel and the steps, rounding halves up");
    const auto doubled =
        core::explorer_layout(LayoutRect{0, 0, 1200, 800}, 192, core::ExplorerWidth{});
    expect(doubled.line_height == 44 && doubled.root_height == 60 && doubled.indent == 24 &&
               doubled.cell == 32 && doubled.inset == 12 && doubled.padding == 8 &&
               doubled.radius == 10 && doubled.dot == 12 && doubled.border == 2,
           "200 percent doubles every step");
    const auto capped =
        core::explorer_layout(LayoutRect{0, 0, 600, 800}, 144, core::ExplorerWidth{});
    expect(core::width_of(panel_of(capped)) == 240,
           "40 percent of the physical width wins over 240 DIP at 150 percent");
    expect(core::explorer_layout(LayoutRect{0, 0, 1200, 800}, 48, core::ExplorerWidth{}).border ==
               1,
           "the border is at least one pixel");
}

// 面の行と部品（#1390・採用案の表）。ルート名の行は 30 DIP、木の行は 22 DIP。丸い面は左右 6 DIP の
// 内側、山形の升は「丸い面の左 + 4 DIP + 深さ × 12 DIP」、印の升はその右、名前は 6 DIP
// 空けて丸い面の 右の余白の手前まで。未保存なら丸は丸い面の右端から 14 DIP の所を右端に 6
// DIP、名前は丸の 6 DIP 手前まで。縦の線は深さ level の山形の升の中央。
void verify_rows_and_parts()
{
    const auto layout =
        core::explorer_layout(LayoutRect{0, 0, 1200, 800}, 96, core::ExplorerWidth{});
    const auto top = panel_of(layout).top;
    expect(core::explorer_line(layout, 0) == LayoutRect{0, top, 239, top + 30} &&
               core::explorer_line(layout, 1) == LayoutRect{0, top + 30, 239, top + 52} &&
               core::explorer_line(layout, 3) == LayoutRect{0, top + 74, 239, top + 96},
           "the root line is 30 DIP and the rows 22 DIP below it, left of the border");
    expect(core::explorer_root_name(layout, core::explorer_line(layout, 0)) ==
               LayoutRect{12, top, 227, top + 30},
           "the root name keeps 12 DIP on both sides");
    const auto line = core::explorer_line(layout, 1);
    const auto parts = core::explorer_row_parts(layout, line, 0, false);
    expect(parts.surface == LayoutRect{6, top + 30, 233, top + 52} &&
               parts.chevron == LayoutRect{10, top + 33, 26, top + 49} &&
               parts.mark == LayoutRect{26, top + 33, 42, top + 49} &&
               parts.name == LayoutRect{48, top + 30, 229, top + 52} &&
               core::width_of(parts.dot) == 0,
           "a top level row lays out its surface, chevron, mark and name");
    const auto deep = core::explorer_row_parts(layout, line, 2, true);
    expect(deep.chevron.left == 34 && deep.mark.left == 50 && deep.name.left == 72 &&
               deep.dot == LayoutRect{213, top + 38, 219, top + 44} && deep.name.right == 207,
           "a deeper unsaved row moves by 12 DIP a level and ends its name before the dot");
    expect(core::explorer_guide_x(layout, line, 0) == 17 &&
               core::explorer_guide_x(layout, line, 1) == 29,
           "the guide of a level runs through the middle of that level's chevron cell");
    const auto scaled =
        core::explorer_layout(LayoutRect{0, 0, 1200, 800}, 144, core::ExplorerWidth{});
    const auto scaled_top = panel_of(scaled).top;
    const auto scaled_line = core::explorer_line(scaled, 2);
    const auto scaled_parts = core::explorer_row_parts(scaled, scaled_line, 1, true);
    expect(scaled_line == LayoutRect{0, scaled_top + 78, 358, scaled_top + 111} &&
               scaled_parts.surface == LayoutRect{9, scaled_top + 78, 349, scaled_top + 111} &&
               scaled_parts.chevron == LayoutRect{33, scaled_top + 82, 57, scaled_top + 106} &&
               scaled_parts.dot == LayoutRect{319, scaled_top + 90, 328, scaled_top + 99} &&
               core::explorer_guide_x(scaled, scaled_line, 0) == 26,
           "150 percent scales the rows and the parts");
    const auto narrow = core::explorer_row_parts(layout, line, 500, true);
    expect(narrow.chevron.left <= narrow.surface.right &&
               narrow.name.right <= narrow.surface.right &&
               narrow.dot.right <= narrow.surface.right && narrow.name.left <= narrow.name.right,
           "a very deep row stays inside its surface");
}

// 点の下の行（#1390）。ルート名の行は 0、その下は 22 DIP ごと。面の外は値なし。描く行と同じ寸法。
void verify_line_at()
{
    const auto layout =
        core::explorer_layout(LayoutRect{0, 0, 1200, 800}, 96, core::ExplorerWidth{});
    const auto top = panel_of(layout).top;
    expect(core::explorer_line_at(layout, 10, top) == std::optional<std::size_t>{0} &&
               core::explorer_line_at(layout, 10, top + 29) == std::optional<std::size_t>{0} &&
               core::explorer_line_at(layout, 10, top + 30) == std::optional<std::size_t>{1} &&
               core::explorer_line_at(layout, 10, top + 51) == std::optional<std::size_t>{1} &&
               core::explorer_line_at(layout, 10, top + 52) == std::optional<std::size_t>{2},
           "the line under the point follows the 30 DIP root line and the 22 DIP rows");
    bool round_trip = true;
    for (std::size_t index = 0; index < 20; ++index)
    {
        const auto line = core::explorer_line(layout, index);
        round_trip = round_trip &&
                     core::explorer_line_at(layout, 100, line.top) == std::optional{index} &&
                     core::explorer_line_at(layout, 100, line.bottom - 1) == std::optional{index};
    }
    expect(round_trip, "every drawn line is the line under its own pixels");
    const auto hidden = core::explorer_layout(LayoutRect{0, 0, 1200, 800}, 96, std::nullopt);
    expect(!core::explorer_line_at(layout, 240, top + 40).has_value() &&
               !core::explorer_line_at(layout, 10, top - 1).has_value() &&
               !core::explorer_line_at(hidden, 10, top + 40).has_value(),
           "outside the panel or with the panel hidden there is no line");
    const auto doubled =
        core::explorer_layout(LayoutRect{0, 0, 1200, 800}, 192, core::ExplorerWidth{});
    const auto doubled_top = panel_of(doubled).top;
    expect(core::explorer_line_at(doubled, 10, doubled_top + 59) == std::optional<std::size_t>{0} &&
               core::explorer_line_at(doubled, 10, doubled_top + 60 + 44) ==
                   std::optional<std::size_t>{2},
           "200 percent doubles the lines under the point");
}

// ファイルの印の色の役割（#1390・採用案の表）。言語は ADR 0154 の 1 本、C / C++ はヘッダを分ける。
void verify_mark_roles()
{
    using core::SyntaxRole;
    const auto role = [](std::string_view name) { return core::explorer_mark_role(name); };
    expect(role("main.cpp") == SyntaxRole::function && role("x.c") == SyntaxRole::function &&
               role("a.cc") == SyntaxRole::function && role("a.py") == SyntaxRole::function,
           "C and C++ sources and Python are the function token");
    expect(role("Layout.hpp") == SyntaxRole::type && role("x.h") == SyntaxRole::type &&
               role("X.HXX") == SyntaxRole::type && role("app.ts") == SyntaxRole::type &&
               role("app.jsx") == SyntaxRole::type,
           "C and C++ headers and JS / TS are the type token");
    expect(role("README.md") == SyntaxRole::string && role("a.json") == SyntaxRole::keyword &&
               role("a.yml") == SyntaxRole::keyword && role("a.toml") == SyntaxRole::keyword &&
               role("a.html") == SyntaxRole::operators && role("a.xml") == SyntaxRole::operators &&
               role("a.css") == SyntaxRole::operators,
           "Markdown, the data files and the markup files follow the table");
    expect(!role("notes.txt").has_value() && !role(".gitignore").has_value() &&
               !role("CMakeLists.txt").has_value() && !role("main.rs").has_value() &&
               !role("Makefile").has_value(),
           "plain and the other languages are muted");
    expect(core::header_of_path("C:\\proj\\a.hpp") && !core::header_of_path("a.ipp") &&
               !core::header_of_path("hpp") && !core::header_of_path("a.cpp"),
           "headers are the h, hh, hpp and hxx extensions only");
}

// 本文の枠は面の幅だけ右へ縮めた client から pane_layouts の 1 本で求める（決定 2・ADR 0148）。
// 面が消えていれば今と同じ枠、出ていれば帯の左端が面の右端で縦は面と同じ。2 枠も同じ形。
void verify_body_panes()
{
    const auto font = core::default_font_size();
    const LayoutRect client{0, 0, 1200, 800};
    const auto hidden = core::explorer_layout(client, 96, std::nullopt);
    const auto plain = core::pane_layouts(client, 96, font, core::SplitLayout::none);
    expect(core::pane_layouts(hidden.body, 96, font, core::SplitLayout::none).first.content ==
               plain.first.content,
           "a hidden explorer leaves the body pane as it was");
    const auto shown = core::explorer_layout(client, 96, core::ExplorerWidth{});
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
// 使う人が決めた幅（追補 D78）。最小 120 DIP・最大は client の 60% と「本文に 200 DIP」の小さい方。
// 窓を狭めたら描く幅だけ切り詰め、値なしは今までの式のまま。
void verify_chosen_width()
{
    const auto chosen = [](std::int32_t dips) { return core::ExplorerWidth{dips}; };
    expect(core::explorer_panel_width(1200, 96, core::ExplorerWidth{}) == 240 &&
               core::explorer_panel_width(500, 96, core::ExplorerWidth{}) == 200,
           "no chosen width keeps min(240 DIP, 40 percent)");
    expect(core::explorer_panel_width(1200, 96, chosen(300)) == 300,
           "a chosen width inside the range is drawn as it is");
    expect(core::explorer_panel_width(1200, 96, chosen(500)) == 500,
           "a chosen width may pass the 240 DIP default");
    expect(core::explorer_panel_width(1200, 96, chosen(900)) == 720,
           "a chosen width is capped at 60 percent of the client");
    expect(core::explorer_panel_width(400, 96, chosen(300)) == 200,
           "a narrow window keeps 200 DIP for the body before 60 percent");
    expect(core::explorer_panel_width(1200, 96, chosen(120)) == 120 &&
               core::explorer_panel_width(1200, 96, chosen(130)) == 130,
           "the minimum is 120 DIP");
    expect(core::explorer_panel_width(1200, 144, chosen(300)) == 450,
           "a chosen width is in DIP and scales with the DPI");
    expect(core::explorer_panel_width(1200, 144, chosen(600)) == 720,
           "the 60 percent cap is in physical pixels");
    expect(core::explorer_panel_width(300, 96, chosen(300)) == 100,
           "when the body reserve leaves less than the minimum the reserve wins");
    expect(core::explorer_panel_width(0, 96, chosen(300)) == 0,
           "an empty client gives an empty panel");
    const auto narrowed = core::explorer_layout(LayoutRect{0, 0, 600, 400}, 96, chosen(500));
    expect(core::width_of(panel_of(narrowed)) == 360 && narrowed.body.left == 360,
           "the layout draws the clamped width and the body starts after it");
    const auto widened = core::explorer_layout(LayoutRect{0, 0, 1600, 400}, 96, chosen(500));
    expect(core::width_of(panel_of(widened)) == 500,
           "widening the window again draws the remembered width");
}

// ドラッグの幅（追補 D78）。右端に置いた画素を同じ範囲に切り詰めて DIP へ。値は設定として読める。
void verify_dragged_width()
{
    expect(core::explorer_dragged_width(1200, 96, 333) == 333,
           "a drag inside the range keeps the pointer position");
    expect(core::explorer_dragged_width(1200, 96, 10) == 120 &&
               core::explorer_dragged_width(1200, 96, -50) == 120,
           "a drag past the left stops at 120 DIP");
    expect(core::explorer_dragged_width(1200, 96, 1100) == 720,
           "a drag past the right stops at 60 percent");
    expect(core::explorer_dragged_width(1200, 144, 450) == 300,
           "the dragged width is stored in DIP");
    expect(core::explorer_dragged_width(1200, 144, 100) == 120 &&
               core::explorer_panel_width(1200, 144, core::ExplorerWidth{120}) == 180,
           "the minimum at 150 percent round-trips to 120 DIP");
    expect(core::explorer_dragged_width(300, 96, 50) == 120,
           "even a window under the minimum stores a readable width");
    for (const std::int32_t pixels : {0, 150, 333, 719, 2000})
    {
        const auto dips = core::explorer_dragged_width(1200, 96, pixels);
        expect(core::explorer_width_of(std::to_string(dips)) == dips,
               "every dragged width reads back as a settings value");
    }
}

// 境の当たり（追補 D78）。右端の左右 3 DIP・縦は面の帯。面が消えていれば当たらない。
void verify_divider_hit()
{
    const LayoutRect client{0, 0, 1200, 800};
    const auto shown = core::explorer_layout(client, 96, core::ExplorerWidth{});
    const auto panel = panel_of(shown);
    const auto middle = (panel.top + panel.bottom) / 2;
    expect(core::explorer_divider_hit(shown, 96, 240, middle) &&
               core::explorer_divider_hit(shown, 96, 237, middle) &&
               core::explorer_divider_hit(shown, 96, 242, middle),
           "the divider takes 3 DIP on both sides of the panel's right edge");
    expect(!core::explorer_divider_hit(shown, 96, 236, middle) &&
               !core::explorer_divider_hit(shown, 96, 243, middle),
           "the panel rows and the body stay outside the divider");
    expect(!core::explorer_divider_hit(shown, 96, 240, panel.top - 1) &&
               !core::explorer_divider_hit(shown, 96, 240, panel.bottom),
           "the title band and the status bar are not the divider");
    const auto scaled = core::explorer_layout(client, 192, core::ExplorerWidth{});
    const auto right = panel_of(scaled).right;
    expect(core::explorer_divider_hit(scaled, 192, right - 6, panel_of(scaled).top) &&
               !core::explorer_divider_hit(scaled, 192, right - 7, panel_of(scaled).top),
           "the grab follows the DPI");
    expect(
        !core::explorer_divider_hit(core::explorer_layout(client, 96, std::nullopt), 96, 0, middle),
        "a hidden panel has no divider");
    expect(!core::explorer_divider_hit(
               core::explorer_layout(LayoutRect{0, 0, 0, 0}, 96, core::ExplorerWidth{}), 96, 0, 0),
           "an empty panel has no divider");
}

// 設定 explorer_width の値の読み（追補 D78）。範囲の外と形の違う値は値なし（既定の式）。
void verify_width_text()
{
    expect(core::explorer_width_of("120") == 120 && core::explorer_width_of("9999") == 9999 &&
               core::explorer_width_of("300") == 300,
           "decimal DIP widths in range read");
    for (const auto *text :
         {"", "0", "119", "10000", "0300", "-1", "+300", " 300", "300 ", "3e2", "abc", "30.5"})
    {
        expect(!core::explorer_width_of(text).has_value(),
               "an empty, malformed or out of range width means the default");
    }
}
} // namespace

void verify_explorer_layout_scope()
{
    verify_width_rule();
    verify_hidden_and_origin();
    verify_steps_and_dpi();
    verify_panel_lines();
    verify_rows_and_parts();
    verify_line_at();
    verify_mark_roles();
    verify_body_panes();
    verify_chosen_width();
    verify_dragged_width();
    verify_divider_hit();
    verify_width_text();
}
} // namespace nenenib::tests
