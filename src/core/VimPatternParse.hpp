#pragma once

#include "VimPatternAtom.hpp"
#include "VimPatternCase.hpp"
#include "VimPatternGroup.hpp"
#include "VimPatternRange.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace nenenib::core
{
// 解析の途中の値（VimPattern.cpp の中だけで使う・ADR 0032 の決定 4・ADR 0168 の決定 4）。atoms と
// ranges は原子と集合の区間、groups は群（閉じる前は end が vim_pattern_group_open）、forced は
// `\c` `\C` の大小、opened は直前の項目が `\(`（その直後の `*` は文字・`^` は錨）。multi は今の
// 項目が量（読み終えると after_multi に移る・量の後ろの量は E871・結合文字は基底の続きにしない）、
// emptied は今の量が `\{0}`（after_emptied に移る・原子を消したので、その後ろの量は E866・Vim の
// nfa_regpiece の早い戻り）、atom_limit は量を開いた後の原子の数の上限（ADR 0184 の決定 2）。
struct VimPatternParse
{
    std::vector<VimPatternAtom> atoms;
    std::vector<VimPatternRange> ranges;
    std::vector<VimPatternGroup> groups;
    std::optional<VimPatternCase> forced;
    std::size_t atom_limit = 0;
    bool opened = false;
    bool multi = false;
    bool emptied = false;
    bool after_multi = false;
    bool after_emptied = false;
};
} // namespace nenenib::core
