#pragma once

#include "ExFailure.hpp"
#include "ThemeLookupFailure.hpp"

#include <string_view>
#include <variant>

namespace nenenib::core
{
using ExEvaluationFailure = std::variant<ExFailure, ThemeLookupFailure>;
[[nodiscard]] DisplayText ex_failure_message(const ExEvaluationFailure &failure);
// 入力を添えた文言（ADR 0057 の決定 4）。unsupported_argument だけが「Not supported: <入力>」で、
// ほかは 1 引数版と同じ。入力の前後の空白は落とし、添えると長すぎるなら入力を添えない。
[[nodiscard]] DisplayText ex_failure_message(const ExEvaluationFailure &failure,
                                             std::string_view input);
} // namespace nenenib::core
