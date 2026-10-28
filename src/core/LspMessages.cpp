#include "LspMessages.hpp"

#include "JsonNull.hpp"
#include "JsonNumber.hpp"
#include "JsonValue.hpp"
#include "JsonWrite.hpp"
#include "LspError.hpp"
#include "LspId.hpp"
#include "LspIncoming.hpp"
#include "LspMessageFailure.hpp"
#include "LspNotification.hpp"
#include "LspRequest.hpp"
#include "LspResponse.hpp"

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::core
{
namespace
{
using Incoming = std::expected<LspIncoming, LspMessageFailure>;
using ParsedId = std::expected<LspId, LspMessageFailure>;
using ParsedError = std::expected<LspError, LspMessageFailure>;
using Member = std::optional<std::reference_wrapper<const JsonValue>>;

constexpr std::string_view json_rpc_version = "2.0";
constexpr std::string_view method_not_found_message = "Method not found";

[[nodiscard]] JsonValue member_or_null(const JsonObject &object, std::string_view key)
{
    const Member found = json_member(object, key);
    if (!found.has_value())
    {
        return JsonValue{JsonNull{}};
    }
    return found.value().get();
}

[[nodiscard]] std::optional<std::string> string_member(const JsonObject &object,
                                                       std::string_view key)
{
    const Member found = json_member(object, key);
    if (!found.has_value())
    {
        return std::nullopt;
    }
    const auto *text = std::get_if<std::string>(&found.value().get().value);
    if (text == nullptr)
    {
        return std::nullopt;
    }
    return *text;
}

[[nodiscard]] std::optional<std::int64_t> integer_of(const JsonValue &value)
{
    const auto *number = std::get_if<JsonNumber>(&value.value);
    if (number == nullptr)
    {
        return std::nullopt;
    }
    return number->integer;
}

[[nodiscard]] ParsedId id_of(const JsonValue &value)
{
    const std::optional<std::int64_t> number = integer_of(value);
    if (number.has_value())
    {
        return LspId{number.value()};
    }
    const auto *text = std::get_if<std::string>(&value.value);
    if (text == nullptr)
    {
        return std::unexpected(LspMessageFailure::invalid_id);
    }
    return LspId{*text};
}

[[nodiscard]] ParsedError error_of(const JsonValue &value)
{
    const auto *object = std::get_if<JsonObject>(&value.value);
    if (object == nullptr)
    {
        return std::unexpected(LspMessageFailure::invalid_error);
    }
    const Member code = json_member(*object, "code");
    const std::optional<std::int64_t> number =
        code.has_value() ? integer_of(code.value().get()) : std::nullopt;
    std::optional<std::string> message = string_member(*object, "message");
    if (!number.has_value() || !message.has_value())
    {
        return std::unexpected(LspMessageFailure::invalid_error);
    }
    return LspError{.code = number.value(), .message = std::move(message).value()};
}

// `method` を持つ 1 通（`id` があれば要求、無ければ通知）。
[[nodiscard]] Incoming call_of(const JsonObject &object, const JsonValue &method)
{
    const auto *name = std::get_if<std::string>(&method.value);
    if (name == nullptr)
    {
        return std::unexpected(LspMessageFailure::invalid_method);
    }
    const Member id = json_member(object, "id");
    if (!id.has_value())
    {
        return LspNotification{.method = *name, .params = member_or_null(object, "params")};
    }
    ParsedId parsed = id_of(id.value().get());
    if (!parsed.has_value())
    {
        return std::unexpected(parsed.error());
    }
    return LspRequest{.id = std::move(parsed).value(),
                      .method = *name,
                      .params = member_or_null(object, "params")};
}

// `method` を持たない 1 通（`id` を持つ答え）。`error` があれば result より先に読む。
[[nodiscard]] Incoming response_of(const JsonObject &object)
{
    const Member id = json_member(object, "id");
    if (!id.has_value())
    {
        return std::unexpected(LspMessageFailure::missing_id);
    }
    ParsedId parsed = id_of(id.value().get());
    if (!parsed.has_value())
    {
        return std::unexpected(parsed.error());
    }
    const Member error = json_member(object, "error");
    if (!error.has_value())
    {
        return LspResponse{.id = std::move(parsed).value(),
                           .result = member_or_null(object, "result")};
    }
    ParsedError failure = error_of(error.value().get());
    if (!failure.has_value())
    {
        return std::unexpected(failure.error());
    }
    return LspResponse{.id = std::move(parsed).value(),
                       .result = std::unexpected(std::move(failure).value())};
}

[[nodiscard]] JsonValue id_alternative(std::int64_t id)
{
    return JsonValue{json_integer(id)};
}

[[nodiscard]] JsonValue id_alternative(const std::string &id)
{
    return JsonValue{id};
}

[[nodiscard]] JsonValue id_value(const LspId &id)
{
    return std::visit([](const auto &alternative) { return id_alternative(alternative); }, id);
}

[[nodiscard]] JsonObject envelope()
{
    JsonObject members;
    members.emplace_back("jsonrpc", JsonValue{std::string{json_rpc_version}});
    return members;
}

[[nodiscard]] std::string call_body(JsonObject members, std::string_view method,
                                    const JsonValue &params)
{
    members.emplace_back("method", JsonValue{std::string{method}});
    if (!std::holds_alternative<JsonNull>(params.value))
    {
        members.emplace_back("params", params);
    }
    return write_json(JsonValue{std::move(members)});
}
} // namespace

std::expected<LspIncoming, LspMessageFailure> lsp_incoming(const JsonValue &message)
{
    const auto *object = std::get_if<JsonObject>(&message.value);
    if (object == nullptr)
    {
        return std::unexpected(LspMessageFailure::not_object);
    }
    if (string_member(*object, "jsonrpc") != std::optional<std::string>{json_rpc_version})
    {
        return std::unexpected(LspMessageFailure::not_json_rpc);
    }
    const Member method = json_member(*object, "method");
    if (method.has_value())
    {
        return call_of(*object, method.value().get());
    }
    return response_of(*object);
}

std::string lsp_request_body(std::int64_t id, std::string_view method, const JsonValue &params)
{
    JsonObject members = envelope();
    members.emplace_back("id", id_value(LspId{id}));
    return call_body(std::move(members), method, params);
}

std::string lsp_notification_body(std::string_view method, const JsonValue &params)
{
    return call_body(envelope(), method, params);
}

std::string lsp_method_not_found_body(const LspId &id)
{
    JsonObject error;
    error.emplace_back("code", JsonValue{json_integer(lsp_method_not_found_code)});
    error.emplace_back("message", JsonValue{std::string{method_not_found_message}});
    JsonObject members = envelope();
    members.emplace_back("id", id_value(id));
    members.emplace_back("error", JsonValue{std::move(error)});
    return write_json(JsonValue{std::move(members)});
}
} // namespace nenenib::core
