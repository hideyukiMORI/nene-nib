// scope `--vim-visual-yank` の単体テスト（ADR 0042 決定 2）。
#include "Scopes.hpp"
#include "TestSupport.hpp"

#include "../vim/VimFixtures.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

namespace nenenib::tests
{
void verify_vim_visual_yank_scope()
{
    constexpr std::array<std::string_view, 10> boundaries{
        "yy-does-not-move-the-caret",
        "yw-keeps-the-caret",
        "yb-moves-to-the-range-start",
        "y-dollar-keeps-the-caret",
        "yj-yanks-two-lines",
        "yk-moves-up-to-the-first-line",
        "yk-clamps-the-column-on-a-short-line",
        "v-y-yanks-the-selection",
        "v-j-y-across-lines",
        "V-indented-line-keeps-the-indent-in-the-register"};
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("visual-yank-") ||
            std::ranges::find(boundaries, fixture.name) != boundaries.end())
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 32, "the scope replays 22 visual yanks and 10 shared-path boundaries");
}
} // namespace nenenib::tests
