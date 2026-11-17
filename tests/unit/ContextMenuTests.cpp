// Issue #1051 / ADR 0078
// の追補。本文の右クリックのメニューの項目の列・使えるか・鍵の表示・配置だけを 測る（core
// の純関数）。時計・ファイル・乱数を使わない。
#include "CaretMotion.hpp"
#include "ChooseContextMenu.hpp"
#include "CloseContextMenu.hpp"
#include "Column.hpp"
#include "CommandChoice.hpp"
#include "CommandPaletteView.hpp"
#include "CommandText.hpp"
#include "ContextMenu.hpp"
#include "ContextMenuLayout.hpp"
#include "ContextMenuMode.hpp"
#include "ContextMenuRequest.hpp"
#include "ContextMenuShape.hpp"
#include "ContextMenuSituation.hpp"
#include "ContextMenuView.hpp"
#include "DevicePixels.hpp"
#include "DisplayLanguage.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorOperation.hpp"
#include "InsertText.hpp"
#include "KeyMap.hpp"
#include "LayoutRect.hpp"
#include "LineNumber.hpp"
#include "MoveCaret.hpp"
#include "OpenContextMenu.hpp"
#include "OpenOperationList.hpp"
#include "OperationTexts.hpp"
#include "PaletteMode.hpp"
#include "PlaceCaret.hpp"
#include "PointContextMenu.hpp"
#include "PointText.hpp"
#include "PointerClicks.hpp"
#include "Scopes.hpp"
#include "ScriptedClipboard.hpp"
#include "ScrollLines.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "StepContextMenu.hpp"
#include "SubmitCommand.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimMode.hpp"
#include "VisibleLines.hpp"

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
using core::ContextMenuSituation;
using Mode = core::ContextMenuMode;
using core::EditorOperation;
using core::LayoutRect;

// 項目の順と区切り線（設計席の決め: 切り取り・コピー・貼り付け・削除｜すべて選択｜検索・置換）。
using OrderRow = std::pair<EditorOperation, bool>;
constexpr std::array<OrderRow, 7> menu_order{{
    {EditorOperation::cut, false},
    {EditorOperation::copy, false},
    {EditorOperation::paste, false},
    {EditorOperation::delete_selection, false},
    {EditorOperation::select_all, true},
    {EditorOperation::find, true},
    {EditorOperation::replace, false},
}};

// 操作 → 通常モード・Vim の INSERT・Vim の NORMAL・Vim の VISUAL の鍵の表示。
using KeyRow = std::tuple<EditorOperation, std::string_view, std::string_view, std::string_view,
                          std::string_view>;
constexpr std::array<KeyRow, 7> menu_keys{{
    {EditorOperation::cut, "Ctrl+X", "Ctrl+X", "", "\"+d"},
    {EditorOperation::copy, "Ctrl+C", "Ctrl+C", "", "\"+y"},
    {EditorOperation::paste, "Ctrl+V", "Ctrl+V", "\"+p", "\"+p"},
    {EditorOperation::delete_selection, "Delete", "Delete", "x", "d"},
    {EditorOperation::select_all, "Ctrl+A", "Ctrl+A", "ggVG", "<Esc>ggVG"},
    {EditorOperation::find, "Ctrl+F", "", "/", "/"},
    {EditorOperation::replace, "Ctrl+H", "", ":%s/", ":s"},
}};

// 状況 → 7 項目の使える / 使えない（menu_order の順）。
using EnabledRow = std::pair<ContextMenuSituation, std::array<bool, 7>>;
constexpr std::array<EnabledRow, 9> enabled_rows{{
    // 通常モードの選択あり: 全部。
    {{Mode::ordinary, true}, {true, true, true, true, true, true, true}},
    // 選択なし: 切り取り・コピー・削除が薄い（貼り付けはクリップボードを読まずに試す）。
    {{Mode::ordinary, false}, {false, false, true, false, true, true, true}},
    // 全面プレビューの選択あり: コピーだけ。選択なし: どれも薄い。
    {{Mode::reading, true}, {false, true, false, false, false, false, false}},
    {{Mode::reading, false}, {false, false, false, false, false, false, false}},
    // Vim の INSERT は通常モードと同じ。
    {{Mode::vim_insert, true}, {true, true, true, true, true, true, true}},
    {{Mode::vim_insert, false}, {false, false, true, false, true, true, true}},
    // Vim の NORMAL: 切り取りとコピーだけ薄い（選択が無い）。
    {{Mode::vim_normal, false}, {false, false, true, true, true, true, true}},
    // Vim の VISUAL: 全部（鍵の列がある）。
    {{Mode::vim_visual, true}, {true, true, true, true, true, true, true}},
    {{Mode::vim_visual, false}, {true, true, true, true, true, true, true}},
}};

void verify_order()
{
    static_assert(core::context_menu_size == menu_order.size());
    static_assert(core::context_menu_separator_count == 2);
    const auto items = core::context_menu_items(ContextMenuSituation{Mode::ordinary, true});
    for (std::size_t index = 0; index < items.size(); ++index)
    {
        expect(items.at(index).operation == menu_order.at(index).first,
               "the menu lists the operations in the order of the table column");
        expect(items.at(index).separated == menu_order.at(index).second,
               "a separator stands above the first row of each later group");
    }
}

void verify_enabled()
{
    for (const auto &[situation, expected] : enabled_rows)
    {
        const auto items = core::context_menu_items(situation);
        for (std::size_t index = 0; index < items.size(); ++index)
        {
            expect(items.at(index).enabled == expected.at(index),
                   "a row is enabled when the selection and editing it needs are there");
        }
    }
}

void verify_keys()
{
    for (const auto &[operation, ordinary, insert, normal, visual] : menu_keys)
    {
        expect(core::context_menu_key(operation, Mode::ordinary) == ordinary &&
                   core::context_menu_key(operation, Mode::reading) == ordinary,
               "the ordinary mode and the full preview show the key label of the table");
        expect(core::context_menu_key(operation, Mode::vim_insert) == insert,
               "vim insert shows the ordinary keys that work in insert");
        expect(core::context_menu_key(operation, Mode::vim_normal) == normal &&
                   core::context_menu_vim_keys(operation, Mode::vim_normal) == normal,
               "vim normal shows and runs the normal keys of the table");
        expect(core::context_menu_key(operation, Mode::vim_visual) == visual &&
                   core::context_menu_vim_keys(operation, Mode::vim_visual) == visual,
               "vim visual shows and runs the visual keys of the table");
        expect(core::context_menu_vim_keys(operation, Mode::vim_insert).empty() &&
                   core::context_menu_vim_keys(operation, Mode::ordinary).empty(),
               "only vim normal and visual run vim keys");
    }
}

// 800 × 600 の窓に 96 DPI で開く。
[[nodiscard]] core::ContextMenuLayout laid(std::int32_t x, std::int32_t y, std::int32_t name,
                                           std::int32_t key)
{
    return core::context_menu_layout(
        core::ContextMenuRequest{x, y, LayoutRect{0, 0, 800, 600}, name, key, 96});
}

void verify_layout_size()
{
    const auto narrow = laid(100, 100, 40, 30);
    expect(core::width_of(narrow.panel) == 200, "the menu is at least 200 DIP wide");
    // 上下の空き 4 + 7 行 × 30 + 区切り 2 本 × (4 + 1 + 4)。
    expect(core::height_of(narrow.panel) == 4 + (7 * 30) + (2 * 9) + 4,
           "the menu is as tall as the rows, the separators and the gaps");
    const auto wide = laid(100, 100, 200, 60);
    expect(core::width_of(wide.panel) == 12 + 200 + 24 + 60 + 12,
           "a wide menu fits the widest name and key with the paddings and the gap");
    const auto keyless = laid(100, 100, 220, 0);
    expect(core::width_of(keyless.panel) == 12 + 220 + 12, "no key leaves no gap");
    const auto scaled = core::context_menu_layout(
        core::ContextMenuRequest{100, 100, LayoutRect{0, 0, 800, 600}, 40, 30, 144});
    expect(core::width_of(scaled.panel) == 300 &&
               core::height_of(scaled.rows.at(0)) == core::to_pixels(30, 144),
           "the sizes follow the dpi");
    expect(narrow.rows.at(0).top == narrow.panel.top + 4 &&
               narrow.rows.at(4).top == narrow.rows.at(3).bottom + 9 &&
               narrow.separators.at(0).top == narrow.rows.at(3).bottom + 4 &&
               core::height_of(narrow.separators.at(0)) == 1 &&
               narrow.separators.at(0).left == narrow.panel.left + 12 &&
               narrow.separators.at(1).top == narrow.rows.at(4).bottom + 4,
           "the separators sit 4 DIP below the row above and are 1 DIP thick");
    const auto inside = core::context_menu_row_inside(narrow, 0);
    expect(inside.left == narrow.panel.left + 12 && inside.right == narrow.panel.right - 12,
           "the row inside leaves 12 DIP on both sides");
}

void verify_layout_place()
{
    const auto opened = laid(100, 100, 40, 30);
    expect(opened.panel.left == 100 && opened.panel.top == 100,
           "the menu opens to the lower right of the point");
    const auto height = core::height_of(opened.panel);
    const auto flipped = laid(700, 550, 40, 30);
    expect(flipped.panel.right == 700 && flipped.panel.bottom == 550,
           "the menu opens to the upper left when the right and bottom edges cut it");
    const auto crowded = core::context_menu_layout(
        core::ContextMenuRequest{150, 100, LayoutRect{0, 0, 300, height + 20}, 40, 30, 96});
    expect(crowded.panel.left == 100 && crowded.panel.top == 20,
           "a menu that fits neither way is pushed against the far edge inside the bounds");
    const auto tiny = core::context_menu_layout(
        core::ContextMenuRequest{50, 50, LayoutRect{10, 10, 100, 100}, 40, 30, 96});
    expect(tiny.panel.left == 10 && tiny.panel.top == 10,
           "a menu larger than the bounds starts at their upper left");
}

void verify_hit()
{
    const auto layout = laid(100, 100, 40, 30);
    expect(core::context_menu_hit(layout, 150, layout.rows.at(0).top) == std::size_t{0} &&
               core::context_menu_hit(layout, 150, layout.rows.at(6).bottom - 1) == std::size_t{6},
           "a point on a row hits that row");
    expect(!core::context_menu_hit(layout, 150, layout.separators.at(0).top).has_value() &&
               !core::context_menu_hit(layout, 150, layout.panel.top).has_value() &&
               !core::context_menu_hit(layout, 99, layout.rows.at(0).top).has_value(),
           "the separators, the gaps and the outside hit nothing");
}

// ---------------------------------------------------------------- controller（application）

constexpr std::size_t cut_row = 0;
constexpr std::size_t copy_row = 1;
constexpr std::size_t paste_row = 2;
constexpr std::size_t delete_row = 3;
constexpr std::size_t select_all_row = 4;

// 1 行 "abc def"（升 10 px・行 20 px）の本文でキャレットを行頭に置いた足場。
void prepare_line(app::EditorController &controller)
{
    static_cast<void>(controller.apply(app::InsertText{"abc def"}));
    static_cast<void>(
        controller.apply(app::PlaceCaret{core::TextPosition{core::LineNumber{1}, core::Column{1}},
                                         core::SelectionAnchoring::collapse}));
    static_cast<void>(controller.apply(app::VisibleLines{5, core::TextViewGeometry{400, 20, 96}}));
}

[[nodiscard]] app::OpenContextMenu right_click(float x)
{
    return app::OpenContextMenu{
        100, 40,
        app::PointText{x, 0, core::SelectionAnchoring::collapse, app::PointerClicks::single}};
}

[[nodiscard]] std::size_t caret_column(app::EditorController &controller)
{
    return controller.frame().caret.position.column.value;
}

[[nodiscard]] std::string copied(Editing &editing)
{
    return editing.clipboard().read().value_or("");
}

void verify_open_ordinary()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_line(controller);
    expect(!controller.context_menu_active(), "the menu starts closed");
    const auto frame = controller.apply_frame(right_click(45.0F));
    expect(controller.context_menu_active() && frame.context_menu.has_value(),
           "a right click in the body opens the menu");
    const auto &view = frame.context_menu.value_or(app::ContextMenuView{});
    expect(view.x == 100 && view.y == 40 && !view.highlighted.has_value(),
           "the menu opens at the point with no row chosen");
    expect(view.rows.at(cut_row).name == "切り取り" && view.rows.at(cut_row).key == "Ctrl+X" &&
               view.rows.at(select_all_row).separated,
           "the rows carry the table names, the key labels and the separators");
    expect(!view.rows.at(cut_row).enabled && !view.rows.at(copy_row).enabled &&
               view.rows.at(paste_row).enabled && !view.rows.at(delete_row).enabled,
           "without a selection cut, copy and delete are disabled and paste is enabled");
    expect(caret_column(controller) == 6,
           "a right click outside the selection places the caret at the point first");
    expect(editing.clipboard().reads() == 0, "opening the menu does not read the clipboard");
}

void verify_keep_selection()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_line(controller);
    static_cast<void>(controller.apply(
        app::MoveCaret{core::CaretMotion::next_word, core::SelectionAnchoring::extend}));
    const auto frame = controller.apply_frame(right_click(15.0F));
    const auto &view = frame.context_menu.value_or(app::ContextMenuView{});
    expect(view.rows.at(cut_row).enabled && view.rows.at(copy_row).enabled,
           "a right click inside the selection keeps it and enables cut and copy");
    const auto chosen = controller.apply_frame(app::ChooseContextMenu{copy_row});
    expect(!controller.context_menu_active() && !chosen.context_menu.has_value() &&
               chosen.operation_request == core::EditorOperation::copy,
           "choosing a row closes the menu and hands the operation to run_operation");
    expect(!controller.apply_frame(app::CloseContextMenu{}).operation_request.has_value(),
           "the operation request lasts one intent");
}

void verify_choose_and_step()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_line(controller);
    static_cast<void>(controller.apply(right_click(5.0F)));
    static_cast<void>(controller.apply(app::ChooseContextMenu{cut_row}));
    expect(controller.context_menu_active(), "a disabled row does not run and the menu stays");
    static_cast<void>(controller.apply(app::StepContextMenu{true}));
    expect(controller.context_menu_highlighted() == paste_row,
           "the down key chooses the first enabled row");
    static_cast<void>(controller.apply(app::StepContextMenu{false}));
    expect(controller.context_menu_highlighted() == std::size_t{6},
           "the up key wraps over the disabled rows");
    static_cast<void>(controller.apply(app::PointContextMenu{copy_row}));
    expect(!controller.context_menu_highlighted().has_value(),
           "the pointer over a disabled row chooses nothing");
    static_cast<void>(controller.apply(app::PointContextMenu{select_all_row}));
    const auto chosen = controller.apply_frame(app::ChooseContextMenu{std::nullopt});
    expect(chosen.operation_request == core::EditorOperation::select_all,
           "Enter runs the chosen row");
}

void verify_close()
{
    Editing editing;
    auto &controller = editing.controller();
    prepare_line(controller);
    static_cast<void>(controller.apply(right_click(5.0F)));
    static_cast<void>(controller.apply(app::CloseContextMenu{}));
    expect(!controller.context_menu_active(), "Esc and the outside click close the menu");
    static_cast<void>(controller.apply(right_click(5.0F)));
    static_cast<void>(controller.apply(app::VisibleLines{5, core::TextViewGeometry{400, 20, 96}}));
    expect(controller.context_menu_active(), "an intent that is not an operation keeps the menu");
    static_cast<void>(controller.apply(app::ScrollLines{1}));
    expect(!controller.context_menu_active(), "the wheel and any other operation close the menu");
    static_cast<void>(controller.apply(app::VimKeyPress{core::VimKey{core::VimCharacter{U'x'}}}));
    static_cast<void>(controller.apply(app::SelectEditMode{core::EditMode::vim}));
    static_cast<void>(controller.apply(app::VimKeyPress{core::VimKey{core::VimCharacter{U':'}}}));
    static_cast<void>(controller.apply(right_click(5.0F)));
    expect(!controller.context_menu_active(), "the menu does not open over the input line");
}

[[nodiscard]] Editing &vim_line(Editing &editing)
{
    prepare_line(editing.controller());
    static_cast<void>(editing.controller().apply(app::SelectEditMode{core::EditMode::vim}));
    return editing;
}

void press_keys(app::EditorController &controller, std::u32string_view keys)
{
    for (const char32_t key : keys)
    {
        static_cast<void>(
            controller.apply(app::VimKeyPress{core::VimKey{core::VimCharacter{key}}}));
    }
}

void verify_vim_normal()
{
    Editing editing;
    auto &controller = vim_line(editing).controller();
    press_keys(controller, U"l");
    const auto frame = controller.apply_frame(right_click(55.0F));
    const auto &view = frame.context_menu.value_or(app::ContextMenuView{});
    expect(caret_column(controller) == 2, "vim keeps the caret when the menu opens");
    expect(!view.rows.at(cut_row).enabled && !view.rows.at(copy_row).enabled &&
               view.rows.at(paste_row).enabled && view.rows.at(paste_row).key == "\"+p" &&
               view.rows.at(delete_row).key == "x" && view.rows.at(select_all_row).key == "ggVG",
           "vim normal shows the normal keys and disables cut and copy");
    editing.clipboard().hold(Content{"XY"});
    const auto text = applied(controller, app::ChooseContextMenu{paste_row});
    expect(text == "abXYc def" && editing.clipboard().reads() == 1 &&
               !controller.frame().operation_request.has_value(),
           "paste runs \"+p through the engine and reads the clipboard once");
    static_cast<void>(controller.apply(right_click(5.0F)));
    expect(applied(controller, app::ChooseContextMenu{delete_row}) == "abXc def",
           "delete runs x in vim normal");
    static_cast<void>(controller.apply(right_click(5.0F)));
    const auto all = controller.apply_frame(app::ChooseContextMenu{select_all_row});
    expect(all.vim_mode == core::VimMode::visual_line, "select all runs ggVG");
}

void verify_vim_visual()
{
    Editing editing;
    auto &controller = vim_line(editing).controller();
    press_keys(controller, U"vl");
    const auto frame = controller.apply_frame(right_click(5.0F));
    const auto &view = frame.context_menu.value_or(app::ContextMenuView{});
    expect(view.rows.at(copy_row).enabled && view.rows.at(copy_row).key == "\"+y" &&
               view.rows.at(cut_row).key == "\"+d" &&
               view.rows.at(select_all_row).key == "<Esc>ggVG",
           "vim visual shows the visual keys");
    const auto copied_frame = controller.apply_frame(app::ChooseContextMenu{copy_row});
    expect(copied(editing) == "ab" && copied_frame.vim_mode == core::VimMode::normal,
           "copy runs \"+y on the visual selection");
    press_keys(controller, U"vl");
    static_cast<void>(controller.apply(right_click(5.0F)));
    expect(applied(controller, app::ChooseContextMenu{cut_row}) == "c def" &&
               copied(editing) == "ab",
           "cut runs \"+d on the visual selection");
    static_cast<void>(controller.apply(right_click(5.0F)));
    const auto all = controller.apply_frame(app::ChooseContextMenu{select_all_row});
    expect(all.vim_mode == core::VimMode::visual_line, "select all from normal is visual line");
}

void verify_vim_insert()
{
    Editing editing;
    auto &controller = vim_line(editing).controller();
    press_keys(controller, U"i");
    const auto frame = controller.apply_frame(right_click(5.0F));
    const auto &view = frame.context_menu.value_or(app::ContextMenuView{});
    expect(view.rows.at(paste_row).key == "Ctrl+V" && view.rows.at(5).key.empty(),
           "vim insert shows the ordinary clipboard keys");
    const auto chosen = controller.apply_frame(app::ChooseContextMenu{paste_row});
    expect(chosen.operation_request == core::EditorOperation::paste,
           "vim insert hands the operation to run_operation like the keys");
}

// メニューから流す前に、待ちの状態を捨てる（V-1791 の M1・Vim の `:amenu` の `<C-C>` と同じ）。
// 待ちのある NORMAL から選んだ結果は、待ちの無いときと同じ。
[[nodiscard]] std::string pasted_after(std::u32string_view waiting, std::size_t row)
{
    Editing editing;
    auto &controller = vim_line(editing).controller();
    press_keys(controller, U"\"ayl");
    editing.clipboard().hold(Content{"XY"});
    press_keys(controller, waiting);
    static_cast<void>(controller.apply(right_click(5.0F)));
    return applied(controller, app::ChooseContextMenu{row});
}

void verify_vim_waiting_discarded()
{
    const auto plain = pasted_after(U"", paste_row);
    expect(plain == "aXYbc def", "the paste row runs \"+p after the caret");
    expect(pasted_after(U"r", paste_row) == plain,
           "a pending r is dropped before the menu keys run");
    expect(pasted_after(U"d", paste_row) == plain,
           "a pending operator is dropped before the menu keys run");
    expect(pasted_after(U"\"a", paste_row) == plain,
           "a named register is dropped before the menu keys run");
    expect(pasted_after(U"2", delete_row) == pasted_after(U"", delete_row),
           "a count is dropped before the menu keys run");
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::InsertText{"one\ntwo\nthree\nfour"}));
    static_cast<void>(controller.apply(app::VisibleLines{5, core::TextViewGeometry{400, 20, 96}}));
    static_cast<void>(controller.apply(app::SelectEditMode{core::EditMode::vim}));
    press_keys(controller, U"gg3");
    static_cast<void>(controller.apply(right_click(5.0F)));
    static_cast<void>(controller.apply(app::ChooseContextMenu{select_all_row}));
    static_cast<void>(controller.apply(right_click(5.0F)));
    static_cast<void>(controller.apply(app::ChooseContextMenu{copy_row}));
    // 置いた本文は文書の改行（CRLF の既定）でつながる。
    expect(copied(editing).starts_with("one") && copied(editing).find("four") != std::string::npos,
           "select all after a count still selects the whole document");
    press_keys(controller, U"vli");
    static_cast<void>(controller.apply(right_click(5.0F)));
    const auto visual = controller.apply_frame(app::ChooseContextMenu{copy_row});
    expect(copied(editing) == "on" && visual.vim_mode == core::VimMode::normal,
           "copy after a pending text object in visual copies the selection");
}

// F1 の一覧の Vim の NORMAL / VISUAL（V-1791 の M2）。5 つの操作の鍵の表示は Vim の列で、列の無い
// 操作は出ず、選ぶとメニューと同じ Vim の鍵の列の道を通る。
[[nodiscard]] std::optional<std::string>
listed_key_of(const std::vector<core::CommandChoice> &choices, core::EditorOperation operation)
{
    for (const auto &choice : choices)
    {
        if (choice.operation == operation)
        {
            return choice.key;
        }
    }
    return std::nullopt;
}

void verify_vim_operation_list()
{
    const auto normal =
        core::operation_choices("", core::PaletteMode{core::EditMode::vim, Mode::vim_normal},
                                core::DisplayLanguage::japanese);
    expect(normal.size() == 41 && !listed_key_of(normal, core::EditorOperation::cut).has_value() &&
               !listed_key_of(normal, core::EditorOperation::copy).has_value() &&
               listed_key_of(normal, core::EditorOperation::paste) == "\"+p" &&
               listed_key_of(normal, core::EditorOperation::select_all) == "ggVG" &&
               listed_key_of(normal, core::EditorOperation::find) == "",
           "the vim normal list shows the vim keys and leaves out the rows without them");
    const auto visual =
        core::operation_choices("", core::PaletteMode{core::EditMode::vim, Mode::vim_visual},
                                core::DisplayLanguage::japanese);
    expect(visual.size() == 43 && listed_key_of(visual, core::EditorOperation::copy) == "\"+y",
           "the vim visual list shows every row with the visual keys");
    expect(
        core::operation_choices("", core::EditMode::vim, core::DisplayLanguage::japanese).size() ==
            43,
        "a list that knows only the edit mode keeps the vim rows without vim keys");
}

void verify_vim_operation_list_runs()
{
    Editing editing;
    auto &controller = vim_line(editing).controller();
    editing.clipboard().hold(Content{"XY"});
    static_cast<void>(controller.apply(app::OpenOperationList{}));
    static_cast<void>(controller.apply(app::CommandText{"はりつけ"}));
    const auto listed = controller.frame().command_palette.value_or(app::CommandPaletteView{});
    expect(!listed.rows.empty() && listed.rows.at(0).operation == core::EditorOperation::paste,
           "the list finds paste first");
    const auto text = applied(controller, app::SubmitCommand{});
    expect(text == "aXYbc def" && !controller.frame().operation_request.has_value(),
           "paste from the vim list runs \"+p through the engine like the menu");
}

// 表の Vim の鍵の列は全部 map の行と同じ読み手で読める（読めない列は何も流さない）。
void verify_vim_columns_parse()
{
    for (const auto &text : core::operation_texts)
    {
        for (const auto keys : {text.vim_normal_keys, text.vim_visual_keys})
        {
            expect(keys.empty() || core::vim_keys_of_notation(keys).has_value(),
                   "every vim key column of the menu parses as key notation");
        }
    }
}
} // namespace

void verify_context_menu_scope()
{
    verify_order();
    verify_enabled();
    verify_keys();
    verify_layout_size();
    verify_layout_place();
    verify_hit();
    verify_open_ordinary();
    verify_keep_selection();
    verify_choose_and_step();
    verify_close();
    verify_vim_normal();
    verify_vim_visual();
    verify_vim_insert();
    verify_vim_columns_parse();
    verify_vim_waiting_discarded();
    verify_vim_operation_list();
    verify_vim_operation_list_runs();
}
} // namespace nenenib::tests
