#pragma once

#include "CaptionPress.hpp"
#include "TitleBarHit.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace nenenib::ui::win32
{
// WM_NCHITTEST の答え（#1498）。最大化していない窓では縁（辺ごとの物理画素・resize_borders）が
// 帯の要素より先に勝つ。右上の角・× □ ＿ の上端・× の右端でも大きさを変えられる（Windows
// の標準・D82。標準の
// 窓は右と角の伸縮の帯を見えない外側に持つ）。最大化した窓とモニターの端に接した辺には縁が無いので、
// そこでは隅の画素まで帯の要素（× など）を返す。帯の要素 hit は呼び手が今の title_bar()
// から引いた値で、この 1 本は 配置を作らない（マウスの動くたびに通る道）。

// 縁は 3 行 3 列の表で引く。分岐で書くと CPP-012 の認知的複雑度を越える。
inline constexpr std::array<LRESULT, 9> border_codes{HTNOWHERE, HTLEFT,       HTRIGHT,
                                                     HTTOP,     HTTOPLEFT,    HTTOPRIGHT,
                                                     HTBOTTOM,  HTBOTTOMLEFT, HTBOTTOMRIGHT};

// margins は辺ごとの縁の幅（物理画素・left / top / right / bottom の 4 辺）。0 の辺は当たらない。
[[nodiscard]] constexpr LRESULT border_hit(POINT point, const RECT &client,
                                           const RECT &margins) noexcept
{
    const std::size_t row =
        point.y < margins.top ? 1U : (point.y >= client.bottom - margins.bottom ? 2U : 0U);
    const std::size_t column =
        point.x < margins.left ? 1U : (point.x >= client.right - margins.right ? 2U : 0U);
    return border_codes.at(row * 3U + column);
}

// 辺ごとの縁の幅（物理画素）。規則はこの 1 本（#1498）。最大化した窓は 4 辺とも 0。最大化して
// いなくても、モニターの端（rcMonitor・作業領域ではなく物理の端）に接している辺は 0 にする。
// Snap した窓で画面の右上の角や右端へマウスを投げると × に当たる（標準の窓は伸縮の帯が見えない
// 外側の枠にあって画面の外へ出るので、同じ結果になる・D82）。Snap の窓は見えない枠の分だけ
// モニターの外へはみ出すことがあるので、接するかは等号つきの不等号で見る。複数のモニターの間の
// 辺もモニターの端として扱う（その辺では隣のモニターへ出る手前で大きさを変えられない）。
// モニターを取れなかったとき（nullopt）は「どの辺も接していない」として margin のまま。
[[nodiscard]] constexpr RECT resize_borders(std::int32_t margin, bool zoomed, const RECT &window,
                                            const std::optional<RECT> &monitor) noexcept
{
    if (zoomed)
    {
        return RECT{0, 0, 0, 0};
    }
    if (!monitor.has_value())
    {
        return RECT{margin, margin, margin, margin};
    }
    const RECT &edge = monitor.value();
    return RECT{window.left <= edge.left ? 0 : margin, window.top <= edge.top ? 0 : margin,
                window.right >= edge.right ? 0 : margin, window.bottom >= edge.bottom ? 0 : margin};
}

// 窓の操作（＿ □ ×・46 × 40 DIP）の上の縁は半分（8 DIP の縁なら 4 DIP）。Nib の窓は client が窓
// 全体で、標準の窓のように見えない外側の伸縮の帯を持たない。窓の操作の上でも 8 DIP にすると
// × の高さの 2 割が押せなくなるので、角・上端・右端で大きさを変えられることを保ったまま、ボタン
// の削られる幅を半分にする（#1498）。ほかの帯の要素（タブ・caption）と本文の上は borders のまま。
[[nodiscard]] constexpr RECT border_over(core::TitleBarHit hit, const RECT &borders) noexcept
{
    if (!window_button(hit))
    {
        return borders;
    }
    return RECT{borders.left / 2, borders.top / 2, borders.right / 2, borders.bottom / 2};
}

[[nodiscard]] constexpr LRESULT window_hit(POINT point, const RECT &client, const RECT &borders,
                                           core::TitleBarHit hit) noexcept
{
    const LRESULT edge = border_hit(point, client, border_over(hit, borders));
    return edge != HTNOWHERE ? edge : caption_code(hit);
}
} // namespace nenenib::ui::win32
