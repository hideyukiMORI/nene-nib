#pragma once

#include "CaptionPress.hpp"
#include "TitleBarHit.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace nenenib::ui::win32
{
// WM_NCHITTEST の答え（#1498）。最大化していない窓では縁（margin 物理画素）が帯の要素より先に
// 勝つ。右上の角・× □ ＿ の上端・× の右端でも大きさを変えられる（Windows の標準・D82。標準の
// 窓は右と角の伸縮の帯を見えない外側に持つ）。最大化した窓には縁が無いので、隅の画素まで帯の要素
// （× など）を返す。帯の要素 hit は呼び手が今の title_bar() から引いた値で、この 1 本は
// 配置を作らない（マウスの動くたびに通る道）。

// 縁は 3 行 3 列の表で引く。分岐で書くと CPP-012 の認知的複雑度を越える。
inline constexpr std::array<LRESULT, 9> border_codes{HTNOWHERE, HTLEFT,       HTRIGHT,
                                                     HTTOP,     HTTOPLEFT,    HTTOPRIGHT,
                                                     HTBOTTOM,  HTBOTTOMLEFT, HTBOTTOMRIGHT};

[[nodiscard]] constexpr LRESULT border_hit(POINT point, const RECT &client,
                                           std::int32_t margin) noexcept
{
    const std::size_t row = point.y < margin ? 1U : (point.y >= client.bottom - margin ? 2U : 0U);
    const std::size_t column = point.x < margin ? 1U : (point.x >= client.right - margin ? 2U : 0U);
    return border_codes.at(row * 3U + column);
}

// 縁の幅（物理画素）。最大化した窓には縁が無い（0 なら border_hit は client の中で当たらない）。
[[nodiscard]] constexpr std::int32_t resize_border_width(std::int32_t margin, bool zoomed) noexcept
{
    return zoomed ? 0 : margin;
}

// 窓の操作（＿ □ ×・46 × 40 DIP）の上の縁は半分（8 DIP の縁なら 4 DIP）。Nib の窓は client が窓
// 全体で、標準の窓のように見えない外側の伸縮の帯を持たない。窓の操作の上でも 8 DIP にすると
// × の高さの 2 割が押せなくなるので、角・上端・右端で大きさを変えられることを保ったまま、ボタン
// の削られる幅を半分にする（#1498）。ほかの帯の要素（タブ・caption）と本文の上は border のまま。
[[nodiscard]] constexpr std::int32_t border_over(core::TitleBarHit hit,
                                                 std::int32_t border) noexcept
{
    return window_button(hit) ? border / 2 : border;
}

[[nodiscard]] constexpr LRESULT window_hit(POINT point, const RECT &client, std::int32_t border,
                                           core::TitleBarHit hit) noexcept
{
    const LRESULT edge = border_hit(point, client, border_over(hit, border));
    return edge != HTNOWHERE ? edge : caption_code(hit);
}
} // namespace nenenib::ui::win32
