#pragma once

#include "LineNumber.hpp"
#include "Offset.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::core
{
// 文字組みの値からengineが決めた画面移動。同じmotion/range/selection経路が消費する。
// 画面行の単位（ADR 0144 の決定 5）では先頭行の画面行first_rowと、キャレットの置き所placed
// （モードの寄せの前）を決め、caretの行とpreserve_columnは使わない。
struct VimDisplayNavigation
{
    LineNumber first;
    LineNumber caret;
    bool preserve_column = false;
    std::optional<std::size_t> scroll_lines = std::nullopt;
    std::size_t first_row = 0;
    std::optional<Offset> placed = std::nullopt;
};
} // namespace nenenib::core
