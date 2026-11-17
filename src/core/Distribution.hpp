#pragma once

#include <cstdint>

namespace nenenib::core
{
// 製品がどの形で入っているか（ADR 0178 の追補 #1809）。packaged は MSIX（Store 版）で package
// identity が在るとき、unpackaged は ZIP 版。OS に問うのは adapters の起動時の 1 回だけで、合成
// ルートが controller に値を渡す。core と application は値を受けるだけ。
enum class Distribution : std::uint8_t
{
    unpackaged,
    packaged
};
} // namespace nenenib::core
