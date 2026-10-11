#pragma once
#include "ExCompletionSample.hpp"
#include "ExCompletionWorkload.hpp"
#include "ProbeFailure.hpp"
#include "TimingPort.hpp"
#include <expected>
#include <string>
namespace nenenib::tests::performance
{
[[nodiscard]] std::string ex_completion_input(ExCompletionWorkload workload);
[[nodiscard]] const ExCompletionSample &ex_completion_sample(ExCompletionWorkload workload);
[[nodiscard]] std::uint64_t completion_checksum(std::span<const std::string> candidates);
[[nodiscard]] bool completions_equal(std::span<const std::string> candidates,
                                     const ExCompletionSample &sample);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
probed_ex_completion(ExCompletionWorkload workload, const std::string &input,
                     application::TimingPort &timing);
} // namespace nenenib::tests::performance
