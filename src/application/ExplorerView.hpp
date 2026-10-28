#pragma once

#include "ExplorerRowView.hpp"
#include "FilePath.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace nenenib::application
{
// エクスプローラーの面の表示値（ADR 0162 の決定 2・3・8）。面が出ているときだけ frame に載る。
// root は設定のルート（無ければ面に案内の 1 行を描く）、rows は木の見えている行の全部
// （visible_rows の写し・費用は行数だけ）、selected は rows の位置、focused は面に焦点があるか、
// first_visible は面の見えている行の先頭（rows の位置）。
struct ExplorerView
{
    std::optional<core::FilePath> root;
    std::vector<ExplorerRowView> rows;
    std::optional<std::size_t> selected;
    bool focused;
    std::size_t first_visible;
};
} // namespace nenenib::application
