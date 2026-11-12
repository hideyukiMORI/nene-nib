#pragma once

#include "PieceListWorkload.hpp"
#include "ProbeFailure.hpp"
#include "TimingPort.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace nenenib::tests::performance
{
[[nodiscard]] std::string piece_list_input(PieceListWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
piece_list_run(PieceListWorkload workload, const std::string &input,
               application::TimingPort &timing);
} // namespace nenenib::tests::performance
