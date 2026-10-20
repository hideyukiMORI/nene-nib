#pragma once

#include "MarkdownWorkload.hpp"
#include "ProbeFailure.hpp"
#include "TimingPort.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace nenenib::tests::performance
{
[[nodiscard]] std::string markdown_input(MarkdownWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
parsed_markdown(MarkdownWorkload workload, const std::string &input,
                application::TimingPort &timing);
} // namespace nenenib::tests::performance
