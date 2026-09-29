#pragma once

#include "VimCharacter.hpp"
#include "VimSpecialKey.hpp"

#include <string_view>
#include <variant>

namespace nenenib::tests
{
// 記法の名前が指す鍵。多くは特殊鍵で、`<NL>` `<Space>` は文字（ADR 0048 の決定 9・ADR 0049）。
using VimNamedKey = std::variant<nenenib::core::VimCharacter, nenenib::core::VimSpecialKey>;

// fixture の記法（<Esc> <CR> <NL> <BS> <C-r> <Space> 矢印など）の 1 行と鍵の対応。この型だけが
// 手書きで、表そのものは eng/vim-oracle.py の KEY_TABLE から作る生成物 VimKeyNames.hpp である
// （ADR 0054）。
struct VimKeyName
{
    std::string_view text;
    VimNamedKey key;
};
} // namespace nenenib::tests
