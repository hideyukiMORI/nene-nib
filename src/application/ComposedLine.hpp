#pragma once

#include "Column.hpp"
#include "Composition.hpp"
#include "DisplayLine.hpp"
#include "OffsetRange.hpp"

#include <cstddef>
#include <string>

namespace nenenib::application
{
// 本文の外にあるIMEを差し込んだ表示文字列。描画と位置解決がこの同じ値を使う。
struct ComposedLine
{
    std::string text;
    core::OffsetRange insertion;
    core::Column insertion_column;
    std::size_t inserted_columns;
    core::Column cursor;
};

[[nodiscard]] ComposedLine composed_line(const core::DisplayLine &line, core::Column at,
                                         const core::Composition &composition);
[[nodiscard]] core::Column uncomposed_column(const ComposedLine &line, core::Column shown) noexcept;
} // namespace nenenib::application
