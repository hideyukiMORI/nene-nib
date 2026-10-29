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
} // namespace nenenib::core
