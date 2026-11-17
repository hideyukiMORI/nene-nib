// 契約 `--finder-layout`（#1785・ADR 0189 の段 P1 の決定 1〜4・8）。core: 置き方 × 窓の大きさの表・
// 入らない窓の落とし方・当たりの判定と描画が同じ矩形・切り替えの順・欄を持つ出どころと名前の表。
// application: 置き方の切り替えと設定の往復（変えたときだけ書く）・欄を持たない出どころ・開いている
// タブの窓（先頭から 40 行・未保存の中身・全文検索の当たりの前 7 行から）・まだ読んでいないタブを
// 読んだことにしない・開いていないファイルは見出しと 1 行の知らせ。Ex の `:set finderpreview=`。
#include "ActivateCommandChoice.hpp"
#include "Appearance.hpp"
#include "CancelCommand.hpp"
#include "CommandEdit.hpp"
#include "CommandPaletteView.hpp"
#include "CommandText.hpp"
#include "CycleFinderPreview.hpp"
#include "DevicePixels.hpp"
#include "EditCommand.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "ExResult.hpp"
#include "FilePath.hpp"
#include "FinderPreview.hpp"
#include "InsertText.hpp"
#include "LayoutRect.hpp"
#include "NewTab.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "OpenOperationList.hpp"
#include "OpenProjectSearch.hpp"
#include "OpenTabList.hpp"
#include "PaletteContent.hpp"
#include "PaletteLayout.hpp"
#include "PaletteScope.hpp"
#include "PreviewSnippet.hpp"
#include "ProjectHit.hpp"
#include "ProjectSearchBatch.hpp"
#include "ProjectSearchHit.hpp"
#include "ProjectSearchPhase.hpp"
#include "ProjectSearchProgress.hpp"
#include "Scopes.hpp"
#include "ScriptedProjectSearch.hpp"
#include "Session.hpp"
#include "SessionTab.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using core::FinderPreview;

// 置き方 × 窓の大きさの表（DIP の窓・96 と 144 DPI）。窓の幅 792 DIP が right の境（プレビュー欄
// 360 DIP）。below の境（当たり 3 行 ＋ プレビュー 6 行）は 96 DPI で 500 DIP で、欄ごとの丸めで
// DPI により 1〜2 画素動くので、表は境の上の 520 DIP と下の 499 DIP を置き、境そのものは
// verify_below_layout（96 DPI）と verify_below_boundary（120・144 DPI）が見る。
using DrawnRow = std::tuple<std::int32_t, std::int32_t, FinderPreview, FinderPreview>;
constexpr std::array<DrawnRow, 16> drawn_rows{{
    {1600, 1200, FinderPreview::automatic, FinderPreview::right},
    {1600, 1200, FinderPreview::right, FinderPreview::right},
    {1600, 1200, FinderPreview::below, FinderPreview::below},
    {1600, 1200, FinderPreview::off, FinderPreview::off},
    {792, 1200, FinderPreview::automatic, FinderPreview::right},
    {791, 1200, FinderPreview::automatic, FinderPreview::below},
    {791, 1200, FinderPreview::right, FinderPreview::below},
    {700, 520, FinderPreview::automatic, FinderPreview::below},
    {700, 499, FinderPreview::automatic, FinderPreview::off},
    {700, 499, FinderPreview::right, FinderPreview::off},
    {700, 499, FinderPreview::below, FinderPreview::off},
    {1600, 300, FinderPreview::automatic, FinderPreview::right},
    {1600, 300, FinderPreview::below, FinderPreview::off},
    {300, 1200, FinderPreview::below, FinderPreview::below},
    {0, 0, FinderPreview::automatic, FinderPreview::off},
    {0, 0, FinderPreview::off, FinderPreview::off},
}};

void verify_drawn_table()
{
    for (const std::uint32_t dpi : {96U, 144U})
    {
        for (const auto &[width, height, placement, drawn] : drawn_rows)
        {
            const auto shown = core::palette_preview_drawn(placement, core::to_pixels(width, dpi),
                                                           core::to_pixels(height, dpi), dpi);
            expect(shown == drawn, "each placement falls back to the form that fits the window");
            const auto layout =
                core::palette_layout(core::to_pixels(width, dpi), core::to_pixels(height, dpi), dpi,
                                     core::PaletteContent{3, placement});
            expect(layout.preview == drawn, "the layout carries the drawn form");
        }
    }
}

[[nodiscard]] bool inside(const core::LayoutRect &outer, const core::LayoutRect &inner)
{
    return inner.left >= outer.left && inner.top >= outer.top && inner.right <= outer.right &&
           inner.bottom <= outer.bottom;
}

void verify_right_layout()
{
    for (const std::uint32_t dpi : {96U, 144U})
    {
        const auto layout =
            core::palette_layout(core::to_pixels(1600, dpi), core::to_pixels(1200, dpi), dpi,
                                 core::PaletteContent{1, FinderPreview::right});
        expect(core::width_of(layout.panel) == core::to_pixels(1000, dpi) &&
                   core::width_of(layout.rows) == core::to_pixels(400, dpi) &&
                   layout.rows.left == layout.panel.left && layout.pane.left == layout.rows.right &&
                   layout.pane.right == layout.panel.right && layout.pane.top == layout.rows.top &&
                   layout.pane.bottom == layout.rows.bottom,
               "right: a 1000 DIP panel with the 400 DIP list on the left and the pane beside it");
        expect(layout.visible_rows == core::palette_row_limit &&
                   core::palette_window_rows(core::to_pixels(1600, dpi), core::to_pixels(1200, dpi),
                                             dpi, FinderPreview::right) == layout.visible_rows,
               "right: the list keeps every row the window holds whatever the count of choices");
        expect(core::height_of(layout.pane_header) == core::to_pixels(28, dpi) &&
                   layout.pane_body.top == layout.pane_header.bottom &&
                   layout.pane_body.bottom == layout.pane.bottom &&
                   core::width_of(layout.pane_toggle) == core::to_pixels(28, dpi) &&
                   layout.pane_toggle.right == layout.pane.right &&
                   inside(layout.pane_header, layout.pane_toggle),
               "right: a 28 DIP heading with the toggle at its right end above the body");
        expect(core::palette_preview_lines(layout, dpi) == 14,
               "right: the 320 DIP pane shows 14 lines of 19 DIP");
        const auto first = core::palette_preview_line(layout, dpi, 0);
        expect(first.top == layout.pane_body.top + core::to_pixels(6, dpi) &&
                   core::height_of(first) == core::to_pixels(19, dpi) &&
                   core::palette_preview_number(first, dpi).right ==
                       first.left + core::to_pixels(40, dpi) &&
                   core::palette_preview_text(first, dpi).left ==
                       first.left + core::to_pixels(54, dpi),
               "right: lines start 6 DIP down with a 40 DIP number and the text 14 DIP after it");
    }
}

void verify_below_layout()
{
    constexpr std::uint32_t dpi = 96;
    const auto layout =
        core::palette_layout(1600, 1200, dpi, core::PaletteContent{1, FinderPreview::below});
    expect(core::width_of(layout.panel) == 640 && layout.visible_rows == 5 &&
               core::height_of(layout.rows) == 5 * 40 && layout.pane.top == layout.rows.bottom &&
               layout.footer.top == layout.pane.bottom &&
               layout.panel.bottom == layout.footer.bottom &&
               core::palette_window_rows(1600, 1200, dpi, FinderPreview::below) == 5,
           "below: a 640 DIP panel with five rows and the pane under them above the footer");
    const auto lines = core::palette_preview_lines(layout, dpi);
    expect(lines >= 6 && lines <= core::finder_preview_line_limit,
           "below: the pane holds between 6 and 40 lines");
    const auto low =
        core::palette_layout(700, 500, dpi, core::PaletteContent{9, FinderPreview::below});
    expect(low.visible_rows == 3 && core::palette_preview_lines(low, dpi) == 6,
           "below: the lowest window that fits keeps three rows and six lines");
    const auto plain =
        core::palette_layout(1600, 1200, dpi, core::PaletteContent{2, FinderPreview::off});
    expect(plain.preview == FinderPreview::off && plain.visible_rows == 2 &&
               core::width_of(plain.pane) == 0 && core::width_of(plain.pane_toggle) == 0 &&
               core::width_of(plain.panel) == 640,
           "off: the panel stays as before with no pane");
}

// 当たりの判定と描画は同じ矩形を読む（決定 1）。行の上は行、欄と印の上は行ではない。
void verify_same_rectangles_at(std::uint32_t dpi)
{
    for (const auto placement : {FinderPreview::right, FinderPreview::below, FinderPreview::off})
    {
        const auto layout =
            core::palette_layout(core::to_pixels(1600, dpi), core::to_pixels(1200, dpi), dpi,
                                 core::PaletteContent{8, placement});
        for (std::size_t index = 0; index < layout.visible_rows; ++index)
        {
            const auto row = core::palette_row(layout, index);
            expect(core::palette_hit(layout, (row.left + row.right) / 2,
                                     (row.top + row.bottom) / 2) == index,
                   "a click on a drawn row hits that row");
        }
        const auto toggle = layout.pane_toggle;
        const auto x = (toggle.left + toggle.right) / 2;
        const auto y = (toggle.top + toggle.bottom) / 2;
        expect(core::palette_toggle_hit(layout, x, y) == (placement != FinderPreview::off) &&
                   !core::palette_hit(layout, x, y).has_value(),
               "the toggle in the heading is not a row and only exists with a pane");
        const auto body = layout.pane_body;
        expect(core::width_of(body) == 0 ||
                   (!core::palette_hit(layout, (body.left + body.right) / 2, body.top + 1) &&
                    !core::palette_toggle_hit(layout, (body.left + body.right) / 2, body.top + 1)),
               "the preview body is neither a row nor the toggle");
    }
}

// 96 DPI のほかに 125% と 150%（V-1792 の 7）。
void verify_same_rectangles()
{
    for (const std::uint32_t dpi : {96U, 120U, 144U})
    {
        verify_same_rectangles_at(dpi);
    }
}

// below の「入る」の判定と実際の配置は同じ丸めの式（V-1792 の 5）。125% と 150% で窓の高さを境の
// 前後に 1 画素ずつ動かし、below で描く窓は必ず当たり 3 行とプレビュー 6 行を持ち、境の 1 画素下は
// off に落ちる。
// below で描く最も低い窓の高さ（無ければ 0）。below で描く窓は必ず当たり 3 行とプレビュー 6 行。
[[nodiscard]] std::int32_t lowest_below(std::int32_t width, std::uint32_t dpi)
{
    std::int32_t least = 0;
    for (auto height = core::to_pixels(470, dpi); height <= core::to_pixels(530, dpi); ++height)
    {
        const auto layout =
            core::palette_layout(width, height, dpi, core::PaletteContent{3, FinderPreview::below});
        if (layout.preview == FinderPreview::below)
        {
            least = least == 0 ? height : least;
            expect(layout.visible_rows >= 3 && core::palette_preview_lines(layout, dpi) >= 6,
                   "a below layout always holds three rows and six preview lines");
        }
    }
    return least;
}

void verify_below_boundary()
{
    for (const std::uint32_t dpi : {120U, 144U})
    {
        const auto width = core::to_pixels(700, dpi);
        const auto least = lowest_below(width, dpi);
        expect(least > core::to_pixels(470, dpi) &&
                   core::palette_preview_drawn(FinderPreview::below, width, least - 1, dpi) ==
                       FinderPreview::off,
               "one pixel below the lowest below window falls back to off");
    }
}

void verify_cycle_and_names()
{
    using Cycle = std::pair<FinderPreview, FinderPreview>;
    constexpr std::array<Cycle, 4> cycles{{
        {FinderPreview::right, FinderPreview::below},
        {FinderPreview::below, FinderPreview::off},
        {FinderPreview::off, FinderPreview::right},
        {FinderPreview::automatic, FinderPreview::right},
    }};
    for (const auto &[drawn, next] : cycles)
    {
        expect(core::finder_preview_cycled(drawn) == next,
               "the cycle steps right, below, off from the drawn form");
    }
    for (const auto preview :
         {FinderPreview::automatic, FinderPreview::right, FinderPreview::below, FinderPreview::off})
    {
        expect(core::finder_preview_named(core::finder_preview_name(preview)) == preview,
               "the names round trip");
    }
    expect(core::finder_preview_name(FinderPreview::automatic) == "auto" &&
               !core::finder_preview_named("automatic").has_value() &&
               !core::finder_preview_named("left").has_value() &&
               !core::finder_preview_named("").has_value(),
           "only auto, right, below and off are names");
    using core::PaletteScope;
    for (const auto scope :
         {PaletteScope::files, PaletteScope::tabs, PaletteScope::bookmarks, PaletteScope::history,
          PaletteScope::folder, PaletteScope::project_hits})
    {
        expect(core::palette_previews(scope) &&
                   core::palette_placement(scope, FinderPreview::below) == FinderPreview::below,
               "the file sources carry the pane");
    }
    for (const auto scope : {PaletteScope::commands, PaletteScope::operations})
    {
        expect(!core::palette_previews(scope) &&
                   core::palette_placement(scope, FinderPreview::right) == FinderPreview::off,
               "settings and the operation list keep the plain panel");
    }
}

void verify_ex_option()
{
    const auto settings = core::default_editor_settings();
    expect(settings.finder_preview == FinderPreview::automatic, "the default is automatic");
    const auto set = core::evaluate_ex("set finderpreview=below", settings, core::Appearance::dark);
    expect(set && set.value().settings.has_value() &&
               set.value().settings.value_or(core::default_editor_settings()).finder_preview ==
                   FinderPreview::below &&
               set.value().message.text() == "finderpreview=below",
           ":set finderpreview=below changes the saved setting");
    const auto shorter = core::evaluate_ex("set fpv=right", settings, core::Appearance::dark);
    expect(shorter && shorter.value().settings.has_value() &&
               shorter.value().settings.value_or(core::default_editor_settings()).finder_preview ==
                   FinderPreview::right,
           "fpv is the short name");
    for (const auto question : {"set finderpreview?", "set finderpreview", "set fpv?"})
    {
        const auto asked = core::evaluate_ex(question, settings, core::Appearance::dark);
        expect(asked && !asked.value().settings.has_value() &&
                   asked.value().message.text() == "finderpreview=auto",
               "the question form answers the value and changes nothing");
    }
    auto changed = settings;
    changed.finder_preview = FinderPreview::off;
    const auto reset = core::evaluate_ex("set finderpreview&", changed, core::Appearance::dark);
    expect(reset && reset.value().settings.has_value() &&
               reset.value().settings.value_or(core::default_editor_settings()).finder_preview ==
                   FinderPreview::automatic,
           "finderpreview& goes back to auto");
    const auto wrong =
        core::evaluate_ex("set finderpreview=left", settings, core::Appearance::dark);
    expect(!wrong.has_value(), "an unknown value is refused");
}

// ---------------------------------------------------------------- application

[[nodiscard]] core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

// 1 行目から count 行目まで「line N」の本文。
[[nodiscard]] std::string numbered(std::size_t count)
{
    std::string text;
    for (std::size_t line = 1; line <= count; ++line)
    {
        text += "line " + std::to_string(line) + (line < count ? "\n" : "");
    }
    return text;
}

[[nodiscard]] std::optional<app::PreviewSnippet> preview_of(app::EditorController &controller)
{
    const auto frame = controller.frame();
    if (!frame.command_palette.has_value())
    {
        return std::nullopt;
    }
    return frame.command_palette.value().preview;
}

[[nodiscard]] FinderPreview placement_of(app::EditorController &controller)
{
    const auto frame = controller.frame();
    return frame.command_palette.has_value() ? frame.command_palette.value().placement
                                             : FinderPreview::off;
}

// 窓の大きさを届けた controller（1600 × 1200・96 DPI）。
void sized(app::EditorController &controller, std::int32_t width, std::int32_t height)
{
    applied(controller,
            app::VisibleLines{10, core::TextViewGeometry{600, 20, 96, 0}, width, height});
}

void verify_cycle_saves()
{
    Editing editing;
    auto &controller = editing.controller();
    sized(controller, 1600, 1200);
    applied(controller, app::OpenCommandPalette{});
    applied(controller, app::CancelCommand{});
    expect(editing.settings().writes() == 0, "opening and closing the palette writes nothing");
    applied(controller, app::OpenCommandPalette{});
    const std::array<FinderPreview, 4> order{FinderPreview::below, FinderPreview::off,
                                             FinderPreview::right, FinderPreview::below};
    std::size_t writes = 0;
    for (const auto next : order)
    {
        applied(controller, app::CycleFinderPreview{});
        ++writes;
        expect(editing.settings().writes() == writes && editing.settings().written().has_value() &&
                   editing.settings()
                           .written()
                           .value_or(core::default_editor_settings())
                           .finder_preview == next &&
                   placement_of(controller) == next,
               "each Ctrl+Shift+P steps right, below, off and is saved once");
    }
    // automatic は今描いている形の次。細い窓では below を描いているので次は off。
    Editing narrow;
    auto &small = narrow.controller();
    sized(small, 700, 1200);
    applied(small, app::OpenTabList{});
    applied(small, app::CycleFinderPreview{});
    expect(
        narrow.settings().written().has_value() &&
            narrow.settings().written().value_or(core::default_editor_settings()).finder_preview ==
                FinderPreview::off,
        "automatic on a narrow window steps from the drawn below to off");
    // 手で right を選んでいても、細い窓で below に落ちていれば次は off（見た目が必ず変わる）。
    auto chosen = core::default_editor_settings();
    chosen.finder_preview = FinderPreview::right;
    Editing manual(chosen);
    auto &hand = manual.controller();
    sized(hand, 700, 1200);
    applied(hand, app::OpenTabList{});
    applied(hand, app::CycleFinderPreview{});
    expect(manual.settings().written().value_or(core::default_editor_settings()).finder_preview ==
               FinderPreview::off,
           "a hand-picked right drawn as below on a narrow window steps to off");
}

// 選択を動かしても設定を書かない（V-1792 の 7）。automatic で窓に入らず off で描くときは欄の
// 表示値を作らない（V-1792 の 2）。
void verify_quiet_moves()
{
    Editing editing;
    auto &controller = editing.controller();
    sized(controller, 1600, 1200);
    applied(controller, app::NewTab{});
    applied(controller, app::NewTab{});
    applied(controller, app::OpenTabList{});
    for (const auto edit : {core::CommandEdit::complete_next, core::CommandEdit::complete_next,
                            core::CommandEdit::complete_previous})
    {
        applied(controller, app::EditCommand{edit});
    }
    expect(editing.settings().writes() == 0 && preview_of(controller).has_value(),
           "moving the selection shows previews and writes no settings");
    Editing small;
    auto &tiny = small.controller();
    sized(tiny, 640, 360);
    applied(tiny, app::OpenTabList{});
    expect(placement_of(tiny) == FinderPreview::automatic && !preview_of(tiny).has_value(),
           "a window that fits no pane draws off and makes no preview");
}

void verify_scope_placement()
{
    Editing editing;
    auto &controller = editing.controller();
    sized(controller, 1600, 1200);
    applied(controller, app::OpenTabList{});
    expect(placement_of(controller) == FinderPreview::automatic &&
               preview_of(controller).has_value(),
           "the tab list carries the pane with the chosen placement");
    applied(controller, app::CancelCommand{});
    applied(controller, app::OpenOperationList{});
    expect(placement_of(controller) == FinderPreview::off && !preview_of(controller).has_value(),
           "the operation list keeps the plain panel");
    applied(controller, app::CancelCommand{});
    applied(controller, app::OpenCommandPalette{});
    applied(controller, app::CommandText{":"});
    expect(placement_of(controller) == FinderPreview::off && !preview_of(controller).has_value(),
           "the settings source keeps the plain panel");
}

void verify_open_tab_window()
{
    Editing editing;
    auto &controller = editing.controller();
    sized(controller, 1600, 1200);
    editing.files().hold_at("C:\\work\\long.txt", Bytes{numbered(60)});
    applied(controller, app::OpenDocument{path_of("C:\\work\\long.txt")});
    applied(controller, app::NewTab{});
    applied(controller, app::InsertText{"draft\tone\x01x\nsecond"});
    const auto reads = editing.files().reads();
    applied(controller, app::OpenTabList{});
    const auto unsaved = preview_of(controller);
    expect(unsaved.has_value() && unsaved.value().lines.size() == 2 &&
               unsaved.value().lines.front().text == "draft\tone x" &&
               unsaved.value().lines.at(1).number.value == 2 && !unsaved.value().notice.has_value(),
           "the untitled tab shows its unsaved text with control characters as spaces");
    applied(controller, app::EditCommand{core::CommandEdit::complete_previous});
    const auto parked = preview_of(controller);
    expect(parked.has_value() && parked.value().name.text() == "long.txt" &&
               parked.value().place.has_value() &&
               parked.value().place.value().text() == "C:\\work" &&
               parked.value().lines.size() == core::finder_preview_line_limit &&
               parked.value().lines.front().number.value == 1 &&
               parked.value().lines.back().text == "line 40" && !parked.value().line.has_value() &&
               editing.files().reads() == reads,
           "a parked tab shows its first 40 lines from memory without reading the file");
}

void verify_long_line_window()
{
    Editing editing;
    auto &controller = editing.controller();
    sized(controller, 1600, 1200);
    // 511 バイトの ASCII の後ろに 3 バイトの字（512 バイト目で割れる）。
    applied(controller, app::InsertText{std::string(511, 'a') + "あ" + "tail"});
    applied(controller, app::OpenTabList{});
    const auto preview = preview_of(controller);
    expect(preview.has_value() && preview.value().lines.size() == 1 &&
               preview.value().lines.front().text == std::string(511, 'a'),
           "a line is cut at 512 bytes without splitting a code point");
}

[[nodiscard]] Editing rooted_editing()
{
    auto settings = core::default_editor_settings();
    settings.root = path_of("C:\\proj");
    return Editing(settings);
}

// 行 line の本文 text の行頭から 7 バイトの当たり。
[[nodiscard]] app::ProjectSearchHit hit_at(std::string_view file, std::size_t line)
{
    constexpr std::string_view text = "line 12";
    return app::ProjectSearchHit{path_of(file),
                                 core::ProjectHit{core::LineNumber{line}, core::Column{1},
                                                  std::string(text), 0, 0, text.size()}};
}

void verify_project_hit_window()
{
    Editing editing = rooted_editing();
    auto &controller = editing.controller();
    sized(controller, 1600, 1200);
    editing.files().hold_at("C:\\proj\\b.txt", Bytes{numbered(30)});
    applied(controller, app::OpenDocument{path_of("C:\\proj\\b.txt")});
    applied(controller, app::NewTab{});
    applied(controller, app::OpenProjectSearch{});
    applied(controller, app::CommandText{"line 12"});
    auto &port = editing.project_search();
    port.serve(
        app::ProjectSearchBatch{port.requests().back().ticket,
                                {hit_at("C:\\proj\\b.txt", 12), hit_at("C:\\proj\\far.txt", 3)},
                                {app::ProjectSearchPhase::complete, 2, 2}});
    applied(controller, app::WorkCompleted{});
    const auto open = preview_of(controller);
    expect(open.has_value() && open.value().line.has_value() &&
               open.value().lines.front().number.value == 5 &&
               open.value().lines.at(7).number.value == 12 && open.value().lines.at(7).hit &&
               open.value().lines.at(7).match_begin == 0 &&
               open.value().lines.at(7).match_end == 7 && !open.value().lines.at(6).hit,
           "a hit in an open tab starts the window seven lines above and marks the hit");
    applied(controller, app::EditCommand{core::CommandEdit::complete_next});
    const auto closed = preview_of(controller);
    expect(closed.has_value() && closed.value().name.text() == "far.txt" &&
               closed.value().lines.empty() && closed.value().notice.has_value() &&
               closed.value().line.has_value(),
           "a hit in a file that is not open shows its heading and one line without content");
}

void verify_unloaded_tab()
{
    using app::Session;
    using app::SessionTab;
    const core::TextPosition start{core::LineNumber{1}, core::Column{1}};
    const SessionReading listed{std::optional<Session>{
        Session{{SessionTab{path_of("C:\\work\\a.txt"), start, core::LineNumber{1}, 0},
                 SessionTab{path_of("C:\\work\\b.txt"), start, core::LineNumber{1}, 1}},
                0}}};
    Editing editing(listed,
                    [](ScriptedFiles &files)
                    {
                        files.hold_at("C:\\work\\a.txt", Bytes{"alpha"});
                        files.hold_at("C:\\work\\b.txt", Bytes{"beta"});
                    });
    auto &controller = editing.controller();
    sized(controller, 1600, 1200);
    const auto reads = editing.files().reads();
    applied(controller, app::OpenTabList{});
    applied(controller, app::EditCommand{core::CommandEdit::complete_next});
    const auto preview = preview_of(controller);
    expect(preview.has_value() && preview.value().name.text() == "b.txt" &&
               preview.value().lines.empty() && preview.value().notice.has_value() &&
               editing.files().reads() == reads,
           "a tab not read yet shows the heading only and is not read");
}
} // namespace

void verify_finder_layout_contracts()
{
    verify_drawn_table();
    verify_right_layout();
    verify_below_layout();
    verify_same_rectangles();
    verify_below_boundary();
    verify_cycle_and_names();
    verify_ex_option();
    verify_cycle_saves();
    verify_quiet_moves();
    verify_scope_placement();
    verify_open_tab_window();
    verify_long_line_window();
    verify_project_hit_window();
    verify_unloaded_tab();
}
} // namespace nenenib::tests
