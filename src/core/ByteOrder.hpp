#pragma once

#include <cstdint>

namespace nenenib::core
{
// UTF-16 のバイト列の並び（ADR 0149）。BOM の FF FE が little、FE FF が big。
enum class ByteOrder : std::uint8_t
{
    little,
    big
};
} // namespace nenenib::core
