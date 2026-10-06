#include "OperationBindings.hpp"

#include <algorithm>

namespace nenenib::core
{
std::optional<EditorOperation> operation_for(KeyChord chord, EditMode mode) noexcept
{
    const auto found =
        std::ranges::find_if(operation_bindings, [&](const OperationBinding &row)
                             { return row.chord == chord && modes_cover(row.modes, mode); });
    if (found == operation_bindings.end())
    {
        return std::nullopt;
    }
    return found->operation;
}

std::optional<KeyChord> shown_chord(EditorOperation operation, EditMode mode) noexcept
{
    const auto found = std::ranges::find_if(
        operation_bindings, [&](const OperationBinding &row)
        { return row.operation == operation && modes_cover(row.modes, mode); });
    if (found == operation_bindings.end())
    {
        return std::nullopt;
    }
    return found->chord;
}

bool operation_available(EditorOperation operation, EditMode mode) noexcept
{
    return std::ranges::any_of(
        operation_bindings, [&](const OperationBinding &row)
        { return row.operation == operation && modes_cover(row.modes, mode); });
}
} // namespace nenenib::core
