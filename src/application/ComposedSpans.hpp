#pragma once

#include "Column.hpp"

#include <cstddef>
#include <vector>

namespace nenenib::application
{
// IME の差し込み行の区間（#839・ADR 0161 の決定 4）。差し込みの手前は本文の桁のまま、後ろは差し
// 込んだ code point の数だけずらし、差し込みをまたぐ区間は 2 つに割る（格子の split と同じ理屈）。
// Span は桁の begin と end を持つ区間（構文の色 ColorSpan と診断の下線 DiagnosticSpan）で、ほかの
// 欄はそのまま写す。
template <typename Span>
[[nodiscard]] std::vector<Span> composed_spans(const std::vector<Span> &spans, core::Column at,
                                               std::size_t count)
{
    std::vector<Span> result;
    result.reserve(spans.size() + 1);
    const auto moved = [count](core::Column column) { return core::Column{column.value + count}; };
    const auto placed = [](Span span, core::Column begin, core::Column end)
    {
        span.begin = begin;
        span.end = end;
        return span;
    };
    for (const auto &span : spans)
    {
        if (span.end.value <= at.value)
        {
            result.push_back(span);
            continue;
        }
        if (span.begin.value < at.value)
        {
            result.push_back(placed(span, span.begin, at));
            result.push_back(placed(span, moved(at), moved(span.end)));
            continue;
        }
        result.push_back(placed(span, moved(span.begin), moved(span.end)));
    }
    return result;
}
} // namespace nenenib::application
