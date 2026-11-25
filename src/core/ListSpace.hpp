#pragma once

#include <cstdint>

namespace nenenib::core
{
// 空白の印の出し方（ADR 0193 の決定 4・settings.v7 の鍵 `list_space`・`:set listspace=`）。none は
// 出さない、trail は行末の空白の列だけ（#1011 を取り込んだ既定）、all は全部（行末の分は
// trailing_space の強めの色）。
enum class ListSpace : std::uint8_t
{
    none,
    trail,
    all
};
} // namespace nenenib::core
