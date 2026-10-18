#pragma once

#include "VimLineRows.hpp"
#include "VimRowSpan.hpp"
#include "VimScrollUnit.hpp"

#include <cstddef>
#include <span>

namespace nenenib::core
{
// 未測定の行はVimStep.layout_requestで求める。無効時は従来の1行1画面行。
// 画面行の範囲はVimStep.row_requestで求め、答えをspansに積む（ADR 0144 の決定 2）。unitは
// 画面移動の単位で、controllerが格子のときだけrowにする（決定 5）。row_columnsは1画面行の升の
// 数（格子はGridCellsのcolumns・text形は本文の欄の幅 ÷ 平均の字幅・無効なら0）で、engineは
// gj gkの最初に欲しい列を「行の画面行数 × row_columns − 1」に切る（決定 10）。0なら切らない。
struct VimLayoutView
{
    bool enabled = false;
    std::span<const VimLineRows> lines{};
    VimScrollUnit unit = VimScrollUnit::line;
    std::span<const VimRowSpan> spans{};
    std::size_t row_columns = 0;
};
} // namespace nenenib::core
