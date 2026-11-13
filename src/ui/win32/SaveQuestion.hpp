#pragma once

#include "SaveAnswer.hpp"
#include "SaveQuestionText.hpp"

#include <windows.h>

#include <array>
#include <span>
#include <utility>

namespace nenenib::ui::win32
{
// 「保存しますか」の 2 つの形（ADR 0133 の決定 2・3）。箱は TaskDialogIndirect の custom button
// で出し、ボタンの文言も core の save_question_text から引く（ADR 0133 の追補 #1282）。
enum class SaveQuestion
{
    // 保存する／保存しない／キャンセル（Esc と × はキャンセル）。
    unsaved,
    // 保存する／保存しない（取消は無い。Esc と × は効かない・MB_YESNO と同じ）。
    rendering_lost,
};

// 箱のボタン 1 つ（id と文の名前）。行の型を足さず std::pair の別名で書く（CPP-011）。
using SaveButton = std::pair<int, core::SaveQuestionText>;

inline constexpr std::array<SaveButton, 3> unsaved_buttons{{
    {IDYES, core::SaveQuestionText::save_button},
    {IDNO, core::SaveQuestionText::discard_button},
    {IDCANCEL, core::SaveQuestionText::cancel_button},
}};

inline constexpr std::array<SaveButton, 2> rendering_lost_buttons{{
    {IDYES, core::SaveQuestionText::save_button},
    {IDNO, core::SaveQuestionText::discard_button},
}};

// 並びの順に左から出し、先頭が既定のボタン（MB_DEFBUTTON1 と同じ）。
[[nodiscard]] constexpr std::span<const SaveButton> save_buttons(SaveQuestion question) noexcept
{
    switch (question)
    {
    case SaveQuestion::unsaved:
        return unsaved_buttons;
    case SaveQuestion::rendering_lost:
        return rendering_lost_buttons;
    }
    std::unreachable();
}

[[nodiscard]] constexpr core::SaveQuestionText save_message(SaveQuestion question) noexcept
{
    switch (question)
    {
    case SaveQuestion::unsaved:
        return core::SaveQuestionText::unsaved_question;
    case SaveQuestion::rendering_lost:
        return core::SaveQuestionText::rendering_lost_question;
    }
    std::unreachable();
}

// 箱を出して答えを返す。入れ子のメッセージループを回す（MessageBoxW と同じ）。
// 文は language の文（#1019・ui は controller の display_language から引く）。
[[nodiscard]] SaveAnswer ask_save(HWND owner, const wchar_t *title, SaveQuestion question,
                                  core::DisplayLanguage language);
} // namespace nenenib::ui::win32
