// scope `--vim-characters` の単体テスト（ADR 0042 決定 2・ADR 0053・語の種類の表は ADR 0031 の補足
// #985）。
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "VimCaret.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimWordClass.hpp"
#include "VimWordClassTable.hpp"
#include "VimWordMotion.hpp"

#include "../vim/VimFixtures.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
using core::LineNumber;
using core::Offset;
using core::TextBuffer;
using core::vim_character_class;
using core::vim_character_end;
using core::vim_character_start;
using core::VimWordClass;

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

// 語の種類の表は Vim 9.1.0004 の src/mbyte.c（SHA-256
// 5d56d30e6a5cab9b8dc1460d5995a8a53077924f26c056091825c1bb600f4a8b）の classes[] 71 区間と
// emoji_all[] 146 区間の写し（Issue #985・ADR 0031 の補足）。値は oracle の Vim の charclass()。
void verify_vim_word_class_table()
{
    expect(core::vim_utf_classes.size() == 71, "the table copies all 71 intervals of classes[]");
    expect(core::vim_emoji_all.size() == 146, "the table copies all 146 intervals of emoji_all[]");
    bool sorted = true;
    for (std::size_t at = 1; at < core::vim_emoji_all.size(); ++at)
    {
        sorted = sorted && core::vim_emoji_all.at(at - 1).last < core::vim_emoji_all.at(at).first;
    }
    expect(sorted, "emoji_all[] stays sorted and apart for the binary search");
    const auto word = [](char32_t code) { return vim_character_class(code, VimWordClass::word); };
    expect(word(0x0085) == 1 && word(0x00A0) == 0 && word(0x00B5) == 2 && word(0x00D7) == 2,
           "below 0x100 the default iskeyword decides");
    expect(word(0x2190) == 1 && word(0x200D) == 1 && word(0x2000) == 0,
           "arrows and the joiner are punctuation, the general spaces are blanks");
    expect(word(0x2074) == 1 && word(0x2081) == 1,
           "the overlapping 0x2060-0x27ff row wins over superscripts as in Vim's search");
    expect(word(0x26A0) == 3 && word(0x1F468) == 3 && word(0x1F600) == 3, "emoji are class 3");
    expect(word(0x3042) == 0x3040 && word(0x30AB) == 0x30A0 && word(0x6F22) == 0x4E00 &&
               word(0xD55C) == 0xAC00 && word(0x2801) == 0x2800,
           "Hiragana, Katakana, CJK, Hangul and Braille keep their own classes");
    expect(word(0x0964) == 1 && word(0x1D400) == 1 && word(0x03B1) == 2 && word(0xFF10) == 2,
           "the danda and math letters are punctuation, Greek and fullwidth digits are words");
    expect(vim_character_class(0x26A0, VimWordClass::big_word) == 2,
           "a WORD folds every non-blank class into one");
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
    std::size_t classes = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("word-class-"))
        {
            verify_vim_fixture(fixture);
            ++classes;
        }
    }
    expect(classes == 39, "the scope replays all 39 word-class fixtures (Issue #985)");
}
} // namespace

void verify_vim_character_contracts()
{
    verify_vim_character_end();
    verify_vim_character_start();
    verify_resting_caret_on_a_long_line();
    verify_vim_word_class_table();
}

void verify_vim_character_scope()
{
    verify_vim_character_fixtures();
    verify_vim_character_contracts();
}
} // namespace nenenib::tests
