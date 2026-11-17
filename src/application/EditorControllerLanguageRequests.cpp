#include "EditorController.hpp"

#include "AutosaveId.hpp"
#include "JsonNumber.hpp"
#include "JsonValue.hpp"
#include "LanguageAnswers.hpp"
#include "LanguageCompletion.hpp"
#include "LanguageDefinition.hpp"
#include "LanguageHover.hpp"
#include "LanguageServerDocument.hpp"
#include "LanguageServerLink.hpp"
#include "LanguageServerPhase.hpp"
#include "LspAnswerFailure.hpp"
#include "LspAnswers.hpp"
#include "LspCompletionContext.hpp"
#include "LspCompletionList.hpp"
#include "LspError.hpp"
#include "LspLocation.hpp"
#include "LspPosition.hpp"
#include "LspPositionJson.hpp"
#include "LspRequestContext.hpp"
#include "LspRequestKind.hpp"
#include "LspResponse.hpp"
#include "PendingLspContext.hpp"
#include "PendingLspRequest.hpp"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::application
{
namespace
{
// LSP の `CompletionTriggerKind`（Invoked と TriggerCharacter）。
constexpr std::int64_t invoked_trigger = 1;
constexpr std::int64_t character_trigger = 2;

// `TextDocumentPositionParams`（ADR 0190 の決定 1）。
[[nodiscard]] core::JsonObject position_params(const std::string &uri, core::LspPosition position)
{
    core::JsonObject document;
    document.emplace_back("uri", core::JsonValue{uri});
    core::JsonObject params;
    params.emplace_back("textDocument", core::JsonValue{std::move(document)});
    params.emplace_back("position", core::lsp_position_json(position));
    return params;
}

// `CompletionContext`。手で開いたら Invoked、`triggerCharacters` の 1 文字なら TriggerCharacter。
[[nodiscard]] core::JsonValue completion_context(std::optional<std::string_view> trigger)
{
    core::JsonObject context;
    context.emplace_back("triggerKind",
                         core::JsonValue{core::json_integer(
                             trigger.has_value() ? character_trigger : invoked_trigger)});
    if (trigger.has_value())
    {
        context.emplace_back("triggerCharacter", core::JsonValue{std::string{trigger.value()}});
    }
    return core::JsonValue{std::move(context)};
}

// 補完の答え → 候補の列。誤りの答えと読めない答えは空の列（面は本文の語だけで続く）。
[[nodiscard]] core::LspCompletionList
completion_list_of(const std::expected<core::JsonValue, core::LspError> &result)
{
    auto list = result.has_value() ? core::lsp_completion_items(result.value())
                                   : std::unexpected{core::LspAnswerFailure::invalid_result};
    return list.has_value() ? std::move(list).value()
                            : core::LspCompletionList{.incomplete = false, .items = {}};
}

// 定義の答え → 場所の列。誤りの答えと読めない答えは空の列（見つからない）。
[[nodiscard]] std::vector<core::LspLocation>
locations_of(const std::expected<core::JsonValue, core::LspError> &result)
{
    auto locations = result.has_value() ? core::lsp_locations(result.value())
                                        : std::unexpected{core::LspAnswerFailure::invalid_result};
    return locations.has_value() ? std::move(locations).value() : std::vector<core::LspLocation>{};
}

// 補完の答えを使うのは同じ文書がアクティブなときだけ（決定 1・面がまだ開いているかは段 B の面が
// session で比べる）。
[[nodiscard]] std::optional<LanguageCompletion> completion_answer(const PendingLspRequest &request,
                                                                  const core::LspResponse &response,
                                                                  AutosaveId active)
{
    const auto *context = std::get_if<LspCompletionContext>(&request.context);
    if (context == nullptr || context->document != active)
    {
        return std::nullopt;
    }
    return LanguageCompletion{.session = context->session,
                              .document = context->document,
                              .list = completion_list_of(response.result)};
}

// hover と定義への答えを使うのは、同じ文書がアクティブで revision とキャレットが同じときだけ
// （決定 1）。current は今の文脈。
[[nodiscard]] std::optional<LanguageHover> hover_answer(const PendingLspRequest &request,
                                                        const core::LspResponse &response,
                                                        const LspRequestContext &current)
{
    const auto *context = std::get_if<LspRequestContext>(&request.context);
    if (context == nullptr || !(*context == current))
    {
        return std::nullopt;
    }
    return LanguageHover{.context = *context,
                         .text = response.result.has_value()
                                     ? core::lsp_hover_text(response.result.value())
                                     : std::nullopt};
}

[[nodiscard]] std::optional<LanguageDefinition> definition_answer(const PendingLspRequest &request,
                                                                  const core::LspResponse &response,
                                                                  const LspRequestContext &current)
{
    const auto *context = std::get_if<LspRequestContext>(&request.context);
    if (context == nullptr || !(*context == current))
    {
        return std::nullopt;
    }
    return LanguageDefinition{.context = *context, .locations = locations_of(response.result)};
}
} // namespace

// ---------------------------------------------------------------- 文脈つきの要求（ADR 0190）

bool EditorController::request_completion(std::uint64_t session,
                                          std::optional<std::string_view> trigger)
{
    const LanguageServerDocument *document =
        requestable_document(language_server_.abilities.completion);
    if (document == nullptr)
    {
        return false;
    }
    auto params = position_params(document->uri, request_position());
    params.emplace_back("context", completion_context(trigger));
    send_language_request(LspRequestKind::completion, "textDocument/completion",
                          core::JsonValue{std::move(params)},
                          LspCompletionContext{.session = session, .document = document->tab});
    return true;
}

bool EditorController::request_hover()
{
    const LanguageServerDocument *document = requestable_document(language_server_.abilities.hover);
    if (document == nullptr)
    {
        return false;
    }
    send_language_request(LspRequestKind::hover, "textDocument/hover",
                          core::JsonValue{position_params(document->uri, request_position())},
                          request_context());
    return true;
}

bool EditorController::request_definition()
{
    const LanguageServerDocument *document =
        requestable_document(language_server_.abilities.definition);
    if (document == nullptr)
    {
        return false;
    }
    send_language_request(LspRequestKind::definition, "textDocument/definition",
                          core::JsonValue{position_params(document->uri, request_position())},
                          request_context());
    return true;
}

const LanguageAnswers &EditorController::language_answers() const noexcept
{
    return state_.language_answers();
}

// 要求を出せるアクティブな文書（決定 2・3）。running で、能力があり、アクティブな文書を今の URI で
// `didOpen` してあるとき。無題・上限を超えた本文・言語の違う文書・まだ開いていない文書では
// 値なし。出せるなら、要求より先に届くように未送信の変更を流す。
LanguageServerDocument *EditorController::requestable_document(bool ability)
{
    auto &link = language_server_;
    if (link.phase != LanguageServerPhase::running || !ability)
    {
        return nullptr;
    }
    const auto uri = language_server_uri();
    if (!uri.has_value())
    {
        return nullptr;
    }
    const AutosaveId active = state_.autosave_ids().at(state_.active_tab());
    const auto found =
        std::ranges::find_if(link.documents, [active, &uri](const LanguageServerDocument &document)
                             { return document.tab == active && document.uri == uri.value(); });
    if (found == link.documents.end())
    {
        return nullptr;
    }
    flush_language_server_changes(*found);
    return &*found;
}

// 同じ種類の古い要求を取り消してから送る（決定 1）。遅れて届いた古い答えは id が表に無いので
// 捨てられる。
void EditorController::send_language_request(LspRequestKind kind, std::string_view method,
                                             core::JsonValue params, PendingLspContext context)
{
    cancel_lsp_requests(kind);
    send_lsp_request(kind, method, params, std::move(context));
}

void EditorController::cancel_lsp_requests(LspRequestKind kind)
{
    auto &pending = language_server_.pending;
    for (const PendingLspRequest &request : pending)
    {
        if (request.kind == kind)
        {
            send_lsp_cancel(request.id);
        }
    }
    std::erase_if(pending,
                  [kind](const PendingLspRequest &request) { return request.kind == kind; });
}

// 要求の位置はキャレット（本文の位置 → Position は core の lsp_position_of の 1 本）。
core::LspPosition EditorController::request_position() const
{
    return core::lsp_position_of(state_.text(), state_.selection().caret,
                                 language_server_.encoding);
}

LspRequestContext EditorController::request_context() const
{
    return LspRequestContext{.document = state_.autosave_ids().at(state_.active_tab()),
                             .revision = static_cast<std::uint64_t>(state_.history().revision()),
                             .at = state_.selection().caret};
}

// 文脈を照合して答えを状態に置く（決定 1）。合わなければ捨て、知らせは出さない。
void EditorController::receive_answer(const PendingLspRequest &request,
                                      const core::LspResponse &response)
{
    LanguageAnswers answers = state_.language_answers();
    const AutosaveId active = state_.autosave_ids().at(state_.active_tab());
    const LspRequestContext current = request_context();
    switch (request.kind)
    {
    case LspRequestKind::initialize:
    case LspRequestKind::shutdown:
        return;
    case LspRequestKind::completion:
        answers.completion = completion_answer(request, response, active);
        if (!answers.completion.has_value())
        {
            return;
        }
        break;
    case LspRequestKind::hover:
        answers.hover = hover_answer(request, response, current);
        if (!answers.hover.has_value())
        {
            return;
        }
        break;
    case LspRequestKind::definition:
        answers.definition = definition_answer(request, response, current);
        if (!answers.definition.has_value())
        {
            return;
        }
        break;
    }
    state_ = state_.with_language_answers(std::move(answers));
}
} // namespace nenenib::application
