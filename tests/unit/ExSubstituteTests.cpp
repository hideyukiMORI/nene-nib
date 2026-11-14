// scope `--ex-substitute`（ADR 0168 の段 1・Issue #1007）。`:s` の族の字面（evaluate_ex と
// substitute_command）・範囲の字面（parsed_ex_range）・照合器の群（VimPattern::captured）・置き換えの
// 本文（SubstituteReplacement）と、oracle の fixture にできない形（報せの文言・`c` の拒否・NUL と
// BS の置き換え・CRLF の文書・`.` に積まないこと・undo 1 単位）を controller の道で確かめる。
// 本文とキャレットの答えの正本は fixture（`substitute-`・Vim 9.1 の実測）。
#include "DisplayText.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "ExRange.hpp"
#include "ExResult.hpp"
#include "ExSubstituteCommand.hpp"
#include "ExSubstituteRequest.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "SaveDocument.hpp"
#include "Scopes.hpp"
#include "Selection.hpp"
#include "SubstituteAll.hpp"
#include "SubstituteFailure.hpp"
#include "SubstituteReplacement.hpp"
#include "SubstituteRequest.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
#include "VimEditorView.hpp"
#include "VimNoEffect.hpp"
#include "VimPattern.hpp"
#include "VimPatternCaptures.hpp"
#include "VimPatternCase.hpp"
#include "VimPatternFailure.hpp"
#include "VimRegisterKind.hpp"
#include "VimRepeatFailure.hpp"
#include "VimReplaceBlock.hpp"
#include "VimSearchDirection.hpp"
#include "VimSearchNotice.hpp"
#include "VimSearchNoticeKind.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
using nenenib::core::Appearance;
using nenenib::core::ExFailure;
using nenenib::core::ExSubstituteSource;
using nenenib::core::VimPattern;
using nenenib::core::VimPatternCase;
using nenenib::core::VimPatternFailure;
using nenenib::core::VimSearchDirection;

constexpr std::string_view six_lines = "a1 a2\n  b a\nc a a\na\n\tx a y\nlast a";

[[nodiscard]] bool fails_with(std::string_view text, ExFailure failure)
{
    const auto parsed = core::substitute_command(text);
    return parsed.has_value() && !parsed.value().has_value() && parsed.value().error() == failure;
}

// 読めた `:s` の字面（:s でない・失敗なら値なし）。
[[nodiscard]] std::optional<core::ExSubstituteRequest> request_of(std::string_view text)
{
    const auto parsed = core::substitute_command(text);
    if (!parsed.has_value() || !parsed.value().has_value())
    {
        return std::nullopt;
    }
    return parsed.value().value();
}

// 読めなかったときの比べる相手（どの期待にも当たらない値）。
[[nodiscard]] core::ExSubstituteRequest unread()
{
    return core::ExSubstituteRequest{
        std::nullopt, ExSubstituteSource::typed, '?', "?", "?", false, "?", std::nullopt};
}

void verify_substitute_command_text()
{
    const auto request = request_of("s#a/b#X\\#Y#gi 3").value_or(unread());
    expect(request.source == ExSubstituteSource::typed && request.delimiter == '#' &&
               request.pattern == "a/b" && request.replacement == "X\\#Y" &&
               request.flags == "gi" && !request.keep_flags &&
               request.count == std::optional<std::size_t>{3},
           "the pattern, replacement, flags and count are read as typed");
    expect(request_of("s/[/]x/-/").value_or(unread()).pattern == "[/]x",
           "a delimiter inside [...] does not end the pattern (skip_regexp)");
    const auto open = request_of("s/a/X").value_or(unread());
    expect(open.replacement == "X" && open.flags.empty(), "the trailing delimiter may be left out");
    for (const std::string_view text : {"s", "s g", "sg", "&", "&&", "~", "& 3", "substitute"})
    {
        const auto parsed = request_of(text);
        expect(parsed.has_value() && parsed.value_or(unread()).source != ExSubstituteSource::typed,
               "the repeating forms use the previous substitute");
    }
    const auto kept = request_of("&&").value_or(unread());
    expect(kept.keep_flags && kept.source == ExSubstituteSource::repeated,
           ":&& keeps the previous flags");
    expect(request_of("~").value_or(unread()).source == ExSubstituteSource::tilde,
           ":~ uses the last pattern");
    for (const std::string_view text : {"set", "syntax on", "sil", "sign", "sp", "sort", "sx/a/b/"})
    {
        expect(!core::substitute_command(text).has_value(), "other commands are not :s");
    }
    expect(fails_with("s xaxbx", ExFailure::letter_delimiter), "a letter delimiter is E146");
    for (const std::string_view text :
         {"s/a/b/ 2147483647", "s/a/b/ 2147483648", "s/a/b/g4294967297",
          "s/a/b/ 99999999999999999999999", "&& 2147483647"})
    {
        expect(fails_with(text, ExFailure::value_too_large),
               "a count of INT_MAX or more is E1510 (Vim 9.1, measured)");
    }
    expect(request_of("s/a/b/ 2147483646").value_or(unread()).count ==
                   std::optional<std::size_t>{2147483646} &&
               request_of("s/a/b/ 0").value_or(unread()).count == std::optional<std::size_t>{0},
           "2147483646 and 0 are read; the engine decides 0 by the e flag");
    expect(fails_with("s/a/b/q", ExFailure::trailing_characters), "trailing text is E488");
    for (const std::string_view text :
         {"s/a/b/c", "s/a/b/gc", "&&c", "s/a/b/p", "s/a/b/#", "s/a/b/l", "s/a/\\=1/", "s\\/b/",
          "s\xC3\xA9"
          "a\xC3\xA9"})
    {
        expect(fails_with(text, ExFailure::unsupported_argument),
               "forms stage 1 does not take are closed failures (c is stage 2)");
    }
    expect(request_of("s/a/b/ \"note").has_value(), "a comment after the flags is allowed");
    expect(core::ex_failure_message(ExFailure::letter_delimiter).text() ==
                   "E146: Regular expressions can't be delimited by letters" &&
               core::ex_failure_message(ExFailure::value_too_large).text() ==
                   "E1510: Value too large" &&
               core::ex_failure_message(ExFailure::trailing_characters).text() ==
                   "E488: Trailing characters",
           "the parse failures keep Vim's numbers and words");
}

void verify_substitute_ranges()
{
    const auto settings = core::default_editor_settings();
    for (const std::string_view text : {"%s/a/b/g", "'<,'>s/a/b/", "2,4s/a/b/", ".,$s/a/b/",
                                        "'a,'bs/a/b/", ".,.+2s", "2;+1&&", "$~", "s/a/b/"})
    {
        const auto result = core::evaluate_ex(text, settings, Appearance::dark);
        expect(result.has_value() && result.value().substitute.has_value(),
               "every range form reaches the substitute request");
    }
    const auto substitute_of = [&settings](std::string_view text)
    {
        const auto result = core::evaluate_ex(text, settings, Appearance::dark);
        return result.has_value() ? result.value().substitute : std::nullopt;
    };
    const auto all = substitute_of("%s/a/b/").value_or(unread());
    expect(all.range.value_or(core::ExRange{}).addresses.size() == 2,
           "% is the two addresses 1 and $");
    expect(substitute_of("s/a/b/").has_value() &&
               !substitute_of("s/a/b/").value_or(all).range.has_value(),
           "no range is the caret's line");
    const auto [range, length] = core::parsed_ex_range("'<,'>s");
    expect(length == 5 && range.addresses.size() == 2, "the VISUAL range is 5 bytes long");
    const auto [relative, relative_length] = core::parsed_ex_range(".,.+12s");
    expect(relative_length == 6 && relative.addresses.back().offset == 12,
           "the count range keeps its offset");
    const auto [semicolon, semicolon_length] = core::parsed_ex_range("2;+1s");
    expect(semicolon_length == 4 && semicolon.addresses.back().after_semicolon &&
               semicolon.addresses.back().offset == 1,
           "the address after ; counts from the address before it");
    expect(core::parsed_ex_range("s/a/b/").second == 0, "no range is length 0");
    expect(core::parsed_ex_range(",5s").first.addresses.size() == 2,
           "an empty address next to a comma is the caret's line");
}

[[nodiscard]] std::optional<std::string> group_text(std::string_view pattern, std::string_view line,
                                                    std::size_t group)
{
    const auto parsed =
        VimPattern::parse(pattern, VimSearchDirection::forward, VimPatternCase::sensitive);
    if (!parsed.has_value())
    {
        return std::nullopt;
    }
    const auto match = parsed.value().matched(line, 0);
    if (!match.has_value())
    {
        return std::nullopt;
    }
    const auto captures = parsed.value().captured(line, match.value());
    if (!captures.has_value())
    {
        return std::nullopt;
    }
    const auto &range = captures.value().at(group);
    if (!range.has_value())
    {
        return std::nullopt;
    }
    return std::string(line.substr(range.value().begin, range.value().end - range.value().begin));
}

[[nodiscard]] std::optional<VimPatternFailure> parse_failure(std::string_view pattern)
{
    const auto parsed =
        VimPattern::parse(pattern, VimSearchDirection::forward, VimPatternCase::sensitive);
    return parsed.has_value() ? std::nullopt : std::optional{parsed.error()};
}

// 群つきの with と群を外した without が line の同じ所に当たるか。
[[nodiscard]] bool same_match(std::string_view with, std::string_view without,
                              std::string_view line)
{
    const auto grouped =
        VimPattern::parse(with, VimSearchDirection::forward, VimPatternCase::sensitive);
    const auto plain =
        VimPattern::parse(without, VimSearchDirection::forward, VimPatternCase::sensitive);
    if (!grouped.has_value() || !plain.has_value())
    {
        return false;
    }
    const auto left = grouped.value().matched(line, 0);
    const auto right = plain.value().matched(line, 0);
    if (!left.has_value() || !right.has_value())
    {
        return left.has_value() == right.has_value();
    }
    return left.value().begin == right.value().begin && left.value().end == right.value().end;
}

void verify_pattern_groups()
{
    expect(group_text("\\(a*\\)\\(a\\)b", "aaab", 1) == std::optional<std::string>{"aa"} &&
               group_text("\\(a*\\)\\(a\\)b", "aaab", 2) == std::optional<std::string>{"a"},
           "a starred group gives back what the next atom needs (Vim: [aa|a])");
    expect(group_text("\\(a\\(b\\)c\\)d", "abcd", 1) == std::optional<std::string>{"abc"} &&
               group_text("\\(a\\(b\\)c\\)d", "abcd", 2) == std::optional<std::string>{"b"},
           "nested groups are numbered by their opening");
    expect(group_text("x\\(y*\\)z", "xz", 1) == std::optional<std::string>{""},
           "an empty group is an empty range");
    expect(group_text("\\(^a\\)", "abc", 1) == std::optional<std::string>{"a"},
           "^ right after \\( is the line start");
    expect(group_text("\\(*b\\)", "a*b", 1) == std::optional<std::string>{"*b"},
           "* right after \\( is a character");
    expect(group_text("\\(b$\\)", "ab", 1) == std::optional<std::string>{"b"},
           "$ right before \\) is the line end");
    expect(group_text("a", "a", 0) == std::optional<std::string>{"a"}, "group 0 is the match");
    expect(!group_text("a", "a", 1).has_value(), "a group the pattern has not is empty");
    expect(parse_failure("\\(a") == std::optional{VimPatternFailure::group} &&
               parse_failure("a\\)") == std::optional{VimPatternFailure::unmatched_close} &&
               !parse_failure("\\(ab\\)*").has_value() &&
               parse_failure(
                   "\\(a\\)\\(a\\)\\(a\\)\\(a\\)\\(a\\)\\(a\\)\\(a\\)\\(a\\)\\(a\\)\\(a\\)") ==
                   std::optional{VimPatternFailure::too_many_groups},
           "an open group, a stray \\) and a tenth group are refused, a starred group is taken "
           "(ADR 0184 decision 3)");
    for (const auto &[with, without] :
         {std::pair<std::string_view, std::string_view>{"\\(ab\\)c", "abc"},
          {"\\(a*\\)b", "a*b"},
          {"x\\(\\<y\\)", "x\\<y"},
          {"\\([a-c]\\).\\(d\\)", "[a-c].d"}})
    {
        for (const std::string_view line : {"zzabcab", "aaab b", "x yxy", "zbad cxd"})
        {
            expect(same_match(with, without, line),
                   "groups never change where a pattern matches (ADR 0168 decision 4)");
        }
    }
}

[[nodiscard]] std::string expanded(std::string_view replacement, std::string_view line,
                                   std::size_t begin, std::size_t end)
{
    core::VimPatternCaptures captures{};
    captures.at(0) = core::VimPatternMatch{begin, end, begin, end};
    std::string out;
    core::SubstituteReplacement::vim(replacement).appended(out, line, captures);
    expect(core::SubstituteReplacement::vim(replacement).expanded_bytes(line, captures) ==
               out.size(),
           "the counted bytes are the bytes appended");
    return out;
}

void verify_replacement_text()
{
    expect(expanded("[&]", "foo", 0, 3) == "[foo]" && expanded("[\\0]", "foo", 0, 3) == "[foo]",
           "& and \\0 are the match");
    expect(expanded("\\&\\\\\\/\\~", "foo", 0, 3) == "&\\/~", "escaped characters are themselves");
    expect(expanded("a\\rb", "x", 0, 1) == "a\nb" && expanded("a\rb", "x", 0, 1) == "a\nb",
           "\\r and a typed CR are line breaks");
    expect(expanded("\\n\\t\\b", "x", 0, 1) == std::string("\0\t\b", 3),
           "\\n is NUL, \\t is Tab and \\b is BS (Vim's vim_regsub)");
    expect(expanded("a\\", "x", 0, 1) == "a\\", "a trailing backslash stays");
    expect(expanded("\\u&", "foo", 0, 3) == "Foo" && expanded("\\U&x", "foo", 0, 3) == "FOOX" &&
               expanded("\\U&\\Ex", "foo", 0, 3) == "FOOx" &&
               expanded("\\L\\u&", "FOO", 0, 3) == "Foo" &&
               expanded("\\u\\L&", "FOO", 0, 3) == "Foo" && expanded("\\l&", "FOO", 0, 3) == "fOO",
           "the case escapes follow Vim's func_one and func_all");
    expect(expanded("\\u\\rx", "a", 0, 1) == "\nx", "\\u is spent on the line break");
    expect(expanded("\\U&", "\xC3\xA9 \xC3\x9F", 0, 5) == "\xC3\x89 \xC3\x9F",
           "non-ASCII letters change case, sharp s stays (Vim's utf_toupper)");
    expect(expanded("\\u&", "\xC7\x86", 0, 2) == "\xC7\x84", "dz goes to DZ, not Dz");
    expect(core::vim_tilde_expanded("~[~]\\~", "X") == "X[X]\\~",
           "~ is the previous replacement and \\~ stays for the expansion");
    expect(expanded(core::vim_tilde_expanded("\\~", "X"), "a", 0, 1) == "~", "\\~ becomes a tilde");
}

[[nodiscard]] std::string message_of(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string{};
}

[[nodiscard]] app::EditorFrame substituted(app::EditorController &controller, std::string_view keys)
{
    vim_normal(controller, keys);
    return controller.frame();
}

void verify_substitute_notices()
{
    Editing editing;
    open_vim_document(editing, std::string(six_lines));
    auto &controller = editing.controller();
    expect(message_of(substituted(controller, ":%s/a/X/g<CR>")) == "9 substitutions on 6 lines",
           "more than 'report' substitutions are counted");
    static_cast<void>(controller.apply_frame(app::HistoryAction{core::HistoryDirection::undo}));
    expect(whole_vim_body(controller) == six_lines, "one u undoes the whole :s (one Edit)");
    expect(message_of(substituted(controller, "gg:s/a/X/<CR>")).empty(),
           "one substitution is not reported");
    expect(message_of(substituted(controller, ":%s/a/X/gn<CR>")) == "8 matches on 6 lines",
           ":s///n reports the matches");
    expect(message_of(substituted(controller, ":2s/zzz/X/<CR>")) == "E486: Pattern not found: zzz",
           "no match is E486");
    expect(message_of(substituted(controller, ":9s/a/X/<CR>")) == "E16: Invalid range" &&
               message_of(substituted(controller, ":4,2s/a/X/<CR>")) ==
                   "E493: Backwards range given" &&
               message_of(substituted(controller, ":'zs/a/X/<CR>")) == "E20: Mark not set",
           "range failures keep Vim's words");
    expect(message_of(substituted(controller, ":s/a/X/c<CR>")) == "Not supported: s/a/X/c",
           "the confirm flag is refused in stage 1, nothing is replaced silently");
    expect(message_of(substituted(controller, ":s/\\(a/X/<CR>")) == "E54: Unmatched \\(",
           "a broken group is a closed failure with Vim's words");

    Editing fresh;
    open_vim_document(fresh, std::string(six_lines));
    auto &other = fresh.controller();
    expect(
        message_of(substituted(other, "&")) == "E33: No previous substitute regular expression" &&
            message_of(substituted(other, ":s//X/<CR>")) == "E35: No previous regular expression",
        "no previous substitute is E33 and no previous pattern is E35");
}

void verify_substitute_repeat_and_bytes()
{
    Editing editing;
    open_vim_document(editing, std::string(six_lines));
    auto &controller = editing.controller();
    vim_normal(controller, "x:s/a/X/<CR>j&");
    expect(whole_vim_body(controller).starts_with("1 X2\n  b X\n") &&
               dot_record_is(controller.vim_state(), std::nullopt, "x"),
           "& is not the change . repeats (Vim's nv_optrans)");
    Editing nul;
    open_vim_document(nul, "ab");
    vim_normal(nul.controller(), ":s/a/\\n\\b/<CR>");
    static_cast<void>(
        nul.controller().apply_frame(app::SaveDocument{sample_path(), core::TextEncoding::utf8}));
    expect(nul.files().written() == std::string("\0\bb", 3),
           "\\n writes NUL and \\b writes BS (Vim 9.1, measured)");
    Editing crlf;
    open_vim_document(crlf, "one two\r\nthree four\r\nfive");
    vim_normal(crlf.controller(), ":%s/ /\\r/g<CR>x");
    static_cast<void>(
        crlf.controller().apply_frame(app::SaveDocument{sample_path(), core::TextEncoding::utf8}));
    expect(crlf.files().written() == "one\r\ntwo\r\nthree\r\nour\r\nfive",
           "line breaks from \\r take the document's CRLF and the caret lands on the last one");
}
} // namespace

namespace
{
// engine の 1 歩（今の行・打ったパターン・request の回数とフラグ）。capture_limit
// は群の道の升の数の 上限。
[[nodiscard]] core::VimStep stepped_with(const core::TextBuffer &text,
                                         core::ExSubstituteRequest request,
                                         std::size_t capture_limit)
{
    const core::Selection selection = core::collapsed_at(core::Offset{0});
    core::VimEditorView view{text, selection, std::nullopt};
    view.capture_limit = capture_limit;
    return core::vim_substituted(empty_vim_state(), view, request);
}

[[nodiscard]] core::ExSubstituteRequest typed_request(std::string pattern, std::string replacement)
{
    return core::ExSubstituteRequest{std::nullopt,
                                     ExSubstituteSource::typed,
                                     '/',
                                     std::move(pattern),
                                     std::move(replacement),
                                     false,
                                     "",
                                     std::nullopt};
}

[[nodiscard]] core::VimStep substitute_step(const core::TextBuffer &text, std::string pattern,
                                            std::string replacement, std::size_t capture_limit)
{
    return stepped_with(text, typed_request(std::move(pattern), std::move(replacement)),
                        capture_limit);
}

// 回数 0 の 1 歩（flags は `e` か空）。
[[nodiscard]] core::VimStep zero_counted(const core::TextBuffer &text, std::string flags)
{
    core::ExSubstituteRequest request = typed_request("x", "X");
    request.flags = std::move(flags);
    request.count = 0;
    return stepped_with(text, std::move(request), core::vim_capture_cell_limit);
}

[[nodiscard]] std::string replaced_body(const core::VimStep &step)
{
    const auto *block = std::get_if<core::VimReplaceBlock>(&step.effect);
    return block != nullptr && block->edits.size() == 1 ? block->edits.front().utf8 : "?";
}

[[nodiscard]] bool refused_untouched(const core::VimStep &step)
{
    return std::holds_alternative<core::VimNoEffect>(step.effect) &&
           step.failure == std::optional{core::VimRepeatFailure::refused} &&
           step.notice.has_value() &&
           step.notice.value().kind == core::VimSearchNoticeKind::pattern_too_complex &&
           !step.marks.has_value() && !step.change.has_value() && !step.restore.has_value() &&
           step.next.unnamed_register.value().kind == core::VimRegisterKind::uninitialized;
}

// 群の範囲を引けない当たり（升の数の上限・ADR 0168 の追補 13）は置換全体を断り、本文・レジスタ・
// マーク・変更の印を変えない。上限の内では群は正しい。歩きは升の数に比例する（N = 20000 でも
// 群つきのパターンが 1 歩で返る）。
void verify_capture_limits()
{
    const std::string fields = "x" + std::string(200, 'y') + "," + std::string(200, 'z');
    const core::TextBuffer text = buffer_of(fields);
    const std::string swapped = std::string(200, 'z') + "," + "x" + std::string(200, 'y');
    expect(replaced_body(substitute_step(text, "\\(.*\\),\\(.*\\)", "\\2,\\1",
                                         core::vim_capture_cell_limit)) == swapped,
           "within the limit the groups swap the fields");
    const auto refused = substitute_step(text, "\\(.*\\),\\(.*\\)", "\\2,\\1", 100);
    expect(refused_untouched(refused),
           "over the limit the whole :s is refused with E363 and nothing changes");
    expect(core::vim_search_message(refused.notice.value_or(core::VimSearchNotice{})).text() ==
               "E363: pattern uses more memory than 'maxmempattern'",
           "the refusal says so in one line");
    const std::string huge =
        std::string(std::size_t{1} << 20, 'a') + "," + std::string(std::size_t{1} << 20, 'b');
    expect(refused_untouched(substitute_step(buffer_of(huge), "\\(.*\\),\\(.*\\)", "\\2,\\1",
                                             core::vim_capture_cell_limit)),
           "a 2 MiB field swap is over the product limit: refused, never emptied");
    expect(
        replaced_body(substitute_step(buffer_of(huge), "a*,b", "[&]", core::vim_capture_cell_limit))
            .starts_with("[a"),
        "without \\1 the groups are not walked and the limit does not apply");
    const std::string walked = "y" + std::string(20000, 'z') + "x";
    expect(replaced_body(substitute_step(buffer_of(walked), "\\(.*\\).*y.*x", "[\\1]",
                                         core::vim_capture_cell_limit)) == "[]",
           "the walk is linear in the cells (20000 characters, one step)");
    const auto loud = zero_counted(text, "");
    expect(loud.notice.value_or(core::VimSearchNotice{}).kind ==
                   core::VimSearchNoticeKind::positive_count &&
               loud.failure == std::optional{core::VimRepeatFailure::refused},
           "a zero count without e is E939");
    const auto quiet = zero_counted(text, "e");
    expect(!quiet.failure.has_value() && !quiet.notice.has_value() &&
               std::holds_alternative<core::VimNoEffect>(quiet.effect),
           "a zero count with e does nothing and goes on (Vim 9.1, measured)");
}

// 上限の判定は置き換えが入れる改行を文書の改行の形の長さで数える（CRLF は 2 バイト）。
void verify_limit_counts_document_newlines()
{
    const core::TextBuffer crlf = buffer_of("a\r\nb");
    const auto pattern =
        VimPattern::parse("a", VimSearchDirection::forward, VimPatternCase::sensitive);
    if (!pattern.has_value())
    {
        expect(false, "the pattern parses");
        return;
    }
    const auto replaced = [&](std::size_t limit)
    {
        return core::substituted(
            crlf, pattern.value(), core::SubstituteReplacement::vim("x\\ry"),
            core::SubstituteRequest{core::LineNumber{1}, core::LineNumber{1}, true, false, limit});
    };
    expect(
        replaced(7).has_value() && replaced(6).error_or(core::SubstituteFailure::no_match) ==
                                       core::SubstituteFailure::too_large,
        "x CRLF y CRLF b is 7 bytes: 7 fits and 6 is too large (LF counting would let 6 through)");
}

} // namespace

void verify_ex_substitute_scope()
{
    verify_capture_limits();
    verify_limit_counts_document_newlines();
    verify_substitute_command_text();
    verify_substitute_ranges();
    verify_pattern_groups();
    verify_replacement_text();
    verify_substitute_notices();
    verify_substitute_repeat_and_bytes();
}
} // namespace nenenib::tests
