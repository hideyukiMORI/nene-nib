#include "DisplayQuestion.hpp"

#include "GiantLine.hpp"
#include "LineNumber.hpp"
#include "Utf8.hpp"

#include <cstddef>

namespace nenenib::application
{
// コードポイントの数はバイトの数を超えないので、バイトでしきい値を超えない行は読まない。
// 超える行だけ行の内容を読んで数える（読む量は高々しきい値をバイトで超える行の和）。
bool asks_grid(const core::TextBuffer &text)
{
    for (std::size_t line = 1; line <= text.line_count(); ++line)
    {
        const core::LineNumber number{line};
        const auto begin = text.line_start(number);
        const auto end = text.line_end(number);
        if (end.value - begin.value <= core::giant_line_threshold)
        {
            continue;
        }
        if (core::code_point_count(text.text_range(begin, end)) > core::giant_line_threshold)
        {
            return true;
        }
    }
    return false;
}
} // namespace nenenib::application
