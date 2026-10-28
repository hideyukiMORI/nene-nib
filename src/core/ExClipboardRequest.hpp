#pragma once

#include <cstdint>

namespace nenenib::core
{
// `:set clipboard=unnamed`（`unnamedplus` と `,` 区切りの組も）・`:set clipboard=`・`:set
// clipboard?` （ADR 0051 の追補（#848）の決定 A）。show は「今の値を知らせる」問いで、今の値は Vim
// の状態を持つ controller が答える。値が増えたら写し先の switch がコンパイルで落ちる（CPP-002）。
enum class ExClipboardRequest : std::uint8_t
{
    enable,
    disable,
    show
};
} // namespace nenenib::core
