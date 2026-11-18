#include "LspPositionJson.hpp"

#include "JsonNumber.hpp"

#include <cstdint>
#include <utility>

namespace nenenib::core
{
JsonValue lsp_position_json(LspPosition position)
{
    JsonObject object;
    object.emplace_back("line", JsonValue{json_integer(static_cast<std::int64_t>(position.line))});
    object.emplace_back("character",
                        JsonValue{json_integer(static_cast<std::int64_t>(position.character))});
    return JsonValue{std::move(object)};
}
} // namespace nenenib::core
