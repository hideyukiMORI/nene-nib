#include "LineSpanEvaluation.hpp"

#include "Utf8.hpp"

#include <algorithm>

namespace nenenib::application
{
LineSpanEvaluation::LineSpanEvaluation(std::string_view text, core::Offset start,
                                       core::Offset terminator_end)
    : text_(text), start_(start), terminator_end_(terminator_end)
{
}

core::Column LineSpanEvaluation::column_at(core::Offset at)
{
    const core::Offset inside{at.value - start_.value};
    if (inside.value < cursor_.value)
    {
        cursor_ = core::Offset{0};
        column_ = core::Column{1};
    }
    column_.value +=
        core::code_point_count(text_.substr(cursor_.value, inside.value - cursor_.value));
    cursor_ = inside;
    return column_;
}

// 行をまたぐ範囲は内容末尾から1桁だけはみ出して改行を示す（ADR 0018 / 0095）。
core::SelectionSpan LineSpanEvaluation::span(const core::OffsetRange &range)
{
    const core::Offset content_end{start_.value + text_.size()};
    const std::size_t begin = std::max(range.begin.value, start_.value);
    const std::size_t end = std::min(range.end.value, terminator_end_.value);
    if (begin >= end)
    {
        return core::no_selection_span();
    }
    const core::Column first = column_at(core::Offset{std::min(begin, content_end.value)});
    const core::Column last = end > content_end.value
                                  ? core::Column{column_at(content_end).value + 1}
                                  : column_at(core::Offset{end});
    return core::SelectionSpan{core::SelectionPresence::present, first, last};
}
} // namespace nenenib::application
