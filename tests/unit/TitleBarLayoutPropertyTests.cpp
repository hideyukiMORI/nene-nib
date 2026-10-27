// scope `--title-bar-layout-property` の性質の試験（#929・ADR 0056 の決定 8・9・ADR 0042）。
// タブの帯の配置 title_bar_layout に、固定 seed の決定的な生成器（帯の幅 360〜4000 DIP の物理画素・
// DPI 96 / 120 / 144 / 192・タブ 0〜64 本・範囲の外も含むアクティブの位置・負と巨大を含む送り量・
// 載せた要素・押した要素）で作った入力を渡し、次を確かめる。
//   1. 見えているタブは帯の内で重ならず位置の順・× は自分のタブの内・「＋」「∨」と窓の操作と
//      タブの領域は帯の内で重ならない。
//   2. 各矩形の四隅・中心・外側 1 画素の点と帯の格子（幅に応じた固定の刻み・狭い帯は 1 画素刻みの
//      全点）で、title_bar_target の答えが点を含む矩形から計算した要素と一致する。見えていない
//      タブには当たらない。
//   3. 送り量は 0 以上・あふれた分以下で、送り量を変えてもタブの幅と領域は変わらない。
//   4. 同じ入力で同じ結果。押した要素と離した要素が違えば title_bar_released は値なし（決定 9）。
// 配置の入力は題名と未保存の印を持たない（タブの幅は本数と帯の幅だけで決まる）ので、題名の長さは
// 生成しない。題名の幅の測り方も配置の外（描画の text layout）にある。
// 回数は Debug（ASan）で scope 全体が 1 秒以内に終わる値。50 倍の走りは rounds を手で 50 にして
// 回す（commit しない）。生成器は std::mt19937_64 の剰余だけを使い、分布の型は使わない。
#include "DevicePixels.hpp"
#include "LayoutRect.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "TitleBarHit.hpp"
#include "TitleBarInput.hpp"
#include "TitleBarLayout.hpp"
#include "TitleBarTarget.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <random>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using core::LayoutRect;
using core::TitleBarHit;
using core::TitleBarInput;
using core::TitleBarLayout;
using core::TitleBarTarget;
using Random = std::mt19937_64;
using Point = std::pair<std::int32_t, std::int32_t>;
using Points = std::vector<Point>;
using Check = std::string_view (*)(const TitleBarInput &);

constexpr std::size_t rounds = 1U;
constexpr std::size_t layout_inputs = 300U * rounds;
// hit test の格子は 1 入力で数千点を引くので、ほかの族より少ない。
constexpr std::size_t hit_inputs = 200U * rounds;
constexpr std::size_t exhaustive_inputs = 3U * rounds;
constexpr std::int32_t minimum_width_dips = 360;
constexpr std::int32_t maximum_width_dips = 4000;
// 1 画素刻みで全点を走査する帯の幅の上限（DIP）。
constexpr std::int32_t exhaustive_width_dips = 640;
constexpr std::size_t maximum_tabs = 64U;
constexpr std::int32_t huge_scroll_dips = 1'000'000;
// 格子の刻みは帯の幅をこの数で割った値（1 画素以上）と、高さの 1/10。
constexpr std::int32_t grid_columns = 128;
constexpr std::int32_t grid_rows = 10;
constexpr std::array<std::uint32_t, 4> dpis{{96U, 120U, 144U, 192U}};

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

[[nodiscard]] std::int32_t within(Random &random, std::int32_t first, std::int32_t last)
{
    return first +
           static_cast<std::int32_t>(below(random, static_cast<std::size_t>(last - first) + 1U));
}

// --- 生成器 ---

// 帯の位置の欄。範囲の外（本数と本数 + 1）も混ぜる。
[[nodiscard]] std::size_t tab_index(Random &random, std::size_t count)
{
    return below(random, count + 2U);
}

constexpr std::array<TitleBarHit, 9> all_hits{
    {TitleBarHit::caption, TitleBarHit::tab, TitleBarHit::tab_close, TitleBarHit::add_tab,
     TitleBarHit::tab_list, TitleBarHit::minimize, TitleBarHit::maximize, TitleBarHit::close,
     TitleBarHit::none}};

[[nodiscard]] std::optional<TitleBarTarget> target_sample(Random &random, std::size_t count)
{
    const std::size_t kind = below(random, all_hits.size() + 1U);
    if (kind == all_hits.size())
    {
        return std::nullopt;
    }
    return TitleBarTarget{all_hits.at(kind), tab_index(random, count)};
}

// 送り量（DIP）。0・負・並びの全長の範囲・巨大のどれか。
[[nodiscard]] std::int32_t scroll_sample(Random &random, std::size_t count)
{
    constexpr std::int32_t step_dips = 122;
    const auto strip = static_cast<std::int32_t>(count + 1U) * step_dips;
    const std::size_t kind = below(random, 4U);
    if (kind == 0U)
    {
        return 0;
    }
    if (kind == 1U)
    {
        return -within(random, 1, 500);
    }
    return kind == 2U ? within(random, 0, strip) : within(random, strip, huge_scroll_dips);
}

// タブの本数。小さい本数（あふれない側）を厚めに混ぜる。
[[nodiscard]] std::size_t count_sample(Random &random)
{
    constexpr std::size_t few = 5U;
    return below(random, 3U) == 0U ? below(random, few) : below(random, maximum_tabs + 1U);
}

[[nodiscard]] TitleBarInput input_sample(Random &random, std::int32_t widest_dips)
{
    const std::uint32_t dpi = dpis.at(below(random, dpis.size()));
    const std::int32_t width =
        within(random, core::to_pixels(minimum_width_dips, dpi), core::to_pixels(widest_dips, dpi));
    const std::size_t count = count_sample(random);
    const std::size_t active = tab_index(random, count);
    const std::int32_t scroll = scroll_sample(random, count);
    return TitleBarInput{width, dpi, count, active, scroll, target_sample(random, count)};
}

// --- 矩形の述語 ---

[[nodiscard]] bool empty(const LayoutRect &rectangle)
{
    return rectangle.right <= rectangle.left || rectangle.bottom <= rectangle.top;
}

[[nodiscard]] bool inside(const LayoutRect &outer, const LayoutRect &inner)
{
    return inner.left >= outer.left && inner.right <= outer.right && inner.top >= outer.top &&
           inner.bottom <= outer.bottom;
}

[[nodiscard]] bool overlapping(const LayoutRect &left, const LayoutRect &right)
{
    return !empty(left) && !empty(right) && left.left < right.right && right.left < left.right &&
           left.top < right.bottom && right.top < left.bottom;
}

[[nodiscard]] LayoutRect clipped(const LayoutRect &rectangle, const LayoutRect &clip)
{
    return LayoutRect{std::max(rectangle.left, clip.left), std::max(rectangle.top, clip.top),
                      std::min(rectangle.right, clip.right),
                      std::min(rectangle.bottom, clip.bottom)};
}

[[nodiscard]] bool hovering_tab(const TitleBarLayout &layout, std::size_t index)
{
    if (!layout.hovered.has_value())
    {
        return false;
    }
    const TitleBarTarget hovered = layout.hovered.value();
    return (hovered.hit == TitleBarHit::tab || hovered.hit == TitleBarHit::tab_close) &&
           hovered.tab == index;
}

// --- 不変量 1: 矩形 ---

// タブの領域・「∨」・「＋」・窓の操作の 3 つ。空の「∨」（あふれていない）は重なりを問わない。
[[nodiscard]] std::array<LayoutRect, 6> fixed_rectangles(const TitleBarLayout &layout)
{
    return {{layout.viewport, layout.tab_list, layout.add_tab, layout.minimize, layout.maximize,
             layout.close}};
}

// first 番目の部品が帯の内にあり、後ろの部品と重ならない。
[[nodiscard]] bool apart(const TitleBarLayout &layout, const std::array<LayoutRect, 6> &parts,
                         std::size_t first)
{
    const LayoutRect &part = parts.at(first);
    const bool held = empty(part) || inside(layout.band, part);
    return held &&
           std::all_of(parts.begin() + static_cast<std::ptrdiff_t>(first) + 1, parts.end(),
                       [&part](const LayoutRect &other) { return !overlapping(part, other); });
}

[[nodiscard]] std::string_view fixed_violation(const TitleBarLayout &layout)
{
    const auto parts = fixed_rectangles(layout);
    for (std::size_t first = 0; first < parts.size(); ++first)
    {
        if (!apart(layout, parts, first))
        {
            return "a fixed part leaves the band or overlaps another";
        }
    }
    if (empty(layout.tab_list) == layout.overflowing)
    {
        return "the tab list shows exactly when the tabs overflow";
    }
    return {};
}

// タブ 1 本: 見えていれば領域で切り抜いた矩形が帯の内・空でない。× は値があれば自分のタブの内で、
// 値があるのはアクティブか載せたタブのときだけ（D21）。
[[nodiscard]] std::string_view tab_violation(const TitleBarLayout &layout, std::size_t index)
{
    const LayoutRect tab = core::tab_rect(layout, index);
    const LayoutRect shown = clipped(tab, layout.viewport);
    if (core::tab_visible(layout, index) == empty(shown))
    {
        return "a tab is visible exactly when it reaches into the viewport";
    }
    if (!empty(shown) && !inside(layout.band, shown))
    {
        return "a visible tab leaves the band";
    }
    const auto close = core::tab_close_rect(layout, index);
    if (close.has_value() != (index == layout.active || hovering_tab(layout, index)))
    {
        return "a close box belongs to the active or the hovered tab only";
    }
    if (close.has_value() && (empty(close.value()) || !inside(tab, close.value())))
    {
        return "a close box leaves its tab";
    }
    return {};
}

// タブ 1 本の性質と、次のタブとの順（右端が次の左端より前）。
[[nodiscard]] std::string_view ordered_tab_violation(const TitleBarLayout &layout,
                                                     std::size_t index)
{
    const std::string_view broken = tab_violation(layout, index);
    if (!broken.empty())
    {
        return broken;
    }
    const bool next = index + 1U < layout.tab_count;
    if (next && core::tab_rect(layout, index).right >= core::tab_rect(layout, index + 1U).left)
    {
        return "neighbouring tabs overlap or leave the position order";
    }
    return {};
}

[[nodiscard]] std::string_view tabs_violation(const TitleBarLayout &layout)
{
    if (layout.tab_width <= 0 || layout.tab_gap <= 0)
    {
        return "the tabs have a positive width and gap";
    }
    std::size_t visible = 0;
    for (std::size_t index = 0; index < layout.tab_count; ++index)
    {
        const std::string_view broken = ordered_tab_violation(layout, index);
        if (!broken.empty())
        {
            return broken;
        }
        visible += core::tab_visible(layout, index) ? 1U : 0U;
    }
    if (visible == 0U || core::tab_visible(layout, layout.tab_count))
    {
        return "at least one tab and no tab past the end is visible";
    }
    return {};
}

[[nodiscard]] std::string_view rectangle_violation(const TitleBarInput &input)
{
    const TitleBarLayout layout = core::title_bar_layout(input);
    if (layout.band != LayoutRect{0, 0, input.width, layout.band.bottom} ||
        layout.tab_count != std::max<std::size_t>(input.tab_count, 1U) ||
        layout.active >= layout.tab_count)
    {
        return "the band, the tab count and the active tab follow the input";
    }
    const std::string_view fixed = fixed_violation(layout);
    return fixed.empty() ? tabs_violation(layout) : fixed;
}

// --- 不変量 2: hit test ---

// 点を含む矩形から計算した要素（title_bar_target の手順を使わず、矩形の集合から引く）。
[[nodiscard]] TitleBarTarget expected_target(const TitleBarLayout &layout,
                                             const std::vector<std::size_t> &visible,
                                             std::int32_t x, std::int32_t y)
{
    if (!core::contains(layout.band, x, y))
    {
        return TitleBarTarget{TitleBarHit::none, 0};
    }
    const std::array<std::pair<LayoutRect, TitleBarHit>, 5> parts{
        {{layout.close, TitleBarHit::close},
         {layout.maximize, TitleBarHit::maximize},
         {layout.minimize, TitleBarHit::minimize},
         {layout.add_tab, TitleBarHit::add_tab},
         {layout.tab_list, TitleBarHit::tab_list}}};
    const auto part = std::ranges::find_if(parts, [x, y](const auto &entry)
                                           { return core::contains(entry.first, x, y); });
    if (part != parts.end())
    {
        return TitleBarTarget{part->second, 0};
    }
    const auto holds = [&layout, x, y](std::size_t index)
    { return core::contains(core::tab_rect(layout, index), x, y); };
    const auto tab = core::contains(layout.viewport, x, y) ? std::ranges::find_if(visible, holds)
                                                           : visible.end();
    if (tab == visible.end())
    {
        return TitleBarTarget{TitleBarHit::caption, 0};
    }
    const auto close = core::tab_close_rect(layout, *tab);
    const bool closing = close.has_value() && core::contains(close.value(), x, y);
    return TitleBarTarget{closing ? TitleBarHit::tab_close : TitleBarHit::tab, *tab};
}

[[nodiscard]] std::vector<std::size_t> visible_tabs(const TitleBarLayout &layout)
{
    std::vector<std::size_t> visible;
    for (std::size_t index = 0; index < layout.tab_count; ++index)
    {
        if (core::tab_visible(layout, index))
        {
            visible.push_back(index);
        }
    }
    return visible;
}

// 四隅・中心・辺の外 1 画素。
void add_rectangle_points(const LayoutRect &rectangle, Points &points)
{
    if (empty(rectangle))
    {
        return;
    }
    const std::int32_t middle_x = (rectangle.left + rectangle.right) / 2;
    const std::int32_t middle_y = (rectangle.top + rectangle.bottom) / 2;
    const std::array<Point, 9> around{{{rectangle.left, rectangle.top},
                                       {rectangle.right - 1, rectangle.top},
                                       {rectangle.left, rectangle.bottom - 1},
                                       {rectangle.right - 1, rectangle.bottom - 1},
                                       {middle_x, middle_y},
                                       {rectangle.left - 1, middle_y},
                                       {rectangle.right, middle_y},
                                       {middle_x, rectangle.top - 1},
                                       {middle_x, rectangle.bottom}}};
    points.insert(points.end(), around.begin(), around.end());
}

// 帯とその外 1 画素の格子。step 1 なら全点。
void add_grid_points(const TitleBarLayout &layout, std::int32_t step_x, std::int32_t step_y,
                     Points &points)
{
    for (std::int32_t y = -1; y <= layout.band.bottom; y += step_y)
    {
        for (std::int32_t x = -1; x <= layout.band.right; x += step_x)
        {
            points.emplace_back(x, y);
        }
    }
}

[[nodiscard]] Points probe_points(const TitleBarLayout &layout, bool exhaustive)
{
    Points points;
    for (const LayoutRect &part : fixed_rectangles(layout))
    {
        add_rectangle_points(part, points);
    }
    add_rectangle_points(layout.band, points);
    for (std::size_t index = 0; index < layout.tab_count; ++index)
    {
        add_rectangle_points(core::tab_rect(layout, index), points);
        add_rectangle_points(core::tab_close_rect(layout, index).value_or(LayoutRect{}), points);
    }
    const std::int32_t step_x = exhaustive ? 1 : std::max(layout.band.right / grid_columns, 1);
    const std::int32_t step_y = exhaustive ? 1 : std::max(layout.band.bottom / grid_rows, 1);
    add_grid_points(layout, step_x, step_y, points);
    return points;
}

[[nodiscard]] std::string_view hit_violation_at(const TitleBarLayout &layout, bool exhaustive)
{
    const std::vector<std::size_t> visible = visible_tabs(layout);
    for (const auto &[x, y] : probe_points(layout, exhaustive))
    {
        const TitleBarTarget hit = core::title_bar_target(layout, x, y);
        if (hit != expected_target(layout, visible, x, y))
        {
            return "the hit test differs from the rectangle that holds the point";
        }
        const bool tab = hit.hit == TitleBarHit::tab || hit.hit == TitleBarHit::tab_close;
        if (tab && !core::tab_visible(layout, hit.tab))
        {
            return "the hit test reaches a tab that is not visible";
        }
    }
    return {};
}

[[nodiscard]] std::string_view hit_violation(const TitleBarInput &input)
{
    return hit_violation_at(core::title_bar_layout(input), false);
}

[[nodiscard]] std::string_view exhaustive_hit_violation(const TitleBarInput &input)
{
    return hit_violation_at(core::title_bar_layout(input), true);
}

// --- 不変量 3: 送り量 ---

[[nodiscard]] TitleBarInput scrolled_to(const TitleBarInput &input, std::int32_t scroll_dips)
{
    TitleBarInput moved = input;
    moved.scroll_dips = scroll_dips;
    return moved;
}

[[nodiscard]] std::string_view scroll_range_violation(const TitleBarLayout &layout)
{
    const std::int32_t limit =
        std::max(core::width_of(layout.tabs) - core::width_of(layout.viewport), 0);
    if (layout.scroll < 0 || layout.scroll > limit || (!layout.overflowing && layout.scroll != 0))
    {
        return "the scroll stays within zero and the overflow";
    }
    if (layout.tabs.left != layout.viewport.left - layout.scroll)
    {
        return "the strip starts at the viewport moved by the scroll";
    }
    return {};
}

[[nodiscard]] std::string_view scroll_violation(const TitleBarInput &input)
{
    const TitleBarLayout layout = core::title_bar_layout(input);
    const TitleBarLayout start = core::title_bar_layout(scrolled_to(input, 0));
    const TitleBarLayout end = core::title_bar_layout(scrolled_to(input, huge_scroll_dips));
    const std::string_view range = scroll_range_violation(layout);
    if (!range.empty())
    {
        return range;
    }
    const bool same_size = layout.tab_width == start.tab_width &&
                           layout.tab_width == end.tab_width && layout.viewport == start.viewport &&
                           layout.viewport == end.viewport &&
                           core::width_of(layout.tabs) == core::width_of(start.tabs);
    if (!same_size)
    {
        return "the scroll changes neither the tab width nor the viewport";
    }
    const LayoutRect last = core::tab_rect(end, end.tab_count - 1U);
    const bool reaches = !end.overflowing || last.right == end.viewport.right;
    if (start.scroll != 0 || !reaches)
    {
        return "the scroll reaches the first tab at zero and the last tab at the far end";
    }
    return {};
}

// application は 96 DPI（幅も DIP）で送り量を直す。直した後はアクティブなタブの全体が領域に入る。
[[nodiscard]] std::string_view into_view_violation(const TitleBarInput &input)
{
    TitleBarInput reference = input;
    reference.dpi = core::reference_dpi;
    const std::int32_t scroll = core::tabs_scrolled_into_view(reference);
    const TitleBarLayout layout = core::title_bar_layout(scrolled_to(reference, scroll));
    if (layout.overflowing && layout.scroll != scroll)
    {
        return "the corrected scroll is already within its range";
    }
    const LayoutRect active = core::tab_rect(layout, layout.active);
    const bool fits = layout.tab_width <= core::width_of(layout.viewport);
    if (fits ? !inside(layout.viewport, active) : active.left != layout.viewport.left)
    {
        return "the corrected scroll shows the whole active tab";
    }
    const std::int32_t limit =
        std::max(core::width_of(layout.tabs) - core::width_of(layout.viewport), 0);
    const TitleBarInput corrected = scrolled_to(reference, scroll);
    const std::int32_t wheel = core::tabs_scrolled_by(corrected, 1);
    if (core::tabs_scrolled_by(corrected, 0) != layout.scroll || wheel < 0 || wheel > limit)
    {
        return "the wheel keeps the scroll at rest for no notch and within the range";
    }
    return {};
}

// --- 不変量 4: 決定性と押した要素 ---

// TitleBarLayout は operator== を持たないので欄を並べる。欄が増えたらここも足す。
[[nodiscard]] bool same_layout(const TitleBarLayout &left, const TitleBarLayout &right)
{
    const bool rectangles = left.band == right.band && left.tabs == right.tabs &&
                            left.viewport == right.viewport && left.tab_list == right.tab_list &&
                            left.add_tab == right.add_tab && left.minimize == right.minimize &&
                            left.maximize == right.maximize && left.close == right.close;
    const bool sizes =
        left.tab_width == right.tab_width && left.tab_gap == right.tab_gap &&
        left.corner_radius == right.corner_radius && left.underline == right.underline &&
        left.glyph == right.glyph && left.tab_close_size == right.tab_close_size &&
        left.tab_close_inset == right.tab_close_inset && left.tab_padding == right.tab_padding &&
        left.button_radius == right.button_radius;
    return rectangles && sizes && left.scroll == right.scroll &&
           left.overflowing == right.overflowing && left.tab_count == right.tab_count &&
           left.active == right.active && left.hovered == right.hovered;
}

[[nodiscard]] std::string_view determinism_violation(const TitleBarInput &input)
{
    const TitleBarLayout first = core::title_bar_layout(input);
    const TitleBarLayout second = core::title_bar_layout(input);
    const std::int32_t x = first.band.right / 3;
    const std::int32_t y = first.band.bottom / 2;
    if (!same_layout(first, second) ||
        core::title_bar_target(first, x, y) != core::title_bar_target(second, x, y))
    {
        return "the same input gives the same layout and hit";
    }
    return {};
}

// マウスを載せたとき見た目が変わる要素（タブ・×・「＋」・「∨」）だけが残る。
[[nodiscard]] bool keeps_hover(TitleBarHit hit)
{
    switch (hit)
    {
    case TitleBarHit::tab:
    case TitleBarHit::tab_close:
    case TitleBarHit::add_tab:
    case TitleBarHit::tab_list:
        return true;
    case TitleBarHit::caption:
    case TitleBarHit::minimize:
    case TitleBarHit::maximize:
    case TitleBarHit::close:
    case TitleBarHit::none:
        return false;
    }
    std::unreachable();
}

// 押して帯が送られた後に同じ点で離す（決定 9）。離した要素が押した要素と違えば値なし、
// 同じなら離した要素。hover は見た目の変わる要素だけを残す。
[[nodiscard]] std::string_view release_violation_at(const TitleBarInput &input, Point point,
                                                    std::int32_t moved_dips)
{
    const TitleBarLayout before = core::title_bar_layout(input);
    const TitleBarLayout after =
        core::title_bar_layout(scrolled_to(input, input.scroll_dips + moved_dips));
    const TitleBarTarget pressed = core::title_bar_target(before, point.first, point.second);
    const TitleBarTarget released = core::title_bar_target(after, point.first, point.second);
    const auto settled = core::title_bar_released(pressed, released);
    if (settled.has_value() != (pressed == released) ||
        (settled.has_value() && settled.value() != pressed))
    {
        return "a release acts only on the element it was pressed on";
    }
    if (core::title_bar_released(std::nullopt, released).has_value())
    {
        return "a release without a press does nothing";
    }
    if (core::title_bar_hover(released).has_value() != keeps_hover(released.hit))
    {
        return "the hover keeps only the elements that change their look";
    }
    return {};
}

[[nodiscard]] std::string_view release_violation(const TitleBarInput &input)
{
    const TitleBarLayout layout = core::title_bar_layout(input);
    const std::int32_t step = layout.tab_width + layout.tab_gap;
    const std::int32_t middle = (layout.viewport.top + layout.viewport.bottom) / 2;
    for (std::int32_t x = layout.viewport.left; x < layout.viewport.right; x += step / 3 + 1)
    {
        const std::string_view broken = release_violation_at(input, Point{x, middle}, 61);
        if (!broken.empty())
        {
            return broken;
        }
    }
    return {};
}

// --- 走らせ方 ---

void print_input(const char *family, std::string_view reason, const TitleBarInput &input)
{
    const TitleBarTarget hovered = input.hovered.value_or(TitleBarTarget{TitleBarHit::none, 0});
    std::fprintf(stderr,
                 "  %s: %.*s (width %d, dpi %u, tabs %zu, active %zu, scroll %d, hovered %s %d "
                 "%zu)\n",
                 family, static_cast<int>(reason.size()), reason.data(), input.width, input.dpi,
                 input.tab_count, input.active, input.scroll_dips,
                 input.hovered.has_value() ? "yes" : "no", static_cast<int>(hovered.hit),
                 hovered.tab);
}

// 生成した入力のうち性質を破ったものの数。最初の 1 つを出す。
[[nodiscard]] std::size_t broken_inputs(const char *family, std::uint64_t seed, Check check,
                                        bool exhaustive)
{
    Random random{seed};
    const std::size_t count = exhaustive               ? exhaustive_inputs
                              : check == hit_violation ? hit_inputs
                                                       : layout_inputs;
    const std::int32_t widest = exhaustive ? exhaustive_width_dips : maximum_width_dips;
    std::size_t broken = 0;
    for (std::size_t index = 0; index < count; ++index)
    {
        const TitleBarInput input = input_sample(random, widest);
        const std::string_view reason = check(input);
        if (!reason.empty() && broken++ == 0U)
        {
            print_input(family, reason, input);
        }
    }
    return broken;
}

// 全点走査の固定の入力: 狭い帯であふれる（タブ 8 本・3 本以上が見えない）・送った途中・
// アクティブと別のタブに載せている。あふれていることも確かめる。
void verify_exhaustive_overflow()
{
    constexpr TitleBarInput overflowing{720, 144U, 8U,
                                        4U,  300,  TitleBarTarget{TitleBarHit::tab, 5U}};
    const TitleBarLayout layout = core::title_bar_layout(overflowing);
    const std::size_t hidden = layout.tab_count - visible_tabs(layout).size();
    expect(layout.overflowing && hidden >= 3U,
           "the fixed exhaustive input overflows with at least three hidden tabs");
    const std::string_view reason = exhaustive_hit_violation(overflowing);
    if (!reason.empty())
    {
        print_input("exhaustive overflow", reason, overflowing);
    }
    expect(reason.empty(),
           "the hit test follows the rectangles at every pixel of an overflowing band");
}

void verify_family(const char *description, std::uint64_t seed, Check check)
{
    expect(broken_inputs(description, seed, check, false) == 0U, description);
}

// 生成器そのものが決定的（同じ seed で同じ列）。
void verify_generator_is_deterministic()
{
    Random left{9291U};
    Random right{9291U};
    bool same = true;
    for (std::size_t round = 0; round < 16U; ++round)
    {
        const TitleBarInput first = input_sample(left, maximum_width_dips);
        const TitleBarInput second = input_sample(right, maximum_width_dips);
        same = same && first.width == second.width && first.dpi == second.dpi &&
               first.tab_count == second.tab_count && first.active == second.active &&
               first.scroll_dips == second.scroll_dips && first.hovered == second.hovered;
    }
    expect(same, "the generator gives the same inputs for the same seed");
}

// 1 つの入力に全部の性質を当て、破った数を返す（破ったものは出す）。
[[nodiscard]] std::size_t broken_checks(const TitleBarInput &input)
{
    constexpr std::array<Check, 6> checks{{rectangle_violation, hit_violation, scroll_violation,
                                           into_view_violation, determinism_violation,
                                           release_violation}};
    std::size_t broken = 0;
    for (const Check check : checks)
    {
        const std::string_view reason = check(input);
        if (!reason.empty())
        {
            ++broken;
            print_input("fixed edge", reason, input);
        }
    }
    return broken;
}

// 境目の見本: 最小の幅・あふれる境の本数・タブ 0 本・範囲の外のアクティブと載せたタブ。
void verify_fixed_edges()
{
    constexpr std::array<TitleBarInput, 6> edges{
        {TitleBarInput{360, 96U, 0U, 0U, 0, std::nullopt},
         TitleBarInput{360, 96U, 2U, 5U, huge_scroll_dips, TitleBarTarget{TitleBarHit::tab, 9U}},
         TitleBarInput{450, 120U, 64U, 63U, -1, TitleBarTarget{TitleBarHit::tab_close, 0U}},
         TitleBarInput{540, 144U, 3U, 1U, 7, TitleBarTarget{TitleBarHit::tab_list, 0U}},
         TitleBarInput{720, 192U, 1U, 0U, 0, TitleBarTarget{TitleBarHit::add_tab, 0U}},
         TitleBarInput{8000, 192U, 64U, 32U, 4000, std::nullopt}}};
    std::size_t broken = 0;
    for (const TitleBarInput &edge : edges)
    {
        broken += broken_checks(edge);
    }
    expect(broken == 0U, "the fixed edge samples keep every property");
}
} // namespace

void verify_title_bar_layout_property_scope()
{
    verify_generator_is_deterministic();
    verify_fixed_edges();
    verify_family("the band rectangles stay inside and apart", 9201U, rectangle_violation);
    verify_family("the hit test follows the rectangles", 9202U, hit_violation);
    expect(broken_inputs("the hit test follows the rectangles at every pixel", 9203U,
                         exhaustive_hit_violation, true) == 0U,
           "the hit test follows the rectangles at every pixel of a narrow band");
    verify_exhaustive_overflow();
    verify_family("the scroll stays in range and keeps the tab width", 9204U, scroll_violation);
    verify_family("the corrected scroll shows the active tab", 9205U, into_view_violation);
    verify_family("the same input gives the same layout", 9206U, determinism_violation);
    verify_family("a release acts only on the pressed element", 9207U, release_violation);
}
} // namespace nenenib::tests
