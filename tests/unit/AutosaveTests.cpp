// 自動退避の計画と controller の結線（ADR 0137 の決定 2〜5・8）。
#include "Appearance.hpp"
#include "AutosaveFailure.hpp"
#include "AutosaveId.hpp"
#include "AutosaveLedger.hpp"
#include "AutosavePlan.hpp"
#include "AutosaveRemnant.hpp"
#include "AutosaveSnapshot.hpp"
#include "AutosaveStep.hpp"
#include "AutosaveTick.hpp"
#include "CaretMotion.hpp"
#include "CloseTab.hpp"
#include "Column.hpp"
#include "DiscardAutosave.hpp"
#include "DisplayText.hpp"
#include "Document.hpp"
#include "DocumentView.hpp"
#include "Edit.hpp"
#include "EditBoundary.hpp"
#include "EditHistory.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorState.hpp"
#include "EndSession.hpp"
#include "FilePath.hpp"
#include "InsertText.hpp"
#include "LineNumber.hpp"
#include "MoveCaret.hpp"
#include "NewTab.hpp"
#include "Offset.hpp"
#include "PlaceCaret.hpp"
#include "RestoreAutosave.hpp"
#include "SaveDocument.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "ScriptedAutosave.hpp"
#include "SelectionAnchoring.hpp"
#include "SessionEnd.hpp"
#include "SwitchTab.hpp"
#include "TabTitle.hpp"
#include "TestSupport.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "UnloadedDocument.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <optional>
#include <string>
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

using Plan = std::vector<std::pair<AutosaveId, AutosaveStep>>;

[[nodiscard]] char step_letter(AutosaveStep step)
{
    switch (step)
    {
    case AutosaveStep::store:
        return 's';
    case AutosaveStep::discard:
        return 'd';
    case AutosaveStep::keep:
        return 'k';
    }
    std::unreachable();
}

// 計画を「番号と仕事の頭文字」の列で読む（例: "0s 1k 7d"）。
[[nodiscard]] std::string printed(const Plan &plan)
{
    std::string text;
    for (const auto &[id, step] : plan)
    {
        if (!text.empty())
        {
            text += ' ';
        }
        text += std::to_string(id.value);
        text += step_letter(step);
    }
    return text;
}

[[nodiscard]] std::string planned(const EditorState &state, const AutosaveLedger &ledger)
{
    return printed(app::autosave_plan(state, ledger));
}

[[nodiscard]] EditorState fresh()
{
    return EditorState::create(core::Appearance::dark, core::EditMode::ordinary);
}

// アクティブな文書に 1 打鍵ぶんの編集を積む（本文は計画に関わらないので触らない）。
[[nodiscard]] EditorState typed(const EditorState &state, core::EditBoundary boundary)
{
    const std::size_t at = state.history().position();
    return state.with_history(
        state.history().pushed(core::Edit{core::Offset{at}, "", "x", core::Offset{at}}, boundary));
}

[[nodiscard]] EditorState saved(const EditorState &state)
{
    return state.with_document(
        app::Document{sample_path(), core::TextEncoding::utf8, state.history().position()});
}

[[nodiscard]] app::UnloadedDocument unloaded()
{
    const core::FilePath path = sample_path();
    return app::UnloadedDocument{
        path, core::TextPosition{core::LineNumber{1}, core::Column{1}}, core::LineNumber{1},
        app::DocumentView{core::tab_title_for(path, core::SaveState::saved), path,
                          core::TextEncoding::utf8, core::SaveState::saved, std::nullopt}};
}

// 決定 2 の表（読み込み済みのアクティブ）。
void verify_plan_active()
{
    const EditorState clean = fresh();
    expect(planned(clean, {}) == "0k", "an untouched tab without a remnant is kept");
    const EditorState once = typed(clean, core::EditBoundary::coalesce);
    expect(planned(once, {}) == "0s", "an unsaved tab never stored is stored");
    const AutosaveLedger stored{{AutosaveId{0}, once.history().revision()}};
    expect(planned(once, stored) == "0k", "an unsaved tab unchanged since its store is kept");
    const EditorState folded = typed(once, core::EditBoundary::coalesce);
    expect(folded.history().position() == once.history().position() &&
               planned(folded, stored) == "0s",
           "a keystroke folded into the same undo unit is stored again");
    const EditorState undone = once.with_history(once.history().undone());
    const EditorState branched = typed(undone, core::EditBoundary::separate);
    expect(branched.history().position() == once.history().position() &&
               planned(branched, stored) == "0s",
           "another edit after undo at the same place is stored again");
    const EditorState kept = saved(folded);
    expect(planned(kept, stored) == "0d", "a saved tab with a remnant is discarded");
    expect(planned(kept, {}) == "0k", "a saved tab without a remnant is kept");
}

// 脇の束・閉じたタブ・まだ読んでいない文書。
void verify_plan_tabs()
{
    const EditorState two = typed(fresh(), core::EditBoundary::coalesce).with_new_tab();
    expect(two.autosave_ids().size() == 2 && two.autosave_ids().at(1) == AutosaveId{1},
           "a new tab takes the next number");
    expect(planned(two, {}) == "0s 1k", "an unsaved parked tab is stored like the active one");
    const AutosaveLedger closed{{AutosaveId{7}, 3}};
    expect(planned(two, closed) == "0s 1k 7d", "a remnant of a closed tab is discarded");
    const EditorState restored = fresh().with_restored({unloaded()});
    expect(planned(restored, {}) == "0k 1k", "a tab not read yet is kept");
    expect(planned(restored, AutosaveLedger{{AutosaveId{1}, 0}}) == "0k 1d",
           "a tab not read yet with a remnant is discarded");
    const EditorState reopened = two.with_closed(1).with_new_tab();
    expect(reopened.autosave_ids().size() == 2 && reopened.autosave_ids().at(1) == AutosaveId{2},
           "the number of a closed tab is never reused");
    const EditorState switched = two.with_switched(0);
    expect(switched.autosave_ids() == two.autosave_ids(), "switching keeps the numbers in place");
}

[[nodiscard]] std::string bytes_of(const app::AutosaveSnapshot &snapshot)
{
    std::string text;
    for (const std::byte byte : snapshot.bytes)
    {
        text += static_cast<char>(byte);
    }
    return text;
}

// 合図で頼み、変わっていなければ頼まない（決定 2・7）。
void verify_tick_stores_changes()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::AutosaveTick{});
    expect(editing.autosave().touches() == 0, "a tick over an untouched tab asks nothing");
    applied(controller, app::InsertText{"ab"});
    applied(controller, app::AutosaveTick{});
    const auto &stored = editing.autosave().stored();
    expect(stored.size() == 1 && bytes_of(stored.back()) == "ab" &&
               !stored.back().path.has_value() && stored.back().id == AutosaveId{0},
           "a tick stores the unsaved body of the untitled tab");
    expect(stored.back().caret == core::TextPosition{core::LineNumber{1}, core::Column{3}},
           "the stored caret is the line and the 1-based column");
    expect(stored.back().title.text() ==
               core::tab_title_for(std::nullopt, core::SaveState::modified).text(),
           "the stored title carries the unsaved mark");
    applied(controller, app::AutosaveTick{});
    expect(editing.autosave().stored().size() == 1, "a tick without a change stores nothing");
    applied(controller, app::InsertText{"c"});
    applied(controller, app::AutosaveTick{});
    expect(editing.autosave().stored().size() == 2 &&
               bytes_of(editing.autosave().stored().back()) == "abc",
           "a keystroke in the same undo unit is stored on the next tick");
}

// 保存と同じ符号化（決定 4）・保存したら消す・閉じたら消す（決定 2）・閉じる窓で片付ける（決定
// 5）。
void verify_tick_encodes_and_discards()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"ab"});
    applied(controller, app::SaveDocument{sample_path(), core::TextEncoding::utf8_bom});
    applied(controller, app::InsertText{"c"});
    applied(controller, app::AutosaveTick{});
    const std::string bom_abc = std::string(core::byte_order_mark()) + "abc";
    expect(editing.autosave().stored().size() == 1 &&
               bytes_of(editing.autosave().stored().back()) == bom_abc &&
               editing.autosave().stored().back().path.has_value(),
           "the stored body is encoded the same way as saving");
    applied(controller, app::SaveDocument{sample_path(), core::TextEncoding::utf8_bom});
    applied(controller, app::AutosaveTick{});
    expect(editing.autosave().discarded() == std::vector<AutosaveId>{AutosaveId{0}},
           "a tab saved after its store is discarded");
    applied(controller, app::NewTab{});
    applied(controller, app::InsertText{"z"});
    applied(controller, app::AutosaveTick{});
    applied(controller, app::CloseTab{1});
    applied(controller, app::AutosaveTick{});
    expect(editing.autosave().discarded().size() == 2 &&
               editing.autosave().discarded().back() == AutosaveId{1},
           "the remnant of a closed tab is discarded on the next tick");
    applied(controller, app::EndSession{app::SessionEnd::window_closed});
    expect(editing.autosave().clears() == 1, "ending the session clears the autosave once");
}

[[nodiscard]] std::string notice(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string("none");
}

// 失敗は成功するまで 1 回だけ知らせる（決定 4）。
void verify_failure_notified_once()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"a"});
    applied(controller, app::AutosaveTick{});
    editing.autosave().fail(app::AutosaveFailure::unwritable);
    expect(notice(controller.apply_frame(app::WorkCompleted{})) == "自動退避できませんでした",
           "a failed store leaves one notice line");
    applied(controller, app::InsertText{"b"});
    applied(controller, app::AutosaveTick{});
    editing.autosave().fail(app::AutosaveFailure::unwritable);
    expect(notice(controller.apply_frame(app::WorkCompleted{})) == "none",
           "a second failure before any success is not told again");
    applied(controller, app::InsertText{"c"});
    applied(controller, app::AutosaveTick{});
    expect(notice(controller.apply_frame(app::WorkCompleted{})) == "none",
           "a store that succeeded tells nothing");
    applied(controller, app::InsertText{"d"});
    applied(controller, app::AutosaveTick{});
    editing.autosave().fail(app::AutosaveFailure::location_unavailable);
    expect(notice(controller.apply_frame(app::WorkCompleted{})) == "自動退避できませんでした",
           "a failure after a success is told once again");
}
// 合図は使う人の操作ではないので、上下に動くときに狙う横位置を捨てない（決定 1）。30 秒ごとに
// 必ず来るので、短い行をまたいでいる最中に届いても長い行で元の位置へ戻る。Vim の欲しい列も同じ。
void verify_tick_keeps_wanted_column()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::InsertText{"abcdef\nx\nabcdef"});
    applied(controller, app::VisibleLines{10, core::TextViewGeometry{400, 20, 96}});
    applied(controller, app::PlaceCaret{core::TextPosition{core::LineNumber{1}, core::Column{5}},
                                        core::SelectionAnchoring::collapse});
    const auto down =
        app::MoveCaret{core::CaretMotion::next_line, core::SelectionAnchoring::collapse};
    applied(controller, down);
    applied(controller, app::AutosaveTick{});
    expect(controller.apply_frame(down).caret.position ==
               core::TextPosition{core::LineNumber{3}, core::Column{5}},
           "a tick between two downward moves keeps the wanted x");
    Editing vim;
    open_vim_document(vim, "abcdef\nx\nabcdef");
    vim_replay(vim.controller(), "4lj");
    applied(vim.controller(), app::AutosaveTick{});
    vim_replay(vim.controller(), "j");
    expect(caret_at(vim.controller().frame(), 3, 5),
           "a tick between two j keeps the Vim wanted column");
}
// 前回のプロセスが残した退避の 1 本。location は替え玉が本文を引く鍵。
[[nodiscard]] app::AutosaveRemnant remnant(std::string location, std::optional<core::FilePath> path)
{
    auto title = core::tab_title_for(path, core::SaveState::modified);
    return app::AutosaveRemnant{std::move(location), AutosaveId{0}, std::move(path),
                                std::move(title)};
}

// 起動の問いに「はい」: 残骸を開くのと同じ復号で読み、帯の末尾に未保存のタブとして足し、最初に
// 戻したタブをアクティブにする。残骸は 1 回だけ聞き、戻したら全部 forget する（決定 6）。
void verify_restore_remnants()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    editing.autosave().remain(remnant("1f/0", sample_path()), "alpha\nbeta");
    editing.autosave().remain(remnant("1f/1", std::nullopt),
                              std::string(core::byte_order_mark()) + "gamma");
    expect(controller.autosave_remnants().size() == 2 && editing.autosave().remnant_reads() == 1,
           "the start asks the remnants once and keeps the two");
    const auto restored = controller.apply_frame(app::RestoreAutosave{});
    expect(restored.tabs.size() == 3 && restored.active_tab == 1,
           "two remnants become two tabs at the end and the first of them is active");
    expect(restored.document.path == std::optional{sample_path()} &&
               restored.document.save_state == core::SaveState::modified &&
               vim_body(restored) == "alpha\nbeta",
           "the first restored tab has its path, its body and the unsaved mark");
    expect(restored.tabs.size() == 3 &&
               restored.tabs.at(2).save_state == core::SaveState::modified &&
               !restored.tabs.at(2).path.has_value() &&
               restored.tabs.at(2).encoding == core::TextEncoding::utf8_bom,
           "the untitled remnant is decoded the way opening decodes and stays unsaved");
    expect(editing.autosave().remnant_reads() == 1 && editing.autosave().body_reads() == 2 &&
               editing.autosave().forgotten() == std::vector<std::string>{"1f/0", "1f/1"},
           "restoring reads each body once, forgets every remnant and never asks the list again");
}

// 戻したタブは本文だけを持つ未保存のタブで、次の合図がこのプロセスの退避として書く（決定 2・6）。
void verify_restored_tabs_autosave()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    editing.autosave().remain(remnant("1f/0", sample_path()), "alpha");
    editing.autosave().remain(remnant("1f/1", std::nullopt),
                              std::string(core::byte_order_mark()) + "gamma");
    static_cast<void>(controller.autosave_remnants());
    applied(controller, app::RestoreAutosave{});
    const auto third = controller.apply_frame(app::SwitchTab{2});
    expect(vim_body(third) == "gamma", "the untitled remnant keeps its body without the mark");
    applied(controller, app::AutosaveTick{});
    expect(editing.autosave().stored().size() == 2,
           "the next tick stores the two restored tabs under this process");
}

// 読めない残骸は 1 行の知らせで飛ばし、forget する（決定 6）。
void verify_restore_unreadable()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    editing.autosave().remain(remnant("2a/0", sample_path()), std::nullopt);
    static_cast<void>(controller.autosave_remnants());
    const auto restored = controller.apply_frame(app::RestoreAutosave{});
    expect(restored.tabs.size() == 1 && notice(restored) == "戻せませんでした: note.txt",
           "an unreadable remnant adds no tab and leaves one notice line");
    expect(editing.autosave().forgotten() == std::vector<std::string>{"2a/0"},
           "the unreadable remnant is forgotten too");
}

// 起動の問いに「いいえ」: 残骸を読まずに全部 forget し、タブは増えない（決定 6）。
void verify_discard_remnants()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    editing.autosave().remain(remnant("3b/0", sample_path()), "alpha");
    editing.autosave().remain(remnant("3b/1", std::nullopt), "beta");
    static_cast<void>(controller.autosave_remnants());
    const auto discarded = controller.apply_frame(app::DiscardAutosave{});
    expect(discarded.tabs.size() == 1 && editing.autosave().body_reads() == 0 &&
               editing.autosave().forgotten() == std::vector<std::string>{"3b/0", "3b/1"},
           "discarding forgets every remnant without reading and adds no tab");
    applied(controller, app::RestoreAutosave{});
    expect(editing.autosave().forgotten().size() == 2 && editing.autosave().remnant_reads() == 1,
           "an answer after the first one has nothing left to forget");
}
} // namespace

void verify_autosave_contracts()
{
    verify_plan_active();
    verify_plan_tabs();
    verify_tick_stores_changes();
    verify_tick_encodes_and_discards();
    verify_failure_notified_once();
    verify_tick_keeps_wanted_column();
    verify_restore_remnants();
    verify_restored_tabs_autosave();
    verify_restore_unreadable();
    verify_discard_remnants();
}

void verify_autosave_scope()
{
    verify_autosave_contracts();
}
} // namespace nenenib::tests
