#pragma once

#include "FileNameWorkload.hpp"
#include "ProbeFailure.hpp"
#include "TimingPort.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace nenenib::tests::performance
{
[[nodiscard]] std::string file_name_input(FileNameWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
fixed_file_name(FileNameWorkload workload, const std::string &input,
                application::TimingPort &timing);
} // namespace nenenib::tests::performance
