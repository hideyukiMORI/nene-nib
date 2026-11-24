#include "LspRename.hpp"

#include "JsonNull.hpp"
#include "LspJsonRead.hpp"
#include "LspPosition.hpp"

#include <string>
#include <variant>

namespace nenenib::core
{
namespace
{
using RenameTargetRead = std::expected<std::optional<LspRenameTarget>, LspAnswerFailure>;

// `{defaultBehavior: boolean}`。true はキャレットの下の語で決める形、false は変えられない。
[[nodiscard]] RenameTargetRead default_behavior(const JsonValue &behavior)
{
    const bool *flag = std::get_if<bool>(&behavior.value);
    if (flag == nullptr)
    {
        return std::unexpected{LspAnswerFailure::invalid_result};
    }
    return *flag
               ? std::optional{LspRenameTarget{.range = std::nullopt, .placeholder = std::nullopt}}
               : std::optional<LspRenameTarget>{};
}

// `{range, placeholder}`（placeholder の欠けた形は range だけ）。
[[nodiscard]] RenameTargetRead placeholder_range(const JsonObject &object)
{
    const auto range = lsp_range_member(object, "range");
    if (!range.has_value())
    {
        return std::unexpected{LspAnswerFailure::invalid_result};
    }
    const std::string *placeholder = json_text_member(object, "placeholder");
    return std::optional{LspRenameTarget{
        .range = range,
        .placeholder = placeholder != nullptr ? std::optional{*placeholder} : std::nullopt}};
}

// `Range`（start と end の Position）。
[[nodiscard]] RenameTargetRead bare_range(const JsonObject &object)
{
    const auto start = lsp_position_member(object, "start");
    const auto end = lsp_position_member(object, "end");
    if (!start.has_value() || !end.has_value())
    {
        return std::unexpected{LspAnswerFailure::invalid_result};
    }
    return std::optional{
        LspRenameTarget{.range = LspRange{.start = start.value(), .end = end.value()},
                        .placeholder = std::nullopt}};
}
} // namespace

std::expected<std::optional<LspRenameTarget>, LspAnswerFailure>
lsp_rename_target(const JsonValue &result)
{
    if (std::holds_alternative<JsonNull>(result.value))
    {
        return std::optional<LspRenameTarget>{};
    }
    const JsonObject *object = json_object_of(result);
    if (object == nullptr)
    {
        return std::unexpected{LspAnswerFailure::invalid_result};
    }
    if (const auto behavior = json_member(*object, "defaultBehavior"); behavior.has_value())
    {
        return default_behavior(behavior.value().get());
    }
    if (json_member(*object, "range").has_value())
    {
        return placeholder_range(*object);
    }
    return bare_range(*object);
}
} // namespace nenenib::core
