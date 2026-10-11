#pragma once
#include "CompletionPrefixWorkload.hpp"
#include "ProbeFailure.hpp"
#include "TimingPort.hpp"
#include <cstdint>
#include <expected>
#include <string>
namespace nenenib::tests::performance
{
[[nodiscard]] std::string completion_prefix_input(CompletionPrefixWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
probed_completion_prefix(CompletionPrefixWorkload workload, const std::string &input,
                         application::TimingPort &timing);
} // namespace nenenib::tests::performance
