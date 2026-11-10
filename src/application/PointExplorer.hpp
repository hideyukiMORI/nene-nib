#pragma once

#include "ExplorerPointing.hpp"

#include <cstddef>

namespace nenenib::application
{
// 面の行の上のマウス（ADR 0162 の決定 4）。row は見えている行の列（ExplorerView.rows）の位置で、
// ui が first_visible を足して渡す。押した行と離した行が同じときだけ効く（ADR 0056 の決定 9）。
// 1 回のクリックはフォルダなら開閉、ファイルなら開いて焦点は面に残す。ダブルクリックの 2 打目は
// ファイルなら焦点を本文へ移すだけで、2 回読まず 2 回開閉しない（決定 4 の改め #1388）。直前の
// クリックが効いていれば、2 打目は row を使わずにその節として扱う（行が画面上でずれても取り違え
// ない）。行の外を指したら何もしない（押した印は消す）。
struct PointExplorer
{
    std::size_t row;
    ExplorerPointing pointing;
};
} // namespace nenenib::application
