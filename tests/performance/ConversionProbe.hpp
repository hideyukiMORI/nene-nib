#pragma once

#include "ProbeWorkload.hpp"

namespace nenenib::tests::performance
{
[[nodiscard]] std::string codepage_japanese_input();
[[nodiscard]] std::string utf16_japanese_input();
[[nodiscard]] std::string utf16_ascii_input();
[[nodiscard]] std::string utf16_supplementary_input();
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
converted_codepage(const std::string &input, application::TimingPort &timing);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
converted_utf16_japanese(const std::string &input, application::TimingPort &timing);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
converted_utf16_ascii(const std::string &input, application::TimingPort &timing);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
converted_utf16_supplementary(const std::string &input, application::TimingPort &timing);
} // namespace nenenib::tests::performance
