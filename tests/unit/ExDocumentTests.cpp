// Issue #284 / ADR 0066。Ex の保存・終了と既存の文書操作との境界。
#include "CommandText.hpp"
#include "Editing.hpp"
#include "ExResult.hpp"
#include "HistoryAction.hpp"
#include "InsertText.hpp"
#include "NewTab.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SubmitCommand.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;
using Verb = core::ExDocumentVerb;

bool request_is(std::string_view text, Verb verb)
{
    const auto result =
        core::evaluate_ex(text, core::default_editor_settings(), core::Appearance::dark);
    if (!result)
    {
        return false;
    }
    const auto &document = result.value().document;
    return document.has_value() && document.value().verb == verb &&
           !document.value().path.has_value() && !result.value().tab.has_value() &&
           !result.value().palette.has_value() && !result.value().settings.has_value();
}

std::string message(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : "";
}

void prepare(Editing &editing)
{
    applied(editing.controller(), app::VisibleLines{10});
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
}

void open_named(Editing &editing, std::string bytes = "alpha")
{
    editing.files().hold(Bytes{std::move(bytes)});
    applied(editing.controller(), app::OpenDocument{sample_path()});
    prepare(editing);
}

void verify_names()
{
    for (const std::string_view name : {"w", "wr", "wri", "writ", "write"})
    {
        expect(request_is(name, Verb::write), "write accepts Vim's unambiguous prefixes");
    }
    for (const std::string_view name : {"q", "qu", "qui", "quit"})
    {
        expect(request_is(name, Verb::quit), "quit accepts Vim's prefixes");
        expect(request_is(std::string(name) + "!", Verb::quit_force),
               "a bang turns quit into forced quit");
    }
    expect(request_is("wq", Verb::write_quit), "wq always writes before quitting");
    for (const std::string_view name : {"x", "xi", "xit", "exi", "exit"})
    {
        expect(request_is(name, Verb::update_quit), "xit and exit only write modified documents");
    }
    expect(request_is("  write  ", Verb::write), "outer spaces use the common trimming");
    // Issue #501: Nib has no read-only refusal, so a bang keeps the same condition.
    for (const std::string_view name : {"w!", "write!", "w! "})
    {
        expect(request_is(name, Verb::write), "a forced write is the same write");
    }
    expect(request_is("wq!", Verb::write_quit), "a forced wq is the same write and quit");
    for (const std::string_view name : {"x!", "xit!", "exi!", "exit!"})
    {
        expect(request_is(name, Verb::update_quit), "a forced xit is the same update and quit");
    }
    expect(core::command_completions("q") == std::vector<std::string>{"quit", "quit!"},
           "quit and forced quit share the command catalog");
    expect(core::command_completions("w") == std::vector<std::string>{"write", "wq"},
           "the same table produces save completion");
    expect(core::command_completions("wq!").empty() && core::command_completions("write!").empty(),
           "a bang that keeps the same condition is not listed as a separate command");
}

void verify_rejections()
{
    for (const std::string_view input : {"w!!",
                                         "w!other",
                                         "wq!!",
                                         "saveas!",
                                         "saveas! ",
                                         "q!!",
                                         "q !",
                                         "w ++enc=utf8 other.txt",
                                         "w >> other.txt",
                                         "w !cmd",
                                         "q other",
                                         "wq! other",
                                         "x! other",
                                         "2q",
                                         "1w",
                                         "1,2w",
                                         "%w",
                                         "w|q",
                                         "quitx",
                                         "wqall",
                                         "ex",
                                         "W"})
    {
        expect(!core::evaluate_ex(input, core::default_editor_settings(), core::Appearance::dark),
               "unsupported syntax is never executed as a current-document operation");
    }
    expect(!core::evaluate_ex("w\nq", core::default_editor_settings(), core::Appearance::dark),
           "document commands preserve the one-line boundary");
    expect(!core::evaluate_ex(std::string(257, 'q'), core::default_editor_settings(),
                              core::Appearance::dark),
           "document commands preserve the length limit");
}

void verify_write_and_undo()
{
    Editing editing;
    auto &controller = editing.controller();
    open_named(editing);
    vim_replay(controller, "yyA!<Esc>");
    const auto before = controller.frame();
    const auto held = controller.vim_state().unnamed_register.value().text;
    const auto saved = run_ex(controller, "write");
    expect(editing.files().written() == "alpha!" &&
               editing.files().written_path() == sample_path().text(),
           "write uses the active path and current bytes");
    expect(saved.document.save_state == core::SaveState::saved && message(saved) == "Written" &&
               !saved.closing,
           "a successful write stays open and reports success");
    expect(saved.caret == before.caret &&
               controller.vim_state().unnamed_register.value().text == held,
           "saving keeps the caret and register");
    expect(applied(controller, app::HistoryAction{core::HistoryDirection::undo}) == "alpha",
           "saving keeps the previous undo entry");
    expect(controller.frame().document.save_state == core::SaveState::modified,
           "undoing before the save point is modified");
    expect(applied(controller, app::HistoryAction{core::HistoryDirection::redo}) == "alpha!" &&
               controller.frame().document.save_state == core::SaveState::saved,
           "redoing to the save point clears the modified mark");
}

void verify_quit_guards()
{
    Editing editing;
    auto &controller = editing.controller();
    open_named(editing);
    vim_replay(controller, "A!<Esc>");
    const auto blocked = run_ex(controller, "q");
    expect(message(blocked) == "E37: No write since last change (add ! to override)" &&
               !blocked.closing && blocked.tabs.size() == 1 && vim_body(blocked) == "alpha!",
           "quit refuses unsaved changes without a dialog request");
    expect(!blocked.document.last_failure.has_value() && !blocked.close_request.has_value(),
           "the Ex error is the only failure channel");
    const auto forced = run_ex(controller, "q!");
    expect(forced.closing && forced.tabs.size() == 1 && editing.files().written_path().empty(),
           "forced quit closes the last tab without writing");
    expect(!controller.apply_frame(app::VisibleLines{10}).closing,
           "closing lasts for one intent only");
}

void verify_clean_and_untitled()
{
    for (const std::string_view command : {"q", "q!", "x", "xit", "exit"})
    {
        Editing editing;
        prepare(editing);
        expect(run_ex(editing.controller(), std::string(command)).closing &&
                   editing.files().written_path().empty(),
               "an untouched untitled tab can quit without writing");
    }
    for (const std::string_view command : {"w", "wq"})
    {
        Editing editing;
        prepare(editing);
        const auto frame = run_ex(editing.controller(), std::string(command));
        expect(message(frame) == "E32: No file name" && !frame.closing &&
                   editing.files().written_path().empty(),
               "write requires a name even for an empty untitled document");
    }
    Editing dirty;
    prepare(dirty);
    vim_replay(dirty.controller(), "ibody<Esc>");
    const auto failed = run_ex(dirty.controller(), "x");
    expect(message(failed) == "E32: No file name" && !failed.closing && vim_body(failed) == "body",
           "xit keeps a modified untitled document if it cannot be written");
    Editing clean;
    open_named(clean);
    clean.files().refuse_writes(FileFailure::unwritable);
    expect(run_ex(clean.controller(), "x").closing,
           "xit skips writing an unchanged named document");
}

void verify_write_quit()
{
    for (const std::string_view command : {"wq", "x", "wq!", "x!", "exit!"})
    {
        Editing editing;
        open_named(editing);
        vim_replay(editing.controller(), "A!<Esc>");
        const auto frame = run_ex(editing.controller(), std::string(command));
        expect(frame.closing && editing.files().written() == "alpha!" &&
                   frame.document.save_state == core::SaveState::saved,
               "write-and-quit only closes after successful saving");
    }
    Editing unchanged;
    open_named(unchanged);
    expect(run_ex(unchanged.controller(), "wq").closing && unchanged.files().written() == "alpha",
           "wq writes even when the document is unchanged");
    Editing forced;
    open_named(forced);
    vim_replay(forced.controller(), "A!<Esc>");
    const auto written = run_ex(forced.controller(), "w!");
    expect(!written.closing && forced.files().written() == "alpha!" &&
               written.document.save_state == core::SaveState::saved &&
               message(written) == "Written",
           "a forced write saves and stays open like write");
}

void verify_write_failures()
{
    for (const auto failure :
         {FileFailure::not_found, FileFailure::access_denied, FileFailure::unreadable,
          FileFailure::unwritable, FileFailure::too_large, FileFailure::undecodable,
          FileFailure::unencodable})
    {
        for (const std::string_view command : {"w", "wq", "x", "wq!", "x!"})
        {
            Editing editing;
            open_named(editing);
            vim_replay(editing.controller(), "A!<Esc>");
            editing.files().refuse_writes(failure);
            const auto frame = run_ex(editing.controller(), std::string(command));
            expect(!frame.closing && frame.tabs.size() == 1 && vim_body(frame) == "alpha!" &&
                       frame.document.save_state == core::SaveState::modified,
                   "every write failure preserves the unsaved document");
            expect(message(frame).starts_with("Could not write") &&
                       !frame.document.last_failure.has_value() &&
                       editing.files().written_path().empty(),
                   "Ex write failures stay inline and never claim success");
            expect(applied(editing.controller(),
                           app::HistoryAction{core::HistoryDirection::undo}) == "alpha",
                   "a failed save preserves undo");
        }
    }
}

void verify_encodings()
{
    Editing bom;
    open_named(bom, "\xEF\xBB\xBF日本語\r\n");
    static_cast<void>(run_ex(bom.controller(), "w"));
    expect(bom.files().written() == "\xEF\xBB\xBF日本語\r\n", "Ex preserves BOM and CRLF");
    Editing japanese;
    japanese.code_pages().decode_to(std::string("日"));
    japanese.code_pages().encode_to(std::string("\x93\xFA"));
    open_named(japanese, "\x93\xFA");
    static_cast<void>(run_ex(japanese.controller(), "w"));
    expect(japanese.files().written() == "\x93\xFA" && japanese.code_pages().encoded_from() == "日",
           "Ex uses the existing CP932 conversion");
    vim_replay(japanese.controller(), "A!<Esc>");
    japanese.code_pages().encode_to(std::unexpected(CodePageFailure::unencodable));
    const auto frame = run_ex(japanese.controller(), "wq");
    expect(!frame.closing && message(frame).find("encoding") != std::string::npos &&
               frame.document.encoding == core::TextEncoding::shift_jis &&
               japanese.files().written() == "\x93\xFA",
           "conversion failure neither rewrites bytes nor silently changes the encoding");
}

void verify_only_current_tab()
{
    Editing editing;
    auto &controller = editing.controller();
    open_named(editing);
    vim_replay(controller, "A!<Esc>");
    applied(controller, app::NewTab{});
    vim_replay(controller, "isecond<Esc>");
    const auto refused = run_ex(controller, "q");
    expect(refused.tabs.size() == 2 && refused.active_tab == 1 && !refused.closing,
           "quit cannot discard the current modified tab");
    const auto closed = run_ex(controller, "q!");
    expect(closed.tabs.size() == 1 && vim_body(closed) == "alpha!" && !closed.closing &&
               closed.document.save_state == core::SaveState::modified,
           "forced quit preserves the other unsaved tab");
    expect(applied(controller, app::HistoryAction{core::HistoryDirection::undo}) == "alpha",
           "the other tab retains its undo history");
}

void verify_palette_write()
{
    Editing editing;
    open_named(editing);
    auto &controller = editing.controller();
    applied(controller, app::OpenCommandPalette{});
    applied(controller, app::CommandText{":write"});
    const auto frame = controller.apply_frame(app::SubmitCommand{});
    expect(editing.files().written() == "alpha" && !frame.command_palette.has_value() &&
               !frame.closing,
           "the shared command palette executes the same write operation");
}
// 行番号だけの命令の答え（ADR 0022 の追補・Issue #847）。値が無いのは「当たらなかった」。
std::optional<std::optional<std::size_t>> line_request(std::string_view text)
{
    const auto result =
        core::evaluate_ex(text, core::default_editor_settings(), core::Appearance::dark);
    if (!result)
    {
        return std::nullopt;
    }
    const auto &line = result.value().line;
    if (!line.has_value())
    {
        return std::nullopt;
    }
    return line.value().line;
}

void verify_line_numbers()
{
    expect(line_request("3") == std::optional{std::optional<std::size_t>{3}},
           ":3 is a line request for line 3");
    expect(line_request("0") == std::optional{std::optional<std::size_t>{0}},
           ":0 keeps the written 0 (the engine lands on line 1)");
    expect(line_request("$") ==
               std::optional<std::optional<std::size_t>>{std::optional<std::size_t>{}},
           ":$ is a line request without a number (the last line)");
    expect(line_request("007") == std::optional{std::optional<std::size_t>{7}},
           "leading zeros are read as decimal");
    expect(line_request(" 12 ") == std::optional{std::optional<std::size_t>{12}},
           "surrounding blanks are trimmed like every other command");
    expect(line_request("99999999999999999999999") ==
               std::optional{std::optional<std::size_t>{std::numeric_limits<std::size_t>::max()}},
           "a number beyond size_t saturates (the engine clamps to the last line)");
    for (const std::string_view input :
         {"3,5", "+3", "-3", "'a", "3d", "3 4", "$$", "3$", ".", "%"})
    {
        const auto before =
            core::evaluate_ex(input, core::default_editor_settings(), core::Appearance::dark);
        expect(!before.has_value(), "ranges, offsets, marks and line commands stay unsupported");
    }
    expect(!line_request("tabnext 3").has_value() && !line_request("set fontsize=12").has_value(),
           "other commands never carry a line request");
}

// `:N` は鍵の `{N}G` と同じ着地で、知らせも警告音も無い。
void verify_line_jump()
{
    constexpr std::string_view body = "one\n  two\nthree\n    four\nfive";
    using Case = std::pair<std::string_view, std::string_view>;
    constexpr std::array<Case, 4> cases{
        {Case{"3", "3G"}, Case{"0", "1G"}, Case{"$", "G"}, Case{"8000", "G"}}};
    for (const auto &[command, keys] : cases)
    {
        Editing by_ex;
        open_named(by_ex, std::string(body));
        vim_replay(by_ex.controller(), "jll");
        const auto jumped = run_ex(by_ex.controller(), std::string(command));
        Editing by_key;
        open_named(by_key, std::string(body));
        vim_replay(by_key.controller(), "jll");
        vim_replay(by_key.controller(), keys);
        const auto pressed = by_key.controller().frame();
        expect(jumped.caret == pressed.caret, "a line command lands where {N}G lands");
        expect(message(jumped).empty() && by_ex.bell().rings() == 0,
               "a line command is silent and never rings");
    }
    // Vim 9.1 の `:N` は jump ではなく `''` を置かない（`:help jump-motions`・oracle の
    // ex-line-context-mark）。`''` は直前の跳び（ここでは `G`）の前の位置のまま。
    Editing editing;
    open_named(editing, std::string(body));
    vim_replay(editing.controller(), "jllG");
    static_cast<void>(run_ex(editing.controller(), "2"));
    vim_replay(editing.controller(), "''");
    Editing jumped_only;
    open_named(jumped_only, std::string(body));
    vim_replay(jumped_only.controller(), "jllG''");
    expect(editing.controller().frame().caret == jumped_only.controller().frame().caret,
           "a line command is not a jump and leaves the context mark of the last jump");
}
} // namespace

void verify_ex_document_contracts()
{
    verify_names();
    verify_rejections();
    verify_write_and_undo();
    verify_quit_guards();
    verify_clean_and_untitled();
    verify_write_quit();
    verify_write_failures();
    verify_encodings();
    verify_only_current_tab();
    verify_palette_write();
    verify_line_numbers();
    verify_line_jump();
}

void verify_ex_document_scope()
{
    verify_ex_document_contracts();
    verify_document_save_contracts();
}
} // namespace nenenib::tests
