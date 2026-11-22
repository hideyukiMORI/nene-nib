#include "EditorController.hpp"

#include "CaretMove.hpp"
#include "CaretUnit.hpp"
#include "CommandChoice.hpp"
#include "CommandChoiceKind.hpp"
#include "CommandPalette.hpp"
#include "Diagnostic.hpp"
#include "DiagnosticCommand.hpp"
#include "DiagnosticJump.hpp"
#include "DisplayText.hpp"
#include "EditorControllerDetail.hpp"
#include "ExPaletteRequest.hpp"
#include "LanguageAnswers.hpp"
#include "LanguageHover.hpp"
#include "LanguageServerSlot.hpp"
#include "LspRequestKind.hpp"
#include "NoticeLine.hpp"
#include "PaletteDiagnostic.hpp"
#include "PaletteOrigin.hpp"
#include "PaletteScope.hpp"
#include "PlaceCaret.hpp"
#include "PreviewAnchor.hpp"
#include "RunDiagnosticCommand.hpp"
#include "SharedDiagnostics.hpp"
#include "SwitchTab.hpp"
#include "TextPosition.hpp"
#include "UiMessage.hpp"
#include "VimDiagnosticCommand.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

// 診断の間を跳ぶ鍵・札・一覧（#1939・ADR 0161 の決定 9 の追補）。読むのは文書の値の診断だけで、
// サーバーへは何も頼まない。lsp がオフ・診断が無いときは鍵を押したときだけ欄を 1 つ読む（打鍵の
// 道に費用を足さない）。
namespace nenenib::application
{
namespace
{
// 候補の command（控えた列の位置の 10 進）→ 位置。
[[nodiscard]] std::optional<std::size_t> listed_index(std::string_view command) noexcept
{
    std::size_t index = 0;
    const auto parsed = std::from_chars(command.data(), command.data() + command.size(), index);
    if (parsed.ec != std::errc{} || parsed.ptr != command.data() + command.size())
    {
        return std::nullopt;
    }
    return index;
}

// 診断の始まりの行の中の印の区間（行頭からのバイト・行をまたぐ診断は行末まで）。
[[nodiscard]] PreviewAnchor anchor_of(const core::TextBuffer &text,
                                      const core::Diagnostic &diagnostic)
{
    const core::LineNumber line = text.line_containing(diagnostic.range.begin);
    const std::size_t start = text.line_start(line).value;
    const std::size_t begin = diagnostic.range.begin.value;
    const std::size_t end =
        std::clamp(diagnostic.range.end.value, begin, std::max(begin, text.line_end(line).value));
    return PreviewAnchor{line, begin - start, end - start};
}
} // namespace

void EditorController::accept(const RunDiagnosticCommand &intent)
{
    // 設定の面が開いていた（この意図の入口で閉じた）なら、何もしない（面どうしの排他）。
    if (settings_closed_now_)
    {
        return;
    }
    // IME の変換中の F8 / Shift+F8 は何もしない（変換の文字を確定させずに跳ばない・一覧と同じ）。
    if (state_.composition().has_value())
    {
        return;
    }
    static_cast<void>(run_diagnostic_command(intent.command, 1));
}

// `]d` `[d` `gl`。engine の歩は表に行が無かったときと同じ失敗のまま返り、跳べた・札を出せたら
// stepped_vim が成功の歩に読み替える（#1939 のレビュー R3 の推奨 3）。
void EditorController::perform(const core::VimDiagnosticCommand &effect)
{
    vim_effect_landed_ = run_diagnostic_command(effect.command, effect.count);
}

bool EditorController::run_diagnostic_command(core::DiagnosticCommand command, std::size_t count)
{
    switch (command)
    {
    case core::DiagnosticCommand::next:
        return step_diagnostic(true, count);
    case core::DiagnosticCommand::previous:
        return step_diagnostic(false, count);
    case core::DiagnosticCommand::show:
        if (!show_diagnostic_tip(state_.selection().caret))
        {
            state_ = state_.with_command_message(ui_notice(
                state_.diagnostics().empty() ? core::UiMessage::notice_no_diagnostics
                                             : core::UiMessage::notice_no_diagnostic_here));
            return false;
        }
        return true;
    case core::DiagnosticCommand::list:
        open_diagnostic_list();
        return true;
    }
    std::unreachable();
}

// 次・前の診断の始まりへ跳ぶ（`''` とジャンプの列に前の位置を置く・Neovim と同じ）。跳んだ先の
// 診断の文の全部を札で見せる。診断が無ければ 1 行知らせて偽。Vim の鍵の効果の中では、クリックの
// 道（PlaceCaret の INSERT の割り込みと始め直し）を通らず、VimMoveTo と同じく置いて NORMAL の
// 寄せだけを掛ける（INSERT の `Ctrl-o` の始め直しは鍵の出口が 1 回だけ行う・レビュー R3 の推奨
// 4）。
bool EditorController::step_diagnostic(bool forward, std::size_t count)
{
    const auto landing =
        core::diagnostic_landing(state_.diagnostics(), state_.selection().caret, forward, count);
    if (!landing.has_value())
    {
        state_ = state_.with_command_message(ui_notice(core::UiMessage::notice_no_diagnostics));
        return false;
    }
    set_context_mark();
    const core::TextPosition position = state_.text().position_of(landing.value());
    if (in_vim_effect_)
    {
        move_caret_to(core::placed_caret(state_.text(), position, core::CaretUnit::code_point),
                      core::SelectionAnchoring::collapse);
        settle_vim_caret();
    }
    else
    {
        accept(PlaceCaret{position, core::SelectionAnchoring::collapse});
    }
    // 札の文は着地の位置の診断（Vim の NORMAL が行末の改行の上から 1 字戻しても同じ診断を見せる）。
    static_cast<void>(show_diagnostic_tip(landing.value()));
    return true;
}

// at の上の診断の札を hover の札と同じ欄に置く（文脈は今のキャレット・次の操作で消える・ADR 0190
// の決定 6）。置けたら真。出ている hover の要求は取り消す（後から届いた古い答えが札を消したり
// 上書きしたりしない・#1939 のレビュー R3 の推奨 1）。
bool EditorController::show_diagnostic_tip(core::Offset at)
{
    std::string text = core::diagnostic_tip_text(state_.diagnostics(), at);
    if (text.empty())
    {
        return false;
    }
    for (const LanguageServerSlot slot : language_server_slots)
    {
        cancel_lsp_requests(slot, LspRequestKind::hover);
    }
    LanguageAnswers answers = state_.language_answers();
    answers.hover = LanguageHover{request_context(), std::move(text)};
    state_ = state_.with_language_answers(std::move(answers));
    return true;
}

// 開いている入力行は取消と同じ道で閉じてから、Ctrl+P の一覧を診断の記号で開く（操作の一覧と
// 同じ形・録画に `<Esc>` を積まない）。
void EditorController::open_diagnostic_list()
{
    if (state_.composition().has_value())
    {
        return;
    }
    if (command_line_active())
    {
        cancel_command_input();
    }
    run_palette_request(core::ExPaletteRequest{core::PaletteScope::diagnostics, {}});
}

// 面を開くときに 1 回だけ作る診断の候補（ADR 0062 の決定 15）。アクティブな文書が先、続いて帯の
// 順にほかの読み込み済みのタブ（まだ読んでいないタブは診断を持たない）。
void EditorController::append_diagnostic_choices(std::vector<core::CommandChoice> &entries)
{
    palette_diagnostics_.clear();
    const auto views = detail::tab_views(state_, active_document_view());
    const auto name_of = [&views](std::size_t tab)
    {
        const DocumentView &view = views.at(tab);
        return view.path.has_value() ? std::string(view.path.value().file_name())
                                     : std::string(view.title.text());
    };
    append_tab_diagnostics(state_.active_tab(), name_of(state_.active_tab()), entries);
    for (std::size_t tab = 0; tab < state_.tab_count(); ++tab)
    {
        if (tab != state_.active_tab())
        {
            append_tab_diagnostics(tab, name_of(tab), entries);
        }
    }
}

// 帯の位置 tab の文書の診断（アクティブ・読み込み済みの脇の束）。まだ読んでいない文書は空。
std::span<const core::Diagnostic> EditorController::tab_diagnostics(std::size_t tab) const
{
    if (tab == state_.active_tab())
    {
        return state_.diagnostics();
    }
    const DocumentState *bundle = state_.parked_bundle_at(tab);
    return bundle == nullptr ? std::span<const core::Diagnostic>{}
                             : diagnostics_of(bundle->diagnostics);
}

// 1 つの文書の候補（行 → 桁 → 重さ）。行は「重さの印 ファイル名:行:桁」と、補足に文の最初の行。
void EditorController::append_tab_diagnostics(std::size_t tab, std::string_view name,
                                              std::vector<core::CommandChoice> &entries)
{
    const std::span<const core::Diagnostic> diagnostics = tab_diagnostics(tab);
    if (diagnostics.empty())
    {
        return;
    }
    const auto text = tab_text_at(tab);
    if (!text.has_value())
    {
        return;
    }
    const core::TextBuffer &body = *text.value().first;
    for (const std::size_t index : core::diagnostic_order(diagnostics))
    {
        const core::Diagnostic &diagnostic = diagnostics[index];
        const core::TextPosition at = body.position_of(diagnostic.range.begin);
        auto label = core::clipped_display_text(
            std::string(core::diagnostic_severity_mark(diagnostic.severity)) + " " +
            std::string(name) + ":" + std::to_string(at.line.value) + ":" +
            std::to_string(at.column.value));
        if (!label.has_value())
        {
            continue;
        }
        entries.push_back(core::CommandChoice{std::move(label).value(),
                                              std::to_string(palette_diagnostics_.size()),
                                              core::CommandChoiceKind::land,
                                              core::notice_line(diagnostic.message),
                                              core::PaletteOrigin::diagnostic,
                                              std::nullopt,
                                              {}});
        palette_diagnostics_.push_back(PaletteDiagnostic{tab, at, anchor_of(body, diagnostic)});
    }
}

std::optional<PaletteDiagnostic>
EditorController::palette_diagnostic_of(const core::CommandChoice &choice) const
{
    if (choice.kind != core::CommandChoiceKind::land ||
        choice.origin != std::optional{core::PaletteOrigin::diagnostic})
    {
        return std::nullopt;
    }
    const auto index = listed_index(choice.command);
    if (!index.has_value() || index.value() >= palette_diagnostics_.size())
    {
        return std::nullopt;
    }
    return palette_diagnostics_.at(index.value());
}

// 候補を選ぶ（#1939）。面を閉じ、ほかのタブなら SwitchTab（reach_tab を通る切り替えの道）で
// 切り替えてから、`''` を置いてその位置へ。切り替えられなければ（知らせは切り替えの道が出す）
// 置かない。
void EditorController::land_palette_diagnostic(const core::CommandChoice &choice)
{
    const auto target = palette_diagnostic_of(choice);
    close_command_input();
    if (!target.has_value())
    {
        return;
    }
    if (target.value().tab != state_.active_tab())
    {
        accept(SwitchTab{target.value().tab});
        if (target.value().tab != state_.active_tab())
        {
            return;
        }
    }
    set_context_mark();
    accept(PlaceCaret{target.value().at, core::SelectionAnchoring::collapse});
}
} // namespace nenenib::application
