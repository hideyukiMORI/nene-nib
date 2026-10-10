#include "LineUtf16Evaluation.hpp"

#include "Utf16.hpp"
#include "Utf8.hpp"

#include <algorithm>

namespace nenenib::ui::win32
{
LineUtf16Evaluation::LineUtf16Evaluation(std::string_view text) : text_(text) {}

std::size_t LineUtf16Evaluation::byte_of(core::Column column)
{
    move_to(column);
    return byte_.value;
}

std::uint32_t LineUtf16Evaluation::units_of(core::Column column)
{
    move_to(column);
    return static_cast<std::uint32_t>(units_);
}

void LineUtf16Evaluation::move_to(core::Column column)
{
    const core::Column target{std::max(column.value, std::size_t{1})};
    if (target < column_)
    {
        byte_ = core::Offset{0};
        column_ = core::Column{1};
        units_ = 0;
    }
    const std::size_t begin = byte_.value;
    while (column_ < target && byte_.value < text_.size())
    {
        byte_ = core::next_code_point(text_, byte_);
        ++column_.value;
    }
    units_ += core::utf16_length(text_.substr(begin, byte_.value - begin)).value_or(0);
}
} // namespace nenenib::ui::win32
