// Windows のエクスプローラーへの登録の 2 つの操作（ADR 0178 の決定 3・4）。ShellRegistrationPort を
// 呼ぶのはこの翻訳単位だけで、起動・開く・切り替え・保存・打鍵・自動退避・F1 の一覧の道は呼ばない。
#include "EditorController.hpp"

#include "DisplayText.hpp"
#include "ShellRegistration.hpp"
#include "ShellRegistrationFailure.hpp"
#include "ShellRegistrationInput.hpp"
#include "ShellRegistrationPlan.hpp"
#include "ShellRegistrationPlanFailure.hpp"
#include "ShellRegistrationStatus.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::application
{
namespace
{
// 右クリックの項目の文と、「プログラムから開く」「既定のアプリ」に出る名前。表示言語の表（ADR
// 0123・#1019）が入るまではここのリテラル（ADR 0178 の決定 2）。
constexpr std::string_view verb_name = "NeNe Nib で開く";
constexpr std::string_view application_name = "NeNe Nib";
// 知らせの文（決定 3・4・5）。鳴らさない情報の 1 行。
constexpr std::string_view windows_11_note = "（Windows 11 は「その他のオプションを表示」の中）";
constexpr std::string_view registered_notice = "Windows の右クリックに登録しました";
constexpr std::string_view replaced_notice = "以前の登録を置き換えました";
constexpr std::string_view unregistered_notice = "登録を外しました";
constexpr std::string_view absent_notice = "登録されていません";
constexpr std::string_view unwritten_prefix = "登録できませんでした: ";
constexpr std::string_view unremoved_prefix = "登録を外せませんでした: ";

[[nodiscard]] std::string_view failure_text(ShellRegistrationFailure failure) noexcept
{
    switch (failure)
    {
    case ShellRegistrationFailure::access_denied:
        return "アクセスが拒否されました";
    case ShellRegistrationFailure::io_error:
        return "レジストリを読み書きできませんでした";
    }
    std::unreachable();
}

[[nodiscard]] std::string_view
plan_failure_text(core::ShellRegistrationPlanFailure failure) noexcept
{
    switch (failure)
    {
    case core::ShellRegistrationPlanFailure::empty_path:
        return "exe のパスがありません";
    case core::ShellRegistrationPlanFailure::quote_in_path:
        return "exe のパスに \" があります";
    }
    std::unreachable();
}

[[nodiscard]] std::string joined(std::string_view head, std::string_view tail)
{
    std::string text(head);
    text += tail;
    return text;
}

// 計画を作れなかった理由の 1 行（exe のパスを読めない・パスが計画に使えない）。
using PlanOutcome = std::expected<core::ShellRegistrationPlan, std::string>;

[[nodiscard]] PlanOutcome planned(ShellRegistrationPort &port, std::string_view prefix)
{
    const auto path = port.executable_path();
    if (!path.has_value())
    {
        return std::unexpected(joined(prefix, failure_text(path.error())));
    }
    auto plan = core::shell_registration_plan(core::ShellRegistrationInput{
        path.value(), std::string(verb_name), std::string(application_name),
        core::shell_registration_extensions()});
    if (!plan.has_value())
    {
        return std::unexpected(joined(prefix, plan_failure_text(plan.error())));
    }
    return std::move(plan).value();
}
} // namespace

void EditorController::accept(const RegisterShell &)
{
    auto &port = ports_.shell_registration;
    const auto plan = planned(port, unwritten_prefix);
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
        show_shell_notice(joined(unwritten_prefix, failure_text(applied.error())));
        return;
    }
    show_shell_notice(joined(elsewhere ? replaced_notice : registered_notice, windows_11_note));
}

void EditorController::accept(const UnregisterShell &)
{
    auto &port = ports_.shell_registration;
    const auto plan = planned(port, unremoved_prefix);
    if (!plan.has_value())
    {
        show_shell_notice(plan.error());
        return;
    }
    // 登録が無ければ書かずに知らせる（決定 3）。別の場所の exe の登録は同じ鍵なので外す。
    if (port.status(plan.value()) == ShellRegistrationStatus::absent)
    {
        show_shell_notice(std::string(absent_notice));
        return;
    }
    const auto removed = port.remove(plan.value());
    show_shell_notice(removed.has_value()
                          ? std::string(unregistered_notice)
                          : joined(unremoved_prefix, failure_text(removed.error())));
}

void EditorController::show_shell_notice(const std::string &text)
{
    // 材料は固定の文字と OS が返さない種類の文だけなので parse
    // は必ず成功する（不変条件・ARC-010）。
    state_ = state_.with_command_message(core::DisplayText::parse(text).value());
}
} // namespace nenenib::application
