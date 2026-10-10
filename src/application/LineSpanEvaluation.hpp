#pragma once

#include "Column.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "SelectionSpan.hpp"

#include <string_view>

namespace nenenib::application
{
class EditorController;

// 行の所有済み本文を、この行の選択と一致の投影中だけ借用する（ADR 0095）。
class LineSpanEvaluation
{
    friend class EditorController;

    LineSpanEvaluation(std::string_view text, core::Offset start, core::Offset terminator_end);
    [[nodiscard]] core::SelectionSpan span(const core::OffsetRange &range);
    [[nodiscard]] core::Column column_at(core::Offset at);

    std::string_view text_;
    core::Offset start_;
    core::Offset terminator_end_;
    core::Offset cursor_{0};
    core::Column column_{1};
};
} // namespace nenenib::application
