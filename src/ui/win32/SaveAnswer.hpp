#pragma once

#include <windows.h>

namespace nenenib::ui::win32
{
// 「保存しますか」の答え（ADR 0133 の追補 #1282）。保存する → 保存の道、保存しない → 閉じる、
// キャンセル。箱を出せなかったときもキャンセルに倒し、行き先は save_step が形ごとに決める。
enum class SaveAnswer
{
    save,
    discard,
    cancel,
};

// 押されたボタンの id → 答え。ボタンの id は MessageBox と同じ IDYES / IDNO / IDCANCEL。道具
// （eng/window_driver.py の dismiss_dialog）は TaskDialog に TDM_CLICK_BUTTON で同じ id を送る
// （TaskDialog は posted の WM_COMMAND を受けない）。答えの後の一歩は SaveStep.hpp の save_step。
[[nodiscard]] constexpr SaveAnswer save_answer_of(int button) noexcept
{
    if (button == IDYES)
    {
        return SaveAnswer::save;
    }
    if (button == IDNO)
    {
        return SaveAnswer::discard;
    }
    return SaveAnswer::cancel;
}
} // namespace nenenib::ui::win32
