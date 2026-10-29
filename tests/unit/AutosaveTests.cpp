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
#include "DocumentByteLimit.hpp"
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
#include "OpenDocument.hpp"
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
    case AutosaveStep::oversized:
        return 'o';
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

// 退避を消してよい終わり方は、読み込み済みのタブに未保存が 1 つも無いか、使う人が「いいえ」と
// 答えて捨てたとき（決定 5）。
void verify_clears_autosave()
{
    const app::EndSession forced{app::SessionEnd::window_closed};
    const app::EndSession discarded{.reason = app::SessionEnd::window_closed,
                                    .unsaved_discarded = true};
    expect(app::clears_autosave(fresh(), forced), "an untouched window may clear the autosave");
    const EditorState once = typed(fresh(), core::EditBoundary::coalesce);
    expect(!app::clears_autosave(once, forced), "an unsaved active tab keeps the autosave");
    expect(app::clears_autosave(saved(once), forced), "a saved tab may clear the autosave");
    expect(!app::clears_autosave(once.with_new_tab(), forced),
           "an unsaved parked tab keeps the autosave");
    expect(app::clears_autosave(fresh().with_restored({unloaded()}), forced),
           "a tab not read yet has nothing unsaved");
    expect(app::clears_autosave(once, discarded),
           "an unsaved tab the user chose not to save may clear the autosave");
    expect(app::clears_autosave(once.with_new_tab(), discarded),
           "unsaved parked tabs the user chose not to save may clear the autosave");
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

// 合図で頼み、変わっていなければ頼まない（決定 2・7）。頼まなかった合図は描き直さない（#813）。
void verify_tick_stores_changes()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    expect(!controller.apply(app::AutosaveTick{}).repaint && editing.autosave().touches() == 0,
           "a tick over an untouched tab asks nothing and asks for no repaint");
    applied(controller, app::InsertText{"ab"});
    expect(controller.apply(app::AutosaveTick{}).repaint, "a tick that stores asks for a repaint");
    const auto &stored = editing.autosave().stored();
    expect(stored.size() == 1 && bytes_of(stored.back()) == "ab" &&
               !stored.back().path.has_value() && stored.back().id == AutosaveId{0},
           "a tick stores the unsaved body of the untitled tab");
    expect(stored.back().caret == core::TextPosition{core::LineNumber{1}, core::Column{3}},
           "the stored caret is the line and the 1-based column");
    expect(stored.back().title.text() ==
               core::tab_title_for(std::nullopt, core::SaveState::modified).text(),
           "the stored title carries the unsaved mark");
    expect(!controller.apply(app::AutosaveTick{}).repaint &&
               editing.autosave().stored().size() == 1,
           "a tick without a change stores nothing and asks for no repaint");
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

// 失敗は成功するまで 1 回だけ知らせる（決定 4）。知らせを下ろすのは退避の成功の結果だけで、
// 結果の無い合図（フォルダの WorkCompleted）では下ろさない。失敗と成功が同じ collect に来たら
// 失敗が勝つ（独立レビュー F3）。
void verify_failure_notified_once()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"a"});
    applied(controller, app::AutosaveTick{});
    editing.autosave().fail(AutosaveId{0}, app::AutosaveFailure::unwritable);
    expect(notice(controller.apply_frame(app::WorkCompleted{})) == "自動退避できませんでした",
           "a failed store leaves one notice line");
    applied(controller, app::InsertText{"b"});
    applied(controller, app::AutosaveTick{});
    editing.autosave().fail(AutosaveId{0}, app::AutosaveFailure::unwritable);
    expect(notice(controller.apply_frame(app::WorkCompleted{})) == "none",
           "a second failure before any success is not told again");
    applied(controller, app::InsertText{"c"});
    applied(controller, app::AutosaveTick{});
    expect(notice(controller.apply_frame(app::WorkCompleted{})) == "none",
           "a signal without an autosave outcome tells nothing");
    editing.autosave().fail(AutosaveId{0}, app::AutosaveFailure::unwritable);
    expect(notice(controller.apply_frame(app::WorkCompleted{})) == "none",
           "a signal without an autosave outcome does not lower the notice");
    applied(controller, app::InsertText{"d"});
    applied(controller, app::AutosaveTick{});
    editing.autosave().succeed(AutosaveId{0});
    expect(notice(controller.apply_frame(app::WorkCompleted{})) == "none",
           "a store that succeeded tells nothing");
    applied(controller, app::InsertText{"e"});
    applied(controller, app::AutosaveTick{});
    editing.autosave().fail(AutosaveId{0}, app::AutosaveFailure::location_unavailable);
    expect(notice(controller.apply_frame(app::WorkCompleted{})) == "自動退避できませんでした",
           "a failure after a success is told once again");
    applied(controller, app::InsertText{"f"});
    applied(controller, app::AutosaveTick{});
    editing.autosave().succeed(AutosaveId{0});
    applied(controller, app::WorkCompleted{});
    applied(controller, app::NewTab{});
    applied(controller, app::InsertText{"g"});
    applied(controller, app::AutosaveTick{});
    editing.autosave().fail(AutosaveId{1}, app::AutosaveFailure::unwritable);
    editing.autosave().succeed(AutosaveId{0});
    expect(notice(controller.apply_frame(app::WorkCompleted{})) == "自動退避できませんでした",
           "a failure and a success in one collect still tell the failure");
    applied(controller, app::InsertText{"h"});
    applied(controller, app::AutosaveTick{});
    editing.autosave().fail(AutosaveId{1}, app::AutosaveFailure::unwritable);
    expect(notice(controller.apply_frame(app::WorkCompleted{})) == "none",
           "the success of another tab beside the failure does not lower the notice");
}

// 本文が core::maximum_decoded_document_bytes を超えたら書かず、文書ごとに 1 回だけ知らせる（ADR
// 0159 の決定 7・L-4）。ちょうどは書く。前の退避は消さない。
void verify_tick_skips_oversized()
{
    constexpr std::size_t limit = core::maximum_decoded_document_bytes;
    std::string body;
    body.reserve(limit);
    while (body.size() + 8U <= limit)
    {
        body.append("aaaaaaa\n");
    }
    body.resize(limit - 1U, 'a');
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    editing.files().hold(std::move(body));
    applied(controller, app::OpenDocument{sample_path()});
    applied(controller, app::InsertText{"x"});
    applied(controller, app::AutosaveTick{});
    expect(editing.autosave().stored().size() == 1 &&
               editing.autosave().stored().back().bytes.size() == limit,
           "a body of exactly 64 MiB is stored");
    applied(controller, app::InsertText{"y"});
    const auto told = controller.apply_frame(app::AutosaveTick{});
    expect(editing.autosave().stored().size() == 1 && editing.autosave().discarded().empty() &&
               notice(told) == "64 MiB を超える文書は自動退避しません",
           "a body past 64 MiB is not stored, the earlier store is kept, and it is told");
    applied(controller, app::InsertText{"z"});
    const auto again = controller.apply_frame(app::AutosaveTick{});
    expect(editing.autosave().stored().size() == 1 && notice(again) == "none",
           "the same document is told only once");
    applied(controller, app::NewTab{});
    applied(controller, app::InsertText{"small"});
    applied(controller, app::AutosaveTick{});
    expect(editing.autosave().stored().size() == 2 &&
               bytes_of(editing.autosave().stored().back()) == "small",
           "another document below the limit is still stored");
}

// 失敗した id は帳面から外れ、本文が変わらなくても次の合図で書き直す（決定 4）。成功の後の合図は
// 変わらなければ書かない。
void verify_failed_store_retried()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"a"});
    applied(controller, app::AutosaveTick{});
    editing.autosave().fail(AutosaveId{0}, app::AutosaveFailure::unwritable);
    applied(controller, app::WorkCompleted{});
    applied(controller, app::AutosaveTick{});
    expect(editing.autosave().stored().size() == 2 &&
               editing.autosave().stored().back().id == AutosaveId{0} &&
               bytes_of(editing.autosave().stored().back()) == "a",
           "a failed store is stored again on the next tick without a change");
    editing.autosave().succeed(AutosaveId{0});
    applied(controller, app::WorkCompleted{});
    applied(controller, app::AutosaveTick{});
    expect(editing.autosave().stored().size() == 2,
           "after the store succeeded a tick without a change stores nothing");
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
                                std::move(title),
                                core::TextPosition{core::LineNumber{1}, core::Column{1}}};
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
    expect(editing.autosave().stored().size() == 2,
           "restoring stores the two restored tabs under this process at once");
    applied(controller, app::AutosaveTick{});
    expect(editing.autosave().stored().size() == 2,
           "the next tick has nothing new to store for the restored tabs");
}

// 戻したタブのキャレットは一覧に書いた行と桁（決定 6）。本文の範囲の外の行はセッションの復元と
// 同じく最後の行へ寄せる（改行で終わる本文では最後の行の 1 桁が本文の末尾）。
[[nodiscard]] core::TextPosition restored_caret(core::TextPosition caret, std::string body)
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    auto placed = remnant("4c/0", sample_path());
    placed.caret = caret;
    editing.autosave().remain(std::move(placed), std::move(body));
    static_cast<void>(controller.autosave_remnants());
    return controller.apply_frame(app::RestoreAutosave{}).caret.position;
}

void verify_restore_caret()
{
    expect(restored_caret(core::TextPosition{core::LineNumber{3}, core::Column{5}},
                          "one\ntwo\nthree four\n") ==
               core::TextPosition{core::LineNumber{3}, core::Column{5}},
           "the restored tab puts the caret where the list wrote it");
    expect(restored_caret(core::TextPosition{core::LineNumber{99}, core::Column{1}},
                          "alpha\nbeta\n") ==
               core::TextPosition{core::LineNumber{3}, core::Column{1}},
           "a caret past the body is brought to the end of the body");
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
// 確かめずに未保存のタブが残る終わり方（OS の終了の強行など）では退避を消さず、残骸として次の
// 起動で問う。未保存が残らないか、使う人が「いいえ」と答えて捨てたなら 1 回消す（決定 5）。
void verify_end_session_keeps_unsaved()
{
    Editing forced;
    applied(forced.controller(), app::VisibleLines{10});
    applied(forced.controller(), app::InsertText{"ab"});
    applied(forced.controller(), app::AutosaveTick{});
    applied(forced.controller(), app::EndSession{app::SessionEnd::window_closed});
    expect(forced.autosave().clears() == 0,
           "an ending that leaves an unsaved tab (the OS session end) keeps the autosave");
    Editing last;
    applied(last.controller(), app::VisibleLines{10});
    applied(last.controller(), app::InsertText{"ab"});
    applied(last.controller(), app::EndSession{app::SessionEnd::last_tab_closed});
    expect(last.autosave().clears() == 0,
           "closing the last tab while it is unsaved keeps the autosave");
    Editing parked;
    applied(parked.controller(), app::VisibleLines{10});
    applied(parked.controller(), app::InsertText{"ab"});
    applied(parked.controller(), app::NewTab{});
    applied(parked.controller(), app::EndSession{app::SessionEnd::window_closed});
    expect(parked.autosave().clears() == 0, "an unsaved parked tab keeps the autosave too");
    Editing clean;
    applied(clean.controller(), app::VisibleLines{10});
    applied(clean.controller(), app::EndSession{app::SessionEnd::window_closed});
    expect(clean.autosave().clears() == 1,
           "an ending without any unsaved tab clears the autosave once");
    Editing declined;
    applied(declined.controller(), app::VisibleLines{10});
    applied(declined.controller(), app::InsertText{"ab"});
    applied(declined.controller(), app::AutosaveTick{});
    applied(declined.controller(),
            app::EndSession{.reason = app::SessionEnd::window_closed, .unsaved_discarded = true});
    expect(declined.autosave().clears() == 1,
           "closing the window after answering no clears the autosave once");
    Editing declined_last;
    applied(declined_last.controller(), app::VisibleLines{10});
    applied(declined_last.controller(), app::InsertText{"ab"});
    applied(declined_last.controller(),
            app::EndSession{.reason = app::SessionEnd::last_tab_closed, .unsaved_discarded = true});
    expect(declined_last.autosave().clears() == 1,
           "closing the last tab after answering no clears the autosave once");
}

// 強行の終わり（ADR 0137 の決定 9）を 1 つ走らせ、flush の瞬間の一覧の書きの回数を返す
// （flush が無ければ書きの後の回数のまま）。
[[nodiscard]] std::size_t session_writes_at_flush(Editing &editing, const app::EndSession &intent)
{
    std::size_t at_flush = 0;
    editing.autosave().when_flushed([&] { at_flush = editing.session().writes(); });
    applied(editing.controller(), intent);
    return at_flush;
}

// 確かめずに未保存が残る終わり方は、一覧より先に変わったタブだけを頼み、flush で待ってから一覧を
// 書く。消さない。未保存が残らないか「いいえ」で捨てた終わり方は clear だけで flush しない（決定
// 9・5）。
void verify_end_session_flushes_unsaved()
{
    Editing forced;
    applied(forced.controller(), app::VisibleLines{10});
    applied(forced.controller(), app::InsertText{"ab"});
    applied(forced.controller(), app::AutosaveTick{});
    applied(forced.controller(), app::InsertText{"c"});
    const std::size_t before = forced.session().writes();
    const std::size_t at_flush =
        session_writes_at_flush(forced, app::EndSession{app::SessionEnd::window_closed});
    expect(forced.autosave().calls() == std::vector<std::string>{"store 0", "store 0", "flush"} &&
               bytes_of(forced.autosave().stored().back()) == "abc",
           "a forced ending stores the changed tab once more and then flushes");
    expect(at_flush == before && forced.session().writes() == before + 1,
           "the forced ending flushes before it writes the session list");
    expect(forced.autosave().clears() == 0, "the forced ending keeps the autosave");

    Editing unchanged;
    applied(unchanged.controller(), app::VisibleLines{10});
    applied(unchanged.controller(), app::InsertText{"ab"});
    applied(unchanged.controller(), app::AutosaveTick{});
    static_cast<void>(
        session_writes_at_flush(unchanged, app::EndSession{app::SessionEnd::window_closed}));
    expect(unchanged.autosave().calls() == std::vector<std::string>{"store 0", "flush"},
           "a tab unchanged since its store is not stored again before the flush");

    Editing lost;
    applied(lost.controller(), app::VisibleLines{10});
    applied(lost.controller(), app::InsertText{"ab"});
    const std::size_t lost_before = lost.session().writes();
    const std::size_t lost_at_flush =
        session_writes_at_flush(lost, app::EndSession{app::SessionEnd::rendering_lost});
    expect(lost.autosave().calls() == std::vector<std::string>{"store 0", "flush"} &&
               lost_at_flush == lost_before && lost.autosave().clears() == 0,
           "losing the rendering with an unsaved tab stores, flushes and then writes the list");

    Editing clean;
    applied(clean.controller(), app::VisibleLines{10});
    applied(clean.controller(), app::EndSession{app::SessionEnd::window_closed});
    expect(clean.autosave().flushes() == 0 && clean.autosave().stored().empty() &&
               clean.autosave().clears() == 1,
           "an ending without any unsaved tab only clears");

    Editing declined;
    applied(declined.controller(), app::VisibleLines{10});
    applied(declined.controller(), app::InsertText{"ab"});
    applied(declined.controller(),
            app::EndSession{.reason = app::SessionEnd::rendering_lost, .unsaved_discarded = true});
    expect(declined.autosave().flushes() == 0 && declined.autosave().stored().empty() &&
               declined.autosave().clears() == 1,
           "an ending after answering no only clears without storing or flushing");
}

// 戻したタブは次の合図を待たずにこのプロセスの退避として頼み、その後に残骸を forget する
// （決定 6・F2）。
void verify_restore_stores_before_forget()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    editing.autosave().remain(remnant("5e/0", sample_path()), "alpha");
    editing.autosave().remain(remnant("5e/1", std::nullopt), "beta");
    editing.autosave().remain(remnant("5e/2", std::nullopt), std::nullopt);
    static_cast<void>(controller.autosave_remnants());
    applied(controller, app::RestoreAutosave{});
    expect(editing.autosave().calls() == std::vector<std::string>{"store 1", "store 2",
                                                                  "forget 5e/0", "forget 5e/1",
                                                                  "forget 5e/2"},
           "restoring stores the restored tabs before it forgets any remnant");
}
} // namespace

void verify_autosave_contracts()
{
    verify_plan_active();
    verify_plan_tabs();
    verify_clears_autosave();
    verify_tick_stores_changes();
    verify_tick_encodes_and_discards();
    verify_failure_notified_once();
    verify_failed_store_retried();
    verify_tick_skips_oversized();
    verify_tick_keeps_wanted_column();
    verify_restore_remnants();
    verify_restored_tabs_autosave();
    verify_restore_caret();
    verify_restore_unreadable();
    verify_discard_remnants();
    verify_end_session_keeps_unsaved();
    verify_end_session_flushes_unsaved();
    verify_restore_stores_before_forget();
}

void verify_autosave_scope()
{
    verify_autosave_contracts();
}
} // namespace nenenib::tests
