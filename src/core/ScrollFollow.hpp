#pragma once

namespace nenenib::core
{
enum class ScrollFollow
{
    minimal,
    vim,
    // Vim の INSERT で打った本文の追従（#1363）。Vim の edit は 1 文字ごとに update_topline を
    // 呼ぶので、1 鍵で窓の下へ遠く出たキャレットも 1 行ずつ追う（vim_topline_updated の walks）。
    vim_typed,
};
} // namespace nenenib::core
