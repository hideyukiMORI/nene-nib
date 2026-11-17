#pragma once

#include "DiagnosticCounts.hpp"
#include "LayoutRect.hpp"
#include "StatusBarHit.hpp"
#include "StatusItems.hpp"

#include <array>
#include <cstdint>

namespace nenenib::core
{
// ステータスバーの寸法（docs/design/2026-09-15-look.md 第 2 節）を物理画素で表した値。
// 左はモードトグルと現在モード、右は status_items_for が作る 4 項目（ADR 0008 の決定 4・5）。
struct StatusBarLayout
{
    LayoutRect band;
    LayoutRect toggle;
    LayoutRect toggle_ordinary;
    LayoutRect toggle_vim;
    LayoutRect mode;
    std::array<LayoutRect, status_item_count> items;
    // 診断の件数（ADR 0161 の決定 4）。言語の右のいちばん右で、件数を出さないとき（0 件・言語を
    // 畳んでも `:` の入力欄の最小幅に届かない狭い窓）は右端の幅 0 の矩形（ほかの項目は件数が
    // 無いときの位置・間の余白も取らない）。
    LayoutRect diagnostics;
    // 表示形式の印（ADR 0140 の決定 3）。items の最初の項目の左隣で、案内の右端（items の最初の
    // 左から 28 DIP）より右に収まる幅。右寄せで書く。
    LayoutRect display;
    std::int32_t corner_radius;
    std::int32_t segment_radius;
};

// counts は診断の件数（diagnostic_counts）。counted でなければ件数の項目は幅 0。
[[nodiscard]] StatusBarLayout status_bar_layout(std::int32_t width, std::int32_t height,
                                                std::uint32_t dpi,
                                                const DiagnosticCounts &counts) noexcept;

// 件数の項目を出さないときの配置（帯の高さだけを読む所と、診断を持たない試験）。
[[nodiscard]] StatusBarLayout status_bar_layout(std::int32_t width, std::int32_t height,
                                                std::uint32_t dpi) noexcept;

// モードの文の右端と、その右に並ぶ文（表示形式の印・行と桁）の間に空ける最小の幅（#1658）。
// 6 DIP は最小の窓の日本語の `通常` と `行 17, 桁 4`（直す前に 96 / 120 DPI とも 8 px 空いて
// いた形）を 2 つの DPI とも残す幅（8 DIP は 120 DPI で 10 px になり落とす）。
inline constexpr std::int32_t status_text_gap_dips = 6;

// 測った文の幅に合わせた配置（#1658・ADR 0154 の追補 2026-11-14）。mode_width はモードの文の幅、
// position_width は行と桁の文の幅（物理画素・描画と同じ書式で ui が測る）。行と桁の欄は文が枠
// （96 DIP）より広ければ左へ文の幅まで広げ、表示形式の印はその左へ寄せる。モードの文の右から
// status_text_gap_dips より右に入らないものは、行と桁 → 表示形式の印の順に幅 0 にする（行と桁を
// 落としたら印は空いた所へ寄せて、入るかを見る）。どちらも入って枠に収まるなら layout のまま。
// 案内は status_guide_fitted（OperationGuideLayout.hpp）が同じ規則で落とす。
// 当たり（status_bar_hit）と `:` の入力欄は測らない配置を読む（落とす 2 つはどちらも押せない）。
[[nodiscard]] StatusBarLayout status_bar_fitted(const StatusBarLayout &layout, std::uint32_t dpi,
                                                std::int32_t mode_width,
                                                std::int32_t position_width) noexcept;

[[nodiscard]] StatusBarHit status_bar_hit(const StatusBarLayout &layout, std::int32_t x,
                                          std::int32_t y) noexcept;
} // namespace nenenib::core
