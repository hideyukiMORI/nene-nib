#include "EditorController.hpp"

#include "CommandChoice.hpp"
#include "CommandChoiceKind.hpp"
#include "Diagnostic.hpp"
#include "DisplayText.hpp"
#include "EditMode.hpp"
#include "EditorOperation.hpp"
#include "ExPaletteRequest.hpp"
#include "JsonNumber.hpp"
#include "JsonValue.hpp"
#include "KeyChord.hpp"
#include "LanguageAnswers.hpp"
#include "LanguageCodeActionResolve.hpp"
#include "LanguageCodeActions.hpp"
#include "LanguageServerDocument.hpp"
#include "LanguageServerLink.hpp"
#include "LanguageServerPhase.hpp"
#include "LanguageServerSlot.hpp"
#include "LspCodeAction.hpp"
#include "LspCodeActions.hpp"
#include "LspCommand.hpp"
#include "LspDiagnostics.hpp"
#include "LspEditContext.hpp"
#include "LspJsonRead.hpp"
#include "LspMessages.hpp"
#include "LspPosition.hpp"
#include "LspPositionEncoding.hpp"
#include "LspPositionJson.hpp"
#include "LspRequest.hpp"
#include "LspRequestKind.hpp"
#include "LspResponse.hpp"
#include "LspWorkspaceEditFailure.hpp"
#include "LspWorkspaceEdits.hpp"
#include "NoticeLine.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "PaletteOrigin.hpp"
#include "PaletteScope.hpp"
#include "PendingLspRequest.hpp"
#include "ServerAbilities.hpp"
#include "ServerEditOrigin.hpp"
#include "UiMessage.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

// クイックフィックス（#2005・ADR 0194 の決定 8〜10・6）。Ctrl+.・Vim の `gra`・操作
// 「クイックフィックス」でキャレット（選択があればその範囲）の直し方を頼み、答えを Ctrl+P の一覧の
// 記号 `=` に出す。選ぶと edit → command の順に進み、edit は apply_workspace_edit の 1 本で当てる。
// edit が無く data があり席が resolve に答えるなら `codeAction/resolve` で引く。command は
// `workspace/executeCommand` で送り、その答えを待つ間（待つ列にその要求がある間）だけサーバーからの
// `workspace/applyEdit` を同じ 1 本で当てる。LSP がオフ・言語サーバーが動いていない文書の打鍵の道
// には何も足さない（ADR 0190 の決定 10・D60）。
namespace nenenib::application
{
namespace
{
// LSP の `CodeActionTriggerKind.Invoked`（手で頼んだ）。
constexpr std::int64_t invoked_trigger = 1;

// 好んで選ぶ項目（`isPreferred`）の補足の頭の印。
constexpr std::string_view preferred_mark = "*";

// 範囲 range に掛かる診断か（両端を含む・キャレットなら、キャレットを含む診断）。
[[nodiscard]] bool touches(const core::Diagnostic &diagnostic, const core::OffsetRange &range)
{
    return diagnostic.range.begin.value <= range.end.value &&
           range.begin.value <= diagnostic.range.end.value;
}

// 候補の command（答えの列の位置の 10 進）→ 位置。
[[nodiscard]] std::optional<std::size_t> action_index(std::string_view command) noexcept
{
    std::size_t index = 0;
    const auto parsed = std::from_chars(command.data(), command.data() + command.size(), index);
    if (parsed.ec != std::errc{} || parsed.ptr != command.data() + command.size())
    {
        return std::nullopt;
    }
    return index;
}

// 候補の補足（kind・好んで選ぶ項目は頭に印）。どちらも無ければ値なし。
[[nodiscard]] std::optional<core::DisplayText> action_detail(const core::LspCodeAction &action)
{
    std::string detail = action.preferred ? std::string{preferred_mark} : std::string{};
    if (!action.kind.empty())
    {
        detail += action.preferred ? " " + action.kind : action.kind;
    }
    if (detail.empty())
    {
        return std::nullopt;
    }
    auto shown = core::clipped_display_text(detail);
    return shown.has_value() ? std::optional{std::move(shown).value()} : std::nullopt;
}

// 読みが断った理由 → `failureReason`（サーバーの記録に残る英語の 1 行・画面の文ではない・決定 6）。
[[nodiscard]] std::string_view failure_reason(core::LspWorkspaceEditFailure failure) noexcept
{
    switch (failure)
    {
    case core::LspWorkspaceEditFailure::malformed:
        return "the workspace edit is malformed";
    case core::LspWorkspaceEditFailure::resource_operation:
        return "resource operations are not supported";
    case core::LspWorkspaceEditFailure::needs_confirmation:
        return "change annotations that need confirmation are not supported";
    case core::LspWorkspaceEditFailure::too_many_files:
        return "the workspace edit changes too many files";
    }
    std::unreachable();
}

// `ApplyWorkspaceEditResult`（断ったら failureReason を添える）。
[[nodiscard]] core::JsonValue apply_edit_result(std::optional<std::string_view> refusal)
{
    core::JsonObject result;
    result.emplace_back("applied", core::JsonValue{!refusal.has_value()});
    if (refusal.has_value())
    {
        result.emplace_back("failureReason", core::JsonValue{std::string{refusal.value()}});
    }
    return core::JsonValue{std::move(result)};
}
} // namespace

// 頼めたら真。頼めなければ呼び手（accept(QueryLanguage)）が「〜に答えません」か「動いていません」を
// 1 行知らせる（決定 12）。範囲は code_action_range、context は範囲に掛かる診断と
// Invoked。文脈に席が `didOpen` している文書ごとの改訂番号の写しを添える（選んで当てるときの照合
// (b)(c)）。
bool EditorController::request_code_actions()
{
    const auto slot = requestable_slot(&ServerAbilities::code_action);
    if (!slot.has_value())
    {
        return false;
    }
    const auto encoding = link_of(slot.value()).encoding;
    const core::OffsetRange range = code_action_range();
    core::JsonObject document;
    document.emplace_back("uri", core::JsonValue{active_document_of(slot.value()).uri});
    core::JsonObject placed;
    placed.emplace_back("start", core::lsp_position_json(
                                     core::lsp_position_of(state_.text(), range.begin, encoding)));
    placed.emplace_back(
        "end", core::lsp_position_json(core::lsp_position_of(state_.text(), range.end, encoding)));
    core::JsonObject params;
    params.emplace_back("textDocument", core::JsonValue{std::move(document)});
    params.emplace_back("range", core::JsonValue{std::move(placed)});
    params.emplace_back("context", code_action_context(range, encoding));
    send_language_request(slot.value(), LspRequestKind::code_action,
                          core::JsonValue{std::move(params)},
                          LspEditContext{.request = request_context(),
                                         .opened = opened_revisions(slot.value()),
                                         .slot = slot.value()});
    return true;
}

// 頼む範囲（決定 8）。矩形の選択は覆う行の全体、選択（VISUAL は描く選択と同じ範囲）はその範囲、
// 選択が空ならキャレット（空の範囲）。整形の formatting_range と同じ規則で、空のときだけ違う。
core::OffsetRange EditorController::code_action_range() const
{
    const auto block = block_selection();
    if (block.has_value() && !block.value().lines.empty())
    {
        return formatting_range();
    }
    const core::OffsetRange range = highlighted_range();
    if (!core::is_empty(range))
    {
        return range;
    }
    const core::Offset caret = state_.selection().caret;
    return core::OffsetRange{caret, caret};
}

// `CodeActionContext`（決定 8）。diagnostics は範囲に掛かる今の文書の診断を受けた形で戻し
// （code の型と data・lsp_diagnostic_json の 1 本）、triggerKind は Invoked。`only` は送らない。
core::JsonValue EditorController::code_action_context(const core::OffsetRange &range,
                                                      core::LspPositionEncoding encoding) const
{
    core::JsonArray diagnostics;
    for (const core::Diagnostic &diagnostic : state_.diagnostics())
    {
        if (touches(diagnostic, range))
        {
            diagnostics.push_back(core::lsp_diagnostic_json(diagnostic, state_.text(), encoding));
        }
    }
    core::JsonObject context;
    context.emplace_back("diagnostics", core::JsonValue{std::move(diagnostics)});
    context.emplace_back("triggerKind", core::JsonValue{core::json_integer(invoked_trigger)});
    return core::JsonValue{std::move(context)};
}

// 答え（決定 9）は 1 回使って消す。文書・改訂番号・キャレットは受け取りで照合済み。入力の途中
// （server_edit_interrupts・Vim の INSERT と次の鍵の待ちを含む）と、通常モードで選択の起点が動いた
// ときは一覧を開かずに捨てる（参照と同じ・ADR 0190 の追補 3）。誤りの答えはサーバーの message、
// 空なら「ここで使える直し方はありません」。窓が非アクティブの間は控えるだけ（`=`
// で開けば見える）。
void EditorController::settle_code_actions_answer()
{
    auto answers = state_.language_answers();
    if (!answers.code_actions.has_value())
    {
        return;
    }
    LanguageCodeActions answered = std::move(answers.code_actions).value();
    answers.code_actions.reset();
    state_ = state_.with_language_answers(std::move(answers));
    const bool anchored = state_.mode() == core::EditMode::vim || awaits_nothing();
    const bool usable = anchored && !server_edit_interrupts();
    definition_anchor_.reset();
    if (!usable)
    {
        return;
    }
    if (!answered.server_error.empty())
    {
        auto line = core::notice_line(answered.server_error);
        state_ = state_.with_command_message(
            line.has_value() ? std::move(line).value()
                             : ui_notice(core::UiMessage::notice_code_actions_not_found));
        return;
    }
    if (answered.actions.empty())
    {
        state_ =
            state_.with_command_message(ui_notice(core::UiMessage::notice_code_actions_not_found));
        return;
    }
    held_code_actions_ = std::move(answered);
    if (window_inactive_)
    {
        return;
    }
    run_palette_request(core::ExPaletteRequest{core::PaletteScope::code_actions, {}});
}

// `=` を見せるときに 1 回だけ作る候補（サーバーの順のまま）。控えた答えが今の文書の今の改訂の
// ものでなければ出さない（選んでも照合 (a) で断られる）。題名は title、補足は kind（訳さない）。
void EditorController::append_code_action_choices(std::vector<core::CommandChoice> &entries) const
{
    if (!held_code_actions_.has_value() || !code_actions_shown())
    {
        return;
    }
    const auto &actions = held_code_actions_.value().actions;
    for (std::size_t index = 0; index < actions.size(); ++index)
    {
        auto label = core::clipped_display_text(actions.at(index).title);
        if (!label.has_value())
        {
            continue;
        }
        entries.push_back(core::CommandChoice{std::move(label).value(),
                                              std::to_string(index),
                                              core::CommandChoiceKind::land,
                                              action_detail(actions.at(index)),
                                              core::PaletteOrigin::code_action,
                                              std::nullopt,
                                              {}});
    }
}

// 今の一覧に出せる控えがあるか（空の 1 行を出すかの判定・append_code_action_choices と同じ条件）。
bool EditorController::code_actions_shown() const
{
    return held_code_actions_.has_value() && !held_code_actions_.value().actions.empty() &&
           !server_edit_stale(held_code_actions_.value().context.request);
}

// 候補を選ぶ（決定
// 10）。一覧を閉じ、一覧を開いてから本文が変わっていたら（改訂番号で照らす）当てずに 1 行知らせる。
void EditorController::choose_code_action(const core::CommandChoice &choice)
{
    close_command_input();
    const auto index = action_index(choice.command);
    if (!held_code_actions_.has_value() || !index.has_value() ||
        index.value() >= held_code_actions_.value().actions.size())
    {
        return;
    }
    const LanguageCodeActions held = held_code_actions_.value();
    if (server_edit_stale(held.context.request))
    {
        state_ = state_.with_command_message(ui_notice(core::UiMessage::notice_server_edit_stale));
        return;
    }
    run_code_action(held.actions.at(index.value()), held.context, held.encoding);
}

// edit → command の順（決定 10・LSP の文）。edit は apply_workspace_edit の 1 本で当て、断ったら
// command を送らない。edit が無く data があり、答えた席が resolve に答えるなら `codeAction/resolve`
// で引く（続きは届いた答えで）。Vim の VISUAL は当てた後に NORMAL へ戻す（整形と同じ）。
void EditorController::run_code_action(const core::LspCodeAction &action,
                                       const LspEditContext &context,
                                       core::LspPositionEncoding encoding)
{
    if (action.edit.has_value())
    {
        const auto edit = core::lsp_workspace_edit(action.edit.value());
        if (!edit.has_value())
        {
            state_ = state_.with_command_message(server_edit_refusal(edit.error()));
            return;
        }
        if (apply_workspace_edit(edit.value(), context, encoding, ServerEditOrigin::answer)
                .has_value())
        {
            return;
        }
        leave_visual_for_formatting();
    }
    else if (action.resolvable && link_of(context.slot).phase == LanguageServerPhase::running &&
             link_of(context.slot).abilities.code_action_resolve)
    {
        send_language_request(context.slot, LspRequestKind::code_action_resolve, action.source,
                              context);
        return;
    }
    if (action.command.has_value())
    {
        send_execute_command(context.slot, action.command.value());
    }
}

// resolve の答え（決定 10）。誤りの答えはサーバーの message を 1 行で、読めない答えは「読めない」。
// 届いた項目は edit → command の順に進める（もう一度 resolve は頼まない）。照合は当てるときに
// apply_workspace_edit がする。
void EditorController::settle_code_action_resolve_answer()
{
    auto answers = state_.language_answers();
    if (!answers.code_action_resolve.has_value())
    {
        return;
    }
    LanguageCodeActionResolve answered = std::move(answers.code_action_resolve).value();
    answers.code_action_resolve.reset();
    state_ = state_.with_language_answers(std::move(answers));
    if (!answered.server_error.empty() || !answered.action.has_value())
    {
        auto line = core::notice_line(answered.server_error);
        state_ = state_.with_command_message(
            line.has_value() ? std::move(line).value()
                             : ui_notice(core::UiMessage::notice_server_edit_unreadable));
        return;
    }
    core::LspCodeAction resolved = std::move(answered.action).value();
    resolved.resolvable = false;
    run_code_action(resolved, answered.context, answered.encoding);
}

// `workspace/executeCommand` を送る（決定 10）。席が running で、`executeCommandProvider.commands`
// に 名前があるときだけ（無い名前は client の側の命令で Nib
// には実行できない・何も知らせない）。当てた edit の `didChange`
// が命令より先に届くように席の文書の変更を先に流し、その後の写しを文脈に置く
// （`workspace/applyEdit` の照合 (b)(c)）。同じ種類の古い要求は取り消す（待ちは 1 つだけ）。
void EditorController::send_execute_command(LanguageServerSlot slot,
                                            const core::LspCommand &command)
{
    const auto &link = link_of(slot);
    if (link.phase != LanguageServerPhase::running ||
        std::ranges::find(link.abilities.execute_commands, command.command) ==
            link.abilities.execute_commands.end())
    {
        return;
    }
    flush_language_server_documents(slot);
    send_language_request(
        slot, LspRequestKind::execute_command, core::lsp_execute_command_params(command),
        LspEditContext{
            .request = request_context(), .opened = opened_revisions(slot), .slot = slot});
}

// 席 slot が開いた文書の未送信の変更を全部流す（命令より先に届ける）。
void EditorController::flush_language_server_documents(LanguageServerSlot slot)
{
    for (LanguageServerDocument &document : link_of(slot).documents)
    {
        flush_language_server_changes(slot, document);
    }
}

// 命令の答え（決定 10）。誤りの答えはサーバーの message を訳さずに 1 行で出す。成功は何も出さない
// （編集は `workspace/applyEdit` で届いた分だけ・何も起きなかったときは 1 行も出さない）。
void EditorController::settle_execute_command_answer(const core::LspResponse &response)
{
    if (response.result.has_value())
    {
        return;
    }
    auto line = core::notice_line(response.result.error().message);
    if (line.has_value())
    {
        state_ = state_.with_command_message(std::move(line).value());
    }
}

// サーバーからの `workspace/applyEdit`（決定 6）。当てたら `{applied: true}`、断ったら
// `{applied: false, failureReason}` を返す。
void EditorController::answer_apply_edit(LanguageServerSlot slot, const core::LspRequest &request)
{
    const auto refusal = apply_edit_refusal(slot, request);
    ports_.language_server.send(slot,
                                core::lsp_result_body(request.id, apply_edit_result(refusal)));
}

// 当てたら値なし、断ったら英語の理由。Nib が送った `workspace/executeCommand` の答えを待っていない
// （待つ列にその要求が無い・答え・取り消し・古さ・席の止まりで下りる）・形が崩れている・入力の途中
// （server_edit_interrupts・Nib の側で待たない）・照合と検査が断る、のどれも断る。照合は命令を
// 送ったときの写しで (b)(c) だけ（頼んだ文書が無いので (a) は飛ばす・ServerEditOrigin）。
std::optional<std::string_view>
EditorController::apply_edit_refusal(LanguageServerSlot slot, const core::LspRequest &request)
{
    const auto &pending = link_of(slot).pending;
    const auto awaited =
        std::ranges::find(pending, LspRequestKind::execute_command, &PendingLspRequest::kind);
    const auto *context =
        awaited != pending.end() ? std::get_if<LspEditContext>(&awaited->context) : nullptr;
    if (context == nullptr)
    {
        return "Nib is not waiting for a workspace/executeCommand";
    }
    const LspEditContext copied = *context;
    const auto *params = core::json_object_of(request.params);
    const auto member = params != nullptr ? core::json_member(*params, "edit") : std::nullopt;
    const auto edit = member.has_value()
                          ? core::lsp_workspace_edit(member.value().get())
                          : std::unexpected{core::LspWorkspaceEditFailure::malformed};
    if (!edit.has_value())
    {
        state_ = state_.with_command_message(server_edit_refusal(edit.error()));
        return failure_reason(edit.error());
    }
    if (server_edit_interrupts())
    {
        state_ = state_.with_command_message(ui_notice(core::UiMessage::notice_server_edit_busy));
        return "the editor is in the middle of input";
    }
    if (apply_workspace_edit(edit.value(), copied, link_of(slot).encoding,
                             ServerEditOrigin::server_request)
            .has_value())
    {
        return "the documents changed since the command was sent or could not be edited";
    }
    // 当てた後の本文はサーバーが当てたと見なす本文と同じなので、同じ命令の次の
    // `workspace/applyEdit`
    // はこの写しで照らす（同じ意図の中で写すので、使う人の打鍵は挟まらない）。
    renew_awaited_command(slot);
    return std::nullopt;
}

// 待っている命令の文脈の写しを今の本文で置き直す（apply_edit_refusal が当てた後だけ）。
void EditorController::renew_awaited_command(LanguageServerSlot slot)
{
    auto &pending = link_of(slot).pending;
    const auto awaited =
        std::ranges::find(pending, LspRequestKind::execute_command, &PendingLspRequest::kind);
    if (awaited != pending.end())
    {
        awaited->context = LspEditContext{
            .request = request_context(), .opened = opened_revisions(slot), .slot = slot};
    }
}
} // namespace nenenib::application
