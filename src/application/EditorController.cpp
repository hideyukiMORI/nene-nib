#include "EditorController.hpp"

#include "CaretMove.hpp"
#include "CaretShape.hpp"
#include "Composition.hpp"
#include "DeleteDirection.hpp"
#include "DisplayLine.hpp"
#include "Edit.hpp"
#include "ExResult.hpp"
#include "ModeLabel.hpp"
#include "Palette.hpp"
#include "ScrollBounds.hpp"
#include "SearchLine.hpp"
#include "SearchPreview.hpp"
#include "Selection.hpp"
#include "SelectionSpan.hpp"
#include "StatusItems.hpp"
#include "TabTitle.hpp"
#include "TextPosition.hpp"
#include "Utf8.hpp"
#include "VimBlockEdit.hpp"
#include "VimBlockRange.hpp"
#include "VimCaret.hpp"
#include "VimCharacter.hpp"
#include "VimInsertBlock.hpp"
#include "VimKey.hpp"
#include "VimKeySource.hpp"
#include "VimMatchRequest.hpp"
#include "VimMode.hpp"
#include "VimNavigate.hpp"
#include "VimRemoveBlock.hpp"
#include "VimReplaceBlock.hpp"
#include "VimReplay.hpp"
#include "VimSearch.hpp"
#include "VimSearchDirection.hpp"
#include "VimSearchNotice.hpp"
#include "VimSearchPattern.hpp"
#include "VimStep.hpp"
#include "VimVisualRange.hpp"
#include "VimVisualReselect.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <utility>
#include <variant>

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

// 前後の本文の違う所を覆う 1 つの Edit（ADR 0046 の決定 3）。同じ本文なら空。
[[nodiscard]] std::optional<core::Edit> covering_edit(std::string_view before,
                                                      std::string_view after)
{
    if (before == after)
    {
        return std::nullopt;
    }
    const std::size_t head = common_head(before, after);
    const std::size_t tail = common_tail(before, after, head);
    return core::Edit{core::Offset{head},
                      std::string(before.substr(head, before.size() - head - tail)),
                      std::string(after.substr(head, after.size() - head - tail))};
}
} // namespace

EditorController::EditorController(EditorPorts ports, std::optional<OpenDocument> initial)
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
    if (initial.has_value())
    {
        static_cast<void>(apply(initial.value()));
    }
    // 初期ファイルも通常の意図を通す。その後で起動時の診断を載せ、最初の描画まで保持する。
    state_ = state_.with_command_message(inventory.notice);
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
    // ファイルの失敗は 1 つの意図のあいだだけ表示値に載る（ADR 0010 の決定 9）。
    state_ = state_.with_failure(std::nullopt);
    if (state_.command_message().has_value() && !std::holds_alternative<VisibleLines>(intent) &&
        !std::holds_alternative<RefreshAppearance>(intent) &&
        !std::holds_alternative<CancelComposition>(intent))
    {
        state_ = state_.with_command_message(std::nullopt);
    }
    // 写し先が足りなければここでコンパイルが落ちる＝意図が増えたことに機械が気づく（CPP-002）。
    std::visit([this](const auto &value) { this->accept(value); }, intent);
    return frame();
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
        return CommandPaletteView{palette->choices(), palette->selected()};
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
    const core::Edit edit{range.begin, std::move(removed), std::string(text)};
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
    replace(core::selection_range(state_.selection()), pasted.value(),
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
    state_ = state_.with_command_input(std::nullopt);
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

void EditorController::accept(const VimKeyPress &intent)
{
    static_cast<void>(step_vim(intent.key));
}

std::optional<core::VimRepeatFailure> EditorController::step_vim(const core::VimKey &key)
{
    if (state_.command_input().has_value())
    {
        // 入力行が開いたら残りの鍵は届かない。再生の中なら残りを捨てる（ADR 0046 の決定 3）。
        return core::VimRepeatFailure::refused;
    }
    const core::VimMode before = state_.vim().mode;
    const ScrollState scroll = state_.scroll();
    const core::VimEditorView view{state_.text(), state_.selection(),
                                   core::VimViewport{scroll.first_visible, scroll.visible_lines},
                                   replay_depth_.has_value() ? core::VimKeySource::replayed
                                                             : core::VimKeySource::typed};
    const auto step = core::vim_step(state_.vim(), view, key);
    state_ = state_.with_vim(step.next);
    // INSERT の出入りが undo の区切り（ADR 0012 の決定 6 / ADR 0009 の決定 3 の Vim 側）。
    if (before != step.next.mode && before != core::VimMode::insert)
    {
        state_ = state_.with_history(state_.history().sealed());
    }
    // 写し先が足りなければここでコンパイルが落ちる＝効果が増えたことに機械が気づく（CPP-002）。
    std::visit([this](const auto &value) { this->perform(value); }, step.effect);
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
void EditorController::accept(const StoreVimMacro &intent)
{
    state_ = state_.with_vim(core::vim_macro_stored(state_.vim(), intent.name, intent.keys));
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
    state_ = state_.with_command_input(std::nullopt);
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
    state_ = state_.with_command_input(core::CommandPalette::opened(state_.themes()));
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
        if (intent.index < palette->choices().size())
        {
            submit_palette(palette->selected_at(intent.index));
        }
    }
}

void EditorController::submit_palette(const core::CommandPalette &palette)
{
    const auto choices = palette.choices();
    if (choices.empty())
    {
        return;
    }
    const auto &choice = choices.at(palette.selected());
    switch (choice.kind)
    {
    case core::CommandChoiceKind::execute:
        evaluate_command(choice.command);
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

void EditorController::submit(const core::CommandLine &line)
{
    evaluate_command(std::string(line.text()));
}

void EditorController::submit(const core::CommandPalette &palette)
{
    submit_palette(palette);
}

// 検索の確定は engine の 1 つの鍵（ADR 0032 の決定 3）。先に入力行を閉じてから送る。
void EditorController::submit(const core::SearchLine &line)
{
    // Vim も確定の前に入力前の画面へ戻してから本当の検索をする（ex_getln.c の
    // finish_incsearch_highlighting）。着いた先は engine の鍵のあとの follow_caret が見せる。
    // 鍵は preview の起点を運ぶ（Ctrl-G / Ctrl-T が無ければキャレットと同じ・ADR 0043 の決定 3）。
    const auto preview = state_.search_preview();
    const std::optional<core::TextPosition> from =
        preview.has_value() ? std::optional{preview.value().from} : std::nullopt;
    const core::VimSearchPattern pattern{std::string(line.text()), line.direction(), from};
    state_ = state_.with_command_input(std::nullopt);
    if (preview.has_value())
    {
        restore_search_origin(preview.value().origin);
    }
    accept(VimKeyPress{core::VimKey{pattern}});
}

void EditorController::accept(const SubmitCommand &)
{
    const auto &open = state_.command_input();
    if (!open.has_value())
    {
        return;
    }
    // 入力行は state_ が持つので、写し先が状態を書き換える前に値ごと複製する。
    const CommandInput input = open.value();
    std::visit([this](const auto &value) { this->submit(value); }, input);
}

void EditorController::evaluate_command(std::string_view text)
{
    state_ = state_.with_command_input(std::nullopt);
    if (text.empty())
    {
        return;
    }
    const auto result =
        core::evaluate_ex(text, state_.settings(), state_.appearance(), state_.themes());
    if (!result)
    {
        state_ = state_.with_command_message(core::ex_failure_message(result.error()));
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
    const core::EditHistory history = state_.history();
    const core::TextBuffer text = state_.text();
    replay_queue_.assign(effect.keys.begin(), effect.keys.end());
    replay_depths_.assign(effect.keys.size(), 1);
    while (!replay_queue_.empty())
    {
        const core::VimKey key = replay_queue_.front();
        replay_depth_ = replay_depths_.front();
        replay_queue_.pop_front();
        replay_depths_.pop_front();
        if (step_vim(key).has_value())
        {
            replay_queue_.clear();
            replay_depths_.clear();
        }
    }
    replay_depth_ = std::nullopt;
    merge_replayed_edits(history, text);
    state_ = state_.with_history(state_.history().sealed());
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
    if (state_.history().position() <= before.position() + 1)
    {
        return;
    }
    const auto edit = covering_edit(text.text(), state_.text().text());
    if (!edit.has_value())
    {
        state_ = state_.with_history(before.sealed());
        return;
    }
    state_ = state_.with_history(before.pushed(edit.value(), core::EditBoundary::separate));
}

void EditorController::perform(const core::VimUndo &)
{
    // Vim は戻したあと、変わったところの先頭にキャレットを置く（Issue #22 で実測）。
    const auto edit = state_.history().undo();
    undo_edit();
    if (edit.has_value())
    {
        move_caret_to(edit.value().at, core::SelectionAnchoring::collapse);
    }
}

void EditorController::perform(const core::VimRedo &)
{
    const auto edit = state_.history().redo();
    redo_edit();
    if (edit.has_value())
    {
        move_caret_to(edit.value().at, core::SelectionAnchoring::collapse);
    }
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

void EditorController::accept(const OpenDocument &intent)
{
    // ファイルが変わる途中の変換は捨てる（ADR 0014 の決定 3）。
    state_ = state_.with_composition(std::nullopt);
    // 上限はここが正本で、ポートへ引数で渡す。読んでから断るのでは大きいファイルを先に抱える。
    const auto bytes = ports_.files.read(intent.path, maximum_file_bytes);
    if (!bytes)
    {
        fail(bytes.error());
        return;
    }
    const auto encoding = core::detect_encoding(bytes.value());
    if (!encoding)
    {
        fail(FileFailure::undecodable);
        return;
    }
    const auto utf8 = decoded(encoding.value(), bytes.value());
    if (!utf8)
    {
        fail(utf8.error());
        return;
    }
    auto text = core::TextBuffer::from_utf8(utf8.value());
    if (!text)
    {
        fail(FileFailure::undecodable);
        return;
    }
    state_ = state_.with_opened(std::move(text).value(),
                                Document{intent.path, encoding.value(), std::size_t{0}});
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

// ---------------------------------------------------------------- IME（ADR 0014）

bool EditorController::composition_ignored() const noexcept
{
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

std::optional<CompositionView> EditorController::composed() const
{
    const auto &composition = state_.composition();
    if (!composition.has_value())
    {
        return std::nullopt;
    }
    return CompositionView{composition.value().utf8,
                           core::composition_underlines(composition.value()),
                           composition.value().cursor};
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

EditorFrame EditorController::frame() const
{
    const auto caret = state_.text().position_of(state_.selection().caret);
    const auto &document = state_.document();
    const auto save_state = save_state_of(document, state_.history().position());
    const auto &theme = core::selected_theme(state_.settings(), state_.appearance());
    return EditorFrame{visible_lines(),
                       CaretView{caret, caret_shape_for(state_.mode(), state_.vim().mode)},
                       state_.scroll().first_visible,
                       state_.text().line_count(),
                       theme.appearance,
                       theme.ui,
                       state_.mode(),
                       state_.vim().mode,
                       core::mode_label(state_.mode(), state_.vim().mode),
                       recording_name(),
                       composed(),
                       DocumentView{core::tab_title_for(document.path, save_state), document.path,
                                    document.encoding, save_state, state_.last_failure()},
                       core::status_items_for(caret, document.encoding, state_.line_ending()),
                       state_.settings(),
                       state_.settings_failure(),
                       command_line_view(),
                       state_.command_message(),
                       command_palette_view()};
}
} // namespace nenenib::application
