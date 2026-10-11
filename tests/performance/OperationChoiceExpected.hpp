#pragma once
#include "CommandChoiceKind.hpp"
#include "EditorOperation.hpp"
#include "PaletteOrigin.hpp"
#include <cstddef>
#include <optional>
#include <string_view>
namespace nenenib::tests::performance
{
struct OperationChoiceExpected
{
    core::EditorOperation operation;
    std::size_t operation_ordinal;
    std::string_view label;
    std::size_t label_code_points;
    std::string_view detail;
    std::size_t detail_code_points;
    std::string_view key;
    std::string_view command;
    core::CommandChoiceKind kind;
    std::optional<core::PaletteOrigin> origin;
};
} // namespace nenenib::tests::performance
