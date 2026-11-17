// scope `--project-search-controller` の単体テスト（ADR 0167 の決定 1・2・4・5・7 の application の
// 分）。替え玉 ScriptedProjectSearch で、券・途中の batch・古い券の結果を捨てる・取消・1,000 件・
// open_listed と位置・ルートが無いとき・無題・面を閉じた後に届いた batch を確かめる。
#include "ActivateCommandChoice.hpp"
#include "CancelCommand.hpp"
#include "CommandEdit.hpp"
#include "CommandPalette.hpp"
#include "CommandPaletteView.hpp"
#include "CommandText.hpp"
#include "EditCommand.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "FilePath.hpp"
#include "FirstFramePresented.hpp"
#include "InsertText.hpp"
#include "NewTab.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "OpenProjectSearch.hpp"
#include "PaletteOrigin.hpp"
#include "PlaceCaret.hpp"
#include "ProjectHit.hpp"
#include "ProjectHitRows.hpp"
#include "ProjectSearchBatch.hpp"
#include "ProjectSearchHit.hpp"
#include "ProjectSearchPhase.hpp"
#include "ProjectSearchProgress.hpp"
#include "ProjectSearchRules.hpp"
#include "ProjectSearchStatus.hpp"
#include "Scopes.hpp"
#include "ScriptedProjectSearch.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "SubmitCommand.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "ToggleBookmark.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"
#include "VimMarkPosition.hpp"
#include "VimMarks.hpp"
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

[[nodiscard]] Editing rooted_editing()
{
    auto settings = core::default_editor_settings();
    settings.root = path_of("C:\\proj");
    return Editing(settings);
}

[[nodiscard]] app::ProjectSearchHit hit_at(std::string_view file, std::size_t line,
                                           std::size_t column, std::string text)
{
    const std::size_t size = text.size();
    return app::ProjectSearchHit{path_of(file),
                                 core::ProjectHit{core::LineNumber{line}, core::Column{column},
                                                  std::move(text), 0, 0, size}};
}

[[nodiscard]] app::ProjectSearchBatch batch_of(std::uint64_t ticket,
                                               std::vector<app::ProjectSearchHit> hits,
                                               app::ProjectSearchProgress progress)
{
    return app::ProjectSearchBatch{ticket, std::move(hits), progress};
}

// 面の i 番目の行の名前（無ければ空）。
[[nodiscard]] std::string label_at(app::EditorController &controller, std::size_t index)
{
    const auto frame = controller.frame();
    if (!frame.command_palette.has_value() || index >= frame.command_palette.value().rows.size())
    {
        return {};
    }
    return std::string(frame.command_palette.value().rows.at(index).label.text());
}

[[nodiscard]] std::size_t palette_count(app::EditorController &controller)
{
    const auto frame = controller.frame();
    return frame.command_palette.has_value() ? frame.command_palette.value().total : 0;
}

[[nodiscard]] std::optional<app::ProjectSearchStatus> status_of(app::EditorController &controller)
{
    const auto frame = controller.frame();
    if (!frame.command_palette.has_value() || !frame.command_palette.value().project.has_value())
    {
        return std::nullopt;
    }
    return frame.command_palette.value().project.value().status;
}

// 面の状態の 1 行（入力欄の右か候補の無い行に ui が描く・ADR 0167 の G3 の追補 5）。面の下の行の
// 知らせ（command_message）には載せない。
[[nodiscard]] std::string message_of(app::EditorController &controller)
{
    const auto frame = controller.frame();
    if (!frame.command_palette.has_value() || !frame.command_palette.value().project.has_value())
    {
        return std::string();
    }
    return std::string(frame.command_palette.value().project.value().line.text());
}

[[nodiscard]] std::string input_of(app::EditorController &controller)
{
    const auto frame = controller.frame();
    return frame.command_line.has_value() ? frame.command_line.value().text : std::string("-");
}

void open_search(Editing &editing, std::string_view text)
{
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::OpenProjectSearch{});
    for (const char letter : text)
    {
        applied(controller, app::CommandText{std::string(1, letter)});
    }
}

// 打つたびに新しい券で頼み、前の券を取り消す（決定 1）。古い券の batch は捨て、今の券の途中の
// batch と最後の batch を届いた順に面の後ろへ足す（決定 4）。
void verify_tickets_and_batches()
{
    Editing editing = rooted_editing();
    auto &controller = editing.controller();
    auto &port = editing.project_search();
    open_search(editing, "ab");
    expect(input_of(controller) == "!ab" && port.requests().size() == 2 &&
               port.cancels().size() == 1 &&
               port.cancels().front() == port.requests().front().ticket &&
               port.requests().back().ticket != port.requests().front().ticket &&
               port.requests().back().root.text() == "C:\\proj",
           "each typed letter asks with a new ticket under the root and cancels the previous one");
    const auto old_ticket = port.requests().front().ticket;
    const auto ticket = port.requests().back().ticket;
    port.serve(batch_of(old_ticket, {hit_at("C:\\proj\\old.txt", 1, 1, "a")},
                        {app::ProjectSearchPhase::searching, 1, 1}));
    port.serve(batch_of(
        ticket,
        {hit_at("C:\\proj\\src\\b.txt", 2, 4, "yy ab"), hit_at("C:\\proj\\c.txt", 7, 1, "ab\tcd")},
        {app::ProjectSearchPhase::searching, 2, 5}));
    applied(controller, app::WorkCompleted{});
    expect(palette_count(controller) == 2 &&
               status_of(controller) == app::ProjectSearchStatus::searching &&
               message_of(controller) ==
                   core::ui_text_filled(core::UiMessage::palette_project_searching,
                                        core::DisplayLanguage::japanese, {"2", "5"}),
           "the old ticket's batch is dropped and the current batch's hits arrive while "
           "searching");
    expect(!controller.frame().command_message.has_value(),
           "the search line is not repeated on the face's bottom row");
    const auto frame = controller.frame();
    const auto view = frame.command_palette.value_or(app::CommandPaletteView{});
    expect(view.hit_rows.size() == 2 && view.hit_rows.at(0).path == "src\\b.txt" &&
               view.hit_rows.at(0).line.value == 2 && view.hit_rows.at(0).column.value == 4 &&
               view.hit_rows.at(1).text == "ab cd" && view.rows.size() == 2 &&
               view.rows.at(0).label.text() == "src\\b.txt:2:4" &&
               view.rows.at(0).origin == core::PaletteOrigin::project_hit,
           "the hit rows carry the relative path, the place and the window with tabs as spaces");
    port.serve(batch_of(ticket, {hit_at("C:\\proj\\d.txt", 3, 2, "xab")},
                        {app::ProjectSearchPhase::complete, 5, 5}));
    applied(controller, app::WorkCompleted{});
    expect(palette_count(controller) == 3 &&
               status_of(controller) == app::ProjectSearchStatus::complete &&
               label_at(controller, 2) == "d.txt:3:2" &&
               message_of(controller) ==
                   core::ui_text_filled(core::UiMessage::palette_project_complete,
                                        core::DisplayLanguage::japanese, {"3", "5"}),
           "the last batch appends in arrival order and finishes the search");
    applied(controller, app::CommandText{"c"});
    expect(palette_count(controller) == 0 && port.requests().size() == 3 &&
               port.cancels().size() == 1,
           "a new query drops the previous hits, and a finished ticket is not cancelled");
}

// 当たりは 1,000 件まで。届いた分が上限を越えたら残りを捨てて取り消し、打ち切りにする。
void verify_hit_limit()
{
    Editing editing = rooted_editing();
    auto &controller = editing.controller();
    auto &port = editing.project_search();
    open_search(editing, "e");
    const auto ticket = port.requests().back().ticket;
    std::vector<app::ProjectSearchHit> hits;
    for (std::size_t line = 1; line <= core::project_search_hit_limit + 5; ++line)
    {
        hits.push_back(hit_at("C:\\proj\\a.txt", line, 1, "e"));
    }
    port.serve(batch_of(ticket, std::move(hits), {app::ProjectSearchPhase::searching, 1, 9}));
    applied(controller, app::WorkCompleted{});
    expect(palette_count(controller) == core::project_search_hit_limit &&
               status_of(controller) == app::ProjectSearchStatus::truncated &&
               port.cancels().size() == 1 && port.cancels().back() == ticket &&
               message_of(controller) ==
                   core::ui_text_filled(core::UiMessage::palette_project_truncated,
                                        core::DisplayLanguage::japanese, {"1000"}),
           "the hits stop at the limit, the request is cancelled and the face says so");
    port.serve(batch_of(ticket, {hit_at("C:\\proj\\b.txt", 1, 1, "e")},
                        {app::ProjectSearchPhase::complete, 2, 9}));
    applied(controller, app::WorkCompleted{});
    expect(palette_count(controller) == core::project_search_hit_limit,
           "a batch of the cancelled ticket adds nothing");
}

// 面を閉じると取り消し、閉じた後に届いた batch は捨てる（決定 6）。ほかの出どころへ移っても同じ。
void verify_closing_cancels()
{
    Editing editing = rooted_editing();
    auto &controller = editing.controller();
    auto &port = editing.project_search();
    open_search(editing, "x");
    const auto ticket = port.requests().back().ticket;
    applied(controller, app::CancelCommand{});
    expect(port.cancels().size() == 1 && port.cancels().back() == ticket &&
               !controller.frame().command_palette.has_value() && message_of(controller).empty(),
           "closing the face cancels the live request and drops the progress line");
    port.serve(batch_of(ticket, {hit_at("C:\\proj\\a.txt", 1, 1, "x")},
                        {app::ProjectSearchPhase::complete, 1, 1}));
    applied(controller, app::WorkCompleted{});
    applied(controller, app::OpenProjectSearch{});
    expect(palette_count(controller) == 0 && port.requests().size() == 1 &&
               status_of(controller) == app::ProjectSearchStatus::prompt,
           "a batch that arrives after the face closed is dropped");
    applied(controller, app::CommandText{"y"});
    const auto next = port.requests().back().ticket;
    applied(controller, app::EditCommand{core::CommandEdit::home});
    applied(controller, app::EditCommand{core::CommandEdit::erase});
    expect(port.cancels().back() == next && !status_of(controller).has_value() &&
               port.requests().size() == 2,
           "leaving the `!` source for another source cancels the request");
    // 頼み終えて受け取りきった後の合図は port を呼ばない。
    applied(controller, app::WorkCompleted{});
    const auto touches = port.touches();
    applied(controller, app::WorkCompleted{});
    expect(port.touches() == touches, "a drained port is not collected again");
}

// 検索語が空・解析できない・探す場所が無いときは頼まずに 1 行だけ（決定 2・迷いの答え）。
void verify_unrequested_states()
{
    Editing editing = rooted_editing();
    auto &controller = editing.controller();
    auto &port = editing.project_search();
    open_search(editing, "");
    expect(port.requests().empty() && status_of(controller) == app::ProjectSearchStatus::prompt &&
               message_of(controller) == core::ui_text(core::UiMessage::palette_project_prompt,
                                                       core::DisplayLanguage::japanese),
           "an empty query asks nothing and shows the prompt");
    applied(controller, app::CommandText{"/\\("});
    expect(port.requests().empty() && status_of(controller) == app::ProjectSearchStatus::invalid &&
               !message_of(controller).empty(),
           "a pattern that does not parse asks nothing and shows the failure");
    Editing untitled;
    open_search(untitled, "abc");
    expect(untitled.project_search().requests().empty() &&
               status_of(untitled.controller()) == app::ProjectSearchStatus::unplaced &&
               message_of(untitled.controller()) ==
                   core::ui_text(core::UiMessage::palette_project_no_root,
                                 core::DisplayLanguage::japanese),
           "without a root an untitled document has nowhere to search");
}

// ルートが無ければアクティブな文書のフォルダ（決定 2）。
void verify_document_folder()
{
    Editing editing;
    editing.files().hold_at("C:\\work\\a.txt", Bytes{"a"});
    applied(editing.controller(), app::OpenDocument{path_of("C:\\work\\a.txt")});
    open_search(editing, "q");
    const auto &requests = editing.project_search().requests();
    expect(requests.size() == 1 && requests.back().root.text() == "C:\\work",
           "without a root the search runs in the active document's folder");
}

// Enter は open_listed の 1 本で開き、その行と桁へキャレットを置いて `''` に前の位置を残す
// （決定 5）。開けなければ 1 行の知らせで、置く先は捨てる。
void verify_open_hit()
{
    Editing editing = rooted_editing();
    auto &controller = editing.controller();
    editing.files().hold_at("C:\\proj\\b.txt", Bytes{"one\nyy hello\nthree\nfour"});
    applied(controller, app::VisibleLines{10});
    applied(controller, app::OpenDocument{path_of("C:\\proj\\b.txt")});
    applied(controller, app::PlaceCaret{core::TextPosition{core::LineNumber{4}, core::Column{2}},
                                        core::SelectionAnchoring::collapse});
    applied(controller, app::NewTab{});
    open_search(editing, "hello");
    auto &port = editing.project_search();
    port.serve(batch_of(port.requests().back().ticket,
                        {hit_at("C:\\proj\\missing.txt", 1, 1, "hello"),
                         hit_at("C:\\proj\\b.txt", 2, 4, "yy hello")},
                        {app::ProjectSearchPhase::complete, 2, 2}));
    applied(controller, app::WorkCompleted{});
    applied(controller, app::EditCommand{core::CommandEdit::complete_next});
    applied(controller, app::SubmitCommand{});
    const auto frame = controller.frame();
    expect(!frame.command_palette.has_value() && frame.document.path.has_value() &&
               frame.document.path.value().text() == "C:\\proj\\b.txt" &&
               frame.caret.position.line.value == 2 && frame.caret.position.column.value == 4 &&
               controller.vim_marks()
                       .previous_context.value_or(core::VimMarkPosition{core::LineNumber{0}, 0})
                       .line.value == 4,
           "Enter opens the hit's tab at its line and column and leaves the old place in ''");
    open_search(editing, "hello");
    port.serve(batch_of(port.requests().back().ticket,
                        {hit_at("C:\\proj\\missing.txt", 1, 1, "hello")},
                        {app::ProjectSearchPhase::complete, 1, 1}));
    applied(controller, app::WorkCompleted{});
    applied(controller, app::ActivateCommandChoice{0});
    const auto failed = controller.frame();
    expect(!failed.command_palette.has_value() && failed.command_message.has_value() &&
               failed.document.path.has_value() &&
               failed.document.path.value().text() == "C:\\proj\\b.txt" &&
               failed.caret.position.line.value == 2,
           "a hit whose file cannot be opened says so in one line and moves nothing");
}

// `:grep {pattern}` は `!` の面をその検索語で開く（決定 1）。Ctrl+Shift+F の意図は空の `!` の面。
void verify_entries()
{
    Editing editing = rooted_editing();
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    static_cast<void>(run_ex(controller, "grep  foo bar "));
    expect(input_of(controller) == "!foo bar" && editing.project_search().requests().size() == 1,
           "`:grep` opens the `!` face with the rest of the line and asks once");
    applied(controller, app::CancelCommand{});
    static_cast<void>(run_ex(controller, "gr x"));
    expect(input_of(controller) == "!x" && editing.project_search().requests().size() == 2,
           "`:gr` is the shortest name");
    applied(controller, app::CancelCommand{});
    static_cast<void>(run_ex(controller, R"(grep /foo\|bar)"));
    expect(input_of(controller) == R"(!/foo\|bar)" &&
               editing.project_search().requests().size() == 3,
           "`:grep` keeps a pattern's alternation bar as part of the pattern");
    applied(controller, app::CancelCommand{});
    static_cast<void>(run_ex(controller, "grep a|b"));
    expect(input_of(controller) == "!a|b" && editing.project_search().requests().size() == 4,
           "`:grep` takes a bare `|` as part of the literal");
    applied(controller, app::CancelCommand{});
    static_cast<void>(run_ex(controller, "grep!"));
    expect(!controller.frame().command_palette.has_value() &&
               editing.project_search().requests().size() == 4,
           "`:grep!` is refused");
    applied(controller, app::OpenProjectSearch{});
    expect(input_of(controller) == "!" && editing.project_search().requests().size() == 4,
           "Ctrl+Shift+F opens an empty `!` face and asks nothing yet");
    // 当たりの行はブックマークの対象ではない（設計席の答え）。
    applied(controller, app::ToggleBookmark{});
    expect(editing.bookmarks().writes() == 0 && controller.frame().command_palette.has_value(),
           "the bookmark key does nothing on the `!` face");
}

// 面の入力は検索語で、面の絞り込みの文字ではない（決定 5）。当たりの名前に検索語が無くても、
// 入力を伸ばしても候補は列の順のまま全部出る（core の結果の 1 本）。
void verify_input_is_not_a_filter()
{
    const auto file = path_of("C:\\proj\\a.txt");
    const auto row = core::project_hit_row(
        path_of("C:\\proj"), file,
        core::ProjectHit{core::LineNumber{1}, core::Column{1}, "zzz", 0, 0, 3});
    const std::vector<core::CommandChoice> entries{core::project_hit_choice(file, row),
                                                   core::project_hit_choice(file, row)};
    const auto palette = core::CommandPalette::opened(entries, "!q", core::EditMode::ordinary);
    const auto longer = palette.inserted("w");
    expect(palette.count() == 2 && longer.has_value() && longer.value().count() == 2 &&
               longer.value().dropped(core::PaletteOrigin::project_hit).count() == 0,
           "the `!` input never filters the hits and dropped removes them");
}
// 起動と最初の frame と打鍵の道は port を呼ばない（決定 7・verify_quiet_paths の残り）。
void verify_quiet_start()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::FirstFramePresented{});
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"typed"});
    applied(controller, app::WorkCompleted{});
    expect(editing.project_search().touches() == 0,
           "starting, the first frame, typing and a work signal never touch the search port");
}
} // namespace

void verify_project_search_controller_contracts()
{
    verify_quiet_start();
    verify_tickets_and_batches();
    verify_hit_limit();
    verify_closing_cancels();
    verify_unrequested_states();
    verify_document_folder();
    verify_open_hit();
    verify_entries();
    verify_input_is_not_a_filter();
}
} // namespace nenenib::tests
