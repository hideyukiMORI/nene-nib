// scope `--substitute-confirm`（ADR 0168 の決定 6・段 2a・Issue #1652）。`:s///c`
// の問いは対話なので oracle の fixture にできない。期待値は Vim 9.1 に `--not-a-term` で `:normal!`
// の同じ鍵の列に答えを 続けて流した実測（D:/NeNeNib/scripts/20261113/e3-1652・作業木の
// out/1652-impl/vim-confirm.md）。 答えは窓と同じ意図（CommandText・CancelCommand）と、fixture
// と同じ :normal! の鍵の口の両方で流す。
#include "AutosaveTick.hpp"
#include "CancelCommand.hpp"
#include "CommandText.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "ImeStance.hpp"
#include "InputLinePrompt.hpp"
#include "PlaceCaret.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "SelectionAnchoring.hpp"
#include "SubmitCommand.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "VimTestSupport.hpp"
#include "WorkCompleted.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;

constexpr std::string_view four_lines = "a a a\nb a\nc\n  a d";

[[nodiscard]] std::string message_of(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string{};
}

// 問いの文（問いが開いていなければ空）。
[[nodiscard]] std::string question_of(const app::EditorFrame &frame)
{
    if (!frame.command_line.has_value() ||
        frame.command_line.value().prompt != core::InputLinePrompt::substitute_confirm)
    {
        return {};
    }
    return frame.command_line.value().text;
}

// 窓の 1 打鍵と同じ意図で答える。
[[nodiscard]] app::EditorFrame answered(app::EditorController &controller, std::string_view keys)
{
    for (const char key : keys)
    {
        static_cast<void>(controller.apply_frame(app::CommandText{std::string(1, key)}));
    }
    return controller.frame();
}

// 答えの列ごとの期待（Vim 9.1 の実測）。
struct ConfirmCase
{
    std::string_view answers;
    std::string_view body;
    std::size_t line;
    std::size_t column;
    std::string_view message;
    std::size_t undo_column;
    std::size_t undo_line;
};

// 問いを開いた形の文書（`:` を打つ前の鍵でキャレットを置く）。
void opened(Editing &editing, std::string_view text, std::string_view keys)
{
    open_vim_document(editing, std::string(text));
    vim_normal(editing.controller(), keys);
}

[[nodiscard]] app::EditorFrame undone(app::EditorController &controller)
{
    vim_normal(controller, "u");
    return controller.frame();
}

// 答えの列ごとの本文・キャレット・報せ・undo 1 回の後の本文とキャレット（Vim 9.1 の実測）。
void verify_answers()
{
    constexpr std::array<ConfirmCase, 8> cases{{
        {"yyyyy", "X X X\nb X\nc\n  X d", 4, 1, "5 substitutions on 3 lines", 1, 1},
        {"na", "a X X\nb X\nc\n  X d", 4, 3, "4 substitutions on 3 lines", 1, 1},
        {"yyq", "X X a\nb a\nc\n  a d", 1, 5, "", 5, 1},
        {"ynl", "X a X\nb a\nc\n  a d", 1, 1, "", 1, 1},
        {"l", "X a a\nb a\nc\n  a d", 1, 1, "", 1, 1},
        {"nnnnl", "a a a\nb a\nc\n  X d", 4, 1, "", 1, 4},
        {"nnnny", "a a a\nb a\nc\n  X d", 4, 1, "", 1, 4},
        {"ynyq", "X a X\nb a\nc\n  a d", 2, 3, "", 1, 1},
    }};
    for (const ConfirmCase &entry : cases)
    {
        Editing editing;
        opened(editing, four_lines, "3G:%s/a/X/gc<CR>");
        auto &controller = editing.controller();
        const auto frame = answered(controller, entry.answers);
        const std::string label = "answers " + std::string(entry.answers);
        expect(whole_vim_body(controller) == entry.body,
               (label + ": the body is Vim's (y n a q l, measured)").c_str());
        expect(caret_at(frame, entry.line, entry.column), (label + ": the caret is Vim's").c_str());
        expect(message_of(frame) == entry.message && question_of(frame).empty(),
               (label + ": the question is closed, the count only past 'report'").c_str());
        const auto back = undone(controller);
        expect(whole_vim_body(controller) == four_lines,
               (label + ": one u undoes every replaced match (one undo unit)").c_str());
        expect(caret_at(back, entry.undo_line, entry.undo_column),
               (label + ": u lands where Vim's uh_cursor was (line finished, clamped)").c_str());
    }
}

void verify_quit_and_skip()
{
    Editing editing;
    opened(editing, four_lines, "3G:%s/a/X/gc<CR>");
    auto &controller = editing.controller();
    const auto before = controller.frame().document.save_state;
    const auto all_no = answered(controller, "nnnnn");
    expect(whole_vim_body(controller) == four_lines && caret_at(all_no, 4, 3) &&
               message_of(all_no).empty() && question_of(all_no).empty(),
           "all n: the body stays, the caret is the last asked match, no message and no E486");
    expect(all_no.document.save_state == before, "all n leaves no undo unit (unmodified)");
    vim_normal(controller, "``");
    expect(caret_at(controller.frame(), 3, 1), "'' is the caret before :s even when all are n");

    Editing escaped;
    opened(escaped, four_lines, "3G:%s/a/X/gc<CR>");
    auto &other = escaped.controller();
    static_cast<void>(answered(other, "yy"));
    const auto frame = other.apply_frame(app::CancelCommand{});
    expect(whole_vim_body(other) == "X X a\nb a\nc\n  a d" && caret_at(frame, 1, 5) &&
               question_of(frame).empty(),
           "Esc is q: the asked match stays, the caret is on it");

    Editing samelined;
    opened(samelined, four_lines, "3G:%s/a/XYZ/gc<CR>");
    auto &same = samelined.controller();
    const auto asked = answered(same, "ynq");
    expect(whole_vim_body(same) == "XYZ a a\nb a\nc\n  a d" && caret_at(asked, 1, 7),
           "after a y on the same line the asked match is where it now is (len_change)");
    expect(caret_at(undone(same), 1, 5), "u clamps Vim's uh_cursor to the restored line");
}

void verify_empty_and_breaks()
{
    Editing star;
    opened(star, "abc", "l:s/x*/-/gc<CR>");
    auto &stars = star.controller();
    const auto all = answered(stars, "yyy");
    expect(whole_vim_body(stars) == "-a-b-c" && caret_at(all, 1, 1) && question_of(all).empty(),
           "empty matches are asked the same as :s///g counts them (Vim's -a-b-c)");

    Editing skipped;
    opened(skipped, "abc", "l:s/x*/-/gc<CR>");
    auto &skip = skipped.controller();
    const auto mixed = answered(skip, "nyn");
    expect(whole_vim_body(skip) == "a-bc" && caret_at(mixed, 1, 4) && caret_at(undone(skip), 1, 3),
           "n y n on empty matches is Vim's a-bc");

    const std::array<std::array<std::string_view, 2>, 3> breaks{{
        {"yq", "X\nY a\nb"},
        {"yy", "X\nY X\nY\nb"},
        {"ny", "a X\nY\nb"},
    }};
    const std::array<std::array<std::size_t, 2>, 3> carets{{{2, 3}, {3, 1}, {2, 1}}};
    for (std::size_t index = 0; index < breaks.size(); ++index)
    {
        Editing broken;
        opened(broken, "a a\nb", "j:%s/a/X\\rY/gc<CR>");
        auto &controller = broken.controller();
        const auto frame = answered(controller, breaks.at(index).at(0));
        expect(
            whole_vim_body(controller) == breaks.at(index).at(1) &&
                caret_at(frame, carets.at(index).at(0), carets.at(index).at(1)),
            "\\r in the replacement moves the caret past the inserted line break (Vim's do_sub)");
        static_cast<void>(undone(controller));
        expect(whole_vim_body(controller) == "a a\nb", "one u undoes the line breaks too");
    }
}

void verify_prompt_and_frame()
{
    Editing editing;
    opened(editing, four_lines, "3G:%s/a/X/gc<CR>");
    auto &controller = editing.controller();
    const auto frame = controller.frame();
    expect(question_of(frame) == "replace with X (y/n/a/q/l)?",
           "the question is Vim's words in English (without ^E/^Y, not taken in stage 2a)");
    expect(caret_at(frame, 1, 1), "the caret is on the asked match");
    expect(!frame.lines.empty() && current_match_is(frame, 0, MatchSpan{1, 2}),
           "the current-match frame is on the asked match");
    expect(frame.ime == app::ImeStance::closed, "the IME is closed while asking");
    const auto next = answered(controller, "y");
    expect(caret_at(next, 1, 3) && current_match_is(next, 0, MatchSpan{3, 4}) &&
               vim_body(next).starts_with("X a a"),
           "y shows the replacement at once and moves the frame to the next match");

    Editing hidden;
    opened(hidden, four_lines, ":set nohlsearch<CR>:%s/a/X/gc<CR>");
    const auto off = hidden.controller().frame();
    expect(current_match_is(off, 0, MatchSpan{1, 2}) && frame_matches(off, 0).empty(),
           "with nohlsearch only the current match is framed (Vim's highlight_match)");

    Editing tilde;
    opened(tilde, "a b", ":s/a/Q/<CR>u:s/b/~Y/c<CR>");
    expect(question_of(tilde.controller().frame()) == "replace with QY (y/n/a/q/l)?",
           "the question shows the replacement after ~ is expanded");
    Editing ampersand;
    opened(ampersand, "ab", ":s/a/[&]/c<CR>");
    expect(question_of(ampersand.controller().frame()) == "replace with [&] (y/n/a/q/l)?",
           "& is shown as typed");
}

void verify_keys_and_signals()
{
    Editing editing;
    opened(editing, four_lines, "3G:%s/a/X/gc<CR>");
    auto &controller = editing.controller();
    vim_normal(controller, "zjkxdd<BS><Up><CR>");
    static_cast<void>(controller.apply_frame(app::SubmitCommand{}));
    static_cast<void>(controller.apply_frame(app::WorkCompleted{}));
    static_cast<void>(controller.apply_frame(app::AutosaveTick{}));
    const auto frame = controller.frame();
    expect(whole_vim_body(controller) == four_lines &&
               question_of(frame) == "replace with X (y/n/a/q/l)?" && caret_at(frame, 1, 1),
           "other keys, Enter and the signals neither answer nor reach the body");
    const auto placed = controller.apply_frame(
        app::PlaceCaret{core::TextPosition{core::LineNumber{3}, core::Column{1}},
                        core::SelectionAnchoring::collapse});
    static_cast<void>(placed);
    expect(question_of(controller.frame()).empty(),
           "a user operation that is not an answer ends the question like q first");

    Editing interrupted;
    opened(interrupted, four_lines, "3G:%s/a/X/gc<CR>");
    auto &other = interrupted.controller();
    static_cast<void>(answered(other, "yyn"));
    static_cast<void>(
        other.apply_frame(app::PlaceCaret{core::TextPosition{core::LineNumber{3}, core::Column{1}},
                                          core::SelectionAnchoring::collapse}));
    expect(whole_vim_body(other) == "X X a\nb a\nc\n  a d", "the replaced matches stay");
    static_cast<void>(undone(other));
    expect(whole_vim_body(other) == four_lines, "what was replaced before the end is one unit");
}

void verify_flags_and_repeats()
{
    Editing kept;
    opened(kept, "a\na\na", ":s/a/X/c<CR>yj:&&<CR>");
    expect(question_of(kept.controller().frame()) == "replace with X (y/n/a/q/l)?",
           ":&& keeps the c flag (Vim's save_do_ask)");
    static_cast<void>(answered(kept.controller(), "q"));
    expect(whole_vim_body(kept.controller()) == "X\na\na" &&
               caret_at(kept.controller().frame(), 2, 1),
           "q on the repeated question replaces nothing");

    Editing dropped;
    opened(dropped, "a\na\na", ":s/a/X/c<CR>yj&");
    expect(whole_vim_body(dropped.controller()) == "X\nX\na" &&
               question_of(dropped.controller().frame()).empty(),
           "& is :s without flags and does not ask");

    Editing global;
    opened(global, "a\na\na", ":s/a/X/c<CR>yg&");
    expect(!question_of(global.controller().frame()).empty(), "g& keeps the flags and asks");
    static_cast<void>(answered(global.controller(), "nq"));
    expect(whole_vim_body(global.controller()) == "X\na\na" &&
               caret_at(global.controller().frame(), 3, 1),
           "g& n q leaves the body and the caret on the last asked match");

    for (const std::string_view keys : {"j:%s/a/X/gcn<CR>", "j:%s/a/X/gnc<CR>"})
    {
        Editing counted;
        opened(counted, "a a\na", keys);
        const auto frame = counted.controller().frame();
        expect(question_of(frame).empty() && message_of(frame) == "3 matches on 2 lines" &&
                   caret_at(frame, 2, 1),
               "n with c only counts (Vim's do_count clears do_ask)");
    }

    Editing missing;
    opened(missing, "b", ":s/a/X/c<CR>");
    expect(message_of(missing.controller().frame()) == "E486: Pattern not found: a" &&
               question_of(missing.controller().frame()).empty(),
           "no match is E486 and nothing is asked");

    Editing reported;
    opened(reported, "a a a\na", "j:%s/a/X/gc<CR>");
    const auto all = answered(reported.controller(), "a");
    expect(whole_vim_body(reported.controller()) == "X X X\nX" && caret_at(all, 2, 1) &&
               message_of(all) == "4 substitutions on 2 lines",
           "a replaces the rest and reports");
    Editing blanks;
    opened(blanks, "  a b a\n  a", ":%s/a/X/gc<CR>");
    const auto landed = answered(blanks.controller(), "a");
    expect(caret_at(landed, 2, 3) && message_of(landed) == "3 substitutions on 2 lines",
           "after a the caret is on the first non-blank like :s without c");
    Editing typed;
    opened(typed, "a a a\na", "j:%s/a/X/gc<CR>");
    expect(message_of(answered(typed.controller(), "yyyy")) == "4 substitutions on 2 lines",
           "y after y also reports past 'report'");
}

void verify_replays()
{
    Editing editing;
    open_vim_document(editing, "a b\na");
    auto &controller = editing.controller();
    // 打った鍵で録る（:normal! の鍵は録画に積まない）。答えの `y` も録られる。
    vim_replay(controller, "qa:s/a/X/c<CR>yq");
    expect(whole_vim_body(controller) == "X b\na", "the typed answer replaces while recording");
    vim_normal(controller, "u");
    vim_replay(controller, "@a");
    const auto frame = controller.frame();
    expect(whole_vim_body(controller) == "a b\na" && question_of(frame).empty() &&
               message_of(frame) == "Not supported: the c flag of :s in a replay",
           "a replay does not open the question and stops (stage 2a limit)");
}
} // namespace

void verify_substitute_confirm_scope()
{
    verify_answers();
    verify_quit_and_skip();
    verify_empty_and_breaks();
    verify_prompt_and_frame();
    verify_keys_and_signals();
    verify_flags_and_repeats();
    verify_replays();
}
} // namespace nenenib::tests
