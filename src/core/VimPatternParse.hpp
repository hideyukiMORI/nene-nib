#pragma once

#include "VimPatternAtom.hpp"
#include "VimPatternCase.hpp"
#include "VimPatternGroup.hpp"
#include "VimPatternMark.hpp"
#include "VimPatternRange.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace nenenib::core
{
// 解析の途中の値（VimPattern.cpp の中だけで使う・ADR 0032 の決定 4・ADR 0168 の決定 4）。
// atoms と ranges は原子と集合の区間、
// groups は番号のある群（閉じる前は end が vim_pattern_group_open）、
// marks は原子の境に立つ構造の印（群の開きと閉じと選択・ADR 0184 の決定 3）、
// opens は閉じていない開きの印の番号（内側が後ろ）、forced は `\c` `\C` の大小。
// opened は直前の項目が `\(` か `\|`（その直後の `*` は文字・`^` は錨）、
// opened_plain は直前の項目が `\%(`（`^` は錨・`*` は E866・
// Vim の peekchr は `\%(` の `(` を Magic にしない）、
// closed は直前の項目が `\)`（その直後の量は群に付く・読み終えると after_close に移る）。
// multi は今の項目が量（読み終えると after_multi に移る・量の後ろの量は E871・
// 結合文字は基底の続きにしない）、emptied は今の量が `\{0}`（after_emptied に移る・
// 原子を消したので、その後ろの量は E866・Vim の nfa_regpiece の早い戻り）、
// cased は直前の項目が `\c` `\C`（その直後の量は付く原子が無いので E866・
// Vim の nfa_regconcat）、atom_limit は量を開いた後の原子の数の上限（ADR 0184 の決定 2）、
// backtracks は Vim 9.1 がパターン全体を後戻りの照合器で照合する量（vim_quantity_backtracks）を
// 読んだか（命令の列の道に乗るなら断る・ADR 0184 の段 2 の強制）。
struct VimPatternParse
{
    std::vector<VimPatternAtom> atoms;
    std::vector<VimPatternRange> ranges;
    std::vector<VimPatternGroup> groups;
    std::vector<VimPatternMark> marks;
    std::vector<std::size_t> opens;
    std::optional<VimPatternCase> forced;
    std::size_t atom_limit = 0;
    bool opened = false;
    bool opened_plain = false;
    bool closed = false;
    bool after_close = false;
    bool multi = false;
    bool emptied = false;
    bool after_multi = false;
    bool after_emptied = false;
    bool cased = false;
    bool backtracks = false;
};
} // namespace nenenib::core
