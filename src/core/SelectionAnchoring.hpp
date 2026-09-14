#pragma once

#include <cstdint>

namespace nenenib::core
{
// 移動が選択をどう扱うか。Shift+移動が anchor を固定する（ADR 0009 の決定 4）。
enum class SelectionAnchoring : std::uint8_t
{
    collapse,
    extend
};
} // namespace nenenib::core
