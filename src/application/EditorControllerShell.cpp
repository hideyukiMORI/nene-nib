// Windows のエクスプローラーへの登録の 2 つの操作（ADR 0178 の決定 3・4）。ShellRegistrationPort を
// 呼ぶのはこの翻訳単位だけで、起動・開く・切り替え・保存・打鍵・自動退避・F1 の一覧の道は呼ばない。
#include "EditorController.hpp"

#include "AppIdentity.hpp"
#include "DisplayText.hpp"
#include "ShellRegistration.hpp"
#include "ShellRegistrationFailure.hpp"
#include "ShellRegistrationInput.hpp"
#include "ShellRegistrationPlan.hpp"
#include "ShellRegistrationPlanFailure.hpp"
#include "ShellRegistrationStatus.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::application
{
namespace
{
// 「プログラムから開く」「既定のアプリ」に出る名前（製品名は訳さない・ADR 0178 の決定 2）。右
// クリックの項目の文と知らせ（決定 3・4・5）は表示言語の文の表（ADR 0123 の追補 2026-11-13）。
constexpr std::string_view application_name = "NeNe Nib";

[[nodiscard]] core::UiMessage failure_message(ShellRegistrationFailure failure) noexcept
{
    switch (failure)
    {
    case ShellRegistrationFailure::access_denied:
        return core::UiMessage::notice_shell_access_denied;
    case ShellRegistrationFailure::io_error:
        return core::UiMessage::notice_shell_registry_failed;
    }
    std::unreachable();
}

[[nodiscard]] core::UiMessage
plan_failure_message(core::ShellRegistrationPlanFailure failure) noexcept
{
    switch (failure)
    {
    case core::ShellRegistrationPlanFailure::empty_path:
        return core::UiMessage::notice_shell_empty_path;
    case core::ShellRegistrationPlanFailure::quote_in_path:
        return core::UiMessage::notice_shell_quote_in_path;
    }
    std::unreachable();
}

// 「登録できませんでした: {0}」の置き場に理由の文を入れた 1 行。
[[nodiscard]] std::string failed_notice(core::UiMessage head, core::UiMessage reason,
                                        core::DisplayLanguage language)
{
    return core::ui_text_filled(head, language, {core::ui_text(reason, language)});
}

// 計画を作れなかった理由の 1 行（exe のパスを読めない・パスが計画に使えない）。
using PlanOutcome = std::expected<core::ShellRegistrationPlan, std::string>;

// 登録の表示名（右クリックの項目・ProgId の説明）は登録の時点の表示言語（追補の決定 11）。
[[nodiscard]] PlanOutcome planned(ShellRegistrationPort &port, core::UiMessage head,
                                  core::DisplayLanguage language)
{
    const auto path = port.executable_path();
    if (!path.has_value())
    {
        return std::unexpected(failed_notice(head, failure_message(path.error()), language));
    }
    auto plan = core::shell_registration_plan(
        core::ShellRegistrationInput{
            path.value(), std::string(core::ui_text(core::UiMessage::shell_verb_open, language)),
            std::string(application_name), core::shell_registration_extensions()},
        language);
    if (!plan.has_value())
    {
        return std::unexpected(failed_notice(head, plan_failure_message(plan.error()), language));
    }
    return std::move(plan).value();
}
} // namespace

void EditorController::accept(const RegisterShell &)
{
    auto &port = ports_.shell_registration;
    const auto language = state_.display_language();
    const auto plan = planned(port, core::UiMessage::notice_shell_register_failed, language);
    if (!plan.has_value())
    {
        show_shell_notice(plan.error());
        return;
    }
    // 古い登録（exe を動かした後）は起動では直さず、ここで上書きして知らせる（決定 4）。
    const bool elsewhere =
        port.status(plan.value()) == ShellRegistrationStatus::registered_elsewhere;
    const auto applied = port.apply(plan.value());
    if (!applied.has_value())
    {
        show_shell_notice(failed_notice(core::UiMessage::notice_shell_register_failed,
                                        failure_message(applied.error()), language));
        return;
    }
    show_shell_notice(
        std::string(core::ui_text(elsewhere ? core::UiMessage::notice_shell_replaced
                                            : core::UiMessage::notice_shell_registered,
                                  language)));
}

void EditorController::accept(const UnregisterShell &)
{
    auto &port = ports_.shell_registration;
    const auto language = state_.display_language();
    const auto plan = planned(port, core::UiMessage::notice_shell_unregister_failed, language);
    if (!plan.has_value())
    {
        show_shell_notice(plan.error());
        return;
    }
    // 登録が無ければ書かずに知らせる（決定 3）。別の場所の exe の登録は同じ鍵なので外す。
    if (port.status(plan.value()) == ShellRegistrationStatus::absent)
    {
        show_shell_notice(
            std::string(core::ui_text(core::UiMessage::notice_shell_absent, language)));
        return;
    }
    const auto removed = port.remove(plan.value());
    show_shell_notice(
        removed.has_value()
            ? std::string(core::ui_text(core::UiMessage::notice_shell_unregistered, language))
            : failed_notice(core::UiMessage::notice_shell_unregister_failed,
                            failure_message(removed.error()), language));
}

void EditorController::accept(const ShowVersion &)
{
    // 名乗りの文は固定の ASCII と「©」「·」だけなので parse は必ず成功する（不変条件・ARC-010）。
    state_ = state_.with_command_message(core::DisplayText::parse(core::app_about_line()).value());
}

void EditorController::show_shell_notice(const std::string &text)
{
    // 材料は固定の文字と OS が返さない種類の文だけなので parse
    // は必ず成功する（不変条件・ARC-010）。
    state_ = state_.with_command_message(core::DisplayText::parse(text).value());
}
} // namespace nenenib::application
