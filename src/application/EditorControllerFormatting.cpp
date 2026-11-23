#include "EditorController.hpp"

#include "AutosaveId.hpp"
#include "EditBoundary.hpp"
#include "EditMode.hpp"
#include "JsonNumber.hpp"
#include "JsonValue.hpp"
#include "LanguageAnswers.hpp"
#include "LanguageFormatting.hpp"
#include "LanguageServerDocument.hpp"
#include "LineNumber.hpp"
#include "LspFormatting.hpp"
#include "LspPosition.hpp"
#include "LspPositionJson.hpp"
#include "LspRequestKind.hpp"
#include "OffsetRange.hpp"
#include "Selection.hpp"
#include "SelectionAnchoring.hpp"
#include "ServerAbilities.hpp"
#include "UiMessage.hpp"
#include "VimMode.hpp"
#include "VirtualColumn.hpp"

#include <cstdint>
#include <optional>
#include <utility>

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

// 届いた整形の答えを 1 回使って消す。文書か revision が頼んだときと違えば当てずに知らせ、読めない
// 答え（形の崩れ・写せない位置・重なる編集）も知らせる。空の列と、今の本文と同じ結果は何もしない
// （undo の単位を積まない）。Vim の VISUAL は NORMAL へ戻す（Vim の `gq` `=`
// と同じく選択を終える）。
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
    const LspRequestContext current = request_context();
    if (answered.context.document != current.document ||
        answered.context.revision != current.revision)
    {
        state_ = state_.with_command_message(ui_notice(core::UiMessage::notice_formatting_stale));
        return;
    }
    if (!answered.edits.has_value())
    {
        state_ =
            state_.with_command_message(ui_notice(core::UiMessage::notice_formatting_unreadable));
        return;
    }
    if (answered.edits.value().empty())
    {
        return;
    }
    const auto span = core::formatted_span(state_.text(), answered.edits.value(), answered.encoding,
                                           state_.selection().caret);
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
    replace(formatted.range, formatted.text, core::EditBoundary::separate);
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
