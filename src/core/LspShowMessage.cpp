#include "LspShowMessage.hpp"

#include "JsonNumber.hpp"
#include "JsonValue.hpp"
#include "NoticeLine.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace nenenib::core
{
namespace
{
// LSP 3.17 の MessageType の数。
constexpr std::int64_t error_type = 1;
constexpr std::int64_t warning_type = 2;
constexpr std::int64_t info_type = 3;

[[nodiscard]] std::optional<std::int64_t> type_of(const JsonObject &params)
{
    const auto found = json_member(params, "type");
    const auto *number =
        found.has_value() ? std::get_if<JsonNumber>(&found.value().get().value) : nullptr;
    if (number == nullptr || !number->integer.has_value())
    {
        return std::nullopt;
    }
    return number->integer.value();
}

[[nodiscard]] bool shown_type(const JsonObject &params)
{
    const auto type = type_of(params);
    return type == std::optional{error_type} || type == std::optional{warning_type};
}

[[nodiscard]] const std::string *message_of(const JsonObject &params)
{
    const auto found = json_member(params, "message");
    return found.has_value() ? std::get_if<std::string>(&found.value().get().value) : nullptr;
}

[[nodiscard]] LspMessageType message_type(std::int64_t type) noexcept
{
    return type == error_type     ? LspMessageType::error
           : type == warning_type ? LspMessageType::warning
           : type == info_type    ? LspMessageType::info
                                  : LspMessageType::log;
}
} // namespace

std::optional<DisplayText> lsp_shown_notice(const LspNotification &notification)
{
    if (notification.method != lsp_show_message_method)
    {
        return std::nullopt;
    }
    const auto *params = std::get_if<JsonObject>(&notification.params.value);
    if (params == nullptr || !shown_type(*params))
    {
        return std::nullopt;
    }
    const auto *message = message_of(*params);
    if (message == nullptr)
    {
        return std::nullopt;
    }
    return notice_line(*message);
}

std::optional<std::pair<LspMessageType, std::string>>
lsp_logged_message(const LspNotification &notification)
{
    if (notification.method != lsp_show_message_method &&
        notification.method != lsp_log_message_method)
    {
        return std::nullopt;
    }
    const auto *params = std::get_if<JsonObject>(&notification.params.value);
    const auto type = params != nullptr ? type_of(*params) : std::nullopt;
    const auto *message = params != nullptr ? message_of(*params) : nullptr;
    if (!type.has_value() || message == nullptr)
    {
        return std::nullopt;
    }
    return std::pair{message_type(type.value()), *message};
}
} // namespace nenenib::core
