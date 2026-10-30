#pragma once

#include <cstdint>

namespace nenenib::application
{
// 印刷の期待される失敗（ADR 0177 の決定 2・7・ARC-010）。controller はどれも 1 行で知らせる。
// too_large は本文が core::print_byte_limit を超えるときで、controller は port を呼ぶ前に自分で
// 決める（実装が同じ値を返してもよい）。
enum class PrintFailure : std::uint8_t
{
    no_printer,
    dialog_failed,
    rendering_failed,
    too_large
};
} // namespace nenenib::application
