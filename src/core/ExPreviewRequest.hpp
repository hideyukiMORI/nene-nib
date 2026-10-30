#pragma once

#include <cstdint>

namespace nenenib::core
{
// `:preview full` `:preview side` `:preview off` と `:preview?`・引数の無い `:preview`（ADR 0155 の
// 改訂 16・#1052）。full / side / off は見せ方を選ぶ要求で、show は今の見せ方を知らせる問い。
// 見せ方は窓の値なので、置くのも答えるのも controller。値が増えたら写し先の switch が
// コンパイルで落ちる（CPP-002）。
enum class ExPreviewRequest : std::uint8_t
{
    full,
    side,
    off,
    show
};
} // namespace nenenib::core
