#pragma once

#include "LineNumber.hpp"
#include "OffsetRange.hpp"

#include <string>

namespace nenenib::core
{
// `:s///c` の当たり 1 つ（ADR 0168 の決定 6・段 2a）。range は問いを始めたときの本文の当たり、utf8
// は 広げた置き換え（改行は '\n'・文書の改行へ直すのは controller の 1 か所）、line
// は当たりの行（同じ 本文の番号）。当たりの列は元の本文から 1 回で計画し（Vim の do_sub
// も行の写しの上で当たりを 進める）、答えの間に読み直さない。値は単独で妥当なので公開
// aggregate（CPP-003）。
struct SubstituteMatch
{
    OffsetRange range;
    std::string utf8;
    LineNumber line;
};
} // namespace nenenib::core
