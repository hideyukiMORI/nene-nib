#pragma once

#include "ProbeWorkload.hpp"

namespace nenenib::tests::performance
{
[[nodiscard]] std::string scoped_input_of(ProbeWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
run_scoped_workload(ProbeWorkload workload, const std::string &input,
                    application::TimingPort &timing);
} // namespace nenenib::tests::performance
