#pragma once

#include "TitleBarHit.hpp"

#include <cstddef>

namespace nenenib::core
{
// 帯の上で当たった要素と、それがタブのときの帯の位置（ADR 0056 の決定 9）。tab は hit が
// tab と tab_close のときだけ意味を持ち、ほかの要素では 0。2 つの欄は互いに独立して妥当なので
// 公開 aggregate（ADR 0007）。比較は非メンバー（CPP-003）。
struct TitleBarTarget
{
    TitleBarHit hit;
    std::size_t tab;
};

[[nodiscard]] constexpr bool operator==(const TitleBarTarget &left,
                                        const TitleBarTarget &right) noexcept
{
    return left.hit == right.hit && left.tab == right.tab;
}
} // namespace nenenib::core
