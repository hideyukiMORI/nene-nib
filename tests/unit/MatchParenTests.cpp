// scope `--matchparen` の単体テスト（ADR 0164・Issue #999）。core の matched_bracket（対 3 種・
// 窓の打ち切り・見つからない・入れ子・除く区間・巨大な行の 1 行の中）と、固定 seed のランダムな
// 括弧の列で素朴な全文の走査と窓の中で一致する性質、application の frame の LineView::bracket の
// 写し（INSERT と通常モードの直前・構文の区間の除外・見えていない相手は写さない）。時計・ファイル・
// 乱数の種を読まない。
#include "ComposeText.hpp"
#include "Composition.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "FilePath.hpp"
#include "LineView.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "OpenDocument.hpp"
#include "PlaceCaret.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SelectionSpan.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextPosition.hpp"
#include "VimMatchingPair.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
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
using core::Offset;
using core::OffsetRange;
using Random = std::mt19937_64;

constexpr std::string_view code_path = "C:\\work\\paren.cpp";
constexpr std::string_view plain_path = "C:\\work\\paren.txt";
constexpr std::uint64_t property_seed = 999;
constexpr std::size_t property_rounds = 400;
constexpr std::size_t property_length = 120;
constexpr std::string_view property_alphabet = "()[]{}ab\n";

[[nodiscard]] core::TextBuffer buffer_of(std::string_view text)
{
    return core::TextBuffer::from_utf8(text).value();
}

[[nodiscard]] OffsetRange whole(const core::TextBuffer &text)
{
    return OffsetRange{Offset{0}, Offset{text.size_bytes()}};
}

[[nodiscard]] std::optional<Offset> matched(const core::TextBuffer &text, std::size_t at)
{
    return core::matched_bracket(text, Offset{at}, whole(text), {});
}

// 対 3 種のどちらの向きも、入れ子を数えて相手に着く。
void verify_pairs()
{
    const auto text = buffer_of("a(b)c[d]e{f}");
    expect(matched(text, 1) == Offset{3} && matched(text, 3) == Offset{1},
           "parentheses match both ways");
    expect(matched(text, 5) == Offset{7} && matched(text, 7) == Offset{5},
           "square brackets match both ways");
    expect(matched(text, 9) == Offset{11} && matched(text, 11) == Offset{9},
           "braces match both ways");
    const auto nested = buffer_of("((a)[(b)])");
    expect(matched(nested, 0) == Offset{9} && matched(nested, 9) == Offset{0} &&
               matched(nested, 1) == Offset{3} && matched(nested, 5) == Offset{7},
           "nested pairs count the depth of the same kind");
    expect(!matched(text, 0).has_value() && !matched(text, 2).has_value(),
           "a character that is not a bracket has no partner (no search to the right like %)");
    const auto lone = buffer_of("(()");
    expect(!matched(lone, 0).has_value() && matched(lone, 1) == Offset{2},
           "an unbalanced bracket has no partner");
    expect(core::vim_matching_pair(text, Offset{0}) == Offset{3},
           "% still searches to the right on the line through the same scan");
}

// 窓の外へは出ない（全文を走らない・決定 2）。
void verify_window()
{
    const auto text = buffer_of("(\nx\ny\n)\n");
    expect(matched(text, 0) == Offset{6}, "the whole window finds the partner three lines down");
    const auto nested = buffer_of("(\n(\n)\n)");
    expect(matched(nested, 0) == Offset{6} && matched(nested, 6) == Offset{0},
           "the depth carries over line breaks (the % scan reset it on each line before #999)");
    expect(
        !core::matched_bracket(text, Offset{0}, OffsetRange{Offset{0}, Offset{6}}, {}).has_value(),
        "a window that ends before the partner gives nothing");
    expect(
        !core::matched_bracket(text, Offset{6}, OffsetRange{Offset{2}, Offset{8}}, {}).has_value(),
        "a window that starts after the partner gives nothing backwards");
    expect(
        !core::matched_bracket(text, Offset{0}, OffsetRange{Offset{2}, Offset{8}}, {}).has_value(),
        "a bracket outside the window is not searched");
    // 巨大な行の 1 行の中でも窓で打ち切る。
    std::string giant(40'000, 'x');
    giant.front() = '(';
    giant.back() = ')';
    const auto long_line = buffer_of(giant);
    expect(matched(long_line, 0) == Offset{giant.size() - 1},
           "the whole window reaches the far end of one long line");
    expect(!core::matched_bracket(long_line, Offset{0}, OffsetRange{Offset{0}, Offset{4096}}, {})
                .has_value(),
           "a window inside one long line stops the scan in that line");
}

// 除く区間の中の括弧は数えない（決定 3）。
void verify_skipped()
{
    const auto text = buffer_of("( /* ) */ )");
    expect(matched(text, 0) == Offset{5}, "without ranges the comment bracket is the partner");
    const std::array<OffsetRange, 1> comment{{OffsetRange{Offset{2}, Offset{9}}}};
    expect(core::matched_bracket(text, Offset{0}, whole(text), comment) == Offset{10} &&
               core::matched_bracket(text, Offset{10}, whole(text), comment) == Offset{0},
           "brackets inside a skipped range are not counted");
}

// 素朴な全文の走査（引用符も `\` も無い本文）。同じ種類の括弧の深さが負になった所が相手。
[[nodiscard]] std::optional<std::size_t> naive_partner(std::string_view text, std::size_t at)
{
    constexpr std::string_view opens = "([{";
    constexpr std::string_view closes = ")]}";
    const char self = text.at(at);
    const auto open = opens.find(self);
    const bool forward = open != std::string_view::npos;
    const char other = forward ? closes.at(open) : opens.at(closes.find(self));
    const std::ptrdiff_t step = forward ? 1 : -1;
    const auto size = static_cast<std::ptrdiff_t>(text.size());
    std::ptrdiff_t depth = 0;
    for (auto index = static_cast<std::ptrdiff_t>(at) + step; index >= 0 && index < size;
         index += step)
    {
        const char byte = text.at(static_cast<std::size_t>(index));
        depth +=
            static_cast<std::ptrdiff_t>(byte == self) - static_cast<std::ptrdiff_t>(byte == other);
        if (depth < 0)
        {
            return static_cast<std::size_t>(index);
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::string random_text(Random &random)
{
    std::string text;
    const std::size_t length = 1 + (random() % property_length);
    for (std::size_t index = 0; index < length; ++index)
    {
        text.push_back(property_alphabet.at(random() % property_alphabet.size()));
    }
    return text;
}

[[nodiscard]] bool bracket_at(std::string_view text, std::size_t at)
{
    return std::string_view("()[]{}").find(text.at(at)) != std::string_view::npos;
}

// 1 つの括弧で、ランダムな窓の答えと本文の全体の答えが素朴な走査と一致するか。
[[nodiscard]] bool agrees(const core::TextBuffer &text, std::string_view body, std::size_t at,
                          Random &random)
{
    const std::size_t begin = random() % (at + 1);
    const std::size_t end = at + 1 + (random() % (body.size() - at));
    const auto expected =
        naive_partner(body, at).transform([](std::size_t value) { return Offset{value}; });
    const bool inside =
        expected.has_value() && begin <= expected.value().value && expected.value().value < end;
    const auto found =
        core::matched_bracket(text, Offset{at}, OffsetRange{Offset{begin}, Offset{end}}, {});
    return found == (inside ? expected : std::nullopt) && matched(text, at) == expected;
}

// 性質: 窓の中に相手があれば素朴な走査と同じ位置、無ければ（対が無い時も）値なし。
void verify_property()
{
    Random random(property_seed);
    std::size_t compared = 0;
    bool agreed = true;
    for (std::size_t round = 0; round < property_rounds; ++round)
    {
        const std::string body = random_text(random);
        const auto text = buffer_of(body);
        for (std::size_t at = 0; at < body.size(); ++at)
        {
            const bool bracket = bracket_at(body, at);
            agreed = (!bracket || agrees(text, body, at, random)) && agreed;
            compared += bracket ? 1 : 0;
        }
    }
    expect(compared > property_rounds, "the property compares many brackets");
    expect(agreed, "matched_bracket agrees with a naive whole-text scan inside every window");
}

[[nodiscard]] core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

void open_at(Editing &editing, std::string_view path, std::string text, std::size_t visible)
{
    editing.files().hold_at(std::string(path), Bytes{std::move(text)});
    applied(editing.controller(), app::VisibleLines{visible});
    applied(editing.controller(), app::OpenDocument{path_of(path)});
}

void place(app::EditorController &controller, std::size_t line, std::size_t column)
{
    static_cast<void>(controller.apply(
        app::PlaceCaret{core::TextPosition{core::LineNumber{line}, core::Column{column}},
                        core::SelectionAnchoring::collapse}));
}

[[nodiscard]] core::SelectionSpan cell(std::size_t column)
{
    return core::SelectionSpan{core::SelectionPresence::present, core::Column{column},
                               core::Column{column + 1}};
}

[[nodiscard]] std::size_t marked_lines(const app::EditorFrame &frame)
{
    std::size_t count = 0;
    for (const auto &line : frame.lines)
    {
        count += line.bracket.has_value() ? 1 : 0;
    }
    return count;
}

[[nodiscard]] bool marked_at(const app::EditorFrame &frame, std::size_t index, std::size_t column)
{
    return marked_lines(frame) == 1 && frame.lines.at(index).bracket == cell(column) &&
           !frame.lines.at(index).current_match.has_value();
}

// Vim の NORMAL はキャレットの下だけ、INSERT は直前も（決定 1）。
void verify_vim_frame()
{
    Editing editing;
    auto &controller = editing.controller();
    open_at(editing, plain_path, "f(a, [b])\nx", vim_visible_lines);
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    vim_replay(controller, "l");
    expect(marked_at(controller.frame(), 0, 9), "NORMAL on ( marks the ) at column 9");
    vim_replay(controller, "fb");
    expect(marked_lines(controller.frame()) == 0, "NORMAL on a letter marks nothing");
    vim_replay(controller, "l");
    expect(marked_at(controller.frame(), 0, 6), "NORMAL on ] marks the [");
    vim_replay(controller, "A");
    expect(marked_at(controller.frame(), 0, 2),
           "INSERT at the line end looks at the ) before the caret");
    vim_replay(controller, "<Esc>0");
    expect(marked_lines(controller.frame()) == 0, "NORMAL on f does not look before or right");
}

// 通常モード（棒のキャレット）は INSERT と同じく下を先に、なければ直前。
void verify_ordinary_frame()
{
    Editing editing;
    auto &controller = editing.controller();
    open_at(editing, plain_path, "{a}(b)", vim_visible_lines);
    place(controller, 1, 4);
    expect(marked_at(controller.frame(), 0, 6), "the bracket after the bar caret wins");
    place(controller, 1, 7);
    expect(marked_at(controller.frame(), 0, 4), "at the line end the bracket before is used");
    place(controller, 1, 2);
    expect(marked_at(controller.frame(), 0, 3), "after { with a letter under the caret uses {");
    place(controller, 1, 1);
    expect(marked_at(controller.frame(), 0, 3), "on { at the line start the } is marked");
}

// 構文の区間（決定 3）: 区間の外の括弧は区間の中を数えず、区間の中は同じ区間の中だけ。
void verify_syntax_frame()
{
    const std::string body = "f( // )\n);";
    Editing code;
    open_at(code, code_path, body, vim_visible_lines);
    place(code.controller(), 1, 2);
    expect(marked_at(code.controller().frame(), 1, 1),
           "a code bracket skips the comment and reaches the next line");
    place(code.controller(), 1, 7);
    expect(marked_lines(code.controller().frame()) == 0,
           "a bracket inside a comment looks only inside that comment");
    Editing plain;
    open_at(plain, plain_path, body, vim_visible_lines);
    place(plain.controller(), 1, 2);
    expect(marked_at(plain.controller().frame(), 0, 7),
           "a language without colors does not skip anything");
    Editing quoted;
    open_at(quoted, code_path, "s = \"(a)\";", vim_visible_lines);
    place(quoted.controller(), 1, 6);
    expect(marked_at(quoted.controller().frame(), 0, 8),
           "a bracket inside a string finds its partner in the same string");
}

// 相手が見えていない・窓より遠い・変換中は写さない（決定 2・4）。
void verify_hidden_partner()
{
    std::string body = "(\n";
    for (std::size_t index = 0; index < 4; ++index)
    {
        body += "x\n";
    }
    body += ")";
    Editing near;
    open_at(near, plain_path, body, 3);
    place(near.controller(), 1, 1);
    expect(marked_lines(near.controller().frame()) == 0,
           "a partner below the visible lines is not shown");
    Editing far;
    std::string distant = "(\n";
    for (std::size_t index = 0; index < 20; ++index)
    {
        distant += "x\n";
    }
    distant += ")";
    open_at(far, plain_path, distant, 30);
    place(far.controller(), 1, 1);
    expect(marked_at(far.controller().frame(), 21, 1), "a visible partner 21 lines down is shown");
    static_cast<void>(
        far.controller().apply(app::ComposeText{core::Composition{"x", {}, core::Offset{1}}}));
    expect(marked_lines(far.controller().frame()) == 0, "nothing is searched while composing");
}
} // namespace

void verify_matchparen_scope()
{
    verify_pairs();
    verify_window();
    verify_skipped();
    verify_property();
    verify_vim_frame();
    verify_ordinary_frame();
    verify_syntax_frame();
    verify_hidden_partner();
}
} // namespace nenenib::tests
