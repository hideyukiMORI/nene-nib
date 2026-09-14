#pragma once

#include "LineNumber.hpp"

#include <cstddef>

namespace nenenib::application
{
// 縦スクロールの状態。application が所有し、上限は core の純関数が決める（ADR 0009 の決定 6）。
// 先頭行と見えている行数は互いに独立して妥当なので公開 aggregate（ADR 0007）。
struct ScrollState
{
    core::LineNumber first_visible;
    std::size_t visible_lines;
};

[[nodiscard]] constexpr bool operator==(const ScrollState &left, const ScrollState &right) noexcept
{
    return left.first_visible == right.first_visible && left.visible_lines == right.visible_lines;
}
} // namespace nenenib::application
