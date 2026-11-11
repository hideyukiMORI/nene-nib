// Issue #1464: 窓の操作（＿ □ ×）は押した要素と離した要素が同じときだけ動かす（ADR 0056 の
// 決定 9 の追補）。client で押して（ミニマップ・Vim モードの本文は捕まえない）上へ引いて × の上で
// 離すと、WM_NCLBUTTONUP だけで窓が閉じていた（#1449）。窓は作らず、窓の手続きが押しの覚え
// left_pressed_ と動かす操作を写す 1 本（caption_step）にメッセージの列を流して確かめる。
#include "CaptionPress.hpp"
#include "TitleBarHit.hpp"
#include "TitleBarTarget.hpp"
#include "WindowChecks.hpp"

#include <windows.h>

#include <array>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace nenenib::tests::ui
{
namespace
{
using nenenib::core::TitleBarHit;
using nenenib::core::TitleBarTarget;
using nenenib::ui::win32::caption_step;
using nenenib::ui::win32::caption_target_of;
using Pressed = std::optional<TitleBarTarget>;
// 送るメッセージと、指している要素（非クライアントは hit test の値、client は帯の要素）。
using Message = std::pair<UINT, TitleBarTarget>;

constexpr TitleBarTarget nothing{TitleBarHit::none, 0};

[[nodiscard]] Message nc(UINT message, WPARAM hit) noexcept
{
    return Message{message, caption_target_of(hit)};
}

// 列を流して、動かした窓の操作（hit test の値）を順に返す。pressed は列の前の覚え（client
// の押しの後なら none）。
[[nodiscard]] std::vector<WPARAM> run(std::span<const Message> messages, Pressed pressed)
{
    std::vector<WPARAM> activated;
    for (const auto &[message, at] : messages)
    {
        const auto [next, button] = caption_step(message, at, pressed);
        pressed = next;
        if (button.has_value())
        {
            activated.push_back(button.value());
        }
    }
    return activated;
}

[[nodiscard]] bool only(const std::vector<WPARAM> &activated, WPARAM hit) noexcept
{
    return activated.size() == 1 && activated.front() == hit;
}

void verify_same_button()
{
    for (const WPARAM hit : std::array<WPARAM, 3>{HTCLOSE, HTMAXBUTTON, HTMINBUTTON})
    {
        const std::array down_up{nc(WM_NCLBUTTONDOWN, hit), nc(WM_NCLBUTTONUP, hit)};
        expect(only(run(down_up, std::nullopt), hit),
               "pressing and releasing the same window button runs it");
        // 捕まえている間の離しは client の WM_LBUTTONUP で届く（点は同じボタンの上）。
        const std::array captured{nc(WM_NCLBUTTONDOWN, hit),
                                  Message{WM_LBUTTONUP, caption_target_of(hit)}};
        expect(only(run(captured, std::nullopt), hit),
               "a captured release over the pressed button runs it");
        // 2 回目の押しは DBLCLK で届くが、1 回目と同じ押し。
        const std::array twice{nc(WM_NCLBUTTONDOWN, hit), nc(WM_NCLBUTTONUP, hit),
                               nc(WM_NCLBUTTONDBLCLK, hit), nc(WM_NCLBUTTONUP, hit)};
        expect(run(twice, std::nullopt).size() == 2, "a double click on a button runs it twice");
    }
}

void verify_release_without_press()
{
    for (const WPARAM hit : std::array<WPARAM, 3>{HTCLOSE, HTMAXBUTTON, HTMINBUTTON})
    {
        const std::array up{nc(WM_NCLBUTTONUP, hit)};
        expect(run(up, std::nullopt).empty(), "a release alone over a window button does nothing");
        // client の本文やミニマップの押し（click_client が覚えるのは帯の外 = none）の後。
        expect(run(up, nothing).empty(),
               "a press in the client released over a window button does nothing");
        // 帯のタブの押しの後の窓の操作の離し（レビュー L-3）。
        expect(run(up, TitleBarTarget{TitleBarHit::tab, 1}).empty(),
               "a press on a tab released over a window button does nothing");
        // 縁の押し（HTLEFT）は窓の操作の押しの覚えを消す（レビュー L-3）。
        const std::array border{nc(WM_NCLBUTTONDOWN, hit), nc(WM_NCLBUTTONDOWN, HTLEFT),
                                nc(WM_NCLBUTTONUP, hit)};
        expect(run(border, std::nullopt).empty(),
               "a press on the border forgets an earlier window button press");
        const std::array caption{nc(WM_NCLBUTTONDOWN, HTCAPTION), nc(WM_NCLBUTTONUP, hit)};
        expect(run(caption, std::nullopt).empty(),
               "a press on the caption released over a window button does nothing");
    }
}

void verify_moved(WPARAM pressed, WPARAM released)
{
    const std::array moved{nc(WM_NCLBUTTONDOWN, pressed), nc(WM_NCLBUTTONUP, released)};
    expect(run(moved, std::nullopt).empty(),
           "pressing one window button and releasing on another does nothing");
    const std::array captured{nc(WM_NCLBUTTONDOWN, pressed),
                              Message{WM_LBUTTONUP, caption_target_of(released)}};
    expect(run(captured, std::nullopt).empty(),
           "a captured release over another window button does nothing");
}

void verify_other_button_cancels()
{
    // 押したボタンと違うボタンの上で離す組（3 × 2 通り）。
    constexpr std::array<std::pair<WPARAM, WPARAM>, 6> pairs{{{HTCLOSE, HTMAXBUTTON},
                                                              {HTCLOSE, HTMINBUTTON},
                                                              {HTMAXBUTTON, HTCLOSE},
                                                              {HTMAXBUTTON, HTMINBUTTON},
                                                              {HTMINBUTTON, HTCLOSE},
                                                              {HTMINBUTTON, HTMAXBUTTON}}};
    for (const auto &[pressed, released] : pairs)
    {
        verify_moved(pressed, released);
    }
}

void verify_press_is_forgotten()
{
    // 外で離す（捕まえているので WM_LBUTTONUP で届く）と取り消し、覚えも消える。
    const std::array outside{nc(WM_NCLBUTTONDOWN, HTCLOSE), Message{WM_LBUTTONUP, nothing},
                             nc(WM_NCLBUTTONUP, HTCLOSE)};
    expect(run(outside, std::nullopt).empty(),
           "releasing outside a pressed button cancels it and forgets the press");
    const std::array lost{nc(WM_NCLBUTTONDOWN, HTCLOSE), Message{WM_CAPTURECHANGED, nothing},
                          nc(WM_NCLBUTTONUP, HTCLOSE)};
    expect(run(lost, std::nullopt).empty(), "losing the capture forgets the press");
    const std::array released{nc(WM_NCLBUTTONDOWN, HTCLOSE), nc(WM_NCLBUTTONUP, HTCAPTION),
                              nc(WM_NCLBUTTONUP, HTCLOSE)};
    expect(run(released, std::nullopt).empty(), "a release anywhere forgets the press");
    // 帯のタブの押し（client）は窓の操作の離しで消えず、捕まえを失っても残る（release_title_bar
    // が読む）。
    const TitleBarTarget tab{TitleBarHit::tab_close, 2};
    expect(caption_step(WM_LBUTTONUP, tab, tab).first == Pressed{tab} &&
               caption_step(WM_CAPTURECHANGED, nothing, tab).first == Pressed{tab},
           "a tab press is left to the tab band");
}
} // namespace

void verify_caption_presses()
{
    verify_same_button();
    verify_release_without_press();
    verify_other_button_cancels();
    verify_press_is_forgotten();
}
} // namespace nenenib::tests::ui
