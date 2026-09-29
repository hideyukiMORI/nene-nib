#pragma once

#include "LayoutRect.hpp"
#include "TitleBarInput.hpp"
#include "TitleBarTarget.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace nenenib::core
{
// 自前タイトルバーの寸法（docs/design/2026-09-15-look.md 第 2 節と docs/design/2026-09-29-tabs.md
// 第 2〜3 節）を物理画素で表した値。OS を知らない純関数が作り、窓手続きと描画はこれを写すだけ
// （ADR 0008 の決定 5・ADR 0056 の決定 8）。
// tabs はタブの並びの全体（送り量ぶん左へずれ、viewport の外へはみ出し得る）、viewport は
// タブを描いてよい領域（タブの領域）。あふれていなければ 2 つは同じ矩形で、tab_list は幅 0。
struct TitleBarLayout
{
    LayoutRect band;
    LayoutRect tabs;
    LayoutRect viewport;
    LayoutRect tab_list;
    LayoutRect add_tab;
    LayoutRect minimize;
    LayoutRect maximize;
    LayoutRect close;
    std::int32_t tab_width;
    std::int32_t tab_gap;
    std::int32_t corner_radius;
    std::int32_t underline;
    std::int32_t glyph;
    // × の領域の一辺と、タブの右端からの距離（物理画素）。
    std::int32_t tab_close_size;
    std::int32_t tab_close_inset;
    // 収めた後の送り量（物理画素）。あふれていなければ 0。
    std::int32_t scroll;
    bool overflowing;
    std::size_t tab_count;
    std::size_t active;
    std::optional<TitleBarTarget> hovered;
};

[[nodiscard]] TitleBarLayout title_bar_layout(const TitleBarInput &input) noexcept;

// 帯の位置 index のタブの矩形。送り量を引いた位置で、viewport の外へはみ出した分も含む
// （描く側が viewport で切り抜く）。
[[nodiscard]] LayoutRect tab_rect(const TitleBarLayout &layout, std::size_t index) noexcept;

// タブが少しでも viewport に掛かるか。
[[nodiscard]] bool tab_visible(const TitleBarLayout &layout, std::size_t index) noexcept;

// × の領域（24 × 24 DIP・タブの右端から 6・縦は中央）。アクティブなタブと、マウスを載せている
// タブにだけある（D21）。
[[nodiscard]] std::optional<LayoutRect> tab_close_rect(const TitleBarLayout &layout,
                                                       std::size_t index) noexcept;

// 点に当たる要素。窓の操作 →「＋」→「∨」→ タブの領域の中ならその点のタブの × → タブ →
// それ以外は caption。viewport の外へはみ出したタブの部分とタブの間は当たらない（決定 9）。
[[nodiscard]] TitleBarTarget title_bar_target(const TitleBarLayout &layout, std::int32_t x,
                                              std::int32_t y) noexcept;

// アクティブなタブの全体がタブの領域に入る最小の動きで直した送り量（DIP）。あふれていなければ
// 0。application は 96 DPI の入力で呼ぶ（その DPI では物理画素と DIP が一致する）。
[[nodiscard]] std::int32_t tabs_scrolled_into_view(const TitleBarInput &input) noexcept;

// ホイールで動かした送り量（DIP）。1 刻みはタブ 1 本ぶん（122 DIP）。手前（負の刻み）で右の
// タブが見える向き・奥で左へ戻る向きに動き、端で止まる。
[[nodiscard]] std::int32_t tabs_scrolled_by(const TitleBarInput &input, int notches) noexcept;
} // namespace nenenib::core
