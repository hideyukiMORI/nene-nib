#pragma once

#include <windows.h>

namespace nenenib::ui::win32
{
// 「保存しますか」の答え（ADR 0133 の追補 #1282）。保存する → 保存の道、保存しない → 閉じる、
// キャンセル → 何もしない。箱を出せなかったときもキャンセル（閉じない側）に倒す。
enum class SaveAnswer
{
    save,
    discard,
    cancel,
};

// 押されたボタンの id → 答え。ボタンの id は MessageBox と同じ IDYES / IDNO / IDCANCEL にして
// あり、道具（eng/window_driver.py の dismiss_dialog）が WM_COMMAND で答える口も変わらない。
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
