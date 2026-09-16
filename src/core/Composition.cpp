#include "Composition.hpp"

#include <algorithm>
#include <cstddef>

namespace nenenib::core
{
namespace
{
// 空の範囲は下線にならない。範囲が逆さま（begin > end）の文節もここで落ちる。
void append_underline(std::vector<CompositionClause> &underlines, std::size_t begin,
                      std::size_t end, ClauseEmphasis emphasis)
{
    if (begin >= end)
    {
        return;
    }
    underlines.push_back(CompositionClause{OffsetRange{Offset{begin}, Offset{end}}, emphasis});
}
} // namespace

std::vector<CompositionClause> composition_underlines(const Composition &composition)
{
    const std::size_t size = composition.utf8.size();
    std::vector<CompositionClause> underlines;
    std::size_t covered = 0;
    for (const CompositionClause &clause : composition.clauses)
    {
        const std::size_t begin = std::max(std::min(clause.range.begin.value, size), covered);
        const std::size_t end = std::min(clause.range.end.value, size);
        // 文節が届いていない前の分は「それ以外」で覆う。重なりは先に来た文節が勝つ。
        append_underline(underlines, covered, begin, ClauseEmphasis::other);
        append_underline(underlines, begin, end, clause.emphasis);
        // begin は covered 以上なので、ここまでで隙間なく覆えている。逆さまの文節でも
        // 覆いが後戻りしない＝同じバイトに 2 本の下線が乗らない。
        covered = std::max(begin, end);
    }
    // 文節が 1 つも無い IME でも、変換中の文字列には下線が要る（決定 7）。
    append_underline(underlines, covered, size, ClauseEmphasis::other);
    return underlines;
}
} // namespace nenenib::core
