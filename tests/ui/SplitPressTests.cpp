// Issue #1941（ADR 0148 の 2026-11-22 の追補の決定 3）: 2
// 枠の境界の押し・ドラッグ・離し・Esc・捕まえ を失う・ダブルクリック・全面プレビューと Ctrl+P
// の一覧の間の押し（レビュー R4 の M1）。窓は作らず、 窓の手続きが押しの覚え split_grab_
// と送る比を写す 1 本（split_step）と、押しの当たりの 1 本
// （split_divider_start_under）にメッセージの列を流して確かめる。
#include "FontSize.hpp"
#include "LayoutRect.hpp"
#include "PaneLayouts.hpp"
#include "PaneSplit.hpp"
#include "SplitExtent.hpp"
#include "SplitGrab.hpp"
#include "SplitLayout.hpp"
#include "SplitPoint.hpp"
#include "SplitPress.hpp"
#include "SplitShare.hpp"
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
namespace core = nenenib::core;
using nenenib::ui::win32::split_divider_start_under;
using nenenib::ui::win32::split_step;
using nenenib::ui::win32::SplitGrab;
using nenenib::ui::win32::SplitPoint;
using Held = std::optional<SplitGrab>;
// 送るメッセージと点の分割の向きの座標と、押しの点が見えている境界の上か（押しのときだけ読む）。
using Message = std::tuple<UINT, std::int32_t, bool>;

constexpr std::uint32_t dpi = 96;
constexpr core::LayoutRect client{0, 0, 1000, 600};

[[nodiscard]] core::PaneSplit side_by_side(core::SplitShare share)
{
    return core::PaneSplit{core::SplitLayout::vertical, share};
}

[[nodiscard]] core::PaneLayouts panes_of(core::PaneSplit split)
{
    return core::pane_layouts(client, dpi, core::default_font_size(), split);
}

[[nodiscard]] std::optional<core::SplitExtent> extent_of(core::PaneSplit split)
{
    return core::split_extent(client, dpi, core::default_font_size(), split);
}

// 列を流して、送った比を順に返す（送るたびに今の比を送った比にする＝controller が置く）。
[[nodiscard]] std::vector<core::SplitShare> run(std::span<const Message> messages, Held &held,
                                                core::SplitShare &current)
{
    const auto divider = panes_of(side_by_side(current)).divider;
    const std::int32_t start = divider.has_value() ? divider.value().left : 0;
    std::vector<core::SplitShare> sent;
    for (const auto &[message, at, over] : messages)
    {
        const SplitPoint point{at, over ? std::optional{start} : std::nullopt,
                               extent_of(side_by_side(current)), current};
        const auto [next, share] = split_step(message, point, held);
        held = next;
        if (share.has_value())
        {
            sent.push_back(share.value());
            current = share.value();
        }
    }
    return sent;
}

[[nodiscard]] std::int32_t divider_left(core::SplitShare share)
{
    const auto divider = panes_of(side_by_side(share)).divider;
    return divider.has_value() ? divider.value().left : -1;
}

void verify_press_drag_release()
{
    Held held;
    core::SplitShare current{};
    const auto start = divider_left(current);
    const std::array still{Message{WM_LBUTTONDOWN, start + 1, true},
                           Message{WM_MOUSEMOVE, start + 1, false},
                           Message{WM_LBUTTONUP, start + 1, false}};
    expect(run(still, held, current).empty() && !held.has_value() && current == core::SplitShare{},
           "pressing the divider and releasing it in place sends nothing");
    const std::array drag{
        Message{WM_LBUTTONDOWN, start + 1, true}, Message{WM_MOUSEMOVE, start - 99, false},
        Message{WM_MOUSEMOVE, start - 199, false}, Message{WM_LBUTTONUP, start - 199, false}};
    const auto sent = run(drag, held, current);
    expect(sent.size() == 2 && !held.has_value(),
           "a drag sends each new share and the release at the same point sends nothing more");
    expect(divider_left(current) == start - 200,
           "the divider follows the pointer less the offset of the press");
    const std::array after{Message{WM_MOUSEMOVE, 100, false}, Message{WM_LBUTTONUP, 100, false}};
    expect(run(after, held, current).empty(), "moving after the release sends nothing");
}

void verify_escape_and_lost_capture()
{
    Held held;
    core::SplitShare current{};
    const auto start = divider_left(current);
    const std::array escaped{Message{WM_LBUTTONDOWN, start, true},
                             Message{WM_MOUSEMOVE, start - 150, false},
                             Message{WM_KEYDOWN, 0, false}, Message{WM_MOUSEMOVE, 50, false}};
    const auto sent = run(escaped, held, current);
    expect(sent.size() == 2 && sent.back() == core::SplitShare{} && !held.has_value(),
           "Esc during a drag sends the share before the press and ends the drag");
    const std::array pressed{Message{WM_LBUTTONDOWN, start, true}, Message{WM_KEYDOWN, 0, false}};
    expect(run(pressed, held, current).empty() && !held.has_value(),
           "Esc after a press without a move ends it without sending");
    const std::array lost{Message{WM_LBUTTONDOWN, start, true},
                          Message{WM_MOUSEMOVE, start - 150, false},
                          Message{WM_CAPTURECHANGED, 0, false}, Message{WM_LBUTTONUP, 40, false}};
    const auto kept = run(lost, held, current);
    expect(kept.size() == 1 && !held.has_value() && !(current == core::SplitShare{}),
           "losing the capture keeps the share where the drag left it");
    Held other{SplitGrab{2, 300, true, core::SplitShare{}}};
    const std::array timer{Message{WM_TIMER, 10, false}};
    expect(run(timer, other, current).empty() &&
               other == Held{SplitGrab{2, 300, true, core::SplitShare{}}},
           "other messages keep the drag");
}

void verify_double_click()
{
    Held held;
    core::SplitShare current{1, 3};
    const auto start = divider_left(current);
    const std::array on{Message{WM_LBUTTONDBLCLK, start, true}};
    const auto sent = run(on, held, current);
    expect(sent.size() == 1 && sent.front() == core::SplitShare{} && !held.has_value(),
           "a double click on the divider evens the panes without starting a drag");
    current = core::SplitShare{1, 3};
    const std::array off{Message{WM_LBUTTONDBLCLK, start, false},
                         Message{WM_LBUTTONDOWN, start, false}};
    expect(run(off, held, current).empty() && !held.has_value(),
           "a press or double click away from the divider sends nothing");
}

// 全面プレビューと Ctrl+P の一覧の間は境界を出さない（隠れた境界の位置の押しはプレビューの
// リンクや選択へ届く・レビュー R4 の M1）。カーソル・押し・ダブルクリックが同じ 1 本を読む。
void verify_hidden_divider()
{
    const auto panes = panes_of(side_by_side(core::SplitShare{}));
    const auto start = divider_left(core::SplitShare{});
    constexpr auto vertical = core::SplitLayout::vertical;
    expect(split_divider_start_under(panes, vertical, dpi, POINT{start + 1, 300}) ==
               std::optional{start},
           "a point on the shown divider gives its start");
    expect(
        !split_divider_start_under(std::nullopt, vertical, dpi, POINT{start + 1, 300}).has_value(),
        "a full preview or the Ctrl+P list hides the divider from the press");
    expect(!split_divider_start_under(panes, vertical, dpi, POINT{start - 40, 300}).has_value(),
           "a point away from the divider is not on it");
    expect(!split_divider_start_under(panes_of(core::PaneSplit{}), core::SplitLayout::none, dpi,
                                      POINT{start, 300})
                .has_value(),
           "one pane has no divider");
    const auto stacked = panes_of(core::PaneSplit{core::SplitLayout::horizontal, {}});
    const auto top = stacked.divider.has_value() ? stacked.divider.value().top : -1;
    expect(split_divider_start_under(stacked, core::SplitLayout::horizontal, dpi,
                                     POINT{500, top}) == std::optional{top},
           "a stacked split gives the top of its divider");
    const Held held;
    const auto [grab, share] =
        split_step(WM_LBUTTONDOWN, SplitPoint{start + 1, std::nullopt, std::nullopt, {}}, held);
    expect(!grab.has_value() && !share.has_value(),
           "a press on the hidden divider does not start a drag");
}
} // namespace

void verify_split_presses()
{
    verify_press_drag_release();
    verify_escape_and_lost_capture();
    verify_double_click();
    verify_hidden_divider();
}
} // namespace nenenib::tests::ui
