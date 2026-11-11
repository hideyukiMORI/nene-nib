#pragma once

#include "TitleBarHit.hpp"
#include "TitleBarLayout.hpp"
#include "TitleBarTarget.hpp"

#include <windows.h>

#include <optional>
#include <utility>

namespace nenenib::ui::win32
{
// 窓の操作（＿ □ ×）の押しと離し（ADR 0056 の決定 9 の追補・#1464）。押した要素を帯と同じ
// left_pressed_ に覚え、離した要素と同じときだけ動かす（core::title_bar_released）。押した後は
// 窓が捕まえる（SetCapture）ので、離しは窓の外でも WM_LBUTTONUP で届き、ボタンから外れて
// 離せば取り消し（Windows の標準・D82）。client で押して窓の操作の上で離した WM_NCLBUTTONUP
// だけでは動かない。窓の手続きは押しの覚えの次の値と動かす操作をこの 1 本から写す。

[[nodiscard]] constexpr bool window_button(core::TitleBarHit hit) noexcept
{
    return hit == core::TitleBarHit::minimize || hit == core::TitleBarHit::maximize ||
           hit == core::TitleBarHit::close;
}

// 帯の要素 → WM_NCHITTEST の答え。窓の操作と caption 以外は client。
[[nodiscard]] constexpr LRESULT caption_code(core::TitleBarHit hit) noexcept
{
    switch (hit)
    {
    case core::TitleBarHit::minimize:
        return HTMINBUTTON;
    case core::TitleBarHit::maximize:
        return HTMAXBUTTON;
    case core::TitleBarHit::close:
        return HTCLOSE;
    case core::TitleBarHit::caption:
        return HTCAPTION;
    case core::TitleBarHit::tab:
    case core::TitleBarHit::tab_close:
    case core::TitleBarHit::add_tab:
    case core::TitleBarHit::tab_list:
    case core::TitleBarHit::none:
        return HTCLIENT;
    }
    std::unreachable();
}

// 非クライアントのメッセージの hit test の値 → 帯の要素。窓の操作のほかは none（帯の位置を
// 持たない）。hit test の値は OS の開いた集合なので既定分岐を置く（CPP-017）。
[[nodiscard]] constexpr core::TitleBarTarget caption_target_of(WPARAM hit) noexcept
{
    switch (hit)
    {
    case HTMINBUTTON:
        return core::TitleBarTarget{core::TitleBarHit::minimize, 0};
    case HTMAXBUTTON:
        return core::TitleBarTarget{core::TitleBarHit::maximize, 0};
    case HTCLOSE:
        return core::TitleBarTarget{core::TitleBarHit::close, 0};
    default:
        return core::TitleBarTarget{core::TitleBarHit::none, 0};
    }
}

// 押しの覚えが窓の操作のものか（窓が捕まえている間の押し）。
[[nodiscard]] constexpr bool
holds_caption_press(const std::optional<core::TitleBarTarget> &pressed) noexcept
{
    return pressed.has_value() && window_button(pressed.value().hit);
}

// 押しの覚えの次の値と、動かす窓の操作（hit test の値）。
using CaptionStep = std::pair<std::optional<core::TitleBarTarget>, std::optional<WPARAM>>;

// 窓のメッセージ 1 通の後。at は指している要素（非クライアントのメッセージは hit test の値から、
// WM_LBUTTONUP は client の点から引いた帯の要素）。
// - WM_NCLBUTTONDOWN / WM_NCLBUTTONDBLCLK: 窓の操作の上なら覚え、ほかでは消す（2 回目の押しも
//   1 回目と同じ押し）。
// - WM_NCLBUTTONUP: 覚えを消し、押した窓の操作と同じ上で離したときだけ動かす。
// - WM_LBUTTONUP: 窓の操作の押しを覚えているときだけ、上と同じ（帯のタブの押しは触らない）。
// - WM_CAPTURECHANGED: 窓の操作の押しを消す（捕まえを失ったら取り消し）。
// - ほかは変えない。
[[nodiscard]] inline CaptionStep
caption_step(UINT message, const core::TitleBarTarget &at,
             const std::optional<core::TitleBarTarget> &pressed) noexcept
{
    const auto released = [&pressed, &at]() -> std::optional<WPARAM>
    {
        const auto target = core::title_bar_released(pressed, at);
        if (!target.has_value() || !window_button(target.value().hit))
        {
            return std::nullopt;
        }
        return static_cast<WPARAM>(caption_code(target.value().hit));
    };
    switch (message)
    {
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK:
        return CaptionStep{window_button(at.hit) ? std::optional{at} : std::nullopt, std::nullopt};
    case WM_NCLBUTTONUP:
        return CaptionStep{std::nullopt, released()};
    case WM_LBUTTONUP:
        if (!holds_caption_press(pressed))
        {
            return CaptionStep{pressed, std::nullopt};
        }
        return CaptionStep{std::nullopt, released()};
    case WM_CAPTURECHANGED:
        return CaptionStep{holds_caption_press(pressed) ? std::nullopt : pressed, std::nullopt};
    default:
        return CaptionStep{pressed, std::nullopt};
    }
}
} // namespace nenenib::ui::win32
