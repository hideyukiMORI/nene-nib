// scope `--minimap` の単体テスト（ADR 0171・施主決定 D70・Issue #1017 の段 1）。core の 1 行の塊
// （空白・Tab・全角・構文の区間・100 桁）・面の最上段の行・窓・帯の合わせ・面の配置・設定と Ex の
// `set minimap`、面の上の点は本文の枠の外、application の frame（オフは値なし・オンは窓の範囲と
// 塊・編集の帯・Enter の帯・ブロックコメントの途中の編集・undo / redo の帯・4 KiB を超える行・
// 折り返す行の見えている範囲・全面プレビューの間は出さない・窓より上の編集は窓ごと・別の文書は
// 窓ごと）。時計・ファイル・乱数の種を読まない。
#include "Appearance.hpp"
#include "BuiltinThemes.hpp"
#include "CancelSelection.hpp"
#include "ColorSpan.hpp"
#include "Column.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorIntent.hpp"
#include "EditorSettings.hpp"
#include "ExResult.hpp"
#include "FilePath.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "LayoutRect.hpp"
#include "LineNumber.hpp"
#include "Minimap.hpp"
#include "MinimapBand.hpp"
#include "MinimapBlock.hpp"
#include "MinimapFrame.hpp"
#include "MinimapLayout.hpp"
#include "MinimapWindow.hpp"
#include "OpenDocument.hpp"
#include "PaneLayouts.hpp"
#include "PaneSide.hpp"
#include "PlaceCaret.hpp"
#include "PreviewPlacement.hpp"
#include "RgbColor.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SetPreviewPlacement.hpp"
#include "SplitLayout.hpp"
#include "SyntaxRole.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using core::LineNumber;
using core::MinimapBlock;
using core::SyntaxRole;

constexpr std::string_view code_path = "C:\\work\\minimap.cpp";
constexpr std::string_view other_path = "C:\\work\\other.txt";
constexpr std::string_view markdown_path = "C:\\work\\minimap.md";

[[nodiscard]] MinimapBlock muted(std::uint16_t left, std::uint16_t width)
{
    return MinimapBlock{left, width, std::nullopt};
}

[[nodiscard]] MinimapBlock role(std::uint16_t left, std::uint16_t width, SyntaxRole value)
{
    return MinimapBlock{left, width, value};
}

// 空白は塊にならず、Tab は次の 8 の倍数まで、全角は 2 桁、隣り合う同じ役割は 1 つ（決定 3）。
void verify_row_blocks()
{
    expect(core::minimap_row("ab  cd", {}) == core::MinimapRow{muted(0, 2), muted(4, 2)},
           "spaces split the blocks and are not drawn");
    expect(core::minimap_row("\tx", {}) == core::MinimapRow{muted(8, 1)},
           "a tab advances to the next multiple of 8");
    expect(core::minimap_row("ab\tc", {}) == core::MinimapRow{muted(0, 2), muted(8, 1)},
           "a tab after two characters still stops at column 8");
    expect(core::minimap_row("あい　う", {}) == core::MinimapRow{muted(0, 4), muted(6, 2)},
           "wide characters take 2 columns and the ideographic space is blank");
    const std::vector<core::ColorSpan> spans{
        core::ColorSpan{core::Column{1}, core::Column{4}, SyntaxRole::keyword},
        core::ColorSpan{core::Column{7}, core::Column{8}, SyntaxRole::number}};
    expect(core::minimap_row("int x=1;", spans) ==
               core::MinimapRow{role(0, 3, SyntaxRole::keyword), muted(4, 2),
                                role(6, 1, SyntaxRole::number), muted(7, 1)},
           "the colour spans give the roles and the rest is muted");
    expect(core::minimap_row(std::string(150, 'a'), {}) ==
               core::MinimapRow{muted(0, static_cast<std::uint16_t>(core::minimap_columns))},
           "a long line is cut at 100 columns");
    expect(core::minimap_row("", {}).empty() && core::minimap_row(" \t ", {}).empty(),
           "an empty or blank line has no blocks");
    // 結合文字は幅 0 で塊にならず、前の文字に桁を足さない（column_advance の表の 1 本）。
    expect(core::minimap_row("e\u0301x", {}) == core::MinimapRow{muted(0, 2)},
           "a combining mark takes no column");
    // 制御文字は描く形 `^M` の 2 桁（ADR 0040・同じ表）。
    expect(core::minimap_row("a\rb", {}) == core::MinimapRow{muted(0, 4)},
           "a control character takes the two columns of its ^M form");
    expect(core::minimap_row("\r\tx", {}) == core::MinimapRow{muted(0, 2), muted(8, 1)},
           "a tab after a control character stops at column 8");
}

// 面の最上段は比例の送りで、文書が面に収まれば 1 行目。最後の画面では文書の最後の行が面の
// 最下段に来る。
void verify_top()
{
    expect(core::minimap_top(LineNumber{30}, 20, 50, 100) == LineNumber{1},
           "a document that fits the panel starts at line 1");
    expect(core::minimap_top(LineNumber{1}, 20, 1000, 100) == LineNumber{1},
           "the first screen starts the panel at line 1");
    expect(core::minimap_top(LineNumber{981}, 20, 1000, 100) == LineNumber{901},
           "the last screen puts the last line at the bottom of the panel");
    expect(core::minimap_top(LineNumber{491}, 20, 1000, 100) == LineNumber{451},
           "the middle screen is scrolled in proportion");
}

// 窓は最上段の周りの前後 2,000 行（端では寄せる）で、含んでいれば前の窓のまま（決定 4）。
void verify_window()
{
    const auto built = core::minimap_window(std::nullopt, LineNumber{3000}, 100, 10000);
    expect(built == core::MinimapWindow{LineNumber{1050}, 4000},
           "the window is 2,000 lines before and after the middle of the panel");
    expect(core::minimap_window(built, LineNumber{3500}, 100, 10000) == built,
           "a panel inside the held window keeps it");
    expect(core::minimap_window(built, LineNumber{5000}, 100, 10000) ==
               core::MinimapWindow{LineNumber{3050}, 4000},
           "a panel outside the held window builds a new one");
    expect(core::minimap_window(std::nullopt, LineNumber{9990}, 100, 10000) ==
               core::MinimapWindow{LineNumber{6001}, 4000},
           "the window is moved inside the document at its end");
    expect(core::minimap_window(std::nullopt, LineNumber{1}, 100, 50) ==
               core::MinimapWindow{LineNumber{1}, 50},
           "a short document is the whole window");
    expect(core::minimap_window(core::MinimapWindow{LineNumber{1}, 60}, LineNumber{1}, 100, 50) ==
               core::MinimapWindow{LineNumber{1}, 50},
           "a held window longer than the document is built again");
}

// 2 つの帯を合わせた帯の外は前の行がずれただけ。
void verify_band_join()
{
    const core::MinimapBand typed{5, 1, 1};
    expect(core::minimap_band_joined(typed, core::MinimapBand{5, 1, 1}) == typed,
           "typing twice on the same row is one row");
    expect(core::minimap_band_joined(core::MinimapBand{5, 1, 2}, core::MinimapBand{6, 1, 2}) ==
               core::MinimapBand{5, 1, 3},
           "two Enters in a row grow one band");
    expect(core::minimap_band_joined(core::MinimapBand{10, 1, 1}, core::MinimapBand{2, 2, 1}) ==
               core::MinimapBand{2, 9, 8},
           "an earlier join covers the rows between the two edits");
}

// 面は client の右端の 80 DIP、出ていなければ幅 0（決定 2）。
void verify_layout()
{
    const core::LayoutRect client{0, 0, 1000, 700};
    const auto hidden =
        core::minimap_layout(client, 96, core::default_editor_settings().font_size, false);
    expect(hidden.body == client && core::width_of(hidden.panel) == 0,
           "a hidden minimap keeps the client for the body");
    const auto shown =
        core::minimap_layout(client, 96, core::default_editor_settings().font_size, true);
    expect(shown.panel.left == 920 && shown.panel.right == 1000 && shown.body.right == 920 &&
               shown.panel.top > 0 && shown.panel.bottom < 700 &&
               shown.rows == static_cast<std::size_t>(core::height_of(shown.panel) / 2),
           "the minimap takes 80 DIP on the right of the body band");
    const auto doubled =
        core::minimap_layout(client, 192, core::default_editor_settings().font_size, true);
    expect(core::width_of(doubled.panel) == 160, "the width follows the DPI");
}

using PanelPoints = std::array<std::pair<std::int32_t, std::int32_t>, 5>;

// 枠 band に points のどれも入らないか。
[[nodiscard]] bool outside_band(const core::LayoutRect &band, const PanelPoints &points)
{
    return std::ranges::none_of(points, [&band](const auto &point)
                                { return core::contains(band, point.first, point.second); });
}

// 面の上の点はどの本文の枠の band にも入らない（窓の click_body は band の中だけで place_caret
// するので、面の上の押下はキャレットも選択も動かさない・段 1・レビュー L4）。2 枠のどちらの側でも。
void verify_panel_press_outside_body()
{
    const core::LayoutRect client{0, 0, 1000, 700};
    const auto font = core::default_editor_settings().font_size;
    const auto shown = core::minimap_layout(client, 96, font, true);
    const auto &panel = shown.panel;
    const PanelPoints points{{{panel.left, panel.top},
                              {panel.right - 1, panel.top},
                              {panel.left, panel.bottom - 1},
                              {panel.right - 1, panel.bottom - 1},
                              {(panel.left + panel.right) / 2, (panel.top + panel.bottom) / 2}}};
    bool outside = core::width_of(panel) > 0;
    for (const auto layout :
         {core::SplitLayout::none, core::SplitLayout::vertical, core::SplitLayout::horizontal})
    {
        const auto panes = core::pane_layouts(shown.body, 96, font, layout);
        for (const auto side : {core::PaneSide::first, core::PaneSide::second})
        {
            outside = outside && outside_band(core::active_pane_layout(panes, side).band, points);
        }
    }
    expect(outside, "a press on the minimap panel is outside every body band");
}

// Ex の答えの設定と文言（設定が無い・失敗は既定の設定と空の文言）。
[[nodiscard]] core::EditorSettings
changed_settings(const std::expected<core::ExResult, core::ExEvaluationFailure> &result)
{
    if (!result.has_value())
    {
        return core::default_editor_settings();
    }
    return result.value().settings.value_or(core::default_editor_settings());
}

[[nodiscard]] bool
changes_settings(const std::expected<core::ExResult, core::ExEvaluationFailure> &result)
{
    return result.has_value() && result.value().settings.has_value();
}

[[nodiscard]] std::string
message_of(const std::expected<core::ExResult, core::ExEvaluationFailure> &result)
{
    return result.has_value() ? std::string(result.value().message.text()) : std::string{};
}

// frame のミニマップ（無ければ行の塊の無い値）。
[[nodiscard]] app::MinimapFrame minimap_of(const app::EditorFrame &frame)
{
    return frame.minimap.value_or(
        app::MinimapFrame{nullptr, LineNumber{1}, LineNumber{1}, 0, 0, std::nullopt});
}

// frame の窓の行の塊（無ければ空の窓）。
[[nodiscard]] const app::MinimapRows &rows_of(const app::EditorFrame &frame)
{
    static const app::MinimapRows empty{core::MinimapWindow{LineNumber{1}, 0}, {}, 0};
    const auto &rows = minimap_of(frame).rows;
    return rows == nullptr ? empty : *rows;
}

[[nodiscard]] core::MinimapBand band_of(const app::EditorFrame &frame)
{
    return minimap_of(frame).band.value_or(core::MinimapBand{0, 0, 0});
}

// `set minimap` は features の名前を足す・外す（決定 1・既定オフ）。
void verify_setting()
{
    const auto defaults = core::default_editor_settings();
    expect(!core::minimap_shown(defaults), "the minimap is off by default");
    const auto on = core::evaluate_ex("set minimap", defaults, core::Appearance::dark);
    expect(changes_settings(on) && core::minimap_shown(changed_settings(on)) &&
               changed_settings(on).features == std::vector<std::string>{"minimap"} &&
               message_of(on) == "minimap=on",
           "set minimap adds the feature name");
    const auto shown = core::minimap_set(defaults, true);
    const auto off = core::evaluate_ex("set nominimap", shown, core::Appearance::dark);
    expect(changes_settings(off) && !core::minimap_shown(changed_settings(off)) &&
               changed_settings(off).features.empty() && message_of(off) == "minimap=off",
           "set nominimap removes the feature name");
    const auto asked = core::evaluate_ex("set minimap?", shown, core::Appearance::dark);
    expect(asked.has_value() && !changes_settings(asked) && message_of(asked) == "minimap=on",
           "minimap? tells the current value without changing settings");
    auto others = defaults;
    others.features = {"a", "minimap", "b"};
    expect(core::minimap_set(others, false).features == std::vector<std::string>{"a", "b"} &&
               core::minimap_set(core::minimap_set(others, false), true).features ==
                   std::vector<std::string>{"a", "b", "minimap"},
           "other feature names keep their order");
    expect(!core::evaluate_ex("set mm", defaults, core::Appearance::dark).has_value() &&
               !core::evaluate_ex("set minimap!", defaults, core::Appearance::dark).has_value(),
           "other spellings are refused");
}

[[nodiscard]] core::EditorSettings minimap_settings()
{
    return core::minimap_set(core::default_editor_settings(), true);
}

void open_at(Editing &editing, std::string_view path, std::string text, std::size_t visible)
{
    editing.files().hold_at(std::string(path), Bytes{std::move(text)});
    // 面に約 300 行が入る窓の寸法（core::minimap_layout の rows）。
    applied(editing.controller(), app::VisibleLines{visible, {}, 1000, 700});
    applied(editing.controller(), app::OpenDocument{core::FilePath::parse(path).value()});
}

// frame を作らずに意図を送る（applied は frame を作るので、帯をそこで受け取ってしまう）。
void edit(app::EditorController &controller, const app::EditorIntent &intent)
{
    static_cast<void>(controller.apply(intent));
}

void place(app::EditorController &controller, std::size_t line, std::size_t column)
{
    static_cast<void>(
        controller.apply(app::PlaceCaret{core::TextPosition{LineNumber{line}, core::Column{column}},
                                         core::SelectionAnchoring::collapse}));
}

[[nodiscard]] std::string numbered_lines(std::size_t count)
{
    std::string text;
    for (std::size_t line = 1; line <= count; ++line)
    {
        text += "int value_" + std::to_string(line) + " = " + std::to_string(line) + ";\n";
    }
    return text;
}

// 設定がオフの frame は値なし、オンの frame は窓の範囲と塊（決定 1・3・4）。
void verify_frame_rows()
{
    Editing off;
    open_at(off, code_path, "int x = 1;\n", 20);
    expect(!off.controller().frame().minimap.has_value() && !off.controller().minimap_shown(),
           "the frame has no minimap while the setting is off");
    Editing editing{SettingsReading{std::optional{minimap_settings()}}};
    auto &controller = editing.controller();
    open_at(editing, code_path, "int x = 1;\n// note\n\n  return;\n", 20);
    expect(controller.minimap_shown(), "the minimap setting is read at start");
    const auto frame = controller.frame();
    expect(frame.minimap.has_value(), "the frame borrows the window rows while the setting is on");
    const auto minimap = minimap_of(frame);
    const auto &rows = rows_of(frame);
    expect(rows.window == core::MinimapWindow{LineNumber{1}, 5} && minimap.top == LineNumber{1} &&
               minimap.first_visible == LineNumber{1} && minimap.visible == 5 &&
               !minimap.band.has_value(),
           "the window is the whole short document and starts at the panel top");
    if (rows.rows.size() != 5)
    {
        expect(false, "the short document has 5 rows");
        return;
    }
    expect(rows.rows.at(0).front() == role(0, 3, SyntaxRole::type) &&
               rows.rows.at(1) == core::MinimapRow{role(0, 2, SyntaxRole::comment),
                                                   role(3, 4, SyntaxRole::comment)} &&
               rows.rows.at(2).empty() && rows.rows.at(3).front().left == 2,
           "the rows carry the type and comment roles and the indent");
}

// 1 行の中の打鍵は 1 行の帯、Enter は 1 行が 2 行に替わる帯（決定 4）。前の frame の serial が
// base に載る。
// ブロックコメントの途中の行で `*/` を打つと、下の行の色が comment から外れるまで読み直す。
void verify_comment_relex()
{
    Editing editing{SettingsReading{std::optional{minimap_settings()}}};
    auto &controller = editing.controller();
    open_at(editing, code_path, "int a;\n/* begin\nx\ny\nz\n*/\nint b;\n", 20);
    const auto held = minimap_of(controller.frame()).rows;
    if (held == nullptr || held->rows.size() < 7)
    {
        expect(false, "the comment document borrows its rows");
        return;
    }
    expect(held->rows.at(3).front().role == SyntaxRole::comment,
           "the rows inside the block comment are comments");
    place(controller, 3, 1);
    edit(controller, app::InsertText{"*/ "});
    const auto closed = controller.frame();
    expect(band_of(closed).begin == 2 && band_of(closed).inserted >= 3 &&
               !held->rows.at(3).front().role.has_value() &&
               !held->rows.at(4).front().role.has_value() &&
               held->rows.at(6).front().role == SyntaxRole::type,
           "closing the comment in its middle reads the rows below again");
}

// undo と redo も replace と同じ帯（undo の帯は編集の逆向き）。
void verify_history_bands()
{
    Editing editing{SettingsReading{std::optional{minimap_settings()}}};
    auto &controller = editing.controller();
    open_at(editing, code_path, numbered_lines(30), 20);
    const auto held = minimap_of(controller.frame()).rows;
    if (held == nullptr)
    {
        expect(false, "the history document borrows its rows");
        return;
    }
    const auto original = held->rows.at(2);
    place(controller, 3, 1);
    edit(controller, app::InsertText{"x"});
    static_cast<void>(controller.frame());
    edit(controller, app::HistoryAction{core::HistoryDirection::undo});
    const auto undone = controller.frame();
    expect(minimap_of(undone).rows == held && band_of(undone) == core::MinimapBand{2, 1, 1} &&
               held->rows.at(2) == original,
           "undo redraws the band of the row it restores");
    edit(controller, app::HistoryAction{core::HistoryDirection::redo});
    const auto redone = controller.frame();
    expect(band_of(redone) == core::MinimapBand{2, 1, 1} && held->rows.at(2).front() == muted(0, 4),
           "redo redraws the same band again");
    place(controller, 5, 1);
    edit(controller, app::InsertText{"\n"});
    static_cast<void>(controller.frame());
    edit(controller, app::HistoryAction{core::HistoryDirection::undo});
    const auto joined = controller.frame();
    expect(band_of(joined) == core::MinimapBand{4, 2, 1} && held->window.rows == 31,
           "undoing an Enter turns two rows back into one");
}

// 4 KiB を超える行は先頭だけを読み、100 桁で切って色を付けない。次の行は色が戻る。
void verify_long_line_rows()
{
    Editing editing{SettingsReading{std::optional{minimap_settings()}}};
    auto &controller = editing.controller();
    open_at(editing, code_path, "int" + std::string(10'000, 'x') + "\nint y;\n", 20);
    const auto frame = controller.frame();
    const auto &rows = rows_of(frame);
    expect(rows.rows.size() == 3 &&
               rows.rows.at(0) ==
                   core::MinimapRow{muted(0, static_cast<std::uint16_t>(core::minimap_columns))} &&
               rows.rows.at(1).front().role == SyntaxRole::type,
           "a line over 4 KiB is cut at 100 columns without colour and the next line is coloured");
}

// 折り返す行があっても、見えている範囲は見えている論理行の数（画面行の数ではない・レビュー L2）。
void verify_wrapped_face()
{
    Editing editing{SettingsReading{std::optional{minimap_settings()}}};
    auto &controller = editing.controller();
    editing.files().hold_at(std::string(code_path),
                            Bytes{std::string(50, 'a') + "\n" + numbered_lines(20)});
    // 1 画面行 10 升の幅で、1 行目は 5 画面行、2 行目からの 16 升の行は 2 画面行に折れる。
    // 10 画面行に 1〜4 行目が出る（4 行目は 1 画面行だけ見える）。
    applied(controller, app::VisibleLines{10, core::TextViewGeometry{100, 20, 96}, 1000, 700});
    applied(controller, app::OpenDocument{core::FilePath::parse(code_path).value()});
    const auto frame = controller.frame();
    expect(frame.lines.size() == 4 && frame.lines.front().screen_rows == 5 &&
               minimap_of(frame).visible == 4,
           "the visible face covers the logical lines that are shown");
}

// 全面プレビューの間は本文が隠れているので面を出さない。戻れば出る（レビュー L6）。
void verify_full_preview_hides()
{
    Editing editing{SettingsReading{std::optional{minimap_settings()}}};
    auto &controller = editing.controller();
    open_at(editing, markdown_path, "# title\n\ntext\n", 20);
    expect(controller.frame().minimap.has_value(), "a markdown document shows the minimap");
    const auto full = controller.apply_frame(app::SetPreviewPlacement{app::PreviewPlacement::full});
    expect(full.full_preview.has_value() && !full.minimap.has_value() &&
               !controller.minimap_shown(),
           "the full preview hides the minimap");
    const auto back = controller.apply_frame(app::CancelSelection{});
    expect(!back.full_preview.has_value() && back.minimap.has_value() && controller.minimap_shown(),
           "leaving the full preview shows it again");
}

void verify_frame_bands()
{
    Editing editing{SettingsReading{std::optional{minimap_settings()}}};
    auto &controller = editing.controller();
    open_at(editing, code_path, numbered_lines(30), 20);
    const auto first = controller.frame();
    const auto held = minimap_of(first).rows;
    if (held == nullptr)
    {
        expect(false, "the first frame borrows the rows");
        return;
    }
    const auto serial = held->serial;
    place(controller, 3, 1);
    edit(controller, app::InsertText{"x"});
    const auto typed = controller.frame();
    const auto minimap = minimap_of(typed);
    expect(minimap.rows == held && minimap.base == serial &&
               minimap.band == core::MinimapBand{2, 1, 1} && held->serial != serial,
           "a keystroke redraws the band of its row over the same rows");
    expect(held->rows.at(2).front() == muted(0, 4), "the typed row is read again");
    const auto quiet = controller.frame();
    expect(!minimap_of(quiet).band.has_value() && minimap_of(quiet).base == rows_of(quiet).serial,
           "a frame without an edit carries no band");
    edit(controller, app::InsertText{"\n"});
    const auto entered = controller.frame();
    expect(minimap_of(entered).band == core::MinimapBand{2, 1, 2} &&
               held->window == core::MinimapWindow{LineNumber{1}, 32},
           "Enter replaces one row with two and the window grows by one");
    edit(controller, app::InsertText{"y"});
    edit(controller, app::InsertText{"/*"});
    const auto opened = controller.frame();
    expect(minimap_of(opened).band.has_value() && band_of(opened).begin == 3 &&
               band_of(opened).inserted == 29 && held->rows.size() > 10 &&
               held->rows.at(10).front().role == SyntaxRole::comment,
           "opening a block comment redraws the rows below until the states agree");
}

// Ex の `set nominimap` で frame の値が消え、`set minimap`
// で戻る（設定の変わり目で口を差し替える）。
void verify_minimap_switch(Editing &editing)
{
    auto &controller = editing.controller();
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    const auto off = run_ex(controller, "set nominimap");
    expect(!off.minimap.has_value() && !controller.minimap_shown() &&
               !core::minimap_shown(off.settings),
           "set nominimap removes the minimap from the next frame");
    const auto on = run_ex(controller, "set minimap");
    expect(on.minimap.has_value() && controller.minimap_shown() && core::minimap_shown(on.settings),
           "set minimap shows it again");
}

// 窓より上の編集・別の文書は窓ごと作り直す（段 1 の限界）。
void verify_frame_rebuilds()
{
    Editing editing{SettingsReading{std::optional{minimap_settings()}}};
    auto &controller = editing.controller();
    open_at(editing, code_path, numbered_lines(6000), 20);
    place(controller, 5000, 1);
    const auto scrolled = controller.frame();
    const auto window = rows_of(scrolled).window;
    expect(window.first.value > 1 && window.rows == 2 * core::minimap_reach,
           "a panel outside the first window builds a window of 4,000 lines around it");
    place(controller, 1, 1);
    edit(controller, app::InsertText{"x"});
    place(controller, 5000, 1);
    const auto above = controller.frame();
    expect(minimap_of(above).rows != minimap_of(scrolled).rows,
           "an edit above the window builds the window again");
    open_at(editing, other_path, "plain\n", 20);
    const auto other = controller.frame();
    expect(minimap_of(other).rows != minimap_of(above).rows && !rows_of(other).rows.empty() &&
               rows_of(other).rows.front() == core::MinimapRow{muted(0, 5)},
           "another document has its own window");
    verify_minimap_switch(editing);
}
// 背景 ground に ink を percent % 重ねた色（整数・四捨五入）。
[[nodiscard]] core::RgbColor blended(core::RgbColor ground, core::RgbColor ink, int percent)
{
    const auto mix = [percent](std::uint8_t under, std::uint8_t over)
    { return static_cast<std::uint8_t>((under * (100 - percent) + over * percent + 50) / 100); };
    return core::RgbColor{mix(ground.red, ink.red), mix(ground.green, ink.green),
                          mix(ground.blue, ink.blue)};
}

// 面と縁の線の色（ADR 0171 の追補 #1487）: 組み込みの全テーマで、選んだ色の 70% の線が背景と
// しきい値 1.5 以上離れる。accent が背景に近いテーマだけ text に切り替わる。
void verify_face_colors()
{
    std::vector<std::string_view> switched;
    std::size_t weak = 0;
    for (const auto &theme : core::builtin_themes)
    {
        const auto ink = core::minimap_face_color(theme.ui);
        if (ink == theme.ui.text && !(theme.ui.text == theme.ui.accent))
        {
            switched.push_back(theme.name);
        }
        const auto line = blended(theme.ui.background, ink, 70);
        weak += core::minimap_contrast_at_least(line, theme.ui.background,
                                                core::minimap_face_contrast_tenths)
                    ? 0
                    : 1;
    }
    std::ranges::sort(switched);
    expect(weak == 0, "every built-in theme draws the minimap edge at a contrast of 1.5 or more");
    expect(switched ==
               std::vector<std::string_view>{"cobalt2", "synthwave-84", "tokyo-night-light"},
           "only the themes whose accent is close to the background use the text colour");
    expect(
        core::minimap_contrast_at_least(core::RgbColor{0, 0, 0}, core::RgbColor{255, 255, 255},
                                        210) &&
            !core::minimap_contrast_at_least(core::RgbColor{0, 0, 0}, core::RgbColor{255, 255, 255},
                                             211) &&
            core::minimap_contrast_at_least(core::RgbColor{9, 9, 9}, core::RgbColor{9, 9, 9}, 10),
        "the contrast of black and white is 21 and a colour against itself is 1");
}
} // namespace

void verify_minimap_scope()
{
    verify_row_blocks();
    verify_top();
    verify_window();
    verify_band_join();
    verify_layout();
    verify_face_colors();
    verify_panel_press_outside_body();
    verify_setting();
    verify_frame_rows();
    verify_frame_bands();
    verify_comment_relex();
    verify_history_bands();
    verify_long_line_rows();
    verify_wrapped_face();
    verify_full_preview_hides();
    verify_frame_rebuilds();
}
} // namespace nenenib::tests
