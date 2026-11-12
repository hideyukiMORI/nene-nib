#pragma once

#include <cstdint>

namespace nenenib::core
{
// `:s` の族のパターンと置き換えの出どころ（ADR 0168 の決定 1・Vim の ex_substitute の which_pat）。
enum class ExSubstituteSource : std::uint8_t
{
    // `:s/{pat}/{rep}/`。pat が空なら最後に使ったパターン（RE_LAST）。
    typed,
    // `:s`（パターン無し）・`:&`・NORMAL の `&`。前の置換のパターン（RE_SUBST）と前の置き換え。
    repeated,
    // `:~`。最後に使ったパターン（RE_LAST）と前の置き換え。
    tilde
};
} // namespace nenenib::core
