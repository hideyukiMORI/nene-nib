#pragma once

#include <cstdint>

namespace nenenib::core
{
// 範囲が `"1` へ行くかの印（ADR 0050 の決定 4）。by_extent はレジスタの値（行単位か改行を含むか）
// で決め、always はオペレータの後ろの検索の移動（`/ ? n N * #`）の範囲で、1 行の中でも `"1` へ
// 置く（`:help quote_number`）。決めるのは registers_written の 1 か所だけ。
enum class VimNumberedRule : std::uint8_t
{
    by_extent,
    always
};
} // namespace nenenib::core
