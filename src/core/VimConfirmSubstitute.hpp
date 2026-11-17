#pragma once

#include "Offset.hpp"
#include "SubstituteConfirmation.hpp"
#include "VimBlockEdit.hpp"

#include <optional>
#include <vector>

namespace nenenib::core
{
// `:s///c` の 1 歩の効果（ADR 0168 の決定 6・段 2a）。edits はいまの本文に置く当たり（上から・
// 置かない答えなら空・改行は '\n'）、caret は置いた後のキャレット。asking は続けて問う値で、空なら
// 問いを終える。restore は問いを終えるときに置いた分を 1 つの undo の単位へ畳む戻り先（何も
// 置かなければ空・問いを始めたときの本文の位置）。
struct VimConfirmSubstitute
{
    std::vector<VimBlockEdit> edits;
    Offset caret;
    std::optional<SubstituteConfirmation> asking;
    std::optional<Offset> restore;
};
} // namespace nenenib::core
