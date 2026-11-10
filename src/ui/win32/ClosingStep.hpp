#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
// 窓が閉じていく道の 1 段（ADR 0133 の追補 #1340）。順は closing_steps が決め、ui の
// EditorWindow::close_through が 1 段ずつ写す。値が増えたら写す側の `switch` が落ちる（CPP-002）。
enum class ClosingStep : std::uint8_t
{
    // IME の変換中の文字列を取り消す。隠すと届く WM_KILLFOCUS で IME が確定して本文に入ると、閉じる
    // 確認を通り抜けた後に未保存が生まれるので、隠す前に捨てる（今日も壊すときに消えていた）。
    cancel_composition,
    // 窓を画面とタスクバーから外す（ShowWindow(SW_HIDE)・覆いの子窓も一緒に消える）。
    hide_window,
    // 一覧・履歴・自動退避の片付けを書く意図 EndSession を送る（ADR 0059 / 0060 / 0137）。
    remember_session,
    // 立てた OS の終了を止める理由を外す（ADR 0133 の決定 4）。
    release_session_block,
    // 本窓を壊す（ADR 0133 の決定 1）。
    destroy_window
};
} // namespace nenenib::ui::win32
