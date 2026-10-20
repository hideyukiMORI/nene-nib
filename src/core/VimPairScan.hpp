#pragma once

#include "LineNumber.hpp"

#include <cstddef>
#include <string>

namespace nenenib::core
{
// `%` の走査の途中（Issue #767・Vim の findmatchlimit の局所変数）。行の文字列を持って歩くので、
// 1 文字ごとに本文を引き直さない。index は行の中のバイト位置で、行の内容の終わり（Vim の NUL の
// 桁）も取り得る。quotes は行の `"` が偶数個で引用符の内側を飛ばすか（do_quotes）、inside は走査が
// 越えた `"` の内側か（inquote）、depth は数えた入れ子（count）。どの値も単独で妥当な公開
// aggregate。
struct VimPairScan
{
    LineNumber line;
    std::string content;
    std::size_t index;
    bool quotes;
    bool inside;
    std::size_t depth;
};
} // namespace nenenib::core
