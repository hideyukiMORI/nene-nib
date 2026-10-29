#pragma once

#include <cstdint>

namespace nenenib::core
{
// 検索が `smartcase` を見るか（ADR 0032 の追補・#1041）。`*` `#` は `ignorecase` だけを見て
// `smartcase` を見ず、その後の `n` `N` と空のパターンの `/` `?` も同じ（Vim の no_scs・実測）。
enum class VimSmartcase : std::uint8_t
{
    honored,
    ignored
};
} // namespace nenenib::core
