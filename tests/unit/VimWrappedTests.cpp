#include "Editing.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include "../vim/VimFixtures.hpp"
#include "../vim/VimWrapFixtures.hpp"

namespace nenenib::tests
{
void verify_vim_wrapped_contracts()
{
    for (const auto &fixture : vim_wrap_fixtures)
    {
        verify_vim_fixture(fixture);
    }
    Editing editing;
    open_vim_document(editing, "first\nsecond line wraps here\nthird\nfourth\nfifth\nsixth");
    auto &controller = editing.controller();
    static_cast<void>(
        controller.apply(application::VisibleLines{3, core::TextViewGeometry{80, 20, 96}}));
    const auto before = controller.frame().caret.position;
    editing.text_layouts().reject();
    const auto frame = controller.press_vim_key(core::VimKey{core::VimCharacter{U'M'}});
    expect(frame.layout_failure.has_value() && frame.caret.position == before,
           "a failed Vim display query does not apply the key or move the caret");
}

void verify_vim_wrapped_scope()
{
    verify_vim_wrapped_contracts();
    // 同じH/M/Lとページ処理の呼出元。旧fixtureのうちviewportを持つものだけを再利用する。
    for (const auto &fixture : vim_fixtures)
    {
        if (fixture.viewport.has_value())
        {
            verify_vim_fixture(fixture);
        }
    }
}
} // namespace nenenib::tests
