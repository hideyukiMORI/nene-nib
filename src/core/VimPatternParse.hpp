#pragma once

#include "VimPatternAtom.hpp"
#include "VimPatternCase.hpp"
#include "VimPatternGroup.hpp"
#include "VimPatternRange.hpp"

#include <optional>
#include <vector>

namespace nenenib::core
{
// 解析の途中の値（VimPattern.cpp の中だけで使う・ADR 0032 の決定 4・ADR 0168 の決定 4）。atoms と
// ranges は原子と集合の区間、groups は群（閉じる前は end が vim_pattern_group_open）、forced は
// `\c` `\C` の大小、opened は直前の項目が `\(`（その直後の `*` は文字・`^` は錨）。
struct VimPatternParse
{
    std::vector<VimPatternAtom> atoms;
    std::vector<VimPatternRange> ranges;
    std::vector<VimPatternGroup> groups;
    std::optional<VimPatternCase> forced;
    bool opened = false;
};
} // namespace nenenib::core
