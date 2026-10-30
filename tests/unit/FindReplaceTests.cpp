// scope `--find-replace` の単体テスト（ADR 0174 の決定
// 9・#1046）。通常モードの検索と行へ移動（N1a）と 置換の 2 段・1 件と全件の
// Edit・undo（N1b）。時計・ファイル・乱数を使わない。
#include "BeginFind.hpp"
#include "BeginGoToLine.hpp"
#include "BeginReplace.hpp"
#include "CancelCommand.hpp"
#include "CommandEdit.hpp"
#include "CommandText.hpp"
#include "EditCommand.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "FindState.hpp"
#include "GoToLineInput.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InputLinePrompt.hpp"
#include "InputLineView.hpp"
#include "LiteralPattern.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "OpenDocument.hpp"
#include "Scopes.hpp"
#include "SearchHop.hpp"
#include "SelectEditMode.hpp"
#include "SubmitCommand.hpp"
#include "SubstituteAll.hpp"
#include "TestSupport.hpp"
#include "VimMode.hpp"
#include "VimPattern.hpp"
#include "VimPatternCase.hpp"
#include "VimSearchDirection.hpp"
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
using nenenib::application::BeginFind;
using nenenib::application::BeginGoToLine;
using nenenib::application::BeginReplace;
using nenenib::application::CancelCommand;
using nenenib::application::CommandText;
using nenenib::application::EditorFrame;
using nenenib::application::HistoryAction;
using nenenib::application::SearchHop;
using nenenib::application::SelectEditMode;
using nenenib::application::SubmitCommand;
using nenenib::core::EditMode;
using nenenib::core::HistoryDirection;
using nenenib::core::InputLinePrompt;
using nenenib::core::VimSearchDirection;

constexpr std::string_view three_lines = "alpha beta\nbeta gamma\ndelta beta";
constexpr std::string_view wrapped_bottom = "search hit BOTTOM, continuing at TOP";
constexpr std::string_view wrapped_top = "search hit TOP, continuing at BOTTOM";

void open_ordinary_document(Editing &editing, std::string text)
{
    editing.files().hold(Bytes{std::move(text)});
    applied(editing.controller(), nenenib::application::VisibleLines{vim_visible_lines});
    applied(editing.controller(), nenenib::application::OpenDocument{sample_path()});
}

[[nodiscard]] std::optional<InputLinePrompt> prompt_of(const EditorFrame &frame)
{
    return frame.command_line.transform([](const nenenib::core::InputLineView &line)
                                        { return line.prompt; });
}

[[nodiscard]] std::string line_text_of(const EditorFrame &frame)
{
    return frame.command_line.has_value() ? frame.command_line.value().text : std::string{"<none>"};
}

[[nodiscard]] std::string message_of(const EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string{};
}

[[nodiscard]] EditorFrame typed(EditorController &controller, std::string_view text)
{
    return controller.apply_frame(CommandText{std::string(text)});
}

// literal_pattern は magic の 8 文字だけを逃がし、ほかのバイトは写す（決定 4）。`*` `#` の逃がし
// （Vim の nv_ident）も同じ 1 本。
void verify_literal_pattern()
{
    expect(nenenib::core::literal_pattern("a.b*c") == "a\\.b\\*c",
           "the dot and the star are escaped");
    expect(nenenib::core::literal_pattern("\\/.*[~^$") == "\\\\\\/\\.\\*\\[\\~\\^\\$",
           "every magic character of the eight is escaped");
    expect(nenenib::core::literal_pattern("日本 ?+(){}|") == "日本 ?+(){}|",
           "other bytes, the question mark and multibyte characters stay as typed");
    expect(nenenib::core::literal_pattern("").empty(), "the empty word stays empty");
    const auto pattern = nenenib::core::VimPattern::parse(nenenib::core::literal_pattern("a.c[1]"),
                                                          VimSearchDirection::forward,
                                                          nenenib::core::VimPatternCase::sensitive);
    expect(pattern.has_value() &&
               pattern.value().literal_bytes() == std::optional<std::string>{"a.c[1]"},
           "the escaped word parses back to the literal bytes");
}

// 「行: 」の入力（決定 8）。ASCII の 10 進の `行` と `行:桁` だけ。
void verify_go_to_line_input()
{
    using nenenib::core::go_to_line_target;
    const auto line = go_to_line_target("12");
    expect(line.has_value() && line.value().line == 12 && !line.value().column.has_value(),
           "a number is a line");
    const auto both = go_to_line_target("3:007");
    expect(both.has_value() && both.value().line == 3 && both.value().column == std::size_t{7},
           "a line and a column split at the colon and skip leading zeros");
    const auto huge = go_to_line_target("99999999999999999999999:1");
    expect(huge.has_value() && huge.value().line == std::numeric_limits<std::size_t>::max(),
           "a line too large saturates like :N");
    constexpr std::array<std::string_view, 9> refused{
        {"", "abc", "１２", "3:", ":5", " 3", "3 ", "-3", "3:4:5"}};
    for (const auto text : refused)
    {
        expect(!go_to_line_target(text).has_value(),
               "anything but ASCII digits and one colon is not a line");
    }
}

// 今の当たり（決定 7 の前提・置換の 1 件が使う）。
void verify_current_match()
{
    const auto text = buffer_of("abc abc\nxyz");
    const auto pattern = nenenib::core::VimPattern::parse("abc", VimSearchDirection::forward,
                                                          nenenib::core::VimPatternCase::sensitive);
    if (!pattern)
    {
        expect(false, "the pattern parses");
        return;
    }
    const auto at = [&](std::size_t caret)
    {
        return nenenib::application::current_find_match(text, pattern.value(),
                                                        nenenib::core::Offset{caret});
    };
    const auto range = [](std::size_t begin, std::size_t end) { return std::pair{begin, end}; };
    const auto span = [](const std::optional<nenenib::core::OffsetRange> &found)
    {
        return found.has_value()
                   ? std::optional{std::pair{found.value().begin.value, found.value().end.value}}
                   : std::nullopt;
    };
    expect(span(at(0)) == range(0, 3) && span(at(2)) == range(0, 3),
           "the match containing the caret is the current one");
    expect(span(at(3)) == range(4, 7), "between matches the next one is current");
    expect(span(at(9)) == range(0, 3), "after the last match the search wraps to the first");
    const auto missing = nenenib::core::VimPattern::parse("zzz", VimSearchDirection::forward,
                                                          nenenib::core::VimPatternCase::sensitive);
    expect(missing.has_value() && !nenenib::application::current_find_match(
                                       text, missing.value(), nenenib::core::Offset{0})
                                       .has_value(),
           "a word that never matches has no current match");
}

// Ctrl+F → 打つ → Enter / F3 / Shift+F3 → 折り返しの報せ → Esc（決定 3・5・6）。
void verify_find_hops()
{
    Editing session;
    open_ordinary_document(session, std::string(three_lines));
    EditorController &controller = session.controller();
    const auto opened = controller.apply_frame(BeginFind{});
    expect(prompt_of(opened) == InputLinePrompt::find && line_text_of(opened).empty(),
           "Ctrl+F opens the find prompt empty the first time");
    const auto typing = typed(controller, "beta");
    expect(current_match_is(typing, 0, MatchSpan{7, 11}) && caret_at(typing, 1, 1),
           "incsearch previews the next match while the caret stays");
    const auto first = controller.apply_frame(SubmitCommand{});
    expect(caret_at(first, 1, 7) && prompt_of(first) == InputLinePrompt::find,
           "Enter moves to the next match and keeps the prompt open");
    const auto &remembered = controller.vim_state().last_search;
    expect(remembered.has_value() && remembered.value().text() == "beta",
           "the word becomes the last search of vim");
    expect(frame_matches(first, 1) == std::vector<MatchSpan>{{1, 5}} &&
               frame_matches(first, 2) == std::vector<MatchSpan>{{7, 11}},
           "hlsearch paints every match in the ordinary mode");
    expect(caret_at(controller.apply_frame(SubmitCommand{}), 2, 1), "Enter again goes on");
    expect(caret_at(controller.apply_frame(SearchHop{VimSearchDirection::forward}), 3, 7),
           "F3 in the prompt goes to the next match");
    const auto wrapped = controller.apply_frame(SearchHop{VimSearchDirection::forward});
    expect(caret_at(wrapped, 1, 7) && message_of(wrapped) == wrapped_bottom,
           "past the bottom the search wraps and says so like vim");
    const auto back = controller.apply_frame(SearchHop{VimSearchDirection::backward});
    expect(caret_at(back, 3, 7) && message_of(back) == wrapped_top,
           "Shift+F3 goes back past the top");
    const auto closed = controller.apply_frame(CancelCommand{});
    expect(!closed.command_line.has_value() && caret_at(closed, 3, 7),
           "Esc closes the prompt and the caret stays on the match");
    const auto next = controller.apply_frame(SearchHop{VimSearchDirection::forward});
    expect(caret_at(next, 1, 7), "F3 without the prompt goes to the next match");
    expect(caret_at(controller.apply_frame(SearchHop{VimSearchDirection::backward}), 3, 7),
           "Shift+F3 without the prompt goes to the previous match");
    const auto again = controller.apply_frame(BeginFind{});
    expect(prompt_of(again) == InputLinePrompt::find && line_text_of(again) == "beta",
           "Ctrl+F opens with the last word");
    expect(session.bell().rings() == 0, "a found word does not ring");
}

// literal・E486・空の語・Esc の巻き戻し・Vim の `n` へ続く（決定 4・5・6）。
void verify_find_literal_and_failures()
{
    Editing session;
    open_ordinary_document(session, "abc a.c a.c\nxyz");
    EditorController &controller = session.controller();
    static_cast<void>(controller.apply_frame(BeginFind{}));
    static_cast<void>(typed(controller, "a.c"));
    const auto found = controller.apply_frame(SubmitCommand{});
    expect(caret_at(found, 1, 5), "the dot is a literal dot, not any character");
    const auto &remembered = controller.vim_state().last_search;
    expect(remembered.has_value() && remembered.value().text() == "a\\.c",
           "the last search holds the escaped word");
    static_cast<void>(controller.apply_frame(CancelCommand{}));
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    vim_replay(controller, "n");
    expect(caret_at(controller.frame(), 1, 9), "vim n continues the same literal search");
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::ordinary}));
    static_cast<void>(controller.apply_frame(BeginFind{}));
    static_cast<void>(controller.apply_frame(
        nenenib::application::EditCommand{nenenib::core::CommandEdit::home}));
    const auto before = controller.frame();
    static_cast<void>(typed(controller, "zz"));
    const auto missing = controller.apply_frame(SubmitCommand{});
    expect(message_of(missing).starts_with("E486") && session.bell().rings() == 1 &&
               prompt_of(missing) == InputLinePrompt::find &&
               missing.caret.position == before.caret.position,
           "a missing word says E486, rings once and keeps the prompt and the caret");
    Editing empty;
    open_ordinary_document(empty, "abc");
    static_cast<void>(empty.controller().apply_frame(BeginFind{}));
    const auto nothing = empty.controller().apply_frame(SubmitCommand{});
    expect(caret_at(nothing, 1, 1) && prompt_of(nothing) == InputLinePrompt::find &&
               !empty.controller().vim_state().last_search.has_value() &&
               message_of(nothing).empty(),
           "an empty word does nothing");
    static_cast<void>(typed(empty.controller(), "c"));
    const auto cancelled = empty.controller().apply_frame(CancelCommand{});
    expect(caret_at(cancelled, 1, 1) && !cancelled.command_line.has_value() &&
               !empty.controller().vim_state().last_search.has_value(),
           "Esc before Enter leaves the caret and remembers nothing");
}

// 行へ移動（決定 8）。`:N` と同じ着地・`行:桁`・範囲の外・数でない入力。
void verify_go_to_line()
{
    Editing session;
    open_ordinary_document(session, "one\n  two\nthree\n日本語の行");
    EditorController &controller = session.controller();
    const auto opened = controller.apply_frame(BeginGoToLine{});
    expect(prompt_of(opened) == InputLinePrompt::go_to_line && line_text_of(opened).empty(),
           "Ctrl+G opens the line prompt empty");
    static_cast<void>(typed(controller, "2"));
    const auto line = controller.apply_frame(SubmitCommand{});
    expect(caret_at(line, 2, 3) && !line.command_line.has_value(),
           "a line lands on its first non-blank like :2");
    static_cast<void>(controller.apply_frame(BeginGoToLine{}));
    static_cast<void>(typed(controller, "4:3"));
    expect(caret_at(controller.apply_frame(SubmitCommand{}), 4, 3),
           "a line and a column land on that character");
    static_cast<void>(controller.apply_frame(BeginGoToLine{}));
    static_cast<void>(typed(controller, "99:99"));
    expect(caret_at(controller.apply_frame(SubmitCommand{}), 4, 6),
           "beyond the end the line and the column are clipped");
    static_cast<void>(controller.apply_frame(BeginGoToLine{}));
    static_cast<void>(typed(controller, "0"));
    expect(caret_at(controller.apply_frame(SubmitCommand{}), 1, 1), "line 0 is the first line");
    static_cast<void>(controller.apply_frame(BeginGoToLine{}));
    static_cast<void>(typed(controller, "x"));
    const auto refused = controller.apply_frame(SubmitCommand{});
    expect(message_of(refused) == "行番号を入れてください" && session.bell().rings() == 1 &&
               caret_at(refused, 1, 1) && !refused.command_line.has_value(),
           "a line that is not a number is refused in one line");
    static_cast<void>(controller.apply_frame(BeginGoToLine{}));
    const auto blank = controller.apply_frame(SubmitCommand{});
    expect(caret_at(blank, 1, 1) && !blank.command_line.has_value() && message_of(blank).empty(),
           "an empty line prompt closes quietly");
    Editing ex;
    open_ordinary_document(ex, "one\n  two\nthree\n日本語の行");
    static_cast<void>(ex.controller().apply_frame(SelectEditMode{EditMode::vim}));
    const auto colon = run_ex(ex.controller(), "2");
    expect(colon.caret.position == line.caret.position,
           "the line prompt and :N land on the same place");
}

// Vim モードは鍵が Vim のまま（表の行で守る）・一覧からの前置き（決定 2）・置換は後続。
void verify_vim_alternatives()
{
    Editing session;
    open_vim_document(session, std::string(three_lines));
    EditorController &controller = session.controller();
    const auto slash = controller.apply_frame(BeginFind{});
    expect(prompt_of(slash) == InputLinePrompt::search_forward && line_text_of(slash).empty(),
           "find from the list opens the vim / line");
    vim_replay(controller, "gam<CR>");
    expect(caret_at(controller.frame(), 2, 6), "the / line searches as vim does");
    vim_replay(controller, "gg0");
    static_cast<void>(controller.apply_frame(SearchHop{VimSearchDirection::forward}));
    expect(caret_at(controller.frame(), 2, 6), "find next from the list is vim n");
    const auto colon = controller.apply_frame(BeginGoToLine{});
    expect(prompt_of(colon) == InputLinePrompt::ex && line_text_of(colon).empty(),
           "go to line from the list opens the empty : line");
    static_cast<void>(controller.apply_frame(CancelCommand{}));
    vim_replay(controller, "i");
    const auto from_insert = controller.apply_frame(BeginFind{});
    expect(prompt_of(from_insert) == InputLinePrompt::search_forward &&
               controller.vim_state().mode == nenenib::core::VimMode::normal &&
               vim_body(from_insert) == three_lines,
           "find from INSERT returns to NORMAL first and types no slash");
    static_cast<void>(controller.apply_frame(CancelCommand{}));
    vim_replay(controller, "i");
    const auto replace = controller.apply_frame(BeginReplace{});
    expect(prompt_of(replace) == InputLinePrompt::ex && line_text_of(replace) == "%s/" &&
               controller.vim_state().mode == nenenib::core::VimMode::normal &&
               vim_body(replace) == three_lines,
           "replace from the list opens the : line with %s/ after leaving INSERT");
}

[[nodiscard]] std::optional<nenenib::core::VimPattern> literal_of(std::string_view word)
{
    auto parsed = nenenib::core::VimPattern::parse(nenenib::core::literal_pattern(word),
                                                   VimSearchDirection::forward,
                                                   nenenib::core::VimPatternCase::sensitive);
    if (!parsed)
    {
        return std::nullopt;
    }
    return std::move(parsed).value();
}

// 全件の置換の純関数（決定 7）。最初の当たりから最後の当たりまでの Edit 1 つ・重なりは飛ばす・
// 当たりが無ければ値なし・置換語は literal。
void verify_substituted_all()
{
    using nenenib::core::substituted_all;
    const auto text = buffer_of("say abc\nabc\nxabc tail");
    const auto abc = literal_of("abc");
    if (!abc.has_value())
    {
        expect(false, "the word parses");
        return;
    }
    const auto edit = substituted_all(text, abc.value(), "&\\n");
    expect(edit.has_value() && edit.value().at.value == 4 &&
               edit.value().removed == "abc\nabc\nxabc" &&
               edit.value().inserted == "&\\n\n&\\n\nx&\\n" && edit.value().restore.value == 4,
           "every match from the first to the last is one edit and the replacement is literal");
    const auto overlapping = literal_of("aa");
    const auto pairs = substituted_all(buffer_of("aaaaa"), overlapping.value_or(abc.value()), "b");
    expect(pairs.has_value() && pairs.value().removed == "aaaa" && pairs.value().inserted == "bb",
           "overlapping matches are skipped like :s///g");
    const auto japanese = literal_of("日本");
    const auto wide =
        substituted_all(buffer_of("日本語と日本"), japanese.value_or(abc.value()), "");
    expect(wide.has_value() && wide.value().removed == "日本語と日本" &&
               wide.value().inserted == "語と",
           "multibyte matches are replaced by their bytes and an empty replacement deletes");
    const auto missing = literal_of("zzz");
    expect(!substituted_all(text, missing.value_or(abc.value()), "x").has_value(),
           "a word that never matches has no edit");
}

[[nodiscard]] std::string detail_of(const EditorFrame &frame)
{
    return frame.command_line.has_value() ? frame.command_line.value().prompt_detail
                                          : std::string{"<none>"};
}

// Ctrl+H → 「検索: 」→ Enter → 「置換後 (語): 」→ Enter で 1 件ずつ（決定 3・7）。1 件ごとに undo 1
// 回。
void verify_replace_one()
{
    Editing session;
    open_ordinary_document(session, std::string(three_lines));
    EditorController &controller = session.controller();
    const auto opened = controller.apply_frame(BeginReplace{});
    expect(prompt_of(opened) == InputLinePrompt::find && line_text_of(opened).empty(),
           "Ctrl+H opens the find prompt");
    static_cast<void>(typed(controller, "beta"));
    const auto next = controller.apply_frame(SubmitCommand{});
    expect(prompt_of(next) == InputLinePrompt::replace_with && line_text_of(next).empty() &&
               detail_of(next) == "beta" && caret_at(next, 1, 7),
           "Enter moves to the first match and asks for the replacement with the word");
    static_cast<void>(typed(controller, "B"));
    const auto first = controller.apply_frame(SubmitCommand{});
    expect(vim_body(first) == "alpha B\nbeta gamma\ndelta beta" && caret_at(first, 2, 1) &&
               prompt_of(first) == InputLinePrompt::replace_with,
           "Enter replaces the current match and moves to the next one");
    const auto second = controller.apply_frame(SubmitCommand{});
    expect(vim_body(second) == "alpha B\nB gamma\ndelta beta" && caret_at(second, 3, 7),
           "Enter again replaces the next one");
    const auto skipped = controller.apply_frame(SearchHop{VimSearchDirection::forward});
    expect(vim_body(skipped) == vim_body(second) && caret_at(skipped, 3, 7) &&
               message_of(skipped) == wrapped_bottom,
           "F3 in the replacement prompt moves without replacing");
    const auto last = controller.apply_frame(SubmitCommand{});
    expect(vim_body(last) == "alpha B\nB gamma\ndelta B" && message_of(last).starts_with("E486") &&
               session.bell().rings() == 0,
           "after the last match the prompt says E486 without ringing");
    const auto closed = controller.apply_frame(CancelCommand{});
    expect(!closed.command_line.has_value() && vim_body(closed) == vim_body(last),
           "Esc closes the prompt");
    const auto undone = controller.apply_frame(HistoryAction{HistoryDirection::undo});
    expect(vim_body(undone) == "alpha B\nB gamma\ndelta beta",
           "one undo takes back one replacement");
    static_cast<void>(controller.apply_frame(HistoryAction{HistoryDirection::undo}));
    expect(vim_body(controller.apply_frame(HistoryAction{HistoryDirection::undo})) == three_lines,
           "each replacement is its own undo step");
    static_cast<void>(controller.apply_frame(BeginReplace{}));
    const auto again = controller.apply_frame(SubmitCommand{});
    expect(prompt_of(again) == InputLinePrompt::replace_with && line_text_of(again) == "B",
           "Ctrl+H opens with the last word and the last replacement");
    static_cast<void>(controller.apply_frame(CancelCommand{}));
    static_cast<void>(controller.apply_frame(BeginFind{}));
    const auto find = controller.apply_frame(SubmitCommand{});
    expect(prompt_of(find) == InputLinePrompt::find, "Ctrl+F after Ctrl+H only finds");
}

// 置換語が語を含んでも置き換えた所を探し直さない・見つからない語は「検索: 」のまま（決定 7）。
void verify_replace_edges()
{
    Editing session;
    open_ordinary_document(session, "a a");
    EditorController &controller = session.controller();
    static_cast<void>(controller.apply_frame(BeginReplace{}));
    static_cast<void>(typed(controller, "a"));
    static_cast<void>(controller.apply_frame(SubmitCommand{}));
    static_cast<void>(typed(controller, "aa"));
    const auto grown = controller.apply_frame(SubmitCommand{});
    expect(vim_body(grown) == "aa a" && caret_at(grown, 1, 4),
           "the match under the caret at the start is replaced and the replacement is not "
           "searched again");
    Editing missing;
    open_ordinary_document(missing, "abc");
    static_cast<void>(missing.controller().apply_frame(BeginReplace{}));
    static_cast<void>(typed(missing.controller(), "zz"));
    const auto refused = missing.controller().apply_frame(SubmitCommand{});
    expect(prompt_of(refused) == InputLinePrompt::find && message_of(refused).starts_with("E486") &&
               missing.bell().rings() == 1,
           "a missing word stays in the find prompt with E486 and one ring");
    const auto all_in_find = missing.controller().apply_frame(SubmitCommand{true});
    expect(prompt_of(all_in_find) == InputLinePrompt::find && vim_body(all_in_find) == "abc",
           "Ctrl+Enter outside the replacement prompt does nothing");
}

// Ctrl+F で当たりへ移ってから Ctrl+H → Enter → Enter は、見ていた当たりを置き換える（R3-1・決定 7
// の 「今の当たり」）。
void verify_replace_seen_match()
{
    Editing session;
    open_ordinary_document(session, std::string(three_lines));
    EditorController &controller = session.controller();
    static_cast<void>(controller.apply_frame(BeginFind{}));
    static_cast<void>(typed(controller, "beta"));
    static_cast<void>(controller.apply_frame(SubmitCommand{}));
    static_cast<void>(controller.apply_frame(SubmitCommand{}));
    const auto seen = controller.apply_frame(CancelCommand{});
    expect(caret_at(seen, 2, 1), "Ctrl+F and Enter twice stand on the second match");
    static_cast<void>(controller.apply_frame(BeginReplace{}));
    const auto asked = controller.apply_frame(SubmitCommand{});
    expect(prompt_of(asked) == InputLinePrompt::replace_with && caret_at(asked, 2, 1),
           "Enter in Ctrl+H stays on the match under the caret");
    static_cast<void>(typed(controller, "X"));
    const auto replaced = controller.apply_frame(SubmitCommand{});
    expect(vim_body(replaced) == "alpha beta\nX gamma\ndelta beta" && caret_at(replaced, 3, 7),
           "Enter replaces the match that was seen and moves to the next one");
}

// Ctrl+Enter は全件を Edit 1 回で置き換え、undo 1 回で戻る（決定 7）。
void verify_replace_all()
{
    Editing session;
    open_ordinary_document(session, std::string(three_lines));
    EditorController &controller = session.controller();
    static_cast<void>(controller.apply_frame(BeginReplace{}));
    static_cast<void>(typed(controller, "beta"));
    static_cast<void>(controller.apply_frame(SubmitCommand{}));
    static_cast<void>(typed(controller, "BB"));
    const auto all = controller.apply_frame(SubmitCommand{true});
    expect(vim_body(all) == "alpha BB\nBB gamma\ndelta BB" &&
               prompt_of(all) == InputLinePrompt::replace_with && caret_at(all, 3, 9),
           "Ctrl+Enter replaces every match at once and keeps the prompt");
    const auto none = controller.apply_frame(SubmitCommand{true});
    expect(vim_body(none) == vim_body(all) && message_of(none).starts_with("E486") &&
               session.bell().rings() == 1,
           "Ctrl+Enter without any match says E486 and rings once");
    static_cast<void>(controller.apply_frame(CancelCommand{}));
    const auto undone = controller.apply_frame(HistoryAction{HistoryDirection::undo});
    expect(vim_body(undone) == three_lines, "one undo takes back every replacement");
    const auto again = controller.apply_frame(HistoryAction{HistoryDirection::redo});
    expect(vim_body(again) == vim_body(all), "one redo replays them");
}

// 通常モードの検索と Vim の `/` は同じ最後の検索で、塗りもモードをまたいで続く（決定 5）。
void verify_shared_last_search()
{
    Editing session;
    open_vim_document(session, std::string(three_lines));
    EditorController &controller = session.controller();
    vim_replay(controller, "/gamma<CR>");
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::ordinary}));
    const auto ordinary = controller.frame();
    expect(frame_matches(ordinary, 1) == std::vector<MatchSpan>{{6, 11}},
           "a vim search stays painted in the ordinary mode");
    expect(caret_at(controller.apply_frame(SearchHop{VimSearchDirection::forward}), 2, 6),
           "F3 continues a vim search in the ordinary mode");
}
} // namespace

void verify_find_replace_scope()
{
    verify_literal_pattern();
    verify_go_to_line_input();
    verify_current_match();
    verify_find_hops();
    verify_find_literal_and_failures();
    verify_go_to_line();
    verify_vim_alternatives();
    verify_shared_last_search();
    verify_substituted_all();
    verify_replace_one();
    verify_replace_edges();
    verify_replace_seen_match();
    verify_replace_all();
}
} // namespace nenenib::tests
