#pragma once

#include "GridCellSpan.hpp"
#include "SyntaxRole.hpp"

namespace nenenib::application
{
// 格子の 1 画面行の構文の色の升の範囲（#839・ADR 0156 の追補）。画面行の中では昇順で重ならず、
// 区間の無い升は本文の text の色。変換中の文字列を差し込んだ升には掛からない。
struct GridColorSpan
{
    GridCellSpan cells;
    core::SyntaxRole role;
};

[[nodiscard]] constexpr bool operator==(GridColorSpan left, GridColorSpan right) noexcept
{
    return left.cells == right.cells && left.role == right.role;
}
} // namespace nenenib::application
