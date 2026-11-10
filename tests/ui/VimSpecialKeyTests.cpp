#include "VimSpecialKey.hpp"
#include "VimSpecialKeys.hpp"
#include "WindowChecks.hpp"

#include <windows.h>

namespace nenenib::tests::ui
{
// Vim モードの仮想キーの表（#1325）。Delete は engine の `<Del>` になり、通常モードの DeleteText
// へは落ちない。Shift+Delete は何も送らない。Backspace と Esc は今のまま、表に無い鍵
// （Insert・文字の鍵）は何も送らない。
void verify_vim_special_keys()
{
    using nenenib::core::VimSpecialKey;
    using nenenib::ui::win32::vim_special_for;
    expect(vim_special_for(VK_DELETE, false) == VimSpecialKey::delete_key,
           "the Delete key is the engine key <Del> in Vim mode");
    expect(vim_special_for(VK_BACK, false) == VimSpecialKey::backspace,
           "the Backspace key stays <BS> in Vim mode");
    expect(vim_special_for(VK_ESCAPE, false) == VimSpecialKey::escape,
           "the Escape key stays <Esc> in Vim mode");
    expect(!vim_special_for(VK_DELETE, true).has_value(),
           "Shift+Delete sends no Vim key (<S-Del> is not <Del>)");
    expect(vim_special_for(VK_LEFT, true) == VimSpecialKey::arrow_left,
           "Shift does not change the other Vim keys");
    expect(!vim_special_for(VK_INSERT, false).has_value(), "the Insert key sends no Vim key");
    expect(!vim_special_for('X', false).has_value(), "a character key is not a special Vim key");
}
} // namespace nenenib::tests::ui
