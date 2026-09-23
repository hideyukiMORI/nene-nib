// scope `--vim-search-incremental` の単体テスト（ADR 0042 決定 2）。
#include "CommandText.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "LineView.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "TestSupport.hpp"
#include "VimSearchPattern.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::application::EditorFrame;
using nenenib::application::SelectEditMode;
using nenenib::application::VisibleLines;
using nenenib::core::EditMode;

namespace app = nenenib::application;

// incsearch の preview（ADR 0041）。入力中の当たりは engine の外の別欄で、表示値の matches と
// current_match と先頭行にだけ現れる。本文・キャレット・last_search は確定まで動かない。
struct IncsearchCase
{
    std::string_view typed;
    std::optional<std::size_t> line;
    MatchSpan span;
};

// 今の一致を持つ見えている行と、その桁。無ければ absent。
[[nodiscard]] std::optional<std::pair<std::size_t, MatchSpan>>
frame_current(const EditorFrame &frame)
{
    for (std::size_t index = 0; index < frame.lines.size(); ++index)
    {
        const auto &current = frame.lines.at(index).current_match;
        if (current.has_value())
        {
            return std::pair{index,
                             MatchSpan{current.value().begin.value, current.value().end.value}};
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool frame_paints_nothing(const EditorFrame &frame)
{
    return std::ranges::all_of(frame.lines, [](const app::LineView &line)
                               { return line.matches.empty() && !line.current_match.has_value(); });
}

[[nodiscard]] std::size_t first_line_of(const EditorFrame &frame)
{
    return frame.lines.empty() ? 0 : frame.lines.front().number.value;
}

// 打った途中の各入力で、当たりの位置・本文とキャレットの不変・報せ無し・Esc の巻き戻し（決定
// 3・5）。
void verify_incsearch_typed_cases()
{
    constexpr std::array<IncsearchCase, 12> cases{{
        {"b", 0, {7, 8}},
        {"be", 0, {7, 9}},
        {"beta", 0, {7, 11}},
        {"gam", 1, {6, 9}},
        {"del", 2, {1, 4}},
        {"a b", 0, {5, 8}},
        {"^b", 1, {1, 2}},
        {"a$", 0, {10, 11}},
        {"al", 0, {1, 3}},
        {"zzz", std::nullopt, {0, 0}},
        {"x", std::nullopt, {0, 0}},
        {"\\(", std::nullopt, {0, 0}},
    }};
    for (const auto &item : cases)
    {
        Editing session;
        open_vim_document(session, "alpha beta\nbeta gamma\ndelta beta");
        EditorController &controller = session.controller();
        vim_replay(controller, "/");
        static_cast<void>(controller.apply(app::CommandText{std::string(item.typed)}));
        const auto typing = controller.frame();
        const auto current = frame_current(typing);
        if (item.line.has_value())
        {
            expect(current == std::optional{std::pair{item.line.value(), item.span}},
                   "the typed pattern shows its next match as the current one");
        }
        else
        {
            expect(frame_paints_nothing(typing),
                   "an invalid or missing pattern shows nothing while typing");
        }
        expect(vim_body(typing) == "alpha beta\nbeta gamma\ndelta beta" && caret_at(typing, 1, 1),
               "the preview moves neither the body nor the caret");
        expect(!typing.command_message.has_value(),
               "the preview reports nothing, not even E486 or a wrap");
        vim_replay(controller, "<Esc>");
        const auto cancelled = controller.frame();
        expect(frame_paints_nothing(cancelled) && caret_at(cancelled, 1, 1) &&
                   !cancelled.command_line.has_value(),
               "Esc takes the preview away and the caret stays");
        expect(!controller.vim_state().last_search.has_value(),
               "a previewed pattern is not remembered until it is submitted");
    }
}

// 全一致の面と、打ち足し・Backspace での更新、Enter で着く位置（決定 3・4・5）。
void verify_incsearch_frame()
{
    Editing session;
    open_vim_document(session, "alpha beta\nbeta gamma\ndelta beta");
    EditorController &controller = session.controller();
    vim_replay(controller, "/be");
    const auto typing = controller.frame();
    expect(frame_matches(typing, 0) == std::vector<MatchSpan>{{7, 9}} &&
               frame_matches(typing, 1) == std::vector<MatchSpan>{{1, 3}} &&
               frame_matches(typing, 2) == std::vector<MatchSpan>{{7, 9}},
           "every visible match of the typed pattern is painted");
    static_cast<void>(controller.apply(app::CommandText{"t"}));
    expect(frame_matches(controller.frame(), 1) == std::vector<MatchSpan>{{1, 4}} &&
               frame_current(controller.frame()) ==
                   std::optional{std::pair{std::size_t{0}, MatchSpan{7, 10}}},
           "typing one more character narrows the preview");
    vim_replay(controller, "<BS>");
    expect(frame_matches(controller.frame(), 1) == std::vector<MatchSpan>{{1, 3}} &&
               frame_current(controller.frame()) ==
                   std::optional{std::pair{std::size_t{0}, MatchSpan{7, 9}}},
           "Backspace goes back to the shorter preview");
    vim_replay(controller, "<BS><BS>");
    expect(frame_paints_nothing(controller.frame()) && controller.frame().command_line.has_value(),
           "an emptied input line shows nothing and stays open");
    vim_replay(controller, "be<CR>");
    const auto landed = controller.frame();
    expect(caret_at(landed, 1, 7) && !landed.command_line.has_value(),
           "Enter lands on the match the preview showed");
    const auto &remembered = controller.vim_state().last_search;
    expect(remembered.has_value() &&
               remembered.value_or(nenenib::core::VimSearchPattern{}).pattern == "be",
           "only Enter remembers the pattern");
    expect(frame_current(landed) == std::optional{std::pair{std::size_t{0}, MatchSpan{7, 9}}},
           "after Enter the current match is the one under the caret");
    vim_replay(controller, "/gam");
    expect(frame_current(controller.frame()) ==
                   std::optional{std::pair{std::size_t{1}, MatchSpan{6, 9}}} &&
               frame_matches(controller.frame(), 0).empty(),
           "a new search previews its own pattern, not the remembered one");
    vim_replay(controller, "<Esc>");
    expect(frame_matches(controller.frame(), 0) == std::vector<MatchSpan>{{7, 9}} &&
               caret_at(controller.frame(), 1, 7),
           "Esc goes back to the remembered highlight and the caret");
}

// 見える範囲の外の当たりへ画面が動き、Esc で入力前の先頭行へ戻る（決定 3・5）。
void verify_incsearch_scroll()
{
    std::string body;
    for (std::size_t number = 1; number <= 30; ++number)
    {
        body += number == 20 ? "target line\n" : "plain line\n";
    }
    body += "last";
    const auto narrow = [&body](Editing &session)
    {
        open_vim_document(session, body);
        applied(session.controller(), VisibleLines{5});
    };
    Editing session;
    narrow(session);
    EditorController &controller = session.controller();
    expect(first_line_of(controller.frame()) == 1, "the narrow view starts at the first line");
    vim_replay(controller, "/tar");
    const auto typing = controller.frame();
    const auto current = frame_current(typing);
    expect(first_line_of(typing) > 1 && current.has_value() &&
               typing.lines.at(current.value_or(std::pair{std::size_t{0}, MatchSpan{}}).first)
                       .number.value == 20,
           "the view follows the previewed match out of sight");
    expect(caret_at(typing, 1, 1), "while the caret itself stays on the first line");
    const std::size_t shown = first_line_of(typing);
    static_cast<void>(controller.apply(app::CommandText{"x"}));
    expect(first_line_of(controller.frame()) == 1 && frame_paints_nothing(controller.frame()),
           "a pattern that stops matching goes back to where typing began");
    vim_replay(controller, "<BS>");
    expect(first_line_of(controller.frame()) == shown,
           "and the same input shows the same view again");
    vim_replay(controller, "<Esc>");
    expect(first_line_of(controller.frame()) == 1 && caret_at(controller.frame(), 1, 1),
           "Esc restores the first line the view had before typing");
    vim_replay(controller, "/tar<CR>");
    const auto previewed = controller.frame();
    Editing plain;
    narrow(plain);
    static_cast<void>(run_ex(plain.controller(), "set noincsearch"));
    vim_replay(plain.controller(), "/tar<CR>");
    const auto unpreviewed = plain.controller().frame();
    expect(caret_at(previewed, 20, 1) && caret_at(unpreviewed, 20, 1),
           "Enter lands on the same line with and without the preview");
    expect(first_line_of(previewed) == first_line_of(unpreviewed),
           "and shows the same view, because the search starts from the view before typing");
    vim_replay(controller, "gg/tar");
    static_cast<void>(controller.apply(VisibleLines{8}));
    vim_replay(controller, "<Esc>");
    expect(first_line_of(controller.frame()) == 1 && controller.frame().lines.size() == 8,
           "a resize while typing keeps the new height and restores the first line only");
}

// 後ろ向きと折り返し（決定 3）。報せは出さない。
void verify_incsearch_directions()
{
    Editing backward;
    open_vim_document(backward, "beta one\nalpha\nbeta two");
    EditorController &controller = backward.controller();
    vim_replay(controller, "j?be");
    expect(frame_current(controller.frame()) ==
               std::optional{std::pair{std::size_t{0}, MatchSpan{1, 3}}},
           "? previews the match before the caret");
    expect(caret_at(controller.frame(), 2, 1), "and the caret stays where it was");
    vim_replay(controller, "<Esc>gg?two");
    expect(frame_current(controller.frame()) ==
                   std::optional{std::pair{std::size_t{2}, MatchSpan{6, 9}}} &&
               !controller.frame().command_message.has_value(),
           "? wraps to the bottom without a notice");
    vim_replay(controller, "<Esc>G/alp");
    expect(frame_current(controller.frame()) ==
                   std::optional{std::pair{std::size_t{1}, MatchSpan{1, 4}}} &&
               !controller.frame().command_message.has_value(),
           "/ wraps to the top without a notice");
    vim_replay(controller, "<CR>");
    expect(caret_at(controller.frame(), 2, 1), "and Enter lands on the wrapped match");
}

// `:set noincsearch` / `:set incsearch` が状態を変え、off では入力中に何も見せない（決定 6）。
void verify_incsearch_option()
{
    Editing session;
    open_vim_document(session, "alpha beta\nbeta gamma");
    EditorController &controller = session.controller();
    expect(controller.vim_state().incsearch, "incsearch is on by default");
    static_cast<void>(run_ex(controller, "set noincsearch"));
    expect(!controller.vim_state().incsearch, ":set noincsearch turns the state off");
    vim_replay(controller, "/be");
    expect(frame_paints_nothing(controller.frame()), "with incsearch off typing shows nothing");
    vim_replay(controller, "<CR>");
    expect(caret_at(controller.frame(), 1, 7) && !frame_matches(controller.frame(), 0).empty(),
           "the submitted search still lands and highlights");
    vim_replay(controller, "/gam");
    expect(frame_matches(controller.frame(), 0) == std::vector<MatchSpan>{{7, 9}} &&
               frame_current(controller.frame()) ==
                   std::optional{std::pair{std::size_t{0}, MatchSpan{7, 9}}},
           "with incsearch off the remembered highlight stays while typing");
    vim_replay(controller, "<Esc>");
    static_cast<void>(run_ex(controller, "set incsearch"));
    expect(controller.vim_state().incsearch, ":set incsearch turns it back on");
    vim_replay(controller, "/gam");
    expect(frame_current(controller.frame()) ==
               std::optional{std::pair{std::size_t{1}, MatchSpan{6, 9}}},
           "and typing previews again");
    static_cast<void>(controller.apply(SelectEditMode{EditMode::ordinary}));
    expect(frame_paints_nothing(controller.frame()) && !controller.frame().command_line.has_value(),
           "leaving Vim while typing takes the preview away with the input line");
}

// 入力中の全一致の面は hlsearch が on のときだけ。off でも preview の当たりの枠は出る（Vim と同じ・
// ADR 0041 の決定 4）。
void verify_incsearch_hlsearch()
{
    Editing session;
    open_vim_document(session, "alpha beta\nbeta gamma\ndelta beta");
    EditorController &controller = session.controller();
    static_cast<void>(run_ex(controller, "set nohlsearch"));
    vim_replay(controller, "/be");
    const auto off = controller.frame();
    expect(std::ranges::all_of(off.lines,
                               [](const app::LineView &line) { return line.matches.empty(); }) &&
               frame_current(off) == std::optional{std::pair{std::size_t{0}, MatchSpan{7, 9}}},
           "with hlsearch off typing shows only the current match");
    vim_replay(controller, "<Esc>");
    static_cast<void>(run_ex(controller, "set hlsearch"));
    vim_replay(controller, "/be");
    const auto on = controller.frame();
    expect(frame_matches(on, 0) == std::vector<MatchSpan>{{7, 9}} &&
               frame_matches(on, 1) == std::vector<MatchSpan>{{1, 3}} &&
               frame_current(on) == std::optional{std::pair{std::size_t{0}, MatchSpan{7, 9}}},
           "with hlsearch on typing paints every match and the current one");
}
} // namespace

void verify_vim_search_incremental_contracts()
{
    verify_incsearch_typed_cases();
    verify_incsearch_frame();
    verify_incsearch_scroll();
    verify_incsearch_directions();
    verify_incsearch_option();
    verify_incsearch_hlsearch();
}

void verify_vim_search_incremental_scope()
{
    verify_vim_search_incremental_contracts();
}
} // namespace nenenib::tests
