#include "DisplayQuestion.hpp"

#include "GiantLine.hpp"
#include "LineNumber.hpp"
#include "Utf8.hpp"

#include <cstddef>

namespace nenenib::application
{
// コードポイントの数はバイトの数を超えないので、バイトでしきい値を超えない行は読まない。
// 行の長さは「次の行の先頭 − この行の先頭」（改行を含む）で、改行の索引を 1 行に 1 回だけ引く。
// それがしきい値を超える行だけ、行の内容（改行を除く）を読んで数える（読む量は高々しきい値を
// バイトで超える行の和）。本文全体がしきい値以下なら行を見ない。
bool asks_grid(const core::TextBuffer &text)
{
    if (text.size_bytes() <= core::giant_line_threshold)
    {
        return false;
    }
    core::Offset begin{0};
    for (std::size_t line = 1; line <= text.line_count(); ++line)
    {
        const core::LineNumber number{line};
        const auto next = text.line_terminator_end(number);
        if (next.value - begin.value > core::giant_line_threshold &&
            core::code_point_count(text.text_range(begin, text.line_end(number))) >
                core::giant_line_threshold)
        {
            return true;
        }
        begin = next;
    }
    return false;
}
} // namespace nenenib::application
