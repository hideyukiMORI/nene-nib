#include "LspJsonRead.hpp"

#include "JsonNumber.hpp"

#include <cstddef>
#include <functional>
#include <variant>

namespace nenenib::core
{
namespace
{
using Member = std::optional<std::reference_wrapper<const JsonValue>>;
} // namespace

const JsonObject *json_object_of(const JsonValue &value) noexcept
{
    return std::get_if<JsonObject>(&value.value);
}

const JsonObject *json_object_member(const JsonObject &object, std::string_view key)
{
    const Member found = json_member(object, key);
    return found.has_value() ? json_object_of(found.value().get()) : nullptr;
}

const std::string *json_text_member(const JsonObject &object, std::string_view key)
{
    const Member found = json_member(object, key);
    return found.has_value() ? std::get_if<std::string>(&found.value().get().value) : nullptr;
}

std::optional<std::uint64_t> json_count_of(const JsonValue &value) noexcept
{
    const auto *number = std::get_if<JsonNumber>(&value.value);
    if (number == nullptr || !number->integer.has_value() || number->integer.value() < 0)
    {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(number->integer.value());
}

std::optional<std::uint64_t> json_count_member(const JsonObject &object, std::string_view key)
{
    const Member found = json_member(object, key);
    return found.has_value() ? json_count_of(found.value().get()) : std::nullopt;
}

std::optional<LspPosition> lsp_position_member(const JsonObject &object, std::string_view key)
{
    const JsonObject *position = json_object_member(object, key);
    if (position == nullptr)
    {
        return std::nullopt;
    }
    const std::optional<std::uint64_t> line = json_count_member(*position, "line");
    const std::optional<std::uint64_t> character = json_count_member(*position, "character");
    if (!line.has_value() || !character.has_value())
    {
        return std::nullopt;
    }
    return LspPosition{.line = static_cast<std::size_t>(line.value()),
                       .character = static_cast<std::size_t>(character.value())};
}

std::optional<LspRange> lsp_range_member(const JsonObject &object, std::string_view key)
{
    const JsonObject *range = json_object_member(object, key);
    if (range == nullptr)
    {
        return std::nullopt;
    }
    const std::optional<LspPosition> start = lsp_position_member(*range, "start");
    const std::optional<LspPosition> end = lsp_position_member(*range, "end");
    if (!start.has_value() || !end.has_value())
    {
        return std::nullopt;
    }
    return LspRange{.start = start.value(), .end = end.value()};
}
} // namespace nenenib::core
