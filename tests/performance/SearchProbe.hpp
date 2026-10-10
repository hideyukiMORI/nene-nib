#pragma once

#include "ProbeWorkload.hpp"

namespace nenenib::tests::performance
{
[[nodiscard]] std::string search_buffer_input();
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
searched_buffer(ProbeWorkload workload, const std::string &input, application::TimingPort &timing);
} // namespace nenenib::tests::performance
