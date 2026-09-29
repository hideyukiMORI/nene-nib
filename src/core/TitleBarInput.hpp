#pragma once

#include "TitleBarTarget.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace nenenib::core
{
// 帯の配置の入力を 1 つにまとめた値（ADR 0056 の決定 8・引数の上限 4 つのため）。width は帯の幅
// （物理画素）、scroll_dips は送り量（DIP・配置の関数が 0 以上・上限以下に収める）、hovered は
// マウスを載せている要素。application は 96 DPI（width も DIP）で作り、ui は窓の DPI で作る。
struct TitleBarInput
{
    std::int32_t width;
    std::uint32_t dpi;
    std::size_t tab_count;
    std::size_t active;
    std::int32_t scroll_dips;
    std::optional<TitleBarTarget> hovered;
};
} // namespace nenenib::core
