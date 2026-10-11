#pragma once

#include "CaretAffinity.hpp"
#include "CaretShape.hpp"
#include "TextPosition.hpp"

namespace nenenib::application
{
// キャレットの表示値。形は編集モードが決める（採用案 第 1 節）。
struct CaretView
{
    core::TextPosition position;
    core::CaretShape shape;
    core::CaretAffinity affinity = core::CaretAffinity::leading;
};

[[nodiscard]] constexpr bool operator==(const CaretView &left, const CaretView &right) noexcept
{
    return left.position == right.position && left.shape == right.shape &&
           left.affinity == right.affinity;
}
} // namespace nenenib::application
