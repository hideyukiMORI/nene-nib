#pragma once

#include <cstdint>

namespace nenenib::core
{
// テキストオブジェクトの取り方（ADR 0031 の決定 1）。i は内側だけ、a は周り（空白・括弧）も含む。
enum class VimTextObjectScope : std::uint8_t
{
    inner,
    around
};
} // namespace nenenib::core
