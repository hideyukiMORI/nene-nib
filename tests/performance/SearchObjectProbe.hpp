#pragma once

#include "ProbeFailure.hpp"
#include "SearchObjectWorkload.hpp"
#include "TimingPort.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace nenenib::tests::performance
{
[[nodiscard]] std::string search_object_input(SearchObjectWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
search_object_keys(SearchObjectWorkload workload, const std::string &input,
                   application::TimingPort &timing);
} // namespace nenenib::tests::performance
