#include "EditorController.hpp"
#include "GridLineRows.hpp"
#include "GridLineSource.hpp"

#include "AutosaveOutcome.hpp"
#include "AutosavePlan.hpp"
#include "AutosaveSnapshot.hpp"
#include "AutosaveStep.hpp"
#include "ByteOrder.hpp"
#include "CaretMove.hpp"
#include "CaretShape.hpp"
#include "ClipboardText.hpp"
#include "CommandChoice.hpp"
#include "CommandChoiceKind.hpp"
#include "CommandEdit.hpp"
#include "CommandInput.hpp"
#include "CommandLayout.hpp"
#include "CommandPalette.hpp"
#include "Composition.hpp"
#include "DeleteDirection.hpp"
#include "DevicePixels.hpp"
#include "DisplayInquiry.hpp"
#include "DisplayLine.hpp"
#include "DisplayQuestion.hpp"
#include "DisplayText.hpp"
#include "DocumentByteLimit.hpp"
#include "DocumentState.hpp"
#include "Edit.hpp"
#include "EncodingFailure.hpp"
#include "ExEvaluationFailure.hpp"
#include "ExLanguageRequest.hpp"
#include "ExResult.hpp"
#include "ExTabRequest.hpp"
#include "ExTabVerb.hpp"
#include "ExWindowCommand.hpp"
#include "FileBookmarkEdit.hpp"
#include "FileFolder.hpp"
#include "FileHistory.hpp"
#include "FileHistoryEdit.hpp"
#include "FolderBatch.hpp"
#include "FolderProgress.hpp"
#include "FolderRequest.hpp"
#include "FontFamilies.hpp"
#include "GiantLine.hpp"
#include "HtmlToMarkdown.hpp"
#include "ImeStance.hpp"
#include "Language.hpp"
#include "LanguageTable.hpp"
#include "LineSpanEvaluation.hpp"
#include "MarkdownFailure.hpp"
#include "MarkdownOutcome.hpp"
#include "MarkdownParse.hpp"
#include "MarkdownRequest.hpp"
#include "ModeLabel.hpp"
#include "OrdinaryCharacterBoundary.hpp"
#include "Palette.hpp"
#include "PaletteLayout.hpp"
#include "PaletteMarks.hpp"
#include "PaletteOrigin.hpp"
#include "PaneFrame.hpp"
#include "PaneView.hpp"
#include "ParkedTab.hpp"
#include "PasteKind.hpp"
#include "PreviewFrame.hpp"
#include "PreviewView.hpp"
#include "SaveState.hpp"
#include "ScrollBounds.hpp"
#include "SearchLine.hpp"
#include "SearchPreview.hpp"
#include "Selection.hpp"
#include "SelectionSpan.hpp"
#include "Session.hpp"
#include "SessionEnd.hpp"
#include "SessionTab.hpp"
#include "SplitLayout.hpp"
#include "StatusBarLayout.hpp"
#include "StatusItems.hpp"
#include "TabDestination.hpp"
#include "TabJump.hpp"
#include "TabJumpDirection.hpp"
#include "TabRecency.hpp"
#include "TabTitle.hpp"
#include "TextPosition.hpp"
#include "TitleBarHit.hpp"
#include "TitleBarLayout.hpp"
#include "UnlistedExtensions.hpp"
#include "UnloadedDocument.hpp"
#include "Utf16.hpp"
#include "Utf8.hpp"
#include "VimBlockEdit.hpp"
#include "VimBlockRange.hpp"
#include "VimCaret.hpp"
#include "VimCharacter.hpp"
#include "VimClipboardText.hpp"
#include "VimInsertBlock.hpp"
#include "VimKey.hpp"
#include "VimKeySource.hpp"
#include "VimMarkEdits.hpp"
#include "VimMarkPosition.hpp"
#include "VimMatchRequest.hpp"
#include "VimMode.hpp"
#include "VimNavigate.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimRemoveBlock.hpp"
#include "VimReplaceBlock.hpp"
#include "VimReplay.hpp"
#include "VimSearch.hpp"
#include "VimSearchDirection.hpp"
#include "VimSearchNotice.hpp"
#include "VimSearchPattern.hpp"
#include "VimSpecialKey.hpp"
#include "VimStep.hpp"
#include "VimSwitchTab.hpp"
#include "VimTabs.hpp"
#include "VimVisualRange.hpp"
#include "VimVisualReselect.hpp"
#include "VimWindowCommand.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::application
{
namespace
{
constexpr std::size_t single_page_line = 1;
// 同期で読むのはここまで（ADR 0010 の決定 12）。値の正本は core の 1 か所で、退避の読み戻しも同じ
// 値を読む（#792・ADR 0121 の追補の決定 8）。
constexpr std::size_t maximum_file_bytes = core::maximum_document_bytes;

[[nodiscard]] core::ScrollExtent scroll_extent_for(core::EditMode mode) noexcept
{
    switch (mode)
    {
    case core::EditMode::ordinary:
        return core::ScrollExtent::filled_viewport;
    case core::EditMode::vim:
        return core::ScrollExtent::last_line;
    }
    std::unreachable();
}

// 入力行の編集のうち履歴を歩く 2 つ（#725）。写し先が足りなければコンパイルが落ちる（CPP-002）。
[[nodiscard]] constexpr bool recalls_history(core::CommandEdit edit) noexcept
{
    switch (edit)
    {
    case core::CommandEdit::history_previous:
    case core::CommandEdit::history_next:
        return true;
    case core::CommandEdit::left:
    case core::CommandEdit::right:
    case core::CommandEdit::home:
    case core::CommandEdit::end:
    case core::CommandEdit::backspace:
    case core::CommandEdit::erase:
    case core::CommandEdit::complete_next:
    case core::CommandEdit::complete_previous:
    case core::CommandEdit::page_previous:
    case core::CommandEdit::page_next:
    case core::CommandEdit::list_first:
    case core::CommandEdit::list_last:
        return false;
    }
    std::unreachable();
}

[[nodiscard]] core::ScrollFollow scroll_follow_for(core::EditMode mode) noexcept
{
    switch (mode)
    {
    case core::EditMode::ordinary:
        return core::ScrollFollow::minimal;
    case core::EditMode::vim:
        return core::ScrollFollow::vim;
    }
    std::unreachable();
}

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
        return "Could not write file";
    }
    std::unreachable();
}

// 判定の失敗のうち UTF-32 だけは名指しで断る。ほかは判別できなかった、のまま（#787）。
[[nodiscard]] FileFailure file_failure_of(core::EncodingFailure failure) noexcept
{
    switch (failure)
    {
    case core::EncodingFailure::unsupported_utf32:
        return FileFailure::unsupported_utf32;
    case core::EncodingFailure::undecodable:
    case core::EncodingFailure::odd_utf16_length:
        return FileFailure::undecodable;
    }
    std::unreachable();
}

[[nodiscard]] FileFailure file_failure_of(CodePageFailure failure) noexcept
{
    switch (failure)
    {
    case CodePageFailure::undecodable:
        return FileFailure::undecodable;
    case CodePageFailure::unencodable:
        return FileFailure::unencodable;
    }
    std::unreachable();
}

// 読めない理由（unavailable / unreadable）は区別せず既定の dark を選ぶ（ADR 0007）。
[[nodiscard]] core::Appearance appearance_or_dark(const AppearancePort &port)
{
    const auto current = port.current();
    if (!current)
    {
        return core::Appearance::dark;
    }
    return current.value();
}

// INSERT だけがバー。NORMAL と VISUAL はブロック（採用案 第 1 節・D15 / ADR 0018 の決定 1）。
[[nodiscard]] core::CaretShape caret_shape_for(core::EditMode mode, core::VimMode vim) noexcept
{
    switch (mode)
    {
    case core::EditMode::ordinary:
        return core::CaretShape::bar;
    case core::EditMode::vim:
        break;
    }
    switch (vim)
    {
    case core::VimMode::normal:
    case core::VimMode::visual:
    case core::VimMode::visual_line:
    case core::VimMode::visual_block:
        return core::CaretShape::block;
    case core::VimMode::insert:
        return core::CaretShape::bar;
    }
    std::unreachable();
}

// Vim の NORMAL に選択は無いので、クリックと Ctrl+矢印は Shift ごと畳む
// （SelectEditMode{vim} が選択を畳むのと同じ理屈・ADR 0012 の決定 10）。
[[nodiscard]] core::SelectionAnchoring anchoring_in(core::EditMode mode,
                                                    core::SelectionAnchoring anchoring) noexcept
{
    switch (mode)
    {
    case core::EditMode::vim:
        return core::SelectionAnchoring::collapse;
    case core::EditMode::ordinary:
        return anchoring;
    }
    std::unreachable();
}

[[nodiscard]] core::Offset anchor_for(core::SelectionAnchoring anchoring, core::Offset anchor,
                                      core::Offset caret) noexcept
{
    switch (anchoring)
    {
    case core::SelectionAnchoring::collapse:
        return caret;
    case core::SelectionAnchoring::extend:
        return anchor;
    }
    std::unreachable();
}

[[nodiscard]] core::CaretUnit caret_unit(core::EditMode mode) noexcept
{
    switch (mode)
    {
    case core::EditMode::ordinary:
        return core::CaretUnit::ordinary;
    case core::EditMode::vim:
        return core::CaretUnit::code_point;
    }
    std::unreachable();
}

[[nodiscard]] core::OffsetRange ordinary_deletion_range(const core::TextBuffer &text,
                                                        core::Offset caret,
                                                        core::DeleteDirection direction)
{
    switch (direction)
    {
    case core::DeleteDirection::backward:
        return core::OffsetRange{core::ordinary_character_boundary(
                                     text, caret, core::OrdinaryCharacterAction::backspace),
                                 caret};
    case core::DeleteDirection::forward:
        return core::OffsetRange{
            caret, core::ordinary_character_boundary(text, caret,
                                                     core::OrdinaryCharacterAction::erase_forward)};
    }
    std::unreachable();
}

// 選択が空のときの Backspace / Delete。外部から来る Vim の意図は既存の単位を保つ。
[[nodiscard]] core::OffsetRange deletion_range(const core::TextBuffer &text, core::Offset caret,
                                               core::DeleteDirection direction, core::EditMode mode)
{
    switch (mode)
    {
    case core::EditMode::ordinary:
        return ordinary_deletion_range(text, caret, direction);
    case core::EditMode::vim:
        break;
    }
    switch (direction)
    {
    case core::DeleteDirection::backward:
        return core::OffsetRange{
            core::moved_caret(text, caret,
                              core::CaretMoveRequest{core::CaretMotion::previous_character,
                                                     single_page_line,
                                                     core::CaretUnit::code_point}),
            caret};
    case core::DeleteDirection::forward:
        return core::OffsetRange{
            caret, core::moved_caret(text, caret,
                                     core::CaretMoveRequest{core::CaretMotion::next_character,
                                                            single_page_line,
                                                            core::CaretUnit::code_point})};
    }
    std::unreachable();
}

// LF だけの本文を文書の改行に直す（ARC-009。改行の形を決めるのは controller の 1 か所）。
// Vim のレジスタは「行の列」なので LF で持ち、本文に入る瞬間にだけ文書の形になる（ADR 0015）。
[[nodiscard]] std::string with_document_newlines(std::string_view utf8, std::string_view newline)
{
    std::string result;
    result.reserve(utf8.size());
    for (const char byte : utf8)
    {
        if (byte == '\n')
        {
            result.append(newline);
            continue;
        }
        result.push_back(byte);
    }
    return result;
}

// engine のキャレットは LF で数えた位置なので、直したあとの本文の上へずらす
// （CRLF は改行 1 つにつき 1 バイト長い）。数えるのは貼る位置からキャレットまでの改行だけ。
[[nodiscard]] core::Offset caret_after_insert(core::Offset at, std::string_view utf8,
                                              core::Offset caret, std::size_t newline_bytes)
{
    const auto inside = static_cast<std::ptrdiff_t>(caret.value - at.value);
    const auto newlines =
        static_cast<std::size_t>(std::count(utf8.begin(), utf8.begin() + inside, '\n'));
    return core::Offset{caret.value + newlines * (newline_bytes - 1)};
}

// 描く範囲。矩形のあいだだけ行ごとの範囲へ差し替わる（ADR 0035 の決定 8）。短い行の右側
// （本文の無い所）は塗らない＝ Vim の既定と同じ。
[[nodiscard]] core::OffsetRange block_row(const std::optional<core::VimBlockRange> &block,
                                          core::LineNumber line, const core::OffsetRange &range)
{
    if (!block.has_value())
    {
        return range;
    }
    const core::VimBlockRange &rows = block.value();
    if (line.value < rows.first.value || line.value >= rows.first.value + rows.lines.size())
    {
        return core::OffsetRange{core::Offset{0}, core::Offset{0}};
    }
    return rows.lines.at(line.value - rows.first.value).range;
}
// incsearch が入力中のパターンを見せる状態か（ADR 0041 の決定 3・4）。検索の入力行が開いていて、
// 入力が空でなく、incsearch が on のときだけ。
[[nodiscard]] const core::SearchLine *previewed_line(const EditorState &state)
{
    const auto &input = state.command_input();
    if (!state.vim().incsearch || !input.has_value())
    {
        return nullptr;
    }
    const auto *line = std::get_if<core::SearchLine>(&input.value());
    if (line == nullptr || line->text().empty())
    {
        return nullptr;
    }
    return line;
}

// incsearch の preview が着いた当たりの位置。入力中でないか当たりが無ければ absent（ADR 0041 の
// 決定 4）。
[[nodiscard]] std::optional<core::Offset> previewed_offset(const EditorState &state)
{
    const auto &preview = state.search_preview();
    if (!preview.has_value())
    {
        return std::nullopt;
    }
    const auto &match = preview.value().match;
    if (!match.has_value())
    {
        return std::nullopt;
    }
    return state.text().offset_of(match.value());
}

[[nodiscard]] std::optional<core::VimPattern> typed_pattern(const core::SearchLine &line)
{
    auto parsed = core::VimPattern::parse(line.text(), line.direction());
    if (!parsed)
    {
        return std::nullopt;
    }
    return std::move(parsed).value();
}

// 確定の検索の鍵と同じ vim_find_match 1 本で探した当たりの位置（ADR 0041 の決定 1）。
// 不一致は当たり無しで、報せは出さない。
[[nodiscard]] std::optional<core::TextPosition> found_match(const core::TextBuffer &text,
                                                            const core::VimPattern &pattern,
                                                            const core::VimMatchRequest &request)
{
    const auto hit = core::vim_find_match(text, pattern, request);
    if (!hit)
    {
        return std::nullopt;
    }
    return hit.value().position;
}

// 入力中のパターンの次の当たり。検索の起点から入力行の向きへ、確定の鍵と同じ回数だけ探す
// （ADR 0041 の決定 3・ADR 0043 の決定 1）。解析の失敗と不一致は当たり無し。
[[nodiscard]] std::optional<core::TextPosition> previewed_match(const EditorState &state,
                                                                core::TextPosition from)
{
    const core::SearchLine *line = previewed_line(state);
    if (line == nullptr)
    {
        return std::nullopt;
    }
    const auto pattern = typed_pattern(*line);
    if (!pattern.has_value())
    {
        return std::nullopt;
    }
    return found_match(
        state.text(), pattern.value(),
        core::VimMatchRequest{from, line->direction(), core::vim_search_count(state.vim())});
}

// 境界が文字の途中か CRLF の間なら、覆う範囲を外へ 1 バイト広げる向きに動かせるか。
[[nodiscard]] bool splits_a_character(std::string_view text, std::size_t at) noexcept
{
    if (at == 0 || at >= text.size())
    {
        return false;
    }
    const auto byte = static_cast<unsigned char>(text[at]);
    return (byte & 0xC0U) == 0x80U || (text[at] == '\n' && text[at - 1] == '\r');
}

// 先頭の共通部分の長さ。文字と CRLF を割らない所まで戻す。
[[nodiscard]] std::size_t common_head(std::string_view before, std::string_view after) noexcept
{
    std::size_t head = 0;
    const std::size_t shorter = std::min(before.size(), after.size());
    while (head < shorter && before[head] == after[head])
    {
        ++head;
    }
    while (head > 0 && (splits_a_character(before, head) || splits_a_character(after, head)))
    {
        --head;
    }
    return head;
}

// 末尾の共通部分の長さ（先頭の共通部分とは重ねない）。文字と CRLF を割らない所まで戻す。
[[nodiscard]] std::size_t common_tail(std::string_view before, std::string_view after,
                                      std::size_t head) noexcept
{
    std::size_t tail = 0;
    const std::size_t room = std::min(before.size(), after.size()) - head;
    while (tail < room && before[before.size() - 1 - tail] == after[after.size() - 1 - tail])
    {
        ++tail;
    }
    while (tail > 0 && (splits_a_character(before, before.size() - tail) ||
                        splits_a_character(after, after.size() - tail)))
    {
        --tail;
    }
    return tail;
}

// 前後の本文の違う所を覆う 1 つの Edit（ADR 0046 の決定 3）。同じ本文なら空。restore は畳む
// 単位の最初の編集の値を呼ぶ側が渡す（ADR 0052 の決定 3）。
[[nodiscard]] std::optional<core::Edit> covering_edit(std::string_view before,
                                                      std::string_view after, core::Offset restore)
{
    if (before == after)
    {
        return std::nullopt;
    }
    const std::size_t head = common_head(before, after);
    const std::size_t tail = common_tail(before, after, head);
    return core::Edit{core::Offset{head},
                      std::string(before.substr(head, before.size() - head - tail)),
                      std::string(after.substr(head, after.size() - head - tail)), restore};
}
// 脇のタブの表示値。読み込み済みかどうかを区別せずに並べる（ADR 0059 の決定 4）。
[[nodiscard]] const DocumentView &parked_view(const ParkedTab &tab)
{
    return std::visit([](const auto &value) -> const DocumentView & { return value->view; }, tab);
}

// 帯の順のタブの表示値（ADR 0056 の決定 7）。脇の束は置いたときの値を並べるだけで、
// 題名を作り直さない。アクティブの分は document と同じ値を差し込む。
// ステータスバーの右の 4 項目（ADR 0154 の決定 6・言語は右端）。
[[nodiscard]] std::array<core::DisplayText, core::status_item_count>
status_items_of(const EditorState &state, core::TextPosition caret, core::TextEncoding encoding)
{
    return core::status_items_for(caret, encoding, state.line_ending(), state.language());
}

[[nodiscard]] std::vector<DocumentView> tab_views(const EditorState &state,
                                                  const DocumentView &active)
{
    std::vector<DocumentView> views;
    views.reserve(state.tab_count());
    for (const auto &tab : state.parked())
    {
        if (views.size() == state.active_tab())
        {
            views.push_back(active);
        }
        views.push_back(parked_view(tab));
    }
    if (views.size() == state.active_tab())
    {
        views.push_back(active);
    }
    return views;
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

// 脇のタブのパス。同じファイルの検索は、まだ読んでいない文書にもパスで当たる（ADR 0059 の決定 4）。
[[nodiscard]] const std::optional<core::FilePath> &
parked_path(const std::shared_ptr<const DocumentState> &tab)
{
    return tab->document.path;
}

[[nodiscard]] std::optional<core::FilePath>
parked_path(const std::shared_ptr<const UnloadedDocument> &tab)
{
    return tab->path;
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

// 覚えていた行を本文の範囲の中へ寄せる（行が無ければ最後の行・ADR 0059 の決定 5）。
[[nodiscard]] core::LineNumber line_within(const core::TextBuffer &text, core::LineNumber line)
{
    return core::LineNumber{std::clamp<std::size_t>(line.value, 1, text.line_count())};
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

// 自動退避する本文の出どころ（ADR 0137 の決定 2）。まだ読んでいない文書は退避しない。写し先が
// 足りなければ std::visit が落ちる（CPP-002）。
[[nodiscard]] std::optional<std::shared_ptr<const DocumentState>>
autosave_bundle(const std::shared_ptr<const DocumentState> &tab)
{
    return tab;
}

[[nodiscard]] std::optional<std::shared_ptr<const DocumentState>>
autosave_bundle(const std::shared_ptr<const UnloadedDocument> &)
{
    return std::nullopt;
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

// 断片を 1 本のバイト列に写す（自動退避の写し・決定 4）。
[[nodiscard]] std::vector<std::byte> joined_bytes(std::span<const std::string_view> parts)
{
    std::size_t size = 0;
    for (const std::string_view part : parts)
    {
        size += part.size();
    }
    std::vector<std::byte> bytes;
    bytes.reserve(size);
    for (const std::string_view part : parts)
    {
        const auto view = std::as_bytes(std::span{part});
        bytes.insert(bytes.end(), view.begin(), view.end());
    }
    return bytes;
}

// 自動退避の失敗の 1 行（ADR 0137 の決定 4）。
constexpr std::string_view autosave_failed_message = "自動退避できませんでした";

// プレビューの 1 行（ADR 0155 の決定 3・6）。
constexpr std::string_view preview_too_large_message = "プレビューは 16 MiB までです";
constexpr std::string_view preview_rejected_message = "Markdown を解析できませんでした";
constexpr std::string_view preview_memory_message = "メモリが足りずプレビューできませんでした";

constexpr std::string_view unreached_prefix = "開けませんでした: ";
// クリップボードに書けなかった知らせ（ADR 0134 の決定 3）。
constexpr std::string_view clipboard_unwritten_notice = "クリップボードに書けませんでした";
// 貼り付けの知らせ（ADR 0157 の決定 3・7）。鳴らさない情報の 1 行。
constexpr std::string_view html_missing_notice = "HTML が無いので文字だけを貼りました";
constexpr std::string_view html_unconverted_notice = "HTML を変換できないので文字だけを貼りました";
// 描けない失敗だけを残す（frame の欄・ADR 0133 の追補 2026-10-18）。
[[nodiscard]] std::optional<TextLayoutFailure>
drawing_failure(std::optional<TextLayoutFailure> failure) noexcept
{
    if (failure.has_value() && blocks_drawing(failure.value()))
    {
        return failure;
    }
    return std::nullopt;
}

// Vim の問いの往復の契約の破れの知らせ（ADR 0133 の追補 2026-10-18・#665）。鍵は効かない。
constexpr std::string_view inconsistent_layout_notice = "内部の不整合: 画面行の問い（鍵は無効）";
// 自動退避の残骸を読めなかった知らせ（ADR 0137 の決定 6）。
constexpr std::string_view unrestored_prefix = "戻せませんでした: ";
constexpr std::string_view clipped_mark = "…";

// 名前を 1 つ載せる知らせの 1 行（ADR 0059 の「知らせの文言」）。others は同じ意図の中でほかに
// 読めなかった数。名前は 1 行に収まるよう末尾をコードポイントの境目で落とす（題名と同じ「…」）。
[[nodiscard]] core::DisplayText named_notice(std::string_view prefix, std::string name,
                                             std::size_t others)
{
    const std::string suffix =
        others == 0 ? std::string{} : "（ほか " + std::to_string(others) + " 件）";
    const std::size_t room = core::DisplayText::maximum_bytes - prefix.size() - suffix.size();
    if (name.size() > room)
    {
        core::Offset cut{room - clipped_mark.size()};
        while (!core::is_boundary(name, cut))
        {
            cut = core::previous_code_point(name, cut);
        }
        name.resize(cut.value);
        name += clipped_mark;
    }
    // 材料は検証済みの経路・題名と固定の文字だけなので parse は必ず成功する（不変条件・ARC-010）。
    return core::DisplayText::parse(std::string(prefix) + name + suffix).value();
}

[[nodiscard]] core::DisplayText unreached_message(const core::FilePath &path, std::size_t others)
{
    return named_notice(unreached_prefix,
                        std::string(path.file_name().empty() ? path.text() : path.file_name()),
                        others);
}

// 読めなかった残骸の知らせ。名前は未保存の印の無い題名（無題なら「無題」）。
[[nodiscard]] core::DisplayText unrestored_message(const AutosaveRemnant &remnant,
                                                   std::size_t others)
{
    return named_notice(
        unrestored_prefix,
        std::string(core::tab_title_for(remnant.path, core::SaveState::saved).text()), others);
}

// 自動退避から戻した 1 タブの束（ADR 0137 の決定 6）。保存した時点は無い（未保存の印）。
// キャレットは一覧に書いた位置を reached_bundle と同じく寄せる（行は本文の範囲へ、桁は行の中の
// 文字の境目へ）。スクロールは先頭、履歴は空。
[[nodiscard]] DocumentState restored_document(core::TextBuffer text, Document document,
                                              core::TextPosition caret)
{
    const core::Offset placed =
        text.offset_of(core::TextPosition{line_within(text, caret.line), caret.column});
    DocumentView view{core::tab_title_for(document.path, core::SaveState::modified), document.path,
                      document.encoding, core::SaveState::modified, std::nullopt};
    // 戻した文書の言語もパスから引く（ADR 0154 の決定 5）。
    const core::Language language = language_of(document);
    DocumentState bundle{std::move(text),
                         core::collapsed_at(placed),
                         core::EditHistory::empty(),
                         std::move(document),
                         core::LineNumber{1},
                         std::nullopt,
                         std::nullopt,
                         std::move(view)};
    bundle.language = language;
    return bundle;
}

// 帯の位置 position のタブのパス（無題なら無し）。脇の束は帯の位置からアクティブを抜いた順。
[[nodiscard]] std::optional<core::FilePath> tab_path_at(const EditorState &state,
                                                        std::size_t position)
{
    if (position == state.active_tab())
    {
        return state.document().path;
    }
    const std::size_t parked = position < state.active_tab() ? position : position - 1;
    return std::visit([](const auto &value) { return std::optional{parked_path(value)}; },
                      state.parked().at(parked));
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

// Ctrl+Tab の歩きを続けたまま受け取れる意図（ADR 0058 の決定 4）。歩きの 1 歩と確定、それに窓の
// 寸法・外観・帯の上のマウスとホイールのように文書もアクティブのタブも動かさない意図。これ以外の
// 意図は写す前に歩きを確定する（Ctrl を押したまま帯の上でマウスが動いても、歩きは切れない）。
// 裏の仕事の合図（WorkCompleted）も使う人の操作ではないので歩きを続ける（ADR 0062 の決定 7）。
// 自動退避の合図（AutosaveTick）も同じ理由で歩きを続ける（ADR 0137 の決定 1）。起動の問いの答え
// （RestoreAutosave / DiscardAutosave）は使う人の操作なので、ここに書かず歩きを確定する（決定 6）。
// プレビューの間引きの合図（PreviewTick）も使う人の操作ではないので歩きを続ける（ADR 0155 の
// 決定 8）。開く・閉じる（TogglePreview）は使う人の操作なので、ここにも begin_intent にも書かない。
[[nodiscard]] bool keeps_tab_walk(const EditorIntent &intent) noexcept
{
    return std::holds_alternative<WalkRecentTab>(intent) ||
           std::holds_alternative<SettleRecentTab>(intent) ||
           std::holds_alternative<WorkCompleted>(intent) ||
           std::holds_alternative<AutosaveTick>(intent) ||
           std::holds_alternative<PreviewTick>(intent) ||
           std::holds_alternative<PointTitleBar>(intent) ||
           std::holds_alternative<ScrollTabs>(intent) ||
           std::holds_alternative<TitleBarWidth>(intent) ||
           std::holds_alternative<VisibleLines>(intent) ||
           std::holds_alternative<RefreshAppearance>(intent);
}

// 上下に動くときに狙う横位置を保つ意図。上下の移動そのものと、30 秒ごとに必ず来る自動退避の
// 合図（使う人の操作ではない・ADR 0137 の決定 1）。もう 1 つの枠だけを送るホイールも操作中の枠の
// キャレットに触れないので保つ（ADR 0148 の決定 11）。これ以外の意図は写す前に狙いを捨てる。
// 枠の意図（SplitPane / SwitchPane / ClosePane / ScrollPane）は使う人の操作なので、
// keeps_tab_walk と begin_intent の表には載せない（歩きを終え、告知を消す）。
[[nodiscard]] bool keeps_wanted_x(const EditorIntent &intent) noexcept
{
    return std::holds_alternative<MoveCaret>(intent) ||
           std::holds_alternative<AutosaveTick>(intent) ||
           std::holds_alternative<ScrollPane>(intent);
}

// 面と `:` の補完の面の見えている行数を決める窓の寸法（ADR 0150 の決定 1）。幅か高さが 0 なら
// 寸法が届く前で、行数は上限のまま。
using CommandWindow = std::tuple<std::int32_t, std::int32_t, std::uint32_t>;

[[nodiscard]] bool measured(CommandWindow window) noexcept
{
    return std::get<0>(window) > 0 && std::get<1>(window) > 0;
}

// 行数が変わるときだけ替えた入力（同じなら無し・写しを作らない）。
[[nodiscard]] std::optional<CommandInput> fitted_rows(const core::CommandPalette &palette,
                                                      CommandWindow window)
{
    const auto [width, height, dpi] = window;
    const auto rows =
        measured(window) ? core::palette_window_rows(width, height, dpi) : core::palette_row_limit;
    if (std::max(rows, std::size_t{1}) == palette.visible())
    {
        return std::nullopt;
    }
    return CommandInput{palette.with_visible(rows)};
}

[[nodiscard]] std::optional<CommandInput> fitted_rows(const core::CommandLine &line,
                                                      CommandWindow window)
{
    const auto [width, height, dpi] = window;
    const auto rows = measured(window)
                          ? core::command_layout(core::status_bar_layout(width, height, dpi), dpi,
                                                 core::command_completion_row_limit)
                                .visible_rows
                          : core::command_completion_row_limit;
    if (std::max(rows, std::size_t{1}) == line.completion_rows())
    {
        return std::nullopt;
    }
    return CommandInput{line.with_completion_rows(rows)};
}

[[nodiscard]] std::optional<CommandInput> fitted_rows(const core::SearchLine & /*line*/,
                                                      CommandWindow /*window*/)
{
    return std::nullopt;
}

// 状態の変換の表示値。本文と入力行のどちらに載せるかは呼び手が決める（ADR 0061 の決定 4）。
[[nodiscard]] std::optional<CompositionView>
composition_view_of(const std::optional<core::Composition> &composition)
{
    if (!composition.has_value())
    {
        return std::nullopt;
    }
    return CompositionView{composition.value().utf8,
                           core::composition_underlines(composition.value()),
                           composition.value().cursor};
}

// 格子の照合の窓の余裕（#556）。一致の長さの上限が決まらない（`*` の繰り返し）検索は、この余裕の
// 外へ延びる一致が窓の境目で切れる（ADR 0140 の限界）。数は text の表示の窓と同じだが、ここでは
// バイト（text の表示の窓はコードポイント）。
constexpr std::size_t grid_unbounded_margin = core::giant_line_half_width;
// 窓の前後に読み足すバイト数。窓の端を code point の境目へ寄せる分と、`^` `\<` `\>` が見る
// 隣の 1 code point の分（UTF-8 で 4 バイトずつ）。
constexpr std::size_t grid_context_bytes = 8;

// 見えている範囲 visible に余裕 margin を前後に足し、行の内容 [line.begin, line.end) に収めた窓。
[[nodiscard]] core::OffsetRange
grid_search_window(core::OffsetRange line, core::OffsetRange visible, std::size_t margin) noexcept
{
    const auto begin =
        visible.begin.value - std::min(visible.begin.value - line.begin.value, margin);
    const auto end = visible.end.value + std::min(line.end.value - visible.end.value, margin);
    return core::OffsetRange{core::Offset{begin}, core::Offset{end}};
}

// bytes の at を code point の始まりへ前に寄せる / 後ろへ寄せる。
[[nodiscard]] std::size_t boundary_before(std::string_view bytes, std::size_t at) noexcept
{
    while (at > 0 && !core::is_boundary(bytes, core::Offset{at}))
    {
        --at;
    }
    return at;
}

[[nodiscard]] std::size_t boundary_after(std::string_view bytes, std::size_t at) noexcept
{
    while (at < bytes.size() && !core::is_boundary(bytes, core::Offset{at}))
    {
        ++at;
    }
    return at;
}

// 窓 window（読んだ bytes の中の位置）の当たりを、窓の前後の 1 code point の文脈を付けて照合し、
// 本文の位置 read_begin から数えた本文の位置で返す。文脈は読んだ範囲の中だけで、読んだ範囲の端が
// 行の端なら `^` `$` はそこで当たる。
[[nodiscard]] std::vector<core::OffsetRange> grid_window_matches(std::string_view bytes,
                                                                 core::OffsetRange window,
                                                                 std::size_t read_begin,
                                                                 const core::VimPattern &pattern)
{
    const auto from = boundary_before(bytes, window.begin.value);
    const auto to = boundary_after(bytes, window.end.value);
    const auto first = from == 0 ? 0 : core::previous_code_point(bytes, core::Offset{from}).value;
    const auto last =
        to == bytes.size() ? to : core::next_code_point(bytes, core::Offset{to}).value;
    auto matches = core::vim_window_matches(
        bytes.substr(first, last - first),
        core::OffsetRange{core::Offset{from - first}, core::Offset{to - first}}, pattern);
    for (auto &match : matches)
    {
        match = core::OffsetRange{core::Offset{read_begin + first + match.begin.value},
                                  core::Offset{read_begin + first + match.end.value}};
    }
    return matches;
}

// text の表示の 1 行 shown の当たりを本文の位置で返す。巨大な行の窓（ADR 0139）は格子と同じく
// 窓の前後の 1 code point を本文から文脈に読み足し、grid_window_matches で照合する（#556・
// 照合の経路は 1 本）。窓の無い行は窓 = 行の全体なので、文脈を読み足さず shown をそのまま使う。
[[nodiscard]] std::vector<core::OffsetRange>
shown_matches(const core::TextBuffer &text, const LineView &shown, const core::VimPattern &pattern)
{
    const core::OffsetRange content{text.line_start(shown.number), text.line_end(shown.number)};
    const core::Offset begin{content.begin.value + shown.window.byte};
    const core::OffsetRange window{begin, core::Offset{begin.value + shown.text.size()}};
    const auto read = grid_search_window(content, window, grid_context_bytes);
    if (read == window)
    {
        return grid_window_matches(
            shown.text, core::OffsetRange{core::Offset{0}, core::Offset{shown.text.size()}},
            begin.value, pattern);
    }
    const auto bytes = text.text_range(read.begin, read.end);
    return grid_window_matches(
        bytes,
        core::OffsetRange{core::Offset{window.begin.value - read.begin.value},
                          core::Offset{window.end.value - read.begin.value}},
        read.begin.value, pattern);
}
} // namespace

EditorController::EditorController(EditorPorts ports, const std::vector<OpenDocument> &initial)
    : ports_(ports),
      state_(EditorState::create(appearance_or_dark(ports.appearance), core::EditMode::ordinary))
{
    const auto inventory = ports_.themes.read();
    state_ = state_.with_themes(inventory.catalog);
    const auto loaded = ports_.settings.read(state_.themes());
    if (!loaded)
    {
        state_ = state_.with_settings_failure(loaded.error());
    }
    else
    {
        state_ = state_.with_settings(loaded.value().value_or(state_.settings()));
    }
    // 起動引数のファイルは順にタブで開き、最後に開けたものがアクティブになる（ADR 0056 の決定
    // 13）。失敗は 1 意図だけの値なので、後の引数が消さないよう最後の失敗を控えて載せ直す。
    std::optional<FileFailure> failure;
    for (const OpenDocument &document : initial)
    {
        static_cast<void>(apply(document));
        failure = state_.last_failure().has_value() ? state_.last_failure() : failure;
    }
    state_ = state_.with_failure(failure);
    // ファイルの引数が無いときだけ前回のタブを戻す（ADR 0059 の決定 6・D24）。
    if (initial.empty())
    {
        restore_session();
    }
    // 起動の問いは最後にアクティブになった文書だけ（ADR 0140 の決定 2）。途中で開いた文書の印は
    // 次の引数の意図が消すので、判定は起動の終わりの 1 回だけ。ほかの文書は見るときに判定する。
    starting_ = false;
    ask_display();
    // 初期ファイルも通常の意図を通す。その後で起動時の診断を載せ、最初の描画まで保持する。
    // 前回のタブの知らせがあればそれを残す（ステータスバーは 1 行）。
    if (!state_.command_message().has_value())
    {
        state_ = state_.with_command_message(inventory.notice);
    }
}

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

void EditorController::accept(const AdjustFontSize &intent)
{
    auto settings = state_.settings();
    settings.font_size =
        core::adjusted_font_size(settings.font_size, intent.adjustment, intent.steps);
    (void)persist_settings(std::move(settings));
}

bool EditorController::persist_settings(core::EditorSettings settings)
{
    if (core::same_settings(settings, state_.settings()) && !state_.settings_failure().has_value())
    {
        return true;
    }
    const auto saved = ports_.settings.write(settings);
    if (!saved)
    {
        state_ = state_.with_settings_failure(saved.error());
        return false;
    }
    const bool reshaped = core::affects_line_height(state_.settings(), settings);
    state_ = state_.with_settings(std::move(settings)).with_settings_failure(std::nullopt);
    if (view_geometry_.width <= 0)
    {
        return true;
    }
    if (reshaped)
    {
        // 古い行高のまま組まない。clamp も頼むだけにし、窓が送る新しい幾何の後に解決する
        // （ADR 0128 の決定 8）。
        pending_clamp_ = true;
        follow_caret();
        return true;
    }
    clamp_scroll();
    follow_caret();
    return true;
}

EditorDelivery EditorController::apply(const EditorIntent &intent)
{
    layout_failure_.reset();
    if (!keeps_wanted_x(intent))
    {
        wanted_x_.reset();
    }
    settle_tab_walk_before(keeps_tab_walk(intent));
    // 窓の寸法と帯の上のマウスは文書に触れないので、Vim の告知を消さない（ADR 0056 の決定 3）。
    // Ctrl を離したときの使った順の確定も同じ（ADR 0058 の決定 3）。窓が閉じていくときの一覧の
    // 書き出しも同じ（ADR 0059 の決定 3）。裏の仕事の合図も同じ（ADR 0062 の決定 7）。
    // 自動退避の合図とプレビューの間引きの合図も使う人の操作ではないので同じ（ADR 0137 の決定 1・
    // ADR 0155 の決定 8）。起動の問いの答えは使う人の
    // 操作なので、ほかの意図と同じに 1 意図の知らせを消してから写す（決定 6）。
    begin_intent(std::holds_alternative<VisibleLines>(intent) ||
                 std::holds_alternative<RefreshAppearance>(intent) ||
                 std::holds_alternative<CancelComposition>(intent) ||
                 std::holds_alternative<TitleBarWidth>(intent) ||
                 std::holds_alternative<ScrollTabs>(intent) ||
                 std::holds_alternative<PointTitleBar>(intent) ||
                 std::holds_alternative<SettleRecentTab>(intent) ||
                 std::holds_alternative<EndSession>(intent) ||
                 std::holds_alternative<WorkCompleted>(intent) ||
                 std::holds_alternative<AutosaveTick>(intent) ||
                 std::holds_alternative<PreviewTick>(intent));
    // 写し先が足りなければここでコンパイルが落ちる＝意図が増えたことに機械が気づく（CPP-002）。
    // SelectDisplay は使う人の操作の側（歩きを終え、告知を消す）なので上の 2 つの表に載せない。
    // SelectLanguage も同じ（ADR 0154 の決定 4）。
    // 面のホイール ScrollPaletteList も使う人の操作の側なので載せない（ADR 0150 の決定 2）。
    std::visit([this](const auto &value) { this->accept(value); }, intent);
    fit_command_rows();
    ask_display();
    auto delivered = delivery();
    // 契約の破れは配達に 1 回だけ載せ、ここで消す（ADR 0133 の追補 2026-10-18）。次の apply の
    // 消去に頼らない。layout を作れない失敗は作り直しの apply で消えるまで frame に載る。
    layout_failure_ = drawing_failure(layout_failure_);
    return delivered;
}

EditorFrame EditorController::apply_frame(const EditorIntent &intent)
{
    static_cast<void>(apply(intent));
    return frame();
}

bool EditorController::is_composing() const noexcept
{
    return state_.composition().has_value();
}

std::vector<DocumentView> EditorController::documents() const
{
    return tab_views(state_, active_document_view());
}

// fixture と契約の harness が打つ 1 鍵（ADR 0048 の決定 8）。入力行の写しは再生と同じ
// command_key の 1 か所を通り、打った鍵なので <Esc> は取消のまま。
EditorFrame EditorController::press_vim_key(const core::VimKey &key)
{
    settle_tab_walk_before(false);
    begin_intent(false);
    static_cast<void>(deliver_vim_key(key));
    fit_command_rows();
    return frame();
}

// fixture の harness が Vim の :normal! の意味で鍵の列を流す口（Issue #230）。1 鍵の流し方は
// press_vim_key と同じで、鍵が閉じた失敗で終わったら残りの鍵を流さない（再生の打ち切りと同じ
// deliver_vim_key の返り値で決める）。
EditorFrame EditorController::press_vim_keys(std::span<const core::VimKey> keys)
{
    for (const core::VimKey &key : keys)
    {
        settle_tab_walk_before(false);
        begin_intent(false);
        const bool failed = deliver_vim_key(key).has_value();
        fit_command_rows();
        if (failed)
        {
            break;
        }
    }
    return frame();
}

// 歩いている間に歩きを続けない意図や Vim の鍵が来たら、写す前に SettleRecentTab と同じ確定を
// 通す（ADR 0058 の決定 4）。controller の入口はどれもここを通るので、窓を経ない経路でも確定が
// 漏れない。1 打鍵ごとに通るので、歩いていないときは欄を 1 つ読むだけで抜ける。
void EditorController::settle_tab_walk_before(bool keeps_walk)
{
    if (state_.tab_walking() && !keeps_walk)
    {
        accept(SettleRecentTab{});
    }
}

void EditorController::begin_intent(bool keeps_message)
{
    // ファイルの失敗は 1 つの意図のあいだだけ表示値に載る（ADR 0010 の決定 9）。
    // 最後のタブを閉じる印も同じく 1 意図だけ（ADR 0056 の決定 6）。
    // 一覧で頼まれた操作も同じく 1 意図だけ（ADR 0078 の決定 8）。
    // 格子表示を聞く印も同じく 1 意図だけ（ADR 0140 の決定 2）。
    if (state_.last_failure().has_value() || state_.closing() ||
        state_.close_request().has_value() || state_.operation_request().has_value() ||
        state_.display_question())
    {
        state_ = state_.with_intent_cleared();
    }
    unreached_tabs_ = 0;
    if (state_.command_message().has_value() && !keeps_message)
    {
        state_ = state_.with_command_message(std::nullopt);
    }
}

bool EditorController::command_line_active() const noexcept
{
    return state_.command_input().has_value();
}

bool EditorController::command_palette_active() const noexcept
{
    const auto &input = state_.command_input();
    return input.has_value() && std::holds_alternative<core::CommandPalette>(input.value());
}

std::optional<core::InputLineView> EditorController::command_line_view() const
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return std::nullopt;
    }
    return input_line_of(input.value());
}

std::optional<CommandPaletteView> EditorController::command_palette_view() const
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return std::nullopt;
    }
    if (const auto *palette = std::get_if<core::CommandPalette>(&input.value()))
    {
        // 記号の案内は入力が空で、入力行が変換中でないときだけ（ADR 0060 の決定 9）。
        // 1 文字でも打てば消え、変換を始めても消える（長い変換の文字列に重ねない）。
        auto hint = palette->input().text().empty() && !command_composed().has_value()
                        ? std::optional<core::DisplayText>{core::palette_mark_hint()}
                        : std::nullopt;
        // 載せるのは見えている行の窓だけ（ADR 0062 の決定 2）。全件は写さない。窓の先頭は面の
        // 状態（ADR 0150 の決定 1）。
        const auto first = palette->first();
        return CommandPaletteView{palette->rows(first, palette->visible()), first,
                                  palette->selected(), palette->count(), std::move(hint)};
    }
    return std::nullopt;
}

// 打ち切りは 1 回の知らせではなく今の面の状態（ADR 0062 の決定 17）。状態の知らせが出る意図では
// その 1 回だけそちらが出て、次の意図で打ち切りの 1 行へ戻る。面を閉じると出ない。
std::optional<core::DisplayText> EditorController::command_message() const
{
    if (state_.command_message().has_value())
    {
        return state_.command_message();
    }
    if (!command_palette_active() || !palette_folder_.truncated)
    {
        return std::nullopt;
    }
    return core::DisplayText::parse("同じフォルダは " + std::to_string(palette_folder_.received) +
                                    " 件まで。残りは一覧に出ません")
        .value();
}

void EditorController::fail(FileFailure failure)
{
    state_ = state_.with_failure(failure);
}

void EditorController::replace(const core::OffsetRange &range, std::string_view text,
                               core::EditBoundary boundary)
{
    // 編集を始める瞬間のキャレット。Vim の u と Ctrl-r の戻り先で、書くのはここだけ（ADR 0052 の
    // 決定 2）。
    const core::Offset restore = state_.selection().caret;
    std::string removed = state_.text().text_range(range.begin, range.end);
    if (removed.empty() && text.empty())
    {
        return;
    }
    if (boundary != core::EditBoundary::absorb)
    {
        interrupt_vim_insert();
    }
    auto next = state_.text().replaced(range.begin, range.end, text);
    const core::Edit edit{range.begin, std::move(removed), std::string(text), restore};
    // 行がずれたマークを直し、最後の変更の位置を置く（ADR 0152 の決定 6・7）。
    auto marks = core::marks_adjusted(state_.vim_marks(), state_.text(), edit);
    marks.last_change = core::vim_last_change_of(state_.text(), edit);
    state_ = state_.with_vim_marks(std::move(marks));
    const core::Offset caret{range.begin.value + text.size()};
    const std::size_t before = state_.history().position();
    auto history = state_.history().pushed(edit, boundary);
    auto document = after_edit_at(state_.document(), before);
    adjust_other(edit, next);
    state_ = state_.with_edit(std::move(next), core::collapsed_at(caret), std::move(history),
                              std::move(document));
    follow_caret();
}

void EditorController::adjust_other(const core::Edit &edit, const core::TextBuffer &after)
{
    // 1 枠では状態を写さない（1 打鍵の道に写しを足さない）。本文を置く前に呼ぶ（先頭行の行頭は
    // 編集の前の本文で引く）。
    if (state_.split().other.has_value())
    {
        state_ = state_.with_other_adjusted(edit, after);
    }
}

void EditorController::move_caret_to(core::Offset caret, core::SelectionAnchoring anchoring,
                                     core::CaretAffinity affinity)
{
    const core::Offset anchor = anchor_for(anchoring, state_.selection().anchor, caret);
    state_ = state_.with_selection(core::Selection{anchor, caret}).with_caret_affinity(affinity);
    follow_caret();
}

void EditorController::follow_caret()
{
    follow_position(state_.text().position_of(state_.selection().caret));
}

// 位置が見える範囲に入るようにスクロールすることを頼む。キャレットと incsearch の preview が
// 同じ計算を使う（ADR 0041 の決定 3）。layout は作らず、解決はスクロールを読む所で 1 回だけ
// （ADR 0128 の決定 1・2）。
void EditorController::follow_position(core::TextPosition position)
{
    pending_follow_ = position;
}

// 保留を先に空にしてから view を作るので、visual_view() の先頭の解決へ再入しない。解決はその
// 時点の state で 1 回（ADR 0128 の決定 4）。保留の clamp は追従より先（決定 8）。問いの答えを
// 待つ間は文字組みせず、保留の追従と clamp を消費せずに残して戻る。答えて表示形式が決まった後の
// 最初のスクロールの読みで解決する（ADR 0140 の決定 2）。
void EditorController::settle_follow()
{
    if (display_pending(state_.display_inquiry()))
    {
        return;
    }
    const bool clamp = std::exchange(pending_clamp_, false);
    const auto pending = std::exchange(pending_follow_, std::nullopt);
    if (clamp)
    {
        clamp_scroll();
    }
    if (!pending.has_value())
    {
        return;
    }
    const auto followed =
        visual_view().followed(core::TextCaret{pending.value(), state_.caret_affinity()},
                               scroll_follow_for(state_.mode()), scroll_extent_for(state_.mode()));
    if (!followed)
    {
        layout_failure_ = followed.error();
        return;
    }
    if (!(followed.value() == state_.scroll()))
    {
        state_ = state_.with_scroll(followed.value());
    }
}

// 問いの答えを待つ間の保留は、追従も clamp も、その文書を離れるときに捨てる。問いの答え
// （SelectDisplay）が clamp_scroll と follow_caret を頼み直すので失うものは無い（ADR 0128 の
// 決定 7）。
void EditorController::drop_pending_follow()
{
    if (display_pending(state_.display_inquiry()))
    {
        pending_follow_.reset();
        pending_clamp_ = false;
    }
}

const ScrollState &EditorController::settled_scroll()
{
    settle_follow();
    return state_.scroll();
}

void EditorController::accept(const InsertText &intent)
{
    replace(core::selection_range(state_.selection()), intent.utf8, core::EditBoundary::coalesce);
}

void EditorController::accept(const MoveCaret &intent)
{
    interrupt_vim_insert();
    if (move_visual(intent))
    {
        return;
    }
    const core::Offset caret =
        core::moved_caret(state_.text(), state_.selection().caret,
                          core::CaretMoveRequest{intent.motion, settled_scroll().visible_lines,
                                                 caret_unit(state_.mode())});
    move_caret_to(caret, anchoring_in(state_.mode(), intent.anchoring));
    settle_vim_caret();
}

void EditorController::accept(const PlaceCaret &intent)
{
    interrupt_vim_insert();
    move_caret_to(state_.text().offset_of(intent.position),
                  anchoring_in(state_.mode(), intent.anchoring), intent.affinity);
    settle_vim_caret();
}

void EditorController::accept(const CancelSelection &)
{
    // Vim の NORMAL への遷移は Vim エンジンの縦切りで同じ意図に載せる（ADR 0009 の決定 3）。
    switch (state_.mode())
    {
    case core::EditMode::vim:
        return;
    case core::EditMode::ordinary:
        break;
    }
    move_caret_to(state_.selection().caret, core::SelectionAnchoring::collapse);
}

void EditorController::accept(const DeleteText &intent)
{
    const auto selected = core::selection_range(state_.selection());
    const auto range = core::is_empty(selected) ? deletion_range(state_.text(), selected.begin,
                                                                 intent.direction, state_.mode())
                                                : selected;
    replace(range, std::string_view{}, core::EditBoundary::separate);
}

void EditorController::accept(const NewLine &)
{
    replace(core::selection_range(state_.selection()), core::newline_of(state_.line_ending()),
            core::EditBoundary::separate);
}

void EditorController::accept(const SelectAll &)
{
    interrupt_vim_insert();
    state_ = state_.with_selection(
        core::Selection{core::Offset{0}, core::Offset{state_.text().size_bytes()}});
    follow_caret();
}

void EditorController::accept(const ClipboardAction &intent)
{
    switch (intent.operation)
    {
    case ClipboardOperation::copy:
        copy_selection();
        return;
    case ClipboardOperation::cut:
        cut_selection();
        return;
    case ClipboardOperation::paste:
        paste_clipboard();
        return;
    case ClipboardOperation::paste_plain:
        if (pastes_from_os())
        {
            paste_text();
        }
        return;
    case ClipboardOperation::paste_html:
        if (pastes_from_os())
        {
            paste_html_fragment();
        }
        return;
    }
    std::unreachable();
}

// 描く選択と Ctrl+C / Ctrl+X が覆う本文。VISUAL のあいだだけ Vim の規則で決まり、
// 表示と操作が同じ 1 本の範囲を使う（ADR 0018 の決定 5）。
core::OffsetRange EditorController::highlighted_range() const
{
    switch (state_.mode())
    {
    case core::EditMode::ordinary:
        return core::selection_range(state_.selection());
    case core::EditMode::vim:
        break;
    }
    return core::vim_visual_range(state_.text(), state_.selection(), state_.vim().mode).range;
}

// 矩形は 1 つの範囲では表せないので、モードで先に分かれる（ADR 0035 の決定 2）。
std::optional<core::VimBlockRange> EditorController::block_selection() const
{
    switch (state_.mode())
    {
    case core::EditMode::ordinary:
        return std::nullopt;
    case core::EditMode::vim:
        break;
    }
    switch (state_.vim().mode)
    {
    case core::VimMode::normal:
    case core::VimMode::insert:
    case core::VimMode::visual:
    case core::VimMode::visual_line:
        return std::nullopt;
    case core::VimMode::visual_block:
        break;
    }
    return core::vim_block_range_for(state_.text(), state_.selection(), state_.vim().wanted_column);
}

// 開けない・置けない・変換できないのどれも、使う人には同じ 1 行で見せる。本文は変えない
// （ADR 0134 の決定 3・ARC-010）。成功の表示は出さない。
bool EditorController::clipboard_written(std::string_view text)
{
    if (ports_.clipboard.write(text))
    {
        return true;
    }
    state_ =
        state_.with_command_message(core::DisplayText::parse(clipboard_unwritten_notice).value());
    return false;
}

void EditorController::copy_selection()
{
    const auto block = block_selection();
    if (block.has_value())
    {
        // 矩形は行を文書の改行でつないで置く（ADR 0035 の決定 8）。
        static_cast<void>(clipboard_written(
            with_document_newlines(core::vim_block_text(state_.text(), block.value()),
                                   core::newline_of(state_.line_ending()))));
        return;
    }
    const auto range = highlighted_range();
    if (core::is_empty(range))
    {
        return;
    }
    // 置けなかったことは本文を変えず、知らせだけ出す（ADR 0134 の決定 3）。
    static_cast<void>(clipboard_written(state_.text().text_range(range.begin, range.end)));
}

void EditorController::cut_selection()
{
    const auto block = block_selection();
    if (block.has_value())
    {
        // 置けたときだけ切り取る。消す形は engine の `d` と同じ 1 本（ADR 0035 の決定 8）。
        if (!clipboard_written(
                with_document_newlines(core::vim_block_text(state_.text(), block.value()),
                                       core::newline_of(state_.line_ending()))))
        {
            return;
        }
        perform(core::vim_remove_block(block.value()));
        return;
    }
    const auto range = highlighted_range();
    if (core::is_empty(range))
    {
        return;
    }
    // 置けたときだけ切り取る。行き先の無いまま本文から消す方が損害が大きい。
    if (!clipboard_written(state_.text().text_range(range.begin, range.end)))
    {
        return;
    }
    replace(range, std::string_view{}, core::EditBoundary::separate);
}

// 一覧から選ぶ貼り付けの操作が効くか（ADR 0157 の決定 2・3）。Vim の NORMAL / VISUAL では OS の
// 貼り付けをしない（Ctrl+V は矩形の鍵・ADR 0035 の決定 9）。INSERT と通常モードだけ。
bool EditorController::pastes_from_os() const noexcept
{
    switch (state_.mode())
    {
    case core::EditMode::ordinary:
        return true;
    case core::EditMode::vim:
        break;
    }
    return state_.vim().mode == core::VimMode::insert;
}

// Ctrl+V（ADR 0157 の決定 1）。HTML を読むのは、HTML があれば貼り方が変わる言語の文書だけ
// （決定 9）。HTML が無ければ今までどおり文字を読む。
void EditorController::paste_clipboard()
{
    const core::Language language = state_.language();
    const auto fragment =
        paste_kind_of(language, true) == PasteKind::text ? std::nullopt : clipboard_html();
    switch (paste_kind_of(language, fragment.has_value()))
    {
    case PasteKind::text:
        paste_text();
        return;
    case PasteKind::markdown_from_html:
        paste_markdown(fragment.value_or(std::string{}));
        return;
    case PasteKind::html_as_is:
        paste_fragment(fragment.value_or(std::string{}));
        return;
    }
    std::unreachable();
}

// 「HTML のまま貼り付け」（決定 3）。HTML が無ければ文字だけ貼って 1 行知らせる（鳴らさない）。
void EditorController::paste_html_fragment()
{
    const auto fragment = clipboard_html();
    if (fragment.has_value())
    {
        paste_fragment(fragment.value());
        return;
    }
    paste_text();
    state_ = state_.with_command_message(core::DisplayText::parse(html_missing_notice).value());
}

// 変換できなければ文字だけ貼って 1 行知らせる（決定 7・鳴らさない）。
void EditorController::paste_markdown(std::string_view fragment)
{
    const auto markdown = core::markdown_from_html(fragment);
    if (!markdown.has_value())
    {
        paste_text();
        state_ =
            state_.with_command_message(core::DisplayText::parse(html_unconverted_notice).value());
        return;
    }
    // 変換の結果が空（script だけ・src の無い img だけなど）なら、黙って文字の貼り付けに戻す。
    // 空を貼って選択を消さない（レビュー R3）。
    if (markdown.value().empty())
    {
        paste_text();
        return;
    }
    insert_pasted(markdown.value());
}

// fragment のまま貼る。空の fragment は黙って文字の貼り付けに戻す（空を貼って選択を消さない）。
void EditorController::paste_fragment(std::string_view fragment)
{
    if (fragment.empty())
    {
        paste_text();
        return;
    }
    insert_pasted(fragment);
}

// 読めない HTML は「HTML が無い」と同じに扱う。開けない失敗は文字の読みでも同じく黙る（ADR 0134）。
std::optional<std::string> EditorController::clipboard_html()
{
    auto fragment = ports_.clipboard.html();
    if (!fragment.has_value())
    {
        return std::nullopt;
    }
    return std::move(fragment.value());
}

void EditorController::paste_text()
{
    const auto pasted = ports_.clipboard.read();
    if (!pasted)
    {
        return;
    }
    insert_pasted(pasted.value());
}

// 貼る道は 1 本（決定 8）。改行は `"+p` と同じ 2 段で文書の形に揃える（ADR 0055 の決定 2）。単独の
// `\r` は文字のまま。undo は 1 単位。
void EditorController::insert_pasted(std::string_view pasted)
{
    replace(core::selection_range(state_.selection()),
            with_document_newlines(core::clipboard_line_feeds(pasted),
                                   core::newline_of(state_.line_ending())),
            core::EditBoundary::separate);
}

void EditorController::accept(const HistoryAction &intent)
{
    interrupt_vim_insert();
    switch (intent.direction)
    {
    case core::HistoryDirection::undo:
        undo_edit();
        return;
    case core::HistoryDirection::redo:
        redo_edit();
        return;
    }
    std::unreachable();
}

// undo と Ctrl-r の編集でマークを直す（ADR 0152 の決定 6）。最後の変更は編集の行の行頭（Vim の
// u_undoredo の changed_lines）。edit は今の本文に効く向きの編集。
void EditorController::undone_marks(const core::Edit &edit)
{
    auto marks = core::marks_adjusted(state_.vim_marks(), state_.text(), edit);
    marks.last_change = core::VimMarkPosition{state_.text().position_of(edit.at).line, 0};
    state_ = state_.with_vim_marks(std::move(marks));
}

void EditorController::undo_edit()
{
    const auto edit = state_.history().undo();
    if (!edit)
    {
        return;
    }
    const core::Offset at = edit.value().at;
    const core::Offset end{at.value + edit.value().inserted.size()};
    auto next = state_.text().replaced(at, end, edit.value().removed);
    const core::Offset caret{at.value + edit.value().removed.size()};
    // undo は編集の逆向き（inserted を removed に戻す）。
    adjust_other(core::Edit{at, edit.value().inserted, edit.value().removed, at}, next);
    undone_marks(core::Edit{at, edit.value().inserted, edit.value().removed, at});
    state_ = state_.with_edit(std::move(next), core::collapsed_at(caret), state_.history().undone(),
                              state_.document());
    follow_caret();
}

void EditorController::redo_edit()
{
    const auto edit = state_.history().redo();
    if (!edit)
    {
        return;
    }
    const core::Offset at = edit.value().at;
    const core::Offset end{at.value + edit.value().removed.size()};
    auto next = state_.text().replaced(at, end, edit.value().inserted);
    const core::Offset caret{at.value + edit.value().inserted.size()};
    adjust_other(core::Edit{at, edit.value().removed, edit.value().inserted, at}, next);
    undone_marks(edit.value());
    state_ = state_.with_edit(std::move(next), core::collapsed_at(caret), state_.history().redone(),
                              state_.document());
    follow_caret();
}

void EditorController::accept(const ScrollLines &intent)
{
    const auto &scroll = settled_scroll();
    const auto moved = visual_view().shifted(
        core::VisualPosition{scroll.first_visible, scroll.first_row}, intent.lines);
    if (!moved)
    {
        layout_failure_ = moved.error();
        return;
    }
    state_ = state_.with_scroll(ScrollState{moved.value().line, scroll.visible_lines,
                                            moved.value().row, scroll.horizontal});
    clamp_scroll();
}

void EditorController::clamp_scroll()
{
    // 起動時は幅が未確定。保存した画面行を最初のVisibleLinesまで保つ。問いの答えを待つ間も保つ
    // （ADR 0140 の決定 2・答えの SelectDisplay が収め直す）。
    if ((view_geometry_.width <= 0 && settled_scroll().first_row > 0) ||
        display_pending(state_.display_inquiry()))
    {
        return;
    }
    const auto &scroll = settled_scroll();
    const auto within =
        visual_view().constrained(core::VisualPosition{scroll.first_visible, scroll.first_row},
                                  scroll_extent_for(state_.mode()));
    if (!within)
    {
        layout_failure_ = within.error();
        return;
    }
    state_ = state_.with_scroll(ScrollState{within.value().line, scroll.visible_lines,
                                            within.value().row, scroll.horizontal});
}

void EditorController::accept(const VisibleLines &intent)
{
    const std::size_t lines = std::max<std::size_t>(intent.lines, 1);
    // 前の頼みは前の寸法で解決してから寸法を替える（ADR 0128 の決定 4）。行高を変えた設定の
    // 頼みだけは新しい寸法を待ち、下の clamp_scroll の読みで解決する（決定 8）。
    const std::size_t previous =
        pending_clamp_ ? state_.scroll().visible_lines : settled_scroll().visible_lines;
    view_geometry_ = intent.geometry;
    window_width_ = intent.window_width;
    window_height_ = intent.window_height;
    state_ = state_.with_vim(core::vim_after_resize(state_.vim(), previous, lines));
    auto scroll = state_.scroll();
    scroll.visible_lines = lines;
    state_ = state_.with_scroll(scroll);
    clamp_scroll();
    follow_caret();
}

void EditorController::accept(const SelectEditMode &intent)
{
    close_command_input();
    // モードが変わる途中の変換は捨てる（ADR 0014 の決定 3）。
    state_ = state_.with_composition(std::nullopt);
    // Vim に入ると NORMAL で、回数もオペレータも空。通常へ戻るときも同じ形に捨てる（決定 10）。
    core::VimState vim = core::vim_resting_from(state_.vim(), state_.vim().unnamed_register);
    state_ = state_.with_mode(intent.mode).with_vim(std::move(vim));
    switch (intent.mode)
    {
    case core::EditMode::ordinary:
    {
        clamp_scroll();
        return;
    }
    case core::EditMode::vim:
        break;
    }
    // NORMAL のキャレットは文字の上にあるので、行末を越えていたら 1 つ左へ寄せる。寸法が届く前と
    // 表示形式の問いの間は今までどおりキャレットを追う。
    if (view_geometry_.width <= 0 || display_pending(state_.display_inquiry()))
    {
        move_caret_to(core::vim_resting_caret(state_.text(), state_.selection().caret),
                      core::SelectionAnchoring::collapse);
        return;
    }
    // 届いた後は画面を動かさず、外れたキャレットを見えている端の行へ入れてから寄せる。追従は
    // 頼まない（#785・ADR 0012 の決定 10 の追補）。
    const auto entered = caret_in_view();
    if (!entered)
    {
        layout_failure_ = entered.error();
    }
    const core::Offset caret =
        entered ? state_.text().offset_of(entered.value().position) : state_.selection().caret;
    const core::Offset resting = core::vim_resting_caret(state_.text(), caret);
    state_ = state_.with_selection(core::Selection{resting, resting})
                 .with_caret_affinity(core::CaretAffinity::leading);
}

// 窓は入力行の鍵を VimKeyPress ではなく CommandText / EditCommand / SubmitCommand /
// CancelCommand で送る。入力行（設定一覧を含む）が開いているあいだに届いた Vim の鍵は捨てる。
void EditorController::accept(const VimKeyPress &intent)
{
    if (state_.command_input().has_value())
    {
        return;
    }
    static_cast<void>(step_vim(intent.key));
}

// 再生の鍵の口（ADR 0048 の決定 8）。入力行が開いていれば鍵を窓と同じ入力行の intent に写し、
// 閉じていれば engine へ流す。確定した検索の鍵は入力行を経ずに engine へ。入力行の鍵は
// 失敗しない。失敗を返すのは engine の鍵（入力行の確定が送る検索の鍵を含む）だけ。
std::optional<core::VimRepeatFailure> EditorController::deliver_vim_key(const core::VimKey &key)
{
    if (!state_.command_input().has_value())
    {
        return step_vim(key);
    }
    return std::visit([this](const auto &value) { return this->command_key(value); }, key);
}

std::optional<core::VimRepeatFailure> EditorController::command_key(const core::VimCharacter &key)
{
    std::string utf8;
    core::append_utf8(utf8, key.code);
    accept(CommandText{std::move(utf8)});
    return std::nullopt;
}

// 入力行での特殊鍵の写し（ADR 0048 の決定 8）。窓と同じ EditCommand の写しで、上下（履歴）と
// 入力行に意味の無い鍵は捨てる。再生の中の <Esc> は Vim の c_<Esc> と同じに確定する。
std::optional<core::VimRepeatFailure> EditorController::command_key(core::VimSpecialKey key)
{
    switch (key)
    {
    case core::VimSpecialKey::enter:
        return confirmed_command();
    case core::VimSpecialKey::escape:
        if (replay_depth_.has_value())
        {
            return submitted_command();
        }
        accept(CancelCommand{});
        return std::nullopt;
    case core::VimSpecialKey::backspace:
        accept(EditCommand{core::CommandEdit::backspace});
        return std::nullopt;
    case core::VimSpecialKey::arrow_left:
        accept(EditCommand{core::CommandEdit::left});
        return std::nullopt;
    case core::VimSpecialKey::arrow_right:
        accept(EditCommand{core::CommandEdit::right});
        return std::nullopt;
    case core::VimSpecialKey::home:
        accept(EditCommand{core::CommandEdit::home});
        return std::nullopt;
    case core::VimSpecialKey::end:
        accept(EditCommand{core::CommandEdit::end});
        return std::nullopt;
    case core::VimSpecialKey::arrow_up:
    case core::VimSpecialKey::arrow_down:
    case core::VimSpecialKey::control_r:
    case core::VimSpecialKey::page_up:
    case core::VimSpecialKey::page_down:
    case core::VimSpecialKey::control_d:
    case core::VimSpecialKey::control_u:
    case core::VimSpecialKey::control_f:
    case core::VimSpecialKey::control_b:
    case core::VimSpecialKey::control_v:
    case core::VimSpecialKey::control_w:
        return std::nullopt;
    }
    std::unreachable();
}

std::optional<core::VimRepeatFailure>
EditorController::command_key(const core::VimSearchPattern &key)
{
    return step_vim(core::VimKey{key});
}

// 写しを読みうる状態なら、鍵を流す直前に OS の本文を読んで engine に置く（ADR 0051 の決定 3）。
// 読めなければ空のレジスタを置き、engine が空のレジスタと同じに拒む。
void EditorController::load_vim_clipboard()
{
    if (!core::vim_reads_clipboard(state_.vim()))
    {
        return;
    }
    const auto pasted = ports_.clipboard.read();
    state_ = state_.with_vim(core::vim_clipboard_loaded(
        state_.vim(),
        pasted ? core::vim_register_of_clipboard(pasted.value())
               : core::VimRegister{std::string{}, core::VimRegisterKind::uninitialized}));
}

// `"+` へ書いた本文は文書の改行で OS へ出す（ADR 0051 の決定 7・Ctrl+C と同じ形）。書けなくても
// 本文と無名は engine の結果のまま（copy_selection と同じ）で、知らせだけ出す
// （ADR 0134 の決定 3）。
void EditorController::send_vim_clipboard(const std::optional<core::VimRegister> &written)
{
    if (!written.has_value())
    {
        return;
    }
    static_cast<void>(clipboard_written(
        with_document_newlines(written.value().text, core::newline_of(state_.line_ending()))));
}

std::optional<core::VimRepeatFailure> EditorController::step_vim(const core::VimKey &key)
{
    load_vim_clipboard();
    const core::VimMode before = state_.vim().mode;
    const auto measured = measured_vim_step(key);
    if (!measured)
    {
        layout_failure_ = measured.error();
        // 契約の破れは描画の作り直しでは直らないので、鍵を拒否して 1 行知らせる（ADR 0133 の
        // 追補 2026-10-18）。layout を作れない失敗は ui が作り直す（知らせは出さない）。
        switch (measured.error())
        {
        case TextLayoutFailure::inconsistent:
            state_ = state_.with_command_message(
                core::DisplayText::parse(inconsistent_layout_notice).value());
            break;
        case TextLayoutFailure::unavailable:
            break;
        }
        return core::VimRepeatFailure::refused;
    }
    const auto &step = measured.value();
    state_ = state_.with_vim(step.next);
    // INSERT の出入りが undo の区切り（ADR 0012 の決定 6 / ADR 0009 の決定 3 の Vim 側）。
    if (before != step.next.mode && before != core::VimMode::insert)
    {
        state_ = state_.with_history(state_.history().sealed());
    }
    // オペレータと VISUAL の戻り先（ADR 0052 の決定 7）。効果を写す前にキャレットを置くので、
    // replace が覚える戻り先は Vim の保存の瞬間のカーソルと同じになる。スクロールは効果の後で追う。
    if (step.restore.has_value())
    {
        state_ = state_.with_selection(core::collapsed_at(step.restore.value()));
    }
    // マークは効果を写す前に置く。飛ぶ鍵が置いた前の位置もオペレータの編集で直る（ADR 0152 の
    // 決定 2・6・Vim の setpcmark の後の mark_adjust）。
    adopt_vim_marks(step.marks);
    // 写し先が足りなければここでコンパイルが落ちる＝効果が増えたことに機械が気づく（CPP-002）。
    std::visit([this](const auto &value) { this->perform(value); }, step.effect);
    // 検索の報せは Ex の結果と同じ 1 本に出し、次の入力で消える（ADR 0032 の決定 5）。
    if (step.notice.has_value())
    {
        state_ = state_.with_command_message(core::vim_search_message(step.notice.value()));
    }
    // 書けなかった知らせは検索の報せの後に置き、同じ鍵で両方起きたら書けなかった方を見せる。
    send_vim_clipboard(step.clipboard);
    if (before == core::VimMode::insert && before != step.next.mode)
    {
        state_ = state_.with_history(state_.history().sealed());
    }
    if (!state_.command_input().has_value())
    {
        settle_vim_caret();
    }
    // 鍵の終わりに、動かなかった・消えた前の位置を戻す（決定 5・Vim の checkpcmark）。
    check_vim_marks();
    return step.failure;
}

// 飛ぶ鍵の後だけ前の位置を確かめる（決定 5）。飛ばなかった鍵は位置を引かない。
void EditorController::check_vim_marks()
{
    if (!state_.vim_marks().prior_context.has_value())
    {
        return;
    }
    state_ = state_.with_vim_marks(core::vim_marks_checked(
        state_.vim_marks(), core::vim_mark_position_of(state_.text(), state_.selection().caret)));
}

void EditorController::adopt_vim_marks(const std::optional<core::VimMarks> &marks)
{
    if (marks.has_value())
    {
        state_ = state_.with_vim_marks(marks.value());
    }
}

void EditorController::settle_vim_caret()
{
    switch (state_.mode())
    {
    case core::EditMode::ordinary:
        return;
    case core::EditMode::vim:
        break;
    }
    switch (state_.vim().mode)
    {
    // VISUAL のキャレットは行の内容の終わりにも載る。寄せるのは NORMAL へ戻るときだけ
    // （ADR 0018 の決定 6）。選択も畳まない。
    case core::VimMode::insert:
    case core::VimMode::visual:
    case core::VimMode::visual_line:
    case core::VimMode::visual_block:
        return;
    case core::VimMode::normal:
        break;
    }
    move_caret_to(core::vim_resting_caret(state_.text(), state_.selection().caret),
                  core::SelectionAnchoring::collapse);
}

// Vim の効果を本文に写すときの undo の区切り（ADR 0015 の決定 5）。INSERT にいるあいだの編集は
// 直前の Edit に吸収して 1 単位にし、NORMAL の編集（x d p）は単位を切る。
// VimInsertAtはEsc時の反復も持つため、明示した境界を使う（ADR 0028）。
core::EditBoundary EditorController::vim_boundary() const noexcept
{
    switch (state_.vim().mode)
    {
    case core::VimMode::insert:
        return core::EditBoundary::absorb;
    case core::VimMode::normal:
    case core::VimMode::visual:
    case core::VimMode::visual_line:
    case core::VimMode::visual_block:
        return core::EditBoundary::separate;
    }
    std::unreachable();
}

void EditorController::perform(const core::VimNoEffect &) {}

void EditorController::perform(const core::VimOpenCommandLine &)
{
    state_ = state_.with_command_input(
        core::CommandLine::empty(state_.themes()).with_fonts(given_fonts()));
}

void EditorController::perform(const core::VimOpenSearch &effect)
{
    const ScrollState origin = settled_scroll();
    state_ = state_.with_command_input(core::SearchLine::opened(effect.direction));
    state_ = state_.with_search_preview(
        SearchPreview{std::nullopt, origin, state_.text().position_of(state_.selection().caret)});
}

// 入力行が変わるたびに 1 か所で呼ぶ（ADR 0041 の決定 3）。画面は毎回 origin から数え直すので、
// 打ち直しても同じ入力なら同じ位置に見える。当たりが無ければ origin のまま。
void EditorController::update_search_preview()
{
    const auto &preview = state_.search_preview();
    if (!preview.has_value())
    {
        return;
    }
    const ScrollState origin = preview.value().origin;
    const core::TextPosition from = preview.value().from;
    const auto match = previewed_match(state_, from);
    restore_search_origin(origin);
    state_ = state_.with_search_preview(SearchPreview{match, origin, from});
    if (match.has_value())
    {
        follow_position(match.value());
    }
}

// Ctrl-G / Ctrl-T（ADR 0043 の決定 2）。検索の入力行が開いていて incsearch が当たりを見せている
// ときだけ動く。向きは本文の順（Ctrl-G は下・Ctrl-T は上）で `/` `?` に依らない。確定の鍵は起点から
// 検索の向きへ回数ぶん探すので、起点は新しい当たりから検索の向きの逆へ同じ回数だけ戻った当たりに
// 置く（`/` の Ctrl-G では今の当たりになる）。折り返しは vim_find_match
// の規則のままで報せは出さない。
void EditorController::accept(const StoreVimRegister &intent)
{
    state_ = state_.with_vim(core::vim_register_stored(state_.vim(), intent.name, intent.value));
}

void EditorController::accept(const SearchHop &intent)
{
    // 変換中の鍵は IME のもの。検索の行の変換中は当たりを動かさない（ADR 0061 の #635 の追記）。
    // 変換中かは ui と同じ述語 is_composing の 1 本で見る。
    if (is_composing())
    {
        return;
    }
    const core::SearchLine *line = previewed_line(state_);
    const auto preview = state_.search_preview();
    if (line == nullptr || !preview.has_value() || !preview.value().match.has_value())
    {
        return;
    }
    const auto pattern = typed_pattern(*line);
    if (!pattern.has_value())
    {
        return;
    }
    const std::size_t count = core::vim_search_count(state_.vim());
    const auto match =
        found_match(state_.text(), pattern.value(),
                    core::VimMatchRequest{preview.value().match.value(), intent.relative, count});
    if (!match.has_value())
    {
        return;
    }
    const auto from =
        found_match(state_.text(), pattern.value(),
                    core::VimMatchRequest{match.value(), core::opposite(line->direction()), count});
    if (!from.has_value())
    {
        return;
    }
    state_ = state_.with_search_preview(SearchPreview{match, preview.value().origin, from.value()});
    follow_position(match.value());
}

// 見えている行数は入力中に窓の大きさで変わり得るので、戻すのは先頭行だけ。
void EditorController::restore_search_origin(const ScrollState &origin)
{
    const std::size_t visible = settled_scroll().visible_lines;
    state_ = state_.with_scroll(
        ScrollState{origin.first_visible, visible, origin.first_row, origin.horizontal});
}

void EditorController::accept(const CommandText &intent)
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return;
    }
    auto next = inserted_command(input.value(), intent.utf8);
    if (!next)
    {
        state_ = state_.with_command_message(core::ex_failure_message(next.error()));
        return;
    }
    state_ = state_.with_command_input(std::move(next).value());
    reach_font_families();
    update_search_preview();
}

void EditorController::accept(const EditCommand &intent)
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return;
    }
    // 空の入力行の Backspace は取消（Ex と検索で同じ・ADR 0032 の決定 1）。
    // 設定一覧は候補を出したまま残る。
    const bool palette = std::holds_alternative<core::CommandPalette>(input.value());
    if (!palette && input_line_of(input.value()).text.empty() &&
        intent.edit == core::CommandEdit::backspace)
    {
        accept(CancelCommand{});
        return;
    }
    state_ = state_.with_command_input(recalls_history(intent.edit)
                                           ? recalled_command(input.value(), state_.ex_history(),
                                                              state_.search_history(), intent.edit)
                                           : edited_command(input.value(), intent.edit));
    reach_font_families();
    update_search_preview();
}

void EditorController::accept(const ScrollPaletteList &intent)
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return;
    }
    if (const auto *palette = std::get_if<core::CommandPalette>(&input.value()))
    {
        state_ = state_.with_command_input(palette->scrolled(intent.lines));
    }
}

void EditorController::fit_command_rows()
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return;
    }
    const CommandWindow window{window_width_, window_height_, view_geometry_.dpi};
    auto fitted = std::visit([window](const auto &value) { return fitted_rows(value, window); },
                             input.value());
    if (fitted.has_value())
    {
        state_ = state_.with_command_input(std::move(fitted).value());
    }
}

void EditorController::accept(const CancelCommand &)
{
    const auto &input = state_.command_input();
    const bool searching =
        input.has_value() && std::holds_alternative<core::SearchLine>(input.value());
    // 入力行を閉じると preview も消えるので、戻す先を先に写す（ADR 0041 の決定 5）。
    const auto preview = state_.search_preview();
    close_command_input();
    if (preview.has_value())
    {
        restore_search_origin(preview.value().origin);
    }
    if (searching)
    {
        // 検索の取消は保留中のオペレータと回数も捨てる（`d/<Esc>` のあとの `x` が消すのと
        // 同じ・実測）。何を捨てるかは engine の純関数が決める（ADR 0032 の決定 1）。
        state_ = state_.with_vim(core::vim_cancelled_input(state_.vim()));
    }
}

void EditorController::accept(const OpenCommandPalette &)
{
    if (state_.composition().has_value())
    {
        return;
    }
    // 開いている面の Ctrl+P は前の候補（zsh / Vim と同じ）。閉じるのは Esc（ADR 0150 の決定 2）。
    if (command_palette_active())
    {
        accept(EditCommand{core::CommandEdit::complete_previous});
        return;
    }
    open_palette("", 0);
}

// 開いている間の OpenTabList は閉じる（ADR 0057 の決定 7）。
void EditorController::accept(const OpenTabList &)
{
    if (state_.composition().has_value())
    {
        return;
    }
    if (command_palette_active())
    {
        accept(CancelCommand{});
        return;
    }
    run_palette_request(core::ExPaletteRequest{core::PaletteScope::tabs, {}});
}

// 開いている入力行（検索の preview も）は取消と同じ道で閉じてから開く（ADR 0078 の決定 9）。
void EditorController::accept(const OpenOperationList &)
{
    if (state_.composition().has_value())
    {
        return;
    }
    if (command_line_active())
    {
        accept(CancelCommand{});
    }
    run_palette_request(core::ExPaletteRequest{core::PaletteScope::operations, {}});
}

// 面を開く（ADR 0062 の決定 14）。前の面のときに届いて残った分は collect で捨てる（たまりが空で
// ないと次の列挙の合図が来ない）。券を進めてから頼むので、前の面の分は券で見分けて捨てられる。
void EditorController::open_palette(std::string_view input, std::size_t selected)
{
    static_cast<void>(ports_.folders.collect());
    const auto bookmarks = ports_.bookmarks.read();
    auto entries = palette_entries(bookmarks.value_or(FileBookmarks{}));
    request_folder(entries);
    state_ = state_.with_command_input(
        core::CommandPalette::opened(
            std::move(entries), input, state_.mode(),
            core::CommandLine::empty(state_.themes()).with_fonts(given_fonts()))
            .selected_at(selected));
    if (!bookmarks)
    {
        state_ = state_.with_command_message(
            core::DisplayText::parse("ブックマークを読めませんでした").value());
    }
}

void EditorController::request_folder(const std::vector<core::CommandChoice> &entries)
{
    palette_folder_ = PaletteFolder{palette_folder_.ticket + 1, {}, 0, false};
    const auto &path = state_.document().path;
    if (!path.has_value())
    {
        return;
    }
    const auto folder = core::folder_of(path.value());
    if (!folder.has_value())
    {
        return;
    }
    palette_folder_.listed = listed_in_folder(folder.value(), entries);
    ports_.folders.list(FolderRequest{folder.value(), palette_folder_.ticket});
}

// 面を開いている間はタブも履歴も変わらないので、開くときに 1 回だけ作る（ADR 0062 の決定 15）。
// 比べるのはファイルのあるフォルダと頼んだフォルダで、比べ方は FilePort が OS の規則で決める。
std::vector<core::FilePath>
EditorController::listed_in_folder(const core::FilePath &folder,
                                   const std::vector<core::CommandChoice> &entries) const
{
    std::vector<core::FilePath> listed;
    // path は optional か expected（どちらも値が無ければ数えない）。
    const auto keep = [&](const auto &path)
    {
        if (path.has_value() && in_folder(path.value(), folder))
        {
            listed.push_back(path.value());
        }
    };
    for (const core::CommandChoice &entry : entries)
    {
        if (entry.kind == core::CommandChoiceKind::open)
        {
            keep(core::FilePath::parse(entry.command));
        }
    }
    return listed;
}

bool EditorController::in_folder(const core::FilePath &path, const core::FilePath &folder) const
{
    const auto home = core::folder_of(path);
    return home.has_value() && ports_.files.same_file(home.value(), folder);
}

// 名前と場所は履歴の候補と同じ作り方、確定はパスの文字列（open_listed へ写す）。
void EditorController::append_folder_choices(const std::vector<core::FilePath> &files,
                                             std::vector<core::CommandChoice> &choices) const
{
    for (const core::FilePath &file : files)
    {
        if (!core::folder_lists(file.file_name()) ||
            std::ranges::any_of(palette_folder_.listed, [&](const core::FilePath &shown)
                                { return ports_.files.same_file(shown, file); }))
        {
            continue;
        }
        choices.push_back(core::CommandChoice{core::tab_title_for(file, core::SaveState::saved),
                                              std::string(file.text()),
                                              core::CommandChoiceKind::open,
                                              core::tab_folder_for(file),
                                              core::PaletteOrigin::folder,
                                              std::nullopt,
                                              {}});
    }
}

// ファイルの候補はパスを持つopenの1本。無題だけはパスが無いのでtabnextを使う（ADR 0063）。
std::vector<core::CommandChoice>
EditorController::palette_tabs(const FileBookmarks &bookmarks) const
{
    const auto views = tab_views(state_, active_document_view());
    std::vector<core::CommandChoice> entries;
    entries.reserve(views.size() + bookmarks.files.size());
    for (std::size_t index = 0; index < views.size(); ++index)
    {
        const DocumentView &view = views.at(index);
        const bool named = view.path.has_value();
        const bool marked = named && bookmarks_contain(bookmarks, view.path.value(), ports_.files);
        entries.push_back(core::CommandChoice{
            view.title,
            named ? std::string(view.path.value().text()) : "tabnext " + std::to_string(index + 1),
            named ? core::CommandChoiceKind::open : core::CommandChoiceKind::execute,
            core::tab_folder_for(view.path),
            marked ? core::PaletteOrigin::bookmarked_tab : core::PaletteOrigin::tab,
            std::nullopt,
            {}});
    }
    return entries;
}

void EditorController::append_bookmark_choices(const FileBookmarks &bookmarks,
                                               std::vector<core::CommandChoice> &entries) const
{
    FileBookmarks listed;
    for (const core::FilePath &path : bookmarks.files)
    {
        if (!open_tab_of(path).has_value() && !bookmarks_contain(listed, path, ports_.files))
        {
            entries.push_back(core::CommandChoice{core::tab_title_for(path, core::SaveState::saved),
                                                  std::string(path.text()),
                                                  core::CommandChoiceKind::open,
                                                  core::tab_folder_for(path),
                                                  core::PaletteOrigin::bookmark,
                                                  std::nullopt,
                                                  {}});
            listed.files.push_back(path);
        }
    }
}

std::vector<core::CommandChoice>
EditorController::palette_entries(const FileBookmarks &bookmarks) const
{
    auto entries = palette_tabs(bookmarks);
    append_bookmark_choices(bookmarks, entries);
    const auto history = ports_.history.read();
    if (!history)
    {
        return entries;
    }
    for (const core::FilePath &path : history.value().files)
    {
        if (!open_tab_of(path).has_value() && !bookmarks_contain(bookmarks, path, ports_.files))
        {
            entries.push_back(core::CommandChoice{core::tab_title_for(path, core::SaveState::saved),
                                                  std::string(path.text()),
                                                  core::CommandChoiceKind::open,
                                                  core::tab_folder_for(path),
                                                  core::PaletteOrigin::history,
                                                  std::nullopt,
                                                  {}});
        }
    }
    return entries;
}

// 保存済みの文書か、面で選んだファイルだけ。設定/Ex/検索の文字列をパスとして解釈しない。
std::optional<core::FilePath> EditorController::bookmark_target() const
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return state_.document().path;
    }
    const auto *palette = std::get_if<core::CommandPalette>(&input.value());
    if (palette == nullptr)
    {
        return std::nullopt;
    }
    const auto choice = palette->choice_at(palette->selected());
    if (!choice.has_value() || choice.value().kind != core::CommandChoiceKind::open)
    {
        return std::nullopt;
    }
    const auto path = core::FilePath::parse(choice.value().command);
    return path.has_value() ? std::optional{path.value()} : std::nullopt;
}

bool EditorController::bookmark_toggle_allowed() const
{
    if (state_.composition().has_value() || (command_line_active() && !command_palette_active()))
    {
        return false;
    }
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return true;
    }
    const auto *palette = std::get_if<core::CommandPalette>(&input.value());
    // 設定のコマンド（:）と操作の一覧（?）の面にはファイルが無いので、何も出さず何もしない。
    return palette != nullptr && palette->scope() != core::PaletteScope::commands &&
           palette->scope() != core::PaletteScope::operations;
}

void EditorController::accept(const ToggleBookmark &)
{
    if (!bookmark_toggle_allowed())
    {
        return;
    }
    const auto path = bookmark_target();
    if (!path.has_value())
    {
        state_ = state_.with_command_message(
            core::DisplayText::parse("保存済みのファイルを選んでください").value());
        return;
    }
    const auto before = ports_.bookmarks.read();
    if (!before)
    {
        state_ = state_.with_command_message(
            core::DisplayText::parse("ブックマークを読めませんでした").value());
        return;
    }
    const bool removing = bookmarks_contain(before.value(), path.value(), ports_.files);
    const auto after = bookmarks_toggled(before.value(), path.value(), ports_.files);
    if (!after)
    {
        state_ = state_.with_command_message(
            core::DisplayText::parse("ブックマークは " + std::to_string(bookmark_limit) +
                                     " 件まで登録できます")
                .value());
        return;
    }
    if (!ports_.bookmarks.write(after.value()))
    {
        reject(core::DisplayText::parse("ブックマークを保存できませんでした").value());
        return;
    }
    close_command_input();
    state_ = state_.with_command_message(
        core::DisplayText::parse(removing ? "ブックマークを解除しました"
                                          : "ブックマークに追加しました")
            .value());
}

void EditorController::accept(const ActivateCommandChoice &intent)
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return;
    }
    if (const auto *palette = std::get_if<core::CommandPalette>(&input.value()))
    {
        if (intent.index < palette->count())
        {
            submit_palette(palette->selected_at(intent.index));
        }
    }
}

void EditorController::submit_palette(const core::CommandPalette &palette)
{
    const auto chosen = palette.choice_at(palette.selected());
    if (!chosen.has_value())
    {
        return;
    }
    const auto &choice = chosen.value();
    switch (choice.kind)
    {
    case core::CommandChoiceKind::execute:
        evaluate_command(choice.command);
        return;
    case core::CommandChoiceKind::open:
        // 入力を閉じてから開く（evaluate_command と同じ順・choice は入力ではなく手元の列を指す）。
        // open の候補は palette_entries が検証済みの FilePath の text() から作るので parse は必ず
        // 成功する（不変条件）。失敗したら何もしない。
        close_command_input();
        if (const auto path = core::FilePath::parse(choice.command); path.has_value())
        {
            open_listed(path.value(), choice.origin);
        }
        return;
    case core::CommandChoiceKind::operate:
        // 面を閉じて頼まれた操作を 1 意図だけ立てる。実行は ui の run_operation で、controller は
        // 操作ごとの分岐を持たない（ADR 0078 の決定 7・8）。
        close_command_input();
        state_ = state_.with_operation_request(choice.operation);
        return;
    case core::CommandChoiceKind::fill:
        break;
    }
    auto next = palette.filled(choice.command);
    if (!next)
    {
        state_ = state_.with_command_message(core::ex_failure_message(next.error()));
        return;
    }
    state_ = state_.with_command_input(std::move(next).value());
    reach_font_families();
}

void EditorController::accept(const PasteCommand &)
{
    if (!state_.command_input().has_value())
    {
        return;
    }
    const auto text = ports_.clipboard.read();
    if (!text)
    {
        state_ =
            state_.with_command_message(core::DisplayText::parse("Clipboard unavailable").value());
        return;
    }
    accept(CommandText{text.value()});
}

std::optional<core::VimRepeatFailure> EditorController::submit(const core::CommandLine &line)
{
    evaluate_command(std::string(line.text()));
    return std::nullopt;
}

std::optional<core::VimRepeatFailure> EditorController::submit(const core::CommandPalette &palette)
{
    submit_palette(palette);
    return std::nullopt;
}

// 検索の確定は engine の 1 つの鍵（ADR 0032 の決定 3）。先に入力行を閉じてから送る。
std::optional<core::VimRepeatFailure> EditorController::submit(const core::SearchLine &line)
{
    // Vim も確定の前に入力前の画面へ戻してから本当の検索をする（ex_getln.c の
    // finish_incsearch_highlighting）。着いた先は engine の鍵のあとの follow_caret が見せる。
    // 鍵は preview の起点を運ぶ（Ctrl-G / Ctrl-T が無ければキャレットと同じ・ADR 0043 の決定 3）。
    const auto preview = state_.search_preview();
    const std::optional<core::TextPosition> from =
        preview.has_value() ? std::optional{preview.value().from} : std::nullopt;
    const core::VimSearchPattern pattern{std::string(line.text()), line.direction(), from};
    close_command_input();
    if (preview.has_value())
    {
        restore_search_origin(preview.value().origin);
    }
    const auto failure = step_vim(core::VimKey{pattern});
    // 検索の失敗（E486 E35 未対応の構文）は engine が not_found で返し、入力行の確定なので鳴らす。
    // 同じ報せでも後の `n` は鍵なので鳴らさない。layout の契約の破れ（refused）は使う人の断りでは
    // ないので鳴らさない（ADR 0153 の決定 1・2）。
    const auto &message = state_.command_message();
    if (failure == core::VimRepeatFailure::not_found && message.has_value())
    {
        reject(message.value());
    }
    return failure;
}

void EditorController::accept(const SubmitCommand &)
{
    static_cast<void>(confirmed_command());
}

// 確定の意図（Enter）だけが入力を履歴に足す。Esc は取消で、再生の中の <Esc> の確定
// （submitted_command を直接呼ぶ）も足さない（#725・ADR 0022 決定 2 の追補）。
std::optional<core::VimRepeatFailure> EditorController::confirmed_command()
{
    const auto &open = state_.command_input();
    if (open.has_value())
    {
        const CommandInput input = open.value();
        std::visit([this](const auto &value) { this->remember(value); }, input);
    }
    return submitted_command();
}

void EditorController::remember(const core::CommandLine &line)
{
    state_ = state_.with_ex_history(state_.ex_history().remembered(line.text()));
}

void EditorController::remember(const core::SearchLine &line)
{
    state_ = state_.with_search_history(state_.search_history().remembered(line.text()));
}

void EditorController::remember(const core::CommandPalette & /*palette*/)
{
    // 面の確定は履歴に足さない（#725）。
}

// 確定の失敗（検索の当たりが無い等）は再生を打ち切るので理由を返す（ADR 0048 の決定 8）。
std::optional<core::VimRepeatFailure> EditorController::submitted_command()
{
    const auto &open = state_.command_input();
    if (!open.has_value())
    {
        return std::nullopt;
    }
    // 入力行は state_ が持つので、写し先が状態を書き換える前に値ごと複製する。
    const CommandInput input = open.value();
    return std::visit([this](const auto &value) { return this->submit(value); }, input);
}

// 鳴らすかは呼んだ道で決め、文字列（E で始まるか）では決めない（ADR 0153 の決定 3）。
void EditorController::reject(core::DisplayText message)
{
    state_ = state_.with_command_message(std::move(message));
    ports_.bell.ring();
}

void EditorController::evaluate_command(std::string_view text)
{
    close_command_input();
    if (text.empty())
    {
        return;
    }
    const auto result =
        core::evaluate_ex(text, state_.settings(), state_.appearance(), state_.themes());
    if (!result)
    {
        reject(core::ex_failure_message(result.error(), text));
        return;
    }
    const auto &tab = result.value().tab;
    if (tab.has_value())
    {
        run_tab_request(tab.value());
        return;
    }
    const auto &window = result.value().window;
    if (window.has_value())
    {
        run_window_command(window.value());
        return;
    }
    const auto &palette = result.value().palette;
    if (palette.has_value())
    {
        run_palette_request(palette.value());
        return;
    }
    const auto &document = result.value().document;
    if (document.has_value())
    {
        run_document_request(document.value());
        return;
    }
    const auto &language = result.value().language;
    if (language.has_value())
    {
        answer_language(language.value());
        return;
    }
    const auto &settings = result.value().settings;
    if (settings.has_value() && !persist_settings(settings.value()))
    {
        reject(core::DisplayText::parse("Settings could not be saved").value());
        return;
    }
    apply_unsaved_ex(result.value());
    state_ = state_.with_command_message(result.value().message);
}

// Ex の答えのうち保存しない値（表示形式・検索の強調・incsearch）を写す。
void EditorController::apply_unsaved_ex(const core::ExResult &result)
{
    // 表示形式は文書の値で、窓の問いの「はい」と同じ 1 本を通る（ADR 0140 の決定 3）。
    const auto &display = result.display;
    if (display.has_value())
    {
        accept(SelectDisplay{display.value()});
    }
    // 検索の当たりの強調は設定ではなく Vim の状態なので、保存せずここで写す（ADR 0037 の決定 2）。
    const auto &highlight = result.highlight;
    if (highlight.has_value())
    {
        core::VimState vim = state_.vim();
        vim.highlight = core::requested_highlight(vim.highlight, highlight.value());
        state_ = state_.with_vim(std::move(vim));
    }
    // incsearch も同じく Vim の状態で、保存しない（ADR 0041 の決定 6）。
    const auto &incsearch = result.incsearch;
    if (incsearch.has_value())
    {
        core::VimState vim = state_.vim();
        vim.incsearch = incsearch.value();
        state_ = state_.with_vim(std::move(vim));
    }
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

void EditorController::run_palette_request(const core::ExPaletteRequest &request)
{
    const bool active = request.scope == core::PaletteScope::tabs && request.query.empty();
    open_palette(core::palette_input_for(core::PaletteQuery{request.scope, request.query}),
                 active ? state_.active_tab() : 0);
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

// 2 枠で操作中の枠を閉じる（ADR 0148 の決定 5）。もう 1 つの枠の表示を持ってきてから 1 枠へ。
// プレビューの枠へは移らないので、プレビューだけを閉じる（本文は閉じない・ADR 0155 の決定 4）。
void EditorController::close_operated_pane()
{
    if (state_.preview() == nullptr)
    {
        accept(SwitchPane{});
    }
    state_ = state_.without_split();
}

// `:only` はもう 1 つの枠を閉じる（ADR 0148 の決定 5）。
void EditorController::run_window_command(core::ExWindowCommand command)
{
    switch (command)
    {
    case core::ExWindowCommand::only:
        accept(ClosePane{});
        return;
    }
    std::unreachable();
}

// 行き先は gt / gT と同じ tab_destination の 1 本。範囲の外の数は Vim の実測の文言（決定 4・5）。
// `:tabclose` は状態を変えず、閉じたいタブの位置だけを載せる（決定 6）。
void EditorController::run_tab_request(const core::ExTabRequest &request)
{
    switch (request.verb)
    {
    case core::ExTabVerb::next:
    case core::ExTabVerb::previous:
        break;
    case core::ExTabVerb::open:
        accept(NewTab{});
        return;
    case core::ExTabVerb::close:
        state_ = state_.with_close_request(state_.active_tab());
        return;
    case core::ExTabVerb::list:
        accept(OpenTabList{});
        return;
    }
    const auto direction = request.verb == core::ExTabVerb::next ? core::TabJumpDirection::forward
                                                                 : core::TabJumpDirection::backward;
    const auto destination = core::tab_destination(core::TabJump{direction, request.number},
                                                   state_.active_tab(), state_.tab_count());
    if (!destination.has_value())
    {
        reject(core::DisplayText::parse("E475: Invalid argument: " +
                                        std::to_string(request.number.value_or(0)))
                   .value());
        return;
    }
    accept(SwitchTab{destination.value()});
}

// Vim の外から来た割り込み（クリック・Ctrl+Z・全選択・別経路の編集）。どれが割り込みかは
// ここが決め、何を捨てるかは engine の純関数 vim_interrupted が決める（Issue #92 / ARC-004）。
// undo の単位を切るのは履歴を持つこちらの仕事（ADR 0028 の決定 3）。
void EditorController::interrupt_vim_insert()
{
    if (state_.vim().mode == core::VimMode::insert)
    {
        state_ = state_.with_vim(core::vim_interrupted(state_.vim()))
                     .with_history(state_.history().sealed());
    }
}

// Vim の鍵による移動。入力記録を残すか捨てるかは engine が決めている（insert_moved が捨て、
// i a I A の入りは残す）ので、ここは undo の単位を切るだけにする（ARC-004）。
void EditorController::perform(const core::VimMoveTo &effect)
{
    if (state_.vim().mode == core::VimMode::insert)
    {
        state_ = state_.with_history(state_.history().sealed());
    }
    move_caret_to(effect.caret, core::SelectionAnchoring::collapse);
}

// 縦は engine の値を置く。横は first_column があればその leftcol を新しいキャレットの窓で
// 画素にして置き（追従は呼ばない）、無ければ今の値を保ってキャレットが見える最小の送りへ
// 1 回寄せる（ADR 0145 の決定 8・9）。横位置が求まらなければ、追従やスクロールの失敗と同じく
// layout_failure_ に記録して描画の失敗の道へ渡す（選択と縦の位置は置いたまま・横は前の値）。
void EditorController::perform(const core::VimNavigate &effect)
{
    const ScrollState before = settled_scroll();
    state_ = state_.with_selection(effect.selection)
                 .with_scroll(ScrollState{effect.first_visible, before.visible_lines,
                                          effect.first_row, before.horizontal});
    state_ = state_.with_history(state_.history().sealed());
    auto view = visual_view();
    const auto caret = core::TextCaret{state_.text().position_of(state_.selection().caret),
                                       state_.caret_affinity()};
    const auto horizontal =
        effect.first_column.has_value()
            ? view.horizontal_of_column(caret.position.line, effect.first_column.value())
            : view.followed_horizontal(caret);
    if (!horizontal)
    {
        layout_failure_ = horizontal.error();
        return;
    }
    auto scroll = state_.scroll();
    scroll.horizontal = horizontal.value();
    state_ = state_.with_scroll(scroll);
}

// VISUAL の選択（決定 3）。engine が決めた両端をそのまま置く。範囲にするのは表示と操作の側。
void EditorController::perform(const core::VimSelect &effect)
{
    state_ = state_.with_selection(effect.selection);
    follow_caret();
}

void EditorController::perform(const core::VimRemoveRange &effect)
{
    replace(effect.range, std::string_view{}, vim_boundary());
}

void EditorController::perform(const core::VimRemoveLines &effect)
{
    replace(effect.range, std::string_view{}, core::EditBoundary::separate);
    // 行を消したあとの Vim のキャレットは、その位置に来た行の最初の非空白。
    move_caret_to(core::vim_first_non_blank(state_.text(), effect.range.begin),
                  core::SelectionAnchoring::collapse);
}

void EditorController::perform(const core::VimInsertString &effect)
{
    replace(core::selection_range(state_.selection()), effect.utf8, vim_boundary());
}

void EditorController::perform(const core::VimNewLine &)
{
    replace(core::selection_range(state_.selection()), core::newline_of(state_.line_ending()),
            vim_boundary());
}

// p/P・o/O・Esc時の反復を同じ改行変換とreplaceへ流す（ADR 0028）。
void EditorController::perform(const core::VimInsertAt &effect)
{
    const std::string_view newline = core::newline_of(state_.line_ending());
    replace(core::OffsetRange{effect.at, effect.at}, with_document_newlines(effect.utf8, newline),
            effect.boundary);
    move_caret_to(caret_after_insert(effect.at, effect.utf8, effect.caret, newline.size()),
                  core::SelectionAnchoring::collapse);
}

void EditorController::perform(const core::VimReplaceRange &effect)
{
    const std::string_view newline = core::newline_of(state_.line_ending());
    replace(effect.range, with_document_newlines(effect.utf8, newline),
            core::EditBoundary::separate);
    move_caret_to(caret_after_insert(effect.range.begin, effect.utf8, effect.caret, newline.size()),
                  core::SelectionAnchoring::collapse);
}

// 矩形の編集（ADR 0035 の決定 3）。行ごとの置き換えを、上の行から下の行までを覆う 1 つの
// 置き換えに畳んでから既存の replace へ流す。こうすると履歴に入る Edit が 1 つになり、矩形
// 全体が undo 1 単位になる（固定 Vim も 1 回の `u` で戻す・実測）。改行は文書の形へ直す。
void EditorController::apply_block(const std::vector<core::VimBlockEdit> &edits, core::Offset caret)
{
    if (edits.empty())
    {
        move_caret_to(caret, core::SelectionAnchoring::collapse);
        return;
    }
    const std::string_view newline = core::newline_of(state_.line_ending());
    const core::OffsetRange span{edits.front().range.begin, edits.back().range.end};
    std::string body;
    core::Offset at = span.begin;
    for (const core::VimBlockEdit &edit : edits)
    {
        body += state_.text().text_range(at, edit.range.begin);
        body += with_document_newlines(edit.utf8, newline);
        at = edit.range.end;
    }
    replace(span, body, core::EditBoundary::separate);
    move_caret_to(caret, core::SelectionAnchoring::collapse);
}

void EditorController::perform(const core::VimRemoveBlock &effect)
{
    apply_block(effect.edits, effect.caret);
}

void EditorController::perform(const core::VimReplaceBlock &effect)
{
    apply_block(effect.edits, effect.caret);
}

void EditorController::perform(const core::VimInsertBlock &effect)
{
    apply_block(effect.edits, effect.caret);
}

// `.`（ADR 0030 の決定 7）。前後で履歴を閉じるので、再生した命令が 1 つの undo 単位になる。
// VISUAL の記録なら、鍵を流す前に core が決めた範囲を VimSelect と同じ写しで置く
// （ADR 0033 の決定 4。engine が mode を VISUAL にしてあるので、ここは選択だけを置く）。
//
// `@` も同じ効果で来る（ADR 0046 の決定 3・4）。再生の鍵は 1 本の列に積んで先頭から流す。
// 再生の中で `@` や `.` がまた VimReplay を返したら、その鍵を列の先頭へ差し込む（Vim が
// レジスタの中身を先読みの頭へ入れるのと同じ順）。流している鍵が閉じた失敗で終わったら列の
// 残りをすべて捨てる（Vim の「エラーで残りの打鍵を捨てる」）。差し込みの深さが上限を超えたら
// 何も差し込まずに残りを捨てる。入れ子を C++ の呼び出しの深さにしないので、深い再帰でも
// スタックを食わない。
void EditorController::perform(const core::VimReplay &effect)
{
    state_ = state_.with_history(state_.history().sealed());
    if (effect.reselect.has_value())
    {
        state_ = state_.with_selection(core::vim_visual_reselect(
            state_.text(), state_.selection().caret, effect.reselect.value()));
        follow_caret();
    }
    if (replay_depth_.has_value())
    {
        queue_nested_replay(effect.keys, replay_depth_.value() + 1);
        return;
    }
    replay_history_ = state_.history();
    replay_text_ = state_.text();
    replay_queue_.assign(effect.keys.begin(), effect.keys.end());
    replay_depths_.assign(effect.keys.size(), 1);
    while (!replay_queue_.empty())
    {
        const core::VimKey key = replay_queue_.front();
        replay_depth_ = replay_depths_.front();
        replay_queue_.pop_front();
        replay_depths_.pop_front();
        if (deliver_vim_key(key).has_value())
        {
            replay_queue_.clear();
            replay_depths_.clear();
        }
    }
    replay_depth_ = std::nullopt;
    close_replayed_unit();
    replay_history_ = std::nullopt;
    replay_text_ = std::nullopt;
    state_ = state_.with_history(state_.history().sealed());
}

void EditorController::close_replayed_unit()
{
    if (!replay_history_.has_value() || !replay_text_.has_value())
    {
        return;
    }
    merge_replayed_edits(replay_history_.value(), replay_text_.value());
}

void EditorController::reopen_replayed_unit()
{
    if (!replay_history_.has_value())
    {
        return;
    }
    replay_history_ = state_.history();
    replay_text_ = state_.text();
}

void EditorController::queue_nested_replay(const std::vector<core::VimKey> &keys, std::size_t depth)
{
    if (depth > core::vim_replay_depth_limit)
    {
        replay_queue_.clear();
        replay_depths_.clear();
        state_ = state_.with_command_message(
            core::DisplayText::parse("E132: Macro depth is higher than 100").value());
        return;
    }
    replay_queue_.insert(replay_queue_.begin(), keys.begin(), keys.end());
    replay_depths_.insert(replay_depths_.begin(), keys.size(), depth);
}

// 再生は命令ごとに編集を積むが、`[count]@a` と `.` の全体を 1 回の `u` で戻すのが Vim と同じ
// （ADR 0046 の決定 3・Issue #176 の probe で実測）。履歴の 1 単位は連続した 1 つの Edit なので、
// 2 つ以上積んだときは、再生の前後の本文の違う所をまとめて覆う 1 つの Edit に置き換える
// （矩形の編集を 1 つの範囲の置換にする apply_block と同じ考え方）。
void EditorController::merge_replayed_edits(const core::EditHistory &before,
                                            const core::TextBuffer &text)
{
    // 畳んだ単位の戻り先は再生が積んだ最初の編集の restore（ADR 0052 の決定 3）。
    const auto first = state_.history().applied(before.position());
    if (state_.history().position() <= before.position() + 1 || !first.has_value())
    {
        return;
    }
    const auto edit = covering_edit(text.text(), state_.text().text(), first.value().restore);
    if (!edit.has_value())
    {
        state_ = state_.with_history(before.sealed());
        return;
    }
    state_ = state_.with_history(before.pushed(edit.value(), core::EditBoundary::separate));
}

// Vim の u は戻した単位が覚えた戻り先（最初の編集の瞬間のキャレット）へ置き、行末の 1 つ先などの
// 寄せは step_vim の最後の settle_vim_caret に任せる（ADR 0052 の決定 4）。
void EditorController::perform(const core::VimUndo &)
{
    const auto edit = state_.history().undo();
    undo_edit();
    if (edit.has_value())
    {
        move_caret_to(edit.value().restore, core::SelectionAnchoring::collapse);
    }
}

// Vim の Ctrl-r は同じ戻り先の行とバイトの桁を、やり直した後の本文の同じ行と桁へ写す（ADR 0052 の
// 決定 5）。
void EditorController::perform(const core::VimRedo &)
{
    const auto edit = state_.history().redo();
    const core::TextBuffer before = state_.text();
    redo_edit();
    if (edit.has_value())
    {
        move_caret_to(core::vim_same_line_and_column(before, edit.value().restore, state_.text()),
                      core::SelectionAnchoring::collapse);
    }
}

void EditorController::perform(const core::VimSwitchTab &effect)
{
    accept(SwitchTab{effect.index});
}

// Vim の `Ctrl-w` は枠の鍵と同じ意図へ（ADR 0148 の決定 6・7）。2 枠なので隣は 1 つで、
// `h j k l` も `w` と同じく隣へ移る。`c` `q` は操作中の枠を閉じる（移ってからもう 1 つの枠を
// 閉じる）。1 枠では移る・閉じるは何もしない（Vim は最後の窓の `Ctrl-w c` を失敗にするが、Nib は
// 何もしない）。`o` はもう 1 つの枠を閉じる。
void EditorController::perform(const core::VimWindowCommand &command)
{
    switch (command)
    {
    case core::VimWindowCommand::split_horizontal:
        accept(SplitPane{core::SplitLayout::horizontal});
        return;
    case core::VimWindowCommand::split_vertical:
        accept(SplitPane{core::SplitLayout::vertical});
        return;
    case core::VimWindowCommand::next:
    case core::VimWindowCommand::left:
    case core::VimWindowCommand::down:
    case core::VimWindowCommand::up:
    case core::VimWindowCommand::right:
        accept(SwitchPane{});
        return;
    case core::VimWindowCommand::close:
        if (state_.split().other.has_value())
        {
            close_operated_pane();
        }
        return;
    case core::VimWindowCommand::only:
        accept(ClosePane{});
        return;
    }
    std::unreachable();
}

const core::VimState &EditorController::vim_state() const noexcept
{
    return state_.vim();
}

const SplitState &EditorController::split() const noexcept
{
    return state_.split();
}

void EditorController::accept(const RefreshAppearance &)
{
    state_ = state_.with_appearance(appearance_or_dark(ports_.appearance));
}

std::expected<core::TextBuffer, FileFailure> EditorController::decoded(core::DetectedText text)
{
    switch (text.encoding())
    {
    case core::TextEncoding::utf8:
    case core::TextEncoding::utf8_bom:
    case core::TextEncoding::utf16_le:
    case core::TextEncoding::utf16_be:
        return core::TextBuffer::from_utf8(std::move(text))
            .transform_error([](core::TextFailure) { return FileFailure::undecodable; });
    case core::TextEncoding::shift_jis:
        break;
    }
    auto converted = ports_.code_pages.to_utf8(text.bytes());
    if (!converted)
    {
        return std::unexpected(file_failure_of(converted.error()));
    }
    return core::TextBuffer::from_utf8(converted.value())
        .transform_error([](core::TextFailure) { return FileFailure::undecodable; });
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

// 読んで復号した本文と文書。上限はここが正本で、ポートへ引数で渡す。読んでから断るのでは
// 大きいファイルを先に抱える。
std::expected<std::pair<core::TextBuffer, Document>, FileFailure>
EditorController::read_document(const core::FilePath &path)
{
    auto bytes = ports_.files.read(path, maximum_file_bytes);
    if (!bytes)
    {
        return std::unexpected(bytes.error());
    }
    // 封印済みの列をそのまま判定へ移す。複写はしない（ADR 0141 決定 4）。
    auto detected = core::DetectedText::from_sealed(std::move(bytes).value());
    if (!detected)
    {
        return std::unexpected(file_failure_of(detected.error()));
    }
    const core::TextEncoding encoding = detected.value().encoding();
    auto text = decoded(std::move(detected).value());
    if (!text)
    {
        return std::unexpected(text.error());
    }
    return std::pair{std::move(text).value(), Document{path, encoding, std::size_t{0}}};
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

// 開く（ADR 0056 の決定 5）。(a) 同じファイルのタブへ切り替える（読み直さない）→ (b) 何も
// 書いていない無題ならそこに開く → (c) アクティブの右に新しいタブを足して開く。開けなかったら
// タブを足さず失敗を告げる。
// タブを足さず失敗を返す。失敗の告げ方は呼び出し元が決める（ADR 0060 の決定 7）。
std::expected<void, FileFailure> EditorController::open_document(const core::FilePath &path)
{
    const auto open = open_tab_of(path);
    if (open.has_value())
    {
        accept(SwitchTab{open.value()});
        return {};
    }
    // ファイルが変わる途中の変換は捨てる（ADR 0014 の決定 3）。
    state_ = state_.with_composition(std::nullopt);
    auto document = read_document(path);
    if (!document)
    {
        return std::unexpected(document.error());
    }
    auto [text, opened] = std::move(document).value();
    if (blank_untitled())
    {
        drop_pending_follow();
        settle_follow();
        state_ = state_.with_opened(std::move(text), std::move(opened));
        ask_display();
        // 本文が丸ごと替わるので、プレビューも頼み直す（ADR 0155 の決定 9）。
        request_preview(true);
        return {};
    }
    leave_document();
    state_ = state_.with_new_tab().with_opened(std::move(text), std::move(opened));
    enter_document();
    return {};
}

// Ctrl+O と起動引数の開く。失敗はダイアログ（ADR 0010 の決定 9）。
void EditorController::accept(const OpenDocument &intent)
{
    const auto opened = open_document(intent.path);
    if (!opened)
    {
        fail(opened.error());
    }
}

// 一覧から選んだファイルを開く（ADR 0060 の決定 7）。失敗はダイアログではなく 1 行の知らせで、
// 無くなっていたファイル（not_found）だけ履歴から外す（D30）。ほかの失敗は履歴に残す。
// 外すのは出どころが履歴の候補のときだけで、同じフォルダの候補では履歴を読み書きしない
// （ADR 0062 の決定 18）。
void EditorController::open_listed(const core::FilePath &path,
                                   std::optional<core::PaletteOrigin> origin)
{
    const auto opened = open_document(path);
    if (opened)
    {
        return;
    }
    reject(unreached_message(path, 0));
    if (opened.error() == FileFailure::not_found && origin == core::PaletteOrigin::history)
    {
        forget(path);
    }
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
    auto document = read_document(tab.path);
    if (!document)
    {
        // 読めなかったタブは帯と使った順から外し、1 行知らせる。ダイアログの経路（last_failure）
        // には載せない（D26: 窓を止めない）。
        state_ = state_.with_dropped(position);
        report_unreached(tab.path);
        reveal_active_tab();
        return false;
    }
    auto [text, opened] = std::move(document).value();
    state_ = state_.with_loaded(position, reached_bundle(std::move(text), std::move(opened), tab));
    return true;
}

void EditorController::report_unreached(const core::FilePath &path)
{
    ++unreached_tabs_;
    state_ = state_.with_command_message(unreached_message(path, unreached_tabs_ - 1));
}

void EditorController::accept(const NewTab &)
{
    leave_document();
    state_ = state_.with_new_tab();
    enter_document();
}

void EditorController::accept(const SwitchTab &intent)
{
    if (intent.index >= state_.tab_count())
    {
        return;
    }
    // アクティブ自身は文書を動かさず、歩きの途中なら着いた所で確定する（ADR 0058 の決定 2）。
    if (intent.index == state_.active_tab())
    {
        state_ = state_.with_switched(intent.index);
        return;
    }
    // 行き先がまだ読んでいない文書ならここで読む。読めなければそのタブは外れて知らせだけが残り、
    // 今の文書の一時の値（入力行・変換中の文字列）は閉じない（ADR 0059 の決定 5）。
    if (!reach_tab(intent.index))
    {
        return;
    }
    leave_document();
    state_ = state_.with_switched(intent.index);
    enter_document();
}

void EditorController::accept(const WalkRecentTab &intent)
{
    const auto destination =
        core::tab_recency_walked(state_.recency(), state_.active_tab(), intent.step);
    if (!destination.has_value())
    {
        return;
    }
    // 1 本のときの歩きは同じ位置で、文書は動かさず歩いている印だけを立てる。
    if (destination.value() == state_.active_tab())
    {
        state_ = state_.with_walked(destination.value());
        return;
    }
    // 行き先が読めなければ外れて、歩きは今のタブのまま続く。次の 1 歩は外れた後の使った順の
    // 隣へ行く（ADR 0059 の決定 5）。
    if (!reach_tab(destination.value()))
    {
        return;
    }
    leave_document();
    state_ = state_.with_walked(destination.value());
    enter_document();
}

// 窓が閉じていく（ADR 0059 の決定 3）。一覧を作るのはここだけで、1 打鍵の道には無い。書けなくても
// 窓は閉じていく途中なので、状態を変えず何も出さない。
void EditorController::accept(const EndSession &intent)
{
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

// 自動退避の合図（ADR 0137 の決定 1・2）。入口は歩きも知らせも残す。
void EditorController::accept(const AutosaveTick &)
{
    autosave_now();
}

// 計画は純関数で、帳面を更新するのはこの道だけ（合図と復元の直後）。本文の写しは UI スレッドで
// 作り、書くのは worker（決定 4）。
void EditorController::autosave_now()
{
    for (const auto &[id, step] : autosave_plan(state_, autosave_ledger_))
    {
        switch (step)
        {
        case AutosaveStep::store:
            store_planned(id);
            break;
        case AutosaveStep::discard:
            ports_.autosave.discard(id);
            autosave_ledger_.erase(id);
            break;
        case AutosaveStep::keep:
            break;
        }
    }
}

// 起動の問いの前の一覧（ADR 0137 の決定 6・7）。起動の道で自動退避に触れるのはこの 1 回だけ。
// 読めなければ戻すものは無い（問わない）。
const std::vector<AutosaveRemnant> &EditorController::autosave_remnants()
{
    autosave_remnants_ = ports_.autosave.remnants().value_or(std::vector<AutosaveRemnant>{});
    return autosave_remnants_;
}

// 「はい」（決定 6）。覚えた残骸を順に読み、戻せた分を帯の末尾へ足してから、最初の 1 本へ
// 切り替える。読めない残骸は知らせて飛ばす。戻したタブは次の合図を待たずにこのプロセスの退避と
// して頼み、その後にどの残骸も forget する（頼んだ書きが終わるまでの隙間は ADR の限界）。
void EditorController::accept(const RestoreAutosave &)
{
    const std::vector<AutosaveRemnant> remnants = std::exchange(autosave_remnants_, {});
    std::vector<DocumentState> restored;
    std::size_t unrestored = 0;
    for (const AutosaveRemnant &remnant : remnants)
    {
        auto bundle = restored_bundle(remnant);
        if (bundle.has_value())
        {
            restored.push_back(std::move(bundle).value());
            continue;
        }
        state_ = state_.with_command_message(unrestored_message(remnant, unrestored++));
    }
    if (!restored.empty())
    {
        const std::size_t first = state_.tab_count();
        state_ = state_.with_appended(std::move(restored));
        leave_document();
        state_ = state_.with_switched(first);
        enter_document();
        autosave_now();
    }
    for (const AutosaveRemnant &remnant : remnants)
    {
        ports_.autosave.forget(remnant);
    }
}

// 「いいえ」（決定 6）。本文を読まずに全部 forget する。
void EditorController::accept(const DiscardAutosave &)
{
    for (const AutosaveRemnant &remnant : std::exchange(autosave_remnants_, {}))
    {
        ports_.autosave.forget(remnant);
    }
}

// 開くのと同じ判別と復号（read_document と同じ DetectedText → decoded）。一覧のパスの符号化は
// 使わず、本文の判別が決める。
std::optional<DocumentState> EditorController::restored_bundle(const AutosaveRemnant &remnant)
{
    const auto bytes = ports_.autosave.read(remnant);
    if (!bytes)
    {
        return std::nullopt;
    }
    const std::span<const std::byte> body = bytes.value();
    auto detected = core::DetectedText::from_bytes(
        std::string_view{std::bit_cast<const char *>(body.data()), body.size()});
    if (!detected)
    {
        return std::nullopt;
    }
    const core::TextEncoding encoding = detected.value().encoding();
    auto text = decoded(std::move(detected).value());
    if (!text)
    {
        return std::nullopt;
    }
    return restored_document(std::move(text).value(),
                             Document{remnant.path, encoding, std::nullopt}, remnant.caret);
}

void EditorController::store_planned(AutosaveId id)
{
    const auto &ids = state_.autosave_ids();
    const auto position =
        static_cast<std::size_t>(std::ranges::distance(ids.begin(), std::ranges::find(ids, id)));
    if (position == state_.active_tab())
    {
        if (store_autosave(id, state_.text(), state_.document(),
                           state_.text().position_of(state_.selection().caret)))
        {
            autosave_ledger_.insert_or_assign(id, state_.history().revision());
        }
        return;
    }
    const auto bundle =
        std::visit([](const auto &tab) { return autosave_bundle(tab); },
                   state_.parked().at(position < state_.active_tab() ? position : position - 1));
    if (!bundle.has_value())
    {
        return;
    }
    const DocumentState &tab = *bundle.value();
    if (store_autosave(id, tab.text, tab.document, tab.text.position_of(tab.selection.caret)))
    {
        autosave_ledger_.insert_or_assign(id, tab.history.revision());
    }
}

bool EditorController::store_autosave(AutosaveId id, const core::TextBuffer &text,
                                      const Document &document, core::TextPosition caret)
{
    std::string held;
    const auto parts = encoded_parts(text, document.encoding, held);
    if (!parts)
    {
        notify_autosave_failure();
        return false;
    }
    // store にするのは未保存のタブだけなので、題名は未保存の印つき。
    ports_.autosave.store(AutosaveSnapshot{
        id, document.path, core::tab_title_for(document.path, core::SaveState::modified),
        joined_bytes(parts.value()), caret});
    return true;
}

// worker が終えた退避の結果を全部引く（決定 4・独立レビュー F3）。失敗した id は帳面から外し、
// 本文が変わらなくても次の合図で書き直す。失敗が 1 つでもあれば（まだ出していなければ）1 回
// 知らせ、失敗が無く成功が 1 つでもあれば知らせの印を下ろす（次の失敗でまた 1 回出る）。
// 結果が無ければ（フォルダの合図）何も変えない。
void EditorController::collect_autosave()
{
    const auto outcomes = ports_.autosave.collect();
    bool failed = false;
    for (const AutosaveOutcome &outcome : outcomes)
    {
        if (outcome.failure.has_value())
        {
            failed = true;
            autosave_ledger_.erase(outcome.id);
        }
    }
    if (failed)
    {
        notify_autosave_failure();
        return;
    }
    if (!outcomes.empty())
    {
        autosave_notified_ = false;
    }
}

void EditorController::notify_autosave_failure()
{
    if (autosave_notified_)
    {
        return;
    }
    autosave_notified_ = true;
    // 材料は固定の文字だけなので parse は必ず成功する（不変条件・ARC-010）。
    reject(core::DisplayText::parse(autosave_failed_message).value());
}

// 裏の仕事の合図（ADR 0062 の決定 7・15・17）。入口は歩きも知らせも残す。collect は面が開いて
// いなくても 1 回呼んでたまりを空にする。使うのは面が開いていて今の券の batch だけ。
// 届いた分は 1 回の extended で後ろへ足す（入力と選択は core が保つ）。
// 打ち切りは今の面の欄に覚えるだけで、知らせは frame を作る command_message() が出す。
// 入力・知らせ・ほかの状態には触れない。自動退避の失敗もここで引く（ADR 0137 の決定 4）。
void EditorController::accept(const WorkCompleted &)
{
    collect_autosave();
    const auto batches = ports_.folders.collect();
    // 退避 → フォルダ → Markdown の順（ADR 0155 の決定 6）。プレビューが閉じていれば呼ばない。
    collect_preview();
    const auto &input = state_.command_input();
    const auto *palette =
        input.has_value() ? std::get_if<core::CommandPalette>(&input.value()) : nullptr;
    if (palette == nullptr)
    {
        return;
    }
    std::vector<core::CommandChoice> more;
    for (const FolderBatch &batch : batches)
    {
        if (batch.ticket != palette_folder_.ticket)
        {
            continue;
        }
        palette_folder_.received += batch.files.size();
        append_folder_choices(batch.files, more);
        switch (batch.progress)
        {
        case FolderProgress::more:
        case FolderProgress::complete:
        case FolderProgress::failed:
            break;
        case FolderProgress::truncated:
            palette_folder_.truncated = true;
            break;
        }
    }
    if (!more.empty())
    {
        auto extended = palette->extended(std::move(more));
        state_ = state_.with_command_input(std::move(extended));
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

void EditorController::accept(const SettleRecentTab &)
{
    state_ = state_.with_walk_settled();
}

bool EditorController::tab_walking() const noexcept
{
    return state_.tab_walking();
}

const core::TextBuffer &EditorController::text() const noexcept
{
    return state_.text();
}

void EditorController::accept(const CloseTab &intent)
{
    if (intent.index >= state_.tab_count())
    {
        return;
    }
    // 最後の 1 つは状態を変えず、窓を閉じる印だけを立てる（D22）。
    if (state_.tab_count() == 1)
    {
        state_ = state_.with_closing(true);
        return;
    }
    // 閉じたタブに載せていたマウスは、そのタブと一緒に消える（ADR 0056 の決定 2）。
    const auto &hovered = state_.hovered();
    if (hovered.has_value() && hovered.value().tab == intent.index &&
        (hovered.value().hit == core::TitleBarHit::tab ||
         hovered.value().hit == core::TitleBarHit::tab_close))
    {
        state_ = state_.with_hovered(std::nullopt);
    }
    // 閉じるタブにパスがあれば、閉じた後に履歴へ記録する（ADR 0060 の決定 8）。最後の 1 本は上で
    // 窓を閉じる印だけを立て、窓が閉じるとき（EndSession）に記録する。隣が読めずに外れたタブは
    // 記録しない（使う人が閉じたのは 1 本だけ）。
    const std::optional<core::FilePath> closed = tab_path_at(state_, intent.index);
    // 脇のタブを閉じてもアクティブな文書は動かないので、一時の値も閉じない。
    if (intent.index != state_.active_tab())
    {
        state_ = state_.with_closed(intent.index);
        reveal_active_tab();
    }
    else
    {
        close_active_tab();
    }
    if (closed.has_value())
    {
        remember({closed.value()});
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

// ---------------------------------------------------------------- 枠（ADR 0148）

// 分けるのは同じ文書の 2 表示なので、読むものは無い（決定 2）。もう 1 つの枠は解決済みの
// スクロールを写す。
void EditorController::accept(const SplitPane &intent)
{
    // プレビューを捨てて本文の枠を置く（ADR 0155 の決定 4）。
    if (state_.preview() != nullptr)
    {
        state_ = state_.without_split();
    }
    if (state_.split().layout != core::SplitLayout::none)
    {
        state_ =
            state_.with_command_message(core::DisplayText::parse("もう分かれています").value());
        return;
    }
    settle_follow();
    state_ = state_.with_split(intent.layout);
}

// 枠を移る（決定 3・10）。違う文書ならタブの切り替えと同じく先に読む（読み込み済みのはず・
// 決定 2）。同じ文書でも入力行・変換・追従は移る前に閉じ、着いた枠のスクロールを収める
// （leave_document / enter_document の 1 対をそのまま通す）。
void EditorController::accept(const SwitchPane &)
{
    // プレビューの枠は操作中にならない（ADR 0155 の決定 4）。鳴らさず 1 行だけ知らせる。
    if (state_.preview() != nullptr)
    {
        state_ = state_.with_command_message(
            core::DisplayText::parse("プレビューの枠には入れません").value());
        return;
    }
    const PaneView *other = state_.other_pane();
    if (other == nullptr)
    {
        return;
    }
    const auto &ids = state_.autosave_ids();
    const auto found = std::ranges::find(ids, other->tab);
    if (found == ids.end() ||
        !reach_tab(static_cast<std::size_t>(std::distance(ids.begin(), found))))
    {
        // 読めなかった文書は reach_tab が帯から外して知らせる。もう 1 つの枠も外す。
        state_ = state_.without_split();
        return;
    }
    leave_document();
    state_ = state_.with_pane_swapped();
    enter_document();
}

void EditorController::accept(const ClosePane &)
{
    state_ = state_.without_split();
}

// もう 1 つの枠だけを送る（決定 11）。送り方と収め方は ScrollLines と同じ 2 本（shifted と
// constrained）を、もう 1 つの枠を操作中の枠の形で読む写しに通す。
void EditorController::accept(const ScrollPane &intent)
{
    // プレビューは錨を本文の行で動かし、追従をやめる（ADR 0155 の決定 7）。
    if (const PreviewView *preview = state_.preview(); preview != nullptr)
    {
        PreviewView view = *preview;
        const core::TextBuffer &text = state_.text();
        const core::LineNumber top = settled_scroll().first_visible;
        const core::Offset base = view.follows ? text.line_start(top) : view.anchor;
        const auto line = static_cast<std::int64_t>(text.position_of(base).line.value);
        const auto last = static_cast<std::int64_t>(text.line_count());
        const auto moved = std::clamp(line + std::int64_t{intent.lines}, std::int64_t{1}, last);
        view.anchor = text.line_start(core::LineNumber{static_cast<std::size_t>(moved)});
        view.follows = false;
        state_ = state_.with_preview(std::move(view));
        preview_body_top_ = top;
        return;
    }
    const auto pane = state_.other_pane_state();
    if (!pane.has_value())
    {
        return;
    }
    const EditorState &state = pane.value();
    VisualTextView view(state, ports_.text_layouts,
                        display_pending(state.display_inquiry()) ? core::TextViewGeometry{}
                                                                 : view_geometry_);
    const ScrollState &scroll = state.scroll();
    const auto moved =
        view.shifted(core::VisualPosition{scroll.first_visible, scroll.first_row}, intent.lines)
            .and_then([&view, this](core::VisualPosition at)
                      { return view.constrained(at, scroll_extent_for(state_.mode())); });
    if (!moved)
    {
        layout_failure_ = moved.error();
        return;
    }
    state_ = state_.with_other_scrolled(moved.value().line, moved.value().row);
}

// ---------------------------------------------------------------- プレビュー（ADR 0155）

// 開く・閉じるの 1 本（決定 3）。開く前に、前に開いていたときの残りを捨てる（たまりが空でないと
// 次の合図が来ない・ADR 0062 の決定 15 と同じ）。
void EditorController::accept(const TogglePreview &)
{
    if (state_.preview() != nullptr)
    {
        state_ = state_.without_split();
        return;
    }
    static_cast<void>(ports_.markdown.collect());
    settle_follow();
    state_ = state_.with_preview(PreviewView{});
    request_preview(true);
}

// 打つのを止めてから 1 tick 以上たった本文だけを頼む（決定 8）。比べるのは履歴の位置ではなく
// 改訂番号。
void EditorController::accept(const PreviewTick &)
{
    const PreviewView *preview = state_.preview();
    if (preview == nullptr)
    {
        return;
    }
    const std::size_t revision = state_.history().revision();
    if (revision == preview->seen_revision && revision != preview->requested_revision)
    {
        request_preview(false);
        return;
    }
    if (revision != preview->seen_revision)
    {
        PreviewView view = *preview;
        view.seen_revision = revision;
        state_ = state_.with_preview(std::move(view));
    }
}

void EditorController::request_preview(bool fresh)
{
    const PreviewView *preview = state_.preview();
    if (preview == nullptr)
    {
        return;
    }
    PreviewView view = *preview;
    const std::size_t revision = state_.history().revision();
    view.requested_revision = revision;
    view.seen_revision = revision;
    if (fresh)
    {
        view.document.reset();
        view.notice.reset();
        view.follows = true;
    }
    // 券は頼まなくても進め、前の要求の結果を捨てる（決定 6）。
    view.ticket = ++preview_tickets_;
    if (state_.text().size_bytes() > core::preview_byte_limit)
    {
        view.notice = core::DisplayText::parse(preview_too_large_message).value();
        state_ = state_.with_preview(std::move(view));
        return;
    }
    auto text = std::make_shared<const std::string>(state_.text().text());
    ports_.markdown.parse(MarkdownRequest{view.ticket, std::move(text)});
    state_ = state_.with_preview(std::move(view));
}

void EditorController::collect_preview()
{
    const PreviewView *preview = state_.preview();
    if (preview == nullptr)
    {
        return;
    }
    PreviewView view = *preview;
    bool changed = false;
    for (MarkdownOutcome &outcome : ports_.markdown.collect())
    {
        if (outcome.ticket != view.ticket)
        {
            continue;
        }
        if (outcome.result)
        {
            view.document =
                std::make_shared<const core::MarkdownDocument>(std::move(outcome.result).value());
            view.notice.reset();
            changed = true;
            continue;
        }
        // 差し替わるまで前の結果を出したまま、1 行だけ添える（決定 6）。
        switch (outcome.result.error())
        {
        case core::MarkdownFailure::stopped:
            continue;
        case core::MarkdownFailure::too_large:
            view.notice = core::DisplayText::parse(preview_too_large_message).value();
            break;
        case core::MarkdownFailure::parser_rejected:
            view.notice = core::DisplayText::parse(preview_rejected_message).value();
            break;
        case core::MarkdownFailure::insufficient_memory:
            view.notice = core::DisplayText::parse(preview_memory_message).value();
            break;
        }
        changed = true;
    }
    if (changed)
    {
        state_ = state_.with_preview(std::move(view));
    }
}

ScrollState EditorController::framed_scroll()
{
    const ScrollState scroll = settled_scroll();
    follow_preview_on_scroll(scroll.first_visible);
    return scroll;
}

void EditorController::follow_preview_on_scroll(core::LineNumber first_visible)
{
    const auto previous = std::exchange(preview_body_top_, first_visible);
    const PreviewView *preview = state_.preview();
    if (preview == nullptr || preview->follows || !previous.has_value() ||
        previous.value() == first_visible)
    {
        return;
    }
    PreviewView view = *preview;
    view.follows = true;
    state_ = state_.with_preview(std::move(view));
}

core::TitleBarInput EditorController::title_bar_input(std::int32_t width, std::uint32_t dpi) const
{
    return core::TitleBarInput{
        width, dpi, state_.tab_count(), state_.active_tab(), state_.tab_scroll(), state_.hovered()};
}

core::TitleBarInput EditorController::title_bar_input() const
{
    return title_bar_input(state_.title_bar_width(), core::reference_dpi);
}

void EditorController::reveal_active_tab()
{
    const std::int32_t scroll =
        state_.title_bar_width() > 0 ? core::tabs_scrolled_into_view(title_bar_input()) : 0;
    state_ = state_.with_tab_scroll(scroll);
}

void EditorController::accept(const TitleBarWidth &intent)
{
    state_ = state_.with_title_bar_width(std::max(intent.dip, 0));
    reveal_active_tab();
}

void EditorController::accept(const ScrollTabs &intent)
{
    if (state_.title_bar_width() <= 0)
    {
        return;
    }
    state_ = state_.with_tab_scroll(core::tabs_scrolled_by(title_bar_input(), intent.notches));
}

void EditorController::accept(const PointTitleBar &intent)
{
    state_ = state_.with_hovered(intent.target);
}

// ---------------------------------------------------------------- IME（ADR 0014）

bool EditorController::composition_ignored() const noexcept
{
    // 入力行（面・Ex の行・検索の行）は変換を受け、下のモード（Vim の NORMAL など）の決まりは
    // 見ない（ADR 0061 決定 3・#635 の追記）。
    if (command_line_active())
    {
        return false;
    }
    switch (state_.mode())
    {
    case core::EditMode::ordinary:
        return false;
    case core::EditMode::vim:
        break;
    }
    switch (state_.vim().mode)
    {
    case core::VimMode::insert:
        return false;
    case core::VimMode::normal:
    case core::VimMode::visual:
    case core::VimMode::visual_line:
    case core::VimMode::visual_block:
        return true;
    }
    std::unreachable();
}

void EditorController::accept(const ComposeText &intent)
{
    if (composition_ignored())
    {
        return;
    }
    // 本文と履歴は動かさず、差し込んだ表示中のキャレットへ画面だけを追従する（ADR 0114）。
    state_ = state_.with_composition(intent.composition);
    if (view_geometry_.width > 0 && !state_.command_input().has_value())
    {
        follow_caret();
    }
}

void EditorController::type_as_vim_keys(std::string_view utf8)
{
    core::Offset at{0};
    while (at.value < utf8.size())
    {
        accept(VimKeyPress{core::VimKey{core::VimCharacter{core::code_point_at(utf8, at)}}});
        at = core::next_code_point(utf8, at);
    }
}

void EditorController::accept(const CommitText &intent)
{
    const bool ignored = composition_ignored();
    // 確定したら変換は終わる。本文に入るかどうかに関わらず、変換中の文字列は先に消す。
    state_ = state_.with_composition(std::nullopt);
    if (ignored)
    {
        return;
    }
    // 入力行の確定は打った文字と同じ道で入力行に入る。本文にも engine にも行かない（ADR 0061 の
    // 決定 3・#635 の追記）。上限を越えたときの知らせも、検索の preview も同じ道が出す。
    if (command_line_active())
    {
        accept(CommandText{intent.utf8});
        return;
    }
    switch (state_.mode())
    {
    case core::EditMode::ordinary:
        // 通常モードは InsertText と同じ 1 本。確定 1 回が 1 つの undo 単位（決定 4）。
        accept(InsertText{intent.utf8});
        return;
    case core::EditMode::vim:
        break;
    }
    type_as_vim_keys(intent.utf8);
}

void EditorController::accept(const CancelComposition &)
{
    state_ = state_.with_composition(std::nullopt);
    if (view_geometry_.width > 0 && !state_.command_input().has_value())
    {
        follow_caret();
    }
}

// 読んだ字体の一覧（まだ読んでいなければ空の列）。
core::FontFamilies EditorController::given_fonts() const
{
    return font_families_.value_or(core::FontFamilies::none());
}

// 入力行が字体の補完を求めたら、字体の一覧をプロセスで 1 回だけ読んで入力行へ入れる（#597・D58）。
// 一覧は変わらないものとして再読しない。読めなければ空の列のまま（候補は打った値だけ・知らせなし）。
void EditorController::reach_font_families()
{
    const auto &input = state_.command_input();
    if (font_families_.has_value() || !input.has_value() || !wants_font_families(input.value()))
    {
        return;
    }
    auto listed = ports_.fonts.list();
    font_families_ = listed.has_value() ? core::FontFamilies::from(std::move(listed).value())
                                        : core::FontFamilies::none();
    state_ = state_.with_command_input(with_font_families(input.value(), font_families_.value()));
}

void EditorController::close_command_input()
{
    // 入力行の変換は入力行と一緒に消える（ADR 0061 の決定 3・#635 の追記）。
    const bool opened = command_line_active();
    state_ = state_.with_command_input(std::nullopt);
    if (opened)
    {
        state_ = state_.with_composition(std::nullopt);
    }
}

std::optional<CompositionView> EditorController::composed() const
{
    if (command_line_active())
    {
        return std::nullopt;
    }
    return composition_view_of(state_.composition());
}

std::optional<CompositionView> EditorController::command_composed() const
{
    if (!command_line_active())
    {
        return std::nullopt;
    }
    return composition_view_of(state_.composition());
}

std::optional<char> EditorController::recording_name() const
{
    const auto &recording = state_.vim().macro_recording;
    if (state_.mode() != core::EditMode::vim || !recording.has_value())
    {
        return std::nullopt;
    }
    // core は名前を小文字で持ち、追記（`qA`）は append の印で持つ。Vim は打った名前の
    // まま `recording @A` と出すので、追記なら大文字へ戻す。
    const char name = recording.value().name;
    return recording.value().append ? static_cast<char>(name - 'a' + 'A') : name;
}

std::optional<EditorController::SearchPattern> EditorController::search_pattern() const
{
    // 入力中は入力のパターンで塗る。解析できなければ何も塗らない（ADR 0041 の決定 4）。
    const core::SearchLine *typing = previewed_line(state_);
    if (state_.mode() == core::EditMode::vim && typing != nullptr)
    {
        auto parsed = typed_pattern(*typing);
        if (!parsed.has_value())
        {
            return std::nullopt;
        }
        return SearchPattern{std::move(parsed).value()};
    }
    const auto &remembered = state_.vim().last_search;
    if (state_.mode() != core::EditMode::vim ||
        state_.vim().highlight != core::VimSearchHighlight::on || !remembered.has_value())
    {
        return std::nullopt;
    }
    const auto &parsed = remembered.value().parsed();
    if (!parsed)
    {
        return std::nullopt;
    }
    return SearchPattern{std::cref(parsed.value())};
}

const core::VimPattern &EditorController::frame_pattern(const core::VimPattern &pattern)
{
    return pattern;
}

const core::VimPattern *EditorController::pattern_of(const std::optional<SearchPattern> &pattern)
{
    return pattern.has_value() ? std::visit([](const auto &value) { return &frame_pattern(value); },
                                            pattern.value())
                               : nullptr;
}

const core::VimPattern &
EditorController::frame_pattern(std::reference_wrapper<const core::VimPattern> pattern)
{
    return pattern.get();
}

// 見えている 1 行ぶんの表示値。選択と検索は所有済み本文の同じ LineSpanEvaluation で投影する
// （ADR 0037 / 0095）。塗る面を持たない長さ 0 の一致は span が absent を返すので落ちる。
// 描画用の行は core の display_line 1 本で作る（ADR 0040 の決定 2）。桁は本文の桁のまま渡す。
LineView EditorController::line_view(const PaneSource &source, LineView view,
                                     const core::OffsetRange &range, core::Offset caret) const
{
    const core::TextBuffer &text = source.state.text();
    const core::VimPattern *pattern = source.pattern;
    const auto line = view.number;
    // 巨大な行は窓の中の本文だけを持つ。選択と当たりは窓の始まりから投影し、窓が行末に届かなければ
    // 窓の終わりを行末とみなす（ADR 0139 の限界）。
    const core::Offset start{text.line_start(line).value + view.window.byte};
    const core::Offset window_end{start.value + view.text.size()};
    const core::Offset terminator =
        window_end == text.line_end(line) ? text.line_terminator_end(line) : window_end;
    LineSpanEvaluation projection(view.text, start, terminator);
    view.selection = projection.span(range);
    if (pattern == nullptr)
    {
        return view;
    }
    // 今の一致はキャレットを含む一致。incsearch の入力中は preview の当たりを含む一致（ADR 0041
    // の決定 4）。全一致の面は hlsearch が on のときだけで、off の入力中は今の当たりの枠だけ
    // （Vim と同じ）。入力中でなければ off の search_pattern は何も返さない。
    for (const auto &found : shown_matches(text, view, *pattern))
    {
        const core::SelectionSpan span = projection.span(found);
        if (span.presence != core::SelectionPresence::present)
        {
            continue;
        }
        view.matches.push_back(span);
        if (found.begin.value <= caret.value && caret.value < found.end.value)
        {
            view.current_match = span;
        }
    }
    if (state_.vim().highlight != core::VimSearchHighlight::on)
    {
        view.matches.clear();
    }
    return view;
}

// 枠の見えている行（ADR 0148 の決定 8）。操作中の枠ももう 1 つの枠もこの 1 本を通る。スクロールは
// 呼び手（frame）が解決済み。
std::expected<std::vector<LineView>, TextLayoutFailure>
EditorController::visible_lines(const PaneSource &source) const
{
    const EditorState &state = source.state;
    const ScrollState &scroll = state.scroll();
    const std::size_t total = state.text().line_count();
    const std::size_t first = std::min(scroll.first_visible.value, total);
    const std::size_t last =
        std::min(first + std::max<std::size_t>(scroll.visible_lines, 1) - 1, total);
    const core::Offset caret = source.pattern != nullptr && first <= last
                                   ? previewed_offset(state).value_or(state.selection().caret)
                                   : state.selection().caret;
    std::vector<LineView> lines;
    if (first <= last)
    {
        lines.reserve(last - first + 1);
    }
    // text の表示の frame なので、構築は 1 升を port から読まない（格子でない）。
    const VisualTextView visual(state, ports_.text_layouts, view_geometry_);
    auto row = -static_cast<std::int64_t>(scroll.first_row);
    for (std::size_t number = first;
         number <= last && row < static_cast<std::int64_t>(scroll.visible_lines); ++number)
    {
        const core::LineNumber line{number};
        auto shown = line_view(source, visual.line(line),
                               block_row(source.block, line, source.range), caret);
        const auto count = screen_rows_of(shown);
        if (!count)
        {
            return std::unexpected(count.error());
        }
        shown.screen_row = row;
        shown.screen_rows = count.value();
        row += static_cast<std::int64_t>(count.value());
        lines.push_back(std::move(shown));
    }
    return lines;
}

// 巨大な行は折り返さないので 1 画面行（ADR 0139 の決定 2）。
std::expected<std::size_t, TextLayoutFailure>
EditorController::screen_rows_of(const LineView &shown) const
{
    if (view_geometry_.width <= 0 || shown.window.active)
    {
        return 1;
    }
    return ports_.text_layouts.rows(
        TextLayoutRequest{presented_text(shown), state_.settings(), view_geometry_});
}

GridLineMarks EditorController::grid_marks(const PaneSource &source, core::LineNumber line,
                                           const core::OffsetRange &visible,
                                           const core::OffsetRange &selection) const
{
    const core::VimPattern *pattern = source.pattern;
    GridLineMarks marks{selection, {}, std::nullopt, 0};
    if (pattern == nullptr)
    {
        return marks;
    }
    // 照合は見えている画面行の本文の範囲に一致の長さの上限ぶんの余裕を前後に足した窓だけで、行の
    // 全体を写さない（#556）。余裕の中で始まる一致は見えている範囲に届かないので、窓の端の文脈で
    // 偽って当たっても塗られない。窓の始まりの前と終わりの後ろの 1 code point を文脈に読み、
    // `^` `$` は行の端でだけ当たる。塗るのは見えている画面行の升へ写した分だけ（GridLineRows）。
    const core::TextBuffer &text = source.state.text();
    const core::OffsetRange content{text.line_start(line), text.line_end(line)};
    const auto window = grid_search_window(
        content, visible, pattern->longest_bytes().value_or(grid_unbounded_margin));
    const auto read = grid_search_window(content, window, grid_context_bytes);
    const auto bytes = text.text_range(read.begin, read.end);
    marks.searched = bytes.size();
    const core::Offset caret =
        previewed_offset(source.state).value_or(source.state.selection().caret);
    for (const auto &found :
         grid_window_matches(bytes,
                             core::OffsetRange{core::Offset{window.begin.value - read.begin.value},
                                               core::Offset{window.end.value - read.begin.value}},
                             read.begin.value, *pattern))
    {
        if (found.begin.value >= found.end.value)
        {
            continue;
        }
        marks.matches.push_back(found);
        if (found.begin.value <= caret.value && caret.value < found.end.value)
        {
            marks.current = found;
        }
    }
    if (state_.vim().highlight != core::VimSearchHighlight::on)
    {
        marks.matches.clear();
    }
    return marks;
}

std::optional<TextLayoutFailure> EditorController::grid_visible_rows(const PaneSource &source,
                                                                     std::vector<GridRowView> &grid,
                                                                     std::size_t &scanned,
                                                                     std::size_t &searched) const
{
    const EditorState &state = source.state;
    const VisualTextView view(state, ports_.text_layouts, view_geometry_);
    auto cells = view.grid_cells();
    if (!cells)
    {
        return cells.error();
    }
    const auto caret = view.grid_point(
        cells.value(),
        core::TextCaret{state.text().position_of(state.selection().caret), state.caret_affinity()});
    const ScrollState scroll = state.scroll();
    // 画面の下に半分かかる画面行も描く（text と同じ）。
    const auto limit = scroll.visible_lines + 1;
    std::size_t screen = 0;
    auto first_row = scroll.first_row;
    for (core::LineNumber line{std::max<std::size_t>(scroll.first_visible.value, 1)};
         line.value <= state.text().line_count() && screen < limit; ++line.value)
    {
        const auto count = cells.value().rows(line);
        const auto from = std::min(first_row, count - 1);
        const auto take = std::min(count - from, limit - screen);
        const core::OffsetRange visible{
            cells.value().byte_of(line, core::GridPoint{from, 0}),
            cells.value().byte_of(line, core::GridPoint{from + take, 0})};
        auto marks = grid_marks(source, line, visible, block_row(source.block, line, source.range));
        searched += marks.searched;
        GridLineRows(GridLineSource{state, cells.value(), line, from, from + take,
                                    static_cast<std::int64_t>(screen), visible, std::move(marks),
                                    caret})
            .append_to(grid);
        screen += take;
        first_row = 0;
    }
    scanned = cells.value().scanned();
    return std::nullopt;
}

DocumentView EditorController::active_document_view() const
{
    const auto &document = state_.document();
    const auto save_state = save_state_of(document, state_.history().position());
    return DocumentView{core::tab_title_for(document.path, save_state), document.path,
                        document.encoding, save_state, state_.last_failure()};
}

core::GuideContext EditorController::guide_context() const noexcept
{
    if (state_.settings().guide == core::GuideVisibility::hidden ||
        state_.command_input().has_value() || state_.command_message().has_value() ||
        recording_name().has_value() || state_.composition().has_value())
    {
        return core::GuideContext::hidden;
    }
    if (!state_.document().path.has_value() && state_.text().size_bytes() == 0 &&
        state_.history().size() == 0)
    {
        return core::GuideContext::untouched_untitled;
    }
    return core::GuideContext::other;
}

core::EditorSettings EditorController::shown_settings() const
{
    const auto &input = state_.command_input();
    const auto focused =
        input.has_value() ? focused_command_of(input.value()) : std::optional<std::string>{};
    if (!focused.has_value())
    {
        return state_.settings();
    }
    const auto evaluated =
        core::evaluate_ex(focused.value(), state_.settings(), state_.appearance(), state_.themes());
    if (!evaluated)
    {
        return state_.settings();
    }
    const auto &changed = evaluated.value().settings;
    if (!changed.has_value())
    {
        return state_.settings();
    }
    // 最初の段は配色だけを重ねる（ADR 0150 の決定 4）。字体と字の大きさは保存した値のまま。
    auto shown = state_.settings();
    shown.theme = changed.value().theme;
    return shown;
}

EditorDelivery EditorController::delivery() const
{
    auto settings = shown_settings();
    const auto &theme = core::selected_theme(settings, state_.appearance());
    return EditorDelivery{theme.appearance,
                          state_.mode(),
                          state_.vim().mode,
                          ime_stance_of(state_.mode(), state_.vim().mode, state_.command_input()),
                          state_.command_input().transform(input_line_prompt_of),
                          composed(),
                          command_composed(),
                          active_document_view(),
                          std::move(settings),
                          state_.settings_failure(),
                          command_message(),
                          state_.closing(),
                          state_.close_request(),
                          state_.operation_request(),
                          state_.display_question(),
                          layout_failure_};
}

// 頼まれた追従を解決してから作る（ADR 0128 の決定 3）。解決の失敗は failure に載る。
EditorFrame EditorController::frame()
{
    const ScrollState scroll = framed_scroll();
    auto delivered = delivery();
    const auto caret = state_.text().position_of(state_.selection().caret);
    const auto &theme = core::selected_theme(delivered.settings, delivered.appearance);
    auto tabs = tab_views(state_, delivered.document);
    const auto encoding = delivered.document.encoding;
    // 問いの答えを待つ間は本文の行を読まず文字組みもしない（ADR 0140 の決定 2・renderer は
    // display_pending を見て本文を描かない）。格子では画面行が grid に載る（決定 5）。
    const bool pending = display_pending(state_.display_inquiry());
    const bool gridded =
        !pending && state_.display() == core::DisplayForm::grid && view_geometry_.width > 0;
    const auto pattern = search_pattern();
    const PaneSource source{state_, highlighted_range(), block_selection(), pattern_of(pattern)};
    auto lines = pending || gridded ? std::expected<std::vector<LineView>, TextLayoutFailure>{}
                                    : visible_lines(source);
    // frame の失敗は描けないものだけ（契約の破れは配達だけに載る・ADR 0133 の追補 2026-10-18）。
    const auto failure = lines ? drawing_failure(layout_failure_) : std::optional{lines.error()};
    EditorFrame result{std::move(lines).value_or(std::vector<LineView>{}),
                       CaretView{caret, caret_shape_for(delivered.mode, delivered.vim_mode),
                                 state_.caret_affinity()},
                       scroll.first_visible,
                       state_.text().line_count(),
                       delivered.appearance,
                       theme.ui,
                       delivered.mode,
                       delivered.vim_mode,
                       delivered.ime,
                       core::mode_label(delivered.mode, delivered.vim_mode),
                       recording_name(),
                       std::move(delivered.composition),
                       std::move(delivered.command_composition),
                       std::move(delivered.document),
                       status_items_of(state_, caret, encoding),
                       std::move(delivered.settings),
                       std::move(delivered.settings_failure),
                       command_line_view(),
                       std::move(delivered.command_message),
                       command_palette_view(),
                       std::move(tabs),
                       state_.active_tab(),
                       state_.tab_scroll(),
                       state_.hovered(),
                       delivered.closing,
                       delivered.close_request,
                       delivered.operation_request,
                       delivered.display_question,
                       core::operation_guide(guide_context(), delivered.mode),
                       failure,
                       scroll.horizontal,
                       state_.display(),
                       pending};
    if (gridded)
    {
        const auto grid_failure =
            grid_visible_rows(source, result.grid, result.grid_scanned, result.grid_searched);
        result.layout_failure = grid_failure.has_value() ? grid_failure : result.layout_failure;
    }
    add_other_pane(result, source.pattern);
    return result;
}

// もう 1 つの枠の表示値（ADR 0148 の決定 8）。行は操作中の枠と同じ visible_lines /
// grid_visible_rows を、もう 1 つの枠を操作中の枠の形で読む写しから作る。幅は両枠で等しいので
// 同じ寸法を渡す（決定 1）。選択は無く、検索の面は両枠に塗る（限界）。
void EditorController::add_other_pane(EditorFrame &result, const core::VimPattern *pattern) const
{
    result.layout = state_.split().layout;
    result.active_side = state_.split().active_side;
    const auto &other = state_.split().other;
    if (!other.has_value())
    {
        return;
    }
    std::visit([this, &result, pattern](const auto &view)
               { this->add_other_view(result, pattern, view); }, other.value());
}

// プレビューの枠は値の写しだけで、本文を歩かない（ADR 0155 の決定 11）。追従の間の錨は本文の
// 先頭行の行頭（決定 7）。
void EditorController::add_other_view(EditorFrame &result, const core::VimPattern *,
                                      const PreviewView &view) const
{
    const core::Offset anchor =
        view.follows ? state_.text().line_start(result.first_visible) : view.anchor;
    result.other = PreviewFrame{view.document, anchor, view.notice};
}

void EditorController::add_other_view(EditorFrame &result, const core::VimPattern *pattern,
                                      const PaneView &view) const
{
    const auto pane = state_.other_pane_state();
    if (!pane.has_value())
    {
        return;
    }
    const auto &ids = state_.autosave_ids();
    const auto found = std::ranges::find(ids, view.tab);
    if (found == ids.end())
    {
        return;
    }
    const auto position = static_cast<std::size_t>(std::distance(ids.begin(), found));
    const EditorState &state = pane.value();
    const PaneSource source{state, core::selection_range(state.selection()), std::nullopt, pattern};
    PaneFrame other{{},
                    CaretView{state.text().position_of(state.selection().caret), result.caret.shape,
                              state.caret_affinity()},
                    state.scroll().first_visible,
                    state.text().line_count(),
                    state.scroll().horizontal,
                    state.display(),
                    {},
                    result.tabs.at(position)};
    // 問いの答えを待つ文書は本文を読まない（操作中の枠と同じ・ADR 0140 の決定 2）。
    const bool pending = display_pending(state.display_inquiry());
    std::optional<TextLayoutFailure> failure;
    if (!pending && state.display() == core::DisplayForm::grid && view_geometry_.width > 0)
    {
        std::size_t scanned = 0;
        std::size_t searched = 0;
        failure = grid_visible_rows(source, other.grid, scanned, searched);
    }
    else if (!pending)
    {
        auto lines = visible_lines(source);
        failure = lines ? std::nullopt : std::optional{lines.error()};
        other.lines = std::move(lines).value_or(std::vector<LineView>{});
    }
    result.layout_failure = result.layout_failure.has_value() ? result.layout_failure : failure;
    result.other = std::move(other);
}
} // namespace nenenib::application
