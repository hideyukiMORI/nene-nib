#pragma once

#include <cstdint>

namespace nenenib::core
{
// 行の終わりの改行の形の印（ADR 0193 の決定 1・10・#1977）。行ごとに本文の終わりの 1〜2 バイトで
// 決める（混在の文書でも行ごとに正しい）。lf は `↵`、crlf は `↵` の前に小さな `←`。改行の無い最後の
// 行・Vim のモードで隠す最後の改行・lone CR（`^M` の置き換えで見えている）は印なし。
enum class LineBreakMark : std::uint8_t
{
    lf,
    crlf
};
} // namespace nenenib::core
