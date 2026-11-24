#include "LspCodeActions.hpp"

#include "JsonNull.hpp"
#include "LspJsonRead.hpp"

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::core
{
namespace
{
// `Command`（title と command の文字列・arguments は配列のときだけ）。形が違えば値なし。
[[nodiscard]] std::optional<LspCommand> command_of(const JsonObject &object)
{
    const std::string *title = json_text_member(object, "title");
    const std::string *command = json_text_member(object, "command");
    if (title == nullptr || command == nullptr)
    {
        return std::nullopt;
    }
    const auto arguments = json_member(object, "arguments");
    const bool listed =
        arguments.has_value() && std::holds_alternative<JsonArray>(arguments.value().get().value);
    return LspCommand{.title = *title,
                      .command = *command,
                      .arguments = listed ? std::optional{arguments.value().get()} : std::nullopt};
}

// `CodeAction.isPreferred`（true のときだけ）。
[[nodiscard]] bool preferred_of(const JsonObject &object)
{
    const auto member = json_member(object, "isPreferred");
    const bool *flag =
        member.has_value() ? std::get_if<bool>(&member.value().get().value) : nullptr;
    return flag != nullptr && *flag;
}

// `Command` の literal の項目（command にも自分を置く）。形が違えば値なし。
[[nodiscard]] std::optional<LspCodeAction> command_action_of(const JsonObject &object,
                                                             const JsonValue &value)
{
    auto command = command_of(object);
    if (!command.has_value())
    {
        return std::nullopt;
    }
    std::string title = command.value().title;
    return LspCodeAction{.title = std::move(title),
                         .kind = {},
                         .preferred = false,
                         .edit = std::nullopt,
                         .command = std::move(command),
                         .resolvable = false,
                         .source = value};
}

// `CodeAction` の項目。title が無い・edit も command も data も無ければ値なし。
[[nodiscard]] std::optional<LspCodeAction> code_action_of(const JsonObject &object,
                                                          const JsonValue &value);

// 1 項目（値が `Command` の literal か `CodeAction`）。捨てる項目は値なし。
[[nodiscard]] std::optional<LspCodeAction> action_of(const JsonValue &value)
{
    const JsonObject *object = json_object_of(value);
    if (object == nullptr || json_member(*object, "disabled").has_value())
    {
        return std::nullopt;
    }
    return json_text_member(*object, "command") != nullptr ? command_action_of(*object, value)
                                                           : code_action_of(*object, value);
}

std::optional<LspCodeAction> code_action_of(const JsonObject &object, const JsonValue &value)
{
    const std::string *title = json_text_member(object, "title");
    const std::string *kind = json_text_member(object, "kind");
    const JsonObject *edit = json_object_member(object, "edit");
    const JsonObject *command = json_object_member(object, "command");
    const bool resolvable = json_member(object, "data").has_value();
    auto read_command = command != nullptr ? command_of(*command) : std::nullopt;
    if (title == nullptr || (edit == nullptr && !read_command.has_value() && !resolvable))
    {
        return std::nullopt;
    }
    return LspCodeAction{.title = *title,
                         .kind = kind != nullptr ? *kind : std::string{},
                         .preferred = preferred_of(object),
                         .edit = edit != nullptr ? std::optional{JsonValue{*edit}} : std::nullopt,
                         .command = std::move(read_command),
                         .resolvable = resolvable,
                         .source = value};
}
} // namespace

std::expected<std::vector<LspCodeAction>, LspAnswerFailure>
lsp_code_actions(const JsonValue &result)
{
    if (std::holds_alternative<JsonNull>(result.value))
    {
        return std::vector<LspCodeAction>{};
    }
    const auto *list = std::get_if<JsonArray>(&result.value);
    if (list == nullptr)
    {
        return std::unexpected{LspAnswerFailure::invalid_result};
    }
    std::vector<LspCodeAction> actions;
    for (const JsonValue &entry : *list)
    {
        if (actions.size() == lsp_code_action_limit)
        {
            break;
        }
        auto action = action_of(entry);
        if (action.has_value())
        {
            actions.push_back(std::move(action).value());
        }
    }
    return actions;
}

std::expected<LspCodeAction, LspAnswerFailure> lsp_code_action(const JsonValue &result)
{
    auto action = action_of(result);
    if (!action.has_value())
    {
        return std::unexpected{LspAnswerFailure::invalid_result};
    }
    return std::move(action).value();
}

JsonValue lsp_execute_command_params(const LspCommand &command)
{
    JsonObject params;
    params.emplace_back("command", JsonValue{command.command});
    if (command.arguments.has_value())
    {
        params.emplace_back("arguments", command.arguments.value());
    }
    return JsonValue{std::move(params)};
}
} // namespace nenenib::core
