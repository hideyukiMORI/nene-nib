#include "EditorController.hpp"

#include "DiagnosticCounts.hpp"
#include "DiagnosticEdits.hpp"
#include "DiagnosticNotice.hpp"
#include "DocumentSync.hpp"
#include "DocumentUri.hpp"
#include "FilePath.hpp"
#include "FileUri.hpp"
#include "JsonNumber.hpp"
#include "JsonValue.hpp"
#include "LanguageServerDocument.hpp"
#include "LanguageServerLink.hpp"
#include "LanguageServerSlot.hpp"
#include "LanguageServers.hpp"
#include "LspDiagnostics.hpp"
#include "LspLanguageId.hpp"
#include "LspMessages.hpp"
#include "LspNotification.hpp"
#include "LspPosition.hpp"
#include "LspPositionEncoding.hpp"
#include "LspPositionJson.hpp"
#include "OperationGuide.hpp"
#include "SaveNotice.hpp"
#include "SentRevision.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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

// 1 編集 → `TextDocumentContentChangeEvent`（決定 1・増分）。range は編集の前の本文 before の
// 上の位置で、core の lsp_position_of の 1 本で写す。
[[nodiscard]] core::JsonValue change_json(const core::TextBuffer &before, const core::Edit &edit,
                                          core::LspPositionEncoding encoding)
{
    const core::Offset end{edit.at.value + edit.removed.size()};
    core::JsonObject range;
    range.emplace_back("start",
                       core::lsp_position_json(core::lsp_position_of(before, edit.at, encoding)));
    range.emplace_back("end",
                       core::lsp_position_json(core::lsp_position_of(before, end, encoding)));
    core::JsonObject change;
    change.emplace_back("range", core::JsonValue{std::move(range)});
    change.emplace_back("text", core::JsonValue{edit.inserted});
    return core::JsonValue{std::move(change)};
}

// 種類 full の `TextDocumentContentChangeEvent`（range の無い全文）。
[[nodiscard]] core::JsonValue whole_change_json(const core::TextBuffer &text)
{
    core::JsonObject change;
    change.emplace_back(
        "text", core::JsonValue{text.text_range(core::Offset{0}, core::Offset{text.size_bytes()})});
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

// `languageId` は core の lsp_language_id の 1 本（仕様の表の名前・`.tsx` `.jsx` は拡張子で）。
[[nodiscard]] core::JsonValue did_open_params(const std::string &uri, std::string_view language_id,
                                              std::uint64_t version, const core::TextBuffer &text)
{
    core::JsonObject item = document_identifier(uri, std::nullopt);
    item.emplace_back("languageId", core::JsonValue{std::string{language_id}});
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

// `didSave` の params（`includeText` なら保存した本文を `text` に）。
[[nodiscard]] core::JsonValue did_save_params(const std::string &uri,
                                              std::optional<std::string> text)
{
    core::JsonObject params;
    params.emplace_back("textDocument", core::JsonValue{document_identifier(uri, std::nullopt)});
    if (text.has_value())
    {
        params.emplace_back("text", core::JsonValue{std::move(text).value()});
    }
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

// running の席ごとの apply の終わり（決定 1・2026-11-20 の追補）。閉じたタブを閉じ、アクティブな
// 文書がこの席で送れなくなった（言語・URI・大きさが変わった・この席が受けなくなった）ら閉じ、この
// 意図の編集を文書ごとに 1 通に束ね、serves（この席がアクティブな文書を受ける）ならまだ送っていない
// アクティブな文書を開く。切り替えでは閉じない（サーバーは複数の文書を持てる・ほかの言語のタブへ
// 移っても閉じない・段 S2）。
void EditorController::sync_language_server_documents(LanguageServerSlot slot, bool serves)
{
    auto &documents = link_of(slot).documents;
    const auto &ids = state_.autosave_ids();
    for (auto document = documents.begin(); document != documents.end();)
    {
        if (std::ranges::find(ids, document->tab) == ids.end())
        {
            close_language_server_document(slot, *document, true);
            document = documents.erase(document);
            continue;
        }
        ++document;
    }
    const AutosaveId active = ids.at(state_.active_tab());
    const auto uri = serves ? language_server_uri(slot) : std::nullopt;
    const auto opened = [active](const LanguageServerDocument &document)
    { return document.tab == active; };
    const auto found = std::ranges::find_if(documents, opened);
    if (found != documents.end() && (!uri.has_value() || found->uri != uri.value()))
    {
        close_language_server_document(slot, *found, true);
        documents.erase(found);
    }
    for (LanguageServerDocument &document : documents)
    {
        flush_language_server_changes(slot, document);
    }
    if (uri.has_value() && std::ranges::none_of(documents, opened))
    {
        open_language_server_document(slot, active, uri.value());
    }
}

// アクティブな文書をこの席へ送れるなら、その URI（決定 1）。パスのある文書で、言語がこの席の子の
// 言語（命令がある）で、本文が上限以下のとき。無題は送らない。
std::optional<std::string> EditorController::language_server_uri(LanguageServerSlot slot) const
{
    const auto &path = state_.document().path;
    if (!path.has_value() || !language_server_sendable() ||
        language_servers_.links.at(slot_index(slot)).language !=
            std::optional<core::Language>{state_.language()})
    {
        return std::nullopt;
    }
    return document_uri(path.value());
}

// 送れる形の規則の 1 本（パスがあり本文が上限以下）。席を譲らせる前にも読む（#1916 のレビュー
// R3）。
bool EditorController::language_server_sendable() const
{
    return state_.document().path.has_value() &&
           state_.text().size_bytes() <= language_server_document_limit;
}

void EditorController::open_language_server_document(LanguageServerSlot slot, AutosaveId tab,
                                                     std::string uri)
{
    auto &link = link_of(slot);
    const auto version = static_cast<std::uint64_t>(state_.history().revision());
    const auto &path = state_.document().path;
    const std::string_view language_id =
        core::lsp_language_id(link.language.value_or(state_.language()),
                              path.has_value() ? path.value().text() : std::string_view{});
    ports_.language_server.send(
        slot,
        core::lsp_notification_body("textDocument/didOpen",
                                    did_open_params(uri, language_id, version, state_.text())));
    link.documents.push_back(
        LanguageServerDocument{tab, std::move(uri), {SentRevision{version, state_.text()}}});
}

// この意図の変更を 1 通の `didChange` に束ねる（決定 1）。version は意図の後の revision()。
// 意図の途中で切り替えた文書は脇の束の本文と版で送る。
void EditorController::flush_language_server_changes(LanguageServerSlot slot,
                                                     LanguageServerDocument &document)
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
    auto changes = std::exchange(document.changes, {});
    if (link_of(slot).abilities.sync == DocumentSync::full)
    {
        changes = {whole_change_json(text)};
    }
    ports_.language_server.send(
        slot,
        core::lsp_notification_body("textDocument/didChange",
                                    did_change_params(document.uri, version, std::move(changes))));
    document.sent.push_back(SentRevision{version, text});
    if (document.sent.size() > language_server_kept_revisions)
    {
        document.sent.erase(document.sent.begin());
    }
}

// 閉じた文書の診断は片付ける（その子の診断はもう新しくならない）。閉じたタブなら置く先が無い。
void EditorController::close_language_server_document(LanguageServerSlot slot,
                                                      const LanguageServerDocument &document,
                                                      bool notify)
{
    if (notify)
    {
        ports_.language_server.send(
            slot,
            core::lsp_notification_body("textDocument/didClose", did_close_params(document.uri)));
    }
    place_diagnostics(document.tab, {}, 0, core::DiagnosticsCount::complete);
}

void EditorController::close_language_server_documents(LanguageServerSlot slot, bool notify)
{
    auto &documents = link_of(slot).documents;
    for (const LanguageServerDocument &document : documents)
    {
        close_language_server_document(slot, document, notify);
    }
    documents.clear();
}

// アクティブな文書を `didOpen` してある席（文書は高々 1 つの席にだけ開く）。子が居なければ欄を
// 1 つ読むだけで値なし。
std::optional<LanguageServerSlot> EditorController::holding_slot() const
{
    if (language_servers_.awake == 0)
    {
        return std::nullopt;
    }
    const AutosaveId active = state_.autosave_ids().at(state_.active_tab());
    const auto &links = language_servers_.links;
    const auto found = std::ranges::find_if(language_server_slots,
                                            [&links, active](LanguageServerSlot slot)
                                            {
                                                return std::ranges::any_of(
                                                    links.at(slot_index(slot)).documents,
                                                    [active](const LanguageServerDocument &document)
                                                    { return document.tab == active; });
                                            });
    return found != language_server_slots.end() ? std::optional{*found} : std::nullopt;
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
    const auto slot = holding_slot();
    if (!slot.has_value())
    {
        return;
    }
    auto &link = link_of(slot.value());
    const AutosaveId active = state_.autosave_ids().at(state_.active_tab());
    const auto found = std::ranges::find_if(link.documents, [active](const auto &document)
                                            { return document.tab == active; });
    // 種類 none のサーバーへは変更を送らない。full は送るときに全文の 1 つへ置き換えるので、
    // 位置を写さずに数だけを積む（ADR 0161 の 2026-11-20 の追補）。
    switch (link.abilities.sync)
    {
    case DocumentSync::none:
        return;
    case DocumentSync::full:
        found->changes.emplace_back();
        break;
    case DocumentSync::incremental:
        found->changes.push_back(change_json(state_.text(), edit, link.encoding));
        break;
    }
    found->sent.back().edits.push_back(edit);
}

// 言語サーバーの URI が同じファイルを指すか（ADR 0161 の 2026-11-20 の追補）。定義の着地と同じ
// 規則の 1 本で、core の file_path_of_uri で両方をパスへ写し（百分率の符号化を解き・ドライブ文字を
// 大文字に）、FilePort::same_file で比べる。写せない URI は文字列の完全一致だけ。
bool EditorController::same_document_uri(std::string_view sent, std::string_view answered) const
{
    if (sent == answered)
    {
        return true;
    }
    const auto left = core::file_path_of_uri(sent);
    const auto right = core::file_path_of_uri(answered);
    return left.has_value() && right.has_value() &&
           ports_.files.same_file(left.value(), right.value());
}

// 保存の後の `didSave`（ADR 0161 の 2026-11-20 の追補）。別のパスへ保存した文書は URI が変わる
// ので送らず、apply の終わりの同期が閉じて開き直す。本文は送った本文（改行の形を写す前）。
void EditorController::language_server_saved(const core::FilePath &path)
{
    const auto slot = holding_slot();
    if (!slot.has_value() || link_of(slot.value()).abilities.save == SaveNotice::none)
    {
        return;
    }
    auto &link = link_of(slot.value());
    const AutosaveId active = state_.autosave_ids().at(state_.active_tab());
    const auto found = std::ranges::find_if(link.documents, [active](const auto &document)
                                            { return document.tab == active; });
    if (found->uri != document_uri(path))
    {
        return;
    }
    flush_language_server_changes(slot.value(), *found);
    ports_.language_server.send(
        slot.value(),
        core::lsp_notification_body(
            "textDocument/didSave",
            did_save_params(found->uri,
                            link.abilities.save == SaveNotice::with_text
                                ? std::optional<std::string>{state_.text().text_range(
                                      core::Offset{0}, core::Offset{state_.text().size_bytes()})}
                                : std::nullopt)));
}

// `publishDiagnostics`（決定 2・3・5）。uri で送った文書を引き、届いた版の本文で位置を写す。版が
// 最後に送った版より古ければ、その版の本文で写し直してから、その版の後の編集で順にずらして置く。
// 覚えていない版・送った覚えのない文書の診断は捨てる。
void EditorController::receive_diagnostics(LanguageServerSlot slot,
                                           const core::LspNotification &notification)
{
    auto &link = link_of(slot);
    const std::string *uri = notified_uri(notification);
    const auto found =
        uri == nullptr ? link.documents.end()
                       : std::ranges::find_if(link.documents, [this, uri](const auto &document)
                                              { return same_document_uri(document.uri, *uri); });
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

// 知らせの欄に出す診断の文の優先はこの 1 か所（ADR 0161 の決定 4 の追補）。ほかの知らせ
// （command_message）とマクロの録画（`recording @a` を隠さない・R3-3）が勝つ。
std::optional<core::DisplayText> EditorController::shown_diagnostic_message() const
{
    const auto &diagnostics = state_.diagnostics();
    if (diagnostics.empty() || command_message().has_value() || recording_name().has_value())
    {
        return std::nullopt;
    }
    return core::diagnostic_notice(diagnostics, state_.selection().caret);
}

bool EditorController::diagnostic_message_shown() const
{
    return shown_diagnostic_message().has_value();
}

void EditorController::add_diagnostics(EditorFrame &result) const
{
    if (state_.diagnostics().empty())
    {
        return;
    }
    result.diagnostic_counts = diagnostic_counts();
    result.diagnostic_message = shown_diagnostic_message();
    if (result.diagnostic_message.has_value())
    {
        result.guide =
            core::operation_guide(core::GuideContext::hidden, result.mode, result.language);
    }
}
} // namespace nenenib::application
