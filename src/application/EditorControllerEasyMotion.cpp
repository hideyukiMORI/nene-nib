// イージーモーションの application の分（ADR 0163 の決定 4〜6 と 2026-11-17 の追補・#1779）。`\\f`
// `\\F` `\\s` の 1 文字の入力行（JumpCharacter）を開く・閉じる・確定した文字を engine へ流す所と、
// 印を出している間に本文・スクロール・窓の大きさ・タブ・モードが変わったら印を捨てる所は、この
// 翻訳単位の意図の出口（settle_easymotion）の 1 か所。印の写しは EasyMotionLabels.cpp。
#include "EditorController.hpp"
#include "EditorControllerDetail.hpp"

#include "CommandInput.hpp"
#include "EasyMotionKind.hpp"
#include "HoverExplorer.hpp"
#include "JumpCharacter.hpp"
#include "Offset.hpp"
#include "PointTitleBar.hpp"
#include "RefreshAppearance.hpp"
#include "Utf8.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"

#include <optional>
#include <string_view>
#include <tuple>
#include <variant>

namespace nenenib::application
{
namespace
{
// engine が跳び先の文字を待っている動作（input_wait が EasyMotionKind で印が無い・決定 4）。
[[nodiscard]] std::optional<core::EasyMotionKind> awaited_jump(const core::VimState &vim) noexcept
{
    if (!vim.input_wait.has_value() || vim.easy_motion.has_value())
    {
        return std::nullopt;
    }
    const auto *const kind = std::get_if<core::EasyMotionKind>(&vim.input_wait.value());
    if (kind == nullptr)
    {
        return std::nullopt;
    }
    return *kind;
}

// 印を保つ意図。印の鍵そのもの（Vim の鍵）と、本文も画面も動かさない合図・帯とエクスプローラーの
// hover・外観の写し。ほかの意図（スクロール・クリック・窓の大きさ・タブ・モード・開く…）は印を
// 捨てる。
[[nodiscard]] bool keeps_labels(const EditorIntent &intent) noexcept
{
    return std::holds_alternative<VimKeyPress>(intent) || detail::is_signal(intent) ||
           std::holds_alternative<PointTitleBar>(intent) ||
           std::holds_alternative<HoverExplorer>(intent) ||
           std::holds_alternative<RefreshAppearance>(intent);
}
} // namespace

// 1 文字の入力行が開いていれば、確定した文字列の最初の 1 文字（ADR 0053 の 1 文字の頭の code
// point・結合文字は跳び先の照合で手前の文字に含まれる）を跳び先の文字として engine へ流し、真を
// 返す（入力行へは差し込まない）。行は流す前に閉じる（engine が印を出すか跳ぶ）。
bool EditorController::answered_jump(std::string_view utf8)
{
    const auto &input = state_.command_input();
    if (!input.has_value() || !std::holds_alternative<JumpCharacter>(input.value()))
    {
        return false;
    }
    if (utf8.empty())
    {
        return true;
    }
    const char32_t code = core::code_point_at(utf8, core::Offset{0});
    close_command_input();
    jump_line_open_ = false;
    static_cast<void>(step_vim(core::VimKey{core::VimCharacter{code}}));
    return true;
}

// 意図の出口の 1 か所（settle_transient の後）。
void EditorController::settle_easymotion(const EditorIntent &intent)
{
    settle_jump_line();
    settle_labels(intent);
}

// engine が文字を待つのに行が無ければ開き、開いた行が取り消された（Esc・空の行の Backspace・Enter）
// なら待ちを捨てて INSERT の Ctrl-o を始め直し、ほかの入力行に置き換わった（Ctrl+P…）なら割り込み
// として捨てる。待ちが無くなった
// のに行が残っていれば閉じる。
void EditorController::settle_jump_line()
{
    const auto waiting = awaited_jump(state_.vim());
    const auto &input = state_.command_input();
    const bool shown = input.has_value() && std::holds_alternative<JumpCharacter>(input.value());
    if (!waiting.has_value())
    {
        if (shown)
        {
            close_command_input();
        }
        jump_line_open_ = false;
        return;
    }
    if (shown)
    {
        return;
    }
    if (input.has_value())
    {
        jump_line_open_ = false;
        state_ = state_.with_vim(core::vim_interrupted(state_.vim()));
        return;
    }
    if (jump_line_open_)
    {
        // 取消（Esc・Enter・空の行の Backspace）は `:` `/` の行の取消と同じ形: 待ちを捨てて INSERT
        // の `Ctrl-o` の印は残し、INSERT を始め直す（ADR 0182 の決定 8・#1782 の追い）。
        jump_line_open_ = false;
        state_ = state_.with_vim(core::vim_operator_cleared(state_.vim()));
        resume_vim_insert();
        return;
    }
    state_ = state_.with_command_input(CommandInput{JumpCharacter{waiting.value()}});
    jump_line_open_ = true;
}

// 印を出している間に本文（改訂番号）・スクロールと窓の大きさ・タブ・モードが変わったか、印を保た
// ない意図が来たら、印を割り込みとして捨てる（V-1777 の申し送り）。合図は本文も画面も変えない
// 限り保つ。
void EditorController::settle_labels(const EditorIntent &intent)
{
    if (!state_.vim().easy_motion.has_value())
    {
        labels_ground_.reset();
        return;
    }
    const LabelsGround ground{state_.history().revision(), state_.scroll(), state_.active_tab(),
                              state_.mode()};
    if (labels_ground_.has_value() && (!keeps_labels(intent) || labels_ground_.value() != ground))
    {
        labels_ground_.reset();
        state_ = state_.with_vim(core::vim_interrupted(state_.vim()));
        return;
    }
    labels_ground_ = ground;
}
} // namespace nenenib::application
