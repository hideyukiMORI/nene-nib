#include "EditorController.hpp"

#include "CaretMove.hpp"
#include "CaretShape.hpp"
#include "ClipboardText.hpp"
#include "CommandChoice.hpp"
#include "CommandChoiceKind.hpp"
#include "CommandEdit.hpp"
#include "CommandPalette.hpp"
#include "Composition.hpp"
#include "DeleteDirection.hpp"
#include "DevicePixels.hpp"
#include "DisplayLine.hpp"
#include "DisplayText.hpp"
#include "DocumentState.hpp"
#include "Edit.hpp"
#include "ExEvaluationFailure.hpp"
#include "ExResult.hpp"
#include "ExTabRequest.hpp"
#include "ExTabVerb.hpp"
#include "FileFolder.hpp"
#include "FileHistory.hpp"
#include "FileHistoryEdit.hpp"
#include "FolderBatch.hpp"
#include "FolderProgress.hpp"
#include "FolderRequest.hpp"
#include "ImeStance.hpp"
#include "ModeLabel.hpp"
#include "Palette.hpp"
#include "PaletteLayout.hpp"
#include "PaletteMarks.hpp"
#include "PaletteOrigin.hpp"
#include "ParkedTab.hpp"
#include "SaveState.hpp"
#include "ScrollBounds.hpp"
#include "SearchLine.hpp"
#include "SearchPreview.hpp"
#include "Selection.hpp"
#include "SelectionSpan.hpp"
#include "Session.hpp"
#include "SessionEnd.hpp"
#include "SessionTab.hpp"
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
#include "Utf8.hpp"
#include "VimBlockEdit.hpp"
#include "VimBlockRange.hpp"
#include "VimCaret.hpp"
#include "VimCharacter.hpp"
#include "VimClipboardText.hpp"
#include "VimInsertBlock.hpp"
#include "VimKey.hpp"
#include "VimKeySource.hpp"
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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <iterator>
#include <memory>
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
constexpr std::size_t single_page_line = 1;
// 同期で読むのはここまで（ADR 0010 の決定 12）。1 GB はメモリマップの縦切りで別に決める。
constexpr std::size_t maximum_file_bytes = 64U * 1024U * 1024U;

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

// 選択が空のときの Backspace / Delete が消す範囲。行頭・行末では改行 1 つぶんを丸ごと消す。
[[nodiscard]] core::OffsetRange deletion_range(const core::TextBuffer &text, core::Offset caret,
                                               core::DeleteDirection direction)
{
    switch (direction)
    {
    case core::DeleteDirection::backward:
        return core::OffsetRange{
            core::moved_caret(text, caret, core::CaretMotion::previous_character, single_page_line),
            caret};
    case core::DeleteDirection::forward:
        return core::OffsetRange{
            caret,
            core::moved_caret(text, caret, core::CaretMotion::next_character, single_page_line)};
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

// 1 行ぶんの選択の面。行をまたぐ選択はその行の内容の終わりから 1 桁ぶんはみ出して改行を示す。
// 範囲は呼ぶ側が決める（通常モードは選択そのまま・VISUAL は Vim の規則。ADR 0018 の決定 5）。
[[nodiscard]] core::SelectionSpan span_of(const core::TextBuffer &text,
                                          const core::OffsetRange &range, core::LineNumber line)
{
    const core::Offset start = text.line_start(line);
    const core::Offset content_end = text.line_end(line);
    const std::size_t begin = std::max(range.begin.value, start.value);
    const std::size_t end = std::min(range.end.value, text.line_terminator_end(line).value);
    if (begin >= end)
    {
        return core::no_selection_span();
    }
    const core::Column first =
        text.position_of(core::Offset{std::min(begin, content_end.value)}).column;
    const core::Column last = end > content_end.value
                                  ? core::Column{text.position_of(content_end).column.value + 1}
                                  : text.position_of(core::Offset{end}).column;
    return core::SelectionSpan{core::SelectionPresence::present, first, last};
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
[[nodiscard]] std::optional<SessionTab> session_tab_of(const Document &document,
                                                       const core::TextBuffer &text,
                                                       const core::Selection &selection,
                                                       core::LineNumber first_visible)
{
    if (!document.path.has_value())
    {
        return std::nullopt;
    }
    return SessionTab{document.path.value(), text.position_of(selection.caret), first_visible, 0};
}

// 脇のタブの覚える値。読み込み済みは束から、まだ読んでいない文書は覚えていた位置のまま
// （ADR 0059 の決定 4）。写し先が足りなければ std::visit が落ちる（CPP-002）。
[[nodiscard]] std::optional<SessionTab>
parked_session_tab(const std::shared_ptr<const DocumentState> &tab)
{
    return session_tab_of(tab->document, tab->text, tab->selection, tab->first_visible);
}

[[nodiscard]] std::optional<SessionTab>
parked_session_tab(const std::shared_ptr<const UnloadedDocument> &tab)
{
    return SessionTab{tab->path, tab->caret, tab->first_visible, 0};
}

// 帯の順の各タブの覚える値。アクティブな文書は今の欄から、脇に置いたタブは束から読む。
[[nodiscard]] std::vector<std::optional<SessionTab>> band_session_tabs(const EditorState &state)
{
    const std::optional<SessionTab> active = session_tab_of(
        state.document(), state.text(), state.selection(), state.scroll().first_visible);
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
[[nodiscard]] Session ended_session(const EditorState &state, SessionEnd reason)
{
    switch (reason)
    {
    case SessionEnd::window_closed:
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
    return UnloadedDocument{tab.path, tab.caret, tab.first_visible,
                            DocumentView{core::tab_title_for(tab.path, core::SaveState::saved),
                                         tab.path, core::TextEncoding::utf8, core::SaveState::saved,
                                         std::nullopt}};
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
    return DocumentState{std::move(text),
                         core::collapsed_at(caret),
                         core::EditHistory::empty(),
                         std::move(document),
                         first_visible,
                         std::nullopt,
                         std::nullopt,
                         std::move(view)};
}

constexpr std::string_view unreached_prefix = "開けませんでした: ";
constexpr std::string_view clipped_mark = "…";

// 知らせの 1 行（ADR 0059 の「知らせの文言」）。others は同じ意図の中でほかに読めなかった数。
// 名前は 1 行に収まるよう末尾をコードポイントの境目で落とす（題名と同じ「…」）。
[[nodiscard]] core::DisplayText unreached_message(const core::FilePath &path, std::size_t others)
{
    const std::string suffix =
        others == 0 ? std::string{} : "（ほか " + std::to_string(others) + " 件）";
    std::string name(path.file_name().empty() ? path.text() : path.file_name());
    const std::size_t room =
        core::DisplayText::maximum_bytes - unreached_prefix.size() - suffix.size();
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
    // 材料は検証済みの経路と固定の文字だけなので parse は必ず成功する（不変条件・ARC-010）。
    return core::DisplayText::parse(std::string(unreached_prefix) + name + suffix).value();
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
[[nodiscard]] bool keeps_tab_walk(const EditorIntent &intent) noexcept
{
    return std::holds_alternative<WalkRecentTab>(intent) ||
           std::holds_alternative<SettleRecentTab>(intent) ||
           std::holds_alternative<WorkCompleted>(intent) ||
           std::holds_alternative<PointTitleBar>(intent) ||
           std::holds_alternative<ScrollTabs>(intent) ||
           std::holds_alternative<TitleBarWidth>(intent) ||
           std::holds_alternative<VisibleLines>(intent) ||
           std::holds_alternative<RefreshAppearance>(intent);
}

// 状態の変換の表示値。本文と面の入力行のどちらに載せるかは呼び手が決める（ADR 0061 の決定 4）。
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
    state_ = state_.with_settings(std::move(settings)).with_settings_failure(std::nullopt);
    return true;
}

EditorFrame EditorController::apply(const EditorIntent &intent)
{
    settle_tab_walk_before(keeps_tab_walk(intent));
    // 窓の寸法と帯の上のマウスは文書に触れないので、Vim の告知を消さない（ADR 0056 の決定 3）。
    // Ctrl を離したときの使った順の確定も同じ（ADR 0058 の決定 3）。窓が閉じていくときの一覧の
    // 書き出しも同じ（ADR 0059 の決定 3）。裏の仕事の合図も同じ（ADR 0062 の決定 7）。
    begin_intent(std::holds_alternative<VisibleLines>(intent) ||
                 std::holds_alternative<RefreshAppearance>(intent) ||
                 std::holds_alternative<CancelComposition>(intent) ||
                 std::holds_alternative<TitleBarWidth>(intent) ||
                 std::holds_alternative<ScrollTabs>(intent) ||
                 std::holds_alternative<PointTitleBar>(intent) ||
                 std::holds_alternative<SettleRecentTab>(intent) ||
                 std::holds_alternative<EndSession>(intent) ||
                 std::holds_alternative<WorkCompleted>(intent));
    // 写し先が足りなければここでコンパイルが落ちる＝意図が増えたことに機械が気づく（CPP-002）。
    std::visit([this](const auto &value) { this->accept(value); }, intent);
    return frame();
}

// fixture と契約の harness が打つ 1 鍵（ADR 0048 の決定 8）。入力行の写しは再生と同じ
// command_key の 1 か所を通り、打った鍵なので <Esc> は取消のまま。
EditorFrame EditorController::press_vim_key(const core::VimKey &key)
{
    settle_tab_walk_before(false);
    begin_intent(false);
    static_cast<void>(deliver_vim_key(key));
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
        if (deliver_vim_key(key).has_value())
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
    state_ = state_.with_failure(std::nullopt).with_closing(false).with_close_request(std::nullopt);
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
        // 載せるのは見えている行の窓だけ（ADR 0062 の決定 2）。全件は写さない。
        const auto selected = palette->selected();
        const auto first = core::palette_window_first(selected);
        return CommandPaletteView{palette->rows(first, core::palette_row_limit), first, selected,
                                  palette->count(), std::move(hint)};
    }
    return std::nullopt;
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
    const core::Offset caret{range.begin.value + text.size()};
    const std::size_t before = state_.history().position();
    const auto edited = state_.with_edit(std::move(next), core::collapsed_at(caret),
                                         state_.history().pushed(edit, boundary));
    state_ = edited.with_document(after_edit_at(edited.document(), before));
    follow_caret();
}

void EditorController::move_caret_to(core::Offset caret, core::SelectionAnchoring anchoring)
{
    const core::Offset anchor = anchor_for(anchoring, state_.selection().anchor, caret);
    state_ = state_.with_selection(core::Selection{anchor, caret});
    follow_caret();
}

void EditorController::follow_caret()
{
    follow_position(state_.text().position_of(state_.selection().caret));
}

// 位置が見える範囲に入るようにスクロールする。キャレットと incsearch の preview が同じ計算を
// 使う（ADR 0041 の決定 3）。
void EditorController::follow_position(core::TextPosition position)
{
    const ScrollState scroll = state_.scroll();
    const auto followed =
        core::first_visible_for_caret(scroll.first_visible, position.line, scroll.visible_lines,
                                      scroll_follow_for(state_.mode()));
    const auto last_filled = core::first_visible_within(
        core::LineNumber{state_.text().line_count()}, state_.text().line_count(),
        scroll.visible_lines, core::ScrollExtent::filled_viewport);
    const core::ScrollExtent extent = scroll.first_visible.value <= last_filled.value
                                          ? core::ScrollExtent::filled_viewport
                                          : scroll_extent_for(state_.mode());
    const auto within = core::first_visible_within(followed, state_.text().line_count(),
                                                   scroll.visible_lines, extent);
    state_ = state_.with_scroll(ScrollState{within, scroll.visible_lines});
}

void EditorController::accept(const InsertText &intent)
{
    replace(core::selection_range(state_.selection()), intent.utf8, core::EditBoundary::coalesce);
}

void EditorController::accept(const MoveCaret &intent)
{
    interrupt_vim_insert();
    const core::Offset caret = core::moved_caret(state_.text(), state_.selection().caret,
                                                 intent.motion, state_.scroll().visible_lines);
    move_caret_to(caret, anchoring_in(state_.mode(), intent.anchoring));
    settle_vim_caret();
}

void EditorController::accept(const PlaceCaret &intent)
{
    interrupt_vim_insert();
    move_caret_to(state_.text().offset_of(intent.position),
                  anchoring_in(state_.mode(), intent.anchoring));
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
    const auto range = core::is_empty(selected)
                           ? deletion_range(state_.text(), selected.begin, intent.direction)
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

void EditorController::copy_selection()
{
    const auto block = block_selection();
    if (block.has_value())
    {
        // 矩形は行を文書の改行でつないで置く（ADR 0035 の決定 8）。
        static_cast<void>(ports_.clipboard.write(
            with_document_newlines(core::vim_block_text(state_.text(), block.value()),
                                   core::newline_of(state_.line_ending()))));
        return;
    }
    const auto range = highlighted_range();
    if (core::is_empty(range))
    {
        return;
    }
    // 置けなかったことは本文を変えない。通知はまだ無いので表示値にも載せない（ARC-010）。
    if (!ports_.clipboard.write(state_.text().text_range(range.begin, range.end)))
    {
        return;
    }
}

void EditorController::cut_selection()
{
    const auto block = block_selection();
    if (block.has_value())
    {
        // 置けたときだけ切り取る。消す形は engine の `d` と同じ 1 本（ADR 0035 の決定 8）。
        if (!ports_.clipboard.write(
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
    if (!ports_.clipboard.write(state_.text().text_range(range.begin, range.end)))
    {
        return;
    }
    replace(range, std::string_view{}, core::EditBoundary::separate);
}

void EditorController::paste_clipboard()
{
    const auto pasted = ports_.clipboard.read();
    if (!pasted)
    {
        return;
    }
    // 改行は `"+p` と同じ 2 段で文書の形に揃える（ADR 0055 の決定 2）。単独の `\r` は文字のまま。
    replace(core::selection_range(state_.selection()),
            with_document_newlines(core::clipboard_line_feeds(pasted.value()),
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
    state_ =
        state_.with_edit(std::move(next), core::collapsed_at(caret), state_.history().undone());
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
    state_ =
        state_.with_edit(std::move(next), core::collapsed_at(caret), state_.history().redone());
    follow_caret();
}

void EditorController::accept(const ScrollLines &intent)
{
    const ScrollState scroll = state_.scroll();
    const auto moved = static_cast<std::int64_t>(scroll.first_visible.value) + intent.lines;
    const auto requested = moved < 1 ? std::size_t{1} : static_cast<std::size_t>(moved);
    const auto within =
        core::first_visible_within(core::LineNumber{requested}, state_.text().line_count(),
                                   scroll.visible_lines, scroll_extent_for(state_.mode()));
    state_ = state_.with_scroll(ScrollState{within, scroll.visible_lines});
}

void EditorController::accept(const VisibleLines &intent)
{
    const std::size_t lines = std::max<std::size_t>(intent.lines, 1);
    state_ =
        state_.with_vim(core::vim_after_resize(state_.vim(), state_.scroll().visible_lines, lines));
    const auto within =
        core::first_visible_within(state_.scroll().first_visible, state_.text().line_count(), lines,
                                   scroll_extent_for(state_.mode()));
    state_ = state_.with_scroll(ScrollState{within, lines});
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
        const ScrollState scroll = state_.scroll();
        const auto within = core::first_visible_within(
            scroll.first_visible, state_.text().line_count(), scroll.visible_lines,
            scroll_extent_for(core::EditMode::ordinary));
        state_ = state_.with_scroll(ScrollState{within, scroll.visible_lines});
        return;
    }
    case core::EditMode::vim:
        break;
    }
    // NORMAL のキャレットは文字の上にあるので、行末を越えていたら 1 つ左へ寄せる。
    move_caret_to(core::vim_resting_caret(state_.text(), state_.selection().caret),
                  core::SelectionAnchoring::collapse);
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
        return submitted_command();
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
// 本文と無名は engine の結果のまま（copy_selection と同じ）。
void EditorController::send_vim_clipboard(const std::optional<core::VimRegister> &written)
{
    if (!written.has_value())
    {
        return;
    }
    static_cast<void>(ports_.clipboard.write(
        with_document_newlines(written.value().text, core::newline_of(state_.line_ending()))));
}

std::optional<core::VimRepeatFailure> EditorController::step_vim(const core::VimKey &key)
{
    load_vim_clipboard();
    const core::VimMode before = state_.vim().mode;
    const ScrollState scroll = state_.scroll();
    const core::VimEditorView view{state_.text(), state_.selection(),
                                   core::VimViewport{scroll.first_visible, scroll.visible_lines},
                                   replay_depth_.has_value() ? core::VimKeySource::replayed
                                                             : core::VimKeySource::typed,
                                   core::VimTabs{state_.active_tab(), state_.tab_count()}};
    const auto step = core::vim_step(state_.vim(), view, key);
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
    // 写し先が足りなければここでコンパイルが落ちる＝効果が増えたことに機械が気づく（CPP-002）。
    std::visit([this](const auto &value) { this->perform(value); }, step.effect);
    send_vim_clipboard(step.clipboard);
    // 検索の報せは Ex の結果と同じ 1 本に出し、次の入力で消える（ADR 0032 の決定 5）。
    if (step.notice.has_value())
    {
        state_ = state_.with_command_message(core::vim_search_message(step.notice.value()));
    }
    if (before == core::VimMode::insert && before != step.next.mode)
    {
        state_ = state_.with_history(state_.history().sealed());
    }
    if (!state_.command_input().has_value())
    {
        settle_vim_caret();
    }
    return step.failure;
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
    state_ = state_.with_command_input(core::CommandLine::empty(state_.themes()));
}

void EditorController::perform(const core::VimOpenSearch &effect)
{
    state_ = state_.with_command_input(core::SearchLine::opened(effect.direction));
    state_ = state_.with_search_preview(SearchPreview{
        std::nullopt, state_.scroll(), state_.text().position_of(state_.selection().caret)});
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
    state_ = state_.with_scroll(ScrollState{origin.first_visible, state_.scroll().visible_lines});
}

void EditorController::accept(const CommandText &intent)
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return;
    }
    const auto next = inserted_command(input.value(), intent.utf8);
    if (!next)
    {
        state_ = state_.with_command_message(core::ex_failure_message(next.error()));
        return;
    }
    state_ = state_.with_command_input(next.value());
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
    state_ = state_.with_command_input(edited_command(input.value(), intent.edit));
    update_search_preview();
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
    if (command_palette_active())
    {
        accept(CancelCommand{});
        return;
    }
    open_palette("", 0);
}

// 開いている間の OpenTabList は Ctrl+P と同じく閉じる（ADR 0057 の決定 7）。
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
    open_palette("#", state_.active_tab());
}

// 面を開く（ADR 0062 の決定 14）。前の面のときに届いて残った分は collect で捨てる（たまりが空で
// ないと次の列挙の合図が来ない）。券を進めてから頼むので、前の面の分は券で見分けて捨てられる。
void EditorController::open_palette(std::string_view input, std::size_t selected)
{
    static_cast<void>(ports_.folders.collect());
    auto entries = palette_entries();
    request_folder(entries);
    state_ = state_.with_command_input(
        core::CommandPalette::opened(std::move(entries), input, selected, state_.themes()));
}

void EditorController::request_folder(const std::vector<core::CommandChoice> &entries)
{
    palette_folder_ = PaletteFolder{palette_folder_.ticket + 1, {}, 0};
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
    keep(state_.document().path);
    for (const ParkedTab &tab : state_.parked())
    {
        keep(std::visit([](const auto &value) { return std::optional{parked_path(value)}; }, tab));
    }
    for (const core::CommandChoice &entry : entries)
    {
        if (entry.origin == core::PaletteOrigin::history)
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
        choices.push_back(
            core::CommandChoice{core::tab_title_for(file, core::SaveState::saved),
                                std::string(file.text()), core::CommandChoiceKind::open,
                                core::tab_folder_for(file), core::PaletteOrigin::folder});
    }
}

// タブの候補は帯の表示値から作る（題名は帯と同じ・場所はファイルのあるフォルダ・実行は Ex の
// `tabnext N`・印は tab）。
std::vector<core::CommandChoice> EditorController::palette_entries() const
{
    const auto views = tab_views(state_, active_document_view());
    std::vector<core::CommandChoice> entries;
    entries.reserve(views.size());
    for (std::size_t index = 0; index < views.size(); ++index)
    {
        const DocumentView &view = views.at(index);
        entries.push_back(core::CommandChoice{
            view.title, "tabnext " + std::to_string(index + 1), core::CommandChoiceKind::execute,
            core::tab_folder_for(view.path), core::PaletteOrigin::tab});
    }
    // 履歴は面を開くときに 1 回だけ読む（ADR 0060 の決定 8）。読めなければ履歴の候補なしで開き、
    // 知らせない。開いているタブと同じファイルは、タブの候補として出ているので重ねない。
    const auto history = ports_.history.read();
    if (!history)
    {
        return entries;
    }
    for (const core::FilePath &path : history.value().files)
    {
        if (open_tab_of(path).has_value())
        {
            continue;
        }
        // 名前はタブの題名と同じ作り方（未保存の印なし）。確定は open_listed へ写すパスの文字列。
        entries.push_back(
            core::CommandChoice{core::tab_title_for(path, core::SaveState::saved),
                                std::string(path.text()), core::CommandChoiceKind::open,
                                core::tab_folder_for(path), core::PaletteOrigin::history});
    }
    return entries;
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
    case core::CommandChoiceKind::fill:
        break;
    }
    const auto next = palette.filled(choice.command);
    if (!next)
    {
        state_ = state_.with_command_message(core::ex_failure_message(next.error()));
        return;
    }
    state_ = state_.with_command_input(next.value());
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
    return step_vim(core::VimKey{pattern});
}

void EditorController::accept(const SubmitCommand &)
{
    static_cast<void>(submitted_command());
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
        state_ = state_.with_command_message(core::ex_failure_message(result.error(), text));
        return;
    }
    const auto &tab = result.value().tab;
    if (tab.has_value())
    {
        run_tab_request(tab.value());
        return;
    }
    const auto &settings = result.value().settings;
    if (settings.has_value() && !persist_settings(settings.value()))
    {
        state_ = state_.with_command_message(
            core::DisplayText::parse("Settings could not be saved").value());
        return;
    }
    // 検索の当たりの強調は設定ではなく Vim の状態なので、保存せずここで写す（ADR 0037 の決定 2）。
    const auto &highlight = result.value().highlight;
    if (highlight.has_value())
    {
        core::VimState vim = state_.vim();
        vim.highlight = core::requested_highlight(vim.highlight, highlight.value());
        state_ = state_.with_vim(std::move(vim));
    }
    // incsearch も同じく Vim の状態で、保存しない（ADR 0041 の決定 6）。
    const auto &incsearch = result.value().incsearch;
    if (incsearch.has_value())
    {
        core::VimState vim = state_.vim();
        vim.incsearch = incsearch.value();
        state_ = state_.with_vim(std::move(vim));
    }
    state_ = state_.with_command_message(result.value().message);
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
        state_ = state_.with_command_message(
            core::DisplayText::parse("E475: Invalid argument: " +
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

void EditorController::perform(const core::VimNavigate &effect)
{
    state_ = state_.with_selection(effect.selection)
                 .with_scroll(ScrollState{effect.first_visible, state_.scroll().visible_lines});
    state_ = state_.with_history(state_.history().sealed());
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

const core::VimState &EditorController::vim_state() const noexcept
{
    return state_.vim();
}

void EditorController::accept(const RefreshAppearance &)
{
    state_ = state_.with_appearance(appearance_or_dark(ports_.appearance));
}

std::expected<std::string, FileFailure> EditorController::decoded(core::TextEncoding encoding,
                                                                  std::string_view bytes)
{
    switch (encoding)
    {
    case core::TextEncoding::utf8:
        return std::string(bytes);
    case core::TextEncoding::utf8_bom:
        return std::string(core::without_byte_order_mark(bytes));
    case core::TextEncoding::shift_jis:
        break;
    }
    auto converted = ports_.code_pages.to_utf8(bytes);
    if (!converted)
    {
        return std::unexpected(file_failure_of(converted.error()));
    }
    return std::move(converted).value();
}

std::expected<std::string, FileFailure> EditorController::encoded(core::TextEncoding encoding,
                                                                  std::string_view utf8)
{
    switch (encoding)
    {
    case core::TextEncoding::utf8:
        return std::string(utf8);
    case core::TextEncoding::utf8_bom:
        return std::string(core::byte_order_mark()) + std::string(utf8);
    case core::TextEncoding::shift_jis:
        break;
    }
    auto converted = ports_.code_pages.from_utf8(utf8);
    if (!converted)
    {
        return std::unexpected(file_failure_of(converted.error()));
    }
    return std::move(converted).value();
}

// 読んで復号した本文と文書。上限はここが正本で、ポートへ引数で渡す。読んでから断るのでは
// 大きいファイルを先に抱える。
std::expected<std::pair<core::TextBuffer, Document>, FileFailure>
EditorController::read_document(const core::FilePath &path)
{
    const auto bytes = ports_.files.read(path, maximum_file_bytes);
    if (!bytes)
    {
        return std::unexpected(bytes.error());
    }
    const auto encoding = core::detect_encoding(bytes.value());
    if (!encoding)
    {
        return std::unexpected(FileFailure::undecodable);
    }
    const auto utf8 = decoded(encoding.value(), bytes.value());
    if (!utf8)
    {
        return std::unexpected(utf8.error());
    }
    auto text = core::TextBuffer::from_utf8(utf8.value());
    if (!text)
    {
        return std::unexpected(FileFailure::undecodable);
    }
    return std::pair{std::move(text).value(), Document{path, encoding.value(), std::size_t{0}}};
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
        state_ = state_.with_opened(std::move(text), std::move(opened));
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
    state_ = state_.with_command_message(unreached_message(path, 0));
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
    const auto bytes = encoded(intent.encoding, state_.text().text());
    if (!bytes)
    {
        fail(bytes.error());
        return;
    }
    // 書けなかったときは本文も文書も変えない。元のファイルも adapters が守る（決定 6）。
    const auto written = ports_.files.write(intent.path, bytes.value());
    if (!written)
    {
        fail(written.error());
        return;
    }
    // 保存の直後に単位を閉じる。続く入力が保存時点の単位に混ざらない（決定 7）。
    const auto history = state_.history().sealed();
    const std::size_t position = history.position();
    state_ = state_.with_history(history).with_document(
        Document{intent.path, intent.encoding, position});
}

// ---------------------------------------------------------------- タブ（ADR 0056）

void EditorController::leave_document()
{
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
    // 見えている行数は置いていたあいだに変わり得るので、先頭行だけを今の窓の範囲へ収める。
    // キャレットを追いかけない（ScrollLines で動かしたスクロールを文書ごとに保つ）。
    const ScrollState scroll = state_.scroll();
    const auto within =
        core::first_visible_within(scroll.first_visible, state_.text().line_count(),
                                   scroll.visible_lines, scroll_extent_for(state_.mode()));
    state_ = state_.with_scroll(ScrollState{within, scroll.visible_lines});
    settle_vim_caret();
    reveal_active_tab();
    reopen_replayed_unit();
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
    static_cast<void>(ports_.session.write(ended_session(state_, intent.reason)));
    // 閉じたファイルを履歴に記録する（ADR 0060 の決定 8）。一覧の書きの後。
    remember(ended_paths(state_, intent.reason));
}

// 裏の仕事の合図（ADR 0062 の決定 7・15・17）。入口は歩きも知らせも残す。collect は面が開いて
// いなくても 1 回呼んでたまりを
// 空にする。使うのは面が開いていて今の券の batch だけで、届いた分は 1 回の extended で後ろへ
// 足す（入力と選択は core が保つ）。入力・知らせ・ほかの状態には、打ち切りの 1 行のほか触れない。
void EditorController::accept(const WorkCompleted &)
{
    const auto batches = ports_.folders.collect();
    const auto &input = state_.command_input();
    const auto *palette =
        input.has_value() ? std::get_if<core::CommandPalette>(&input.value()) : nullptr;
    if (palette == nullptr)
    {
        return;
    }
    std::vector<core::CommandChoice> more;
    bool truncated = false;
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
            truncated = true;
            break;
        }
    }
    if (!more.empty())
    {
        auto extended = palette->extended(std::move(more));
        state_ = state_.with_command_input(std::move(extended));
    }
    if (truncated)
    {
        state_ = state_.with_command_message(
            core::DisplayText::parse("同じフォルダは " + std::to_string(palette_folder_.received) +
                                     " 件まで。残りは一覧に出ません")
                .value());
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
    // 面の入力は変換を受け、下のモード（Vim の NORMAL など）の決まりは見ない（ADR 0061 決定 3）。
    if (command_palette_active())
    {
        return false;
    }
    // Ex の行と検索の行は今までどおり捨てる。
    if (command_line_active())
    {
        return true;
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
    // 変換中は本文も履歴もスクロールも動かない。置き換わるのは変換中の文字列だけ（ARC-004）。
    state_ = state_.with_composition(intent.composition);
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
    // 面の確定は打った文字と同じ道で面の入力に入る。本文にも engine にも行かない（ADR 0061 の
    // 決定 3）。上限を越えたときの知らせも同じ道が出す。
    if (command_palette_active())
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
}

void EditorController::close_command_input()
{
    const bool palette = command_palette_active();
    state_ = state_.with_command_input(std::nullopt);
    if (palette)
    {
        state_ = state_.with_composition(std::nullopt);
    }
}

std::optional<CompositionView> EditorController::composed() const
{
    if (command_palette_active())
    {
        return std::nullopt;
    }
    return composition_view_of(state_.composition());
}

std::optional<CompositionView> EditorController::command_composed() const
{
    if (!command_palette_active())
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

std::optional<core::VimPattern> EditorController::search_pattern() const
{
    // 入力中は入力のパターンで塗る。解析できなければ何も塗らない（ADR 0041 の決定 4）。
    const core::SearchLine *typing = previewed_line(state_);
    if (state_.mode() == core::EditMode::vim && typing != nullptr)
    {
        return typed_pattern(*typing);
    }
    const auto &remembered = state_.vim().last_search;
    if (state_.mode() != core::EditMode::vim ||
        state_.vim().highlight != core::VimSearchHighlight::on || !remembered.has_value())
    {
        return std::nullopt;
    }
    auto parsed = core::VimPattern::parse(remembered.value().pattern, remembered.value().direction);
    if (!parsed)
    {
        return std::nullopt;
    }
    return std::move(parsed).value();
}

// 見えている 1 行ぶんの表示値。検索の当たりは行の中だけを数え、桁は選択と同じ span_of で作る
// （ADR 0037 の決定 3・4）。塗る面を持たない長さ 0 の一致は span_of が absent を返すので落ちる。
// 描画用の行は core の display_line 1 本で作る（ADR 0040 の決定 2）。桁は本文の桁のまま渡す。
LineView EditorController::line_view(core::LineNumber line, const core::OffsetRange &range,
                                     const std::optional<core::VimPattern> &pattern) const
{
    const core::TextBuffer &text = state_.text();
    std::string body = text.line_text(line);
    core::DisplayLine display = core::display_line(body);
    LineView view{line, std::move(body), std::move(display), span_of(text, range, line),
                  {},   std::nullopt};
    if (!pattern.has_value())
    {
        return view;
    }
    const std::size_t start = text.line_start(line).value;
    // 今の一致はキャレットを含む一致。incsearch の入力中は preview の当たりを含む一致（ADR 0041
    // の決定 4）。全一致の面は hlsearch が on のときだけで、off の入力中は今の当たりの枠だけ
    // （Vim と同じ）。入力中でなければ off の search_pattern は何も返さない。
    const std::size_t caret = previewed_offset(state_).value_or(state_.selection().caret).value;
    for (const auto &match : core::vim_line_matches(view.text, pattern.value()))
    {
        const core::OffsetRange found{core::Offset{start + match.begin.value},
                                      core::Offset{start + match.end.value}};
        const core::SelectionSpan span = span_of(text, found, line);
        if (span.presence != core::SelectionPresence::present)
        {
            continue;
        }
        view.matches.push_back(span);
        if (found.begin.value <= caret && caret < found.end.value)
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

std::vector<LineView> EditorController::visible_lines() const
{
    const ScrollState scroll = state_.scroll();
    const std::size_t total = state_.text().line_count();
    const std::size_t first = std::min(scroll.first_visible.value, total);
    const std::size_t last =
        std::min(first + std::max<std::size_t>(scroll.visible_lines, 1) - 1, total);
    const core::OffsetRange range = highlighted_range();
    const auto block = block_selection();
    const auto pattern = search_pattern();
    std::vector<LineView> lines;
    for (std::size_t number = first; number <= last; ++number)
    {
        const core::LineNumber line{number};
        lines.push_back(line_view(line, block_row(block, line, range), pattern));
    }
    return lines;
}

DocumentView EditorController::active_document_view() const
{
    const auto &document = state_.document();
    const auto save_state = save_state_of(document, state_.history().position());
    return DocumentView{core::tab_title_for(document.path, save_state), document.path,
                        document.encoding, save_state, state_.last_failure()};
}

EditorFrame EditorController::frame() const
{
    const auto caret = state_.text().position_of(state_.selection().caret);
    const auto &document = state_.document();
    const auto &theme = core::selected_theme(state_.settings(), state_.appearance());
    DocumentView active = active_document_view();
    auto tabs = tab_views(state_, active);
    return EditorFrame{visible_lines(),
                       CaretView{caret, caret_shape_for(state_.mode(), state_.vim().mode)},
                       state_.scroll().first_visible,
                       state_.text().line_count(),
                       theme.appearance,
                       theme.ui,
                       state_.mode(),
                       state_.vim().mode,
                       ime_stance_of(state_.mode(), state_.vim().mode, state_.command_input()),
                       core::mode_label(state_.mode(), state_.vim().mode),
                       recording_name(),
                       composed(),
                       command_composed(),
                       std::move(active),
                       core::status_items_for(caret, document.encoding, state_.line_ending()),
                       state_.settings(),
                       state_.settings_failure(),
                       command_line_view(),
                       state_.command_message(),
                       command_palette_view(),
                       std::move(tabs),
                       state_.active_tab(),
                       state_.tab_scroll(),
                       state_.hovered(),
                       state_.closing(),
                       state_.close_request()};
}
} // namespace nenenib::application
