// 補完の面の窓の側（ADR 0190 の決定 5）。面が見えている間の上下・PageUp / PageDown・Enter・Tab・
// Esc だけを面へ送り、ほかの鍵と文字は今までどおり本文へ通す（Ctrl+P の面とは違う振り分け）。何を
// 出すか・何で閉じるか・確定で何を入れるかは controller が決め、窓は意図を送るだけ（ARC-011）。
#include "EditorWindow.hpp"
#include "EditorWindowDetail.hpp"

#include "ChooseCompletion.hpp"
#include "CloseCompletion.hpp"
#include "CompletionStep.hpp"
#include "OpenCompletion.hpp"
#include "StepCompletion.hpp"

#include <windows.h>

namespace nenenib::ui::win32
{
using detail::held;

// 操作「補完」（通常モードの Ctrl+Space と F1 の一覧）。開けるかは controller が決める。
void EditorWindow::run_complete()
{
    send(application::OpenCompletion{});
}

// 面が見えている間の鍵を面が受けたか。修飾つきの鍵（Shift+矢印の選択・Ctrl の操作）は受けずに
// 今までの道へ流し、controller がその意図の前に面を閉じる。Enter・Tab・Esc の後の WM_CHAR は
// 制御文字なので type_character が捨てる。
bool EditorWindow::take_completion_key(UINT message, WPARAM word)
{
    if (message != WM_KEYDOWN || !controller_.completion_active() || held(VK_CONTROL) ||
        held(VK_MENU) || held(VK_SHIFT))
    {
        return false;
    }
    // OS の仮想キーは開いた集合（CPP-017）。
    switch (word)
    {
    case VK_UP:
        send(application::StepCompletion{application::CompletionStep::previous});
        return true;
    case VK_DOWN:
        send(application::StepCompletion{application::CompletionStep::next});
        return true;
    case VK_PRIOR:
        send(application::StepCompletion{application::CompletionStep::page_previous});
        return true;
    case VK_NEXT:
        send(application::StepCompletion{application::CompletionStep::page_next});
        return true;
    case VK_RETURN:
    case VK_TAB:
        send(application::ChooseCompletion{});
        return true;
    case VK_ESCAPE:
        send(application::CloseCompletion{});
        return true;
    default:
        return false;
    }
}

// 窓が非アクティブになったら面を閉じる（まだ見えていない、答えを待っている面も）。面が無ければ
// 意図を送らない。
void EditorWindow::close_completion_outside()
{
    if (controller_.completion_open())
    {
        send(application::CloseCompletion{});
    }
}
} // namespace nenenib::ui::win32
