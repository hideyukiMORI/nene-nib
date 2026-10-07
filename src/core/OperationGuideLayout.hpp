#pragma once

#include "BodyGuideLayout.hpp"
#include "BodyLayout.hpp"
#include "GuideContext.hpp"
#include "StatusBarLayout.hpp"
#include "StatusGuideLayout.hpp"

#include <variant>

namespace nenenib::core
{
// 排他的な配置。本文が収まらないときだけステータスへ回す（ADR 0079）。
using OperationGuideLayout = std::variant<std::monostate, BodyGuideLayout, StatusGuideLayout>;

[[nodiscard]] OperationGuideLayout operation_guide_layout(GuideContext context,
                                                          const BodyLayout &body,
                                                          const StatusBarLayout &status,
                                                          std::uint32_t dpi) noexcept;
} // namespace nenenib::core
