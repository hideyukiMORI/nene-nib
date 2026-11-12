#pragma once

#include "SubstituteReplacement.hpp"
#include "TextBuffer.hpp"
#include "VimPattern.hpp"

namespace nenenib::core
{
// 置換の 1 回の走り（SubstituteAll.cpp の中だけで使う・ADR 0168 の決定
// 5）。本文・パターン・置き換え を借り、every は行の当たりを全部置き換えるか、groups
// は当たりごとに群の範囲を引くか。借りた値より 長く持たない。
struct SubstituteScan
{
    const TextBuffer &text;
    const VimPattern &pattern;
    const SubstituteReplacement &replacement;
    bool every;
    bool groups;
    // 群の範囲を引く道の升の数の上限（SubstituteRequest.capture_limit）。
    std::size_t capture_limit;
};
} // namespace nenenib::core
