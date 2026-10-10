#pragma once

#include "ProbeWorkload.hpp"
#include "SearchSnapshotWorkload.hpp"

namespace nenenib::tests::performance
{
[[nodiscard]] std::string search_snapshot_input(SearchSnapshotWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
probed_search_snapshot(SearchSnapshotWorkload workload, const std::string &input,
                       application::TimingPort &timing);
} // namespace nenenib::tests::performance
