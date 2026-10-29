#include "EditorController.hpp"
#include "EditorControllerDetail.hpp"

#include "CaretMove.hpp"
#include "ClipboardText.hpp"
#include "DateTimeText.hpp"
#include "DeleteDirection.hpp"
#include "DisplayInquiry.hpp"
#include "DisplayText.hpp"
#include "Edit.hpp"
#include "HtmlToMarkdown.hpp"
#include "Language.hpp"
#include "OrdinaryCharacterBoundary.hpp"
#include "PasteKind.hpp"
#include "Selection.hpp"
#include "Utf8.hpp"
#include "VimBlockRange.hpp"
#include "VimCaret.hpp"
#include "VimCharacter.hpp"
#include "VimJumpLists.hpp"
#include "VimKey.hpp"
#include "VimMarkEdits.hpp"
#include "VimMode.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VisualPosition.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::application
{
using detail::with_document_newlines;

namespace
{
constexpr std::size_t single_page_line = 1;

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
// クリップボードに書けなかった知らせ（ADR 0134 の決定 3）。
constexpr std::string_view clipboard_unwritten_notice = "クリップボードに書けませんでした";
// NORMAL / VISUAL で次の鍵を待っているか（オペレータ・回数・レジスタの名指し・1 鍵の待ち）。
// VISUAL の Esc は VISUAL を出るので、VISUAL ではオペレータと 1 鍵の待ちだけを待ちと見る（回数と
// レジスタの名指しは選択を捨ててまで取り消さない）。
[[nodiscard]] bool vim_waiting(const core::VimState &vim) noexcept
{
    const bool keyed = vim.pending.has_value() || vim.input_wait.has_value();
    switch (vim.mode)
    {
    case core::VimMode::normal:
        return keyed || vim.count.has_value() || vim.selected_register.has_value();
    case core::VimMode::insert:
    case core::VimMode::visual:
    case core::VimMode::visual_line:
    case core::VimMode::visual_block:
        return keyed;
    }
    std::unreachable();
}

// F5 を Vim の NORMAL / VISUAL で押したときの知らせ（ADR 0175 の決定 3）。鳴らさない。
constexpr std::string_view date_time_insert_only_notice = "INSERT モードで F5 を押してください";
// 貼り付けの知らせ（ADR 0157 の決定 3・7）。鳴らさない情報の 1 行。
constexpr std::string_view html_missing_notice = "HTML が無いので文字だけを貼りました";
constexpr std::string_view html_unconverted_notice = "HTML を変換できないので文字だけを貼りました";
} // namespace

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
    auto history = state_.history().pushed(edit, boundary);
    // 行がずれたマークを直し、最後の変更の位置を置く（ADR 0152 の決定 6・7）。変更の列には、
    // 新しい undo の単位を始めた編集かを添えて置く（追補 #1037・Vim の b_new_change）。`.` と `@`
    // の 再生は命令ごとに積んでから 1 単位に畳むので、再生の中は最初の編集だけが新しい単位 （Vim
    // は再生の全体が 1 単位）。
    const bool started =
        history.position() != before &&
        (!replay_history_.has_value() || before == replay_history_.value().position());
    auto marks = core::marks_adjusted(state_.vim_marks(), state_.text(), edit);
    marks.last_change = core::vim_last_change_of(state_.text(), edit);
    marks = core::vim_change_recorded(marks, marks.last_change.value(), started);
    state_ = state_.with_vim_marks(std::move(marks));
    auto document = after_edit_at(state_.document(), before);
    forget_syntax_before(edit);
    adjust_other(edit, next);
    state_ = state_.with_edit(std::move(next), core::collapsed_at(caret), std::move(history),
                              std::move(document));
    follow_caret();
}

// 覚えは line の行頭の状態で、それは line より前の行だけで決まる。編集の始まり（編集の前の本文の
// 行）が line 以降なら line より前の行は動かないので保ち、前なら捨てる（ADR 0156 の追補 #827）。
// 覚えは文書の退避番号で名指しするので、同じ文書を 2 枠で見ているときは両方に同じ判断をする。
void EditorController::forget_syntax_before(const core::Edit &edit)
{
    if (!first_syntax_.has_value() && !second_syntax_.has_value())
    {
        return;
    }
    const AutosaveId tab = state_.autosave_ids().at(state_.active_tab());
    const std::size_t line = state_.text().position_of(edit.at).line.value;
    const auto forget = [tab, line](std::optional<SyntaxStateCache> &cache)
    {
        if (cache.has_value() && cache.value().tab == tab && line < cache.value().line.value)
        {
            cache.reset();
        }
    };
    forget(first_syntax_);
    forget(second_syntax_);
}

void EditorController::move_caret_to(core::Offset caret, core::SelectionAnchoring anchoring,
                                     core::CaretAffinity affinity)
{
    const core::Offset anchor = anchor_for(anchoring, state_.selection().anchor, caret);
    state_ = state_.with_selection(core::Selection{anchor, caret}).with_caret_affinity(affinity);
    follow_caret();
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
    move_caret_to(core::placed_caret(state_.text(), intent.position, caret_unit(state_.mode())),
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
    forget_syntax_before(edit.value());
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
    forget_syntax_before(edit.value());
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

// ---------------------------------------------------------------- IME（ADR 0014）

bool EditorController::composition_ignored() const noexcept
{
    // 入力行（面・Ex の行・検索の行）は変換を受け、下のモード（Vim の NORMAL など）の決まりは
    // 見ない（ADR 0061 決定 3・#635 の追記）。
    if (command_line_active())
    {
        return false;
    }
    // 面に焦点がある間の変換と確定は本文へ入れない（ADR 0162 の E4 の追補・IME は閉じている）。
    if (explorer_focused())
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
    // 本文と履歴は動かさず、差し込んだ表示中のキャレットへ画面だけを追従する（ADR 0114）。
    state_ = state_.with_composition(intent.composition);
    if (view_geometry_.width > 0 && !state_.command_input().has_value())
    {
        follow_caret();
    }
}

// IME の確定は打った鍵の入口（`map` の表）を通らない。積んだ鍵は意図の入口で先に流れている
// （ADR 0176 の決定 4）。入力行が開いていれば捨てる（VimKeyPress と同じ）。
void EditorController::type_as_vim_keys(std::string_view utf8)
{
    core::Offset at{0};
    while (at.value < utf8.size())
    {
        if (!state_.command_input().has_value())
        {
            static_cast<void>(
                step_vim(core::VimKey{core::VimCharacter{core::code_point_at(utf8, at)}}));
        }
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

void EditorController::accept(const InsertDateTime &)
{
    // 変換中は IME の確定を待つ（日時を変換中の文字列の前へ割り込ませない）。
    if (state_.composition().has_value())
    {
        return;
    }
    // 入力行（面・Ex の行・検索の行）は打った文字と同じ道で入れる。下のモードの決まりは見ない。
    if (command_line_active())
    {
        accept(CommandText{core::date_time_text(ports_.wall_clock.now())});
        return;
    }
    switch (state_.mode())
    {
    case core::EditMode::ordinary:
        // 選択があれば置き換え、無ければキャレットに入れる。InsertText と同じ Edit 1 回。
        accept(InsertText{core::date_time_text(ports_.wall_clock.now())});
        return;
    case core::EditMode::vim:
        break;
    }
    switch (state_.vim().mode)
    {
    case core::VimMode::insert:
        // IME の確定と同じく打鍵として engine へ流す（ADR 0014）。undo は INSERT 1 回のまま。
        type_as_vim_keys(core::date_time_text(ports_.wall_clock.now()));
        return;
    case core::VimMode::normal:
    case core::VimMode::visual:
    case core::VimMode::visual_line:
    case core::VimMode::visual_block:
        // Vim の鍵の意味を黙って変えない（D71）。時刻は読まない。待ちの途中なら、Vim が割り当ての
        // 無い鍵を受けたときと同じく Esc の道で待ちを取り消す（#1120 の R3-1）。
        // 材料は固定の文字だけなので parse は必ず成功する（不変条件・ARC-010）。
        if (vim_waiting(state_.vim()))
        {
            static_cast<void>(step_vim(core::VimKey{core::VimSpecialKey::escape}));
        }
        state_ = state_.with_command_message(
            core::DisplayText::parse(date_time_insert_only_notice).value());
        return;
    }
    std::unreachable();
}

void EditorController::accept(const CancelComposition &)
{
    state_ = state_.with_composition(std::nullopt);
    if (view_geometry_.width > 0 && !state_.command_input().has_value())
    {
        follow_caret();
    }
}
} // namespace nenenib::application
