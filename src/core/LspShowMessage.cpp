#include "LspShowMessage.hpp"

#include "JsonNumber.hpp"
#include "JsonValue.hpp"
#include "NoticeLine.hpp"

#include <cstdint>
#include <string>
#include <variant>

namespace nenenib::core
{
namespace
{
// LSP 3.17 の MessageType の Error と Warning。
constexpr std::int64_t error_type = 1;
constexpr std::int64_t warning_type = 2;

[[nodiscard]] bool shown_type(const JsonObject &params)
{
    const auto found = json_member(params, "type");
    const auto *number =
        found.has_value() ? std::get_if<JsonNumber>(&found.value().get().value) : nullptr;
    if (number == nullptr || !number->integer.has_value())
    {
        return false;
    }
    const auto type = number->integer.value();
    return type == error_type || type == warning_type;
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
    const auto found = json_member(*params, "message");
    const auto *message =
        found.has_value() ? std::get_if<std::string>(&found.value().get().value) : nullptr;
    if (message == nullptr)
    {
        return std::nullopt;
    }
    return notice_line(*message);
}
} // namespace nenenib::core
