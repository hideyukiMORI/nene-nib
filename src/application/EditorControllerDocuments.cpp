#include "EditorController.hpp"
#include "EditorControllerDetail.hpp"

#include "AutosavePlan.hpp"
#include "ByteOrder.hpp"
#include "DisplayInquiry.hpp"
#include "DisplayQuestion.hpp"
#include "DisplayText.hpp"
#include "DocumentByteLimit.hpp"
#include "DocumentDecoding.hpp"
#include "DocumentReadRequest.hpp"
#include "DocumentReading.hpp"
#include "DocumentState.hpp"
#include "EditMode.hpp"
#include "EditorState.hpp"
#include "ExLanguageRequest.hpp"
#include "ExResult.hpp"
#include "ExSyntaxRequest.hpp"
#include "FileHistory.hpp"
#include "FileHistoryEdit.hpp"
#include "Language.hpp"
#include "LanguageTable.hpp"
#include "PaletteOrigin.hpp"
#include "ReadingTarget.hpp"
#include "SaveState.hpp"
#include "Selection.hpp"
#include "Session.hpp"
#include "SessionEnd.hpp"
#include "SessionTab.hpp"
#include "SplitLayout.hpp"
#include "TabReach.hpp"
#include "TabRecency.hpp"
#include "TabTitle.hpp"
#include "TextCaret.hpp"
#include "TextPosition.hpp"
#include "UnloadedDocument.hpp"
#include "Utf16.hpp"
#include "VimMarkEdits.hpp"
#include "VimState.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::application
{
using detail::line_within;
using detail::named_line;
using detail::named_notice;
using detail::parked_path;

namespace
{
// 開く本文の上限（ADR 0159 の決定 7）。値の正本は core の 1 か所。
constexpr std::size_t maximum_file_bytes = core::maximum_document_bytes;

[[nodiscard]] std::string_view ex_write_failure(FileFailure failure) noexcept
{
    switch (failure)
    {
    case FileFailure::already_exists:
        return "E13: File exists";
    case FileFailure::unencodable:
        return "Could not write: text cannot be represented in this encoding";
    case FileFailure::not_found:
    case FileFailure::access_denied:
    case FileFailure::unreadable:
    case FileFailure::unwritable:
    case FileFailure::too_large:
    case FileFailure::too_large_to_decode:
    case FileFailure::undecodable:
    case FileFailure::unsupported_utf32:
    case FileFailure::insufficient_memory:
        return "Could not write file";
    }
    std::unreachable();
}

// 帯の 1 本ぶんの覚える値（ADR 0059 の決定 1）。パスの無いタブ（無題）は値なし（D9）。未保存の
// 変更があってもパスがあれば入る。カーソルは行と桁、画面の位置は先頭の行。
[[nodiscard]] std::optional<SessionTab>
session_tab_of(const Document &document, core::TextCaret caret, const ScrollState &scroll)
{
    if (!document.path.has_value())
    {
        return std::nullopt;
    }
    return SessionTab{document.path.value(), caret.position,    scroll.first_visible, 0,
                      scroll.first_row,      scroll.horizontal, caret.affinity};
}

// 脇のタブの覚える値。読み込み済みは束から、まだ読んでいない文書は覚えていた位置のまま
// （ADR 0059 の決定 4）。写し先が足りなければ std::visit が落ちる（CPP-002）。
[[nodiscard]] std::optional<SessionTab>
parked_session_tab(const std::shared_ptr<const DocumentState> &tab)
{
    return session_tab_of(
        tab->document, core::TextCaret{tab->text.position_of(tab->selection.caret), tab->affinity},
        ScrollState{tab->first_visible, 1, tab->first_row, tab->horizontal});
}

[[nodiscard]] std::optional<SessionTab>
parked_session_tab(const std::shared_ptr<const UnloadedDocument> &tab)
{
    return SessionTab{tab->path,      tab->caret,      tab->first_visible, 0,
                      tab->first_row, tab->horizontal, tab->affinity};
}

// 帯の順の各タブの覚える値。アクティブな文書は今の欄から、脇に置いたタブは束から読む。
[[nodiscard]] std::vector<std::optional<SessionTab>> band_session_tabs(const EditorState &state)
{
    const std::optional<SessionTab> active = session_tab_of(
        state.document(),
        core::TextCaret{state.text().position_of(state.selection().caret), state.caret_affinity()},
        state.scroll());
    std::vector<std::optional<SessionTab>> band;
    band.reserve(state.tab_count());
    for (const auto &tab : state.parked())
    {
        if (band.size() == state.active_tab())
        {
            band.push_back(active);
        }
        band.push_back(
            std::visit([](const auto &value) { return parked_session_tab(value); }, tab));
    }
    if (band.size() == state.active_tab())
    {
        band.push_back(active);
    }
    return band;
}

// 使った順の順位を、一覧に入るタブだけで 0 から詰め直す（ADR 0059 の決定 1）。
void rank_session_tabs(std::vector<std::optional<SessionTab>> &band,
                       const core::TabRecency &recency)
{
    std::size_t rank = 0;
    for (const std::size_t position : recency.order())
    {
        if (position >= band.size())
        {
            continue;
        }
        std::optional<SessionTab> &tab = band.at(position);
        if (tab.has_value())
        {
            tab.value().recency = rank;
            ++rank;
        }
    }
}

// 状態から前回のタブの一覧を作る純粋な読み取り（ADR 0059 の決定 1）。active はアクティブなタブの
// 一覧の中の位置で、アクティブが無題なら帯の上で右隣（無ければ左隣）のパスのあるタブ。一覧が
// 空なら 0。
[[nodiscard]] Session session_of(const EditorState &state)
{
    std::vector<std::optional<SessionTab>> band = band_session_tabs(state);
    rank_session_tabs(band, state.recency());
    Session session{{}, 0};
    std::size_t position = 0;
    for (const std::optional<SessionTab> &tab : band)
    {
        if (tab.has_value())
        {
            session.active += position < state.active_tab() ? 1U : 0U;
            session.tabs.push_back(tab.value());
        }
        ++position;
    }
    if (!session.tabs.empty() && session.active == session.tabs.size())
    {
        session.active = session.tabs.size() - 1;
    }
    return session;
}

// 窓が閉じていく理由ごとに覚える一覧（ADR 0059 の決定 3）。最後の 1 本を使う人が閉じたときは空。
// 描画を失って閉じたときは窓を閉じたときと同じ（ADR 0133 の決定 1）。
[[nodiscard]] Session ended_session(const EditorState &state, SessionEnd reason)
{
    switch (reason)
    {
    case SessionEnd::window_closed:
    case SessionEnd::rendering_lost:
        return session_of(state);
    case SessionEnd::last_tab_closed:
        return Session{{}, 0};
    }
    std::unreachable();
}

// 前回のタブ 1 本を、まだ読んでいない文書にする（ADR 0059 の決定 4・6）。符号化は読むまで
// 分からないので既定の UTF-8（見えるのはアクティブの分だけ）。未保存ではない。
[[nodiscard]] UnloadedDocument unloaded_document_of(const SessionTab &tab)
{
    return UnloadedDocument{tab.path,
                            tab.caret,
                            tab.first_visible,
                            DocumentView{core::tab_title_for(tab.path, core::SaveState::saved),
                                         tab.path, core::TextEncoding::utf8, core::SaveState::saved,
                                         std::nullopt},
                            tab.first_row,
                            tab.horizontal,
                            tab.affinity};
}

// 読んだ本文と、まだ読んでいない文書が覚えていた位置から束を作る（決定 5）。カーソルは行を寄せて
// から桁を行の中の文字の境目へ寄せる（offset_of は行の終わりで止まり、コードポイントの先頭にしか
// 着かない）。履歴は空、Vim のマークは戻したキャレットの 1 項だけ（ADR 0152 の追補 #1116 /
// #1124）。
[[nodiscard]] DocumentState reached_bundle(core::TextBuffer text, Document document,
                                           const UnloadedDocument &tab)
{
    const core::Offset caret =
        text.offset_of(core::TextPosition{line_within(text, tab.caret.line), tab.caret.column});
    const core::VimMarkPosition opened = core::vim_mark_position_of(text, caret);
    const core::LineNumber first_visible = line_within(text, tab.first_visible);
    DocumentView view{tab.view.title, document.path, document.encoding, core::SaveState::saved,
                      std::nullopt};
    // 言語は読んだこのときにパスから決める（ADR 0154 の決定 5・一覧には言語を書かない）。
    const core::Language language = language_of(document);
    DocumentState bundle{std::move(text),
                         core::collapsed_at(caret),
                         core::EditHistory::empty(),
                         std::move(document),
                         first_visible,
                         std::nullopt,
                         std::nullopt,
                         std::move(view),
                         tab.first_row,
                         tab.horizontal,
                         tab.affinity};
    bundle.language = language;
    // 戻したキャレットをジャンプの列に 1 項持って始める（viminfo の `'"` で位置を戻した Vim の列の
    // 1 項・ADR 0152 の追補 #1124）。
    bundle.marks = core::vim_marks_opened(opened);
    return bundle;
}

// UTF-8 の本文の断片。BOM つきは先頭に BOM を置く（保存と自動退避の共通・ADR 0137 の決定 4）。
[[nodiscard]] std::vector<std::string_view> utf8_parts(const core::TextBuffer &text,
                                                       core::TextEncoding encoding)
{
    auto parts = text.segments();
    if (encoding == core::TextEncoding::utf8_bom)
    {
        parts.insert(parts.begin(), core::byte_order_mark());
    }
    return parts;
}

// UTF-16 の本文の断片。BOM と、held に写した UTF-16 のバイト列（ADR 0149 の決定 3）。本文は
// 正しい UTF-8 なので写しは失敗しないが、失敗は開けない列と同じに返す。
[[nodiscard]] std::expected<std::vector<std::string_view>, FileFailure>
utf16_parts(const core::TextBuffer &text, std::string_view mark, core::ByteOrder order,
            std::string &held)
{
    auto converted = core::utf16_bytes_of_utf8(text.text(), order);
    if (!converted)
    {
        return std::unexpected(FileFailure::unencodable);
    }
    held = std::move(converted).value();
    return std::vector<std::string_view>{mark, held};
}

constexpr std::string_view unreached_prefix = "開けませんでした: ";
constexpr std::string_view reading_prefix = "読み込み中: ";

// 読み込み中の要求の列の中で券の位置（ADR 0159 の決定 5）。無ければ値なし。
[[nodiscard]] std::optional<std::size_t> reading_index(const std::vector<DocumentReading> &readings,
                                                       std::uint64_t ticket)
{
    const auto found = std::ranges::find(readings, ticket, &DocumentReading::ticket);
    if (found == readings.end())
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(readings.begin(), found));
}

[[nodiscard]] std::vector<DocumentReading> without_ticket(std::vector<DocumentReading> readings,
                                                          std::uint64_t ticket)
{
    std::erase_if(readings,
                  [ticket](const DocumentReading &reading) { return reading.ticket == ticket; });
    return readings;
}

[[nodiscard]] std::vector<DocumentReading> with_appended(std::vector<DocumentReading> readings,
                                                         DocumentReading reading)
{
    readings.push_back(std::move(reading));
    return readings;
}

// 開く要求か（まだ読んでいないタブの読みでないか・追補 1）。
// Vim の待ち（回数・オペレータ・次の 1 鍵・選んだレジスタ）の最中か（ADR 0159 の追補 5）。待ちを
// 終える鍵の意図の終わりで留めた結果を採る。
[[nodiscard]] bool vim_waiting(const EditorState &state) noexcept
{
    const core::VimState &vim = state.vim();
    return state.mode() == core::EditMode::vim &&
           (vim.count.has_value() || vim.pending.has_value() || vim.input_wait.has_value() ||
            vim.selected_register.has_value());
}

[[nodiscard]] bool opens(const DocumentReading &reading) noexcept
{
    return std::holds_alternative<OpenFailureReply>(reading.target);
}

[[nodiscard]] core::DisplayText unreached_message(const core::FilePath &path, std::size_t others)
{
    return named_notice(unreached_prefix,
                        std::string(path.file_name().empty() ? path.text() : path.file_name()),
                        others);
}

// パスのあるタブの全部を、使った順の古いほうから並べる（最後に見ていたタブが末尾）。順位は 0 が
// いちばん最近。std::stable_sort は ARC-003 に落ちるので std::sort（順位は rank_session_tabs が
// 詰め直した一意の値）。
[[nodiscard]] std::vector<core::FilePath> oldest_first_paths(const EditorState &state)
{
    std::vector<SessionTab> tabs = session_of(state).tabs;
    std::ranges::sort(tabs, [](const SessionTab &left, const SessionTab &right)
                      { return left.recency > right.recency; });
    std::vector<core::FilePath> paths;
    paths.reserve(tabs.size());
    for (const SessionTab &tab : tabs)
    {
        paths.push_back(tab.path);
    }
    return paths;
}

// 窓が閉じていくときに履歴へ記録するパス（ADR 0060 の決定 8）。記録の順に並べる。window_closed は
// パスのあるタブの全部を使った順の古いほうから（最後に見ていたタブが履歴の先頭になる）、
// last_tab_closed は閉じたその 1 本（アクティブな文書）。
[[nodiscard]] std::vector<core::FilePath> ended_paths(const EditorState &state, SessionEnd reason)
{
    switch (reason)
    {
    case SessionEnd::window_closed:
    case SessionEnd::rendering_lost:
        return oldest_first_paths(state);
    case SessionEnd::last_tab_closed:
    {
        const auto &path = state.document().path;
        return path.has_value() ? std::vector{path.value()} : std::vector<core::FilePath>{};
    }
    }
    std::unreachable();
}
} // namespace

// 一覧が無い（初めての起動）なら何も知らせない。読めなければ無題 1 本のまま 1 行知らせる
// （ADR 0059 の決定 6）。
void EditorController::restore_session()
{
    const auto session = ports_.session.read();
    if (!session)
    {
        state_ = state_.with_command_message(
            core::DisplayText::parse("前回のタブを読めませんでした").value());
        return;
    }
    const std::optional<Session> &listed = session.value();
    if (!listed.has_value() || listed.value().tabs.empty())
    {
        return;
    }
    restore_tabs(listed.value());
}

// 手順（ADR 0059 の決定 6）: 無題 1 本の右に一覧を並べ、active を reach_tab
// で読んで切り替え、最初の 無題を外し、使った順を順位から作る。active
// が読めなければ外れた後の右隣（無ければ左隣）を 試す。読めたタブが 1
// 本も無ければ無題が残る。active が範囲の外なら最後のタブから試す。
void EditorController::restore_tabs(const Session &session)
{
    std::vector<UnloadedDocument> documents;
    std::vector<std::size_t> ranks;
    documents.reserve(session.tabs.size());
    ranks.reserve(session.tabs.size());
    for (const SessionTab &tab : session.tabs)
    {
        documents.push_back(unloaded_document_of(tab));
        ranks.push_back(tab.recency);
    }
    state_ = state_.with_restored(std::move(documents));
    // 帯の位置は一覧の位置 + 1（最初の無題の右）。ranks の添字は帯の位置 - 1。
    std::size_t position = std::min(session.active, session.tabs.size() - 1) + 1;
    while (state_.tab_count() > 1)
    {
        switch (reach_tab(position))
        {
        case TabReach::reached:
            leave_document();
            state_ = state_.with_switched(position).with_closed(0).with_recency_ranked(ranks);
            enter_document();
            return;
        case TabReach::reading:
            // 裏で読んでいる間は無題を仮に残し、揃ってそのタブへ切り替えたときに外す（追補 1）。
            restore_untitled_ = state_.autosave_ids().at(0);
            restore_ranks_ = std::move(ranks);
            reach_wanted_ = state_.autosave_ids().at(position);
            return;
        case TabReach::dropped:
            break;
        }
        ranks.erase(std::next(ranks.begin(), static_cast<std::ptrdiff_t>(position - 1)));
        position = position < state_.tab_count() ? position : position - 1;
    }
}

void EditorController::fail(FileFailure failure)
{
    state_ = state_.with_failure(failure);
}

// 今の値を controller が答える Ex（言語・構文の色・クリップボードの同期）。どれかなら置いて今の値を
// 1 行知らせ、真を返す。
bool EditorController::answer_current_value(const core::ExResult &result)
{
    if (result.language.has_value())
    {
        answer_language(result.language.value());
        return true;
    }
    if (result.syntax.has_value())
    {
        answer_syntax(result.syntax.value());
        return true;
    }
    if (result.clipboard.has_value())
    {
        answer_clipboard(result.clipboard.value());
        return true;
    }
    if (result.lsp.has_value())
    {
        answer_lsp(result.lsp.value());
        return true;
    }
    return false;
}

void EditorController::answer_language(const core::ExLanguageRequest &request)
{
    if (request.language.has_value())
    {
        accept(SelectLanguage{request.language.value()});
    }
    state_ = state_.with_command_message(
        core::DisplayText::parse("filetype=" + std::string(core::language_name(state_.language())))
            .value());
}

// `:syntax on|off` は意図 SelectSyntax で置き、どの形も今の値を 1 行で知らせる（ADR 0156 の追補）。
void EditorController::answer_syntax(const core::ExSyntaxRequest &request)
{
    if (request.enabled.has_value())
    {
        accept(SelectSyntax{request.enabled.value()});
    }
    state_ = state_.with_command_message(
        core::DisplayText::parse(state_.syntax_enabled() ? "syntax=on" : "syntax=off").value());
}

// `:set clipboard=` は無名と OS の同期を Vim の状態に置き（保存しない）、どの形も今の値を 1 行で
// 知らせる（ADR 0051 の追補（#848）の決定 A・B）。
void EditorController::answer_clipboard(core::ExClipboardRequest request)
{
    core::VimState vim = state_.vim();
    switch (request)
    {
    case core::ExClipboardRequest::enable:
        vim.clipboard_unnamed = true;
        break;
    case core::ExClipboardRequest::disable:
        vim.clipboard_unnamed = false;
        break;
    case core::ExClipboardRequest::show:
        break;
    }
    const bool unnamed = vim.clipboard_unnamed;
    state_ = state_.with_vim(std::move(vim));
    state_ = state_.with_command_message(
        core::DisplayText::parse(unnamed ? "clipboard=unnamed" : "clipboard=").value());
}

bool EditorController::write_ex_document(const core::ExDocumentRequest &request)
{
    const auto path = request.path.has_value() ? request.path : state_.document().path;
    if (!path.has_value())
    {
        reject(core::DisplayText::parse("E32: No file name").value());
        return false;
    }
    const auto target = request.path.has_value()
                            ? ports_.files.resolve(path.value())
                            : std::expected<core::FilePath, FileFailure>{path.value()};
    if (!target)
    {
        reject(core::DisplayText::parse(ex_write_failure(target.error())).value());
        return false;
    }
    return write_ex_path(target.value(), request);
}

bool EditorController::write_ex_path(const core::FilePath &path,
                                     const core::ExDocumentRequest &request)
{
    const auto other = open_tab_of(path);
    if (other.has_value() && other.value() != state_.active_tab())
    {
        reject(core::DisplayText::parse("E139: File is open in another tab").value());
        return false;
    }
    const auto &current = state_.document().path;
    const bool same = current.has_value() && ports_.files.same_file(current.value(), path);
    // `:w! <file>` `:saveas! <file>` は保存と同じ置き換えの道で既存を上書きする（Issue #738）。
    const auto mode = request.path.has_value() && !same && !request.overwrite
                          ? FileWriteMode::create_new
                          : FileWriteMode::replace;
    const auto identity =
        current.has_value() && !same && request.verb != core::ExDocumentVerb::save_as
            ? SaveIdentity::retain_document
            : SaveIdentity::update_document;
    const auto written =
        save_document(SaveDocument{path, state_.document().encoding, mode, identity});
    if (!written)
    {
        reject(core::DisplayText::parse(ex_write_failure(written.error())).value());
        return false;
    }
    state_ = state_.with_command_message(core::DisplayText::parse("Written").value());
    return true;
}

bool EditorController::refuse_modified_quit()
{
    if (active_document_view().save_state != core::SaveState::modified)
    {
        return false;
    }
    reject(core::DisplayText::parse("E37: No write since last change (add ! to override)").value());
    return true;
}

void EditorController::run_document_request(const core::ExDocumentRequest &request)
{
    const bool changed = active_document_view().save_state == core::SaveState::modified;
    switch (request.verb)
    {
    case core::ExDocumentVerb::write:
    case core::ExDocumentVerb::save_as:
        static_cast<void>(write_ex_document(request));
        return;
    case core::ExDocumentVerb::quit:
        break;
    case core::ExDocumentVerb::quit_force:
        break;
    case core::ExDocumentVerb::write_quit:
        if (!write_ex_document(request))
        {
            return;
        }
        break;
    case core::ExDocumentVerb::update_quit:
        if (changed && !write_ex_document(request))
        {
            return;
        }
        break;
    }
    // 2 枠では操作中の枠を閉じる（もう 1 つの枠の表示を持ってきてから 1 枠へ）。文書は残るので
    // 未保存でも拒否しない（ADR 0148 の決定 5・hide の決定を待つ仮置き）。
    if (state_.split().layout != core::SplitLayout::none)
    {
        close_operated_pane();
        return;
    }
    if (request.verb != core::ExDocumentVerb::quit_force && refuse_modified_quit())
    {
        return;
    }
    accept(CloseTab{state_.active_tab()});
}

std::expected<core::TextBuffer, FileFailure> EditorController::decoded(core::DetectedText text)
{
    return decoded_text(ports_.code_pages, std::move(text));
}

std::expected<std::vector<std::string_view>, FileFailure>
EditorController::encoded_parts(const core::TextBuffer &text, core::TextEncoding encoding,
                                std::string &held)
{
    switch (encoding)
    {
    case core::TextEncoding::utf8:
    case core::TextEncoding::utf8_bom:
        return utf8_parts(text, encoding);
    case core::TextEncoding::utf16_le:
        return utf16_parts(text, core::utf16_le_byte_order_mark(), core::ByteOrder::little, held);
    case core::TextEncoding::utf16_be:
        return utf16_parts(text, core::utf16_be_byte_order_mark(), core::ByteOrder::big, held);
    case core::TextEncoding::shift_jis:
        break;
    }
    auto converted = ports_.code_pages.from_utf8(text.text());
    if (!converted)
    {
        return std::unexpected(file_failure_of(converted.error()));
    }
    held = std::move(converted).value();
    return std::vector<std::string_view>{held};
}

std::expected<void, FileFailure> EditorController::write_document(const SaveDocument &intent)
{
    std::string held;
    const auto parts = encoded_parts(state_.text(), intent.encoding, held);
    if (!parts)
    {
        return std::unexpected(parts.error());
    }
    return ports_.files.write(intent.path, parts.value(), intent.mode);
}

// 同じファイルを開いているタブの帯の位置（決定 5 の (a)）。比べ方は FilePort が OS の規則で決める。
std::optional<std::size_t> EditorController::open_tab_of(const core::FilePath &path) const
{
    const auto &active = state_.document().path;
    if (active.has_value() && ports_.files.same_file(active.value(), path))
    {
        return state_.active_tab();
    }
    const auto &parked = state_.parked();
    for (std::size_t index = 0; index < parked.size(); ++index)
    {
        const std::optional<core::FilePath> other =
            std::visit([](const auto &value) { return parked_path(value); }, parked.at(index));
        if (other.has_value() && ports_.files.same_file(other.value(), path))
        {
            // 脇の束は帯の位置からアクティブを抜いた順。アクティブより右は 1 つずれる。
            return index < state_.active_tab() ? index : index + 1;
        }
    }
    return std::nullopt;
}

// 何も書いていない無題（決定 5 の (b)）。やり直しも含めて編集が 1 つも無いこと。
bool EditorController::blank_untitled() const
{
    return !state_.document().path.has_value() && state_.text().size_bytes() == 0 &&
           state_.history().size() == 0;
}

// 開く（ADR 0056 の決定 5・ADR 0159 の決定 1・3〜5・追補 4）。(a) 同じファイルのタブへ切り替える
// （読み直さない）→ 読みの port に頼む。同じファイルを読み込み中なら 2 本目を頼まず今の要求を
// 続ける。使う人の開く意図はほかの開く要求を取り消し、起動の引数は券ごとに並べて順に採る。
// 揃ったら (b) 何も書いていない無題に開く → (c) アクティブの右に新しいタブを足して開く
// （open_read）。開けなかったらタブを足さず、呼び手の告げ方で告げる。
void EditorController::open_document(const core::FilePath &path, OpenFailureReply reply)
{
    const auto &readings = state_.readings();
    if (std::ranges::any_of(
            readings, [&](const DocumentReading &reading)
            { return opens(reading) && ports_.files.same_file(reading.path, path); }))
    {
        return;
    }
    if (!starting_)
    {
        cancel_readings(true);
    }
    const auto open = open_tab_of(path);
    if (open.has_value())
    {
        accept(SwitchTab{open.value()});
        return;
    }
    // ファイルが変わる途中の変換は捨てる（ADR 0014 の決定 3）。
    state_ = state_.with_composition(std::nullopt);
    const std::uint64_t ticket = ++read_tickets_;
    // 要求を先に置き、同期に揃った結果も裏で揃った結果と同じ adopt_read の 1 本で採る（決定 3）。
    state_ = state_.with_readings(
        with_appended(state_.readings(), DocumentReading{path, ticket, 0, 0, reply}));
    auto outcome = ports_.reads.request(DocumentReadRequest{path, ticket, maximum_file_bytes});
    if (outcome.has_value())
    {
        adopt_read(std::move(outcome).value());
    }
    settle_reads();
}

void EditorController::adopt_read(DocumentReadOutcome outcome)
{
    std::visit(
        [this](auto &part)
        {
            using Part = std::remove_cvref_t<decltype(part)>;
            if constexpr (std::is_same_v<Part, DocumentReadProgress>)
            {
                adopt_progress(part);
            }
            else
            {
                static_assert(std::is_same_v<Part, DocumentReadFinished>);
                adopt_finished(std::move(part));
            }
        },
        outcome);
}

void EditorController::adopt_progress(const DocumentReadProgress &progress)
{
    const auto index = reading_index(state_.readings(), progress.ticket);
    if (!index.has_value())
    {
        return;
    }
    auto readings = state_.readings();
    DocumentReading &reading = readings.at(index.value());
    reading.read_bytes = progress.read_bytes;
    reading.total_bytes = progress.total_bytes;
    state_ = state_.with_readings(std::move(readings));
}

// 券が読み込み中の要求に無い結果は捨てる（ADR 0127 §3 の一回確定・決定 5）。揃った結果は
// 採る瞬間まで留める（追補 2）。
void EditorController::adopt_finished(DocumentReadFinished finished)
{
    if (!reading_index(state_.readings(), finished.ticket).has_value())
    {
        return;
    }
    arrived_reads_.push_back(std::move(finished));
}

std::optional<DocumentReadFinished> EditorController::take_arrived(std::uint64_t ticket)
{
    const auto found = std::ranges::find(arrived_reads_, ticket, &DocumentReadFinished::ticket);
    if (found == arrived_reads_.end())
    {
        return std::nullopt;
    }
    DocumentReadFinished finished = std::move(*found);
    arrived_reads_.erase(found);
    return finished;
}

// Ctrl+Tab の歩き・入力行（`:` と検索）・Ctrl+P の面・IME の変換中・Vim の待ちの最中は揃った結果を
// 採らない（追補 2・5）。録画（`q`）は窓の値で文書に依らないので留めない。
bool EditorController::reads_held() const noexcept
{
    return state_.tab_walking() || state_.command_input().has_value() ||
           state_.composition().has_value() || vim_waiting(state_);
}

// 留めた結果を頼んだ順に採る（追補 2・4）。開く要求は先に頼んだ開く要求が揃うまで待ち、
// まだ読んでいないタブの読みは揃った順に採る。採るたびに状態が動くので、券で引き直す。
void EditorController::settle_reads()
{
    if (arrived_reads_.empty() || reads_held())
    {
        return;
    }
    std::vector<std::uint64_t> tickets;
    tickets.reserve(state_.readings().size());
    for (const DocumentReading &reading : state_.readings())
    {
        tickets.push_back(reading.ticket);
    }
    bool open_waiting = false;
    for (const std::uint64_t ticket : tickets)
    {
        open_waiting = settle_read(ticket, open_waiting);
    }
}

// 券 1 つぶんを採る。open_waiting は先に頼んだ開く要求がまだ揃っていないか。返すのは次の券に
// 渡す open_waiting。
bool EditorController::settle_read(std::uint64_t ticket, bool open_waiting)
{
    const auto index = reading_index(state_.readings(), ticket);
    if (!index.has_value() || reads_held())
    {
        return open_waiting;
    }
    const DocumentReading reading = state_.readings().at(index.value());
    if (opens(reading) && open_waiting)
    {
        return open_waiting;
    }
    auto finished = take_arrived(ticket);
    if (!finished.has_value())
    {
        return open_waiting || opens(reading);
    }
    state_ = state_.with_readings(without_ticket(state_.readings(), ticket));
    std::visit([&](const auto &target)
               { finish_reading(reading.path, target, std::move(finished.value().document)); },
               reading.target);
    return open_waiting;
}

void EditorController::finish_reading(const core::FilePath &path, OpenFailureReply reply,
                                      std::expected<OpenedDocument, FileFailure> document)
{
    if (!document)
    {
        answer_unopened(path, document.error(), reply);
        return;
    }
    open_read(path, std::move(document).value());
}

void EditorController::finish_reading(const core::FilePath &, AutosaveId tab,
                                      std::expected<OpenedDocument, FileFailure> document)
{
    finish_tab_read(tab, std::move(document));
}

// まだ読んでいないタブの裏の読みが揃った（追補 1・6）。読めたら束に置き換え、そのタブが待っている
// 切り替え先（最後に指されたタブ）のときだけ切り替える。歩きで飛ばしたタブは置き換えるだけ。読め
// なければ帯から外して 1 行知らせる。起動の復元で無題が仮に残っていれば、切り替えた後に外す。
void EditorController::finish_tab_read(AutosaveId tab,
                                       std::expected<OpenedDocument, FileFailure> document)
{
    const auto position = tab_position_of(tab);
    if (!position.has_value())
    {
        return;
    }
    const bool wanted = reach_wanted_.has_value() && reach_wanted_.value() == tab;
    const std::optional<AutosaveId> placeholder = std::exchange(restore_untitled_, std::nullopt);
    const bool replaces = placeholder.has_value() &&
                          state_.autosave_ids().at(state_.active_tab()) == placeholder.value() &&
                          blank_untitled();
    if (loaded_tab(position.value(), std::move(document)) != TabReach::reached || !wanted)
    {
        return;
    }
    accept(SwitchTab{position.value()});
    if (!replaces)
    {
        return;
    }
    const auto untitled = tab_position_of(placeholder.value());
    if (untitled.has_value() && untitled.value() != state_.active_tab())
    {
        state_ = state_.with_closed(untitled.value()).with_recency_ranked(restore_ranks_);
    }
}

std::optional<std::size_t> EditorController::tab_position_of(AutosaveId tab) const
{
    const auto &ids = state_.autosave_ids();
    const auto found = std::ranges::find(ids, tab);
    if (found == ids.end())
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(ids.begin(), found));
}

void EditorController::open_read(const core::FilePath &path, OpenedDocument opened)
{
    const auto open = open_tab_of(path);
    if (open.has_value())
    {
        accept(SwitchTab{open.value()});
        return;
    }
    state_ = state_.with_composition(std::nullopt);
    Document document{path, opened.encoding, std::size_t{0}};
    if (blank_untitled())
    {
        drop_pending_follow();
        settle_follow();
        state_ = state_.with_opened(std::move(opened.text), std::move(document));
        // 同じ退避番号で改訂番号 0 のまま本文が丸ごと替わるので、構文の覚えを両方捨てる
        // （ADR 0156 の決定 9）。
        first_syntax_.reset();
        second_syntax_.reset();
        ask_display();
        // 本文が丸ごと替わるので、プレビューも頼み直す（ADR 0155 の決定 9）。
        request_preview(true);
        return;
    }
    leave_document();
    state_ = state_.with_new_tab().with_opened(std::move(opened.text), std::move(document));
    enter_document();
}

// Ctrl+O と起動引数は失敗をダイアログで（ADR 0010 の決定 9）。一覧から選んだファイルは 1 行の
// 知らせで、無くなっていたファイル（not_found）だけ出どころが履歴の候補なら履歴から外す（D30・
// ADR 0062 の決定 18）。ほかの失敗は履歴に残す。
void EditorController::answer_unopened(const core::FilePath &path, FileFailure failure,
                                       OpenFailureReply reply)
{
    switch (reply)
    {
    case OpenFailureReply::dialog:
        fail(failure);
        return;
    case OpenFailureReply::notice:
        reject(unreached_message(path, 0));
        return;
    case OpenFailureReply::notice_forgetting:
        reject(unreached_message(path, 0));
        if (failure == FileFailure::not_found)
        {
            forget(path);
        }
        return;
    }
    std::unreachable();
}

// 読み込み中の要求を取り消す（券の仕事をやめさせて要求と留めた結果を消す・決定 5）。only_opens
// なら開く要求だけ（使う人の新しい開く意図）、そうでなければタブの読みも（EndSession）。
void EditorController::cancel_readings(bool only_opens)
{
    std::vector<DocumentReading> kept;
    for (const DocumentReading &reading : state_.readings())
    {
        if (only_opens && !opens(reading))
        {
            kept.push_back(reading);
            continue;
        }
        ports_.reads.cancel(reading.ticket);
        static_cast<void>(take_arrived(reading.ticket));
    }
    if (kept.size() != state_.readings().size())
    {
        state_ = state_.with_readings(std::move(kept));
    }
}

// 揃う前に閉じたタブの読みは取り消す（追補 1）。閉じる道はどれもこの後に通る（apply の終わり）。
void EditorController::cancel_closed_tab_reads()
{
    std::vector<DocumentReading> kept;
    for (const DocumentReading &reading : state_.readings())
    {
        const auto *tab = std::get_if<AutosaveId>(&reading.target);
        if (tab != nullptr && !tab_position_of(*tab).has_value())
        {
            ports_.reads.cancel(reading.ticket);
            static_cast<void>(take_arrived(reading.ticket));
            continue;
        }
        kept.push_back(reading);
    }
    if (kept.size() != state_.readings().size())
    {
        state_ = state_.with_readings(std::move(kept));
    }
}

// 意図の終わりで読みの後始末をする（追補 1・2）。歩きが終わる・入力行や面が閉じる意図の終わりに
// 留めた結果を採る。読み込み中の要求が無ければ欄を 1 つ読むだけで抜ける。
void EditorController::follow_reads()
{
    if (state_.readings().empty())
    {
        return;
    }
    cancel_closed_tab_reads();
    settle_reads();
}

void EditorController::collect_reads()
{
    for (DocumentReadOutcome &outcome : ports_.reads.collect())
    {
        adopt_read(std::move(outcome));
    }
    settle_reads();
}

// 読み込み中の 1 行（決定 4）。頼んだ順の最初の要求を見せる。
std::optional<core::DisplayText> EditorController::reading_message() const
{
    const auto &readings = state_.readings();
    if (readings.empty())
    {
        return std::nullopt;
    }
    const DocumentReading &value = readings.front();
    const std::string name(value.path.file_name().empty() ? value.path.text()
                                                          : value.path.file_name());
    if (value.total_bytes == 0)
    {
        return named_line(reading_prefix, name, {});
    }
    const std::uint64_t percent =
        std::min<std::uint64_t>(100, value.read_bytes * 100 / value.total_bytes);
    return named_line(reading_prefix, name, " " + std::to_string(percent) + "%");
}

// Ctrl+O と起動引数の開く。失敗はダイアログ（ADR 0010 の決定 9）。
void EditorController::accept(const OpenDocument &intent)
{
    open_document(intent.path, OpenFailureReply::dialog);
}

// 一覧から選んだファイルを開く（ADR 0060 の決定 7）。失敗はダイアログではなく 1 行の知らせ
// （answer_unopened）。履歴から外すのは出どころが履歴の候補のときだけで、同じフォルダの候補では
// 履歴を読み書きしない（ADR 0062 の決定 18）。
void EditorController::open_listed(const core::FilePath &path,
                                   std::optional<core::PaletteOrigin> origin)
{
    open_document(path, origin == core::PaletteOrigin::history ? OpenFailureReply::notice_forgetting
                                                               : OpenFailureReply::notice);
}

// 履歴に記録する 1 本（ADR 0060 の決定 8）。書くたびに読んで足す（ほかの窓の分を失わない）。
// 読めなければ空の履歴から。結果の失敗は捨てる（履歴は無くても動く・状態も知らせも変えない）。
void EditorController::remember(const std::vector<core::FilePath> &paths)
{
    if (paths.empty())
    {
        return;
    }
    FileHistory history = ports_.history.read().value_or(FileHistory{});
    for (const core::FilePath &path : paths)
    {
        history = history_recorded(std::move(history), path, ports_.files);
    }
    static_cast<void>(ports_.history.write(history));
}

// 履歴から外す 1 本（D30）。読めなければ外すものが無いので書かない。結果の失敗は捨てる。
void EditorController::forget(const core::FilePath &path)
{
    auto history = ports_.history.read();
    if (!history)
    {
        return;
    }
    static_cast<void>(
        ports_.history.write(history_forgotten(std::move(history).value(), path, ports_.files)));
}

void EditorController::accept(const SaveDocument &intent)
{
    const auto saved = save_document(intent);
    if (!saved)
    {
        fail(saved.error());
    }
}

std::expected<void, FileFailure> EditorController::save_document(const SaveDocument &intent)
{
    // 書けなかったときは本文も文書も変えない。元のファイルも adapters が守る（決定 6）。
    const auto written = write_document(intent);
    if (!written)
    {
        return std::unexpected(written.error());
    }
    if (intent.identity == SaveIdentity::retain_document)
    {
        return {};
    }
    // 保存の直後に単位を閉じる。続く入力が保存時点の単位に混ざらない（決定 7）。
    const auto history = state_.history().sealed();
    const std::size_t position = history.position();
    state_ = state_.with_history(history).with_document(
        Document{intent.path, intent.encoding, position});
    return {};
}

// ---------------------------------------------------------------- タブ（ADR 0056）

void EditorController::leave_document()
{
    // 文書が替わったら、裏の読みが揃っても前に指したタブへは切り替えない（ADR 0159 の追補 6）。
    reach_wanted_.reset();
    // 出ていく文書の追従を今の文書で解決してから置く（ADR 0128）。問いの間は解決できないので
    // 保留を捨て、次の文書へ持ち越さない（ADR 0128 の決定 7）。
    drop_pending_follow();
    settle_follow();
    close_replayed_unit();
    // 入力行の取消は検索の preview を閉じて入力前のスクロールへ戻し、検索なら Vim の保留も
    // 捨てる。Esc と同じ 1 本を通す（ADR 0041 の決定 5・ADR 0056 の決定 4）。
    if (state_.command_input().has_value())
    {
        accept(CancelCommand{});
    }
    state_ = state_.with_composition(std::nullopt);
}

void EditorController::enter_document()
{
    // 問いは最初の本文の文字組みより前（ADR 0140 の決定 2）。下の clamp_scroll より先に判定する。
    ask_display();
    // 見えている行数は置いていたあいだに変わり得るので、先頭行だけを今の窓の範囲へ収める。
    // キャレットを追いかけない（ScrollLines で動かしたスクロールを文書ごとに保つ）。
    clamp_scroll();
    settle_vim_caret();
    reveal_active_tab();
    reopen_replayed_unit();
    // プレビューは操作中の文書に追従する（ADR 0155 の決定 2・9）。
    request_preview(true);
}

TabReach EditorController::reach_tab(std::size_t position)
{
    const auto unloaded = state_.unloaded_at(position);
    if (!unloaded.has_value())
    {
        return position < state_.tab_count() ? TabReach::reached : TabReach::dropped;
    }
    const AutosaveId tab = state_.autosave_ids().at(position);
    // 同じタブを裏で読んでいる最中なら 2 本目を頼まない（追補 1）。
    if (std::ranges::any_of(state_.readings(),
                            [tab](const DocumentReading &reading)
                            {
                                const auto *target = std::get_if<AutosaveId>(&reading.target);
                                return target != nullptr && *target == tab;
                            }))
    {
        return TabReach::reading;
    }
    const core::FilePath path = unloaded.value().path;
    const std::uint64_t ticket = ++read_tickets_;
    state_ = state_.with_readings(
        with_appended(state_.readings(), DocumentReading{path, ticket, 0, 0, tab}));
    auto outcome = ports_.reads.request(DocumentReadRequest{path, ticket, maximum_file_bytes});
    if (!outcome.has_value())
    {
        return TabReach::reading;
    }
    // 同期に揃った結果も同じ adopt_read を通す（決定 3・追補 4）。揃っていればここで採る。
    adopt_read(std::move(outcome).value());
    auto finished = take_arrived(ticket);
    if (!finished.has_value())
    {
        return TabReach::reading;
    }
    state_ = state_.with_readings(without_ticket(state_.readings(), ticket));
    return loaded_tab(position, std::move(finished.value().document));
}

// 揃った本文でまだ読んでいないタブを束に置き換える（ADR 0059 の決定 5）。読めなかったタブは帯と
// 使った順から外し、1 行知らせる。ダイアログの経路（last_failure）には載せない（D26:
// 窓を止めない）。
TabReach EditorController::loaded_tab(std::size_t position,
                                      std::expected<OpenedDocument, FileFailure> document)
{
    const auto unloaded = state_.unloaded_at(position);
    if (!unloaded.has_value())
    {
        return position < state_.tab_count() ? TabReach::reached : TabReach::dropped;
    }
    const UnloadedDocument &tab = unloaded.value();
    if (!document)
    {
        const core::FilePath path = tab.path;
        state_ = state_.with_dropped(position);
        report_unreached(path);
        reveal_active_tab();
        return TabReach::dropped;
    }
    OpenedDocument opened = std::move(document).value();
    state_ = state_.with_loaded(
        position, reached_bundle(std::move(opened.text),
                                 Document{tab.path, opened.encoding, std::size_t{0}}, tab));
    return TabReach::reached;
}

void EditorController::report_unreached(const core::FilePath &path)
{
    ++unreached_tabs_;
    state_ = state_.with_command_message(unreached_message(path, unreached_tabs_ - 1));
}

// 窓が閉じていく（ADR 0059 の決定 3）。一覧を作るのはここだけで、1 打鍵の道には無い。書けなくても
// 窓は閉じていく途中なので、状態を変えず何も出さない。
void EditorController::accept(const EndSession &intent)
{
    // 読み込み中の要求は取消で、ファイルは開かれない（ADR 0159 の決定 5・11・追補 1）。
    cancel_readings(false);
    restore_untitled_.reset();
    reach_wanted_.reset();
    // 言語サーバーは待たずに止める（ADR 0160 の追補）。子が居なければ呼ばない。
    end_language_server_session();
    settle_follow();
    // 確かめずに未保存が残る終わり方（OS の終了の強行・描画を失った終わり）は、一覧より先に
    // 退避を 1 回頼み、worker が書き終えるまで上限つきで待つ。書けなければ前回の退避のまま
    // 終わる（ADR 0137 の決定 9）。
    const bool clears = clears_autosave(state_, intent);
    if (!clears)
    {
        autosave_now();
        ports_.autosave.flush();
    }
    static_cast<void>(ports_.session.write(ended_session(state_, intent.reason)));
    // 閉じたファイルを履歴に記録する（ADR 0060 の決定 8）。一覧の書きの後。
    remember(ended_paths(state_, intent.reason));
    // 未保存のタブが残らないか、使う人が「いいえ」と答えて捨てた終わり方だけ退避を消す（ADR 0137
    // の決定 5）。確かめずに残る終わり方では消さず、次の起動で問う。進行中の store の終わりは
    // adapter が待つ。
    if (clears)
    {
        ports_.autosave.clear();
    }
}

// 表示形式を替えると画面行の数が替わるので、窓の寸法が届いていれば先頭行を収めてキャレットを
// 追う（設定の折り返しを替えたときと同じ・persist_settings）。問いの答え（はい・いいえ・出せな
// かった）もここを通り、止めていた本文の文字組みを始める（ADR 0140 の決定 2）。格子にする文書が
// 索引を持たなければここで持たせる（決定 4）。
void EditorController::accept(const SelectDisplay &intent)
{
    const bool answering = display_pending(state_.display_inquiry());
    if (answering)
    {
        state_ = state_.with_display_inquiry(DisplayInquiry::answered);
    }
    else if (intent.display == state_.display())
    {
        return;
    }
    if (intent.display == core::DisplayForm::grid && !state_.text().has_virtual_columns())
    {
        state_ = state_.with_virtual_columns();
    }
    state_ = state_.with_display(intent.display);
    if (view_geometry_.width > 0)
    {
        clamp_scroll();
        follow_caret();
    }
}

// 言語は文書の値で、設定にもセッションにも書かない（ADR 0154 の決定 1）。読むのは貼り付け・
// プレビュー・構文の色の側で、ここは置くだけ（決定 7）。
void EditorController::accept(const SelectLanguage &intent)
{
    state_ = state_.with_language(intent.language);
}

// 構文の色の入れ切りも文書の値で、設定にもセッションにも書かない（ADR 0156 の追補）。値が変わった
// ときは両方の枠の覚えを捨て、on に戻したときに古い状態から続けない。
void EditorController::accept(const SelectSyntax &intent)
{
    if (state_.syntax_enabled() == intent.enabled)
    {
        return;
    }
    state_ = state_.with_syntax_enabled(intent.enabled);
    first_syntax_.reset();
    second_syntax_.reset();
}

// 判定は文書ごとに 1 回で、改行の索引と巨大な行の数えだけ（文字組みに触れない）。巨大な行の
// 文書は答えに依らず索引を持つ（いいえの窓の文字組みも使う・決定 4）。問いを出すのは歩きの途中
// でも起動の途中でもないときだけで、出したら答えが来るまで本文の文字組みを止める。
void EditorController::ask_display()
{
    if (starting_)
    {
        return;
    }
    if (state_.display_inquiry() == DisplayInquiry::unjudged)
    {
        state_ = asks_grid(state_.text())
                     ? state_.with_virtual_columns().with_display_inquiry(DisplayInquiry::waiting)
                     : state_.with_display_inquiry(DisplayInquiry::unneeded);
    }
    if (state_.display_inquiry() == DisplayInquiry::waiting && !state_.tab_walking())
    {
        state_ = state_.with_display_inquiry(DisplayInquiry::raised).with_display_question(true);
    }
}

// 隣を裏で読んでいる間は空の無題に移り、揃ったら隣へ切り替える（追補 1・6）。
void EditorController::close_toward_reading(std::size_t active, std::size_t neighbour)
{
    const AutosaveId wanted = state_.autosave_ids().at(neighbour);
    leave_document();
    state_ = state_.with_new_tab().with_closed(active);
    enter_document();
    reach_wanted_ = wanted;
}

// アクティブを閉じる。次にアクティブになる隣（右隣・無ければ左隣）を先に読み、読めなければ外して
// 次の隣を試す。隣が 1 本も残らなければ空の無題を 1 本置く。窓は閉じない。使う人が閉じたのは
// 1 本だけで、ほかのタブが読めなかったのは使う人の操作ではない（ADR 0059 の決定 5）。
void EditorController::close_active_tab()
{
    while (state_.tab_count() > 1)
    {
        const std::size_t active = state_.active_tab();
        const std::size_t neighbour = active + 1 < state_.tab_count() ? active + 1 : active - 1;
        switch (reach_tab(neighbour))
        {
        case TabReach::reached:
            leave_document();
            state_ = state_.with_closed(active);
            enter_document();
            return;
        case TabReach::reading:
            close_toward_reading(active, neighbour);
            return;
        case TabReach::dropped:
            break;
        }
    }
    leave_document();
    state_ = state_.with_new_tab().with_closed(0);
    enter_document();
}
} // namespace nenenib::application
