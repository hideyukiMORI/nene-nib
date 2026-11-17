#pragma once

#include "BodyLayout.hpp"
#include "EditorFrame.hpp"
#include "LayoutRect.hpp"
#include "LineView.hpp"
#include "Palette.hpp"
#include "RgbaColor.hpp"

#include <d2d1.h>

#include <cstdint>

// Direct2DRenderer の 2 つ以上の翻訳単位が使う補助（ADR 0158 の決定 2 と同じ形・#1800）。
// 型は足さない（CPP-011）。renderer の .cpp だけが include する。
namespace nenenib::ui::win32::detail
{
[[nodiscard]] D2D1_COLOR_F to_color(core::RgbColor color) noexcept;
[[nodiscard]] D2D1_COLOR_F to_color(core::RgbaColor color) noexcept;
[[nodiscard]] D2D1_RECT_F to_rect(const core::LayoutRect &area) noexcept;
// 本文のキャレットを描かないとき。入力行が開いているか、エクスプローラーの面に焦点があるとき
// （採用案の表「焦点」・Ctrl+P の面と同じ）。
[[nodiscard]] bool body_caret_hidden(const application::EditorFrame &frame) noexcept;
// 本文の画面行 row の上端（物理画素）。
[[nodiscard]] std::int32_t row_pixel(const core::BodyLayout &body, std::int64_t row);
// 行の文字組みの領域。原点のずれ（横の送りと巨大な行の窓の x）は application が行ごとに決める
// （ADR 0139 の決定 5）。行の描画と印の 1 パスが同じ 1 本を通る。
[[nodiscard]] core::LayoutRect line_area(const core::BodyLayout &body,
                                         const application::LineView &line);
} // namespace nenenib::ui::win32::detail
