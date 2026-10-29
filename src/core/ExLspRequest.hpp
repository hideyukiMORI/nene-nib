#pragma once

#include <cstdint>

namespace nenenib::core
{
// `:set lsp`・`:set nolsp`・`:set lsp?`（ADR 0160 の決定 7・ADR 0051 の追補の clipboard と同じ
// 要求の形）。enable と disable は設定 v4 の `lsp` を書き換えて保存し、言語サーバーを起こす・
// 止めるのは controller。enable は止まった後の手での再開も兼ねる（決定 6）。show は今の値を
// 知らせる問いで、今の値は設定を持つ controller が答える。値が増えたら写し先の switch が
// コンパイルで落ちる（CPP-002）。
enum class ExLspRequest : std::uint8_t
{
    enable,
    disable,
    show
};
} // namespace nenenib::core
