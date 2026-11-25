#include "EditorController.hpp"

#include "AutosaveId.hpp"
#include "CompletionCandidates.hpp"
#include "Document.hpp"
#include "EditHistory.hpp"
#include "JsonValue.hpp"
#include "LanguageAnswers.hpp"
#include "LanguageQueryKind.hpp"
#include "LanguageRename.hpp"
#include "LanguageRenameTarget.hpp"
#include "LanguageServerDocument.hpp"
#include "LanguageServerLink.hpp"
#include "LanguageServerSlot.hpp"
#include "LspDiagnostics.hpp"
#include "LspEditContext.hpp"
#include "LspPosition.hpp"
#include "LspPositionJson.hpp"
#include "LspRequestContext.hpp"
#include "LspRequestKind.hpp"
#include "NoticeLine.hpp"
#include "Offset.hpp"
#include "OpenedRevision.hpp"
#include "RenameAsked.hpp"
#include "SaveState.hpp"
#include "SearchLine.hpp"
#include "ServerAbilities.hpp"
#include "ServerEditOrigin.hpp"
#include "UiMessage.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// 名前の変更（#2004・ADR 0194 の決定 7）。F2・Vim の `grn`・操作
// 「名前を変更」で頼む。席が変えられる所の問い（prepareRename）に答える
// ならその答えの範囲（placeholder があればそれ）を、答えないならキャレットの
// 下の語を初めの文字にして、入力行「新しい名前: 」を開く。Enter で
// `textDocument/rename` を頼み、答えの WorkspaceEdit は apply_workspace_edit
// の 1 本で当てる。LSP がオフ・言語サーバーが動いていない文書の打鍵の道には
// 何も足さない（ADR 0190 の決定 10・D60）。
namespace nenenib::application
{
namespace
{
// キャレットの下の語を探す窓の片側のバイト数（巨大な行でも行を写さない）。
constexpr std::size_t rename_word_window = 256;

// `TextDocumentPositionParams` の textDocument と position。
[[nodiscard]] core::JsonObject document_position(const std::string &uri, core::LspPosition position)
{
    core::JsonObject document;
    document.emplace_back("uri", core::JsonValue{uri});
    core::JsonObject params;
    params.emplace_back("textDocument", core::JsonValue{std::move(document)});
    params.emplace_back("position", core::lsp_position_json(position));
    return params;
}
} // namespace

// 頼めたら真（入力行を開いたか、変えられる所を問うた）。頼めなければ呼び手（accept(QueryLanguage)）
// が「〜に答えません」か「動いていません」を 1 行知らせる。
bool EditorController::request_rename_target()
{
    const auto slot = requestable_slot(&ServerAbilities::rename);
    if (!slot.has_value())
    {
        return false;
    }
    if (!link_of(slot.value()).abilities.prepare_rename)
    {
        open_rename_line(rename_word(), request_context());
        return true;
    }
    send_language_request(slot.value(), LspRequestKind::prepare_rename,
                          core::JsonValue{document_position(active_document_of(slot.value()).uri,
                                                            request_position(slot.value()))},
                          request_context());
    return true;
}

// キャレットの下の語（補完の語の文字の規則・completion_word_around）。行の中のキャレットの前後
// rename_word_window バイトの窓だけを読む。
std::string EditorController::rename_word() const
{
    const core::TextBuffer &text = state_.text();
    const core::Offset caret = state_.selection().caret;
    const core::LineNumber line = text.line_containing(caret);
    const std::size_t line_start = text.line_start(line).value;
    const std::size_t line_end = text.line_end(line).value;
    const std::size_t from =
        std::max(line_start, caret.value - std::min(caret.value, rename_word_window));
    const std::size_t to = std::min(line_end, caret.value + rename_word_window);
    const std::string window = text.text_range(core::Offset{from}, core::Offset{to});
    // 窓の頭で切れた code point の残りを飛ばし、終わりで切れた code point
    // を落とす（#1292・#1322）。 キャレットは code point
    // の境目なので、飛ばした分はキャレットより手前にある。
    const std::size_t skipped = from > line_start ? core::leading_continuations(window) : 0;
    std::string_view content = std::string_view{window}.substr(skipped);
    content = content.substr(0, core::whole_code_points(content));
    const std::size_t at = std::min(caret.value - from - skipped, content.size());
    const auto [begin, end] = core::completion_word_around(content, at);
    return std::string{content.substr(begin, end - begin)};
}

// 変えられる所の答え（決定 7）。文書・改訂番号・キャレットが頼んだときと同じで、入力行・面・変換が
// 開いておらず何も待っていないときだけ使う（ほかは知らせずに捨てる・ADR 0190 の追補 3）。null は
// 「ここは名前を変えられません」、誤りの答えはサーバーの message を訳さずに 1 行で出す。
void EditorController::settle_rename_target_answer()
{
    LanguageAnswers answers = state_.language_answers();
    if (!answers.rename_target.has_value())
    {
        return;
    }
    const LanguageRenameTarget answered = std::move(answers.rename_target).value();
    answers.rename_target.reset();
    state_ = state_.with_language_answers(std::move(answers));
    if (!(answered.context == request_context()) || !faces_closed() || !awaits_nothing())
    {
        return;
    }
    auto initial = rename_initial(answered);
    if (!initial.has_value())
    {
        state_ = state_.with_command_message(std::move(initial).error());
        return;
    }
    open_rename_line(std::move(initial).value(), answered.context);
}

// 変えられる所の答え → 入力行の初めの文字（決定 7）。placeholder
// があればそれ、範囲だけならその範囲の 本文、`defaultBehavior`
// ならキャレットの下の語。変えられない答え・写せない範囲・誤りの答えは断る 1
// 行（誤りの答えはサーバーの message を訳さずに）。
std::expected<std::string, core::DisplayText>
EditorController::rename_initial(const LanguageRenameTarget &answered) const
{
    if (!answered.target.has_value())
    {
        auto line = core::notice_line(answered.server_error);
        return std::unexpected{line.has_value()
                                   ? std::move(line).value()
                                   : ui_notice(core::UiMessage::notice_rename_refused)};
    }
    const auto &target = answered.target.value();
    if (!target.has_value())
    {
        return std::unexpected{ui_notice(core::UiMessage::notice_rename_refused)};
    }
    if (target.value().placeholder.has_value())
    {
        return target.value().placeholder.value();
    }
    if (!target.value().range.has_value())
    {
        return rename_word();
    }
    const auto range =
        core::lsp_range_to_offsets(state_.text(), target.value().range.value(), answered.encoding);
    if (!range.has_value())
    {
        return std::unexpected{ui_notice(core::UiMessage::notice_rename_refused)};
    }
    return state_.text().text_range(range.value().begin, range.value().end);
}

// 入力行「新しい名前: 」を初めの文字 initial で開く（既存の入力行・新しい箱は作らない）。
void EditorController::open_rename_line(std::string initial, const LspRequestContext &context)
{
    state_ = state_.with_command_input(core::SearchLine::opened_rename(initial));
    rename_asked_ = RenameAsked{.context = context, .initial = std::move(initial)};
}

// Enter（決定 7）。入力行を閉じ、空・初めの文字と同じ名前なら頼まない。開いた後に文書が変わって
// いたら頼まずに 1 行知らせる。
void EditorController::submit_rename(const core::SearchLine &line)
{
    const std::string name(line.text());
    close_command_input();
    const auto asked = std::exchange(rename_asked_, std::nullopt);
    if (!asked.has_value() || name.empty() || name == asked.value().initial)
    {
        return;
    }
    if (server_edit_stale(asked.value().context))
    {
        state_ = state_.with_command_message(ui_notice(core::UiMessage::notice_server_edit_stale));
        return;
    }
    request_rename(name, asked.value().context);
}

// `textDocument/rename` を頼む（決定 2・7）。文脈に席が `didOpen`
// している文書ごとの改訂番号の写しを 添える（決定 4 の照合
// (b)(c)）。位置は変えられる所を問うたときのキャレット。
void EditorController::request_rename(const std::string &name, const LspRequestContext &context)
{
    const auto slot = requestable_slot(&ServerAbilities::rename);
    if (!slot.has_value())
    {
        state_ = state_.with_command_message(
            ui_notice(unanswered_notice(core::LanguageQueryKind::rename)));
        return;
    }
    auto params = document_position(
        active_document_of(slot.value()).uri,
        core::lsp_position_of(state_.text(), context.at, link_of(slot.value()).encoding));
    params.emplace_back("newName", core::JsonValue{name});
    send_language_request(slot.value(), LspRequestKind::rename, core::JsonValue{std::move(params)},
                          LspEditContext{.request = context,
                                         .opened = opened_revisions(slot.value()),
                                         .slot = slot.value()});
}

// 帯の読み込み済みのタブごとの今のパス・改訂番号と、頼んだ席 slot が `didOpen` しているか
// （決定 4・#2011 のレビュー M1・パスは #2042）。アクティブは状態の欄、脇は読み込み済みの束から
// 引く。まだ読んでいないタブはディスクの本文なので写さない。
std::vector<OpenedRevision> EditorController::opened_revisions(LanguageServerSlot slot) const
{
    const auto &ids = state_.autosave_ids();
    std::vector<OpenedRevision> opened;
    opened.reserve(ids.size());
    for (std::size_t position = 0; position < ids.size(); ++position)
    {
        auto revision = opened_revision_at(slot, position);
        if (revision.has_value())
        {
            opened.push_back(std::move(revision).value());
        }
    }
    return opened;
}

// 帯の position 番目のタブの今の写し（まだ読んでいないタブは値なし・#2022 のレビュー M1 で 1 タブ
// ずつ進めるために分けた）。
std::optional<OpenedRevision> EditorController::opened_revision_at(LanguageServerSlot slot,
                                                                   std::size_t position) const
{
    const auto &documents = language_servers_.links.at(slot_index(slot)).documents;
    const DocumentState *bundle = state_.parked_bundle_at(position);
    if (bundle == nullptr && position != state_.active_tab())
    {
        return std::nullopt;
    }
    const core::EditHistory &history = bundle != nullptr ? bundle->history : state_.history();
    const Document &document = bundle != nullptr ? bundle->document : state_.document();
    const AutosaveId tab = state_.autosave_ids().at(position);
    return OpenedRevision{
        .tab = tab,
        .path = document.path,
        .revision = static_cast<std::uint64_t>(history.revision()),
        .sent = std::ranges::any_of(documents, [tab](const LanguageServerDocument &sent)
                                    { return sent.tab == tab; }),
        .saved = save_state_of(document, history.content_position()) == core::SaveState::saved};
}

// 名前の変更の答え（決定 7）。誤りの答えはサーバーの message を 1 行で、null・空の答えは「名前を
// 変える所が見つかりません」。WorkspaceEdit は apply_workspace_edit の 1 本で当てる。
void EditorController::settle_rename_answer()
{
    LanguageAnswers answers = state_.language_answers();
    if (!answers.rename.has_value())
    {
        return;
    }
    const LanguageRename answered = std::move(answers.rename).value();
    answers.rename.reset();
    state_ = state_.with_language_answers(std::move(answers));
    if (!answered.server_error.empty())
    {
        auto line = core::notice_line(answered.server_error);
        state_ = state_.with_command_message(
            line.has_value() ? std::move(line).value()
                             : ui_notice(core::UiMessage::notice_server_edit_unreadable));
        return;
    }
    if (answered.edit.has_value() && answered.edit.value().files.empty())
    {
        state_ = state_.with_command_message(ui_notice(core::UiMessage::notice_rename_not_found));
        return;
    }
    if (!answered.edit.has_value())
    {
        state_ = state_.with_command_message(server_edit_refusal(answered.edit.error()));
        return;
    }
    static_cast<void>(apply_workspace_edit(answered.edit.value(), answered.context,
                                           answered.encoding, ServerEditOrigin::answer));
}
} // namespace nenenib::application
