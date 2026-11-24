#include "EditorController.hpp"

#include "AutosaveId.hpp"
#include "DisplayInquiry.hpp"
#include "DisplayQuestion.hpp"
#include "DisplayText.hpp"
#include "Document.hpp"
#include "DocumentReadFinished.hpp"
#include "DocumentReadRequest.hpp"
#include "DocumentState.hpp"
#include "EditBoundary.hpp"
#include "EditorControllerDetail.hpp"
#include "FilePath.hpp"
#include "FileUri.hpp"
#include "FormattedSpan.hpp"
#include "FormattingPieces.hpp"
#include "LanguageServerLink.hpp"
#include "LspEditContext.hpp"
#include "LspFileEdits.hpp"
#include "LspFormatting.hpp"
#include "LspPositionEncoding.hpp"
#include "LspTextEdit.hpp"
#include "LspWorkspaceEdit.hpp"
#include "LspWorkspaceEditFailure.hpp"
#include "LspWorkspaceEdits.hpp"
#include "Offset.hpp"
#include "OpenedRevision.hpp"
#include "SelectionAnchoring.hpp"
#include "ServerEditFile.hpp"
#include "TabReach.hpp"
#include "TextBuffer.hpp"
#include "UiMessage.hpp"
#include "UnopenedFile.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

// 言語サーバーの編集（WorkspaceEdit）を当てる 1 本（#2004・ADR 0194 の決定 4・5）。名前の変更
// （と S7 b のクイックフィックス・`workspace/applyEdit`）が同じ道を通る（ARC-001）。
//
// 検査（何も変えない）: 入力の途中でない（server_edit_interrupts）・頼んだ文書と改訂番号が同じ
// （照合 (a)）・席が `didOpen` していた文書の改訂番号が写しと同じで、答えに版があればその版が今の
// 改訂番号と同じ（照合 (b)(c)）・各ファイルが `file:` の URI
// で本文が手元にある（アクティブ・脇の束・ まだ読んでいないタブは
// reach_tab・タブの無いファイルは開く道と同じ読みの port で 4 MiB まで）・
// まだ文字組みの問いを判定していない本文に巨大な行が無い・各ファイルの編集を formatted_span で
// 1 区間に組める。1 つでも断れば 1 つも当てず、理由の 1 行を出す。
//
// 当てる: 答えの順にファイルへ切り替え（タブがあれば切り替えの 1 本・無ければ開く道の最後の段
// place_opened で固定のタブ）、replace を 1 回（FormattingPieces・1 文書に 1 つの undo の単位）。
// 最後に頼んだタブへ戻り、使った順（Ctrl+Tab）は当てる前の順に戻す（新しく開いたタブは頼んだタブの
// すぐ後ろ）。ディスクには書かない（変えたファイルは未保存のまま）。
namespace nenenib::application
{
namespace
{
// 名前の 1 行（ファイルの名前を置き場 {0} に入れる・長い名前は named_line が切る）。
[[nodiscard]] core::DisplayText file_notice(core::UiMessage message, core::DisplayLanguage language,
                                            const core::FilePath &path)
{
    const std::string name(path.file_name().empty() ? path.text() : path.file_name());
    return detail::named_line(message, language, name, {});
}
} // namespace

void EditorController::apply_workspace_edit(const core::LspWorkspaceEdit &edit,
                                            const LspEditContext &context,
                                            core::LspPositionEncoding encoding)
{
    auto files = server_edit_files(edit, context);
    if (files.has_value())
    {
        const auto placed = place_server_edit_spans(files.value(), encoding);
        if (!placed.has_value())
        {
            files = std::unexpected{placed.error()};
        }
    }
    if (!files.has_value())
    {
        state_ = state_.with_command_message(files.error());
        return;
    }
    // 今の本文と同じ結果だけなら何も変えず、何も知らせない（undo の単位を積まない）。
    if (files.value().empty())
    {
        return;
    }
    run_server_edits(std::move(files).value());
}

// 読みが断った理由 → 1 行（決定 4）。
core::DisplayText EditorController::server_edit_refusal(core::LspWorkspaceEditFailure failure) const
{
    switch (failure)
    {
    case core::LspWorkspaceEditFailure::malformed:
        return ui_notice(core::UiMessage::notice_server_edit_unreadable);
    case core::LspWorkspaceEditFailure::resource_operation:
        return ui_notice(core::UiMessage::notice_server_edit_resource_operation);
    case core::LspWorkspaceEditFailure::needs_confirmation:
        return ui_notice(core::UiMessage::notice_server_edit_needs_confirmation);
    case core::LspWorkspaceEditFailure::too_many_files:
        return ui_notice(core::UiMessage::notice_server_edit_too_many_files,
                         {std::to_string(core::lsp_workspace_edit_file_limit)});
    }
    std::unreachable();
}

// 検査の 1 段目（決定 4・5）。入力の途中・照合 (a)、ファイルごとの当て先と照合
// (b)(c)。同じファイルを 指す項目（URI の書き方が違うものを含む）は 1 つにまとめる。
std::expected<std::vector<ServerEditFile>, core::DisplayText>
EditorController::server_edit_files(const core::LspWorkspaceEdit &edit,
                                    const LspEditContext &context)
{
    if (server_edit_interrupts())
    {
        return std::unexpected{ui_notice(core::UiMessage::notice_server_edit_busy)};
    }
    if (server_edit_stale(context.request))
    {
        return std::unexpected{ui_notice(core::UiMessage::notice_server_edit_stale)};
    }
    std::vector<ServerEditFile> files;
    files.reserve(edit.files.size());
    for (const core::LspFileEdits &file : edit.files)
    {
        const auto resolved = resolve_server_edit(files, file, context);
        if (!resolved.has_value())
        {
            return std::unexpected{resolved.error()};
        }
    }
    return files;
}

std::expected<void, core::DisplayText>
EditorController::resolve_server_edit(std::vector<ServerEditFile> &files,
                                      const core::LspFileEdits &file, const LspEditContext &context)
{
    const auto path = core::file_path_of_uri(file.uri);
    if (!path.has_value())
    {
        return std::unexpected{ui_notice(core::UiMessage::notice_server_edit_not_a_file)};
    }
    auto found = std::ranges::find_if(files, [this, &path](const ServerEditFile &known)
                                      { return ports_.files.same_file(known.path, path.value()); });
    if (found == files.end())
    {
        auto place = server_edit_place(path.value());
        if (!place.has_value())
        {
            return std::unexpected{place.error()};
        }
        files.push_back(ServerEditFile{.place = std::move(place).value(),
                                       .path = path.value(),
                                       .edits = {},
                                       .span = std::nullopt});
        found = std::prev(files.end());
    }
    const auto *tab = std::get_if<AutosaveId>(&found->place);
    if (tab != nullptr && !server_edit_current(*tab, file, context))
    {
        return std::unexpected{ui_notice(core::UiMessage::notice_server_edit_stale)};
    }
    found->edits.insert(found->edits.end(), file.edits.begin(), file.edits.end());
    return {};
}

// 当て先（決定 5）。タブがあればその退避番号（まだ読んでいないタブは reach_tab で読み、揃わなければ
// 断る）、無ければ開く道と同じ読みの port で読んだ本文（言語サーバーへ送る上限 4 MiB・裏の読みに
// なったら取り消して断る・無い・読めない・大きすぎるも断る）。
std::expected<std::variant<AutosaveId, UnopenedFile>, core::DisplayText>
EditorController::server_edit_place(const core::FilePath &path)
{
    const auto unreachable = [this, &path]
    {
        return file_notice(core::UiMessage::notice_server_edit_unreachable,
                           state_.display_language(), path);
    };
    const auto position = open_tab_of(path);
    if (position.has_value())
    {
        const AutosaveId tab = state_.autosave_ids().at(position.value());
        if (reach_tab(position.value()) != TabReach::reached)
        {
            return std::unexpected{unreachable()};
        }
        return tab;
    }
    const std::uint64_t ticket = ++read_tickets_;
    auto outcome =
        ports_.reads.request(DocumentReadRequest{path, ticket, language_server_document_limit});
    auto *finished =
        outcome.has_value() ? std::get_if<DocumentReadFinished>(&outcome.value()) : nullptr;
    if (finished == nullptr || finished->ticket != ticket)
    {
        ports_.reads.cancel(ticket);
        return std::unexpected{unreachable()};
    }
    if (!finished->document.has_value())
    {
        return std::unexpected{unreachable()};
    }
    return UnopenedFile{.path = path, .document = std::move(finished->document).value()};
}

// 照合 (b)(c)（決定 4）。席が頼んだときに `didOpen`
// していた文書なら、改訂番号が写しと同じで、答えに
// 版があればその版も今の改訂番号と同じ。開いていなかった文書の版（gopls の 0・pyright の null）は
// 照らさない。
bool EditorController::server_edit_current(AutosaveId tab, const core::LspFileEdits &file,
                                           const LspEditContext &context) const
{
    const auto copied = std::ranges::find(context.opened, tab, &OpenedRevision::tab);
    if (copied == context.opened.end())
    {
        return true;
    }
    const auto position = tab_position_of(tab);
    if (!position.has_value())
    {
        return false;
    }
    const DocumentState *bundle = state_.parked_bundle_at(position.value());
    if (bundle == nullptr && position.value() != state_.active_tab())
    {
        return false;
    }
    const auto revision = static_cast<std::uint64_t>(
        bundle != nullptr ? bundle->history.revision() : state_.history().revision());
    return revision == copied->revision &&
           (!file.version.has_value() || file.version.value() == revision);
}

// 当て先の今の本文・当てる前のキャレット・文字組みの問いの段階。アクティブは状態の欄、脇は読み込み
// 済みの束、タブの無いファイルは読んだ本文（キャレットは先頭・問いは未判定）。
EditorController::ServerEditSource
EditorController::server_edit_source(const ServerEditFile &file) const
{
    if (const auto *unopened = std::get_if<UnopenedFile>(&file.place))
    {
        return {&unopened->document.text, core::Offset{0}, DisplayInquiry::unjudged};
    }
    const auto *tab = std::get_if<AutosaveId>(&file.place);
    const auto position = tab != nullptr ? tab_position_of(*tab) : std::nullopt;
    const DocumentState *bundle =
        position.has_value() ? state_.parked_bundle_at(position.value()) : nullptr;
    if (bundle != nullptr)
    {
        return {&bundle->text, bundle->selection.caret, bundle->inquiry};
    }
    return {&state_.text(), state_.selection().caret, state_.display_inquiry()};
}

// 検査の 2 段目（決定 5）。各ファイルの本文の上に編集を 1 区間に組む。newText の改行は整形と同じく
// 文書の形へ揃える。写せない位置・重なる編集は断り、今の本文と同じ結果のファイルは列から外す。
// まだ文字組みの問いを判定していない本文（読んだばかりのタブ・タブの無いファイル）に巨大な行が
// あれば断る（当てる途中で表示の問いを立てない）。
std::expected<void, core::DisplayText>
EditorController::place_server_edit_spans(std::vector<ServerEditFile> &files,
                                          core::LspPositionEncoding encoding) const
{
    for (ServerEditFile &file : files)
    {
        const auto [text, caret, inquiry] = server_edit_source(file);
        if (inquiry == DisplayInquiry::unjudged && asks_grid(*text))
        {
            return std::unexpected{file_notice(core::UiMessage::notice_server_edit_giant_line,
                                               state_.display_language(), file.path)};
        }
        const auto edits = detail::edits_with_document_newlines(file.edits, text->line_ending());
        auto span = core::formatted_span(*text, edits, encoding, caret);
        if (!span.has_value())
        {
            return std::unexpected{ui_notice(core::UiMessage::notice_server_edit_unreadable)};
        }
        if (text->text_range(span.value().range.begin, span.value().range.end) != span.value().text)
        {
            file.span = std::move(span);
        }
    }
    std::erase_if(files, [](const ServerEditFile &file) { return !file.span.has_value(); });
    return {};
}

// 当てる（決定 5）。検査を通った後は replace が断る道が無い（入力の途中でなく、各区間は今の本文の
// 上で組んだ・切り替えと開く道は本文を変えない）。
void EditorController::run_server_edits(std::vector<ServerEditFile> files)
{
    const auto &ids = state_.autosave_ids();
    const AutosaveId origin = ids.at(state_.active_tab());
    std::vector<AutosaveId> order;
    for (const std::size_t position : state_.recency().order())
    {
        order.push_back(ids.at(position));
    }
    std::vector<AutosaveId> opened;
    std::size_t places = 0;
    const std::size_t changed = files.size();
    for (ServerEditFile &file : files)
    {
        if (!file.span.has_value())
        {
            continue;
        }
        const bool unopened = std::holds_alternative<UnopenedFile>(file.place);
        enter_server_edit_file(file);
        if (unopened)
        {
            opened.push_back(state_.autosave_ids().at(state_.active_tab()));
        }
        const core::FormattedSpan &span = file.span.value();
        places += span.pieces.size();
        const std::size_t before = state_.history().position();
        replace(span.range, span.text, core::EditBoundary::separate, FormattingPieces{span.pieces});
        remember_formatted_unit(before, span);
        move_caret_to(span.caret, core::SelectionAnchoring::collapse);
    }
    switch_for_server_edit(origin);
    // 使った順は当てる前の順に戻し、新しく開いたタブは頼んだタブのすぐ後ろに置く（切り替えの 1 本が
    // 並べ替えた分を戻す・設計席の決め「使った順を動かさない形があればそれ」）。
    order.erase(std::ranges::remove(order, origin).begin(), order.end());
    order.insert(order.begin(), opened.begin(), opened.end());
    order.insert(order.begin(), origin);
    std::vector<std::size_t> ranks;
    for (const AutosaveId tab : state_.autosave_ids())
    {
        ranks.push_back(
            static_cast<std::size_t>(std::distance(order.begin(), std::ranges::find(order, tab))));
    }
    state_ = state_.with_recency_ranked(ranks);
    state_ = state_.with_command_message(
        changed == 1
            ? ui_notice(core::UiMessage::notice_server_edit_applied, {std::to_string(places)})
            : ui_notice(core::UiMessage::notice_server_edit_applied_files,
                        {std::to_string(changed), std::to_string(places)}));
}

// 当て先へ行く。タブがあれば切り替えの 1 本、無ければ開く道の最後の段（固定のタブ）。
void EditorController::enter_server_edit_file(ServerEditFile &file)
{
    if (const auto *tab = std::get_if<AutosaveId>(&file.place))
    {
        switch_for_server_edit(*tab);
        return;
    }
    auto *unopened = std::get_if<UnopenedFile>(&file.place);
    Document document =
        opened_record(unopened->path, unopened->document.encoding, unopened->document.text);
    place_opened(std::move(unopened->document.text), std::move(document));
}

// 退避番号のタブへ切り替える 1 本（leave_document → with_switched → enter_document）。今のタブなら
// 何もしない。読み込み済みのタブだけを呼ぶ（検査で reach_tab を通した）。
void EditorController::switch_for_server_edit(AutosaveId tab)
{
    const auto position = tab_position_of(tab);
    if (!position.has_value() || position.value() == state_.active_tab())
    {
        return;
    }
    leave_document();
    state_ = state_.with_switched(position.value());
    enter_document();
}
} // namespace nenenib::application
