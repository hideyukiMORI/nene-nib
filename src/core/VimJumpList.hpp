#pragma once

#include "VimMarkPosition.hpp"

#include <array>
#include <cstddef>

namespace nenenib::core
{
// ジャンプの列と変更の列が持てる位置の数（Vim の JUMPLISTSIZE・ADR 0152 の追補 #1037）。
inline constexpr std::size_t vim_jump_list_limit = 100;

// Vim のジャンプの列（`Ctrl-o` `Ctrl-i`）と変更の列（`g;` `g,`）の形（ADR 0152 の追補 #1037）。
// 文書の値（VimMarks の中）で、entries の [0, count) が古い順の位置、index は今の位置の番号
// （count なら列の外＝末尾の後ろ。Vim の w_jumplistidx / w_changelistidx）。固定の大きさの配列に
// するのは、鍵ごとに写す値に確保を持ち込まないため。どの値も単独で妥当な公開 aggregate
// （CPP-003）。比較は非メンバーで、[count, limit) の残りは比べない。
struct VimJumpList
{
    std::array<VimMarkPosition, vim_jump_list_limit> entries{};
    std::size_t count = 0;
    std::size_t index = 0;
};

[[nodiscard]] constexpr bool operator==(const VimJumpList &left, const VimJumpList &right) noexcept
{
    if (left.count != right.count || left.index != right.index)
    {
        return false;
    }
    for (std::size_t at = 0; at < left.count; ++at)
    {
        if (!(left.entries.at(at) == right.entries.at(at)))
        {
            return false;
        }
    }
    return true;
}
} // namespace nenenib::core
