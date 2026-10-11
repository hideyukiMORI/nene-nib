#pragma once

#include "ProbeFailure.hpp"
#include "TabDisplayWorkload.hpp"
#include "TimingPort.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace nenenib::tests::performance
{
[[nodiscard]] std::string tab_display_input(TabDisplayWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
fixed_tab_display(TabDisplayWorkload workload, const std::string &input,
                  application::TimingPort &timing);
} // namespace nenenib::tests::performance
