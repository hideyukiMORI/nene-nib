// scope `--vim-characters` の単体テスト（ADR 0042 決定 2・ADR 0053）。
#include "GiantLine.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "VimCaret.hpp"
#include "VimCharacterBoundary.hpp"

#include "../vim/VimFixtures.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
using core::giant_line_window;
using core::LineNumber;
using core::Offset;
using core::OffsetRange;
using core::TextBuffer;
using core::vim_character_end;
using core::vim_character_start;

// 1 文字は code point 1 つと直後に続く幅 0 の code point の列（ADR 0053 の決定 1）。
void verify_vim_character_end()
{
    constexpr std::string_view marks = "aẹ́b";
    expect(vim_character_end(marks, Offset{0}) == Offset{1}, "a plain character is one code point");
    expect(vim_character_end(marks, Offset{1}) == Offset{6},
           "every zero-width code point after the base belongs to the same character");
    expect(vim_character_end(marks, Offset{7}) == Offset{7}, "the end of the line does not move");

    constexpr std::string_view lone = "́́a";
    expect(vim_character_end(lone, Offset{0}) == Offset{4},
           "a mark at the line start is a character of its own and takes the next mark");

    constexpr std::string_view flag = "\U0001F1EF\U0001F1F5";
    expect(vim_character_end(flag, Offset{0}) == Offset{4},
           "regional indicators stay separate characters as in Vim");
}

void verify_vim_character_start()
{
    constexpr std::string_view marks = "aẹ́b";
    expect(vim_character_start(marks, Offset{6}) == Offset{1},
           "the character before a boundary starts at its base");
    expect(vim_character_start(marks, Offset{3}) == Offset{1} &&
               vim_character_start(marks, Offset{5}) == Offset{1},
           "a position on a mark belongs to the character the mark sits on");
    expect(vim_character_start(marks, Offset{1}) == Offset{0}, "a plain character steps back one");
    expect(vim_character_start(marks, Offset{0}) == Offset{0}, "the line start does not move");

    constexpr std::string_view lone = "́́a";
    expect(vim_character_start(lone, Offset{4}) == Offset{0} &&
               vim_character_start(lone, Offset{2}) == Offset{0},
           "a mark at the line start is itself the start of a character");
}

// 1 MiB の 1 行: 途中と行末に、窓の片側 64 バイトより長い結合文字の列（100 個・200 バイト）。
constexpr std::size_t long_half = std::size_t{1} << 19U;
constexpr std::size_t long_marks = 200;
constexpr std::size_t long_middle = long_half;
constexpr std::size_t long_last = long_half + 1 + long_marks + long_half;
constexpr std::size_t long_size = long_last + 1 + long_marks;

[[nodiscard]] std::string long_line()
{
    std::string marks;
    for (std::size_t index = 0; index < long_marks / 2; ++index)
    {
        marks += "́";
    }
    return std::string(long_half, 'a') + "e" + marks + std::string(long_half, 'a') + "x" + marks +
           "\nb";
}

// 1 MiB の 1 行でも寄せは窓だけを読んで行の全体と同じ答えを出す（ADR 0053 の決定 5）。窓を倍に
// 広げる枝も通す。
void verify_resting_caret_on_a_long_line()
{
    const auto buffer = TextBuffer::from_utf8(long_line());
    expect(buffer.has_value(), "the long line is valid UTF-8");
    if (!buffer.has_value())
    {
        return;
    }
    const TextBuffer &text = buffer.value();
    expect(core::vim_resting_caret(text, Offset{long_middle - 1}) == Offset{long_middle - 1},
           "a caret on a plain character in a long line stays");
    expect(core::vim_resting_caret(text, Offset{long_middle}) == Offset{long_middle},
           "a caret on a base with a long run of marks stays");
    expect(core::vim_resting_caret(text, Offset{long_middle + 1 + 150}) == Offset{long_middle},
           "a caret deep in a long run of marks goes back to the base");
    expect(core::vim_resting_caret(text, Offset{long_middle + 1 + 151}) == Offset{long_middle},
           "a caret inside a mark's bytes goes back to the base");
    expect(core::vim_resting_caret(text, Offset{long_size}) == Offset{long_last},
           "a caret past the line end rests on the last character's base");
    expect(core::vim_line_and_column(text, LineNumber{1}, long_middle + 1 + 180) ==
               Offset{long_middle},
           "a column in a long run of marks maps to the base");
    expect(core::vim_line_and_column(text, LineNumber{1}, long_size + 5) == Offset{long_size},
           "a column past the line maps to the line content end");
}

// ADR 0139（試作）: 巨大な行の窓は錨の前後のコードポイント数で切り、端を Vim の 1 文字に寄せる。
void verify_giant_line_window()
{
    constexpr std::string_view plain = "abcdefghij";
    expect(giant_line_window(plain, Offset{0}, 3) == OffsetRange{Offset{0}, Offset{3}},
           "an anchor at the line start takes only the characters after it");
    expect(giant_line_window(plain, Offset{10}, 3) == OffsetRange{Offset{7}, Offset{10}},
           "an anchor at the line end takes only the characters before it");
    expect(giant_line_window(plain, Offset{5}, 2) == OffsetRange{Offset{3}, Offset{7}},
           "an anchor in the middle takes the same count on both sides");

    // "a" + e + U+0323 + U+0301 + "b": 前後 1 コードポイントで切ると結合文字の途中になる。
    constexpr std::string_view marks = "xa"
                                       "e\u0323\u0301"
                                       "by";
    expect(giant_line_window(marks, Offset{5}, 1) == OffsetRange{Offset{2}, Offset{7}},
           "both ends move outward to the boundary of the Vim character");

    // 切り出しの両端がコードポイントの途中（"あいう" の 1 byte 目と最後の 1 byte を欠く）。
    constexpr std::string_view slice = "\x81\x82\xe3\x81\x84\xe3\x81";
    expect(giant_line_window(slice, Offset{2}, 8) == OffsetRange{Offset{2}, Offset{5}},
           "partial sequences at the slice edges stay outside the window");
}

void verify_vim_character_fixtures()
{
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("combining-"))
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 92, "the scope replays all 92 adopted combining-character fixtures");
}
} // namespace

void verify_vim_character_contracts()
{
    verify_vim_character_end();
    verify_vim_character_start();
    verify_resting_caret_on_a_long_line();
    verify_giant_line_window();
}

void verify_vim_character_scope()
{
    verify_vim_character_fixtures();
    verify_vim_character_contracts();
}
} // namespace nenenib::tests
