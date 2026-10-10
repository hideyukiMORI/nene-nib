#pragma once

#include "FrameRowsWorkload.hpp"
#include "ProbeWorkload.hpp"

namespace nenenib::tests::performance
{
[[nodiscard]] std::string frame_rows_input(FrameRowsWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
framed_rows(FrameRowsWorkload workload, const std::string &input, application::TimingPort &timing);
} // namespace nenenib::tests::performance
