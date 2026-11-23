#include "EditorController.hpp"

#include "AutosaveId.hpp"
#include "ClipboardText.hpp"
#include "Edit.hpp"
#include "EditBoundary.hpp"
#include "EditHistory.hpp"
#include "EditMode.hpp"
#include "EditorControllerDetail.hpp"
#include "FormattedSpan.hpp"
#include "FormattedUnit.hpp"
#include "FormattingPieces.hpp"
#include "HistoryDirection.hpp"
#include "JsonNumber.hpp"
#include "JsonValue.hpp"
#include "LanguageAnswers.hpp"
#include "LanguageFormatting.hpp"
#include "LanguageServerDocument.hpp"
#include "LineEnding.hpp"
#include "LineNumber.hpp"
#include "LspFormatting.hpp"
#include "LspPosition.hpp"
#include "LspPositionJson.hpp"
#include "LspRequestKind.hpp"
#include "LspTextEdit.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "Selection.hpp"
#include "SelectionAnchoring.hpp"
#include "ServerAbilities.hpp"
#include "UiMessage.hpp"
#include "VimBlockEdit.hpp"
#include "VimMode.hpp"
#include "VimState.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// 整形（#1993・ADR 0191 の決定 7）。操作「文書を整形」「選択範囲を整形」（鍵なし・F1 の一覧）で
// 頼み、答えの `TextEdit[]` を全部を覆う 1 区間に組んで replace を 1 回だけ呼ぶ（1 つの undo の
// 単位・`Edit.restore` は当てる前のキャレット）。頼んだときから文書か revision が変わっていたら
// 当てずに 1 行知らせる。打鍵の道には何も足さない。
namespace nenenib::application
{
namespace
{
// `FormattingOptions`。Nib の固定の値（tabstop 8・noexpandtab）をそのまま渡す（可変にするのは
// #1991・設計席の決め 2026-11-23）。
[[nodiscard]] core::JsonValue formatting_options()
{
    core::JsonObject options;
    options.emplace_back("tabSize", core::JsonValue{core::json_integer(
                                        static_cast<std::int64_t>(core::vim_tab_stop))});
    options.emplace_back("insertSpaces", core::JsonValue{false});
    return core::JsonValue{std::move(options)};
}
} // namespace

// 選択範囲の整形で頼む範囲。選択（VISUAL は Vim の規則の範囲・矩形は覆う行の全体）が空なら
// キャレットの行（VS Code と同じ）。
core::OffsetRange EditorController::formatting_range() const
{
    const auto block = block_selection();
    if (block.has_value() && !block.value().lines.empty())
    {
        const core::LineNumber first = block.value().first;
        const core::LineNumber last{first.value + block.value().lines.size() - 1};
        return core::OffsetRange{state_.text().line_start(first), state_.text().line_end(last)};
    }
    const core::OffsetRange range = highlighted_range();
    if (!core::is_empty(range))
    {
        return range;
    }
    const core::LineNumber line = state_.text().line_containing(state_.selection().caret);
    return core::OffsetRange{state_.text().line_start(line), state_.text().line_end(line)};
}

bool EditorController::request_formatting(bool selection)
{
    const auto slot = requestable_slot(selection ? &ServerAbilities::range_formatting
                                                 : &ServerAbilities::formatting);
    if (!slot.has_value())
    {
        return false;
    }
    core::JsonObject document;
    document.emplace_back("uri", core::JsonValue{active_document_of(slot.value()).uri});
    core::JsonObject params;
    params.emplace_back("textDocument", core::JsonValue{std::move(document)});
    params.emplace_back("options", formatting_options());
    if (selection)
    {
        const core::OffsetRange range = formatting_range();
        const auto encoding = link_of(slot.value()).encoding;
        core::JsonObject placed;
        placed.emplace_back("start", core::lsp_position_json(core::lsp_position_of(
                                         state_.text(), range.begin, encoding)));
        placed.emplace_back("end", core::lsp_position_json(
                                       core::lsp_position_of(state_.text(), range.end, encoding)));
        params.emplace_back("range", core::JsonValue{std::move(placed)});
    }
    send_language_request(slot.value(),
                          selection ? LspRequestKind::range_formatting : LspRequestKind::formatting,
                          core::JsonValue{std::move(params)}, request_context());
    return true;
}

// 入力の途中か（#1997 のレビュー L5・2 回目のレビュー P2・F1）。答えは合図（WorkCompleted）の中で
// 届くので、入力行（Ctrl+P の一覧を含む）・右クリックのメニュー・IME の変換・補完の一覧・設定の
// 画面が開いている間（faces_closed）と、Vim が次の鍵を待っている間（vim_waits_for_key）と、Vim の
// INSERT を組み立てている間（矩形の `I` `A` の写し・`3i` の回数・`.` の記録が生きている間。当てると
// replace の割り込みがどれも捨てる）は本文を変えない。
bool EditorController::formatting_interrupts() const noexcept
{
    if (!faces_closed() || vim_waits_for_key())
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
    const core::VimState &vim = state_.vim();
    return vim.mode == core::VimMode::insert &&
           (vim.block_insert.has_value() || vim.insert_repeat.has_value() ||
            vim.recording.has_value());
}

// 当てずに知らせる理由（無ければ値なし）。文書か revision が頼んだときと違う・入力の途中・読めない
// 答え（誤りの答え・形の崩れ）。
std::optional<core::UiMessage>
EditorController::formatting_refusal(const LanguageFormatting &answered) const
{
    const LspRequestContext current = request_context();
    if (answered.context.document != current.document ||
        answered.context.revision != current.revision)
    {
        return core::UiMessage::notice_formatting_stale;
    }
    if (formatting_interrupts())
    {
        return core::UiMessage::notice_formatting_busy;
    }
    if (!answered.edits.has_value())
    {
        return core::UiMessage::notice_formatting_unreadable;
    }
    return std::nullopt;
}

// 届いた整形の答えを 1 回使って消す。当てられなければ理由を 1 行知らせる（formatting_refusal・
// 写せない位置と重なる編集も読めない答え）。空の列と、今の本文と同じ結果は何もしない（undo の単位を
// 積まない）。Vim の VISUAL は NORMAL へ戻す（Vim の `gq` `=` と同じく選択を終える）。
void EditorController::settle_formatting_answer()
{
    LanguageAnswers answers = state_.language_answers();
    if (!answers.formatting.has_value())
    {
        return;
    }
    const LanguageFormatting answered = std::move(answers.formatting).value();
    answers.formatting.reset();
    state_ = state_.with_language_answers(std::move(answers));
    const auto refusal = formatting_refusal(answered);
    if (refusal.has_value())
    {
        state_ = state_.with_command_message(ui_notice(refusal.value()));
        return;
    }
    // 読めない答えは formatting_refusal が断った後なので、ここでは値がある。
    std::vector<core::LspTextEdit> edits =
        answered.edits.value_or(std::vector<core::LspTextEdit>{});
    if (edits.empty())
    {
        return;
    }
    // newText の改行は貼り付けと同じく本文の改行の形へ揃えてから組む（D19・#1997 のレビュー R2）。
    const std::string_view newline = core::newline_of(state_.line_ending());
    for (core::LspTextEdit &edit : edits)
    {
        edit.new_text =
            detail::with_document_newlines(core::clipboard_line_feeds(edit.new_text), newline);
    }
    const auto span =
        core::formatted_span(state_.text(), edits, answered.encoding, state_.selection().caret);
    if (!span.has_value())
    {
        state_ =
            state_.with_command_message(ui_notice(core::UiMessage::notice_formatting_unreadable));
        return;
    }
    const core::FormattedSpan &formatted = span.value();
    if (state_.text().text_range(formatted.range.begin, formatted.range.end) == formatted.text)
    {
        return;
    }
    // マーク（`ma`・`''`・`'.`・ジャンプと変更の列）ともう 1 つの枠は、畳んだ 1 区間でなく
    // 編集ごとに直す（FormattingPieces・undo の単位は 1 つのまま・#1997 のレビュー R1）。
    // `u` と `Ctrl-r` も同じ列で直すように、積んだ単位と列を脇に残す（2 回目のレビュー P1）。
    const std::size_t before = state_.history().position();
    replace(formatted.range, formatted.text, core::EditBoundary::separate,
            FormattingPieces{formatted.pieces});
    remember_formatted_unit(before, formatted);
    leave_visual_for_formatting();
    move_caret_to(formatted.caret, core::SelectionAnchoring::collapse);
}

// 整形が積んだ単位を脇に残す（#1997 の 2 回目のレビュー P1）。replace が新しい単位を 1 つ積み、
// その単位が頼んだ区間と本文のままのとき（eol の旗の最後の改行を足し引きしていない）だけ残す。同じ
// 文書の同じ位置かその後ろの古い物は、この単位が積まれた時点で戻せないので外す。
void EditorController::remember_formatted_unit(std::size_t before,
                                               const core::FormattedSpan &formatted)
{
    const core::EditHistory &history = state_.history();
    const auto edit = history.applied(before);
    if (history.position() != before + 1 || !edit.has_value() ||
        edit.value().at != formatted.range.begin || edit.value().inserted != formatted.text ||
        edit.value().removed.size() != formatted.range.end.value - formatted.range.begin.value)
    {
        return;
    }
    const AutosaveId document = request_context().document;
    std::erase_if(formatted_units_, [document, before](const FormattedUnit &unit)
                  { return unit.document == document && unit.index >= before; });
    formatted_units_.push_back(FormattedUnit{
        .document = document, .index = before, .edit = edit.value(), .pieces = formatted.pieces});
}

// 履歴の index 番目の単位の編集の列（#1997 の 2 回目のレビュー P1）。脇に残した単位と文書・位置・
// 編集が同じときだけ返し、位置が同じで編集が違えば（戻した後に別の単位が積まれた）外す。undo は
// 今の本文（整形の後）の上の逆向きの列: 各編集の後の区間 [始まり + 手前の差の和, + newText の長さ)
// を編集の前の本文（単位の removed から切る）で置き換える。redo は残した列のまま。
std::optional<std::vector<core::VimBlockEdit>>
EditorController::formatted_pieces_at(std::size_t index, const core::Edit &edit,
                                      core::HistoryDirection direction)
{
    if (formatted_units_.empty())
    {
        return std::nullopt;
    }
    const AutosaveId document = request_context().document;
    const auto found =
        std::ranges::find_if(formatted_units_, [document, index](const auto &unit)
                             { return unit.document == document && unit.index == index; });
    if (found == formatted_units_.end())
    {
        return std::nullopt;
    }
    if (!(found->edit == edit))
    {
        formatted_units_.erase(found);
        return std::nullopt;
    }
    switch (direction)
    {
    case core::HistoryDirection::redo:
        return found->pieces;
    case core::HistoryDirection::undo:
        break;
    }
    std::vector<core::VimBlockEdit> undone;
    undone.reserve(found->pieces.size());
    std::size_t removed_before = 0;
    std::size_t added_before = 0;
    for (const core::VimBlockEdit &piece : found->pieces)
    {
        const std::size_t length = piece.range.end.value - piece.range.begin.value;
        const core::Offset begin{piece.range.begin.value - removed_before + added_before};
        undone.push_back(core::VimBlockEdit{
            .range = core::OffsetRange{begin, core::Offset{begin.value + piece.utf8.size()}},
            .utf8 = edit.removed.substr(piece.range.begin.value - edit.at.value, length)});
        removed_before += length;
        added_before += piece.utf8.size();
    }
    return undone;
}

// Vim の VISUAL を NORMAL へ戻す（整形の答えを当てた後だけ）。NORMAL のキャレットの寄せは意図の
// 入口の終わりの keep_vim_character が 1 回だけ行う。
void EditorController::leave_visual_for_formatting()
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
    case core::VimMode::normal:
    case core::VimMode::insert:
        return;
    case core::VimMode::visual:
    case core::VimMode::visual_line:
    case core::VimMode::visual_block:
        break;
    }
    core::VimState vim = state_.vim();
    vim.mode = core::VimMode::normal;
    state_ = state_.with_vim(std::move(vim));
}
} // namespace nenenib::application
