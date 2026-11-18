#pragma once

#include "CompletionFaceLayout.hpp"
#include "CompletionFaceRequest.hpp"
#include "LayoutRect.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace nenenib::core
{
// 補完の面の寸法（DIP・ADR 0169 の決定 4・ADR 0190 の決定 5）。幅 240（補足の欄があるときは
// 360）・最大 8 行。行の高さ・面の上下の内側の空き・キャレットとの空き・行の左右の内側の空き・
// 出どころの印の欄の幅は設計席の決め（Ctrl+P の面と右クリックのメニューに合わせた値）。
inline constexpr std::int32_t completion_face_width_dips = 240;
inline constexpr std::int32_t completion_face_wide_dips = 360;
inline constexpr std::size_t completion_face_rows = 8;
inline constexpr std::int32_t completion_face_row_dips = 24;
inline constexpr std::int32_t completion_face_gap_dips = 4;
inline constexpr std::int32_t completion_face_caret_gap_dips = 2;
inline constexpr std::int32_t completion_face_padding_dips = 10;
inline constexpr std::int32_t completion_face_mark_dips = 16;

// 面の矩形と描く行（純関数）。キャレットの下に行が全部入れば下、入らなければ上、どちらにも入ら
// なければ広い側に入るだけの行（1 行以上）を出し、選んでいる行が見えるように first を送る。左端は
// x を bounds の中へ寄せ、1 行ぶんの高さも無いときは上端を bounds の中へ寄せる（キャレットに
// 掛かってよい）。
[[nodiscard]] CompletionFaceLayout
completion_face_layout(const CompletionFaceRequest &request) noexcept;

// 点（クライアント座標の物理画素）の下の行。表示値の行の中の位置（layout.first + 面の行）で、
// 面の外と上下の内側の空きは値なし。描画と窓の当たりが同じ配置を読む。
[[nodiscard]] std::optional<std::size_t>
completion_face_hit(const CompletionFaceLayout &layout, std::int32_t x, std::int32_t y) noexcept;

// 面の index 番目（0 始まり・layout.rows 未満）の行の矩形。
[[nodiscard]] LayoutRect completion_face_row(const CompletionFaceLayout &layout,
                                             std::size_t index) noexcept;
// 行の中の欄。左から出どころの印・名前・（補足の欄があれば）右に補足。
[[nodiscard]] LayoutRect completion_face_mark(const CompletionFaceLayout &layout,
                                              std::size_t index) noexcept;
[[nodiscard]] LayoutRect completion_face_label(const CompletionFaceLayout &layout,
                                               std::size_t index) noexcept;
[[nodiscard]] LayoutRect completion_face_detail(const CompletionFaceLayout &layout,
                                                std::size_t index) noexcept;
} // namespace nenenib::core
