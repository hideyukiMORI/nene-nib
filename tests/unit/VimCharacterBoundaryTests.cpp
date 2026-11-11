// scope `--vim-characters` の単体テスト（ADR 0042 決定 2・ADR 0053・語の種類の表は ADR 0031 の補足
// #985）。
#include "EditorIntent.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "VimCaret.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimLineColumns.hpp"
#include "VimTestSupport.hpp"
#include "VimWordClass.hpp"
#include "VimWordClassTable.hpp"
#include "VimWordMotion.hpp"

#include "../vim/VimFixtures.hpp"

#include <array>
#include <cstddef>
#include <random>
#include <string>
#include <string_view>
#include <utility>

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

// ---------------------------------------- 窓の終わりで切れた UTF-8（#1292）

// U+20DD（3 バイト・幅 0）を count 個。`a` U+0301 か「漢」（どちらも 3 バイト）の後ろに 21 個以上
// 並べると、行頭から読んだ 64 バイトの窓の 63 番目のバイトに U+20DD の頭が来る。
[[nodiscard]] std::string enclosing_marks(std::size_t count)
{
    std::string marks;
    for (std::size_t index = 0; index < count; ++index)
    {
        marks += "\xE2\x83\x9D";
    }
    return marks;
}

// 寄せ（VimCaret の character_near）は窓の終わりで切れた 3 バイトの頭を decode しない。答えは
// 窓を倍にして読み直した行の答え。
void verify_resting_caret_across_a_window_end()
{
    const auto buffer =
        TextBuffer::from_utf8("a\xCC\x81" + enclosing_marks(25) + std::string(100, 'x'));
    expect(buffer.has_value(), "the marked line is valid UTF-8");
    if (!buffer.has_value())
    {
        return;
    }
    expect(core::vim_resting_caret(buffer.value(), Offset{0}) == Offset{0} &&
               core::vim_resting_caret(buffer.value(), Offset{3}) == Offset{0},
           "a run of 3-byte marks across the window end rests on its base");
}

// 1 升 10 画素・幅 40 升の折り返さない窓で開く。
void open_unwrapped_line(Editing &editing, std::string text)
{
    open_vim_document(editing, std::move(text));
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(nenenib::application::OpenCommandPalette{}));
    static_cast<void>(controller.apply(nenenib::application::CommandText{":set nowrap"}));
    static_cast<void>(controller.apply(nenenib::application::SubmitCommand{}));
    applied(controller, nenenib::application::VisibleLines{
                            3, nenenib::core::TextViewGeometry{400, 20, 96, 10.0F}});
}

// 横送りの押し（VimHorizontalScroll の character_after）と INSERT の <Right>（VimStepInsert）は、
// 窓の終わりで切れた 3 バイトの頭を decode しない。行頭の x の後ろの文字の頭から窓を読む（行頭の
// キャレットの寄せは x だけを歩く）。「漢」の 2 升目が左端に来た `2zl` は次の文字へ、`a<Right>`
// は結合の列の後ろへ着く（後ろの x は「漢」の行で 28 番目・`a` U+0301 の行で 29 番目の code
// point）。
void verify_window_walks_across_a_window_end()
{
    Editing scrolled;
    open_unwrapped_line(scrolled, "x\xE6\xBC\xA2" + enclosing_marks(25) + std::string(200, 'x'));
    vim_normal(scrolled.controller(), "2zl");
    const auto pushed = scrolled.controller().frame();
    expect(pushed.horizontal == 20.0F && pushed.caret.position.column.value == 28,
           "zl past a fullwidth base with a long run of marks pushes the caret to the next "
           "character");
    Editing inserted;
    open_unwrapped_line(inserted, "xa\xCC\x81" + enclosing_marks(25) + std::string(100, 'x'));
    vim_normal(inserted.controller(), "a<Right>");
    expect(inserted.controller().frame().caret.position.column.value == 29,
           "<Right> in INSERT steps over a long run of marks across the window end");
}

// 乱数の本文の材料: 1〜4 バイトの code point・Tab・幅 0（U+0301 は 2 バイト・U+20DD と U+FE0F は
// 3 バイト・異体字セレクタ補助の U+E0100 は 4 バイト・#1322）・絵文字。
constexpr std::array<std::string_view, 10> window_pieces{"a",
                                                         "\t",
                                                         "\xC3\xA9",
                                                         "\xE3\x81\x82",
                                                         "\xE6\xBC\xA2",
                                                         "\xF0\x9F\x98\x80",
                                                         "\xCC\x81",
                                                         "\xE2\x83\x9D",
                                                         "\xEF\xB8\x8F",
                                                         "\xF3\xA0\x84\x80"};
constexpr std::size_t window_plain_pieces = 6;

// 固定 seed の 1 行（改行を含まない）。幅 0 の材料は 1〜40 個の列で入れ、窓（64 バイト）の境を
// またがせる。
[[nodiscard]] std::string window_line(std::mt19937_64 &random)
{
    std::string line;
    const std::size_t target = 40 + static_cast<std::size_t>(random() % 400U);
    while (line.size() < target)
    {
        const auto piece = static_cast<std::size_t>(random() % window_pieces.size());
        const std::size_t repeat =
            piece < window_plain_pieces ? 1 : 1 + static_cast<std::size_t>(random() % 40U);
        for (std::size_t index = 0; index < repeat; ++index)
        {
            line += window_pieces.at(piece);
        }
    }
    return line;
}

// 行の写しの上の素朴な答え: byte を含む Vim の 1 文字の先頭（行の終わりなら最後の文字の先頭）。
[[nodiscard]] std::size_t copied_character_containing(std::string_view line, std::size_t byte)
{
    std::size_t start = 0;
    std::size_t at = 0;
    while (at < line.size())
    {
        const std::size_t end = vim_character_end(line, Offset{at}).value;
        if (byte < end)
        {
            return at;
        }
        start = at;
        at = end;
    }
    return start;
}

// 行の写しの上で start から Vim の 1 文字ずつ count 回進んだ終わりと、count 回戻った頭。
[[nodiscard]] std::pair<std::size_t, std::size_t> copied_steps(std::string_view line,
                                                               std::size_t start, std::size_t count)
{
    std::size_t forward = start;
    std::size_t back = start;
    for (std::size_t step = 0; step < count; ++step)
    {
        forward = forward < line.size() ? vim_character_end(line, Offset{forward}).value : forward;
        back = back > 0 ? vim_character_start(line, Offset{back}).value : back;
    }
    return {forward, back};
}

// 索引を持つ本文の VimLineColumns の character_end と character_start（窓の道）が、start の
// 文字の頭から count 回の歩きで行の写しと同じ答えを返すか。
[[nodiscard]] bool columns_agree(const core::VimLineColumns &columns, std::string_view line,
                                 std::size_t start)
{
    bool same = true;
    for (const std::size_t count : {std::size_t{1}, std::size_t{3}, std::size_t{50}})
    {
        const auto [forward, back] = copied_steps(line, start, count);
        same = same && columns.character_end(start, count) == forward &&
               columns.character_start(start, count) == back;
    }
    return same;
}

// 窓の道の 5 つの答え（vim_character_after・vim_character_before・vim_resting_caret・
// VimLineColumns の character_end と character_start）を、行の全体を写した素朴な答えと比べる。
// compared は比べた数。
[[nodiscard]] bool window_walks_agree(const std::string &line, std::size_t &compared)
{
    const auto buffer = TextBuffer::from_utf8(line + "\nb");
    if (!buffer.has_value())
    {
        return false;
    }
    const TextBuffer &text = buffer.value();
    const TextBuffer indexed = text.with_virtual_columns();
    const core::VimLineColumns columns{indexed, LineNumber{1}};
    const Offset line_end{line.size()};
    bool same = true;
    for (std::size_t at = 0; at < line.size(); at = core::next_code_point(line, Offset{at}).value)
    {
        const std::size_t start = copied_character_containing(line, at);
        same = same &&
               core::vim_character_after(text, Offset{at}, line_end) ==
                   vim_character_end(line, Offset{at}) &&
               core::vim_resting_caret(text, Offset{at}).value == start &&
               columns_agree(columns, line, start);
        compared += 8;
        if (start > 0)
        {
            same = same && core::vim_character_before(text, Offset{start}, Offset{0}) ==
                               vim_character_start(line, Offset{start});
            ++compared;
        }
    }
    same = same && core::vim_resting_caret(text, line_end).value ==
                       copied_character_containing(line, line.size());
    return same;
}

// 性質: 乱数の本文（固定 seed）で、窓だけを読む 1 文字の道は行を写した答えと同じ（#1292）。
void verify_window_walks_match_the_copied_line()
{
    std::mt19937_64 random{0x1292U};
    std::size_t compared = 0;
    bool same = true;
    for (std::size_t round = 0; round < 60; ++round)
    {
        same = window_walks_agree(window_line(random), compared) && same;
    }
    expect(same, "window walks over random UTF-8 lines answer as the copied line does");
    expect(compared > 10'000, "the window property compares more than 10000 answers");
}

// 行の内容の終わりより後ろ（CRLF の CR と LF の間）の at は、1 文字の終わりも code point も行の
// 外を読まない（#1424・語・引用符・括弧のテキストオブジェクトの next_in_line の守り）。
void verify_window_walks_past_the_line_end()
{
    const auto buffer = TextBuffer::from_utf8("ab\r\ncd\xF0\x9F\x98\x80");
    expect(buffer.has_value(), "the CRLF text is valid UTF-8");
    const TextBuffer &text = buffer.value();
    const Offset line_end = text.line_end(LineNumber{1});
    expect(line_end == Offset{2}, "the CRLF line content ends before the CR");
    expect(core::vim_character_after(text, Offset{3}, line_end) == Offset{3} &&
               core::vim_code_point_at(text, Offset{3}, line_end) == 0,
           "a position between CR and LF stays put and reads as the line end");
    const Offset last_end = text.line_end(LineNumber{2});
    expect(core::vim_code_point_at(text, Offset{0}, line_end) == U'a' &&
               core::vim_code_point_at(text, Offset{6}, last_end) == U'\U0001F600' &&
               core::vim_code_point_at(text, last_end, last_end) == 0,
           "the code point reads one character up to the line end and NUL at the end");
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
    verify_resting_caret_across_a_window_end();
    verify_window_walks_across_a_window_end();
    verify_window_walks_match_the_copied_line();
    verify_window_walks_past_the_line_end();
    verify_vim_word_class_table();
    verify_vim_external_caret();
    verify_vim_window_path();
}

void verify_vim_character_scope()
{
    verify_vim_character_fixtures();
    verify_vim_character_contracts();
}
} // namespace nenenib::tests
