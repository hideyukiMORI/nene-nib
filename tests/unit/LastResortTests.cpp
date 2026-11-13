// 最後の砦の計画と controller の口（ADR 0137 の追補 #1625）。手続きそのもの（std::terminate・
// スレッド・終了コード）は adapters の子 process の試験（nib_adapters_last_resort）が見る。ここは
// 「意図の途中で落ちたとき、途中の本文で前の退避を上書きしない」ことを、投げる port の替え玉で
// 本当に意図の途中から抜けて確かめる。
#include "Appearance.hpp"
#include "AutosaveFailure.hpp"
#include "AutosaveId.hpp"
#include "AutosaveLedger.hpp"
#include "AutosavePlan.hpp"
#include "AutosaveStep.hpp"
#include "AutosaveTick.hpp"
#include "Edit.hpp"
#include "EditBoundary.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorState.hpp"
#include "FilePath.hpp"
#include "InsertText.hpp"
#include "IntentMark.hpp"
#include "LastResortPlan.hpp"
#include "NewTab.hpp"
#include "Offset.hpp"
#include "OpenDocument.hpp"
#include "Scopes.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <cstddef>
#include <stdexcept>
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
using app::AutosaveId;
using app::AutosaveLedger;
using app::AutosaveStep;
using app::EditorState;
using app::IntentMark;

[[nodiscard]] std::string letters(const std::vector<std::pair<AutosaveId, AutosaveStep>> &plan)
{
    std::string text;
    for (const auto &[id, step] : plan)
    {
        text += text.empty() ? "" : " ";
        text += std::to_string(id.value);
        switch (step)
        {
        case AutosaveStep::store:
            text += 's';
            break;
        case AutosaveStep::discard:
            text += 'd';
            break;
        case AutosaveStep::keep:
            text += 'k';
            break;
        case AutosaveStep::oversized:
            text += 'o';
            break;
        }
    }
    return text;
}

[[nodiscard]] EditorState fresh()
{
    return EditorState::create(core::Appearance::dark, core::EditMode::ordinary);
}

// アクティブな文書に 1 打鍵ぶんの編集を積む（改訂番号が 1 進む・本文は計画に関わらない）。
[[nodiscard]] EditorState typed(const EditorState &state)
{
    const std::size_t at = state.history().position();
    return state.with_history(state.history().pushed(
        core::Edit{core::Offset{at}, "", "x", core::Offset{at}}, core::EditBoundary::separate));
}

[[nodiscard]] core::FilePath other_path()
{
    auto parsed = core::FilePath::parse("C:\\work\\other.txt");
    expect(parsed.has_value(), "the other sample path parses");
    return std::move(parsed).value();
}

// 3 つの枝（決め 1）: 意図の外は autosave_plan そのまま・意図の中でアクティブの改訂番号だけ進んだら
// そのタブだけ keep・アクティブかタブの数が変わったら空。
void verify_plan_branches()
{
    const EditorState two = typed(typed(fresh()).with_new_tab());
    const IntentMark entered = app::intent_mark_of(two);
    expect(entered.active == AutosaveId{1} && entered.tabs == 2 &&
               entered.revision == two.history().revision(),
           "the mark is the active number, its revision and the tab count");
    expect(letters(app::last_resort_plan(two, {}, entered, 0)) ==
               letters(app::autosave_plan(two, {})),
           "outside an intent the plan is the autosave plan");
    expect(letters(app::last_resort_plan(two, {}, entered, 1)) == "0s 1s",
           "inside an intent that has not moved the active tab the plan is the autosave plan");
    const EditorState moved = typed(two);
    expect(letters(app::last_resort_plan(moved, {}, entered, 1)) == "0s 1k",
           "inside an intent the active tab that moved keeps its previous backup");
    expect(letters(app::last_resort_plan(moved, AutosaveLedger{{AutosaveId{7}, 1}}, entered, 2)) ==
               "0s 1k 7d",
           "the other tabs and closed tabs follow the autosave plan inside a nested intent");
    expect(letters(app::last_resort_plan(moved, {}, entered, 0)) == "0s 1s",
           "outside an intent even a moved active tab is stored");
    expect(app::last_resort_plan(moved.with_switched(0), {}, entered, 1).empty(),
           "inside an intent a switched active tab writes nothing");
    expect(
        app::last_resort_plan(typed(two.with_new_tab()).with_switched(1), {}, entered, 1).empty(),
        "inside an intent a changed tab count writes nothing");
}

[[nodiscard]] core::VimKey key_of(char32_t character)
{
    return core::VimKey{core::VimCharacter{character}};
}

// 本当に意図の途中から抜ける（@a の再生の中の :w で write が投げる）。test が受けるので apply の
// 出口を通らず、深さは 1 のまま残る（製品では窓の手続きの noexcept が terminate を呼ぶ）。
[[nodiscard]] bool thrown_midway(Editing &editing)
{
    editing.files().throw_on_writes();
    applied(editing.controller(), app::VimKeyPress{key_of(U'@')});
    try
    {
        static_cast<void>(editing.controller().apply(app::VimKeyPress{key_of(U'a')}));
    }
    catch (const std::runtime_error &)
    {
        return true;
    }
    return false;
}

// 意図の外（深さ 0）: 決定 9 と同じに変わったタブを store して flush し、全部書けたら真。flush の
// 時間切れと書けなかった結果は偽。
void verify_backup_outside_intent()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"ab"});
    expect(controller.back_up_before_defect() &&
               editing.autosave().calls() == std::vector<std::string>{"store 0", "flush"},
           "outside an intent the last resort stores the unsaved tab and flushes");
    expect(editing.session().writes() == 0 && editing.autosave().clears() == 0,
           "the last resort writes no session list and keeps the autosave");
    applied(controller, app::InsertText{"c"});
    editing.autosave().flush_returns(false);
    expect(!controller.back_up_before_defect(), "a flush that hits its limit is not backed up");
    editing.autosave().flush_returns(true);
    applied(controller, app::InsertText{"d"});
    editing.autosave().fail(AutosaveId{0}, app::AutosaveFailure::unwritable);
    expect(!controller.back_up_before_defect(), "a failed store is not backed up");
}

// 意図の途中（マクロの再生の途中）で投げた: 途中まで写した本文のタブは書かず前の退避を残し、ほかの
// 変わったタブは書く。
void verify_backup_midway_keeps_active()
{
    Editing editing;
    open_vim_document(editing, "abcd\n");
    auto &controller = editing.controller();
    vim_replay(controller, "qax:w<CR>q");
    vim_replay(controller, "x");
    applied(controller, app::NewTab{});
    vim_replay(controller, "izz<Esc>");
    applied(controller, app::AutosaveTick{});
    vim_replay(controller, "aY<Esc>");
    applied(controller, app::SwitchTab{0});
    const std::size_t before = editing.autosave().calls().size();
    expect(thrown_midway(editing), "the scripted write throws in the middle of @a");
    expect(controller.text().text() == "d\n",
           "the macro's x was written to the state before the write threw");
    expect(controller.back_up_before_defect(), "the last resort answers backed up");
    const std::vector<std::string> calls(editing.autosave().calls().begin() +
                                             static_cast<std::ptrdiff_t>(before),
                                         editing.autosave().calls().end());
    expect(calls == std::vector<std::string>{"store 1", "flush"},
           "the half-replayed active tab keeps its backup and the other changed tab is stored");
}

// 意図の途中で アクティブのタブが替わった（マクロの gt）: 何も書かず、flush だけ。
void verify_backup_midway_switched()
{
    Editing editing;
    open_vim_document(editing, "abcd\n");
    auto &controller = editing.controller();
    applied(controller, app::NewTab{});
    applied(controller, app::OpenDocument{other_path()});
    applied(controller, app::SwitchTab{0});
    vim_replay(controller, "qaxgt:w<CR>q");
    vim_replay(controller, "gtx");
    applied(controller, app::AutosaveTick{});
    const std::size_t before = editing.autosave().calls().size();
    expect(thrown_midway(editing), "the scripted write throws after gt in @a");
    expect(controller.back_up_before_defect(), "flushing nothing is backed up");
    const std::vector<std::string> calls(editing.autosave().calls().begin() +
                                             static_cast<std::ptrdiff_t>(before),
                                         editing.autosave().calls().end());
    expect(calls == std::vector<std::string>{"flush"},
           "a switched active tab inside the intent writes nothing");
}
} // namespace

void verify_last_resort_contracts()
{
    verify_plan_branches();
    verify_backup_outside_intent();
    verify_backup_midway_keeps_active();
    verify_backup_midway_switched();
}
} // namespace nenenib::tests
