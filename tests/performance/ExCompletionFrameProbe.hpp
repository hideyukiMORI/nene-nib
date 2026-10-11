#pragma once
#include "ExCompletionProbe.hpp"
namespace nenenib::tests::performance
{
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
probed_ex_completion_frame(ExCompletionWorkload workload, const ExCompletionSample &sample,
                           application::TimingPort &timing);
}
