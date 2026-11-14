#pragma once

#include <cstdint>

namespace nenenib::core
{
// 原子の列の間に parse が置く構造の印の種類（ADR 0184 の決定 3）。open は群の開き（`\(` と
// `\%(`）、close は群の閉じ（群に付く量を持つ）、branch は選択 `\|`、match_begin と match_end は
// `\zs` と `\ze`（当たりの始まりと終わりを置き直す・ADR 0184 の決定 5）。
enum class VimPatternMarkKind : std::uint8_t
{
    open,
    close,
    branch,
    match_begin,
    match_end
};
} // namespace nenenib::core
