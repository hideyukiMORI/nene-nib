#pragma once

#include "LineNumber.hpp"

#include <cstddef>
#include <string>

namespace nenenib::core
{
// 語の走査の途中の位置。行の文字列を持って歩くので、1 文字ごとに本文を引き直さない。
// index は行の中のバイト位置で、行の内容の終わり（Vim の NUL の桁）も取り得る。
struct VimScanPoint
{
    LineNumber line;
    std::string content;
    std::size_t index;
};
} // namespace nenenib::core
