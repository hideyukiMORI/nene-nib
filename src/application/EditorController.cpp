#include "EditorController.hpp"

#include "CaretMove.hpp"
#include "CaretShape.hpp"
#include "DeleteDirection.hpp"
#include "Edit.hpp"
#include "ModeLabel.hpp"
#include "Palette.hpp"
#include "ScrollBounds.hpp"
#include "Selection.hpp"
#include "SelectionSpan.hpp"
#include "StatusItems.hpp"
#include "TabTitle.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
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

[[nodiscard]] core::CaretShape caret_shape_for(core::EditMode mode) noexcept
{
    switch (mode)
    {
    case core::EditMode::ordinary:
        return core::CaretShape::bar;
    case core::EditMode::vim:
        return core::CaretShape::block;
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

// 1 行ぶんの選択の面。行をまたぐ選択はその行の内容の終わりから 1 桁ぶんはみ出して改行を示す。
[[nodiscard]] core::SelectionSpan span_of(const core::TextBuffer &text,
                                          const core::Selection &selection, core::LineNumber line)
{
    const auto range = core::selection_range(selection);
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
} // namespace

EditorController::EditorController(const AppearancePort &appearance, ClipboardPort &clipboard,
                                   FilePort &files, CodePagePort &code_pages)
    : appearance_(appearance), clipboard_(clipboard), files_(files), code_pages_(code_pages),
      state_(EditorState::create(appearance_or_dark(appearance), core::EditMode::ordinary))
{
}

EditorFrame EditorController::apply(const EditorIntent &intent)
{
    // ファイルの失敗は 1 つの意図のあいだだけ表示値に載る（ADR 0010 の決定 9）。
    state_ = state_.with_failure(std::nullopt);
    // 写し先が足りなければここでコンパイルが落ちる＝意図が増えたことに機械が気づく（CPP-002）。
    std::visit([this](const auto &value) { this->accept(value); }, intent);
    return frame();
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
    auto next = state_.text().erase(range.begin, range.end).insert(range.begin, text);
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
    const auto caret_line = state_.text().position_of(state_.selection().caret).line;
    const ScrollState scroll = state_.scroll();
    const auto followed =
        core::first_visible_for_caret(scroll.first_visible, caret_line, scroll.visible_lines);
    const auto within =
        core::first_visible_within(followed, state_.text().line_count(), scroll.visible_lines);
    state_ = state_.with_scroll(ScrollState{within, scroll.visible_lines});
}

void EditorController::accept(const InsertText &intent)
{
    replace(core::selection_range(state_.selection()), intent.utf8, core::EditBoundary::coalesce);
}

void EditorController::accept(const MoveCaret &intent)
{
    const core::Offset caret = core::moved_caret(state_.text(), state_.selection().caret,
                                                 intent.motion, state_.scroll().visible_lines);
    move_caret_to(caret, intent.anchoring);
}

void EditorController::accept(const PlaceCaret &intent)
{
    move_caret_to(state_.text().offset_of(intent.position), intent.anchoring);
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

void EditorController::copy_selection()
{
    const auto range = core::selection_range(state_.selection());
    if (core::is_empty(range))
    {
        return;
    }
    // 置けなかったことは本文を変えない。通知はまだ無いので表示値にも載せない（ARC-010）。
    if (!clipboard_.write(state_.text().text_range(range.begin, range.end)))
    {
        return;
    }
}

void EditorController::cut_selection()
{
    const auto range = core::selection_range(state_.selection());
    if (core::is_empty(range))
    {
        return;
    }
    // 置けたときだけ切り取る。行き先の無いまま本文から消す方が損害が大きい。
    if (!clipboard_.write(state_.text().text_range(range.begin, range.end)))
    {
        return;
    }
    replace(range, std::string_view{}, core::EditBoundary::separate);
}

void EditorController::paste_clipboard()
{
    const auto pasted = clipboard_.read();
    if (!pasted)
    {
        return;
    }
    replace(core::selection_range(state_.selection()), pasted.value(),
            core::EditBoundary::separate);
}

void EditorController::accept(const HistoryAction &intent)
{
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
    auto next = state_.text().erase(at, end).insert(at, edit.value().removed);
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
    auto next = state_.text().erase(at, end).insert(at, edit.value().inserted);
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
    const auto within = core::first_visible_within(
        core::LineNumber{requested}, state_.text().line_count(), scroll.visible_lines);
    state_ = state_.with_scroll(ScrollState{within, scroll.visible_lines});
}

void EditorController::accept(const VisibleLines &intent)
{
    const std::size_t lines = std::max<std::size_t>(intent.lines, 1);
    const auto within = core::first_visible_within(state_.scroll().first_visible,
                                                   state_.text().line_count(), lines);
    state_ = state_.with_scroll(ScrollState{within, lines});
    follow_caret();
}

void EditorController::accept(const SelectEditMode &intent)
{
    state_ = state_.with_mode(intent.mode);
}

void EditorController::accept(const RefreshAppearance &)
{
    state_ = state_.with_appearance(appearance_or_dark(appearance_));
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
    auto converted = code_pages_.to_utf8(bytes);
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
    auto converted = code_pages_.from_utf8(utf8);
    if (!converted)
    {
        return std::unexpected(file_failure_of(converted.error()));
    }
    return std::move(converted).value();
}

void EditorController::accept(const OpenDocument &intent)
{
    // 上限はここが正本で、ポートへ引数で渡す。読んでから断るのでは大きいファイルを先に抱える。
    const auto bytes = files_.read(intent.path, maximum_file_bytes);
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
    state_ = state_.with_opened(std::move(text).value(), core::detect_line_ending(utf8.value()),
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
    const auto written = files_.write(intent.path, bytes.value());
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

std::vector<LineView> EditorController::visible_lines() const
{
    const ScrollState scroll = state_.scroll();
    const std::size_t total = state_.text().line_count();
    const std::size_t first = std::min(scroll.first_visible.value, total);
    const std::size_t last =
        std::min(first + std::max<std::size_t>(scroll.visible_lines, 1) - 1, total);
    std::vector<LineView> lines;
    for (std::size_t number = first; number <= last; ++number)
    {
        const core::LineNumber line{number};
        lines.push_back(LineView{line, state_.text().line_text(line),
                                 span_of(state_.text(), state_.selection(), line)});
    }
    return lines;
}

EditorFrame EditorController::frame() const
{
    const auto caret = state_.text().position_of(state_.selection().caret);
    const auto &document = state_.document();
    const auto save_state = save_state_of(document, state_.history().position());
    return EditorFrame{visible_lines(),
                       CaretView{caret, caret_shape_for(state_.mode())},
                       state_.scroll().first_visible,
                       state_.text().line_count(),
                       state_.appearance(),
                       core::palette_for(state_.appearance()),
                       state_.mode(),
                       core::mode_label(state_.mode()),
                       DocumentView{core::tab_title_for(document.path, save_state), document.path,
                                    document.encoding, save_state, state_.last_failure()},
                       core::status_items_for(caret, document.encoding, state_.line_ending())};
}
} // namespace nenenib::application
