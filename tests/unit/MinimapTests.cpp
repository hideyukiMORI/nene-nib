// scope `--minimap` の単体テスト（ADR 0171・施主決定 D70・Issue #1017 の段 1）。core の 1 行の塊
// （空白・Tab・全角・構文の区間・100 桁）・面の最上段の行・窓・帯の合わせ・面の配置・設定と Ex の
// `set minimap`、application の frame（オフは値なし・オンは窓の範囲と塊・編集の帯・Enter の帯・
// 窓より上の編集は窓ごと・別の文書は窓ごと）。時計・ファイル・乱数の種を読まない。
#include "Appearance.hpp"
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
#include "PlaceCaret.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SyntaxRole.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

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
               minimap.first_visible == LineNumber{1} && minimap.visible == 20 &&
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
} // namespace

void verify_minimap_scope()
{
    verify_row_blocks();
    verify_top();
    verify_window();
    verify_band_join();
    verify_layout();
    verify_setting();
    verify_frame_rows();
    verify_frame_bands();
    verify_frame_rebuilds();
}
} // namespace nenenib::tests
