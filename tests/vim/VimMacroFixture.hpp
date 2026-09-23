#pragma once

#include <string_view>

namespace nenenib::tests
{
// fixture の `register`（ADR 0046 の決定 6）。oracle が `let @{name} = "{keys}"` で先に置いた
// マクロのレジスタで、keys は fixture の鍵と同じ記法。テストは同じ鍵を録画して置き直す。
struct VimMacroFixture
{
    char name;
    std::string_view keys;
};
} // namespace nenenib::tests
