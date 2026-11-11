// Issue #1371（ADR 0171 の決定 5 の追補）: ミニマップの面の押し・ドラッグ・離し。窓は作らず、窓の
// 手続きが押しの覚え minimap_grab_ と送る意図を写す 1 本（minimap_step）にメッセージの列を流して
// 確かめる。押す → 動かす → 離す・同じ段の動きは送らない・面の外の押し・捕まえを失う・窓の外・
// Esc。面を押して × の上で離しても窓の操作は動かない（#1464 の caption_step と組む）。
#include "CaptionPress.hpp"
#include "LayoutRect.hpp"
#include "MinimapGrab.hpp"
#include "MinimapPointing.hpp"
#include "MinimapPress.hpp"
#include "PointMinimap.hpp"
#include "TitleBarHit.hpp"
#include "TitleBarTarget.hpp"
#include "WindowChecks.hpp"

#include <windows.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <tuple>
#include <vector>

namespace nenenib::tests::ui
{
namespace
{
using nenenib::application::MinimapPointing;
using nenenib::application::PointMinimap;
using nenenib::ui::win32::minimap_step;
using nenenib::ui::win32::MinimapGrab;
using Held = std::optional<MinimapGrab>;
// 送るメッセージと点の y と、押しの点が面の上か（押しのときだけ読む）。
using Message = std::tuple<UINT, std::int32_t, bool>;

constexpr nenenib::core::LayoutRect panel{900, 60, 1000, 660};

// 列を流して、送った意図を順に返す。held は列の後の覚え。
[[nodiscard]] std::vector<PointMinimap> run(std::span<const Message> messages, Held &held)
{
    std::vector<PointMinimap> sent;
    for (const auto &[message, y, over] : messages)
    {
        const auto [next, intent] =
            minimap_step(message, y, over ? std::optional{panel} : std::nullopt, held);
        held = next;
        if (intent.has_value())
        {
            sent.push_back(intent.value());
        }
    }
    return sent;
}

[[nodiscard]] bool is(const PointMinimap &intent, MinimapPointing pointing, std::int32_t row)
{
    return intent.pointing == pointing && intent.row == row;
}

void verify_press_drag_release()
{
    Held held;
    const std::array messages{Message{WM_LBUTTONDOWN, 160, true}, Message{WM_MOUSEMOVE, 161, false},
                              Message{WM_MOUSEMOVE, 170, false}, Message{WM_MOUSEMOVE, 400, false},
                              Message{WM_LBUTTONUP, 402, false}};
    const auto sent = run(messages, held);
    expect(sent.size() == 4 && is(sent.at(0), MinimapPointing::pressed, 50) &&
               is(sent.at(1), MinimapPointing::dragged, 55) &&
               is(sent.at(2), MinimapPointing::dragged, 170) &&
               is(sent.at(3), MinimapPointing::dragged, 171),
           "a press jumps, a drag follows each new row and the release sends the last row");
    expect(!held.has_value(), "the release ends the drag");
    const std::array after{Message{WM_MOUSEMOVE, 300, false}, Message{WM_LBUTTONUP, 300, false}};
    expect(run(after, held).empty(), "moving after the release sends nothing");
}

void verify_outside_and_beyond()
{
    Held held;
    const std::array outside{Message{WM_LBUTTONDOWN, 160, false}, Message{WM_MOUSEMOVE, 200, false},
                             Message{WM_LBUTTONUP, 200, false}};
    expect(run(outside, held).empty() && !held.has_value(),
           "a press beside the panel sends nothing");
    // 面を押して上の帯（× の上・y = 5）まで引いて離す。段は負のまま送る（切り詰めは application）。
    const std::array up{Message{WM_LBUTTONDOWN, 160, true}, Message{WM_MOUSEMOVE, 5, false},
                        Message{WM_LBUTTONUP, 5, false}};
    const auto sent = run(up, held);
    expect(sent.size() == 2 && is(sent.at(1), MinimapPointing::dragged, -28) && !held.has_value(),
           "dragging above the panel sends a negative row and ends on release");
    // 窓の操作の押しを覚えていないので、× の上の WM_LBUTTONUP は窓を閉じない（#1464）。
    const auto close = nenenib::ui::win32::caption_step(
        WM_LBUTTONUP, nenenib::core::TitleBarTarget{nenenib::core::TitleBarHit::close, 0},
        std::nullopt);
    expect(!close.second.has_value(),
           "releasing a minimap drag over the close button keeps the window");
    // 2 回目の押しは DBLCLK で届くが、1 回目と同じ押し。
    const std::array twice{Message{WM_LBUTTONDOWN, 160, true}, Message{WM_LBUTTONUP, 160, false},
                           Message{WM_LBUTTONDBLCLK, 160, true}, Message{WM_LBUTTONUP, 160, false}};
    const auto both = run(twice, held);
    expect(both.size() == 2 && is(both.at(0), MinimapPointing::pressed, 50) &&
               is(both.at(1), MinimapPointing::pressed, 50),
           "a double click on the panel is two presses of the same row");
}

void verify_ended()
{
    for (const UINT ending : std::array<UINT, 3>{WM_CAPTURECHANGED, WM_MOUSELEAVE, WM_KEYDOWN})
    {
        Held held;
        const std::array messages{Message{WM_LBUTTONDOWN, 160, true}, Message{ending, 0, false},
                                  Message{WM_MOUSEMOVE, 300, false},
                                  Message{WM_LBUTTONUP, 300, false}};
        const auto sent = run(messages, held);
        expect(sent.size() == 1 && is(sent.front(), MinimapPointing::pressed, 50) &&
                   !held.has_value(),
               "losing the capture, leaving the window or Esc ends the drag where it is");
    }
    Held held{MinimapGrab{60, 50}};
    const std::array other{Message{WM_TIMER, 300, false}};
    expect(run(other, held).empty() && held == Held{MinimapGrab{60, 50}},
           "other messages keep the drag");
}
} // namespace

void verify_minimap_presses()
{
    verify_press_drag_release();
    verify_outside_and_beyond();
    verify_ended();
}
} // namespace nenenib::tests::ui
