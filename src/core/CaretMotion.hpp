#pragma once

#include <cstdint>

namespace nenenib::core
{
// キャレットの移動の閉じた一覧（CPP-002）。窓はキーをこの値に写すだけで、位置の計算はしない。
enum class CaretMotion : std::uint8_t
{
    previous_character,
    next_character,
    previous_word,
    next_word,
    previous_line,
    next_line,
    line_start,
    line_end,
    page_up,
    page_down,
    document_start,
    document_end
};
} // namespace nenenib::core
