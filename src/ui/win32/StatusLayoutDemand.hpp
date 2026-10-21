#pragma once

#include "EditorFrame.hpp"
#include "OperationGuideLayout.hpp"
#include "StatusBarLayout.hpp"
#include "StatusLeftState.hpp"

#include <cstddef>

namespace nenenib::ui::win32
{
// ステータスの文字組みを保持する枠の数（ADR 0079 の決定 11 と追補・ADR 0154 の決定 6）。
// 通常 7 枠（トグル 2・モード 1・右の 4 項目）と案内 4 枠。録画中は案内の代わりに 1 枠で 8。
// Direct2DRenderer の保持の配列はこの長さで、status_layout_demand が超えないことを
// nib_window_tests が確かめる。
inline constexpr std::size_t status_layout_capacity = 11;

// 左側に何を描くか。面が開いているときは `:` の入力も知らせもトグルの側へ落ちる。
// draw_status_left はこの値で分かれる（分かれ目はこの 1 か所）。
[[nodiscard]] StatusLeftState status_left_state(const application::EditorFrame &frame) noexcept;

// 1 つの frame の draw_status_bar と draw_status_guide が write_status / status_text_layout で
// 使う枠の数の上限。左側の状態・右の項目のうち幅のあるもの（畳んだ言語の欄は書かない）・
// ステータスの案内の行ごとの鍵と説明を数える。
[[nodiscard]] std::size_t status_layout_demand(StatusLeftState left,
                                               const core::StatusBarLayout &status,
                                               const core::OperationGuideLayout &guide) noexcept;
} // namespace nenenib::ui::win32
