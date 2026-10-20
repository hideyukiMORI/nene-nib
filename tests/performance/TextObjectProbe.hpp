#pragma once

#include "ProbeFailure.hpp"
#include "TextObjectWorkload.hpp"
#include "TimingPort.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace nenenib::tests::performance
{
[[nodiscard]] std::string text_object_input(TextObjectWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
text_object_ranges(TextObjectWorkload workload, const std::string &input,
                   application::TimingPort &timing);
} // namespace nenenib::tests::performance
