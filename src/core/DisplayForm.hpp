#pragma once

#include <cstddef>
#include <cstdint>

namespace nenenib::core
{
// 文書ごとの表示形式（ADR 0140 の決定 3・試作）。text は今日どおりの文字組み、grid は等幅の升目。
enum class DisplayForm : std::uint8_t
{
    text,
    grid
};

// 巨大な行の定義（決定 1）。表示文字列のコードポイント数がこれを超える行。
inline constexpr std::size_t giant_line_threshold = 16'384;
} // namespace nenenib::core
