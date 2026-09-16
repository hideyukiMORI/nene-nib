#pragma once

#include <cstdint>

namespace nenenib::core
{
// 保留中のオペレータの閉じた一覧（ADR 0012 の決定 1）。この縦切りは d だけで、c y > < は次。
enum class VimOperator : std::uint8_t
{
    remove
};
} // namespace nenenib::core
