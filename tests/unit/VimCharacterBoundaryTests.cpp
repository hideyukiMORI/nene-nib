// scope `--vim-characters` の単体テスト（ADR 0042 決定 2・ADR 0053）。
#include "Offset.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "VimCharacterBoundary.hpp"

#include "../vim/VimFixtures.hpp"

#include <cstddef>
#include <string_view>

namespace nenenib::tests
{
namespace
{
using core::Offset;
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
    expect(selected == 49, "the scope replays all 49 adopted combining-character fixtures");
}
} // namespace

void verify_vim_character_contracts()
{
    verify_vim_character_end();
    verify_vim_character_start();
}

void verify_vim_character_scope()
{
    verify_vim_character_fixtures();
    verify_vim_character_contracts();
}
} // namespace nenenib::tests
