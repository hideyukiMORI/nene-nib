#pragma once

#include "EditMode.hpp"

#include <cstdint>
#include <utility>

namespace nenenib::core
{
// 割り当ての表の 1 行が効くモード（ADR 0078 の決定 3）。
enum class OperationModes : std::uint8_t
{
    ordinary,
    vim,
    both
};

// その行が今のモードに効くか。
[[nodiscard]] constexpr bool modes_cover(OperationModes modes, EditMode mode) noexcept
{
    switch (modes)
    {
    case OperationModes::ordinary:
        return mode == EditMode::ordinary;
    case OperationModes::vim:
        return mode == EditMode::vim;
    case OperationModes::both:
        return true;
    }
    std::unreachable();
}
} // namespace nenenib::core
