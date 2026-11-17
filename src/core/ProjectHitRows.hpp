#pragma once

#include "CommandChoice.hpp"
#include "FilePath.hpp"
#include "ProjectHit.hpp"
#include "ProjectHitRow.hpp"

#include <string>

namespace nenenib::core
{
// 全文検索の当たりの面の行を作る純関数（ADR 0167 の決定 5）。

// file の root からの相対パス。file が root の下（root の文字列に区切り `\` か `/` が続く・root が
// 区切りで終わる）でなければ file の全体。比べるのは文字列だけ（adapter は root の文字列の後ろに
// 名前を足して木を歩く）。
[[nodiscard]] std::string project_relative_path(const FilePath &root, const FilePath &file);

// 当たり 1 つの表示値。
[[nodiscard]] ProjectHitRow project_hit_row(const FilePath &root, const FilePath &file,
                                            const ProjectHit &hit);

// 今の面の 1 行の形（決定 5: `<相対パス>:<行>:<列>  <行の本文>`）の候補。label は
// `<相対パス>:<行>:<列>`、detail は窓（省いた窓は先頭に「…」）で、どちらも DisplayText の 256
// バイトに切る（窓が空白だけでも出す・空なら detail は無し）。確定はファイルのパスの文字列（kind
// open・出どころ project_hit）。
[[nodiscard]] CommandChoice project_hit_choice(const FilePath &file, const ProjectHitRow &row);
} // namespace nenenib::core
