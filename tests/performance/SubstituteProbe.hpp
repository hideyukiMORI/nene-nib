#pragma once

#include "ProbeFailure.hpp"
#include "SubstituteWorkload.hpp"
#include "TimingPort.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace nenenib::tests::performance
{
[[nodiscard]] std::string substitute_input(SubstituteWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
substitute_lines(SubstituteWorkload workload, const std::string &input,
                 application::TimingPort &timing);
} // namespace nenenib::tests::performance
