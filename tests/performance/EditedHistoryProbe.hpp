#pragma once

#include "EditedHistoryWorkload.hpp"
#include "ProbeFailure.hpp"
#include "TimingPort.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace nenenib::tests::performance
{
[[nodiscard]] std::string edited_history_input(EditedHistoryWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
edited_history_keys(EditedHistoryWorkload workload, const std::string &input,
                    application::TimingPort &timing);
} // namespace nenenib::tests::performance
