#include "ComposedLine.hpp"

#include "Utf8.hpp"

#include <algorithm>
#include <utility>

namespace nenenib::application
{
ComposedLine composed_line(const core::DisplayLine &line, core::Column at,
                           const core::Composition &composition)
{
    const auto column = core::display_position(line, at.value > 0 ? at.value - 1 : 0);
    core::Offset byte{0};
    for (std::size_t index = 0; index < column; ++index)
    {
        byte = core::next_code_point(line.text, byte);
    }
    std::string text = line.text;
    text.insert(byte.value, composition.utf8);
    const auto prefix = std::string_view(composition.utf8).substr(0, composition.cursor.value);
    return ComposedLine{std::move(text),
                        core::OffsetRange{byte, core::Offset{byte.value + composition.utf8.size()}},
                        core::Column{column + 1}, core::code_point_count(composition.utf8),
                        core::Column{column + core::code_point_count(prefix) + 1}};
}

core::Column uncomposed_column(const ComposedLine &line, core::Column shown) noexcept
{
    if (shown.value <= line.insertion_column.value)
    {
        return shown;
    }
    const auto beyond = shown.value - line.insertion_column.value;
    return core::Column{line.insertion_column.value + beyond -
                        std::min(beyond, line.inserted_columns)};
}
} // namespace nenenib::application
