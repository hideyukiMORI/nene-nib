#include "EditorController.hpp"

#include "AutosaveId.hpp"
#include "EditMode.hpp"
#include "JsonNumber.hpp"
#include "JsonValue.hpp"
#include "LanguageAnswers.hpp"
#include "LanguageServerDocument.hpp"
#include "LanguageServerPhase.hpp"
#include "LanguageSignature.hpp"
#include "LineNumber.hpp"
#include "LineView.hpp"
#include "LspPositionJson.hpp"
#include "LspRequestKind.hpp"
#include "ServerAbilities.hpp"
#include "SignatureTip.hpp"
#include "UiMessage.hpp"
#include "VimMode.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// 引数の情報（#1959・ADR 0191 の決定 6）。挿入中にサーバーの `triggerCharacters` の 1 文字を打った
// とき（札が出ている間は `retriggerCharacters` も）と、操作「引数の情報を表示」で頼む。答えは
// hover の札と別の欄（signature_tip_）に置き、打鍵では消さない。消すのは settle_signature_tip の
// 1 か所と、`)`・Esc（通常モードの CancelSelection）・空の答え。
namespace nenenib::application
{
namespace
{
// LSP の `SignatureHelpTriggerKind`（Invoked と TriggerCharacter）。
constexpr std::int64_t invoked_trigger = 1;
constexpr std::int64_t character_trigger = 2;

[[nodiscard]] bool listed(const std::vector<std::string> &characters, std::string_view typed)
{
    return std::ranges::find(characters, typed) != characters.end();
}
} // namespace

// 打鍵の道（accept(InsertText) と Vim の INSERT の打鍵）。呼ぶのは言語サーバーが running のとき
// だけで、能力の無い席では欄を 1 つ読んで返る（打鍵の道に費用を足さない・ADR 0190 の決定 10）。
// `)` は札を消し、trigger か（消す前に札が出ていて）retrigger なら頼み直す（入れ子の呼び出しの
// 外側の札はその答えが戻す・`)` が retrigger だけのサーバーも同じ・#1968 のレビュー c-R1）。
void EditorController::settle_typed_signature(std::string_view typed)
{
    const ServerAbilities &abilities = active_language_server().abilities;
    if (!abilities.signature_help || typed.empty())
    {
        return;
    }
    const bool shown = signature_tip_.has_value();
    if (typed == ")")
    {
        signature_tip_.reset();
    }
    const bool asked = listed(abilities.signature_triggers, typed) ||
                       (shown && listed(abilities.signature_retriggers, typed));
    if (asked)
    {
        static_cast<void>(request_signature_help(std::string{typed}, shown));
    }
}

// `SignatureHelpParams`。trigger が無ければ手で頼んだ（Invoked）。retrigger は札が出ている間の
// 頼み直し。新しい要求は前の「手で頼んだ」の印を下ろす（捨てられた答えの印を残さない・c-R5）。
bool EditorController::request_signature_help(std::optional<std::string> trigger, bool retrigger)
{
    const auto slot = requestable_slot(&ServerAbilities::signature_help);
    if (!slot.has_value())
    {
        return false;
    }
    signature_invoked_ = false;
    core::JsonObject document;
    document.emplace_back("uri", core::JsonValue{active_document_of(slot.value()).uri});
    core::JsonObject context;
    context.emplace_back("triggerKind",
                         core::JsonValue{core::json_integer(
                             trigger.has_value() ? character_trigger : invoked_trigger)});
    if (trigger.has_value())
    {
        context.emplace_back("triggerCharacter", core::JsonValue{std::move(trigger).value()});
    }
    context.emplace_back("isRetrigger", core::JsonValue{retrigger});
    core::JsonObject params;
    params.emplace_back("textDocument", core::JsonValue{std::move(document)});
    params.emplace_back("position", core::lsp_position_json(request_position(slot.value())));
    params.emplace_back("context", core::JsonValue{std::move(context)});
    send_language_request(slot.value(), LspRequestKind::signature_help,
                          core::JsonValue{std::move(params)}, request_context());
    return true;
}

// 届いた答え（文脈の照合は receive_answer）。照合で捨てた答えは今の札を残す（次の打鍵の答えが
// 来る）。空の答えは札を消し、手で頼んだときだけ 1 行知らせる。出せない状態（面・入力行・hover
// の札など）の答えは札にしない。
void EditorController::settle_signature_answer()
{
    auto answers = state_.language_answers();
    if (!answers.signature.has_value())
    {
        return;
    }
    LanguageSignature answered = std::move(answers.signature).value();
    answers.signature.reset();
    state_ = state_.with_language_answers(std::move(answers));
    const bool invoked = std::exchange(signature_invoked_, false);
    if (!answered.signature.has_value())
    {
        signature_tip_.reset();
        if (invoked)
        {
            state_ =
                state_.with_command_message(ui_notice(core::UiMessage::notice_signature_not_found));
        }
        return;
    }
    if (!signature_allowed())
    {
        return;
    }
    signature_tip_ = SignatureTip{.document = answered.context.document,
                                  .line = state_.text().line_containing(answered.context.at),
                                  .signature = std::move(answered.signature).value()};
    repaint_ = true;
}

// 札を出してよい状態（決定 6）。挿入中（通常モードはいつも・Vim は INSERT）で、入力行・右クリックの
// メニュー・設定の面が開いておらず、窓がアクティブで、hover と診断の札が出ていない。
bool EditorController::signature_allowed() const
{
    const bool inserting =
        state_.mode() == core::EditMode::ordinary || state_.vim().mode == core::VimMode::insert;
    return inserting && !state_.command_input().has_value() && !context_menu_active() &&
           !settings_open() && !window_inactive_ && !state_.language_answers().hover.has_value();
}

// 意図の出口で札を消す 1 か所（決定 6）。文書が替わった・キャレットが別の行へ移った・挿入を抜けた・
// 出せない状態になった。札が無ければ欄を 1 つ読むだけ。
void EditorController::settle_signature_tip()
{
    if (!signature_tip_.has_value())
    {
        return;
    }
    const SignatureTip &tip = signature_tip_.value();
    const AutosaveId active = state_.autosave_ids().at(state_.active_tab());
    // 言語サーバーが止まった・`lsp` を切ったときも消す（c-L3）。
    if (tip.document != active || !signature_allowed() ||
        active_language_server().phase != LanguageServerPhase::running ||
        state_.text().line_containing(state_.selection().caret) != tip.line)
    {
        signature_tip_.reset();
    }
}

// 操作「引数の情報を表示」（鍵なし・F1 の一覧から）。
bool EditorController::invoke_signature_help()
{
    signature_invoked_ = request_signature_help(std::nullopt, signature_tip_.has_value());
    return signature_invoked_;
}

// frame に載せる札（決定 6）。キャレットの行が見えている行に無い（ホイールで送った）間は隠す
// （消さない・戻れば出る・#1968 のレビュー c-R2'）。札が無ければ欄を 1 つ読むだけ。
std::optional<core::LspSignature>
EditorController::shown_signature(const std::vector<LineView> &lines) const
{
    if (!signature_tip_.has_value())
    {
        return std::nullopt;
    }
    const core::LineNumber line = signature_tip_.value().line;
    const bool visible =
        std::ranges::any_of(lines, [line](const LineView &view) { return view.number == line; });
    return visible ? std::optional{signature_tip_.value().signature} : std::nullopt;
}

void EditorController::drop_signature_tip() noexcept
{
    signature_tip_.reset();
}
} // namespace nenenib::application
