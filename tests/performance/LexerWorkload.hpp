#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
// C++ の行の字句器 lex_line（#815・段 2 の ADR 0156）。1 行 100 バイトの C++ の 6
// 種の行を繰り返す。 visible_60 は 1 枠ぶんの 60 行を区間ごと読み、backfill_200
// は見えている行の前の 200 行を 状態だけを次へ渡して読む（区間は捨てる・段 2 の遡りの形）。
enum class LexerWorkload : std::uint8_t
{
    cpp_visible_60,
    cpp_backfill_200
};
} // namespace nenenib::tests::performance
