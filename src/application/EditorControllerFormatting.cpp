#include "EditorController.hpp"

#include "AutosaveId.hpp"
#include "ClipboardText.hpp"
#include "EditBoundary.hpp"
#include "EditMode.hpp"
#include "EditorControllerDetail.hpp"
#include "FormattingPieces.hpp"
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
#include "OffsetRange.hpp"
#include "Selection.hpp"
#include "SelectionAnchoring.hpp"
#include "ServerAbilities.hpp"
#include "UiMessage.hpp"
#include "VimBlockEdit.hpp"
#include "VimMode.hpp"
#include "VirtualColumn.hpp"

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

// 入力の途中か（#1997 のレビュー L5）。答えは合図（WorkCompleted）の中で届くので、入力行・Ctrl+P の
// 一覧・右クリックのメニュー・IME の変換・補完の一覧・設定の画面が開いている間と、Vim が次の鍵を
// 待っている間（回数・オペレータ・1 鍵の待ち・レジスタの名指し・イージーモーション・INSERT の
// `Ctrl-o`・`map` の左辺の途中）は本文を変えない。
bool EditorController::formatting_interrupts() const noexcept
{
    if (!faces_closed() || command_palette_active())
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
    return vim.count.has_value() || vim.pending.has_value() || vim.input_wait.has_value() ||
           vim.selected_register.has_value() || vim.register_count.has_value() ||
           vim.easy_motion.has_value() || vim.insert_restart.has_value() ||
           !state_.key_map_pending().empty();
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
    // マーク（`ma`・`''`・`'.`・ジャンプと変更の列）ともう 1 つの枠は、畳んだ 1
    // 区間でなく編集ごとに 直す（矩形の道の口・undo の単位は 1 つのまま・#1997 のレビュー R1）。
    replace(formatted.range, formatted.text, core::EditBoundary::separate,
            FormattingPieces{formatted.pieces});
    leave_visual_for_formatting();
    move_caret_to(formatted.caret, core::SelectionAnchoring::collapse);
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
