#pragma once

#include "BodyGuideLayout.hpp"
#include "BodyLayout.hpp"
#include "GuideRequest.hpp"
#include "StatusBarLayout.hpp"
#include "StatusGuideLayout.hpp"

#include <variant>

namespace nenenib::core
{
// 排他的な配置。本文が収まらないときだけステータスへ回す（ADR 0079）。
using OperationGuideLayout = std::variant<std::monostate, BodyGuideLayout, StatusGuideLayout>;

// 本文の案内の鍵の列は request.key_width と 64 DIP の広いほう。説明の列（144 DIP）と間（20 DIP）は
// 変えず、広がった分だけ全体の幅が増えて本文の横中央に置き直す。
[[nodiscard]] OperationGuideLayout operation_guide_layout(GuideRequest request,
                                                          const BodyLayout &body,
                                                          const StatusBarLayout &status,
                                                          std::uint32_t dpi) noexcept;

// 測ったモードの文の幅に合わせた案内（#1658）。mode_width はモードの文の幅（物理画素・ui が描画と
// 同じ書式で測る・モードを描かない frame は 0）。ステータスの案内の左端がモードの文の右から
// status_text_gap_dips を空けて入らなければ案内を出さない（status_bar_fitted と同じ規則・
// `(insert) VISUAL BLOCK` などの長い文）。入るとき・本文の案内・案内なしは guide のまま。
[[nodiscard]] OperationGuideLayout status_guide_fitted(const OperationGuideLayout &guide,
                                                       const StatusBarLayout &status,
                                                       std::uint32_t dpi,
                                                       std::int32_t mode_width) noexcept;
} // namespace nenenib::core
