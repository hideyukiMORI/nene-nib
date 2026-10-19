#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
// 描く本文の枠が操作中か（ADR 0148 の決定 8・採用案 docs/design/2026-10-19-split-look.md）。
// active は今までの本文（current_line を塗り、キャレットを accent で描き、IME の候補窓の位置を
// 書く）。passive はもう 1 つの枠で、現在行を塗らず、キャレットは muted の受け身の印だけを描く。
// 値を足したら落ちた switch を直す（CPP-002）。
enum class PaneRole : std::uint8_t
{
    active,
    passive
};
} // namespace nenenib::ui::win32
