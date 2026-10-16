#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
// 16 MiB・200,000 行の文書を前向きに 1 回だけ検索する（#583）。miss は無い語で 1 周、next は
// 末尾の行の 1 か所へ先頭から `n`。
enum class SearchSpanWorkload : std::uint8_t
{
    miss_16mib,
    next_16mib
};
} // namespace nenenib::tests::performance
