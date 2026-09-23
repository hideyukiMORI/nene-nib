// scope `--vim-search-highlight` の単体テスト（ADR 0042 決定 2）。
#include "Appearance.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorSettings.hpp"
#include "ExResult.hpp"
#include "OpenDocument.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "SelectEditMode.hpp"
#include "TestSupport.hpp"
#include "VimPattern.hpp"
#include "VimSearch.hpp"
#include "VimSearchDirection.hpp"
#include "VimSearchHighlight.hpp"
#include "VimState.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::application::OpenDocument;
using nenenib::application::SelectEditMode;
using nenenib::application::VisibleLines;
using nenenib::core::EditMode;
using nenenib::core::VimPattern;
using nenenib::core::VimSearchDirection;
using nenenib::core::VimSearchHighlight;
using nenenib::core::VimState;

namespace core = nenenib::core;
namespace app = nenenib::application;

[[nodiscard]] std::vector<MatchSpan> matches_of(std::string_view line, std::string_view pattern)
{
    const auto parsed = VimPattern::parse(pattern, VimSearchDirection::forward);
    std::vector<MatchSpan> spans;
    for (const auto &match : nenenib::core::vim_line_matches(line, parsed.value()))
    {
        spans.emplace_back(match.begin.value, match.end.value);
    }
    return spans;
}

// 行の中の一致の列（決定 3）。数え方は vim_search と同じ 1 本で、総数は固定 Vim 9.1 の
// searchcount() で測った（out/issue123-oracle/probe2.json）。
void verify_vim_line_matches()
{
    expect(matches_of("aaaa", "aa") == std::vector<MatchSpan>{{0, 2}, {2, 4}},
           "the scan restarts at the end of the match, so four a characters hold two");
    expect(matches_of("aaaa", "a").size() == 4, "every character can be its own match");
    expect(matches_of("abab", "ab") == std::vector<MatchSpan>{{0, 2}, {2, 4}},
           "two matches that do not touch");
    expect(matches_of("ababa", "aba") == std::vector<MatchSpan>{{0, 3}},
           "the overlap of a three character match is skipped, so there is only one");
    expect(matches_of("xa xa", "xa") == std::vector<MatchSpan>{{0, 2}, {3, 5}},
           "two matches with a gap between them");
    expect(matches_of("abc", "a*") == std::vector<MatchSpan>{{0, 1}, {1, 1}, {2, 2}},
           "a zero-length match counts and steps one character");
    expect(matches_of("abc", "^") == std::vector<MatchSpan>{{0, 0}}, "the line start matches once");
    expect(matches_of("abc", "$") == std::vector<MatchSpan>{{3, 3}}, "and so does the line end");
    expect(matches_of("abc", "zzz").empty(), "a pattern that is not there has no match");
    expect(matches_of("あいう かきく", "あいう") == std::vector<MatchSpan>{{0, 9}},
           "a multibyte match is measured in bytes");
    expect(matches_of("Foo foo", "foo") == std::vector<MatchSpan>{{4, 7}},
           "the scan is case sensitive like the search (noignorecase)");
}

[[nodiscard]] bool highlight_is(const EditorController &controller, VimSearchHighlight expected)
{
    return controller.vim_state().highlight == expected;
}

// 3 値の遷移（決定 1）。期待値は固定 Vim 9.1 の `v:hlsearch` を 29 ケース測った
// out/issue123-oracle/probe.json と probe3.json。強調そのものは Vim の報告に出ないので
// fixture にできず、ここが正本の検査である（決定 8）。
void verify_vim_search_highlight_state()
{
    Editing session;
    open_vim_document(session, "alpha beta\nbeta gamma\ndelta beta");
    EditorController &controller = session.controller();
    expect(highlight_is(controller, VimSearchHighlight::on),
           "the highlight is on as soon as Vim mode starts (D17)");
    vim_replay(controller, "/beta<CR>");
    expect(highlight_is(controller, VimSearchHighlight::on), "a search leaves it on");
    static_cast<void>(run_ex(controller, "nohlsearch"));
    expect(highlight_is(controller, VimSearchHighlight::suspended), ":nohlsearch suspends it");
    vim_replay(controller, "j");
    expect(highlight_is(controller, VimSearchHighlight::suspended),
           "a motion does not bring it back");
    vim_replay(controller, "x");
    expect(highlight_is(controller, VimSearchHighlight::suspended), "neither does an edit");
    vim_replay(controller, "n");
    expect(highlight_is(controller, VimSearchHighlight::on), "n brings it back");
    static_cast<void>(run_ex(controller, "noh"));
    expect(highlight_is(controller, VimSearchHighlight::suspended),
           ":noh is the same command in short");
    vim_replay(controller, "N");
    expect(highlight_is(controller, VimSearchHighlight::on), "N brings it back");
    static_cast<void>(run_ex(controller, "noh"));
    vim_replay(controller, "*");
    expect(highlight_is(controller, VimSearchHighlight::on), "* brings it back");
    static_cast<void>(run_ex(controller, "noh"));
    vim_replay(controller, "?beta<CR>");
    expect(highlight_is(controller, VimSearchHighlight::on), "a backward search brings it back");
    static_cast<void>(run_ex(controller, "noh"));
    vim_replay(controller, "/zzz<CR>");
    expect(highlight_is(controller, VimSearchHighlight::on),
           "a search that finds nothing brings it back too (E486)");
    static_cast<void>(run_ex(controller, "set nohlsearch"));
    expect(highlight_is(controller, VimSearchHighlight::off), ":set nohlsearch turns it off");
    vim_replay(controller, "/beta<CR>");
    expect(highlight_is(controller, VimSearchHighlight::off),
           "a search does not turn an option that is off back on");
    static_cast<void>(run_ex(controller, "noh"));
    expect(highlight_is(controller, VimSearchHighlight::off),
           ":noh has nothing to suspend while the option is off");
    static_cast<void>(run_ex(controller, "set hlsearch"));
    expect(highlight_is(controller, VimSearchHighlight::on), ":set hlsearch turns it on");
    static_cast<void>(run_ex(controller, "noh"));
    static_cast<void>(run_ex(controller, "set hlsearch"));
    expect(highlight_is(controller, VimSearchHighlight::on),
           ":set hlsearch also clears a suspension");
    Editing empty;
    open_vim_document(empty, "alpha beta\nbeta gamma");
    EditorController &second = empty.controller();
    static_cast<void>(run_ex(second, "noh"));
    vim_replay(second, "n");
    expect(highlight_is(second, VimSearchHighlight::on),
           "a search key with no previous pattern brings it back too (E35)");
}

// 保留を捨てる resting の形と、モードの出入り（決定 1）。
void verify_vim_search_highlight_carry()
{
    const VimState rested = empty_vim_state();
    expect(rested.highlight == VimSearchHighlight::on, "the resting state starts on");
    VimState suspended = rested;
    suspended.highlight = VimSearchHighlight::suspended;
    expect(nenenib::core::vim_resting_from(suspended, suspended.unnamed_register).highlight ==
               VimSearchHighlight::suspended,
           "finishing a key does not forget the suspension");
    expect(nenenib::core::vim_after_resize(suspended, 10, 20).highlight ==
               VimSearchHighlight::suspended,
           "a resize does not forget it either");
    expect(nenenib::core::requested_highlight(
               VimSearchHighlight::off, VimSearchHighlight::suspended) == VimSearchHighlight::off,
           ":noh leaves an option that is off alone");
    expect(nenenib::core::requested_highlight(VimSearchHighlight::suspended,
                                              VimSearchHighlight::on) == VimSearchHighlight::on,
           ":set hlsearch replaces whatever was there");
    Editing session;
    open_vim_document(session, "alpha beta\nbeta gamma");
    EditorController &controller = session.controller();
    vim_replay(controller, "/beta<CR>");
    static_cast<void>(run_ex(controller, "noh"));
    static_cast<void>(controller.apply(app::SelectEditMode{EditMode::ordinary}));
    static_cast<void>(controller.apply(app::SelectEditMode{EditMode::vim}));
    expect(highlight_is(controller, VimSearchHighlight::suspended),
           "leaving and re-entering Vim mode keeps it, like the remembered pattern");
}

// Ex の 4 命令と補完（決定 2）。
void verify_ex_highlight_commands()
{
    const auto settings = core::default_editor_settings();
    const auto on = core::evaluate_ex("set hlsearch", settings, Appearance::dark).value();
    expect(on.highlight == std::optional<VimSearchHighlight>{VimSearchHighlight::on} &&
               !on.settings.has_value() && on.message.text() == "hlsearch=on",
           ":set hlsearch turns the highlight on without touching the saved settings");
    const auto off = core::evaluate_ex("set nohlsearch", settings, Appearance::dark).value();
    expect(off.highlight == std::optional<VimSearchHighlight>{VimSearchHighlight::off} &&
               off.message.text() == "hlsearch=off",
           ":set nohlsearch turns it off");
    for (const auto text : {"nohlsearch", "noh", " noh "})
    {
        const auto cleared = core::evaluate_ex(text, settings, Appearance::dark).value();
        expect(cleared.highlight ==
                       std::optional<VimSearchHighlight>{VimSearchHighlight::suspended} &&
                   !cleared.settings.has_value() && cleared.message.text() == "hlsearch=suspended",
               ":nohlsearch and its short spelling only suspend");
    }
    const auto theme = core::evaluate_ex("colorscheme dracula", settings, Appearance::dark).value();
    expect(!theme.highlight.has_value(),
           "commands that are not about the highlight leave it alone");
    for (const auto text : {"nohlsearch now", "noh 1", "set hlsearch=1", "set hls", "hlsearch"})
    {
        expect(!core::evaluate_ex(text, settings, Appearance::dark),
               "only the four measured spellings are accepted");
    }
    const auto completions = core::command_completions("set ");
    expect(std::ranges::find(completions, "set hlsearch") != completions.end() &&
               std::ranges::find(completions, "set nohlsearch") != completions.end(),
           "both option spellings are offered as completions");
    expect(core::command_completions("set noh").size() == 1 &&
               core::command_completions("set noh").front() == "set nohlsearch",
           "a prefix narrows to the one that matches");
    expect(choices_for("hls").front().command == "set hlsearch",
           "the palette reaches the option through the shared catalog");
}

// 見えている行ごとの当たりと現在の当たり（決定 3・4）。桁は選択と同じ span_of で作る。
void verify_search_highlight_frame()
{
    Editing session;
    open_vim_document(session, "alpha beta\nbeta gamma\ndelta beta");
    EditorController &controller = session.controller();
    expect(frame_matches(controller.frame(), 0).empty(),
           "nothing is highlighted before the first search");
    vim_replay(controller, "/beta<CR>");
    const auto found = controller.frame();
    expect(frame_matches(found, 0) == std::vector<MatchSpan>{{7, 11}} &&
               frame_matches(found, 1) == std::vector<MatchSpan>{{1, 5}} &&
               frame_matches(found, 2) == std::vector<MatchSpan>{{7, 11}},
           "every visible match is a span in display columns");
    expect(caret_at(found, 1, 7), "the caret sits on the head of the first match below it");
    expect(current_match_is(found, 0, MatchSpan{7, 11}) &&
               current_match_is(found, 1, std::nullopt) && current_match_is(found, 2, std::nullopt),
           "the current match is the one that holds the caret");
    vim_replay(controller, "l");
    expect(current_match_is(controller.frame(), 0, MatchSpan{7, 11}),
           "a caret inside the match is still the current one");
    vim_replay(controller, "n");
    expect(current_match_is(controller.frame(), 1, MatchSpan{1, 5}) &&
               current_match_is(controller.frame(), 0, std::nullopt),
           "n moves the current match to the next line");
    vim_replay(controller, "$");
    expect(current_match_is(controller.frame(), 1, std::nullopt) &&
               frame_matches(controller.frame(), 1) == std::vector<MatchSpan>{{1, 5}},
           "a caret outside every match leaves the spans but no current one");
    vim_replay(controller, "0x");
    expect(frame_matches(controller.frame(), 1).empty(),
           "an edit that breaks the word takes the match away with it");
}

// 強調しない場合（決定 3・6）と、見えていない行（決定 3）。
void verify_search_highlight_line_scope()
{
    Editing session;
    open_vim_document(session, "beta one\nbeta two\nbeta three\nbeta four");
    EditorController &controller = session.controller();
    vim_replay(controller, "/beta<CR>");
    static_cast<void>(controller.apply(VisibleLines{2}));
    const auto narrow = controller.frame();
    expect(narrow.lines.size() == 2, "only the visible lines are in the frame");
    expect(frame_matches(narrow, 0) == std::vector<MatchSpan>{{1, 5}} &&
               frame_matches(narrow, 1) == std::vector<MatchSpan>{{1, 5}},
           "and only those lines are scanned");
    static_cast<void>(controller.apply(VisibleLines{vim_visible_lines}));
    static_cast<void>(run_ex(controller, "nohlsearch"));
    expect(frame_matches(controller.frame(), 0).empty() &&
               !controller.frame().lines.at(0).current_match.has_value(),
           "a suspended highlight paints nothing");
    vim_replay(controller, "n");
    expect(!frame_matches(controller.frame(), 0).empty(), "the next search paints again");
    static_cast<void>(run_ex(controller, "set nohlsearch"));
    expect(frame_matches(controller.frame(), 0).empty(), "and so does an option that is off");
    static_cast<void>(run_ex(controller, "set hlsearch"));
    static_cast<void>(controller.apply(app::SelectEditMode{EditMode::ordinary}));
    expect(frame_matches(controller.frame(), 0).empty(),
           "the ordinary mode has no search, so it highlights nothing");
}

// 桁の作り方が選択と同じ 1 本であること（決定 4）。全角・Tab・CRLF で確かめる。
void verify_search_highlight_columns()
{
    Editing wide;
    open_vim_document(wide, "あいbetaう\tbeta");
    EditorController &controller = wide.controller();
    vim_replay(controller, "/beta<CR>");
    expect(frame_matches(controller.frame(), 0) == std::vector<MatchSpan>{{3, 7}, {9, 13}},
           "multibyte characters and a tab each count as one column, as they do for a selection");
    vim_replay(controller, "vll");
    const auto selected = controller.frame();
    expect(selected.lines.at(0).selection.begin == selected.lines.at(0).matches.at(0).begin,
           "a VISUAL selection that starts on the match starts at the same column");
    Editing crlf;
    crlf.files().hold(Bytes{std::string("alpha beta\r\nbeta gamma\r\n")});
    applied(crlf.controller(), VisibleLines{vim_visible_lines});
    applied(crlf.controller(), OpenDocument{sample_path()});
    applied(crlf.controller(), SelectEditMode{EditMode::vim});
    vim_replay(crlf.controller(), "/beta<CR>");
    const auto lines = crlf.controller().frame();
    expect(frame_matches(lines, 0) == std::vector<MatchSpan>{{7, 11}} &&
               frame_matches(lines, 1) == std::vector<MatchSpan>{{1, 5}},
           "a CRLF document counts the same columns (the carriage return is not content)");
    Editing anchored;
    open_vim_document(anchored, "alpha\nbeta");
    vim_replay(anchored.controller(), "/^<CR>");
    expect(frame_matches(anchored.controller().frame(), 0).empty() &&
               frame_matches(anchored.controller().frame(), 1).empty(),
           "a zero-length match has no face to paint");
}
} // namespace

void verify_vim_search_highlight_contracts()
{
    verify_vim_line_matches();
    verify_vim_search_highlight_state();
    verify_vim_search_highlight_carry();
    verify_ex_highlight_commands();
    verify_search_highlight_frame();
    verify_search_highlight_line_scope();
    verify_search_highlight_columns();
}

void verify_vim_search_highlight_scope()
{
    verify_vim_search_highlight_contracts();
}
} // namespace nenenib::tests
