#pragma once

#include "ExplorerRowView.hpp"
#include "FilePath.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace nenenib::application
{
// エクスプローラーの面の表示値（ADR 0162 の決定 2・3・8）。面が出ているときだけ frame に載る。
// root は設定のルート（無ければ面に案内の 1 行を描く）。rows は面に見える行だけで、rows[i] は木の
// 行（visible_rows）の first_visible + i 番目（件数は面の行数＝本文の見えている行数を越えない・
// 費用は画面の行数だけ）。total は木の行の全部の数、selected は木の行の位置（rows
// の位置ではない）、 focused は面に焦点があるか、first_visible
// は面の見えている行の先頭（木の行の位置）。 PointExplorer.row も木の行の位置で、ui は
// first_visible を足して渡す。
struct ExplorerView
{
    std::optional<core::FilePath> root;
    std::vector<ExplorerRowView> rows;
    std::size_t total;
    std::optional<std::size_t> selected;
    bool focused;
    std::size_t first_visible;
};
} // namespace nenenib::application
