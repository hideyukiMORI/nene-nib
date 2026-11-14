// scope `--ex-lines`（ADR 0168 の段 3a・Issue #1654）。`:d` `:y` と `:g` `:v` の字面（lines_command と
// global_command・evaluate_ex）と、oracle の fixture にできない形（報せの文言・段 3a が断る形・
// `.` に積まないこと・undo 1 単位・CRLF の文書・変更の印・クリップボードの替え玉）を controller と
// engine の道で確かめる。本文とキャレットとレジスタの答えの正本は fixture（`ex-delete-` `ex-yank-`
// `ex-global-`・Vim 9.1 の実測）。
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "ExFailure.hpp"
#include "ExGlobalRequest.hpp"
#include "ExLineCommand.hpp"
#include "ExLineVerb.hpp"
#include "ExLinesRequest.hpp"
#include "ExResult.hpp"
#include "ExSubstituteRequest.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "Offset.hpp"
#include "SaveDocument.hpp"
#include "Scopes.hpp"
#include "Selection.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
#include "VimChange.hpp"
#include "VimEditorView.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"

#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
using nenenib::core::ExFailure;
using nenenib::core::ExLineVerb;

constexpr std::string_view seven_lines = "l1 a\n  l2 b\nl3 a\n\tl4 b\nl5 a\n  l6 b\nl7 a";

[[nodiscard]] core::ExLinesRequest unread_lines()
{
    return core::ExLinesRequest{std::nullopt, ExLineVerb::yank_lines, '?', std::size_t{999}};
}

[[nodiscard]] core::ExLinesRequest lines_of(std::string_view text)
{
    const auto parsed = core::lines_command(text);
    return parsed.has_value() && parsed.value().has_value() ? parsed.value().value()
                                                            : unread_lines();
}

[[nodiscard]] bool lines_fail(std::string_view text, ExFailure failure)
{
    const auto parsed = core::lines_command(text);
    return parsed.has_value() && !parsed.value().has_value() && parsed.value().error() == failure;
}

[[nodiscard]] bool global_fails(std::string_view text, ExFailure failure)
{
    const auto parsed = core::global_command(text);
    return parsed.has_value() && !parsed.value().has_value() && parsed.value().error() == failure;
}

void verify_lines_command_text()
{
    const auto plain = lines_of("d");
    expect(plain.verb == ExLineVerb::delete_lines && !plain.register_name.has_value() &&
               !plain.count.has_value(),
           ":d reads with no register and no count");
    const auto full = lines_of("delete a 3");
    expect(full.register_name == std::optional<char>{'a'} &&
               full.count == std::optional<std::size_t>{3},
           ":delete a 3 reads the register and the count");
    expect(lines_of("d a2").count == std::optional<std::size_t>{2} &&
               lines_of("d3").count == std::optional<std::size_t>{3} &&
               !lines_of("d 1").register_name.has_value(),
           "a digit is the count, never a register (Vim's EX_COUNT)");
    expect(lines_of("y A").verb == ExLineVerb::yank_lines &&
               lines_of("y A").register_name == std::optional<char>{'A'} &&
               lines_of("ya").verb == ExLineVerb::yank_lines &&
               lines_of("yank _").register_name == std::optional<char>{'_'},
           ":y :ya :yank read like :d");
    expect(lines_of("d \"note").verb == ExLineVerb::delete_lines &&
               !lines_of("d \"note").register_name.has_value(),
           "a double quote starts a comment, not the unnamed register");
    expect(lines_of("d 99999999999999999999999").count ==
               std::optional<std::size_t>{std::numeric_limits<std::size_t>::max()},
           "a huge count saturates (Vim clamps it to the last line)");
    expect(lines_fail("d!", ExFailure::no_bang_allowed) &&
               lines_fail("y!", ExFailure::no_bang_allowed),
           ":d! and :y! are E477");
    expect(lines_fail("d a b", ExFailure::trailing_characters) &&
               lines_fail("d 2x", ExFailure::trailing_characters) &&
               lines_fail("d .", ExFailure::trailing_characters) &&
               lines_fail("d 2 l", ExFailure::trailing_characters),
           "trailing characters after the register and the count are E488");
    for (const std::string_view text : {"dl", "dp", "dell", "deletep", "d #", "d|1d", "d 2|y"})
    {
        expect(lines_fail(text, ExFailure::unsupported_argument),
               "the print flags, the # register and a following | command are refused");
    }
    for (const std::string_view text : {"da", "display", "dig", "yy", "s/a/b/", "g/a/d"})
    {
        expect(!core::lines_command(text).has_value(), "other commands are not :d or :y");
    }
}

void verify_global_command_text()
{
    const auto parsed = core::global_command("g/a\\/b/d a 2");
    const bool read = parsed.has_value() && parsed.value().has_value();
    const auto *const removal =
        read ? std::get_if<core::ExLinesRequest>(&parsed.value().value().command) : nullptr;
    expect(read && !parsed.value().value().inverted && parsed.value().value().pattern == "a\\/b" &&
               removal != nullptr && removal->register_name == std::optional<char>{'a'} &&
               removal->count == std::optional<std::size_t>{2},
           ":g reads the pattern as typed and the :d command with its register and count");
    for (const std::string_view text : {"g!/a/d", "v/a/d", "vglobal/a/d", "g! /a/d"})
    {
        const auto inverted = core::global_command(text);
        expect(inverted.has_value() && inverted.value().has_value() &&
                   inverted.value().value().inverted,
               ":g! and :v keep the lines that do not match");
    }
    const auto substitute = core::global_command("global#a#s/l/X/g");
    expect(substitute.has_value() && substitute.value().has_value() &&
               substitute.value().value().delimiter == '#' &&
               std::holds_alternative<core::ExSubstituteRequest>(substitute.value().value().command),
           ":g carries the :s family as its command");
    const auto bar = core::global_command("g/a|b/:d");
    expect(bar.has_value() && bar.value().has_value() && bar.value().value().pattern == "a|b",
           "a bar inside the :g pattern is a character and a leading colon is skipped");
}

void verify_global_command_failures()
{
    expect(global_fails("g", ExFailure::global_pattern_missing) &&
               global_fails("g!", ExFailure::global_pattern_missing),
           "a :g without a pattern is E148");
    expect(global_fails("g xaxd", ExFailure::letter_delimiter), "a letter delimiter is E146");
    expect(global_fails("v!/a/d", ExFailure::no_bang_allowed), ":v! is E477");
    for (const std::string_view text :
         {"g/a/", "g/a", "g/a/p", "g/a/normal Ax", "g/a/m0", "g/a/j", "g/a/y", "g/a/.,+1d",
          "g/a/.s/l/X/", "g/a/g/l/d", "g/a/v/l/d", "g/a/s/l/X/n", "g/a/s/l/X/ 2", "g/a/d|1d",
          "g/a/dl", "g\\/d", "g\\&d"})
    {
        expect(global_fails(text, ExFailure::unsupported_argument),
               "commands other than :d and :s inside :g are a closed failure");
    }
    for (const std::string_view text : {"gu", "goto", "vsplit", "vs", "d", "s/a/b/"})
    {
        expect(!core::global_command(text).has_value(), "other commands are not :g");
    }
}

void verify_global_through_evaluate()
{
    const auto ranged =
        core::evaluate_ex("2,5g/a|b/d", core::default_editor_settings(), core::Appearance::dark);
    const bool global = ranged.has_value() && ranged.value().global.has_value();
    expect(global && ranged.value().global.value_or(core::ExGlobalRequest{std::nullopt, false, '/',
                                                                          std::string{},
                                                                          core::ExLinesRequest{}})
                         .range.has_value(),
           "evaluate_ex keeps the range and lets a bar in the :g pattern through");
    const auto yanked = core::evaluate_ex("%y a", core::default_editor_settings(), core::Appearance::dark);
    expect(yanked.has_value() && yanked.value().lines.has_value(), "evaluate_ex reads :%y a");
}

[[nodiscard]] std::string message_of(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string{};
}

[[nodiscard]] std::string ran(app::EditorController &controller, std::string_view keys)
{
    vim_normal(controller, keys);
    return message_of(controller.frame());
}

void verify_line_notices()
{
    Editing editing;
    open_vim_document(editing, std::string(seven_lines));
    auto &controller = editing.controller();
    expect(ran(controller, ":2,4d<CR>") == "3 fewer lines", "more than 'report' lines are counted");
    static_cast<void>(controller.apply_frame(app::HistoryAction{core::HistoryDirection::undo}));
    expect(whole_vim_body(controller) == seven_lines, "one u undoes the whole :d");
    expect(ran(controller, ":2,3d<CR>u").empty(), "two lines are not reported");
    expect(ran(controller, ":g/a/d<CR>") == "4 fewer lines", ":g reports the lines it deleted");
    static_cast<void>(controller.apply_frame(app::HistoryAction{core::HistoryDirection::undo}));
    expect(whole_vim_body(controller) == seven_lines, "one u undoes the whole :g (one Edit)");
    expect(ran(controller, ":2,4y<CR>") == "3 lines yanked" &&
               ran(controller, ":2,4y a<CR>") == "3 lines yanked into \"a" &&
               ran(controller, ":2,4y A<CR>") == "3 lines yanked into \"A" &&
               ran(controller, ":2,4y _<CR>").empty(),
           ":y reports the lines and the register it wrote");
    expect(ran(controller, ":g/zzz/d<CR>") == "Pattern not found: zzz" &&
               ran(controller, ":v/l/d<CR>") == "Pattern found in every line: l",
           ":g with no line to run on is a message, not an error");
    expect(ran(controller, ":d!<CR>") == "E477: No ! allowed" &&
               ran(controller, ":g<CR>") == "E148: Regular expression missing from :global" &&
               ran(controller, ":d 0<CR>") == "E939: Positive count required" &&
               ran(controller, ":d a b<CR>") == "E488: Trailing characters" &&
               ran(controller, ":9d<CR>") == "E16: Invalid range",
           "the failures keep Vim's words");
    expect(ran(controller, ":g/a/p<CR>") == "Not supported: g/a/p" &&
               whole_vim_body(controller) == seven_lines,
           "a :g command stage 3a does not run is refused before anything changes");
    expect(ran(controller, ":s/l/Q/<CR>u:g/b/s/b/~~/<CR>") == "Not supported" &&
               whole_vim_body(controller) == seven_lines,
           "a ~ that grows line by line inside :g is refused, nothing is replaced");
    expect(ran(controller, ":%d<CR>") == "--No lines in buffer--" &&
               whole_vim_body(controller).empty(),
           "deleting every line says the buffer is empty");
}

void verify_line_records()
{
    Editing editing;
    open_vim_document(editing, std::string(seven_lines));
    auto &controller = editing.controller();
    vim_normal(controller, "x:g/b/d<CR>:2y<CR>");
    expect(dot_record_is(controller.vim_state(), std::nullopt, "x"),
           ":d :y and :g are not the change . repeats");
    Editing crlf;
    open_vim_document(crlf, "a1\r\nb2\r\na3\r\nb4");
    vim_normal(crlf.controller(), ":g/b/d<CR>");
    static_cast<void>(
        crlf.controller().apply_frame(app::SaveDocument{sample_path(), core::TextEncoding::utf8}));
    expect(crlf.files().written() == "a1\r\na3", "the CRLF line breaks go with the lines");
    Editing clip;
    open_vim_document(clip, std::string(seven_lines));
    vim_normal(clip.controller(), ":2,3d +<CR>");
    expect(clip.clipboard().read().value_or(std::string{}) == "  l2 b\nl3 a\n" &&
               whole_vim_body(clip.controller()).starts_with("l1 a\n\tl4 b"),
           ":d + writes the deleted lines to the clipboard once (the scripted one)");
}

[[nodiscard]] core::VimStep lines_step(const core::TextBuffer &text, core::ExLinesRequest request)
{
    const core::Selection selection = core::collapsed_at(core::Offset{0});
    const core::VimEditorView view{text, selection, std::nullopt};
    return core::vim_lines_edited(empty_vim_state(), view, request);
}

void verify_change_marks()
{
    const auto text = core::TextBuffer::from_utf8(std::string(seven_lines)).value();
    const auto removed = lines_step(
        text, core::ExLinesRequest{std::nullopt, ExLineVerb::delete_lines, std::nullopt, 2});
    expect(removed.change.has_value() && removed.change.value().kind == core::VimChange::changed,
           ":d marks a change (ADR 0181)");
    const auto yanked = lines_step(
        text, core::ExLinesRequest{std::nullopt, ExLineVerb::yank_lines, std::nullopt, 2});
    expect(!yanked.change.has_value() && yanked.next.unnamed_register.value().text == "l1 a\n  l2 b\n",
           ":y writes the lines and marks no change");
    const auto empty = core::TextBuffer::empty();
    const auto nothing = lines_step(
        empty, core::ExLinesRequest{std::nullopt, ExLineVerb::delete_lines, std::nullopt,
                                    std::nullopt});
    expect(!nothing.change.has_value() && nothing.next.unnamed_register.value().text.empty(),
           ":d on an empty buffer deletes nothing and writes no register (Vim's ML_EMPTY)");
}
} // namespace

void verify_ex_lines_scope()
{
    verify_lines_command_text();
    verify_global_command_text();
    verify_global_command_failures();
    verify_global_through_evaluate();
    verify_line_notices();
    verify_line_records();
    verify_change_marks();
}
} // namespace nenenib::tests
