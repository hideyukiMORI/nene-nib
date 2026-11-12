#pragma once

#include "LineNumber.hpp"
#include "VimBlockEdit.hpp"

#include <cstddef>
#include <vector>

namespace nenenib::core
{
// 置換の答え（ADR 0168 の決定 5）。edits は置き換えた行ごとに 1 つ（上の行から・行の最初の当たり
// の始まりから最後の当たりの終わりまでを置き換えた本文で、改行は '\n'・数えるだけなら空）。
// substitutions は置き換えた（数えた）当たりの数、lines は当たった行の数、last_line は最後に
// 当たった行（置換の前の番号）、breaks は置き換えが入れた改行の数（last_line の当たりまでの全部）。
// どの値も単独で妥当なので公開 aggregate（CPP-003）。
struct SubstituteResult
{
    std::vector<VimBlockEdit> edits;
    std::size_t substitutions;
    std::size_t lines;
    LineNumber last_line;
    std::size_t breaks;
};
} // namespace nenenib::core
