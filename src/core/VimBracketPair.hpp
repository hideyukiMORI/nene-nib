#pragma once

namespace nenenib::core
{
// 対になる括弧の 1 組（ADR 0031 の決定 7）。走査の引数を 1 つにまとめるための値。
struct VimBracketPair
{
    char32_t open;
    char32_t close;
};
} // namespace nenenib::core
