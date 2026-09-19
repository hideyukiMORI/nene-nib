#pragma once

#include "ExFailure.hpp"
#include "ThemeLookupFailure.hpp"

#include <variant>

namespace nenenib::core
{
using ExEvaluationFailure = std::variant<ExFailure, ThemeLookupFailure>;
[[nodiscard]] DisplayText ex_failure_message(const ExEvaluationFailure &failure);
} // namespace nenenib::core
