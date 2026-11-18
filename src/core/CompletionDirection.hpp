#pragma once

#include <cstdint>

namespace nenenib::core
{
// 候補を集める向き（ADR 0169 の決定 2）。forward は Ctrl-n（キャレットの後ろから前方へ・折り返して
// 前へ）、backward は Ctrl-p（キャレットの前から手前へ・折り返して後ろから）。Vim は向きごとに
// 走った順で重複を落とすので、backward の列は forward の列の逆とは限らない（oracle で実測）。
enum class CompletionDirection : std::uint8_t
{
    forward,
    backward
};
} // namespace nenenib::core
