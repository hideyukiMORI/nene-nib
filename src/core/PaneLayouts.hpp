#pragma once

#include "BodyLayout.hpp"
#include "FontSize.hpp"
#include "LayoutRect.hpp"
#include "PaneDimension.hpp"
#include "PaneSide.hpp"
#include "PaneSizing.hpp"
#include "PaneSplit.hpp"
#include "SplitExtent.hpp"
#include "SplitLayout.hpp"
#include "SplitShare.hpp"

#include <cstdint>
#include <optional>

namespace nenenib::core
{
// 本文の枠の矩形（ADR 0148 の決定 1・8）。位置の順で、first は左か上の枠、second は右か下の枠、
// divider は 2 つの枠の境界。どちらが操作中かは SplitState::active_side（frame の active_side）で
// 引く。1 枠では second と divider は値なしで、first は body_layout と同じ値。どの欄も単独で
// 妥当な値なので公開 aggregate（CPP-003）。
struct PaneLayouts
{
    BodyLayout first;
    std::optional<BodyLayout> second;
    std::optional<LayoutRect> divider;
};

// 本文の帯を配置 split.layout と比 split.share で分ける（ADR 0148 の決定 1 と 2026-11-22 の追補）。
// 等分の比は (帯の幅 − 境界) / 2 を切り捨てて両枠に同じ幅を与え、余りの画素は境界に足す（境界は 1
// DIP を物理画素に丸めて 1 以上）。等分でない比は first の長さを split_first_length で決め、境界は
// ちょうど 1 DIP、残りを second が取る。上下は高さを同じ規則で分ける。各枠の行番号の欄・本文・見え
// ている行数は body_layout と同じ規則で枠の帯から作る。client は窓の client 領域（左上が原点）で、
// 1 枠の値は body_layout(client の幅, 高さ, dpi, font_size) と同じ。
[[nodiscard]] PaneLayouts pane_layouts(LayoutRect client, std::uint32_t dpi, FontSize font_size,
                                       PaneSplit split) noexcept;

// 境界を動かすときの今の寸法（追補の決定 1・2）。1 枠なら値なし。first は今の比の first の長さ。
[[nodiscard]] std::optional<SplitExtent> split_extent(LayoutRect client, std::uint32_t dpi,
                                                      FontSize font_size, PaneSplit split) noexcept;

// 比 share の first の枠の長さ（pane_layouts と鍵の大きさの変え方が共用する 1 本）。等分か、帯が
// 2 つの最小より狭ければ available / 2。ほかは available × first / whole を切り捨て、どちらの枠にも
// minimum を残すように挟む。extent.first は読まない。
[[nodiscard]] std::int32_t split_first_length(const SplitExtent &extent, SplitShare share) noexcept;

// 境界の始まり（左か上の端）を座標 at に置いたときの比（ドラッグ・追補の決定 3）。範囲は
// split_first_length と同じく最小で挟む。帯が 2 つの最小より狭ければ等分。
[[nodiscard]] SplitShare split_share_at(const SplitExtent &extent, std::int32_t at) noexcept;

// 鍵の 1 回の後の比（追補の決定 4）。操作中の枠（active の側）の長さを sizing で決め、もう 1 つの枠
// が残りを取る。cell は升 1 つの画素（1 未満は 1）。向きが分割と合わなければ値なし（even を除く）。
[[nodiscard]] std::optional<SplitShare> sized_share(const SplitExtent &extent, PaneSide active,
                                                    PaneSizing sizing, std::int32_t cell) noexcept;

// 点 (x, y) が境界の上か（境界の両側 3 DIP・面の境と同じ広さ）。1 枠なら偽。
[[nodiscard]] bool split_divider_hit(const PaneLayouts &layouts, std::uint32_t dpi, std::int32_t x,
                                     std::int32_t y) noexcept;

// 分割の向き（左右は幅・上下は高さ）。1 枠なら値なし。
[[nodiscard]] std::optional<PaneDimension> split_dimension(SplitLayout layout) noexcept;

// 操作中の枠の配置（ADR 0148 の決定 8）。active はその枠の側（frame の active_side）。1 枠
// （second が値なし）では側を問わず first。描画・窓の行数・マウスが同じ 1 本で引く（ARC-001）。
[[nodiscard]] BodyLayout active_pane_layout(const PaneLayouts &layouts, PaneSide active) noexcept;

// もう 1 つの枠の配置。active の反対の側で、1 枠なら値なし。
[[nodiscard]] std::optional<BodyLayout> other_pane_layout(const PaneLayouts &layouts,
                                                          PaneSide active) noexcept;
} // namespace nenenib::core
