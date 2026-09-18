#pragma once

#include <cstdint>

namespace nenenib::core
{
// p と P の違い（ADR 0015 の決定 4）。文字単位ならキャレットの文字の後ろか前、
// 行単位なら下の行か上の行。鍵ごとに別の関数を書くと貼り方の規則が 2 か所になる。
enum class VimPutSide : std::uint8_t
{
    after,
    before
};
} // namespace nenenib::core
