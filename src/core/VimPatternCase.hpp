#pragma once

#include <cstdint>

namespace nenenib::core
{
// 検索の大小の旗（ADR 0032 の追補・#1041）。照合器 VimPattern::parse の引数 1 つで、旗を
// パターンの文字列に埋め込まない。sensitive は区別する（`noignorecase`・Vim の既定）、ignore は
// 区別しない（`ignorecase`）、smart はパターンの literal の文字に大文字があれば区別し、無ければ
// 区別しない（`ignorecase` と `smartcase` の両方）。パターンの中の `\c` `\C` はこの旗より強い。
enum class VimPatternCase : std::uint8_t
{
    sensitive,
    ignore,
    smart
};
} // namespace nenenib::core
