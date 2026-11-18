#pragma once

#include <cstdint>

namespace nenenib::core
{
// 速さへの影響の段（ADR 0170 の決定 2）。基準の 1 打鍵に対して +1% 未満 / 1〜5% / 5〜20% /
// 20% 超。裏で CPU・メモリ・子プロセスを使う機能は high。宣言の順が軽い順。
enum class CostTier : std::uint8_t
{
    none,
    low,
    medium,
    high
};
} // namespace nenenib::core
