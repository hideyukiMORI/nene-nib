#pragma once

#include "ProbeWorkload.hpp"
#include "StatusItemsWorkload.hpp"

namespace nenenib::tests::performance
{
[[nodiscard]] std::string status_items_input(StatusItemsWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
fixed_status_items(StatusItemsWorkload workload, const std::string &input,
                   application::TimingPort &timing);
} // namespace nenenib::tests::performance
