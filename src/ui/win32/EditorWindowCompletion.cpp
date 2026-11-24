// 補完の面の窓の側（ADR 0190 の決定 5）。面が見えている間の上下・PageUp / PageDown・Enter・Tab・
// Esc だけを面へ送り、ほかの鍵と文字は今までどおり本文へ通す（Ctrl+P の面とは違う振り分け）。面の
// 行のクリックはその候補の確定、面の上のホイールは面の行の送りで、どちらも本文へ渡さない（#1844 の
// R1）。何を出すか・何で閉じるか・確定で何を入れるかは controller が決め、窓は意図を送るだけ
// （ARC-011）。当たりは renderer が最後に描いた配置（core::completion_face_layout の 1 本）を引く。
#include "EditorWindow.hpp"
#include "EditorWindowDetail.hpp"

#include "ChooseCompletion.hpp"
#include "CloseCompletion.hpp"
#include "CompletionCommitKey.hpp"
#include "CompletionFace.hpp"
#include "CompletionPointer.hpp"
#include "CompletionStep.hpp"
#include "OpenCompletion.hpp"
#include "ScrollCompletion.hpp"
#include "StepCompletion.hpp"
#include "WheelTurn.hpp"

#include <windows.h>

#include <cstdint>
#include <optional>
#include <utility>

namespace nenenib::ui::win32
{
using detail::held;
using detail::high_word_of;
using detail::low_word_of;

// 操作「補完」（通常モードの Ctrl+Space と F1 の一覧）。開けるかは controller が決める。
void EditorWindow::run_complete()
{
    send(application::OpenCompletion{});
}

// 面が見えている間の鍵を面が受けたか。修飾つきの鍵（Shift+矢印の選択・Ctrl の操作）は受けずに
// 今までの道へ流し、controller がその意図の前に面を閉じる。Enter・Tab・Esc の後の WM_CHAR は
// 制御文字なので type_character が捨てる。Vim のモードの面は engine が持つので、ここでは受けず、
// どの鍵も Vim の鍵の道へ流す（Esc は確定して INSERT を抜ける・Enter は確定して改行・#1845）。
bool EditorWindow::take_completion_key(UINT message, WPARAM word)
{
    if (message != WM_KEYDOWN || mode_ == core::EditMode::vim || !controller_.completion_active() ||
        held(VK_CONTROL) || held(VK_MENU) || held(VK_SHIFT))
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
        return commit_completion(application::CompletionCommitKey::enter);
    case VK_TAB:
        return commit_completion(application::CompletionCommitKey::tab);
    case VK_ESCAPE:
        send(application::CloseCompletion{});
        return true;
    default:
        return false;
    }
}

// Enter と Tab。確定が本文を変えるなら確定して鍵を面が受ける。変えない（打った語と同じ綴りだけが
// 残っている・自動で開いてまだ選んでいない面の Enter・#1971）なら面を閉じ、鍵は本文へ通す（改行・
// Tab が 1 回目で入る・#1844 の L1）。
bool EditorWindow::commit_completion(application::CompletionCommitKey key)
{
    if (!controller_.completion_commits(key))
    {
        send(application::CloseCompletion{});
        return false;
    }
    send(application::ChooseCompletion{});
    return true;
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

// 面の上の左の押しは覚えるだけ（離したときに同じ行なら確定する）。面の上の押しは本文へ渡さない。
// 面の外の押しは受けず、今までどおり本文へ渡る（controller がその意図の前に面を閉じる）。
bool EditorWindow::press_completion(LPARAM data)
{
    completion_pressed_.reset();
    const POINT point{low_word_of(data), high_word_of(data)};
    if (!controller_.completion_active() || renderer_ == nullptr ||
        !renderer_->completion_covers(point.x, point.y))
    {
        return false;
    }
    completion_pressed_ = renderer_->completion_row_at(point.x, point.y);
    completion_click_began_ = true;
    return true;
}

// 窓の外へ出た・捕まえが変わったときに、面の上で押した行の覚えを下ろす（次の離しを呑まない・
// #1844 の L1b）。面が閉じた後の離しは release_completion が覚えを消すだけで確定しない。
void EditorWindow::lose_completion_press(UINT message)
{
    if (completion_press_lost(message))
    {
        completion_pressed_.reset();
    }
}

// 面の上の右ボタン・中ボタンの押しは面を閉じるだけ（本文へ渡さない・#1844 の L3b）。受けたら真。
bool EditorWindow::close_completion_under(LPARAM data)
{
    const POINT point{low_word_of(data), high_word_of(data)};
    if (!completion_takes_button(controller_.completion_active(),
                                 renderer_ != nullptr &&
                                     renderer_->completion_covers(point.x, point.y)))
    {
        return false;
    }
    send(application::CloseCompletion{});
    return true;
}

// 押した行と離した行が同じときだけ確定する（ADR 0056 の決定 9 と同じ考え）。面の上で押した離しは
// 本文へ渡さない。
bool EditorWindow::release_completion(LPARAM data)
{
    const auto pressed = std::exchange(completion_pressed_, std::nullopt);
    if (!pressed.has_value())
    {
        return false;
    }
    const POINT point{low_word_of(data), high_word_of(data)};
    if (controller_.completion_active() && renderer_ != nullptr &&
        renderer_->completion_row_at(point.x, point.y) == pressed)
    {
        send(application::ChooseCompletion{pressed});
    }
    return true;
}

// 面の上のホイールは面の行だけを送る（本文を送らず、面を閉じない）。位置は画面の座標なので
// クライアントの座標に直してから見る。面の上でなければ偽で、残りを捨てる。
bool EditorWindow::scroll_completion(std::int32_t delta, LPARAM data)
{
    POINT point{low_word_of(data), high_word_of(data)};
    ScreenToClient(window_, &point);
    if (!controller_.completion_active() || renderer_ == nullptr ||
        !renderer_->completion_covers(point.x, point.y))
    {
        completion_wheel_remainder_ = 0;
        return false;
    }
    // 1 刻みの行数は OS の設定から。1 画面の設定なら面の行数。
    const auto turn =
        wheel_turned(completion_wheel_remainder_, delta,
                     wheel_lines_per_notch(wheel_setting_, core::completion_face_rows));
    completion_wheel_remainder_ = turn.remainder;
    if (turn.lines != 0)
    {
        send(application::ScrollCompletion{turn.lines});
    }
    return true;
}
} // namespace nenenib::ui::win32
