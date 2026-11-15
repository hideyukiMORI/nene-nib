#pragma once

#include "ExLinesWorkload.hpp"
#include "ProbeFailure.hpp"
#include "TimingPort.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace nenenib::tests::performance
{
[[nodiscard]] std::string ex_lines_input(ExLinesWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
ex_lines_run(ExLinesWorkload workload, const std::string &input, application::TimingPort &timing);
} // namespace nenenib::tests::performance
