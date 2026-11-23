#include "EditorController.hpp"

#include "AutosaveId.hpp"
#include "JsonNumber.hpp"
#include "JsonValue.hpp"
#include "LanguageAnswers.hpp"
#include "LanguageCompletion.hpp"
#include "LanguageDefinition.hpp"
#include "LanguageFormatting.hpp"
#include "LanguageHover.hpp"
#include "LanguageServerDocument.hpp"
#include "LanguageServerLink.hpp"
#include "LanguageServerPhase.hpp"
#include "LanguageServerSlot.hpp"
#include "LanguageServers.hpp"
#include "LanguageSignature.hpp"
#include "LanguageSymbols.hpp"
#include "LspAnswerFailure.hpp"
#include "LspAnswers.hpp"
#include "LspCompletionContext.hpp"
#include "LspCompletionList.hpp"
#include "LspError.hpp"
#include "LspFormatting.hpp"
#include "LspLocation.hpp"
#include "LspPosition.hpp"
#include "LspPositionEncoding.hpp"
#include "LspPositionJson.hpp"
#include "LspRequestContext.hpp"
#include "LspRequestKind.hpp"
#include "LspResponse.hpp"
#include "LspSignatureHelp.hpp"
#include "LspSymbol.hpp"
#include "LspSymbols.hpp"
#include "PendingLspContext.hpp"
#include "PendingLspRequest.hpp"
#include "ServerAbilities.hpp"

#include <algorithm>
#include <cstddef>
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

// encoding は答えた席の位置の単位（着地の写しまで運ぶ・#1916 のレビュー R2）。
[[nodiscard]] std::optional<LanguageDefinition>
definition_answer(const PendingLspRequest &request, const core::LspResponse &response,
                  const LspRequestContext &current, core::LspPositionEncoding encoding)
{
    const auto *context = std::get_if<LspRequestContext>(&request.context);
    if (context == nullptr || !(*context == current))
    {
        return std::nullopt;
    }
    auto locations = locations_of(response.result);
    const std::size_t total = locations.size();
    return LanguageDefinition{.context = *context,
                              .locations = std::move(locations),
                              .encoding = encoding,
                              .total = total};
}

// 参照の答え（ADR 0191 の決定 4）。定義と同じ照合で、受け取りの時点で lsp_reference_limit 件に切る
// （全件を状態へ写さない・#1954 のレビュー R2）。total は切る前の件数。
[[nodiscard]] std::optional<LanguageDefinition>
references_answer(const PendingLspRequest &request, const core::LspResponse &response,
                  const LspRequestContext &current, core::LspPositionEncoding encoding)
{
    auto answer = definition_answer(request, response, current, encoding);
    if (answer.has_value() && answer.value().locations.size() > core::lsp_reference_limit)
    {
        answer.value().locations.resize(core::lsp_reference_limit);
    }
    return answer;
}

// 記号の答え（#1955・ADR 0191 の決定 5）。照合は定義と同じ。誤りの答えと読めない答えは空の列。
[[nodiscard]] std::optional<LanguageSymbols> symbols_answer(const PendingLspRequest &request,
                                                            const core::LspResponse &response,
                                                            const LspRequestContext &current,
                                                            core::LspPositionEncoding encoding)
{
    const auto *context = std::get_if<LspRequestContext>(&request.context);
    if (context == nullptr || !(*context == current))
    {
        return std::nullopt;
    }
    auto read = response.result.has_value()
                    ? core::lsp_document_symbols(response.result.value())
                    : std::unexpected{core::LspAnswerFailure::invalid_result};
    core::LspSymbolList list =
        read.has_value() ? std::move(read).value() : core::LspSymbolList{.symbols = {}, .total = 0};
    return LanguageSymbols{.context = *context,
                           .symbols = std::move(list.symbols),
                           .encoding = encoding,
                           .total = list.total};
}
// 引数の情報の答え（#1959・ADR 0191 の決定 6）。照合は hover と同じ。誤りの答えと読めない答えは
// 署名なし（札を消す）。
[[nodiscard]] std::optional<LanguageSignature> signature_answer(const PendingLspRequest &request,
                                                                const core::LspResponse &response,
                                                                const LspRequestContext &current)
{
    const auto *context = std::get_if<LspRequestContext>(&request.context);
    if (context == nullptr || !(*context == current))
    {
        return std::nullopt;
    }
    const auto read = response.result.has_value()
                          ? core::lsp_signature_help(response.result.value())
                          : std::unexpected{core::LspAnswerFailure::invalid_result};
    return LanguageSignature{.context = *context,
                             .signature = read.has_value() ? read.value() : std::nullopt};
}

// 整形の答え（#1993・ADR 0191 の決定 7）。文脈は照合せずに要求のものを添える（当てる前に比べ、
// 変わっていたら 1 行知らせる）。誤りの答えと読めない答えは編集の列なし。
[[nodiscard]] std::optional<LanguageFormatting>
formatting_answer(const PendingLspRequest &request, const core::LspResponse &response,
                  core::LspPositionEncoding encoding)
{
    const auto *context = std::get_if<LspRequestContext>(&request.context);
    if (context == nullptr)
    {
        return std::nullopt;
    }
    auto read = response.result.has_value()
                    ? core::lsp_text_edits(response.result.value())
                    : std::unexpected{core::LspAnswerFailure::invalid_result};
    return LanguageFormatting{.context = *context,
                              .edits = read.has_value() ? std::optional{std::move(read).value()}
                                                        : std::nullopt,
                              .encoding = encoding};
}
} // namespace

// ---------------------------------------------------------------- 文脈つきの要求（ADR 0190）

bool EditorController::request_completion(std::uint64_t session,
                                          std::optional<std::string_view> trigger)
{
    const auto slot = requestable_slot(&ServerAbilities::completion);
    if (!slot.has_value())
    {
        return false;
    }
    const LanguageServerDocument &document = active_document_of(slot.value());
    auto params = position_params(document.uri, request_position(slot.value()));
    params.emplace_back("context", completion_context(trigger));
    send_language_request(slot.value(), LspRequestKind::completion,
                          core::JsonValue{std::move(params)},
                          LspCompletionContext{.session = session, .document = document.tab});
    return true;
}

bool EditorController::request_hover()
{
    const auto slot = requestable_slot(&ServerAbilities::hover);
    if (!slot.has_value())
    {
        return false;
    }
    send_language_request(slot.value(), LspRequestKind::hover,
                          core::JsonValue{position_params(active_document_of(slot.value()).uri,
                                                          request_position(slot.value()))},
                          request_context());
    return true;
}

bool EditorController::request_definition()
{
    const auto slot = requestable_slot(&ServerAbilities::definition);
    if (!slot.has_value())
    {
        return false;
    }
    send_language_request(slot.value(), LspRequestKind::definition,
                          core::JsonValue{position_params(active_document_of(slot.value()).uri,
                                                          request_position(slot.value()))},
                          request_context());
    return true;
}

// 参照（#1947・ADR 0191 の決定 4）。`ReferenceParams` は位置と `context.includeDeclaration`。
bool EditorController::request_references()
{
    const auto slot = requestable_slot(&ServerAbilities::references);
    if (!slot.has_value())
    {
        return false;
    }
    auto params =
        position_params(active_document_of(slot.value()).uri, request_position(slot.value()));
    core::JsonObject context;
    context.emplace_back("includeDeclaration", core::JsonValue{true});
    params.emplace_back("context", core::JsonValue{std::move(context)});
    send_language_request(slot.value(), LspRequestKind::references,
                          core::JsonValue{std::move(params)}, request_context());
    return true;
}

// 記号（#1955・ADR 0191 の決定 5）。`DocumentSymbolParams` は文書だけ。
bool EditorController::request_document_symbols()
{
    const auto slot = requestable_slot(&ServerAbilities::document_symbols);
    if (!slot.has_value())
    {
        return false;
    }
    core::JsonObject document;
    document.emplace_back("uri", core::JsonValue{active_document_of(slot.value()).uri});
    core::JsonObject params;
    params.emplace_back("textDocument", core::JsonValue{std::move(document)});
    send_language_request(slot.value(), LspRequestKind::document_symbol,
                          core::JsonValue{std::move(params)}, request_context());
    return true;
}

// 言語サーバーは動いているが、アクティブな文書を受ける席がその能力を持たない（ADR 0191 の決定 2）。
// 知らせを分けるためだけに読む（要求の可否は requestable_slot の 1 本）。
bool EditorController::lacks_ability(bool ServerAbilities::*ability) const
{
    const auto slot = holding_slot();
    if (!slot.has_value())
    {
        return false;
    }
    const auto &link = language_servers_.links.at(slot_index(slot.value()));
    return link.phase == LanguageServerPhase::running && !(link.abilities.*ability);
}

const LanguageAnswers &EditorController::language_answers() const noexcept
{
    return state_.language_answers();
}

// 要求を出せるアクティブな文書の席（決定 2・3・2026-11-20 の追補）。アクティブな文書を開いた席が
// running で、能力（ability の欄）があり、その文書を今の URI で `didOpen` してあるとき。無題・
// 上限を超えた本文・言語の違う文書・まだ開いていない文書では値なし。出せるなら、要求より先に
// 届くように未送信の変更を流す。補完・hover・定義は文書の言語の席へ出る。
std::optional<LanguageServerSlot> EditorController::requestable_slot(bool ServerAbilities::*ability)
{
    const auto slot = holding_slot();
    if (!slot.has_value())
    {
        return std::nullopt;
    }
    auto &link = link_of(slot.value());
    if (link.phase != LanguageServerPhase::running || !(link.abilities.*ability))
    {
        return std::nullopt;
    }
    const auto uri = language_server_uri(slot.value());
    LanguageServerDocument &document = active_document_of(slot.value());
    if (!uri.has_value() || document.uri != uri.value())
    {
        return std::nullopt;
    }
    flush_language_server_changes(slot.value(), document);
    return slot;
}

// 席が開いたアクティブな文書（holding_slot が見つけた席でだけ呼ぶ）。
LanguageServerDocument &EditorController::active_document_of(LanguageServerSlot slot)
{
    const AutosaveId active = state_.autosave_ids().at(state_.active_tab());
    auto &documents = link_of(slot).documents;
    return *std::ranges::find_if(documents, [active](const LanguageServerDocument &document)
                                 { return document.tab == active; });
}

// 同じ種類の古い要求を取り消してから送る（決定 1）。遅れて届いた古い答えは id が表に無いので
// 捨てられる。
void EditorController::send_language_request(LanguageServerSlot slot, LspRequestKind kind,
                                             core::JsonValue params, PendingLspContext context)
{
    cancel_lsp_requests(slot, kind);
    send_lsp_request(slot, kind, params, std::move(context));
}

void EditorController::cancel_lsp_requests(LanguageServerSlot slot, LspRequestKind kind)
{
    auto &pending = link_of(slot).pending;
    for (const PendingLspRequest &request : pending)
    {
        if (request.kind == kind)
        {
            send_lsp_cancel(slot, request.id);
        }
    }
    std::erase_if(pending,
                  [kind](const PendingLspRequest &request) { return request.kind == kind; });
}

// 要求の位置はキャレット（本文の位置 → Position は core の lsp_position_of の 1 本）。
core::LspPosition EditorController::request_position(LanguageServerSlot slot) const
{
    return core::lsp_position_of(state_.text(), state_.selection().caret,
                                 language_servers_.links.at(slot_index(slot)).encoding);
}

LspRequestContext EditorController::request_context() const
{
    return LspRequestContext{.document = state_.autosave_ids().at(state_.active_tab()),
                             .revision = static_cast<std::uint64_t>(state_.history().revision()),
                             .at = state_.selection().caret};
}

// 文脈を照合して答えを状態に置く（決定 1）。合わなければ捨て、その種類の前の答えも消す（古い答えが
// 後から使われる筋を無くす・#1825 のレビュー R4）。知らせは出さない。
void EditorController::receive_answer(LanguageServerSlot slot, const PendingLspRequest &request,
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
        break;
    case LspRequestKind::hover:
        answers.hover = hover_answer(request, response, current);
        break;
    case LspRequestKind::definition:
        answers.definition = definition_answer(request, response, current, link_of(slot).encoding);
        break;
    case LspRequestKind::references:
        answers.references = references_answer(request, response, current, link_of(slot).encoding);
        break;
    case LspRequestKind::document_symbol:
        answers.symbols = symbols_answer(request, response, current, link_of(slot).encoding);
        break;
    case LspRequestKind::signature_help:
        answers.signature = signature_answer(request, response, current);
        break;
    case LspRequestKind::formatting:
    case LspRequestKind::range_formatting:
        answers.formatting = formatting_answer(request, response, link_of(slot).encoding);
        break;
    }
    state_ = state_.with_language_answers(std::move(answers));
}
} // namespace nenenib::application
