// #997（ADR 0162 の決定 2・8・9）と採用案 B（#1390・施主決定 D81）: エクスプローラーの面の画素。
// WIC の bitmap（SoftwareTarget）に ExplorerPainter で描き、地（title_bar）・境界（焦点で
// accent・面の上端から下端までの直線）・本文の左上の直角の角（#1488）・選んだ行の丸い面
// （background・焦点で accent、無ければ panel_border の枠）・線の形の保持・載せた行（tab_hover・
// 選んだ行には重ねない）・ファイルの印の色（構文の色のトークン）・未保存の丸（accent）・アクティブの
// semibold・山形とフォルダの絵・字下げの縦の線・名前の右端・面が無いときの不変・行の文字組みの保持と、
// 上下の分割でも面の行が窓の全高に並ぶこと（#1125）を確かめる。窓・device は作らない。字の画素は
// 字体に依るので、Segoe UI が無い機械では測らない（QLT-013）。窓が送る寸法 window_lines（#1125）は
// 字体に依らないので、その前に必ず見る。
#include "DirectWriteFactory.hpp"
#include "EditorSettings.hpp"
#include "ExplorerLayout.hpp"
#include "ExplorerPainter.hpp"
#include "ExplorerPointer.hpp"
#include "ExplorerPointing.hpp"
#include "ExplorerRowView.hpp"
#include "ExplorerShapes.hpp"
#include "ExplorerText.hpp"
#include "ExplorerView.hpp"
#include "ExplorerWidth.hpp"
#include "FilePath.hpp"
#include "FontSize.hpp"
#include "LayoutRect.hpp"
#include "Palette.hpp"
#include "PaletteRowLayouts.hpp"
#include "PaneLayouts.hpp"
#include "PaneSide.hpp"
#include "SoftwareTarget.hpp"
#include "SplitLayout.hpp"
#include "SyntaxRole.hpp"
#include "Theme.hpp"
#include "WindowChecks.hpp"
#include "WindowLines.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nenenib::tests::ui
{
namespace
{
namespace core = nenenib::core;
namespace application = nenenib::application;
using nenenib::ui::win32::ExplorerPainter;
using nenenib::ui::win32::ExplorerShapes;
using nenenib::ui::win32::ExplorerText;
using nenenib::ui::win32::PaletteRowLayouts;
using Format = Microsoft::WRL::ComPtr<IDWriteTextFormat>;

// 面は bitmap の左上 240 × 80。ルート名の行（30）と行 2 つ（22 ずつ）と 3 つ目の行の上の 6 画素。
constexpr core::LayoutRect panel{0, 0, 240, 80};
constexpr std::uint32_t dpi = 96;

[[nodiscard]] core::ExplorerLayout layout_of(bool shown)
{
    auto layout =
        core::explorer_layout(core::LayoutRect{0, 0, 1300, 800}, dpi,
                              shown ? std::optional{core::ExplorerWidth{}} : std::nullopt);
    if (shown)
    {
        layout.panel = panel;
    }
    return layout;
}

[[nodiscard]] core::Theme theme_of(core::Appearance appearance)
{
    // テーマの名前は設定の文字列を借りるので、設定は試験の間ずっと生かす。
    static const auto settings = core::default_editor_settings();
    return core::selected_theme(settings, appearance);
}

// 選んだ行は 2 行目の adr（木の行 1・first_visible 0）。3 行目は隠し属性の長い名前。
[[nodiscard]] application::ExplorerView view_of(bool focused)
{
    std::vector<application::ExplorerRowView> rows{
        {"docs", 0, true, true, false, false},
        {"adr", 1, true, false, false, false},
        {std::string(80, 'w') + ".md", 1, false, false, true, false},
    };
    return application::ExplorerView{core::FilePath::parse("C:/work/NeNeNib").value(),
                                     std::move(rows),
                                     3,
                                     std::size_t{1},
                                     focused,
                                     0};
}

// ファイルの 2 行。README.md はアクティブ（string の印）、main.cpp は未保存（function の印）で
// マウスを載せた行。選んだ行は無い。
[[nodiscard]] application::ExplorerView files_of()
{
    std::vector<application::ExplorerRowView> rows{
        {"README.md", 0, false, false, false, false, core::SyntaxRole::string, true, false},
        {"main.cpp", 0, false, false, false, false, core::SyntaxRole::function, false, true},
    };
    application::ExplorerView view{core::FilePath::parse("C:/work/NeNeNib").value(),
                                   std::move(rows),
                                   2,
                                   std::nullopt,
                                   false,
                                   0};
    view.hovered = 1;
    return view;
}

[[nodiscard]] Format format_of(IDWriteFactory2 &factory, DWRITE_FONT_WEIGHT weight)
{
    Format format;
    if (FAILED(factory.CreateTextFormat(L"Segoe UI", nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                        DWRITE_FONT_STRETCH_NORMAL, 12.5F, L"",
                                        format.GetAddressOf())))
    {
        return nullptr;
    }
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    Microsoft::WRL::ComPtr<IDWriteInlineObject> ellipsis;
    const DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
    if (FAILED(factory.CreateEllipsisTrimmingSign(format.Get(), ellipsis.GetAddressOf())) ||
        FAILED(format->SetTrimming(&trimming, ellipsis.Get())))
    {
        return nullptr;
    }
    return format;
}

// 画素（BGRA・premultiplied・不透明）が色と同じか。
[[nodiscard]] bool pixel_is(const SoftwareTarget::Pixels &pixels, std::int32_t x, std::int32_t y,
                            core::RgbColor color)
{
    const auto at =
        ((static_cast<std::size_t>(y) * SoftwareTarget::width) + static_cast<std::size_t>(x)) * 4;
    return pixels.size() > at + 3 && pixels.at(at) == color.blue &&
           pixels.at(at + 1) == color.green && pixels.at(at + 2) == color.red;
}

// 矩形の中で色と違う画素（字の墨）の数。
[[nodiscard]] std::size_t ink_in(const SoftwareTarget::Pixels &pixels, const core::LayoutRect &area,
                                 core::RgbColor ground)
{
    std::size_t ink = 0;
    for (auto y = area.top; y < area.bottom; ++y)
    {
        for (auto x = area.left; x < area.right; ++x)
        {
            ink += pixel_is(pixels, x, y, ground) ? 0 : 1;
        }
    }
    return ink;
}

// 面を WIC の bitmap に描く口。テーマと保持を束ねて、描く面と配置だけを受ける。
// 保持は行の文字組みと線の形の組（renderer の 2 つの欄と同じ）。
using Held = std::pair<PaletteRowLayouts &, ExplorerShapes &>;

[[nodiscard]] auto painter_of(SoftwareTarget &target, const ExplorerText &text, Held held,
                              const core::Theme &theme)
{
    return [&target, text, held, theme](const application::ExplorerView &view,
                                        const core::ExplorerLayout &layout)
    {
        return target.painted(
            [&](ID2D1RenderTarget &surface, ID2D1SolidColorBrush *brush)
            {
                ExplorerPainter painter(surface, *brush, text, held.first, held.second);
                painter.paint(view, layout, theme.ui, theme.body);
            });
    };
}

void verify_panel_pixels(SoftwareTarget &target, const ExplorerText &text)
{
    const auto theme = theme_of(core::Appearance::dark);
    const auto &palette = theme.ui;
    expect(palette.title_bar != palette.background && palette.title_bar != palette.panel_border &&
               palette.title_bar != palette.tab_hover && palette.background != palette.tab_hover &&
               palette.panel_border != palette.accent,
           "the dark palette tells the panel tokens apart");
    const auto layout = layout_of(true);
    PaletteRowLayouts layouts(ExplorerPainter::retained);
    ExplorerShapes shapes;
    const auto painted = painter_of(target, text, Held{layouts, shapes}, theme);
    const auto pixels = painted(view_of(false), layout);
    expect(pixels.size() ==
               static_cast<std::size_t>(SoftwareTarget::width) * SoftwareTarget::height * 4,
           "the explorer panel is painted");
    const auto border_x = panel.right - layout.border;
    expect(pixel_is(pixels, border_x - 2, 2, palette.title_bar),
           "the panel ground is the title bar token");
    expect(pixel_is(pixels, border_x, 0, palette.panel_border) &&
               pixel_is(pixels, border_x, 3, palette.panel_border) &&
               pixel_is(pixels, border_x, 10, palette.panel_border) &&
               pixel_is(pixels, border_x, 70, palette.panel_border),
           "the border without focus is panel_border from the panel's top to its bottom");
    expect(pixel_is(pixels, 200, 63, palette.background) &&
               pixel_is(pixels, 3, 63, palette.title_bar) &&
               pixel_is(pixels, 236, 63, palette.title_bar),
           "the selected row is a background surface 6 DIP inside the panel");
    expect(pixel_is(pixels, 200, 41, palette.title_bar), "unselected rows keep the panel ground");
    // 本文の帯の左上の角は直角（#1488）。面の右には何も描かない（角の外の塗りも弧も無い）。
    expect(ink_in(pixels, core::LayoutRect{panel.right, 0, panel.right + 12, 12},
                  core::RgbColor{0, 0, 0}) == 0,
           "the body's top left corner is square and nothing is painted right of the panel");
    // 山形とフォルダの絵（docs は深さ 0・adr は深さ 1 で選んだ行の面の上）。
    expect(ink_in(pixels, core::LayoutRect{10, 33, 26, 49}, palette.title_bar) > 0 &&
               ink_in(pixels, core::LayoutRect{26, 33, 42, 49}, palette.title_bar) > 0,
           "an open folder shows its chevron and its folder");
    expect(ink_in(pixels, core::LayoutRect{22, 55, 38, 71}, palette.background) > 0,
           "a closed folder one level down shows its chevron after the indent");
    expect(pixel_is(pixels, 17, 63, palette.panel_border) &&
               ink_in(pixels, core::LayoutRect{11, 56, 16, 70}, palette.background) == 0,
           "the second level has a panel_border guide and otherwise an empty indent");
    expect(ink_in(pixels, core::LayoutRect{12, 0, 120, 30}, palette.title_bar) > 0,
           "the root name is on the first line");
    // 字の文字組みは保持から引く（ルート・名前 3 つ）。2 回目の描画で増えない。
    const auto held = layouts.size();
    expect(held == 4, "every visible text is held once");
    static_cast<void>(painted(view_of(false), layout));
    expect(layouts.size() == held, "the same panel in the next frame creates no layout");
    // 長い名前は丸い面の右の余白の手前で切れ、その先と境界の手前には墨が無い。
    application::ExplorerView longer = view_of(false);
    longer.rows = {{std::string(80, 'w') + ".md", 1, false, false, true, false}};
    longer.selected = std::nullopt;
    const auto cut = painted(longer, layout);
    expect(ink_in(cut, core::LayoutRect{189, 30, 229, 52}, palette.title_bar) > 0,
           "a long name runs up to the right padding");
    expect(ink_in(cut, core::LayoutRect{229, 30, border_x, 52}, palette.title_bar) == 0,
           "a long name is cut before the right padding");
}

// 焦点の無いときの選んだ行の panel_border の枠（V-1428 の F-1・背景が黒のテーマでは面の地と本文の
// 色が同じなので、枠が無いと見えない）と、線の形の保持（L-1・作り直すのは factory が変わったとき
// だけ・面が動いても作り直さない）。
void verify_unfocused_frame_and_shapes(SoftwareTarget &target, const ExplorerText &text)
{
    const auto theme = theme_of(core::Appearance::dark);
    const auto &palette = theme.ui;
    const auto layout = layout_of(true);
    PaletteRowLayouts layouts(ExplorerPainter::retained);
    ExplorerShapes shapes;
    const auto painted = painter_of(target, text, Held{layouts, shapes}, theme);
    const auto pixels = painted(view_of(false), layout);
    expect(palette.panel_border != palette.background &&
               pixel_is(pixels, 120, 52, palette.panel_border) &&
               pixel_is(pixels, 120, 63, palette.background),
           "the selected row without focus has a 1 DIP panel_border frame over its surface");
    expect(shapes.made() == 1, "the stroke style is made for the first frame");
    static_cast<void>(painted(view_of(true), layout));
    expect(shapes.made() == 1, "the next frame on the same panel makes no shape");
    auto wider = layout;
    wider.panel = core::LayoutRect{0, 0, 260, 80};
    const auto moved = painted(view_of(false), wider);
    expect(shapes.made() == 1 && pixel_is(moved, 260 - layout.border, 0, palette.panel_border) &&
               pixel_is(moved, 260 - layout.border, 10, palette.panel_border),
           "a moved panel edge makes no shape and paints the straight border there");
}

void verify_focus_and_hidden(SoftwareTarget &target, const ExplorerText &text)
{
    const auto theme = theme_of(core::Appearance::light);
    const auto &palette = theme.ui;
    const auto layout = layout_of(true);
    PaletteRowLayouts layouts(ExplorerPainter::retained);
    ExplorerShapes shapes;
    const auto painted = painter_of(target, text, Held{layouts, shapes}, theme);
    const auto focused = painted(view_of(true), layout);
    const auto border_x = panel.right - layout.border;
    expect(pixel_is(focused, border_x, 10, palette.accent) &&
               pixel_is(focused, border_x, 70, palette.accent),
           "the border with focus is the accent");
    expect(pixel_is(focused, 120, 52, palette.accent) &&
               pixel_is(focused, 120, 63, palette.background),
           "the selected row with focus has a 1 DIP accent frame over its surface");
    // 面が無い frame は何も描かない（bitmap は黒のまま）。
    const auto hidden = painted(view_of(true), layout_of(false));
    expect(ink_in(hidden, core::LayoutRect{0, 0, 300, 80}, core::RgbColor{0, 0, 0}) == 0,
           "a frame without the panel leaves every pixel as it was");
    // ルートの無い面は案内の 1 行だけ。
    auto rootless = view_of(false);
    rootless.root = std::nullopt;
    rootless.rows.clear();
    rootless.selected = std::nullopt;
    const auto guide = painted(rootless, layout);
    expect(ink_in(guide, core::LayoutRect{12, 0, border_x, 30}, palette.title_bar) > 0 &&
               ink_in(guide, core::LayoutRect{0, 30, border_x, 80}, palette.title_bar) == 0,
           "a panel without a root shows the guide line only");
}

// ファイルの行（#1390）。印の色は行の mark の構文の色（値なしは muted）、載せた行は tab_hover の
// 丸い面（選んだ行には重ねない）、未保存は accent の丸、アクティブは semibold（墨が増える）、
// ファイルの行は山形の升を空ける。
void verify_file_rows(SoftwareTarget &target, const ExplorerText &text)
{
    const auto theme = theme_of(core::Appearance::dark);
    const auto &palette = theme.ui;
    expect(theme.body.string != theme.body.function && theme.body.string != palette.muted &&
               palette.tab_hover != palette.title_bar,
           "the dark theme tells the mark tokens apart");
    const auto layout = layout_of(true);
    PaletteRowLayouts layouts(ExplorerPainter::retained);
    ExplorerShapes shapes;
    const auto painted = painter_of(target, text, Held{layouts, shapes}, theme);
    const auto pixels = painted(files_of(), layout);
    expect(pixel_is(pixels, 34, 41, theme.body.string) &&
               pixel_is(pixels, 34, 63, theme.body.function),
           "the file marks are the syntax tokens of their languages");
    expect(ink_in(pixels, core::LayoutRect{10, 33, 26, 49}, palette.title_bar) == 0,
           "a file leaves the chevron cell empty");
    expect(pixel_is(pixels, 150, 63, palette.tab_hover) &&
               pixel_is(pixels, 150, 41, palette.title_bar),
           "the hovered row is a tab_hover surface");
    expect(pixel_is(pixels, 216, 63, palette.accent) &&
               pixel_is(pixels, 216, 41, palette.title_bar),
           "an unsaved file has the accent dot at the right of its row");
    auto plain = files_of();
    plain.rows.at(0).active = false;
    plain.rows.at(0).mark = std::nullopt;
    const auto normal = painted(plain, layout);
    const core::LayoutRect readme{48, 30, 229, 52};
    expect(ink_in(pixels, readme, palette.title_bar) > ink_in(normal, readme, palette.title_bar),
           "the active document's name is semibold");
    expect(pixel_is(normal, 34, 41, palette.muted), "a file without a mark role is muted");
    auto selected = files_of();
    selected.selected = 1;
    const auto both = painted(selected, layout);
    expect(pixel_is(both, 150, 63, palette.background),
           "the hover surface is not painted over the selected row");
}

// 上下の分割（ADR 0148）でも面の行数は client の高さから導く（#1125・ADR 0162 の追補）。面の
// 行数は explorer_panel_lines − 1（ルート名の行・application の --explorer の契約が同じ規則を
// 固定する）で、操作中の枠の見えている行数（本文の半分の高さ）では導かない。bitmap は 80 画素の
// 高さなので、面の下端を bitmap の下端へ平行移動して、面の最後の行に名前の墨があることを見る。
void verify_split_rows(SoftwareTarget &target, const ExplorerText &text)
{
    const auto theme = theme_of(core::Appearance::dark);
    const core::LayoutRect client{0, 0, 1300, 900};
    const auto font = core::default_font_size();
    const auto layout = core::explorer_layout(client, dpi, core::ExplorerWidth{});
    const auto lines = core::explorer_panel_lines(client, dpi);
    const auto split = core::active_pane_layout(
        core::pane_layouts(layout.body, dpi, font, core::SplitLayout::horizontal),
        core::PaneSide::second);
    expect(layout.panel.has_value() && lines >= 4, "the explorer panel has its lines");
    if (!layout.panel.has_value() || lines < 4)
    {
        return;
    }
    const auto panel_area = layout.panel.value();
    expect(lines ==
               1 + static_cast<std::size_t>((core::height_of(panel_area) - layout.root_height) /
                                            layout.line_height),
           "the panel's lines are the root line and the rest of the band over the row height");
    expect(split.visible_lines < lines - 1,
           "a horizontal split halves the body lines but not the panel's");
    std::vector<application::ExplorerRowView> rows;
    for (std::size_t index = 0; index + 1 < lines; ++index)
    {
        rows.push_back({std::format("file-{:02}.txt", index), 0, false, false, false, false});
    }
    const application::ExplorerView view{core::FilePath::parse("C:/proj").value(),
                                         std::move(rows),
                                         lines - 1,
                                         std::nullopt,
                                         false,
                                         0};
    PaletteRowLayouts layouts(ExplorerPainter::retained);
    ExplorerShapes shapes;
    const auto shift =
        static_cast<float>(panel_area.bottom) - static_cast<float>(SoftwareTarget::height);
    const auto pixels = target.painted(
        [&](ID2D1RenderTarget &surface, ID2D1SolidColorBrush *brush)
        {
            surface.SetTransform(D2D1::Matrix3x2F::Translation(0.0F, -shift));
            ExplorerPainter painter(surface, *brush, text, layouts, shapes);
            painter.paint(view, layout, theme.ui, theme.body);
            surface.SetTransform(D2D1::Matrix3x2F::Identity());
        });
    // bitmap の上の面の最後の行の名前の欄。
    const auto last_line = core::explorer_line(layout, lines - 1);
    const auto name = core::explorer_row_parts(layout, last_line, 0, false).name;
    const auto lifted = static_cast<std::int32_t>(shift);
    const core::LayoutRect last{name.left, last_line.top - lifted, name.left + 80,
                                last_line.bottom - lifted};
    expect(last.top >= 0 && last.bottom <= static_cast<std::int32_t>(SoftwareTarget::height),
           "the panel's last line is inside the bitmap");
    expect(ink_in(pixels, last, theme.ui.title_bar) > 0,
           "the panel's rows run down to the last line of the window's band");
}

// 窓が送る寸法（EditorWindow::body_view の window_lines・#1125）。上下の分割の操作中の枠の
// body を渡しても、面の行数は窓の client の高さから数えた explorer_panel_lines で、0 のまま
// 送らない。窓の高さを変えれば面の行数も変わる。面の行数は本文の字の大きさに依らない（#1390）。
void verify_window_lines()
{
    const auto lines_at = [&](std::int32_t height, core::FontSize font)
    {
        const core::LayoutRect client{0, 0, 1300, height};
        const auto body = core::active_pane_layout(
            core::pane_layouts(core::explorer_layout(client, dpi, core::ExplorerWidth{}).body, dpi,
                               font, core::SplitLayout::horizontal),
            core::PaneSide::first);
        const auto sent = nenenib::ui::win32::window_lines(body, client, dpi);
        expect(sent.lines == body.visible_lines && sent.window_height == height,
               "the window sends the split pane's body lines and its client height");
        expect(sent.explorer_lines != 0 &&
                   sent.explorer_lines == core::explorer_panel_lines(client, dpi),
               "the window sends the panel's lines counted from the client height");
        expect(sent.explorer_lines > sent.lines + 1,
               "the panel's lines are not the split pane's lines");
        return sent.explorer_lines;
    };
    const auto font = core::default_font_size();
    expect(lines_at(600, font) < lines_at(900, font), "a taller window sends more panel lines");
    expect(lines_at(900, core::FontSize::from_points(8.0F).value()) == lines_at(900, font),
           "the panel's lines do not follow the body font size");
}

// 面の幅のドラッグ（ADR 0162 の追補 D78）。窓の境の当たりは面の右端の境界の画素と本文の左端の
// 画素を含み、面の行の当たり（panel の中）と重なる（窓は境を面の行より先に引く・押した要素と離した
// 要素が面の行の規則に混ざらない）。描く面は使う人が決めた幅（切り詰めた後）で、境界はその右端に
// 描かれる。
void verify_chosen_width_pixels(SoftwareTarget &target, const ExplorerText &text)
{
    const auto theme = theme_of(core::Appearance::dark);
    auto layout =
        core::explorer_layout(core::LayoutRect{0, 0, 1300, 800}, dpi, core::ExplorerWidth{400});
    expect(layout.panel.has_value() && core::width_of(layout.panel.value()) == 400 &&
               layout.body.left == 400,
           "a chosen 400 DIP width lays the panel out at 400 pixels at 96 DPI");
    layout.panel = core::LayoutRect{0, 0, 400, 80};
    const auto border_x = 400 - layout.border;
    expect(core::explorer_divider_hit(layout, dpi, border_x, 10) &&
               core::explorer_divider_hit(layout, dpi, 400, 10) &&
               core::contains(layout.panel.value(), border_x, 10),
           "the divider covers the border pixel, which is also inside the panel rows");
    expect(!core::explorer_divider_hit(layout, dpi, 300, 10) &&
               core::contains(layout.panel.value(), 300, 10),
           "a row away from the edge is not the divider");
    PaletteRowLayouts layouts(ExplorerPainter::retained);
    ExplorerShapes shapes;
    const auto painted = painter_of(target, text, Held{layouts, shapes}, theme);
    const auto pixels = painted(view_of(false), layout);
    expect(pixel_is(pixels, border_x, 10, theme.ui.panel_border) &&
               pixel_is(pixels, border_x - 2, 41, theme.ui.title_bar) &&
               pixel_is(pixels, 404, 30, core::RgbColor{0, 0, 0}),
           "the panel is painted at the chosen width with its border at the right edge");
}
// 面の行の上の左ボタン（ADR 0162 の決定 4 の改め・#1388）。窓は面の上の速い 2 回の押しを、OS の
// 「押す → 離す → DBLCLK → 離す」のまま pressed → released → double_click → released と送る。
// 押していない離しは送らない。2 打目の意味（読まない・開閉しない）は application が決める。
// 契約は --explorer の verify_double_click_reads_once と verify_folder_double_click_toggles_once。
void verify_click_messages()
{
    using application::ExplorerPointing;
    using nenenib::ui::win32::explorer_pointing_of;
    using nenenib::ui::win32::explorer_pressed_after;
    constexpr std::array<UINT, 4> twice{WM_LBUTTONDOWN, WM_LBUTTONUP, WM_LBUTTONDBLCLK,
                                        WM_LBUTTONUP};
    constexpr std::array<ExplorerPointing, 4> sent{
        ExplorerPointing::pressed, ExplorerPointing::released, ExplorerPointing::double_click,
        ExplorerPointing::released};
    bool pressed = false;
    std::size_t matched = 0;
    for (std::size_t at = 0; at < twice.size(); ++at)
    {
        const auto pointing = explorer_pointing_of(twice.at(at), pressed);
        if (pointing.has_value() && pointing.value() == sent.at(at))
        {
            ++matched;
            pressed = explorer_pressed_after(pointing.value());
        }
    }
    expect(matched == twice.size() && !pressed,
           "a double click on the panel sends press, release, double click and release");
    expect(!explorer_pointing_of(WM_LBUTTONUP, false).has_value(),
           "a release without a press on the panel sends nothing");
    expect(!explorer_pointing_of(WM_RBUTTONDOWN, false).has_value() &&
               !explorer_pointing_of(WM_MOUSEMOVE, true).has_value() &&
               !explorer_pointing_of(WM_MBUTTONDBLCLK, false).has_value(),
           "other buttons and moves are not presses on the panel rows");
}
} // namespace

void verify_explorer_paint()
{
    verify_window_lines();
    verify_click_messages();
    SoftwareTarget target;
    const auto factory = directwrite_factory();
    if (!target.ready() || factory == nullptr)
    {
        unmeasured("the explorer panel needs WIC and DirectWrite");
        return;
    }
    if (!installed(*factory.Get(), L"Segoe UI"))
    {
        unmeasured("the explorer panel pixels need Segoe UI");
        return;
    }
    const auto format = format_of(*factory.Get(), DWRITE_FONT_WEIGHT_NORMAL);
    const auto strong = format_of(*factory.Get(), DWRITE_FONT_WEIGHT_SEMI_BOLD);
    expect(format != nullptr && strong != nullptr, "the explorer name formats are created");
    if (format == nullptr || strong == nullptr)
    {
        return;
    }
    const ExplorerText text{factory.Get(), format.Get(), strong.Get(), dpi};
    verify_panel_pixels(target, text);
    verify_unfocused_frame_and_shapes(target, text);
    verify_focus_and_hidden(target, text);
    verify_file_rows(target, text);
    verify_split_rows(target, text);
    verify_chosen_width_pixels(target, text);
}
} // namespace nenenib::tests::ui
