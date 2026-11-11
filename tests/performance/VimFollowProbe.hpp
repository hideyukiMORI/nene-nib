#pragma once

#include "ProbeFailure.hpp"
#include "TimingPort.hpp"
#include "VimBlockWorkload.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace nenenib::tests::performance
{
[[nodiscard]] std::string vim_follow_input();
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
vim_insert_scrolled(VimBlockWorkload workload, const std::string &input,
                    application::TimingPort &timing);
} // namespace nenenib::tests::performance
