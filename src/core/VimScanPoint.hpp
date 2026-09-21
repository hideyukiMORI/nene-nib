#pragma once

#include "LineNumber.hpp"
#include "VimWordClass.hpp"

#include <cstddef>
#include <string>

namespace nenenib::core
{
// 語の走査の途中の位置。行の文字列を持って歩くので、1 文字ごとに本文を引き直さない。
// index は行の中のバイト位置で、行の内容の終わり（Vim の NUL の桁）も取り得る。
// kind は走査中ずっと変わらない語の切れ方で、値に持たせると引数が増えない（ADR 0031）。
struct VimScanPoint
{
    LineNumber line;
    std::string content;
    std::size_t index;
    VimWordClass kind;
};
} // namespace nenenib::core
