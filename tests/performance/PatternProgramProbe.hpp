#pragma once

#include "PatternProgramWorkload.hpp"
#include "ProbeFailure.hpp"
#include "TimingPort.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace nenenib::tests::performance
{
[[nodiscard]] std::string pattern_program_input(PatternProgramWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
matched_program(PatternProgramWorkload workload, const std::string &input,
                application::TimingPort &timing);
} // namespace nenenib::tests::performance
