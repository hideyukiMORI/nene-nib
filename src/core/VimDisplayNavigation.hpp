#pragma once

#include "LineNumber.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::core
{
// 文字組みの値からengineが決めた画面移動。同じmotion/range/selection経路が消費する。
struct VimDisplayNavigation
{
    LineNumber first;
    LineNumber caret;
    bool preserve_column = false;
    std::optional<std::size_t> scroll_lines = std::nullopt;
};
} // namespace nenenib::core
