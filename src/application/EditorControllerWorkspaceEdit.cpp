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
#include "EditHistory.hpp"
#include "EditorControllerDetail.hpp"
#include "FilePath.hpp"
#include "FilePort.hpp"
#include "FileUri.hpp"
#include "FormattedSpan.hpp"
#include "FormattingPieces.hpp"
#include "LanguageServerDocument.hpp"
#include "LanguageServerLink.hpp"
#include "LanguageServerPhase.hpp"
#include "LanguageServerSlot.hpp"
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
#include "SaveState.hpp"
#include "SelectionAnchoring.hpp"
#include "ServerEditFile.hpp"
#include "ServerEditOrigin.hpp"
#include "TabReach.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
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

// 言語サーバーの編集（WorkspaceEdit）を当てる 1 本（#2004・ADR 0194 の決定 4・5）。名前の変更と
// クイックフィックスの edit・resolve の
// edit・`workspace/applyEdit`（#2005）が同じ道を通る（ARC-001）。
//
// 検査（何も変えない段を先に済ませ、状態を動かす reach_tab は最後）:
// - 入力の途中でない（server_edit_interrupts）。頼んだ文書と改訂番号が同じ（照合 (a)）。
// - 各ファイルが `file:` の URI で、同じファイルを 2 つの項目が指さない。
// - 開いているタブは、頼んだときの写しの改訂番号と同じ（照合 (b)）。頼んだ席が `didOpen` していた
//   文書は答えの版も今の改訂番号と同じ（照合 (c)）。`didOpen` していなかった文書は、未保存の変更が
//   無いときだけ（サーバーはディスクの本文で数えている）。
// - 頼んだときにタブに在ったファイルは今も同じタブに在り、当て先のタブは頼んだときも同じファイル
//   （写しのパス・displaced_since・#2042）。
// - タブの無いファイルは開く道と同じ読みの port で 4 MiB まで、同期に揃った UTF-8 の本文だけ。
// - まだ読んでいないタブは reach_tab で読む（揃わなければ断る）。
// - まだ文字組みの問いを判定していない本文に巨大な行が無い。各ファイルの編集を formatted_span で
//   1 区間に組める。
// 1 つでも断れば 1 つも当てず、理由の 1 行を出す。
//
// 当てる: 答えの順にファイルへ切り替える（タブがあれば切り替えの 1 本・
// 無ければ開く道の最後の段 place_opened で固定のタブ）。頼んだ席がまだ
// 開いていない文書は、当てる前に今の `didOpen` の 1 本で開く（当てた
// 編集は意図の終わりの `didChange` になる）。replace を 1 回
// （FormattingPieces・1 文書に 1 つの undo の単位）。最後に頼んだタブへ
// 戻り、使った順（Ctrl+Tab）は当てる前の順に戻す（新しく開いたタブは
// 頼んだタブのすぐ後ろ）。ディスクには書かない（未保存のまま残す）。
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

// 頼んだ席が開いていない文書を、サーバーがディスクのバイト列から Nib と同じ位置で数えるか
// （#2011 のレビュー R2・R6 の N2）。BOM なしの UTF-8 だけ。BOM つきは clangd 19.1.5 が 1 行目の
// BOM を 1 文字と数えた（D:/NeNeNib/scripts/20261124/s7a-rename の answers-bom.json）ので断る側。
[[nodiscard]] bool plain_utf8(core::TextEncoding encoding) noexcept
{
    switch (encoding)
    {
    case core::TextEncoding::utf8:
        return true;
    case core::TextEncoding::utf8_bom:
    case core::TextEncoding::shift_jis:
    case core::TextEncoding::utf16_le:
    case core::TextEncoding::utf16_be:
        return false;
    }
    std::unreachable();
}

// 照合 (a)（頼んだ文書と改訂番号）を照らすか。サーバーからの `workspace/applyEdit` には頼んだ文書が
// 無い（#2005・ADR 0194 の決定 4）。
[[nodiscard]] bool requests_document(ServerEditOrigin origin) noexcept
{
    switch (origin)
    {
    case ServerEditOrigin::answer:
        return true;
    case ServerEditOrigin::server_request:
        return false;
    }
    std::unreachable();
}

// 文書の改訂番号と、未保存の変更が無いか（判定は save_state_of の 1 か所）と、文字コード。
[[nodiscard]] std::tuple<std::uint64_t, bool, core::TextEncoding>
revision_and_saved(const core::EditHistory &history, const Document &document)
{
    return {static_cast<std::uint64_t>(history.revision()),
            save_state_of(document, history.content_position()) == core::SaveState::saved,
            document.encoding};
}

// 頼んだときの写し copied（無ければ nullptr）から本文が動いたか。改訂番号が違う、または頼んだ席が
// `didOpen` していた文書で答えの版 version が今の改訂番号と違う。
[[nodiscard]] bool moved_since(const OpenedRevision *copied, std::optional<std::uint64_t> version,
                               std::uint64_t revision) noexcept
{
    if (copied == nullptr)
    {
        return false;
    }
    if (copied->revision != revision)
    {
        return true;
    }
    return copied->sent && version.has_value() && version.value() != revision;
}

// 頼んだ席が頼んだときに `didOpen` していた文書か。
[[nodiscard]] bool sent_by_seat(const OpenedRevision *copied) noexcept
{
    return copied != nullptr && copied->sent;
}

// 頼んだときに `didOpen` していた文書 copied を、席が頼んだ後にその文書だけ閉じたか（`:set ft=`・
// URI が変わった・送れる大きさを超えた）。サーバーはもうバッファの本文を持たないので、照合 (b)(c)
// が通っても照らせない（#2039・レビュー R10 の C1）。documents は答えた席が今開いている文書。
[[nodiscard]] bool closed_by_seat(const OpenedRevision *copied,
                                  const std::vector<LanguageServerDocument> &documents)
{
    return sent_by_seat(copied) &&
           std::ranges::none_of(documents, [copied](const LanguageServerDocument &document)
                                { return document.tab == copied->tab; });
}

// 頼んだ席が `didOpen` していなかった当て先の断り（無ければ値なし・#2011 のレビュー R6 の
// N1・N2）。 サーバーはディスクの本文で数えるので、頼んだときも今も保存済みで、BOM なしの UTF-8
// のときだけ 当てる。写しに無いタブ（頼んだ後に開いた・読んだ）は、読んでから 1
// 度も編集していない（改訂番号が
// 0）ときだけ（保存しても改訂番号は動かないので、保存の状態だけでは読んだ後の変更を言えない）。
[[nodiscard]] std::optional<core::UiMessage> unsent_refusal(const OpenedRevision *copied,
                                                            std::uint64_t revision, bool saved,
                                                            core::TextEncoding encoding) noexcept
{
    if (copied == nullptr && revision != 0)
    {
        return core::UiMessage::notice_server_edit_stale;
    }
    if (!saved || (copied != nullptr && !copied->saved))
    {
        return core::UiMessage::notice_server_edit_unsent;
    }
    return plain_utf8(encoding) ? std::nullopt
                                : std::optional{core::UiMessage::notice_server_edit_not_utf8};
}

// 当て先のファイル path（今の置き場は place）が、頼んだときの写し opened と食い違うか（#2042・
// #2039 の形を含む）。サーバーは頼んだときにタブに在ったファイルをそのタブのバッファの本文で、
// 無かったファイルをディスクの本文で数えた。次のどちらかなら、今の当て先はサーバーが数えた本文
// ではない:
// - 頼んだときにタブに在ったファイルが、今は同じタブに無い（閉じた・閉じて開き直した・名前を
//   付けて保存で別のパスにした）。
// - 当て先のタブが、頼んだときは別のファイル（か無題）だった（名前を付けて保存でこのパスにした・
//   何も書いていない無題に開いた）。
// 写しに無いタブ（頼んだ後に開いた・まだ読んでいない）とタブの無いファイルは、頼んだときにタブに
// 無かったファイルなら食い違わない（そこから先は server_edit_check とディスクの読みの規則）。
[[nodiscard]] bool displaced_since(const std::variant<AutosaveId, UnopenedFile> &place,
                                   const core::FilePath &path,
                                   const std::vector<OpenedRevision> &opened, const FilePort &files)
{
    const auto *tab = std::get_if<AutosaveId>(&place);
    return std::ranges::any_of(opened,
                               [tab, &path, &files](const OpenedRevision &copied)
                               {
                                   const bool same_tab = tab != nullptr && copied.tab == *tab;
                                   const bool same_path =
                                       copied.path.has_value() &&
                                       files.same_file(copied.path.value(), path);
                                   return same_tab != same_path;
                               });
}
} // namespace

// 当てたら当てたタブの退避番号の列（今の本文と同じ結果なら空）、断ったら理由の 1 行（知らせにも
// 置く）。origin が server_request（`workspace/applyEdit`・#2005）なら照合 (a) を飛ばす（ADR 0194
// の 決定
// 4）。当てたタブの列は、クイックフィックスが命令の写しを当てたタブだけ進めるのに使う（#2022 の
// レビュー M1）。
std::expected<std::vector<AutosaveId>, core::DisplayText>
EditorController::apply_workspace_edit(const core::LspWorkspaceEdit &edit,
                                       const LspEditContext &context,
                                       core::LspPositionEncoding encoding, ServerEditOrigin origin)
{
    auto files = server_edit_files(edit, context, origin);
    if (files.has_value())
    {
        auto placed = reach_server_edit_tabs(files.value());
        if (placed.has_value())
        {
            placed = place_server_edit_spans(files.value(), encoding);
        }
        if (!placed.has_value())
        {
            files = std::unexpected{placed.error()};
        }
    }
    if (!files.has_value())
    {
        state_ = state_.with_command_message(files.error());
        return std::unexpected{files.error()};
    }
    // 今の本文と同じ結果だけなら何も変えず、何も知らせない（undo の単位を積まない）。
    if (files.value().empty())
    {
        return std::vector<AutosaveId>{};
    }
    return run_server_edits(std::move(files).value(), context.slot);
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

// 検査の 1 段目（決定 4・5）。入力の途中・照合 (a)、ファイルごとの当て先と照合 (b)(c)。状態は
// 動かさない（まだ読んでいないタブは退避番号のまま置き、reach_server_edit_tabs が後で読む）。
std::expected<std::vector<ServerEditFile>, core::DisplayText>
EditorController::server_edit_files(const core::LspWorkspaceEdit &edit,
                                    const LspEditContext &context, ServerEditOrigin origin)
{
    if (server_edit_interrupts())
    {
        return std::unexpected{ui_notice(core::UiMessage::notice_server_edit_busy)};
    }
    if (requests_document(origin) && server_edit_stale(context.request))
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

// 1 ファイルぶん。同じファイルを指す 2 つ目の項目（URI の書き方が違うものを含む）は、LSP の文では
// 1 つ目を当てた後の本文の位置を指すので、元の本文の位置としてつながずに答え全体を断る。
std::expected<void, core::DisplayText>
EditorController::resolve_server_edit(std::vector<ServerEditFile> &files,
                                      const core::LspFileEdits &file, const LspEditContext &context)
{
    const auto path = core::file_path_of_uri(file.uri);
    if (!path.has_value())
    {
        return std::unexpected{ui_notice(core::UiMessage::notice_server_edit_not_a_file)};
    }
    if (std::ranges::any_of(files, [this, &path](const ServerEditFile &known)
                            { return ports_.files.same_file(known.path, path.value()); }))
    {
        return std::unexpected{ui_notice(core::UiMessage::notice_server_edit_unreadable)};
    }
    auto place = server_edit_place(path.value());
    if (!place.has_value())
    {
        return std::unexpected{place.error()};
    }
    if (displaced_since(place.value(), path.value(), context.opened, ports_.files))
    {
        return std::unexpected{ui_notice(core::UiMessage::notice_server_edit_stale)};
    }
    if (const auto *tab = std::get_if<AutosaveId>(&place.value()))
    {
        const auto checked = server_edit_check(*tab, file, context, path.value());
        if (!checked.has_value())
        {
            return std::unexpected{checked.error()};
        }
    }
    files.push_back(ServerEditFile{.place = std::move(place).value(),
                                   .path = path.value(),
                                   .edits = file.edits,
                                   .span = std::nullopt});
    return {};
}

// 当て先（決定 5）。タブがあればその退避番号（まだ読んでいなくても、ここでは読まない）、無ければ
// 開く道と同じ読みの port で読んだ本文（言語サーバーへ送る上限 4 MiB・裏の読みになったら取り消して
// 断る・無い・読めない・大きすぎる・UTF-8 でないも断る）。
std::expected<std::variant<AutosaveId, UnopenedFile>, core::DisplayText>
EditorController::server_edit_place(const core::FilePath &path)
{
    const auto position = open_tab_of(path);
    if (position.has_value())
    {
        return state_.autosave_ids().at(position.value());
    }
    const auto unreachable = [this, &path]
    {
        return file_notice(core::UiMessage::notice_server_edit_unreachable,
                           state_.display_language(), path);
    };
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
    if (!plain_utf8(finished->document.value().encoding))
    {
        return std::unexpected{file_notice(core::UiMessage::notice_server_edit_not_utf8,
                                           state_.display_language(), path)};
    }
    return UnopenedFile{.path = path, .document = std::move(finished->document).value()};
}

// 開いているタブの照合（決定 4・#2011 のレビュー M1）。まだ読んでいないタブはディスクの本文なので
// 照らさない。読み込み済みのタブは、頼んだときの写しに在れば改訂番号が同じ（照合
// (b)）で、頼んだ席が `didOpen` していた文書なら答えの版も今の改訂番号と同じ（照合 (c)）。席が
// `didOpen` していなかった
// 文書（席が起きる前のタブ・ほかの席の文書・頼んだ後に開いたタブ）は、未保存の変更が無いときだけ
// 当てる（gopls の 0・pyright の null の版は照らさない）。
std::expected<void, core::DisplayText>
EditorController::server_edit_check(AutosaveId tab, const core::LspFileEdits &file,
                                    const LspEditContext &context, const core::FilePath &path) const
{
    const auto position = tab_position_of(tab);
    if (!position.has_value() || state_.unloaded_at(position.value()).has_value())
    {
        return {};
    }
    const DocumentState *bundle = state_.parked_bundle_at(position.value());
    const auto [revision, saved, encoding] =
        bundle != nullptr ? revision_and_saved(bundle->history, bundle->document)
                          : revision_and_saved(state_.history(), state_.document());
    const auto copied = std::ranges::find(context.opened, tab, &OpenedRevision::tab);
    const OpenedRevision *known = copied != context.opened.end() ? &*copied : nullptr;
    if (moved_since(known, file.version, revision) ||
        closed_by_seat(known, language_servers_.links.at(slot_index(context.slot)).documents))
    {
        return std::unexpected{ui_notice(core::UiMessage::notice_server_edit_stale)};
    }
    if (sent_by_seat(known))
    {
        return {};
    }
    const auto refusal = unsent_refusal(known, revision, saved, encoding);
    if (refusal.has_value())
    {
        return std::unexpected{refusal.value() == core::UiMessage::notice_server_edit_stale
                                   ? ui_notice(refusal.value())
                                   : file_notice(refusal.value(), state_.display_language(), path)};
    }
    return {};
}

// 検査の最後の段（#2011 のレビュー R3）。状態を動かさない検査を全部通した後に、まだ読んでいない
// タブだけを reach_tab で読む。揃わなければ断る（読めなかったタブが帯から外れるのは reach_tab の
// もともとの振る舞い・裏の読みになったタブは読みが続く）。
std::expected<void, core::DisplayText>
EditorController::reach_server_edit_tabs(const std::vector<ServerEditFile> &files)
{
    for (const ServerEditFile &file : files)
    {
        const auto reached = reach_server_edit_tab(file);
        if (!reached.has_value())
        {
            return reached;
        }
    }
    return {};
}

// 1 ファイルぶん。まだ読んでいないタブだけを読み、読んだ本文の文字コードを照らす（席が開いていない
// ディスクの本文・N2）。
std::expected<void, core::DisplayText>
EditorController::reach_server_edit_tab(const ServerEditFile &file)
{
    const auto *tab = std::get_if<AutosaveId>(&file.place);
    const auto position = tab != nullptr ? tab_position_of(*tab) : std::nullopt;
    if (!position.has_value() || !state_.unloaded_at(position.value()).has_value())
    {
        return {};
    }
    if (reach_tab(position.value()) != TabReach::reached)
    {
        return std::unexpected{file_notice(core::UiMessage::notice_server_edit_unreachable,
                                           state_.display_language(), file.path)};
    }
    const DocumentState *bundle = state_.parked_bundle_at(position.value());
    if (bundle == nullptr || !plain_utf8(bundle->document.encoding))
    {
        return std::unexpected{file_notice(core::UiMessage::notice_server_edit_not_utf8,
                                           state_.display_language(), file.path)};
    }
    return {};
}

// 当て先の今の本文・当てる前のキャレット・文字組みの問いの段階。アクティブは状態の欄、脇は読み込み
// 済みの束、タブの無いファイルは読んだ本文（キャレットは先頭・問いは未判定）。帯に無い・読み込んで
// いないタブは値なし（#2011 のレビュー L4）。
std::optional<EditorController::ServerEditSource>
EditorController::server_edit_source(const ServerEditFile &file) const
{
    if (const auto *unopened = std::get_if<UnopenedFile>(&file.place))
    {
        return ServerEditSource{&unopened->document.text, core::Offset{0},
                                DisplayInquiry::unjudged};
    }
    const auto *tab = std::get_if<AutosaveId>(&file.place);
    const auto position = tab != nullptr ? tab_position_of(*tab) : std::nullopt;
    if (!position.has_value())
    {
        return std::nullopt;
    }
    if (position.value() == state_.active_tab())
    {
        return ServerEditSource{&state_.text(), state_.selection().caret, state_.display_inquiry()};
    }
    const DocumentState *bundle = state_.parked_bundle_at(position.value());
    if (bundle == nullptr)
    {
        return std::nullopt;
    }
    return ServerEditSource{&bundle->text, bundle->selection.caret, bundle->inquiry};
}

// 検査の 2 段目（決定 5）。各ファイルの本文の上に編集を 1 区間に組む。newText の改行は整形と同じく
// 文書の形へ揃える。写せない位置・重なる編集は断り、今の本文と同じ結果のファイルは列から外す。
// まだ文字組みの問いを判定していない本文（読んだばかりのタブ・タブの無いファイル）に巨大な行が
// あれば断る（当てる途中で表示の問いを立てない）。本文の外の行は整形と同じく末尾へ寄せる。
std::expected<void, core::DisplayText>
EditorController::place_server_edit_spans(std::vector<ServerEditFile> &files,
                                          core::LspPositionEncoding encoding) const
{
    for (ServerEditFile &file : files)
    {
        const auto source = server_edit_source(file);
        if (!source.has_value())
        {
            return std::unexpected{ui_notice(core::UiMessage::notice_server_edit_unreadable)};
        }
        const auto [text, caret, inquiry] = source.value();
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
// 上で組んだ・切り替えと開く道は本文を変えない）。当てたタブ（新しく開いたタブを含む）の退避番号を
// 当てた順に返す。
std::vector<AutosaveId> EditorController::run_server_edits(std::vector<ServerEditFile> files,
                                                           LanguageServerSlot slot)
{
    const auto &ids = state_.autosave_ids();
    const AutosaveId origin = ids.at(state_.active_tab());
    std::vector<AutosaveId> order;
    for (const std::size_t position : state_.recency().order())
    {
        order.push_back(ids.at(position));
    }
    std::vector<AutosaveId> opened;
    std::vector<AutosaveId> edited;
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
        announce_server_edit_target(slot);
        edited.push_back(state_.autosave_ids().at(state_.active_tab()));
        const core::FormattedSpan &span = file.span.value();
        places += span.pieces.size();
        const std::size_t before = state_.history().position();
        replace(span.range, span.text, core::EditBoundary::separate, FormattingPieces{span.pieces});
        remember_formatted_unit(before, span);
        move_caret_to(span.caret, core::SelectionAnchoring::collapse);
    }
    switch_for_server_edit(origin);
    restore_recency(order, origin, opened);
    state_ = state_.with_command_message(
        changed == 1
            ? ui_notice(core::UiMessage::notice_server_edit_applied, {std::to_string(places)})
            : ui_notice(core::UiMessage::notice_server_edit_applied_files,
                        {std::to_string(changed), std::to_string(places)}));
    return edited;
}

// 当てる前に、答えた席 slot（文脈の席・#2011 のレビュー R6 の
// P1）がまだ開いていないアクティブな文書（当て先）を今の `didOpen` の 1 本で 開く（#2011 のレビュー
// M1 の (b)・ADR 0194 の決定 5）。当てる編集は replace の言語サーバーの口が 積み、意図の終わりに
// `didChange` になる。席がこの文書を受ける（同じ言語・同じ root・送れる大きさ）
// ときだけで、ほかの席が開いている文書・受けない文書は開かない（次の頼みでは未保存なら断られる）。
void EditorController::announce_server_edit_target(LanguageServerSlot slot)
{
    if (holding_slot().has_value() || link_of(slot).phase != LanguageServerPhase::running ||
        serving_slot(state_.language()) != std::optional{slot})
    {
        return;
    }
    auto uri = language_server_uri(slot);
    if (!uri.has_value())
    {
        return;
    }
    open_language_server_document(slot, state_.autosave_ids().at(state_.active_tab()),
                                  std::move(uri).value());
}

// 使った順は当てる前の順 order に戻し、新しく開いたタブ opened は頼んだタブ origin のすぐ後ろに
// 置く（切り替えの 1 本が並べ替えた分を戻す・設計席の決め「使った順を動かさない形があればそれ」）。
void EditorController::restore_recency(std::vector<AutosaveId> order, AutosaveId origin,
                                       const std::vector<AutosaveId> &opened)
{
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
