#pragma once

#include <cstdint>

namespace nenenib::core
{
// 検索の向き（ADR 0032 の決定 1）。`/` は forward・`?` は backward で、`n` はこの向きのまま、
// `N` は反対の向きで探す。パターンの区切りの文字（offset の始まり）もこの値で決まる。
enum class VimSearchDirection : std::uint8_t
{
    forward,
    backward
};

[[nodiscard]] constexpr VimSearchDirection opposite(VimSearchDirection direction) noexcept
{
    return direction == VimSearchDirection::forward ? VimSearchDirection::backward
                                                    : VimSearchDirection::forward;
}
} // namespace nenenib::core
