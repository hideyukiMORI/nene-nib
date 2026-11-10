#pragma once

#include <cstdint>

namespace nenenib::ui::win32
{
// 閉じる手順の道（ADR 0133 の追補 #1340）。window_closes は未保存の確認を通り抜けた後の leave
// （× と Alt+F4・最後のタブ・描画を失った終わり）、session_ends は OS の終了が確定した
// WM_ENDSESSION（窓は壊さない・ADR 0059 の決定 3）。
enum class ClosingRoute : std::uint8_t
{
    window_closes,
    session_ends
};
} // namespace nenenib::ui::win32
