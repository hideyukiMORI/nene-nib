#pragma once

#include "ProbeWorkload.hpp"

namespace nenenib::tests::performance
{
[[nodiscard]] std::string named_palette_input();
[[nodiscard]] std::string located_palette_input();
[[nodiscard]] std::string narrowing_palette_input();
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
listed_palette(const std::string &input, application::TimingPort &timing);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
narrowed_palette(const std::string &input, application::TimingPort &timing);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
left_palette(const std::string &input, application::TimingPort &timing);
} // namespace nenenib::tests::performance
