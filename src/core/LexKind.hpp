#pragma once

#include <cstdint>

namespace nenenib::core
{
// 行をまたぐ字句の状態の種類（#815・ADR 0156 の決定 3）。言語ごとに値を増やさない。
enum class LexKind : std::uint8_t
{
    plain,
    block_comment,
    raw_string
};
} // namespace nenenib::core
