#include "EditorController.hpp"

#include "DiagnosticCounts.hpp"
#include "DiagnosticEdits.hpp"
#include "DiagnosticNotice.hpp"
#include "DocumentUri.hpp"
#include "JsonNumber.hpp"
#include "JsonValue.hpp"
#include "LanguageServerDocument.hpp"
#include "LanguageServerLink.hpp"
#include "LanguageTable.hpp"
#include "LspDiagnostics.hpp"
#include "LspMessages.hpp"
#include "LspNotification.hpp"
#include "LspPosition.hpp"
#include "LspPositionEncoding.hpp"
#include "OperationGuide.hpp"
#include "SentRevision.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::application
{
namespace
{
[[nodiscard]] core::JsonValue count_json(std::uint64_t value)
{
    return core::JsonValue{core::json_integer(static_cast<std::int64_t>(value))};
}

// LSP の Position の JSON（ADR 0161 の決定 1）。
[[nodiscard]] core::JsonValue position_json(core::LspPosition position)
{
    core::JsonObject object;
    object.emplace_back("line", count_json(position.line));
    object.emplace_back("character", count_json(position.character));
    return core::JsonValue{std::move(object)};
}

// 1 編集 → `TextDocumentContentChangeEvent`（決定 1・増分）。range は編集の前の本文 before の
// 上の位置で、core の lsp_position_of の 1 本で写す。
[[nodiscard]] core::JsonValue change_json(const core::TextBuffer &before, const core::Edit &edit,
                                          core::LspPositionEncoding encoding)
{
    const core::Offset end{edit.at.value + edit.removed.size()};
    core::JsonObject range;
    range.emplace_back("start", position_json(core::lsp_position_of(before, edit.at, encoding)));
    range.emplace_back("end", position_json(core::lsp_position_of(before, end, encoding)));
    core::JsonObject change;
    change.emplace_back("range", core::JsonValue{std::move(range)});
    change.emplace_back("text", core::JsonValue{edit.inserted});
    return core::JsonValue{std::move(change)};
}

// `textDocument` の欄（version があれば `VersionedTextDocumentIdentifier`）。
[[nodiscard]] core::JsonObject document_identifier(const std::string &uri,
                                                   std::optional<std::uint64_t> version)
{
    core::JsonObject identifier;
    identifier.emplace_back("uri", core::JsonValue{uri});
    if (version.has_value())
    {
        identifier.emplace_back("version", count_json(version.value()));
    }
    return identifier;
}

[[nodiscard]] core::JsonValue did_open_params(const std::string &uri, core::Language language,
                                              std::uint64_t version, const core::TextBuffer &text)
{
    core::JsonObject item = document_identifier(uri, std::nullopt);
    item.emplace_back("languageId", core::JsonValue{std::string{core::language_name(language)}});
    item.emplace_back("version", count_json(version));
    item.emplace_back(
        "text", core::JsonValue{text.text_range(core::Offset{0}, core::Offset{text.size_bytes()})});
    core::JsonObject params;
    params.emplace_back("textDocument", core::JsonValue{std::move(item)});
    return core::JsonValue{std::move(params)};
}

[[nodiscard]] core::JsonValue did_change_params(const std::string &uri, std::uint64_t version,
                                                core::JsonArray changes)
{
    core::JsonObject params;
    params.emplace_back("textDocument", core::JsonValue{document_identifier(uri, version)});
    params.emplace_back("contentChanges", core::JsonValue{std::move(changes)});
    return core::JsonValue{std::move(params)};
}

[[nodiscard]] core::JsonValue did_close_params(const std::string &uri)
{
    core::JsonObject params;
    params.emplace_back("textDocument", core::JsonValue{document_identifier(uri, std::nullopt)});
    return core::JsonValue{std::move(params)};
}

// 通知の params の `uri`（届いた診断の文書を引く鍵だけ・中身の検査は core の写しが行う）。
[[nodiscard]] const std::string *notified_uri(const core::LspNotification &notification)
{
    const auto *params = std::get_if<core::JsonObject>(&notification.params.value);
    const auto uri = params != nullptr ? core::json_member(*params, "uri") : std::nullopt;
    return uri.has_value() ? std::get_if<std::string>(&uri.value().get().value) : nullptr;
}

// 古い版 from で届いた診断（ADR 0161 の決定 3）。from の版の本文で位置を写し直し、その版の後の
// 編集（from から末尾までの版の edits）で順にずらす。
[[nodiscard]] std::expected<core::PublishedDiagnostics, core::LspDiagnosticsFailure>
published_since(const core::LspNotification &notification, std::span<const SentRevision> sent,
                core::LspPositionEncoding encoding)
{
    auto published = core::lsp_published_diagnostics(notification, sent.front().text, encoding);
    if (!published)
    {
        return published;
    }
    for (const SentRevision &revision : sent)
    {
        for (const core::Edit &edit : revision.edits)
        {
            published.value().diagnostics =
                core::diagnostics_adjusted(std::move(published.value().diagnostics), edit);
        }
    }
    return published;
}
} // namespace

// ---------------------------------------------------------------- 文書の同期と診断（ADR 0161）

// running の間の apply の終わり（決定 1）。閉じたタブを閉じ、アクティブな文書が送れなくなった・
// URI が変わったら閉じ、この意図の編集を文書ごとに 1 通に束ね、まだ送っていないアクティブな文書を
// 開く。切り替えでは閉じない（サーバーは複数の文書を持てる）。
void EditorController::sync_language_server_documents()
{
    auto &documents = language_server_.documents;
    const auto &ids = state_.autosave_ids();
    for (auto document = documents.begin(); document != documents.end();)
    {
        if (std::ranges::find(ids, document->tab) == ids.end())
        {
            close_language_server_document(*document, true);
            document = documents.erase(document);
            continue;
        }
        ++document;
    }
    const AutosaveId active = ids.at(state_.active_tab());
    const auto uri = language_server_uri();
    const auto opened = [active](const LanguageServerDocument &document)
    { return document.tab == active; };
    const auto found = std::ranges::find_if(documents, opened);
    if (found != documents.end() && (!uri.has_value() || found->uri != uri.value()))
    {
        close_language_server_document(*found, true);
        documents.erase(found);
    }
    for (LanguageServerDocument &document : documents)
    {
        flush_language_server_changes(document);
    }
    if (uri.has_value() && std::ranges::none_of(documents, opened))
    {
        open_language_server_document(active, uri.value());
    }
}

// アクティブな文書を送れるなら、その URI（決定 1）。パスのある文書で、言語が起きている子の言語
// （命令がある）で、本文が上限以下のとき。無題は送らない。
std::optional<std::string> EditorController::language_server_uri() const
{
    const auto &path = state_.document().path;
    if (!path.has_value() ||
        language_server_.language != std::optional<core::Language>{state_.language()} ||
        state_.text().size_bytes() > language_server_document_limit)
    {
        return std::nullopt;
    }
    return document_uri(path.value());
}

void EditorController::open_language_server_document(AutosaveId tab, std::string uri)
{
    auto &link = language_server_;
    const auto version = static_cast<std::uint64_t>(state_.history().revision());
    ports_.language_server.send(core::lsp_notification_body(
        "textDocument/didOpen",
        did_open_params(uri, link.language.value_or(state_.language()), version, state_.text())));
    link.documents.push_back(
        LanguageServerDocument{tab, std::move(uri), {SentRevision{version, state_.text()}}});
}

// この意図の変更を 1 通の `didChange` に束ねる（決定 1）。version は意図の後の revision()。
// 意図の途中で切り替えた文書は脇の束の本文と版で送る。
void EditorController::flush_language_server_changes(LanguageServerDocument &document)
{
    if (document.changes.empty())
    {
        return;
    }
    const auto position = tab_position_of(document.tab);
    if (!position.has_value())
    {
        return;
    }
    const DocumentState *bundle = state_.parked_bundle_at(position.value());
    if (bundle == nullptr && position.value() != state_.active_tab())
    {
        return;
    }
    const auto &text = bundle != nullptr ? bundle->text : state_.text();
    const auto version = static_cast<std::uint64_t>(
        bundle != nullptr ? bundle->history.revision() : state_.history().revision());
    // 意図の中の編集が差し引き 0 なら、再生の畳みが履歴を意図の前へ戻すので版は進まない。本文は
    // 最後に送った版と同じなので、変更も積んだ編集も捨てて送らない（LSP は版の増加を求める）。
    if (version <= document.sent.back().version)
    {
        auto &edits = document.sent.back().edits;
        const auto kept = edits.size() - std::min(edits.size(), document.changes.size());
        edits.erase(std::next(edits.begin(), static_cast<std::ptrdiff_t>(kept)), edits.end());
        document.changes.clear();
        return;
    }
    ports_.language_server.send(core::lsp_notification_body(
        "textDocument/didChange",
        did_change_params(document.uri, version, std::exchange(document.changes, {}))));
    document.sent.push_back(SentRevision{version, text});
    if (document.sent.size() > language_server_kept_revisions)
    {
        document.sent.erase(document.sent.begin());
    }
}

// 閉じた文書の診断は片付ける（その子の診断はもう新しくならない）。閉じたタブなら置く先が無い。
void EditorController::close_language_server_document(const LanguageServerDocument &document,
                                                      bool notify)
{
    if (notify)
    {
        ports_.language_server.send(
            core::lsp_notification_body("textDocument/didClose", did_close_params(document.uri)));
    }
    place_diagnostics(document.tab, {}, 0, core::DiagnosticsCount::complete);
}

void EditorController::close_language_server_documents(bool notify)
{
    for (const LanguageServerDocument &document : language_server_.documents)
    {
        close_language_server_document(document, notify);
    }
    language_server_.documents.clear();
}

// 編集の 1 本（replace・undo・redo）が本文を置く前に呼ぶ（決定 1・3）。診断はマークと同じ規則で
// ずらし、`didOpen` した文書なら変更を積む（送るのは apply の終わりの 1 通）。off では送った文書が
// 無いので積まない。
void EditorController::language_server_edit(const core::Edit &edit)
{
    if (!state_.diagnostics().empty())
    {
        state_ = state_.with_diagnostics(core::diagnostics_adjusted(state_.diagnostics(), edit),
                                         state_.diagnostics_version(), state_.diagnostics_count());
    }
    auto &documents = language_server_.documents;
    if (documents.empty())
    {
        return;
    }
    const AutosaveId active = state_.autosave_ids().at(state_.active_tab());
    const auto found = std::ranges::find_if(documents, [active](const auto &document)
                                            { return document.tab == active; });
    if (found == documents.end())
    {
        return;
    }
    found->changes.push_back(change_json(state_.text(), edit, language_server_.encoding));
    found->sent.back().edits.push_back(edit);
}

// `publishDiagnostics`（決定 2・3・5）。uri で送った文書を引き、届いた版の本文で位置を写す。版が
// 最後に送った版より古ければ、その版の本文で写し直してから、その版の後の編集で順にずらして置く。
// 覚えていない版・送った覚えのない文書の診断は捨てる。
void EditorController::receive_diagnostics(const core::LspNotification &notification)
{
    auto &link = language_server_;
    const std::string *uri = notified_uri(notification);
    const auto found = uri == nullptr
                           ? link.documents.end()
                           : std::ranges::find_if(link.documents, [uri](const auto &document)
                                                  { return document.uri == *uri; });
    if (found == link.documents.end())
    {
        return;
    }
    auto &sent = found->sent;
    auto published = core::lsp_published_diagnostics(notification, sent.back().text, link.encoding);
    if (!published)
    {
        return;
    }
    const std::uint64_t version = published.value().version.value_or(sent.back().version);
    const auto from = std::ranges::find_if(sent, [version](const SentRevision &revision)
                                           { return revision.version == version; });
    if (from == sent.end())
    {
        return;
    }
    if (from != std::prev(sent.end()))
    {
        published = published_since(notification, std::span<const SentRevision>{from, sent.end()},
                                    link.encoding);
    }
    sent.erase(sent.begin(), from);
    if (!published)
    {
        return;
    }
    place_diagnostics(found->tab, std::move(published.value().diagnostics), version,
                      published.value().count);
}

// タブの退避番号で置く先を引く 1 本（決定 5）。アクティブなら欄、脇の読み込み済みの束ならその束。
void EditorController::place_diagnostics(AutosaveId tab, std::vector<core::Diagnostic> diagnostics,
                                         std::uint64_t version, core::DiagnosticsCount count)
{
    const auto position = tab_position_of(tab);
    if (!position.has_value())
    {
        return;
    }
    state_ = position.value() == state_.active_tab()
                 ? state_.with_diagnostics(std::move(diagnostics), version, count)
                 : state_.with_parked_diagnostics(position.value(), std::move(diagnostics), version,
                                                  count);
}

core::DiagnosticCounts EditorController::diagnostic_counts() const
{
    return core::diagnostic_counts(state_.diagnostics(), state_.diagnostics_count());
}

void EditorController::add_diagnostics(EditorFrame &result) const
{
    const auto &diagnostics = state_.diagnostics();
    if (diagnostics.empty())
    {
        return;
    }
    result.diagnostic_counts = diagnostic_counts();
    if (result.command_message.has_value())
    {
        return;
    }
    result.diagnostic_message = core::diagnostic_notice(diagnostics, state_.selection().caret);
    if (result.diagnostic_message.has_value())
    {
        result.guide = core::operation_guide(core::GuideContext::hidden, result.mode);
    }
}
} // namespace nenenib::application
