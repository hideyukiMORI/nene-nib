#pragma once

#include "CommandInputWorkload.hpp"
#include "ProbeFailure.hpp"
#include "TimingPort.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace nenenib::tests::performance
{
[[nodiscard]] std::string command_input_input(CommandInputWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
probed_command_input(CommandInputWorkload workload, const std::string &input,
                     application::TimingPort &timing);
} // namespace nenenib::tests::performance
