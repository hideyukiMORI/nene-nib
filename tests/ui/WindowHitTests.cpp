// Issue #1498: WM_NCHITTEST の答え。最大化していない窓では縁（8 DIP・窓の操作の上は 4 DIP）が
// 窓の操作より先に勝ち、4 隅・× □ ＿ の上端・× の右端で大きさを変えられる。最大化した窓と、
// モニターの端に接した辺（Snap した窓の右端など）には縁が無く、画面の端へ投げたマウスが × に
// 当たる。窓は作らず、窓の手続きが答えを引く 2 本（resize_borders と window_hit）を、本物の帯の
// 配置（title_bar_layout）から引いた要素で確かめる。DPI 96 / 120 / 144 / 168 / 192 と、ふつうの
// 窓と最小の窓（360 × 200 DIP）。期待値は実装の式から作らず、定数の表で書く。
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
using nenenib::ui::win32::resize_borders;
using nenenib::ui::win32::window_hit;

constexpr std::int32_t border_dips = 8;

// DPI・縁の幅・窓の操作の上の縁の幅（物理画素）。丸めは 168 DPI で奇数の 14 → 7 になる。
using DpiBorder = std::tuple<std::uint32_t, std::int32_t, std::int32_t>;
constexpr std::array<DpiBorder, 5> dpi_borders{{
    {96U, 8, 4},
    {120U, 10, 5},
    {144U, 12, 6},
    {168U, 14, 7},
    {192U, 16, 8},
}};

// 点と、ふつうの窓（モニターの中ほど）・最大化した窓・右半分に Snap した窓・左半分に Snap した
// 窓の答え。HTNOWHERE は「その点の帯の要素の答え」。Snap した窓は高さがモニターいっぱいで、
// 右（左）と上と下がモニターの端に接する。
using HitRow = std::tuple<POINT, LRESULT, LRESULT, LRESULT, LRESULT>;

// 窓の大きさ（DIP）。
using WindowSize = std::pair<std::int32_t, std::int32_t>;
constexpr std::array<WindowSize, 2> sizes{{{1000, 700}, {360, 200}}};

// w × h の client（物理画素）の点と答え。× の中心は右端から 23 DIP・□ は 69・＿ は 115（46 DIP
// の幅）。caption の点は窓の操作の左のつかみ（40 DIP）。
std::array<HitRow, 19> rows_of(std::int32_t w, std::int32_t h, const DpiBorder &dpi_border)
{
    const std::uint32_t dpi = std::get<0>(dpi_border);
    const std::int32_t border = std::get<1>(dpi_border);
    const std::int32_t thin = std::get<2>(dpi_border);
    const auto px = [dpi](std::int32_t dips) { return to_pixels(dips, dpi); };
    const std::int32_t close = w - px(23);
    const std::int32_t maximize = w - px(69);
    const std::int32_t minimize = w - px(115);
    const std::int32_t grab = w - px(158);
    const std::int32_t middle = px(20);
    return {{
        {POINT{w - 1, 0}, HTTOPRIGHT, HTCLOSE, HTCLOSE, HTRIGHT},
        {POINT{w - thin, thin - 1}, HTTOPRIGHT, HTCLOSE, HTCLOSE, HTRIGHT},
        {POINT{close, 0}, HTTOP, HTCLOSE, HTCLOSE, HTCLOSE},
        {POINT{close, thin - 1}, HTTOP, HTCLOSE, HTCLOSE, HTCLOSE},
        {POINT{close, thin}, HTCLOSE, HTCLOSE, HTCLOSE, HTCLOSE},
        {POINT{w - 1, middle}, HTRIGHT, HTCLOSE, HTCLOSE, HTRIGHT},
        {POINT{w - thin - 1, middle}, HTCLOSE, HTCLOSE, HTCLOSE, HTCLOSE},
        {POINT{maximize, 0}, HTTOP, HTMAXBUTTON, HTMAXBUTTON, HTMAXBUTTON},
        {POINT{maximize, thin}, HTMAXBUTTON, HTMAXBUTTON, HTMAXBUTTON, HTMAXBUTTON},
        {POINT{minimize, 0}, HTTOP, HTMINBUTTON, HTMINBUTTON, HTMINBUTTON},
        {POINT{minimize, thin}, HTMINBUTTON, HTMINBUTTON, HTMINBUTTON, HTMINBUTTON},
        {POINT{grab, border - 1}, HTTOP, HTCAPTION, HTCAPTION, HTCAPTION},
        {POINT{grab, border}, HTCAPTION, HTCAPTION, HTCAPTION, HTCAPTION},
        {POINT{0, 0}, HTTOPLEFT, HTNOWHERE, HTLEFT, HTNOWHERE},
        {POINT{0, h / 2}, HTLEFT, HTCLIENT, HTLEFT, HTCLIENT},
        {POINT{0, h - 1}, HTBOTTOMLEFT, HTCLIENT, HTLEFT, HTCLIENT},
        {POINT{w / 2, h - 1}, HTBOTTOM, HTCLIENT, HTCLIENT, HTCLIENT},
        {POINT{w - 1, h - 1}, HTBOTTOMRIGHT, HTCLIENT, HTCLIENT, HTRIGHT},
        {POINT{w / 2, h / 2}, HTCLIENT, HTCLIENT, HTCLIENT, HTCLIENT},
    }};
}

[[nodiscard]] bool same_rect(const RECT &left, const RECT &right) noexcept
{
    return left.left == right.left && left.top == right.top && left.right == right.right &&
           left.bottom == right.bottom;
}

[[nodiscard]] LRESULT answer_or_element(LRESULT answer, core::TitleBarHit hit) noexcept
{
    return answer == HTNOWHERE ? caption_code(hit) : answer;
}

void verify_window_size(WindowSize size, const DpiBorder &dpi_border)
{
    const auto [dpi, border, thin] = dpi_border;
    const std::int32_t w = to_pixels(size.first, dpi);
    const std::int32_t h = to_pixels(size.second, dpi);
    const RECT client{0, 0, w, h};
    const auto layout = core::title_bar_layout(core::TitleBarInput{w, dpi, 1, 0, 0, std::nullopt});
    const std::int32_t margin = to_pixels(border_dips, dpi);
    expect(margin == border, "the border is 8 DIP rounded to pixels");
    // ふつうの窓はモニターの中ほど。Snap した窓は幅 2w・高さ h のモニターの右半分と左半分。
    const RECT inner{-w, -h, 2 * w, 2 * h};
    const RECT wide{0, 0, 2 * w, h};
    const RECT right_half{w, 0, 2 * w, h};
    const RECT restored = resize_borders(margin, false, client, inner);
    const RECT failed = resize_borders(margin, false, right_half, std::nullopt);
    const RECT zoomed = resize_borders(margin, true, client, wide);
    const RECT right = resize_borders(margin, false, right_half, wide);
    const RECT left = resize_borders(margin, false, client, wide);
    for (const auto &[point, in_middle, in_zoomed, in_right, in_left] : rows_of(w, h, dpi_border))
    {
        const auto hit = core::title_bar_target(layout, point.x, point.y).hit;
        expect(window_hit(point, client, restored, hit) == in_middle,
               "a window that is not maximized answers its border before the window buttons");
        expect(window_hit(point, client, failed, hit) == in_middle,
               "a window whose monitor is unknown keeps the border on every side");
        expect(window_hit(point, client, zoomed, hit) == answer_or_element(in_zoomed, hit),
               "a maximized window has no border and answers the title bar element");
        expect(window_hit(point, client, right, hit) == answer_or_element(in_right, hit),
               "a window snapped to the right half has no border on the monitor's edges");
        expect(window_hit(point, client, left, hit) == answer_or_element(in_left, hit),
               "a window snapped to the left half has no border on the monitor's edges");
    }
}

// 窓の矩形・モニター（nullopt は取れなかった）・最大化・答え（left top right bottom）。縁は 8。
using BorderRow = std::tuple<RECT, std::optional<RECT>, bool, RECT>;
constexpr RECT first_monitor{0, 0, 1920, 1080};
constexpr RECT second_monitor{1920, 0, 3840, 1080};

constexpr std::array<BorderRow, 14> border_rows{{
    // モニターの中ほど（今と同じ）。
    {RECT{200, 100, 1200, 800}, first_monitor, false, RECT{8, 8, 8, 8}},
    // 1 画素だけ内側の辺は接していない。
    {RECT{1, 1, 1919, 1079}, first_monitor, false, RECT{8, 8, 8, 8}},
    // 右半分と左半分の Snap。見えない枠の分だけモニターの外へはみ出しても同じ。
    {RECT{960, 0, 1920, 1080}, first_monitor, false, RECT{8, 0, 0, 0}},
    {RECT{953, -7, 1927, 1087}, first_monitor, false, RECT{8, 0, 0, 0}},
    {RECT{0, 0, 960, 1080}, first_monitor, false, RECT{0, 0, 8, 0}},
    {RECT{-7, -7, 967, 1087}, first_monitor, false, RECT{0, 0, 8, 0}},
    // 上端に接する窓。
    {RECT{200, 0, 1200, 700}, first_monitor, false, RECT{8, 0, 8, 8}},
    // 4 隅の四分の一。
    {RECT{0, 0, 960, 540}, first_monitor, false, RECT{0, 0, 8, 8}},
    {RECT{960, 0, 1920, 540}, first_monitor, false, RECT{8, 0, 0, 8}},
    {RECT{0, 540, 960, 1080}, first_monitor, false, RECT{0, 8, 8, 0}},
    {RECT{960, 540, 1920, 1080}, first_monitor, false, RECT{8, 8, 0, 0}},
    // 2 枚目のモニターの左半分。モニターの間の辺もモニターの端として扱う。
    {RECT{1920, 0, 2880, 1080}, second_monitor, false, RECT{0, 0, 8, 0}},
    // モニターを取れなければ、接していても今の縁。
    {RECT{960, 0, 1920, 1080}, std::nullopt, false, RECT{8, 8, 8, 8}},
    // 最大化は 4 辺とも 0（モニターを取れなくても）。
    {RECT{200, 100, 1200, 800}, std::nullopt, true, RECT{0, 0, 0, 0}},
}};
} // namespace

void verify_window_hits()
{
    for (const auto &[placed, edges, zoomed, wanted] : border_rows)
    {
        expect(same_rect(resize_borders(8, zoomed, placed, edges), wanted),
               "the sides that touch the monitor's edge have no border");
    }
    for (const auto size : sizes)
    {
        for (const auto &dpi_border : dpi_borders)
        {
            verify_window_size(size, dpi_border);
        }
    }
}
} // namespace nenenib::tests::ui
