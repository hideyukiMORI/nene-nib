// 契約 `--finder-preview`（#1793・ADR 0189 の段 P2 の決定 4・8）。替え玉 ScriptedPreview で、開いて
// いないファイルの窓を port で読む道を確かめる: 券（選択が動くたびに新しい券・前の券を取り消す）・
// 古い券の結果を捨てる・直近 8 つの保持（戻ったときは port を呼ばない・9 つ目で最も古いものを
// 捨てる）・作れない理由の 1 行・面を閉じると取り消して全部捨てる・欄を描かない窓では頼まない・
// 窓の作り方（構文の色・最後の空の行・書式用文字の見え方）。
#include "CancelCommand.hpp"
#include "CommandEdit.hpp"
#include "CommandPaletteView.hpp"
#include "CommandText.hpp"
#include "EditCommand.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "Language.hpp"
#include "OpenProjectSearch.hpp"
#include "PreviewAnchor.hpp"
#include "PreviewBatch.hpp"
#include "PreviewFailure.hpp"
#include "PreviewLine.hpp"
#include "PreviewLines.hpp"
#include "PreviewSnippet.hpp"
#include "ProjectHit.hpp"
#include "ProjectSearchBatch.hpp"
#include "ProjectSearchHit.hpp"
#include "ProjectSearchPhase.hpp"
#include "ProjectSearchProgress.hpp"
#include "Scopes.hpp"
#include "ScriptedPreview.hpp"
#include "ScriptedProjectSearch.hpp"
#include "SyntaxRole.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <cstdint>
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

[[nodiscard]] core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

[[nodiscard]] std::string file_of(std::size_t index)
{
    return "C:\\proj\\f" + std::to_string(index) + ".txt";
}

// 開いていない count 個のファイルの当たり（行 index + 1）で全文検索の面を開いた editing。
void searched(Editing &editing, std::size_t count)
{
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10, core::TextViewGeometry{600, 20, 96, 0}, 1600, 1200});
    applied(controller, app::OpenProjectSearch{});
    applied(controller, app::CommandText{"x"});
    std::vector<app::ProjectSearchHit> hits;
    for (std::size_t index = 0; index < count; ++index)
    {
        hits.push_back(app::ProjectSearchHit{
            path_of(file_of(index)),
            core::ProjectHit{core::LineNumber{index + 1}, core::Column{1}, "x", 0, 0, 1}});
    }
    auto &port = editing.project_search();
    port.serve(app::ProjectSearchBatch{port.requests().back().ticket,
                                       std::move(hits),
                                       {app::ProjectSearchPhase::complete, count, count}});
    applied(controller, app::WorkCompleted{});
}

[[nodiscard]] Editing rooted_editing()
{
    auto settings = core::default_editor_settings();
    settings.root = path_of("C:\\proj");
    return Editing(settings);
}

[[nodiscard]] std::optional<app::PreviewSnippet> preview_of(app::EditorController &controller)
{
    const auto frame = controller.frame();
    return frame.command_palette.has_value() ? frame.command_palette.value().preview : std::nullopt;
}

[[nodiscard]] app::PreviewBatch lines_for(std::uint64_t ticket, std::string text)
{
    std::vector<app::PreviewLine> lines;
    app::PreviewLine line{core::LineNumber{1}, text};
    line.source = std::move(text);
    lines.push_back(std::move(line));
    return app::PreviewBatch{ticket, std::move(lines)};
}

[[nodiscard]] std::string first_text(app::EditorController &controller)
{
    const auto preview = preview_of(controller);
    return preview.has_value() && !preview.value().lines.empty()
               ? preview.value().lines.front().text
               : std::string("-");
}

void move(app::EditorController &controller, core::CommandEdit edit)
{
    applied(controller, app::EditCommand{edit});
}

// 選択が動くたびに新しい券で頼み、前の券を取り消す。古い券の結果は捨て、今の券の結果だけを出す。
// 見出しは動いた瞬間に新しい候補のもの・中身は届くまで空。
void verify_tickets()
{
    Editing editing = rooted_editing();
    auto &controller = editing.controller();
    searched(editing, 2);
    auto &port = editing.previews();
    expect(port.requests().size() == 1 && port.requests().back().path.text() == file_of(0) &&
               port.requests().back().anchor.has_value() &&
               port.requests()
                       .back()
                       .anchor.value_or(app::PreviewAnchor{core::LineNumber{0}, 0, 0})
                       .line.value == 1 &&
               port.requests().back().language == core::Language::plain,
           "the selected hit in a file that is not open asks the port around the hit");
    const auto first = port.requests().back().ticket;
    move(controller, core::CommandEdit::complete_next);
    const auto second = port.requests().back().ticket;
    const auto heading = preview_of(controller);
    expect(port.requests().size() == 2 && second != first && port.cancels().size() == 1 &&
               port.cancels().back() == first && heading.has_value() &&
               heading.value().name.text() == "f1.txt" && heading.value().lines.empty(),
           "moving the selection cancels the old ticket, asks again and shows the new heading "
           "with no content");
    port.serve(lines_for(first, "stale"));
    applied(controller, app::WorkCompleted{});
    expect(first_text(controller) == "-", "a result of an old ticket is thrown away");
    port.serve(lines_for(second, "second"));
    applied(controller, app::WorkCompleted{});
    expect(first_text(controller) == "second", "the result of the live ticket is shown");
    move(controller, core::CommandEdit::complete_previous);
    expect(port.requests().size() == 3 && first_text(controller) == "-",
           "going back to a file whose result was thrown away asks again");
    port.serve(lines_for(port.requests().back().ticket, "first"));
    applied(controller, app::WorkCompleted{});
    move(controller, core::CommandEdit::complete_next);
    expect(port.requests().size() == 3 && first_text(controller) == "second",
           "going back to a held window shows it without the port");
}

// 直近 8 つを持ち、9 つ目で最も古いものを捨てる。
void verify_held_limit()
{
    Editing editing = rooted_editing();
    auto &controller = editing.controller();
    searched(editing, 10);
    auto &port = editing.previews();
    for (std::size_t index = 0; index < 9; ++index)
    {
        port.serve(lines_for(port.requests().back().ticket, "w" + std::to_string(index)));
        applied(controller, app::WorkCompleted{});
        move(controller, core::CommandEdit::complete_next);
    }
    const auto asked = port.requests().size();
    move(controller, core::CommandEdit::complete_previous);
    expect(port.requests().size() == asked && first_text(controller) == "w8",
           "the newest held window comes back without the port");
    move(controller, core::CommandEdit::list_first);
    expect(port.requests().size() == asked + 1 && first_text(controller) == "-",
           "the oldest of nine windows was dropped and is asked again");
}

// 作れない理由は 1 行の文。面を閉じると今の券を取り消し、保持を全部捨てる。
void verify_failure_and_close()
{
    Editing editing = rooted_editing();
    auto &controller = editing.controller();
    searched(editing, 2);
    auto &port = editing.previews();
    port.serve(app::PreviewBatch{port.requests().back().ticket,
                                 std::unexpected(app::PreviewFailure::binary)});
    applied(controller, app::WorkCompleted{});
    const auto failed = preview_of(controller);
    expect(failed.has_value() && failed.value().lines.empty() &&
               failed.value().notice.has_value() &&
               failed.value().notice.value().text() ==
                   core::ui_text(core::UiMessage::palette_preview_binary,
                                 core::DisplayLanguage::japanese),
           "a file that cannot be previewed shows one line");
    move(controller, core::CommandEdit::complete_next);
    const auto live = port.requests().back().ticket;
    applied(controller, app::CancelCommand{});
    expect(port.cancels().back() == live, "closing the palette cancels the live ticket");
    const auto asked = port.requests().size();
    applied(controller, app::OpenProjectSearch{});
    applied(controller, app::CommandText{"x"});
    auto &search = editing.project_search();
    search.serve(app::ProjectSearchBatch{
        search.requests().back().ticket,
        {app::ProjectSearchHit{
            path_of(file_of(0)),
            core::ProjectHit{core::LineNumber{1}, core::Column{1}, "x", 0, 0, 1}}},
        {app::ProjectSearchPhase::complete, 1, 1}});
    applied(controller, app::WorkCompleted{});
    expect(port.requests().size() == asked + 1,
           "after closing the palette nothing is held and the same file is asked again");
}

// 面を閉じる直前に届いていた batch は、次の WorkCompleted で捨てて棚を空にする（V-1794 の必須 1）。
// 開き直して選んだ窓はまた届く。
void verify_drained_after_close()
{
    Editing editing = rooted_editing();
    auto &controller = editing.controller();
    searched(editing, 1);
    auto &port = editing.previews();
    port.serve(lines_for(port.requests().back().ticket, "late"));
    applied(controller, app::CancelCommand{});
    applied(controller, app::WorkCompleted{});
    expect(port.pending() == 0, "a batch that came before the palette closed is drained");
    searched(editing, 1);
    port.serve(lines_for(port.requests().back().ticket, "again"));
    applied(controller, app::WorkCompleted{});
    expect(first_text(controller) == "again", "the window of the reopened palette arrives");
    applied(controller, app::WorkCompleted{});
    const auto collects = port.collects();
    applied(controller, app::WorkCompleted{});
    expect(port.collects() == collects, "once the shelf is empty the port is not asked again");
}

// 同じ path・同じ行で範囲の違う当たりは、保持の窓の本文を使い、印を今の当たりから付け直す
// （V-1794 の必須 2）。
void verify_marks_follow_the_hit()
{
    Editing editing = rooted_editing();
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10, core::TextViewGeometry{600, 20, 96, 0}, 1600, 1200});
    applied(controller, app::OpenProjectSearch{});
    auto &search = editing.project_search();
    auto &port = editing.previews();
    const auto found = [&](std::size_t end)
    {
        search.serve(app::ProjectSearchBatch{
            search.requests().back().ticket,
            {app::ProjectSearchHit{
                path_of(file_of(0)),
                core::ProjectHit{core::LineNumber{1}, core::Column{3}, "a foo", 0, 2, end}}},
            {app::ProjectSearchPhase::complete, 1, 1}});
        applied(controller, app::WorkCompleted{});
    };
    applied(controller, app::CommandText{"fo"});
    found(4);
    port.serve(lines_for(port.requests().back().ticket, "a foo"));
    applied(controller, app::WorkCompleted{});
    applied(controller, app::CommandText{"o"});
    found(5);
    const auto preview = preview_of(controller);
    expect(port.requests().size() == 1 && preview.has_value() && !preview.value().lines.empty() &&
               preview.value().lines.front().hit &&
               preview.value().lines.front().match_begin == 2 &&
               preview.value().lines.front().match_end == 5,
           "a held window takes the marks of the current hit without asking the port again");
}

// 欄を描かない窓（640 × 360 DIP・automatic で off）と置き方 off では port を呼ばない。
void verify_not_drawn()
{
    Editing editing = rooted_editing();
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10, core::TextViewGeometry{600, 20, 96, 0}, 640, 360});
    applied(controller, app::OpenProjectSearch{});
    applied(controller, app::CommandText{"x"});
    auto &search = editing.project_search();
    search.serve(app::ProjectSearchBatch{
        search.requests().back().ticket,
        {app::ProjectSearchHit{
            path_of(file_of(0)),
            core::ProjectHit{core::LineNumber{1}, core::Column{1}, "x", 0, 0, 1}}},
        {app::ProjectSearchPhase::complete, 1, 1}});
    applied(controller, app::WorkCompleted{});
    expect(editing.previews().touches() == 0,
           "a window that draws no pane never asks the preview port");
}

// 窓の作り方（preview_lines の 1 本）。構文の色・最後の空の行・書式用文字の見え方。
void verify_window_lines()
{
    const auto text = core::TextBuffer::from_utf8("return a; // c\nx\xE2\x80\xAEy\n").value();
    const auto lines = app::preview_lines(text, std::nullopt, core::Language::cpp);
    expect(lines.size() == 2, "the empty line after the final line break is not shown");
    const auto &first = lines.front();
    const bool keyword = !first.colors.empty() && first.colors.front().begin == 0 &&
                         first.colors.front().end == 6 &&
                         first.colors.front().role == core::SyntaxRole::keyword;
    expect(keyword, "the window carries the syntax colors of its lines");
    expect(lines.at(1).text == "x<202e>y" && lines.at(1).colors.empty(),
           "format characters look the same as in the body");
    const auto marked =
        app::preview_lines(core::TextBuffer::from_utf8("\xE2\x80\xAE"
                                                       "ab needle")
                               .value(),
                           app::PreviewAnchor{core::LineNumber{1}, 6, 12}, core::Language::plain);
    expect(marked.size() == 1 && marked.front().hit &&
               marked.front().text.substr(marked.front().match_begin,
                                          marked.front().match_end - marked.front().match_begin) ==
                   "needle",
           "the hit range follows the display form");
}
} // namespace

void verify_finder_preview_contracts()
{
    verify_tickets();
    verify_held_limit();
    verify_failure_and_close();
    verify_drained_after_close();
    verify_marks_follow_the_hit();
    verify_not_drawn();
    verify_window_lines();
}
} // namespace nenenib::tests
