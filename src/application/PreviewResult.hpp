#pragma once

#include "PreviewFailure.hpp"
#include "PreviewLine.hpp"

#include <expected>
#include <vector>

namespace nenenib::application
{
// 1 つのファイルの窓か、作れない理由（ADR 0189 の決定 4）。
using PreviewResult = std::expected<std::vector<PreviewLine>, PreviewFailure>;
} // namespace nenenib::application
