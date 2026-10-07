#pragma once

#include "EditMode.hpp"
#include "GuideContext.hpp"
#include "GuideEntry.hpp"

#include <array>

namespace nenenib::core
{
struct OperationGuide
{
    GuideContext context;
    std::array<GuideEntry, 3> entries;
};

// 非表示なら鍵文字列を構築しない。本文・ステータスは同じ3操作の表示値を読む。
[[nodiscard]] OperationGuide operation_guide(GuideContext context, EditMode mode);
} // namespace nenenib::core
