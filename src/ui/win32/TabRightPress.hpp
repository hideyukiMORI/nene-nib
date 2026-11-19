#pragma once

#include "TitleBarTarget.hpp"

#include <optional>

namespace nenenib::ui::win32
{
// 右の押しの後に覚える帯のタブ（ADR 0078 の追補 #1790・レビューの推奨 1）。右の押しは捕まえない
// ので、タブの上で押して窓の外で離した押しは残り得る。設定の面・補完の面・開いているメニューが
// 押しを取ったら覚えを消し（古い押しと次の離しを同じタブと読ませない）、それ以外は押した点の下の
// タブを覚える。窓の press_right は押しのたびにこの 1 本で tab_right_pressed_ を書き直す。
[[nodiscard]] constexpr std::optional<core::TitleBarTarget>
tab_right_press_kept(bool taken, const std::optional<core::TitleBarTarget> &under) noexcept
{
    return taken ? std::nullopt : under;
}
} // namespace nenenib::ui::win32
