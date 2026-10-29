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
#include "ExLanguageRequest.hpp"
#include "ExResult.hpp"
#include "ExSyntaxRequest.hpp"
#include "FileHistory.hpp"
#include "FileHistoryEdit.hpp"
#include "Language.hpp"
#include "LanguageTable.hpp"
#include "PaletteOrigin.hpp"
#include "SaveState.hpp"
#include "Selection.hpp"
#include "Session.hpp"
#include "SessionEnd.hpp"
#include "SessionTab.hpp"
#include "SplitLayout.hpp"
#include "TabRecency.hpp"
#include "TabTitle.hpp"
#include "TextCaret.hpp"
#include "TextPosition.hpp"
#include "UnloadedDocument.hpp"
#include "Utf16.hpp"

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
// 同期で読むのはここまで（ADR 0010 の決定 12）。値の正本は core の 1 か所で、退避の読み戻しも同じ
// 値を読む（#792・ADR 0121 の追補の決定 8）。
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
// 着かない）。履歴は空、Vim の文書ごとの値は無し。
[[nodiscard]] DocumentState reached_bundle(core::TextBuffer text, Document document,
                                           const UnloadedDocument &tab)
{
    const core::Offset caret =
        text.offset_of(core::TextPosition{line_within(text, tab.caret.line), tab.caret.column});
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
        if (reach_tab(position))
        {
            leave_document();
            state_ = state_.with_switched(position).with_closed(0).with_recency_ranked(ranks);
            enter_document();
            return;
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

// 開く（ADR 0056 の決定 5・ADR 0159 の決定 1・3〜5）。(a) 同じファイルのタブへ切り替える（読み
// 直さない）→ 読みの port に頼む。同じファイルを読み込み中なら 2 本目を頼まず今の要求を続け、
// ほかの読み込み中の要求は取り消す。揃ったら (b) 何も書いていない無題に開く → (c) アクティブの
// 右に新しいタブを足して開く（open_read）。開けなかったらタブを足さず、呼び手の告げ方で告げる。
void EditorController::open_document(const core::FilePath &path, OpenFailureReply reply)
{
    const auto &reading = state_.reading();
    if (reading.has_value() && ports_.files.same_file(reading.value().path, path))
    {
        return;
    }
    cancel_reading();
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
    state_ = state_.with_reading(DocumentReading{path, ticket, 0, 0, reply});
    auto outcome = ports_.reads.request(DocumentReadRequest{path, ticket, maximum_file_bytes});
    if (outcome.has_value())
    {
        adopt_read(std::move(outcome).value());
    }
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
    const auto &reading = state_.reading();
    if (!reading.has_value() || reading.value().ticket != progress.ticket)
    {
        return;
    }
    DocumentReading next = reading.value();
    next.read_bytes = progress.read_bytes;
    next.total_bytes = progress.total_bytes;
    state_ = state_.with_reading(std::move(next));
}

// 券が今の要求と違う結果は捨てる（ADR 0127 §3 の一回確定・決定 5）。
void EditorController::adopt_finished(DocumentReadFinished finished)
{
    const auto &reading = state_.reading();
    if (!reading.has_value() || reading.value().ticket != finished.ticket)
    {
        return;
    }
    const DocumentReading done = reading.value();
    state_ = state_.with_reading(std::nullopt);
    if (!finished.document)
    {
        answer_unopened(done.path, finished.document.error(), done.reply);
        return;
    }
    open_read(done.path, std::move(finished.document).value());
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

void EditorController::cancel_reading()
{
    const auto &reading = state_.reading();
    if (!reading.has_value())
    {
        return;
    }
    ports_.reads.cancel(reading.value().ticket);
    state_ = state_.with_reading(std::nullopt);
}

void EditorController::collect_reads()
{
    for (DocumentReadOutcome &outcome : ports_.reads.collect())
    {
        adopt_read(std::move(outcome));
    }
}

std::optional<core::DisplayText> EditorController::reading_message() const
{
    const auto &reading = state_.reading();
    if (!reading.has_value())
    {
        return std::nullopt;
    }
    const DocumentReading &value = reading.value();
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

bool EditorController::reach_tab(std::size_t position)
{
    const auto unloaded = state_.unloaded_at(position);
    if (!unloaded.has_value())
    {
        return position < state_.tab_count();
    }
    const UnloadedDocument &tab = unloaded.value();
    auto document = read_now(tab.path);
    if (!document)
    {
        // 読めなかったタブは帯と使った順から外し、1 行知らせる。ダイアログの経路（last_failure）
        // には載せない（D26: 窓を止めない）。
        state_ = state_.with_dropped(position);
        report_unreached(tab.path);
        reveal_active_tab();
        return false;
    }
    OpenedDocument opened = std::move(document).value();
    state_ = state_.with_loaded(
        position, reached_bundle(std::move(opened.text),
                                 Document{tab.path, opened.encoding, std::size_t{0}}, tab));
    return true;
}

// 裏へ回された（同期の上限を超える）ときは頼みを取り消し、今の上限を超えたときと同じ too_large
// として扱う。まだ読んでいないタブを裏で読む形は ADR 0159 に無く、合成ルートの上限（L2c）までの
// 暫定（今の同期の上限と同じ振る舞い）。
std::expected<OpenedDocument, FileFailure> EditorController::read_now(const core::FilePath &path)
{
    const std::uint64_t ticket = ++read_tickets_;
    auto outcome = ports_.reads.request(DocumentReadRequest{path, ticket, maximum_file_bytes});
    auto *finished =
        outcome.has_value() ? std::get_if<DocumentReadFinished>(&outcome.value()) : nullptr;
    if (finished == nullptr)
    {
        ports_.reads.cancel(ticket);
        return std::unexpected(FileFailure::too_large);
    }
    return std::move(finished->document);
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
    // 読み込み中の要求は取消で、ファイルは開かれない（ADR 0159 の決定 5・11）。
    cancel_reading();
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

// アクティブを閉じる。次にアクティブになる隣（右隣・無ければ左隣）を先に読み、読めなければ外して
// 次の隣を試す。隣が 1 本も残らなければ空の無題を 1 本置く。窓は閉じない。使う人が閉じたのは
// 1 本だけで、ほかのタブが読めなかったのは使う人の操作ではない（ADR 0059 の決定 5）。
void EditorController::close_active_tab()
{
    while (state_.tab_count() > 1)
    {
        const std::size_t active = state_.active_tab();
        const std::size_t neighbour = active + 1 < state_.tab_count() ? active + 1 : active - 1;
        if (reach_tab(neighbour))
        {
            leave_document();
            state_ = state_.with_closed(active);
            enter_document();
            return;
        }
    }
    leave_document();
    state_ = state_.with_new_tab().with_closed(0);
    enter_document();
}
} // namespace nenenib::application
