#include "LspWorkspaceEdits.hpp"

#include "JsonNull.hpp"
#include "LspFormatting.hpp"
#include "LspJsonRead.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::core
{
namespace
{
// 編集 edit の `annotationId` が `needsConfirmation: true` の注釈を指すか。注釈の表が無い・名指しが
// 無い・表に無い名前は確かめの要らない編集。
[[nodiscard]] bool confirms(const JsonObject *annotations, const JsonValue &edit)
{
    const JsonObject *object = json_object_of(edit);
    if (annotations == nullptr || object == nullptr)
    {
        return false;
    }
    const std::string *id = json_text_member(*object, "annotationId");
    const JsonObject *annotation = id != nullptr ? json_object_member(*annotations, *id) : nullptr;
    if (annotation == nullptr)
    {
        return false;
    }
    const auto flag = json_member(*annotation, "needsConfirmation");
    const bool *value = flag.has_value() ? std::get_if<bool>(&flag.value().get().value) : nullptr;
    return value != nullptr && *value;
}

// 1 ファイルの `TextEdit[]`（配列でなければ崩れ）。確かめの要る編集が 1 つでもあれば断る。
[[nodiscard]] std::expected<std::vector<LspTextEdit>, LspWorkspaceEditFailure>
file_edits(const JsonValue &value, const JsonObject *annotations)
{
    const auto *list = std::get_if<JsonArray>(&value.value);
    if (list == nullptr)
    {
        return std::unexpected{LspWorkspaceEditFailure::malformed};
    }
    if (std::ranges::any_of(*list, [annotations](const JsonValue &edit)
                            { return confirms(annotations, edit); }))
    {
        return std::unexpected{LspWorkspaceEditFailure::needs_confirmation};
    }
    auto edits = lsp_text_edits(value);
    if (!edits.has_value())
    {
        return std::unexpected{LspWorkspaceEditFailure::malformed};
    }
    return std::move(edits).value();
}

// ファイルの列へ足す。同じ URI が既にあれば届いた順につなぐ（版は先の項目のまま）。編集の無い
// 項目は足さない。
void add_file(std::vector<LspFileEdits> &files, const std::string &uri,
              std::optional<std::uint64_t> version, std::vector<LspTextEdit> edits)
{
    if (edits.empty())
    {
        return;
    }
    const auto found = std::ranges::find(files, uri, &LspFileEdits::uri);
    if (found != files.end())
    {
        found->edits.insert(found->edits.end(), std::make_move_iterator(edits.begin()),
                            std::make_move_iterator(edits.end()));
        return;
    }
    files.push_back(LspFileEdits{.uri = uri, .version = version, .edits = std::move(edits)});
}

// `OptionalVersionedTextDocumentIdentifier.version`。欠けた欄と null は値なし、0 以上の整数は
// その値、ほかの形は崩れ。
[[nodiscard]] std::expected<std::optional<std::uint64_t>, LspWorkspaceEditFailure>
document_version(const JsonObject &document)
{
    const auto member = json_member(document, "version");
    if (!member.has_value() || std::holds_alternative<JsonNull>(member.value().get().value))
    {
        return std::optional<std::uint64_t>{};
    }
    const auto version = json_count_of(member.value().get());
    if (!version.has_value())
    {
        return std::unexpected{LspWorkspaceEditFailure::malformed};
    }
    return version;
}

// `documentChanges` の 1 項目（`TextDocumentEdit`）。`kind` を持つ項目は resource operation。
[[nodiscard]] std::expected<void, LspWorkspaceEditFailure>
read_document_change(std::vector<LspFileEdits> &files, const JsonValue &entry,
                     const JsonObject *annotations)
{
    const JsonObject *item = json_object_of(entry);
    if (item == nullptr)
    {
        return std::unexpected{LspWorkspaceEditFailure::malformed};
    }
    if (json_member(*item, "kind").has_value())
    {
        return std::unexpected{LspWorkspaceEditFailure::resource_operation};
    }
    const JsonObject *document = json_object_member(*item, "textDocument");
    const std::string *uri = document != nullptr ? json_text_member(*document, "uri") : nullptr;
    const auto edits = json_member(*item, "edits");
    if (uri == nullptr || !edits.has_value())
    {
        return std::unexpected{LspWorkspaceEditFailure::malformed};
    }
    const auto version = document_version(*document);
    if (!version.has_value())
    {
        return std::unexpected{version.error()};
    }
    auto read = file_edits(edits.value().get(), annotations);
    if (!read.has_value())
    {
        return std::unexpected{read.error()};
    }
    add_file(files, *uri, version.value(), std::move(read).value());
    return {};
}

// `documentChanges` の配列。
[[nodiscard]] std::expected<std::vector<LspFileEdits>, LspWorkspaceEditFailure>
document_changes(const JsonValue &value, const JsonObject *annotations)
{
    const auto *list = std::get_if<JsonArray>(&value.value);
    if (list == nullptr)
    {
        return std::unexpected{LspWorkspaceEditFailure::malformed};
    }
    std::vector<LspFileEdits> files;
    for (const JsonValue &entry : *list)
    {
        const auto read = read_document_change(files, entry, annotations);
        if (!read.has_value())
        {
            return std::unexpected{read.error()};
        }
    }
    return files;
}

// `changes`（URI → `TextEdit[]` の object・版なし）。
[[nodiscard]] std::expected<std::vector<LspFileEdits>, LspWorkspaceEditFailure>
uri_changes(const JsonValue &value, const JsonObject *annotations)
{
    const JsonObject *changes = json_object_of(value);
    if (changes == nullptr)
    {
        return std::unexpected{LspWorkspaceEditFailure::malformed};
    }
    std::vector<LspFileEdits> files;
    for (const auto &[uri, edits] : *changes)
    {
        auto read = file_edits(edits, annotations);
        if (!read.has_value())
        {
            return std::unexpected{read.error()};
        }
        add_file(files, uri, std::nullopt, std::move(read).value());
    }
    return files;
}
} // namespace

std::expected<LspWorkspaceEdit, LspWorkspaceEditFailure> lsp_workspace_edit(const JsonValue &result)
{
    if (std::holds_alternative<JsonNull>(result.value))
    {
        return LspWorkspaceEdit{};
    }
    const JsonObject *edit = json_object_of(result);
    if (edit == nullptr)
    {
        return std::unexpected{LspWorkspaceEditFailure::malformed};
    }
    const JsonObject *annotations = json_object_member(*edit, "changeAnnotations");
    const auto changed_documents = json_member(*edit, "documentChanges");
    const auto changed_uris = json_member(*edit, "changes");
    using Files = std::expected<std::vector<LspFileEdits>, LspWorkspaceEditFailure>;
    Files files = changed_documents.has_value()
                      ? document_changes(changed_documents.value().get(), annotations)
                  : changed_uris.has_value() ? uri_changes(changed_uris.value().get(), annotations)
                                             : Files{};
    if (!files.has_value())
    {
        return std::unexpected{files.error()};
    }
    if (files.value().size() > lsp_workspace_edit_file_limit)
    {
        return std::unexpected{LspWorkspaceEditFailure::too_many_files};
    }
    return LspWorkspaceEdit{.files = std::move(files).value()};
}
} // namespace nenenib::core
