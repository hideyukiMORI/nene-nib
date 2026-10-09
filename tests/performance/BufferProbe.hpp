#pragma once

#include "ProbeWorkload.hpp"

namespace nenenib::tests::performance
{
[[nodiscard]] std::string crlf_buffer_input();
[[nodiscard]] std::string lf_buffer_input();
[[nodiscard]] std::string position_buffer_input();
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
queried_lines(const std::string &input, application::TimingPort &timing);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
queried_long_position(const std::string &input, application::TimingPort &timing);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
queried_scattered_position(const std::string &input, application::TimingPort &timing);
} // namespace nenenib::tests::performance
