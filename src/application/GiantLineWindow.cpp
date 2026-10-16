#include "GiantLineWindow.hpp"

#include "GiantLine.hpp"

#include <algorithm>
#include <cstdint>

namespace nenenib::application
{
// コードポイントの数はバイトの数を超えないので、バイトでしきい値を超えない行は数えない。
const core::VirtualColumnIndex *giant_line_index(const core::TextBuffer &text,
                                                 core::LineNumber line)
{
    if (!text.has_virtual_columns())
    {
        return nullptr;
    }
    const auto end = text.line_end(line);
    if (end.value - text.line_start(line).value <= core::giant_line_threshold)
    {
        return nullptr;
    }
    const auto &index = text.virtual_columns().value().get();
    return index.code_point_at(text, end) > core::giant_line_threshold ? &index : nullptr;
}

float window_advance(core::TextViewGeometry geometry) noexcept
{
    return std::max(geometry.average_advance, 1.0F);
}

std::pair<LineWindow, core::OffsetRange> giant_window_at(const core::VirtualColumnIndex &index,
                                                         const core::TextBuffer &text,
                                                         core::LineNumber line, core::Offset anchor)
{
    const auto range = core::giant_line_window(index, text, line, anchor);
    const LineWindow window{true,
                            range.begin.value - text.line_start(line).value,
                            index.code_point_at(text, range.begin),
                            index.virtual_column_at(text, range.begin),
                            index.virtual_column_at(text, range.end),
                            0.0F};
    return {window, range};
}

// 窓の x は「仮想桁 × 字幅」の見積もりを origin からの差の整数で掛ける（決定 5）。
float window_x(std::size_t column, std::size_t origin, float advance) noexcept
{
    const auto difference = static_cast<std::int64_t>(column) - static_cast<std::int64_t>(origin);
    return static_cast<float>(difference) * advance;
}

core::Offset giant_offset_of(const core::VirtualColumnIndex &index, const core::TextBuffer &text,
                             core::TextPosition position)
{
    return index.byte_at_code_point(text, position.line,
                                    position.column.value > 0 ? position.column.value - 1 : 0);
}

core::TextPosition giant_position_of(const core::VirtualColumnIndex &index,
                                     const core::TextBuffer &text, core::LineNumber line,
                                     core::Offset at)
{
    return core::TextPosition{line, core::Column{index.code_point_at(text, at) + 1}};
}
} // namespace nenenib::application
