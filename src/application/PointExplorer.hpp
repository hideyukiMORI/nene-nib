#pragma once

#include "ExplorerPointing.hpp"

#include <cstddef>

namespace nenenib::application
{
// 面の行の上のマウス（ADR 0162 の決定 4）。row は見えている行の列（ExplorerView.rows）の位置で、
// ui が first_visible を足して渡す。押した行と離した行が同じときだけ効く（ADR 0056 の決定 9）。
// 1 回のクリックは選ぶだけでフォルダなら開閉、ダブルクリックは開く（ファイルは開いて本文へ焦点を
// 戻す）。行の外を指したら何もしない（押した印は消す）。
struct PointExplorer
{
    std::size_t row;
    ExplorerPointing pointing;
};
} // namespace nenenib::application
