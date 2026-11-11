#pragma once

#include "ProbeFailure.hpp"
#include "TimingPort.hpp"
#include "WordObjectWorkload.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace nenenib::tests::performance
{
[[nodiscard]] std::string word_object_input(WordObjectWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
word_object_ranges(WordObjectWorkload workload, const std::string &input,
                   application::TimingPort &timing);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
word_double_clicked(WordObjectWorkload workload, const std::string &input,
                    application::TimingPort &timing);
} // namespace nenenib::tests::performance
