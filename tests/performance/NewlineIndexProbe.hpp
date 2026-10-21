#pragma once

#include "NewlineIndexWorkload.hpp"
#include "ProbeFailure.hpp"
#include "TimingPort.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace nenenib::tests::performance
{
[[nodiscard]] std::string newline_index_input(NewlineIndexWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
indexed_payload(NewlineIndexWorkload workload, const std::string &input,
                application::TimingPort &timing);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
indexed_lengths(NewlineIndexWorkload workload, const std::string &input,
                application::TimingPort &timing);
} // namespace nenenib::tests::performance
