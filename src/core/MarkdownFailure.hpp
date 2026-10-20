#pragma once

#include <cstdint>

namespace nenenib::core
{
// parse_markdown の期待される失敗（ADR 0151 の決定 6・ARC-010 / CPP-005）。parser_rejected は
// md4c が -1 を返したとき（記憶の確保の失敗など）、too_large は md4c の大きさの型（unsigned）に
// 入らない本文、stopped は止める述語が true を返して打ち切ったとき。
enum class MarkdownFailure : std::uint8_t
{
    parser_rejected,
    too_large,
    stopped
};
} // namespace nenenib::core
