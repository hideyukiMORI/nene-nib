#pragma once

#include "PatternCostWorkload.hpp"
#include "ProbeFailure.hpp"
#include "TimingPort.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace nenenib::tests::performance
{
[[nodiscard]] std::string pattern_cost_input(PatternCostWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
pattern_cost_run(PatternCostWorkload workload, const std::string &input,
                 application::TimingPort &timing);
} // namespace nenenib::tests::performance
