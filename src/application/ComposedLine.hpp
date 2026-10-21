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
// 差し込んだ位置の本文の桁（line は差し込む前の描画用の行・#839）。差し込みは本文のこの桁の文字の
// 手前に入る。構文の色の区間をずらす application と、ずらした区間を描画の位置へ戻す renderer が
// 同じ 1 本で引く。
[[nodiscard]] core::Column composed_source_column(const core::DisplayLine &line,
                                                  const ComposedLine &composed) noexcept;
} // namespace nenenib::application
