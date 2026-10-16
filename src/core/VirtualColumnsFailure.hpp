#pragma once

#include <cstdint>

namespace nenenib::core
{
// 本文が仮想桁の索引を持たない（ADR 0140 の決定 4）。索引は要る文書だけが
// TextBuffer::with_virtual_columns で持ち、持たない本文に問うのは呼び出し側の誤り。
enum class VirtualColumnsFailure : std::uint8_t
{
    not_held
};
} // namespace nenenib::core
