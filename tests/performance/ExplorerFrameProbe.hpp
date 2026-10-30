#pragma once

#include "ExplorerFrameWorkload.hpp"
#include "ProbeFailure.hpp"
#include "TimingPort.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace nenenib::tests::performance
{
[[nodiscard]] std::string explorer_frame_input(ExplorerFrameWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
explorer_frame_typing(ExplorerFrameWorkload workload, const std::string &input,
                      application::TimingPort &timing);
} // namespace nenenib::tests::performance
