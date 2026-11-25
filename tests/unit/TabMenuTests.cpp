// Issue #1790 / ADR 0078 の追補・施主決定 D98。タブの右クリックのメニューの項目の列・使えるか・
// 鍵の表示・配置（core の純関数）と、どのタブのメニューか・切り替えずに実行する項目と切り替えて
// から実行する項目・まだ読んでいないタブを読まない・port の呼ばれた回数（controller）と、まとめて
// 閉じる相手の引き方（application の純関数）を測る。時計・ファイル・乱数を使わない。
#include "AutosaveId.hpp"
#include "BeginGoToLine.hpp"
#include "ChooseContextMenu.hpp"
#include "CloseContextMenu.hpp"
#include "CloseTab.hpp"
#include "ComposeText.hpp"
#include "ContextMenu.hpp"
#include "ContextMenuKind.hpp"
#include "ContextMenuLayout.hpp"
#include "ContextMenuRequest.hpp"
#include "ContextMenuShape.hpp"
#include "ContextMenuView.hpp"
#include "CopyTabName.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorDelivery.hpp"
#include "EditorFrame.hpp"
#include "EditorOperation.hpp"
#include "FilePath.hpp"
#include "LayoutRect.hpp"
#include "NewTab.hpp"
#include "OpenDocument.hpp"
#include "OpenTabMenu.hpp"
#include "RevealFailure.hpp"
#include "RevealTab.hpp"
#include "Scopes.hpp"
#include "ScriptedClipboard.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedReveal.hpp"
#include "Session.hpp"
#include "SessionTab.hpp"
#include "StepContextMenu.hpp"
#include "SweptTab.hpp"
#include "SwitchTab.hpp"
#include "TabMenuSituation.hpp"
#include "TabNamePart.hpp"
#include "TabSweep.hpp"
#include "TabSweepMove.hpp"
#include "TabSweepSide.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "WorkCompleted.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
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
using core::EditorOperation;
using core::TabMenuSituation;

// 項目の順と区切り線（施主決定 D98: エクスプローラーで場所を開く・パスをコピー・ファイル名をコピー
// ｜保存・名前を付けて保存｜閉じる・ほかのタブを閉じる・右のタブを閉じる）。
using OrderRow = std::pair<EditorOperation, bool>;
constexpr std::array<OrderRow, 8> tab_order{{
    {EditorOperation::reveal_in_explorer, false},
    {EditorOperation::copy_path, false},
    {EditorOperation::copy_file_name, false},
    {EditorOperation::save, true},
    {EditorOperation::save_as, false},
    {EditorOperation::close_tab, true},
    {EditorOperation::close_other_tabs, false},
    {EditorOperation::close_right_tabs, false},
}};

constexpr std::size_t reveal_row = 0;
constexpr std::size_t copy_path_row = 1;
constexpr std::size_t copy_name_row = 2;
constexpr std::size_t save_row = 3;
constexpr std::size_t close_row = 5;
constexpr std::size_t close_others_row = 6;
constexpr std::size_t close_right_row = 7;

// 様子 → 8 項目の使える / 使えない（tab_order の順）。
using EnabledRow = std::pair<TabMenuSituation, std::array<bool, 8>>;
constexpr std::array<EnabledRow, 5> enabled_rows{{
    // 名前があって読んであり、ほかと右にタブがある: 全部。
    {{true, true, true, true}, {true, true, true, true, true, true, true, true}},
    // 無題: パスの 3 つが薄い。保存は使える（名前を付けて保存へ）。
    {{false, true, true, true}, {false, false, false, true, true, true, true, true}},
    // まだ読んでいない: 保存の 2 つが薄い（変更が無い）。パスの 3 つは読まずに使える。
    {{true, false, true, false}, {true, true, true, false, false, true, true, false}},
    // 1 つだけのタブ: ほかと右を閉じるが薄い。閉じるは使える（最後のタブで窓を閉じる・D21）。
    {{true, true, false, false}, {true, true, true, true, true, true, false, false}},
    // 右端のタブ: 右を閉じるだけ薄い。
    {{true, true, true, false}, {true, true, true, true, true, true, true, false}},
}};

void verify_tab_order()
{
    static_assert(core::tab_menu_size == tab_order.size());
    static_assert(core::tab_menu_separator_count == 2);
    const auto items = core::tab_menu_items(TabMenuSituation{true, true, true, true});
    for (std::size_t index = 0; index < items.size(); ++index)
    {
        expect(items.at(index).operation == tab_order.at(index).first,
               "the tab menu lists the operations in the order of the tab_menu column");
        expect(items.at(index).separated == tab_order.at(index).second,
               "a separator stands above the first row of each later tab menu group");
    }
}

void verify_tab_enabled()
{
    for (const auto &[situation, expected] : enabled_rows)
    {
        const auto items = core::tab_menu_items(situation);
        for (std::size_t index = 0; index < items.size(); ++index)
        {
            expect(items.at(index).enabled == expected.at(index),
                   "a tab menu row is enabled when the tab has what it needs");
        }
    }
}

void verify_tab_keys()
{
    expect(core::tab_menu_key(EditorOperation::save, core::EditMode::ordinary) == "Ctrl+S" &&
               core::tab_menu_key(EditorOperation::save_as, core::EditMode::ordinary) ==
                   "Ctrl+Shift+S" &&
               core::tab_menu_key(EditorOperation::close_tab, core::EditMode::ordinary) == "Ctrl+W",
           "the tab menu shows the keys of save, save as and close");
    expect(core::tab_menu_key(EditorOperation::save, core::EditMode::vim) == "Ctrl+S" &&
               core::tab_menu_key(EditorOperation::close_tab, core::EditMode::vim) == "Ctrl+F4",
           "vim shows the keys that close and save in vim");
    for (const auto operation : {EditorOperation::reveal_in_explorer, EditorOperation::copy_path,
                                 EditorOperation::copy_file_name, EditorOperation::close_other_tabs,
                                 EditorOperation::close_right_tabs})
    {
        expect(core::tab_menu_key(operation, core::EditMode::ordinary).empty() &&
                   core::tab_menu_key(operation, core::EditMode::vim).empty(),
               "the tab rows without keys show no key");
    }
}

void verify_tab_layout()
{
    const auto layout = core::context_menu_layout(core::ContextMenuRequest{
        100, 40, core::LayoutRect{0, 0, 800, 600}, 40, 30, 96, core::ContextMenuKind::tab});
    // 上下の空き 4 + 8 行 × 30 + 区切り 2 本 × (4 + 1 + 4)。
    expect(layout.row_count == 8 && layout.separator_count == 2 &&
               core::height_of(layout.panel) == 4 + (8 * 30) + (2 * 9) + 4,
           "the tab menu is as tall as its 8 rows and 2 separators");
    expect(layout.panel.left == 100 && layout.panel.top == 40 &&
               layout.rows.at(save_row).top == layout.rows.at(save_row - 1).bottom + 9 &&
               layout.rows.at(close_row).top == layout.rows.at(close_row - 1).bottom + 9,
           "the separators of the tab menu sit above save and close");
    expect(core::context_menu_hit(layout, 150, layout.rows.at(close_right_row).top) ==
               std::optional<std::size_t>{close_right_row},
           "the last tab row is hit");
    const auto body = core::context_menu_layout(core::ContextMenuRequest{
        100, 40, core::LayoutRect{0, 0, 800, 600}, 40, 30, 96, core::ContextMenuKind::body});
    expect(
        body.row_count == core::context_menu_size &&
            !core::context_menu_hit(body, 150, layout.rows.at(close_right_row).top + 9).has_value(),
        "the body menu keeps its own rows and hits nothing below them");
}

// ---------------------------------------------------------------- 閉じる相手（application）

void verify_swept()
{
    const std::vector<app::AutosaveId> band{{1}, {2}, {3}, {4}};
    const app::TabSweep others{app::AutosaveId{2}, app::TabSweepSide::others};
    const app::TabSweep right{app::AutosaveId{2}, app::TabSweepSide::right};
    expect(app::swept_tab(band, others) == std::optional<std::size_t>{0} &&
               app::swept_tab(band, right) == std::optional<std::size_t>{2},
           "the next tab to close is the leftmost one on the swept side");
    expect(!app::tab_swept(band, others, 1) && app::tab_swept(band, others, 3) &&
               !app::tab_swept(band, right, 0) && app::tab_swept(band, right, 3),
           "the kept tab stays and the right sweep leaves the left side");
    const std::vector<app::AutosaveId> kept{{2}};
    expect(!app::swept_tab(kept, others).has_value() &&
               !app::swept_tab(band, app::TabSweep{app::AutosaveId{9}, app::TabSweepSide::others})
                    .has_value(),
           "nothing is closed when only the kept tab is left or the kept tab is gone");
}

// ---------------------------------------------------------------- controller（application）

[[nodiscard]] core::FilePath work_path(char name)
{
    return core::FilePath::parse("C:\\work\\" + std::string(1, name) + ".txt").value();
}

void hold_three(ScriptedFiles &files)
{
    for (const char name : {'a', 'b', 'c'})
    {
        files.hold_at("C:\\work\\" + std::string(1, name) + ".txt", Bytes{std::string(1, name)});
    }
}

// a b c を開いて c がアクティブ。
void open_three(Editing &editing)
{
    hold_three(editing.files());
    for (const char name : {'a', 'b', 'c'})
    {
        applied(editing.controller(), app::OpenDocument{work_path(name)});
    }
}

[[nodiscard]] app::ContextMenuView menu_of(app::EditorController &controller)
{
    return controller.frame().context_menu.value_or(app::ContextMenuView{});
}

[[nodiscard]] std::string notice_of(app::EditorController &controller)
{
    const auto message = controller.frame().command_message;
    return message.has_value() ? std::string(message.value().text()) : std::string{};
}

[[nodiscard]] std::string copy_of(Editing &editing)
{
    return editing.clipboard().read().value_or("");
}

void verify_tab_open()
{
    Editing editing;
    open_three(editing);
    auto &controller = editing.controller();
    expect(controller.frame().tabs.size() == 3 && controller.frame().active_tab == 2,
           "three tabs are open with the last one active");
    applied(controller, app::OpenTabMenu{50, 40, 0});
    const auto view = menu_of(controller);
    expect(controller.context_menu_active() && view.kind == core::ContextMenuKind::tab &&
               view.rows.size() == 8 && view.x == 50 && view.y == 40,
           "a right click on a tab opens the tab menu at the point");
    expect(view.rows.at(reveal_row).name == "エクスプローラーで場所を開く" &&
               view.rows.at(save_row).key == "Ctrl+S" && view.rows.at(close_row).key == "Ctrl+W" &&
               view.rows.at(save_row).separated && view.rows.at(close_row).separated,
           "the tab rows carry the table names, the key labels and the separators");
    expect(controller.frame().active_tab == 2, "opening the menu of another tab does not switch");
    applied(controller, app::StepContextMenu{true});
    expect(controller.context_menu_highlighted() == std::optional<std::size_t>{reveal_row},
           "the arrow keys walk the tab rows like the body menu");
    applied(controller, app::ChooseContextMenu{copy_path_row});
    expect(!controller.context_menu_active() && copy_of(editing) == "C:\\work\\a.txt" &&
               controller.frame().active_tab == 2,
           "copy path writes the full path of the clicked tab without switching");
    applied(controller, app::OpenTabMenu{50, 40, 1});
    applied(controller, app::ChooseContextMenu{copy_name_row});
    expect(copy_of(editing) == "b.txt" && controller.frame().active_tab == 2,
           "copy file name writes the file name of the clicked tab without switching");
}

// 入力行と変換の間はタブのメニューも開かない（本文のメニューと同じ）。
void verify_tab_menu_refusals()
{
    Editing typing;
    open_three(typing);
    applied(typing.controller(), app::BeginGoToLine{});
    applied(typing.controller(), app::OpenTabMenu{50, 40, 0});
    expect(!typing.controller().context_menu_active(),
           "the tab menu does not open over an input line");
    Editing composing;
    open_three(composing);
    applied(composing.controller(), app::ComposeText{composed_of("に", {}, 0)});
    applied(composing.controller(), app::OpenTabMenu{50, 40, 0});
    expect(!composing.controller().context_menu_active(),
           "the tab menu does not open over the IME composition");
}

void verify_tab_untitled()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::OpenTabMenu{50, 40, 0});
    const auto view = menu_of(controller);
    expect(!view.rows.at(reveal_row).enabled && !view.rows.at(copy_path_row).enabled &&
               !view.rows.at(copy_name_row).enabled && view.rows.at(save_row).enabled &&
               !view.rows.at(close_others_row).enabled,
           "an untitled lone tab greys out the path rows and the sweeps");
    applied(controller, app::ChooseContextMenu{copy_path_row});
    expect(controller.context_menu_active() && editing.clipboard().writes() == 0,
           "a greyed row does nothing and keeps the menu open");
    applied(controller, app::CloseContextMenu{});
    applied(controller, app::CopyTabName{app::TabNamePart::file_name});
    applied(controller, app::RevealTab{});
    expect(editing.clipboard().writes() == 0 && editing.reveal().touches() == 0 &&
               notice_of(controller) == core::ui_text(core::UiMessage::notice_tab_untitled,
                                                      core::DisplayLanguage::japanese),
           "the list items on an untitled tab copy nothing, ask nothing and say why");
}

void verify_tab_save_switches()
{
    Editing editing;
    open_three(editing);
    auto &controller = editing.controller();
    applied(controller, app::OpenTabMenu{50, 40, 0});
    const auto frame = controller.apply_frame(app::ChooseContextMenu{save_row});
    expect(frame.active_tab == 0 && frame.operation_request == EditorOperation::save,
           "save switches to the clicked tab first and hands save to run_operation");
}

void verify_tab_closes()
{
    Editing editing;
    open_three(editing);
    auto &controller = editing.controller();
    applied(controller, app::OpenTabMenu{50, 40, 0});
    const auto closed = controller.apply(app::ChooseContextMenu{close_row});
    expect(closed.close_request == std::optional<std::size_t>{0} &&
               controller.frame().tabs.size() == 3 && controller.frame().active_tab == 2,
           "close asks the window to close the clicked tab the way the x does");
    applied(controller, app::OpenTabMenu{50, 40, 1});
    const auto others = controller.apply(app::ChooseContextMenu{close_others_row});
    const auto ids = controller.tab_ids();
    expect(others.tab_sweep.has_value() && others.tab_sweep.value().keep == ids.at(1) &&
               others.tab_sweep.value().side == app::TabSweepSide::others &&
               controller.frame().tabs.size() == 3 && controller.frame().active_tab == 2,
           "close others asks the window to sweep around the clicked tab without switching");
    applied(controller, app::OpenTabMenu{50, 40, 0});
    const auto right = controller.apply(app::ChooseContextMenu{close_right_row});
    expect(right.tab_sweep.has_value() && right.tab_sweep.value().keep == ids.at(0) &&
               right.tab_sweep.value().side == app::TabSweepSide::right,
           "close right asks the window to sweep the right side of the clicked tab");
    expect(!controller.apply(app::CloseContextMenu{}).tab_sweep.has_value(),
           "the sweep request lasts one intent");
}

void verify_tab_reveal()
{
    Editing editing;
    open_three(editing);
    auto &controller = editing.controller();
    applied(controller, app::OpenTabMenu{50, 40, 1});
    applied(controller, app::ChooseContextMenu{reveal_row});
    expect(editing.reveal().paths().size() == 1 &&
               editing.reveal().paths().at(0) == work_path('b') &&
               controller.frame().active_tab == 2,
           "reveal asks the port with the clicked tab's path without switching");
    editing.reveal().serve(std::unexpected(app::RevealFailure::not_found));
    applied(controller, app::WorkCompleted{});
    expect(editing.reveal().collects() == 1 &&
               notice_of(controller) == core::ui_text(core::UiMessage::notice_reveal_not_found,
                                                      core::DisplayLanguage::japanese),
           "a reveal failure arrives with the work signal as a one line notice");
    applied(controller, app::WorkCompleted{});
    expect(editing.reveal().collects() == 1,
           "the port is not collected again once every request has come back");
    applied(controller, app::OpenTabMenu{50, 40, 1});
    applied(controller, app::ChooseContextMenu{reveal_row});
    editing.reveal().serve(std::unexpected(app::RevealFailure::failed));
    applied(controller, app::WorkCompleted{});
    expect(notice_of(controller) == core::ui_text(core::UiMessage::notice_reveal_failed,
                                                  core::DisplayLanguage::japanese),
           "a reveal the shell could not show says it failed, apart from a missing file");
}

// 前回のタブを戻した起動（a と c はまだ読んでいない・b がアクティブ）。
[[nodiscard]] SessionReading three_restored()
{
    const auto tab = [](char name, std::size_t rank)
    {
        return app::SessionTab{work_path(name),
                               core::TextPosition{core::LineNumber{1}, core::Column{1}},
                               core::LineNumber{1}, rank};
    };
    return SessionReading{
        std::optional<app::Session>{app::Session{{tab('a', 1), tab('b', 0), tab('c', 2)}, 1}}};
}

void verify_tab_unloaded()
{
    Editing restored(three_restored(), hold_three);
    auto &controller = restored.controller();
    const auto reads = restored.files().reads();
    applied(controller, app::OpenTabMenu{50, 40, 0});
    const auto view = menu_of(controller);
    expect(view.rows.at(reveal_row).enabled && view.rows.at(copy_path_row).enabled &&
               !view.rows.at(save_row).enabled && !view.rows.at(save_row + 1).enabled,
           "a tab not read yet keeps the path rows and greys out the saves");
    applied(controller, app::ChooseContextMenu{reveal_row});
    applied(controller, app::OpenTabMenu{50, 40, 2});
    applied(controller, app::ChooseContextMenu{copy_name_row});
    expect(restored.reveal().paths().size() == 1 &&
               restored.reveal().paths().at(0) == work_path('a') && copy_of(restored) == "c.txt" &&
               restored.files().reads() == reads && controller.frame().active_tab == 1,
           "reveal and copy on tabs not read yet use the path without reading them");
}

// まとめて閉じる道の一歩（決定 6・レビューの推奨 2）。アクティブでない相手が先、アクティブは最後。
void verify_sweep_steps()
{
    const std::vector<app::AutosaveId> band{{1}, {2}, {3}};
    const app::TabSweep others{app::AutosaveId{3}, app::TabSweepSide::others};
    using Step = std::pair<app::TabSweepMove, std::size_t>;
    expect(app::tab_sweep_step(band, others, 0) == Step{app::TabSweepMove::close, 1} &&
               app::tab_sweep_step(band, others, 1) == Step{app::TabSweepMove::close, 0},
           "a sweep closes the tabs that are not active first");
    const std::vector<app::AutosaveId> left{{1}, {3}};
    expect(app::tab_sweep_step(left, others, 0) == Step{app::TabSweepMove::switch_to_keep, 1} &&
               app::tab_sweep_step(left, others, 1) == Step{app::TabSweepMove::close, 0},
           "the active tab is left last and the sweep switches to the kept tab before it");
    expect(app::tab_sweep_step({{3}}, others, 0).first == app::TabSweepMove::done &&
               app::tab_sweep_step(band, app::TabSweep{app::AutosaveId{9}, others.side}, 0).first ==
                   app::TabSweepMove::done,
           "nothing is left to close when only the kept tab stays or it is gone");
}

// 窓の sweep_step と同じ 1 歩を controller に流す（未保存のタブは無い形）。続けてよければ真。
[[nodiscard]] bool swept_one(app::EditorController &controller, const app::TabSweep &sweep)
{
    const auto [move, position] =
        app::tab_sweep_step(controller.tab_ids(), sweep, controller.active_tab());
    switch (move)
    {
    case app::TabSweepMove::close:
        applied(controller, app::CloseTab{position});
        return true;
    case app::TabSweepMove::switch_to_keep:
        applied(controller, app::SwitchTab{position});
        return controller.tab_ids().at(controller.active_tab()) == sweep.keep;
    case app::TabSweepMove::done:
        return false;
    }
    std::unreachable();
}

void sweep_by_steps(app::EditorController &controller, const app::TabSweep &sweep)
{
    while (swept_one(controller, sweep))
    {
    }
}

[[nodiscard]] bool asked_for(Editing &editing, char name)
{
    const auto &requests = editing.reads().requests();
    return std::ranges::any_of(requests, [name](const auto &request)
                               { return request.path == work_path(name); });
}

// a と c はまだ読んでいない・b がアクティブ。c を残す「ほかのタブを閉じる」は a を読まずに閉じる。
void verify_sweep_reads_nothing_beside()
{
    Editing deferred(three_restored(), hold_three);
    deferred.reads().defer_above(0);
    auto &waiting = deferred.controller();
    const app::TabSweep keep_c{waiting.tab_ids().at(2), app::TabSweepSide::others};
    sweep_by_steps(waiting, keep_c);
    expect(!asked_for(deferred, 'a') && asked_for(deferred, 'c') && waiting.tab_ids().size() == 2 &&
               waiting.frame().document.title.text() == "b.txt" &&
               notice_of(waiting) == "読み込み中: c.txt",
           "a sweep closes the unread side tab unread and keeps the active tab while the kept "
           "one is read in the background");
    Editing restored(three_restored(), hold_three);
    auto &controller = restored.controller();
    const app::TabSweep kept{controller.tab_ids().at(2), app::TabSweepSide::others};
    sweep_by_steps(controller, kept);
    expect(!asked_for(restored, 'a') && controller.tab_ids().size() == 1 &&
               controller.tab_ids().front() == kept.keep &&
               controller.frame().document.title.text() == "c.txt",
           "a sweep switches to the kept tab last and closes the active one after it");
}

void verify_tab_menu_closes()
{
    Editing editing;
    open_three(editing);
    auto &controller = editing.controller();
    applied(controller, app::OpenTabMenu{50, 40, 0});
    applied(controller, app::NewTab{});
    expect(!controller.context_menu_active(), "another user operation closes the tab menu");
    applied(controller, app::OpenTabMenu{50, 40, 9});
    expect(!controller.context_menu_active(), "a tab outside the band opens nothing");
    applied(controller, app::OpenTabMenu{50, 40, 0});
    applied(controller, app::WorkCompleted{});
    expect(controller.context_menu_active(),
           "a signal that leaves the band alone keeps the tab menu open");
}
} // namespace

void verify_tab_menu_contracts()
{
    verify_tab_order();
    verify_tab_enabled();
    verify_tab_keys();
    verify_tab_layout();
    verify_swept();
    verify_tab_open();
    verify_tab_menu_refusals();
    verify_tab_untitled();
    verify_tab_save_switches();
    verify_tab_closes();
    verify_tab_reveal();
    verify_tab_unloaded();
    verify_sweep_steps();
    verify_sweep_reads_nothing_beside();
    verify_tab_menu_closes();
}
} // namespace nenenib::tests
