#include "DisplayQuestion.hpp"

#include "GiantLine.hpp"
#include "LineNumber.hpp"
#include "Utf8.hpp"

#include <cstddef>
#include <utility>

namespace nenenib::application
{
bool asks_grid(const core::TextBuffer &text)
{
    return asks_grid_read(text).first;
}

// コードポイントの数はバイトの数を超えないので、バイトでしきい値を超えない行は読まない。
// 最長の行のバイト数（改行の索引の塊の要約・O(塊)）がしきい値以下なら行を 1 つも引かない
// （Issue #553）。超えるときだけ、行の長さ「次の行の先頭 − この行の先頭」（改行を含む）を改行の
// 索引から 1 行に 1 回引き、それがしきい値を超える行だけ行の内容（改行を除く）を読んで数える。
std::pair<bool, std::size_t> asks_grid_read(const core::TextBuffer &text)
{
    if (text.longest_line_bytes() <= core::giant_line_threshold)
    {
        return {false, 0};
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
            return {true, line};
        }
        begin = next;
    }
    return {false, text.line_count()};
}
} // namespace nenenib::application
