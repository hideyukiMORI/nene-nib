#pragma once

#include <cstdint>

namespace nenenib::core
{
// 字句器が 1 行を読んで何を残すか（#815）。spans は lex_line（区間と状態）、state_only は lex_state
// （状態だけ・区間の vector を確保しない）。どちらも同じ状態機械を通る。
enum class LexOutput : std::uint8_t
{
    spans,
    state_only
};
} // namespace nenenib::core
