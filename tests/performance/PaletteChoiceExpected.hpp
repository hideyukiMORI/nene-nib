#pragma once
#include "CommandChoiceKind.hpp"
#include "EditorOperation.hpp"
#include "PaletteOrigin.hpp"
#include <cstddef>
#include <optional>
#include <string_view>
namespace nenenib::tests::performance
{
struct PaletteChoiceExpected
{
    std::string_view command;
    std::string_view label;
    std::size_t label_code_points;
    core::CommandChoiceKind kind;
    std::optional<std::string_view> detail;
    std::optional<core::PaletteOrigin> origin;
    std::optional<core::EditorOperation> operation;
    std::string_view key;
};
} // namespace nenenib::tests::performance
