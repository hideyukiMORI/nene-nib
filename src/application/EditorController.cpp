#include "EditorController.hpp"

#include "CaretMove.hpp"
#include "CaretShape.hpp"
#include "Composition.hpp"
#include "DeleteDirection.hpp"
#include "Edit.hpp"
#include "ExResult.hpp"
#include "ModeLabel.hpp"
#include "Palette.hpp"
#include "ScrollBounds.hpp"
#include "Selection.hpp"
#include "SelectionSpan.hpp"
#include "StatusItems.hpp"
#include "TabTitle.hpp"
#include "Utf8.hpp"
#include "VimCaret.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimMode.hpp"
#include "VimNavigate.hpp"
#include "VimStep.hpp"
#include "VimVisualRange.hpp"

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

std::optional<core::CommandLine> EditorController::command_line_view() const
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return std::nullopt;
    }
    return command_line_of(input.value());
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
    const auto followed = core::first_visible_for_caret(
        scroll.first_visible, caret_line, scroll.visible_lines, scroll_follow_for(state_.mode()));
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

void EditorController::copy_selection()
{
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
    if (state_.command_input().has_value())
    {
        return;
    }
    const core::VimMode before = state_.vim().mode;
    const ScrollState scroll = state_.scroll();
    const core::VimEditorView view{state_.text(), state_.selection(),
                                   core::VimViewport{scroll.first_visible, scroll.visible_lines}};
    const auto step = core::vim_step(state_.vim(), view, intent.key);
    state_ = state_.with_vim(step.next);
    // INSERT の出入りが undo の区切り（ADR 0012 の決定 6 / ADR 0009 の決定 3 の Vim 側）。
    if (before != step.next.mode && before != core::VimMode::insert)
    {
        state_ = state_.with_history(state_.history().sealed());
    }
    // 写し先が足りなければここでコンパイルが落ちる＝効果が増えたことに機械が気づく（CPP-002）。
    std::visit([this](const auto &value) { this->perform(value); }, step.effect);
    if (before == core::VimMode::insert && before != step.next.mode)
    {
        state_ = state_.with_history(state_.history().sealed());
    }
    if (!state_.command_input().has_value())
    {
        settle_vim_caret();
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
        return core::EditBoundary::separate;
    }
    std::unreachable();
}

void EditorController::perform(const core::VimNoEffect &) {}

void EditorController::perform(const core::VimOpenCommandLine &)
{
    state_ = state_.with_command_input(core::CommandLine::empty(state_.themes()));
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
}

void EditorController::accept(const EditCommand &intent)
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return;
    }
    const auto &command = command_line_of(input.value());
    if (std::holds_alternative<core::CommandLine>(input.value()) && command.text().empty() &&
        intent.edit == core::CommandEdit::backspace)
    {
        accept(CancelCommand{});
        return;
    }
    state_ = state_.with_command_input(edited_command(input.value(), intent.edit));
}

void EditorController::accept(const CancelCommand &)
{
    state_ = state_.with_command_input(std::nullopt);
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

void EditorController::accept(const SubmitCommand &)
{
    const auto &input = state_.command_input();
    if (!input.has_value())
    {
        return;
    }
    if (const auto *palette = std::get_if<core::CommandPalette>(&input.value()))
    {
        submit_palette(*palette);
        return;
    }
    const auto text = std::string(command_line_of(input.value()).text());
    evaluate_command(text);
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
    state_ = state_.with_command_message(result.value().message);
}

void EditorController::interrupt_vim_insert()
{
    if (state_.vim().mode == core::VimMode::insert)
    {
        core::VimState next = state_.vim();
        next.insert_repeat = std::nullopt;
        state_ = state_.with_vim(std::move(next)).with_history(state_.history().sealed());
    }
}

void EditorController::perform(const core::VimMoveTo &effect)
{
    interrupt_vim_insert();
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

std::vector<LineView> EditorController::visible_lines() const
{
    const ScrollState scroll = state_.scroll();
    const std::size_t total = state_.text().line_count();
    const std::size_t first = std::min(scroll.first_visible.value, total);
    const std::size_t last =
        std::min(first + std::max<std::size_t>(scroll.visible_lines, 1) - 1, total);
    const core::OffsetRange range = highlighted_range();
    std::vector<LineView> lines;
    for (std::size_t number = first; number <= last; ++number)
    {
        const core::LineNumber line{number};
        lines.push_back(
            LineView{line, state_.text().line_text(line), span_of(state_.text(), range, line)});
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
