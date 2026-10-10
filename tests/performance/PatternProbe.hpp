#pragma once

#include "ProbeWorkload.hpp"

namespace nenenib::tests::performance
{
[[nodiscard]] std::string pattern_probe_input(ProbeWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
matched_pattern(ProbeWorkload workload, const std::string &input, application::TimingPort &timing);
} // namespace nenenib::tests::performance
