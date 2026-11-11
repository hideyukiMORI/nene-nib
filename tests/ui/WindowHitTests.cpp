// Issue #1498: WM_NCHITTEST の答え。最大化していない窓では縁（8 DIP・窓の操作の上は 4 DIP）が
// 窓の操作より先に勝ち、4 隅・× □ ＿ の上端・× の右端で大きさを変えられる。最大化した窓では隅の
// 画素まで × を押せる。窓は作らず、窓の手続きが答えを引く 1 本（window_hit）を、本物の帯の配置
// （title_bar_layout）から引いた要素で確かめる。DPI 96 / 120 / 144 と、ふつうの窓と最小の窓
// （360 × 200 DIP）。
#include "TitleBarHit.hpp"
#include "WindowChecks.hpp"
#include "WindowHit.hpp"

#include "DevicePixels.hpp"
#include "TitleBarInput.hpp"
#include "TitleBarLayout.hpp"

#include <windows.h>

#include <array>
#include <cstdint>
#include <optional>
#include <tuple>
#include <utility>

namespace nenenib::tests::ui
{
namespace
{
using nenenib::core::to_pixels;
using nenenib::ui::win32::caption_code;
using nenenib::ui::win32::resize_border_width;
using nenenib::ui::win32::window_hit;

constexpr std::int32_t border_dips = 8;

// 点・最大化していない窓の答え・最大化した窓の答え（HTNOWHERE は「その点の帯の要素の答え」）。
using HitRow = std::tuple<POINT, LRESULT, LRESULT>;

// 窓の大きさ（DIP）。
using WindowSize = std::pair<std::int32_t, std::int32_t>;
constexpr std::array<WindowSize, 2> sizes{{{1000, 700}, {360, 200}}};
constexpr std::array<std::uint32_t, 3> dpis{96U, 120U, 144U};

// w × h の client（物理画素）の点と答え。× の中心は右端から 23 DIP・□ は 69・＿ は 115（46 DIP
// の幅）。縁は 8 DIP で、窓の操作の上はその半分。caption の点は窓の操作の左のつかみ（40 DIP）。
std::array<HitRow, 17> rows_of(std::int32_t w, std::int32_t h, std::uint32_t dpi)
{
    const auto px = [dpi](std::int32_t dips) { return to_pixels(dips, dpi); };
    const std::int32_t border = px(border_dips);
    const std::int32_t thin = border / 2;
    const std::int32_t close = w - px(23);
    const std::int32_t maximize = w - px(69);
    const std::int32_t minimize = w - px(115);
    const std::int32_t grab = w - px(158);
    const std::int32_t middle = px(20);
    return {{
        {POINT{w - 1, 0}, HTTOPRIGHT, HTCLOSE},
        {POINT{w - thin, thin - 1}, HTTOPRIGHT, HTCLOSE},
        {POINT{close, 0}, HTTOP, HTCLOSE},
        {POINT{close, thin - 1}, HTTOP, HTCLOSE},
        {POINT{close, thin}, HTCLOSE, HTCLOSE},
        {POINT{w - 1, middle}, HTRIGHT, HTCLOSE},
        {POINT{w - thin - 1, middle}, HTCLOSE, HTCLOSE},
        {POINT{maximize, 0}, HTTOP, HTMAXBUTTON},
        {POINT{maximize, thin}, HTMAXBUTTON, HTMAXBUTTON},
        {POINT{minimize, 0}, HTTOP, HTMINBUTTON},
        {POINT{minimize, thin}, HTMINBUTTON, HTMINBUTTON},
        {POINT{grab, border - 1}, HTTOP, HTCAPTION},
        {POINT{grab, border}, HTCAPTION, HTCAPTION},
        {POINT{0, 0}, HTTOPLEFT, HTNOWHERE},
        {POINT{0, h - 1}, HTBOTTOMLEFT, HTCLIENT},
        {POINT{w - 1, h - 1}, HTBOTTOMRIGHT, HTCLIENT},
        {POINT{w / 2, h / 2}, HTCLIENT, HTCLIENT},
    }};
}

void verify_window_size(WindowSize size, std::uint32_t dpi)
{
    const std::int32_t w = to_pixels(size.first, dpi);
    const std::int32_t h = to_pixels(size.second, dpi);
    const RECT client{0, 0, w, h};
    const auto layout = core::title_bar_layout(core::TitleBarInput{w, dpi, 1, 0, 0, std::nullopt});
    const std::int32_t margin = to_pixels(border_dips, dpi);
    for (const auto &[point, restored, zoomed] : rows_of(w, h, dpi))
    {
        const auto hit = core::title_bar_target(layout, point.x, point.y).hit;
        expect(window_hit(point, client, resize_border_width(margin, false), hit) == restored,
               "a window that is not maximized answers its border before the window buttons");
        const LRESULT wanted = zoomed == HTNOWHERE ? caption_code(hit) : zoomed;
        expect(window_hit(point, client, resize_border_width(margin, true), hit) == wanted,
               "a maximized window has no border and answers the title bar element");
    }
}
} // namespace

void verify_window_hits()
{
    for (const auto size : sizes)
    {
        for (const auto dpi : dpis)
        {
            verify_window_size(size, dpi);
        }
    }
}
} // namespace nenenib::tests::ui
