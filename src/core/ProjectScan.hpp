#pragma once

#include "LineNumber.hpp"

#include <string>

namespace nenenib::core
{
// 1 ファイルを塊に切って照合する途中の状態（ADR 0167 の決定 3）。値で受け渡し、project_hits_in が
// 次の値を返す。ファイルの最初の塊には既定の値を渡す。
struct ProjectScan
{
    // 次の塊の先頭のバイトが属する行（1 始まり）。
    LineNumber line{1};
    // 前の塊の終わりで切れた行の始まり（最大 1 行・project_search_line_limit バイトまで）。
    std::string carry;
    // 前の塊で切れた行が project_search_line_limit を超えたので、次の `\n` まで読み飛ばす途中か。
    bool skipping = false;
};
} // namespace nenenib::core
